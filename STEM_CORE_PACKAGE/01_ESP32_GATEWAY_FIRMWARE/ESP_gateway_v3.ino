#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Preferences.h>

// ==============================================================================
// NEURON IOT GATEWAY V3 - CENTRAL INTELLIGENCE & ADAPTIVE MULTI-RATE POLLING
// ==============================================================================
// Phiên bản nâng cấp V3:
// 1. Phân luồng quét thích ứng 3 tầng (Adaptive 3-Tier Multi-Rate Polling):
//    - TẦNG 1 (Siêu nhanh 20ms): Nút nhấn (Slave 2), Biến trở (Slave 4), Siêu âm (Slave 5).
//    - TẦNG 2 (Sự kiện / Lệnh): Đèn LED (Slave 1), Còi (Slave 3), Động cơ DC (Slave 7),
//                             Servo (Slave 8), LCD 1602 (Slave 6), OLED SSD1306 (Slave 9).
//    - TẦNG 3 (Định kỳ chậm 1.5s - 2.0s): Độ ẩm đất (Slave 10), DHT11 (Slave 11), DHT22 (Slave 12).
//    ==> Kết quả: Giảm độ trễ nút bấm và biến trở từ 240ms xuống chỉ còn 15 - 20ms!
// 2. Nhận diện trọn bộ cử chỉ nút thông minh (Single Click, Double Click, Long Press).
// 3. Hỗ trợ toàn diện 12 Slaves trong topology mạng vòng Daisy-Chain Ring UART.
// 4. Khử nhiễu Echo (Dual-Frame Verification) cho tất cả các node.
// 5. Đóng gói JSON Telemetry chuẩn công nghiệp gửi lên App qua UDP port 9999 / 8888.
// ==============================================================================

// ==========================================
// 1. QUẢN LÝ BỘ NHỚ NVS & WEB CẤU HÌNH WIFI
// ==========================================
Preferences preferences;    
WebServer server(80);       

char device_id[32] = "KIT_01";       // Mã nhận diện duy nhất của bộ kit (có thể đặt tên NHOM_01)
char pc_ip[20] = "255.255.255.255";  // Mặc định broadcast hoặc IP tự học từ PC
const int pc_port = 9999;           // Cổng Telemetry PC lắng nghe
const int local_port = 8888;        // Cổng nhận lệnh điều khiển trên ESP32
WiFiUDP udp;
bool is_ap_mode = false;

// ==========================================
// 2. CẤU HÌNH MẠNG VÒNG NEURON RING UART
// ==========================================
#define NEURON_SERIAL Serial1
#define RX_PIN 5  
#define TX_PIN 4  
#define STATUS_LED_PIN 15 

#define FRAME_HEADER     0xAA

// ĐỊNH DANH 12 SLAVE NODES
#define ID_LED           0x01
#define ID_BUTTON        0x02
#define ID_BUZZER        0x03  
#define ID_POT           0x04  
#define ID_DISTANCE      0x05  
#define ID_LCD           0x06
#define ID_MOTOR         0x07
#define ID_SERVO         0x08
#define ID_OLED          0x09
#define ID_SOIL          0x0A
#define ID_DHT11         0x0B
#define ID_DHT22         0x0C

// TRẠNG THÁI ONLINE CỦA 12 NODES
uint8_t online_led      = 0;
uint8_t online_btn      = 0;
uint8_t online_buz      = 0;
uint8_t online_pot      = 0;
uint8_t online_dist     = 0;
uint8_t online_lcd      = 0;
uint8_t online_motor    = 0;
uint8_t online_servo    = 0;
uint8_t online_oled     = 0;
uint8_t online_soil     = 0;
uint8_t online_dht11    = 0;
uint8_t online_dht22    = 0;

uint8_t timeout_btn     = 0;
uint8_t timeout_pot     = 0;
uint8_t timeout_dist    = 0;
uint8_t timeout_soil    = 0;
uint8_t timeout_dht11   = 0;
uint8_t timeout_dht22   = 0;

// ==========================================
// 3. BIẾN LƯU TRỮ TRẠNG THÁI & DỮ LIỆU CẢM BIẾN
// ==========================================
// Slave 1: LED
uint8_t target_led_mode = 0;   // 0: Tắt, 0x20: Bật, 0x10: Blink, 0x71: Thở, 0x72: Nháy đôi, 0x73: SOS, 0x74: Heartbeat
uint8_t actual_led_mode = 0;

// Slave 2: Nút nhấn thông minh (Gestures)
uint8_t btn_last_gesture = 0x50; // 0x50: Idle, 0x51: Single, 0x52: Double, 0x53: Long Press
uint8_t actual_btn_raw   = 0;    // 0: Nhả, 1: Giữ (Trạng thái vật lý thực tế)
bool is_smart_button     = false; // Tự động nhận diện bản Smart Button (0x56)
uint32_t btn_single_count = 0;
uint32_t btn_double_count = 0;
uint32_t btn_long_count   = 0;
uint32_t btn_total_count  = 0;
String btn_gesture_str    = "IDLE";

// Slave 3: Còi Buzzer
uint8_t target_buz_state = 0;   // 0: Tắt, 1: Bật, 2: Beep
uint8_t actual_buz_state = 0;

// Slave 4: Biến trở B10K
uint8_t actual_pot_value = 0;   // 0 - 255
uint8_t last_pot_value   = 0;

// Slave 5: Siêu âm HC-SR04
uint8_t actual_dist_cm   = 0;   // 0 - 255 cm
uint8_t last_dist_cm     = 0;

// Slave 7: Động cơ DC
uint8_t motor_dir        = 0;   // 0: Dừng, 1: Tiến, 2: Lùi, 3: Phanh
uint8_t motor_speed      = 0;   // 0 - 100%

// Slave 8: Servo RC SG90
uint8_t servo_angle      = 90;  // 0 - 180 độ

// Slave 10: Cảm biến độ ẩm đất LM393
uint8_t soil_moisture_pct = 0;  // 0 - 100%
uint8_t soil_raw_adc      = 0;  // 0 - 255
uint8_t soil_do_state     = 0;  // 0: Đủ ẩm, 1: Khô

// Slave 11: Cảm biến nhiệt ẩm DHT11
uint8_t dht11_temp       = 0;   // °C
uint8_t dht11_humi       = 0;   // %
uint8_t dht11_status     = 0;

// Slave 12: Cảm biến nhiệt ẩm DHT22
uint8_t dht22_temp_int   = 0;
uint8_t dht22_temp_dec   = 0;
uint8_t dht22_humi_int   = 0;
uint8_t dht22_humi_dec   = 0;
uint8_t dht22_status     = 0;

// Hàng đợi gửi lệnh Tầng 2 (Lệnh điều khiển gửi tức thì)
struct PendingCommand {
    bool has_cmd;
    uint8_t target_id;
    uint8_t cmd_data;
    bool is_text;
    uint8_t text_line_cmd;
    char text_buf[17];
};
PendingCommand pending_queue[8];
uint8_t queue_head = 0;
uint8_t queue_tail = 0;

void Queue_Command(uint8_t id, uint8_t cmd) {
    uint8_t next = (queue_head + 1) % 8;
    if (next != queue_tail) {
        pending_queue[queue_head].has_cmd = true;
        pending_queue[queue_head].target_id = id;
        pending_queue[queue_head].cmd_data = cmd;
        pending_queue[queue_head].is_text = false;
        queue_head = next;
    }
}

void Queue_Text_Command(uint8_t id, uint8_t line_cmd, const char* str) {
    uint8_t next = (queue_head + 1) % 8;
    if (next != queue_tail) {
        pending_queue[queue_head].has_cmd = true;
        pending_queue[queue_head].target_id = id;
        pending_queue[queue_head].is_text = true;
        pending_queue[queue_head].text_line_cmd = line_cmd;
        memset(pending_queue[queue_head].text_buf, ' ', 16);
        int len = strlen(str);
        if (len > 16) len = 16;
        memcpy(pending_queue[queue_head].text_buf, str, len);
        pending_queue[queue_head].text_buf[16] = '\0';
        queue_head = next;
    }
}

// ==========================================
// 4. TIỆN ÍCH CHECKSUM VÀ UART RING
// ==========================================
uint8_t Calc_Checksum(uint8_t id, uint8_t data) {
    return (uint8_t)((id + data) & 0xFF);
}

void clear_rx_buffer() {
    while (NEURON_SERIAL.available()) {
        NEURON_SERIAL.read();
    }
}

// Gửi khung chuẩn 4 byte có chờ ACK Dual-Frame
bool Send_Query_Dual_Frame(uint8_t target_id, uint8_t cmd, uint8_t *ack_out, uint32_t timeout_ms) {
    uint8_t tx_frame[4];
    tx_frame[0] = FRAME_HEADER;
    tx_frame[1] = target_id;
    tx_frame[2] = cmd;
    tx_frame[3] = Calc_Checksum(target_id, cmd);

    clear_rx_buffer();
    NEURON_SERIAL.write(tx_frame, 4);
    NEURON_SERIAL.flush();

    uint32_t wait_start = millis();
    uint8_t parser_state = 0;
    uint8_t frames_received = 0;
    uint8_t temp_val = 0;
    bool real_ack = false;

    while ((millis() - wait_start) < timeout_ms) {
        while (NEURON_SERIAL.available()) {
            uint8_t c = NEURON_SERIAL.read();
            switch (parser_state) {
                case 0: if (c == FRAME_HEADER) parser_state = 1; break;
                case 1: parser_state = (c == target_id) ? 2 : 0; break;
                case 2: temp_val = c; parser_state = 3; break;
                case 3:
                    if (c == Calc_Checksum(target_id, temp_val)) {
                        frames_received++;
                        // Nếu là lệnh truy vấn và dữ liệu trả về KHÁC mã hỏi, chắc chắn là Real ACK
                        // Hoặc khi nhận >= 2 frame (Frame 1 Echo, Frame 2 Real ACK)
                        if (temp_val != cmd || frames_received >= 2) {
                            real_ack = true;
                            if (ack_out) *ack_out = temp_val;
                        }
                    }
                    parser_state = 0;
                    break;
            }
            if (real_ack) break;
        }
        if (real_ack) break;
    }
    return real_ack;
}

// Gửi khung text 20 byte cho LCD hoặc OLED
void Send_Text_Frame_20B(uint8_t target_id, uint8_t line_cmd, const char* text16) {
    uint8_t frame[20];
    frame[0] = FRAME_HEADER;
    frame[1] = target_id;
    frame[2] = line_cmd;
    uint8_t crc = (uint8_t)(target_id + line_cmd);
    for (int i = 0; i < 16; i++) {
        frame[3 + i] = (uint8_t)text16[i];
        crc = (uint8_t)(crc + frame[3 + i]);
    }
    frame[19] = crc;

    clear_rx_buffer();
    NEURON_SERIAL.write(frame, 20);
    NEURON_SERIAL.flush();
    delay(2);
}

// ==========================================
// 5. WEB SERVER CẤU HÌNH WIFI (AP MODE)
// ==========================================
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Cấu hình Neuron Gateway V3</title>
    <style>
        body { font-family: sans-serif; background: #0b1329; color: #f8fafc; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; }
        .card { background: #1e293b; padding: 25px; border-radius: 12px; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.5); width: 100%; max-width: 380px; }
        h2 { color: #38bdf8; margin-top: 0; text-align: center; }
        label { display: block; margin-top: 15px; font-size: 14px; color: #94a3b8; }
        input[type="text"], input[type="password"] { width: 100%; padding: 10px; margin-top: 5px; border-radius: 6px; border: 1px solid #334155; background: #0f172a; color: #fff; box-sizing: border-box; }
        button { width: 100%; padding: 12px; margin-top: 25px; background: #0284c7; color: white; border: none; border-radius: 6px; font-size: 16px; font-weight: bold; cursor: pointer; }
        button:hover { background: #0369a1; }
    </style>
</head>
<body>
    <div class="card">
        <h2>Neuron Gateway V3</h2>
        <p style="font-size:12px; color:#94a3b8; text-align:center;">12-Node Ring | Adaptive Multi-Rate Polling</p>
        <form action="/save" method="POST">
            <label>Tên Bộ Kit / Mã Nhóm (Device ID):</label>
            <input type="text" name="devid" placeholder="Ví dụ: NHOM_01 (Mặc định: KIT_xxxx)">
            <label>Tên WiFi (SSID):</label>
            <input type="text" name="ssid" required placeholder="Nhập tên mạng WiFi">
            <label>Mật khẩu WiFi:</label>
            <input type="password" name="pass" placeholder="Nhập mật khẩu (nếu có)">
            <label>IP Máy tính nhận Telemetry (Tùy chọn):</label>
            <input type="text" name="pcip" placeholder="Mặc định: Tự động học từ PC">
            <button type="submit">LƯU & KẾT NỐI</button>
        </form>
    </div>
</body>
</html>
)rawliteral";

void handleRoot() {
    server.send(200, "text/html", HTML_PAGE);
}

void handleSave() {
    if (server.hasArg("ssid")) {
        String s = server.arg("ssid");
        String p = server.arg("pass");
        String pip = server.arg("pcip");
        String did = server.arg("devid");
        did.trim();

        preferences.begin("config", false);
        preferences.putString("ssid", s);
        preferences.putString("pass", p);
        if (pip.length() > 0) preferences.putString("pcip", pip);
        if (did.length() > 0) preferences.putString("devid", did);
        preferences.end();

        server.send(200, "text/html", "<h3 style='font-family:sans-serif; text-align:center; padding-top:40px;'>Da luu thanh cong! ESP32 dang khoi dong lai...</h3>");
        delay(1000);
        ESP.restart();
    } else {
        server.send(400, "text/plain", "Thieu tham so SSID");
    }
}

// ==========================================
// 6. GIAO TIẾP MẠNG UDP VỚI MÁY TÍNH
// ==========================================
void Check_UDP_Commands() {
    int packetSize = udp.parsePacket();
    if (packetSize) {
        IPAddress sender_ip = udp.remoteIP();

        char rx_buf[128] = {0};
        int len = udp.read(rx_buf, sizeof(rx_buf) - 1);
        if (len <= 0) return;
        rx_buf[len] = '\0';
        String cmd = String(rx_buf);
        cmd.trim();

        // --- KHÓA AN TOÀN ĐÍCH DANH (DEVICE ID FILTERING) ---
        // Lệnh có thể kèm tiền tố: @NHOM_01:LED_ON hoặc @KIT_1234:ON hoặc @ALL:RESET_CONFIG
        bool should_update_pc_ip = false;
        if (cmd.startsWith("@")) {
            int colon = cmd.indexOf(':');
            if (colon > 1) {
                String target_id = cmd.substring(1, colon);
                target_id.trim();
                String actual_cmd = cmd.substring(colon + 1);
                actual_cmd.trim();
                if (!target_id.equalsIgnoreCase(device_id) && !target_id.equalsIgnoreCase("ALL")) {
                    // Lệnh này gửi cho bộ kit khác trên cùng WiFi -> BỎ QUA NGAY LẬP TỨC, không cập nhật pc_ip!
                    return;
                }
                cmd = actual_cmd;
                should_update_pc_ip = true;
            } else {
                // Tiền tố @ nhưng không đúng cú pháp -> Bỏ qua ngay
                return;
            }
        } else {
            // Lệnh không có tiền tố @ (như "ON", "OFF" từ ElectroBlocks / App / Hercules gửi trực tiếp)
            should_update_pc_ip = true;
        }

        IPAddress current_broadcast = ~WiFi.subnetMask() | WiFi.localIP();
        if (should_update_pc_ip && sender_ip[0] != 0 && sender_ip[0] != 255 && sender_ip != current_broadcast) {
            sender_ip.toString().toCharArray(pc_ip, sizeof(pc_ip));
        }

        Serial.printf("[UDP Command] [%s] Tu %s: %s\n", device_id, pc_ip, cmd.c_str());

        // --- ĐIỀU KHIỂN ĐÈN LED (SLAVE 1 - SMART EFFECTS) ---
        if (cmd.equals("LED_ON") || cmd.equals("ON")) {
            target_led_mode = 0x20;
            Queue_Command(ID_LED, 0x20);
        }
        else if (cmd.equals("LED_OFF") || cmd.equals("OFF")) {
            target_led_mode = 0x00;
            Queue_Command(ID_LED, 0x30);
        }
        else if (cmd.equals("LED_BLINK") || cmd.equals("BLINK")) {
            target_led_mode = 0x10;
            Queue_Command(ID_LED, 0x10);
        }
        else if (cmd.equals("LED_BREATHE")) {
            target_led_mode = 0x71;
            Queue_Command(ID_LED, 0x71);
        }
        else if (cmd.equals("LED_DOUBLE_FLASH")) {
            target_led_mode = 0x72;
            Queue_Command(ID_LED, 0x72);
        }
        else if (cmd.equals("LED_SOS")) {
            target_led_mode = 0x73;
            Queue_Command(ID_LED, 0x73);
        }
        else if (cmd.equals("LED_HEARTBEAT")) {
            target_led_mode = 0x74;
            Queue_Command(ID_LED, 0x74);
        }
        else if (cmd.startsWith("LED_PWM:")) {
            int val = cmd.substring(8).toInt();
            if (val < 0) val = 0; if (val > 100) val = 100;
            target_led_mode = (uint8_t)val;
            Queue_Command(ID_LED, (uint8_t)val);
        }

        // --- ĐIỀU KHIỂN CÒI BUZZER (SLAVE 3) ---
        else if (cmd.equals("BUZ_ON")) {
            target_buz_state = 1;
            Queue_Command(ID_BUZZER, 0x40);
        }
        else if (cmd.equals("BUZ_OFF")) {
            target_buz_state = 0;
            Queue_Command(ID_BUZZER, 0x41);
        }
        else if (cmd.equals("BUZ_BEEP")) {
            target_buz_state = 2;
            Queue_Command(ID_BUZZER, 0x42);
        }

        // --- ĐIỀU KHIỂN ĐỘNG CƠ DC (SLAVE 7) ---
        else if (cmd.equals("MOTOR_STOP")) {
            motor_dir = 0; motor_speed = 0;
            Queue_Command(ID_MOTOR, 0x00);
        }
        else if (cmd.equals("MOTOR_BRAKE")) {
            motor_dir = 3; motor_speed = 100;
            Queue_Command(ID_MOTOR, 0xFF);
        }
        else if (cmd.startsWith("MOTOR_FWD:")) {
            int spd = cmd.substring(10).toInt();
            if (spd <= 0) {
                motor_dir = 0; motor_speed = 0;
                Queue_Command(ID_MOTOR, 0x00);
            } else {
                if (spd > 100) spd = 100;
                motor_dir = 1; motor_speed = (uint8_t)spd;
                Queue_Command(ID_MOTOR, (uint8_t)spd);
            }
        }
        else if (cmd.startsWith("MOTOR_REV:")) {
            int spd = cmd.substring(10).toInt();
            if (spd <= 0) {
                motor_dir = 0; motor_speed = 0;
                Queue_Command(ID_MOTOR, 0x00);
            } else {
                if (spd > 100) spd = 100;
                motor_dir = 2; motor_speed = (uint8_t)spd;
                Queue_Command(ID_MOTOR, (uint8_t)(0x80 + spd));
            }
        }

        // --- ĐIỀU KHIỂN SERVO RC (SLAVE 8) ---
        else if (cmd.startsWith("SERVO_ANGLE:") || cmd.startsWith("SERVO:")) {
            int colon = cmd.indexOf(':');
            int ang = cmd.substring(colon + 1).toInt();
            if (ang < 0) ang = 0; if (ang > 180) ang = 180;
            servo_angle = (uint8_t)ang;
            Queue_Command(ID_SERVO, (uint8_t)ang);
        }
        else if (cmd.equals("SERVO_LEFT"))  { servo_angle = 0;   Queue_Command(ID_SERVO, 0xC0); }
        else if (cmd.equals("SERVO_MID"))   { servo_angle = 90;  Queue_Command(ID_SERVO, 0xC1); }
        else if (cmd.equals("SERVO_RIGHT")) { servo_angle = 180; Queue_Command(ID_SERVO, 0xC2); }

        // --- ĐIỀU KHIỂN MÀN HÌNH LCD 1602 (SLAVE 6) ---
        else if (cmd.startsWith("LCD1:")) {
            Queue_Text_Command(ID_LCD, 0x11, cmd.substring(5).c_str());
        }
        else if (cmd.startsWith("LCD2:")) {
            Queue_Text_Command(ID_LCD, 0x12, cmd.substring(5).c_str());
        }
        else if (cmd.equals("LCD_CLEAR")) {
            Queue_Command(ID_LCD, 0x01);
        }

        // --- ĐIỀU KHIỂN MÀN HÌNH OLED 0.96" (SLAVE 9) ---
        else if (cmd.startsWith("OLED1:")) {
            Queue_Text_Command(ID_OLED, 0x10, cmd.substring(6).c_str());
        }
        else if (cmd.startsWith("OLED2:")) {
            Queue_Text_Command(ID_OLED, 0x11, cmd.substring(6).c_str());
        }
        else if (cmd.startsWith("OLED3:")) {
            Queue_Text_Command(ID_OLED, 0x12, cmd.substring(6).c_str());
        }
        else if (cmd.startsWith("OLED4:")) {
            Queue_Text_Command(ID_OLED, 0x13, cmd.substring(6).c_str());
        }
        else if (cmd.equals("OLED_CLEAR"))  { Queue_Command(ID_OLED, 0x82); }
        else if (cmd.equals("OLED_INVERT")) { Queue_Command(ID_OLED, 0x83); }
        else if (cmd.equals("OLED_SPLASH")) { Queue_Command(ID_OLED, 0x84); }

        // --- LỆNH CÀI ĐẶT & ĐỌC DEVICE ID TỪ XA ---
        else if (cmd.startsWith("SET_ID:")) {
            String new_id = cmd.substring(7);
            new_id.trim();
            if (new_id.length() > 0 && new_id.length() < 30) {
                preferences.begin("config", false);
                preferences.putString("devid", new_id);
                preferences.end();
                new_id.toCharArray(device_id, sizeof(device_id));
                Serial.printf("[CONFIG] Da cap nhat Device ID thanh: '%s'\n", device_id);
                Send_Telemetry_To_PC();
            }
        }
        else if (cmd.equals("GET_ID")) {
            char id_reply[80];
            snprintf(id_reply, sizeof(id_reply), "{\"type\":\"DEVICE_ID\",\"device_id\":\"%s\"}", device_id);
            udp.beginPacket(pc_ip, pc_port);
            udp.print(id_reply);
            udp.endPacket();
        }

        // --- LỆNH XÓA CẤU HÌNH WIFI TỪ XA ---
        else if (cmd.equals("RESET_CONFIG")) {
            Serial.println("[CONFIG] Nhan lenh RESET_CONFIG -> Khoi dong lai ve AP...");
            preferences.begin("config", false);
            preferences.clear();
            preferences.end();
            delay(500);
            ESP.restart();
        }
    }
}

// ==========================================
// 7. ĐÓNG GÓI TELEMETRY GỬI LÊN APP MÁY TÍNH
// ==========================================
void Send_Telemetry_To_PC() {
    if (WiFi.status() != WL_CONNECTED) return;

    char udp_msg[1536];
    float resistance = ((float)actual_pot_value / 255.0) * 10.0;
    float angle      = ((float)actual_pot_value / 255.0) * 270.0;
    int32_t rssi     = WiFi.RSSI();

    uint8_t btn_raw         = actual_btn_raw;
    uint8_t led_actual_bool = (actual_led_mode > 0) ? 1 : 0;
    uint8_t led_target_bool = (target_led_mode > 0) ? 1 : 0;
    uint8_t buz_actual_bool = (actual_buz_state > 0) ? 1 : 0;
    uint8_t buz_target_bool = (target_buz_state > 0) ? 1 : 0;

    snprintf(udp_msg, sizeof(udp_msg),
        "{"
          "\"timestamp\":%lu,"
          "\"gateway\":{\"device_id\":\"%s\",\"version\":\"V3_CENTRAL_INTEL\",\"wifi_rssi\":%d,\"uptime_s\":%lu},"
          "\"network_topology\":{"
            "\"led_node\":{\"online\":%d,\"timeout_cnt\":0},"
            "\"button_node\":{\"online\":%d,\"timeout_cnt\":0},"
            "\"buzzer_node\":{\"online\":%d,\"timeout_cnt\":0},"
            "\"pot_node\":{\"online\":%d,\"timeout_cnt\":0},"
            "\"distance_node\":{\"online\":%d,\"timeout_cnt\":0},"
            "\"dist_node\":{\"online\":%d,\"timeout_cnt\":0},"
            "\"lcd_node\":{\"online\":%d,\"timeout_cnt\":0},"
            "\"motor_node\":{\"online\":%d,\"timeout_cnt\":0},"
            "\"servo_node\":{\"online\":%d,\"timeout_cnt\":0},"
            "\"oled_node\":{\"online\":%d,\"timeout_cnt\":0},"
            "\"soil_node\":{\"online\":%d,\"timeout_cnt\":0},"
            "\"dht11_node\":{\"online\":%d,\"timeout_cnt\":0},"
            "\"dht22_node\":{\"online\":%d,\"timeout_cnt\":0}"
          "},"
          "\"telemetry\":{"
            "\"button\":{\"raw_state\":%d,\"press_count\":%lu,\"gesture\":\"%s\",\"raw_code\":\"0x%02X\",\"single_clicks\":%lu,\"double_clicks\":%lu,\"long_presses\":%lu,\"total\":%lu},"
            "\"potentiometer\":{\"raw_adc\":%d,\"resistance_kohm\":%.2f,\"mapped_angle_deg\":%.1f},"
            "\"distance\":{\"distance_cm\":%d},"
            "\"soil_moisture\":{\"moisture_pct\":%d,\"raw_adc\":%d,\"dry_alert\":%d},"
            "\"dht11\":{\"temperature_c\":%d,\"humidity_pct\":%d,\"status\":%d},"
            "\"dht22\":{\"temperature_c\":%.1f,\"humidity_pct\":%.1f,\"status\":%d}"
          "},"
          "\"control_state\":{"
            "\"led\":{\"target\":%d,\"actual\":%d},"
            "\"buzzer\":{\"target\":%d,\"actual\":%d},"
            "\"motor\":{\"dir\":%d,\"speed\":%d},"
            "\"servo\":{\"angle\":%d}"
          "},"
          "\"actuators\":{"
            "\"led_mode\":%d,\"buzzer\":%d,\"motor\":{\"dir\":%d,\"speed\":%d},\"servo_angle\":%d"
          "}"
        "}",
        millis(),
        device_id, rssi, millis() / 1000,
        online_led, online_btn, online_buz, online_pot,
        online_dist, online_dist, online_lcd, online_motor, online_servo,
        online_oled, online_soil, online_dht11, online_dht22,
        btn_raw, btn_total_count, btn_gesture_str.c_str(), btn_last_gesture, btn_single_count, btn_double_count, btn_long_count, btn_total_count,
        actual_pot_value, resistance, angle,
        actual_dist_cm,
        soil_moisture_pct, soil_raw_adc, soil_do_state,
        dht11_temp, dht11_humi, dht11_status,
        (float)dht22_temp_int + ((float)dht22_temp_dec / 10.0), (float)dht22_humi_int + ((float)dht22_humi_dec / 10.0), dht22_status,
        led_target_bool, led_actual_bool,
        buz_target_bool, buz_actual_bool,
        motor_dir, motor_speed,
        servo_angle,
        target_led_mode, target_buz_state, motor_dir, motor_speed, servo_angle
    );

    udp.beginPacket(pc_ip, pc_port);
    udp.print(udp_msg);
    udp.endPacket();
}

// ==========================================
// 8. KHỞI TẠO HỆ THỐNG
// ==========================================
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2500);
    delay(300);

    Serial.println("\n=======================================================");
    Serial.println("  NEURON IOT GATEWAY V3 - ADAPTIVE MULTI-RATE POLLING  ");
    Serial.println("  12 Slaves Support | Smart Gestures | Low Latency 20ms");
    Serial.println("=======================================================");

    preferences.begin("config", true);
    String saved_ssid = preferences.getString("ssid", "");
    String saved_pass = preferences.getString("pass", "");
    String nvs_ip     = preferences.getString("pcip", "");
    String nvs_id     = preferences.getString("devid", "");

    // Khởi tạo Device ID mặc định theo 2 byte cuối của MAC nếu chưa từng đặt tên
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char default_id[32];
    snprintf(default_id, sizeof(default_id), "KIT_%02X%02X", mac[4], mac[5]);

    if (nvs_id.length() > 0) {
        nvs_id.toCharArray(device_id, sizeof(device_id));
    } else {
        strncpy(device_id, default_id, sizeof(device_id));
    }
    if (nvs_ip != "") nvs_ip.toCharArray(pc_ip, sizeof(pc_ip));
    preferences.end();

    Serial.printf("[SYSTEM] Device ID: %s (MAC: %s)\n", device_id, WiFi.macAddress().c_str());

    bool wifi_connected = false;

    if (saved_ssid != "") {
        Serial.printf("[WiFi] Dang ket noi toi '%s'...\n", saved_ssid.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.begin(saved_ssid.c_str(), saved_pass.c_str());

        uint32_t start_connect = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start_connect < 8000) {
            delay(200);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            wifi_connected = true;
            IPAddress broadcast = ~WiFi.subnetMask() | WiFi.localIP();

            IPAddress parsed_ip;
            bool same_subnet = false;
            if (nvs_ip != "" && parsed_ip.fromString(nvs_ip)) {
                if ((parsed_ip & WiFi.subnetMask()) == (WiFi.localIP() & WiFi.subnetMask())) {
                    same_subnet = true;
                }
            }

            if (same_subnet) {
                nvs_ip.toCharArray(pc_ip, sizeof(pc_ip));
            } else {
                broadcast.toString().toCharArray(pc_ip, sizeof(pc_ip));
            }

            Serial.printf("\n[WiFi] KET NOI THANH CONG! IP: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("[UDP] Telemetry Target: %s:%d\n", pc_ip, pc_port);

            udp.begin(local_port);
            udp.beginPacket(pc_ip, pc_port);
            char hello_msg[128];
            snprintf(hello_msg, sizeof(hello_msg), "{\"type\":\"HELLO\",\"device_id\":\"%s\",\"tier_polling\":true}", device_id);
            udp.print(hello_msg);
            udp.endPacket();
        }
    }

    if (!wifi_connected) {
        is_ap_mode = true;
        WiFi.mode(WIFI_AP);
        char ap_name[40];
        snprintf(ap_name, sizeof(ap_name), "Neuron_%s_Config", device_id);
        WiFi.softAP(ap_name);
        Serial.printf("\n[AP Mode] Phat AP '%s'\n", ap_name);
        Serial.println("[AP Mode] Vao IP: 192.168.4.1 de cau hinh WiFi");

        server.on("/", HTTP_GET, handleRoot);
        server.on("/save", HTTP_POST, handleSave);
        server.begin();
        udp.begin(local_port);
    }

    NEURON_SERIAL.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    // Initial probe for Tier 2 nodes (LED, Buzzer) to detect presence at startup
    uint8_t probe_ack = 0;
    if (Send_Query_Dual_Frame(ID_LED, 0x70, &probe_ack, 25)) {
        online_led = 1;
    }
    delay(2);
    if (Send_Query_Dual_Frame(ID_BUZZER, 0x41, &probe_ack, 25)) {
        online_buz = 1;
    }
}

// ==========================================
// 9. BỘ ĐIỀU PHỐI ADAPTIVE 3-TIER MULTI-RATE POLLING
// ==========================================
void Master_Network_Loop() {
    bool state_changed = false;
    uint32_t now = millis();

    // ----------------------------------------------------
    // BƯỚC 0: NHẬN LỆNH TỪ PC / APP QUA UDP
    // ----------------------------------------------------
    Check_UDP_Commands();

    // ----------------------------------------------------
    // TẦNG 2: XỬ LÝ HÀNG ĐỢI LỆNH ĐIỀU KHIỂN (EVENT-DRIVEN TỨC THÌ)
    // ----------------------------------------------------
    while (queue_tail != queue_head) {
        PendingCommand p = pending_queue[queue_tail];
        queue_tail = (queue_tail + 1) % 8;

        if (p.is_text) {
            Send_Text_Frame_20B(p.target_id, p.text_line_cmd, p.text_buf);
            if (p.target_id == ID_LCD)  online_lcd = 1;
            if (p.target_id == ID_OLED) online_oled = 1;
            state_changed = true;
            delay(15); // Nghỉ 15ms đảm bảo node hiển thị (LCD/OLED) vẽ xong trang I2C trước khi nhận khung kế
        } else {
            uint8_t ack_res = 0;
            if (Send_Query_Dual_Frame(p.target_id, p.cmd_data, &ack_res, 25)) {
                if (p.target_id == ID_LED) {
                    online_led = 1;
                    actual_led_mode = (p.cmd_data == 0x30 || p.cmd_data == 0x00) ? 0 : 1;
                }
                if (p.target_id == ID_BUZZER) {
                    online_buz = 1;
                    actual_buz_state = target_buz_state;
                }
                if (p.target_id == ID_MOTOR) online_motor = 1;
                if (p.target_id == ID_SERVO) online_servo = 1;
                if (p.target_id == ID_LCD)   online_lcd = 1;
                if (p.target_id == ID_OLED)  online_oled = 1;
                state_changed = true;
            }
        }
        delay(2); // Khoảng nghỉ xả đệm vòng Ring
    }

    // ----------------------------------------------------
    // TẦNG 1: THĂM DÒ SIÊU NHANH (CHU KỲ ~20ms - ƯU TIÊN CAO)
    // ----------------------------------------------------
    static uint32_t last_tier1_poll = 0;
    if (now - last_tier1_poll >= 18) {
        last_tier1_poll = now;

        // 1.1. NÚT NHẤN THÔNG MINH (SLAVE 2) - GESTURES
        uint8_t btn_res = 0;
        if (Send_Query_Dual_Frame(ID_BUTTON, 0x50, &btn_res, 25)) {
            online_btn = 1; timeout_btn = 0;
            if (btn_res != btn_last_gesture) {
                btn_last_gesture = btn_res;
                if (btn_res == 0x51) {
                    btn_single_count++; btn_total_count++;
                    btn_gesture_str = "SINGLE_CLICK";
                    if (!is_smart_button) {
                        actual_btn_raw = 1; // Nút cơ bản: 0x51 là trạng thái đang nhấn
                    } else {
                        actual_btn_raw = 0; // Smart button: 0x51 là sự kiện chốt sau khi đã nhả phím
                    }
                    state_changed = true;
                    Serial.printf("\n>>> [GESTURE] NUT NHAN DON (Single Click #%lu) <<<\n", btn_single_count);
                }
                else if (btn_res == 0x52) {
                    is_smart_button = true;
                    btn_double_count++; btn_total_count++;
                    btn_gesture_str = "DOUBLE_CLICK";
                    actual_btn_raw = 0;
                    state_changed = true;
                    Serial.printf("\n>>> [GESTURE] NUT NHAN DUP (Double Click #%lu) <<<\n", btn_double_count);
                }
                else if (btn_res == 0x53) {
                    is_smart_button = true;
                    btn_long_count++; btn_total_count++;
                    btn_gesture_str = "LONG_PRESS";
                    actual_btn_raw = 1; // Nhấn giữ: nút vẫn đang được bấm giữ vật lý
                    state_changed = true;
                    Serial.printf("\n>>> [GESTURE] NUT NHAN GIU (Long Press #%lu) <<<\n", btn_long_count);
                }
                else if (btn_res == 0x56) {
                    is_smart_button = true;
                    actual_btn_raw = 1;
                    btn_gesture_str = "RAW_PRESSED";
                    state_changed = true;
                }
                else if (btn_res == 0x50) {
                    actual_btn_raw = 0;
                    btn_gesture_str = "IDLE";
                    state_changed = true;
                }
            }
        } else {
            if (++timeout_btn > 3) online_btn = 0;
        }

        delay(2);

        // 1.2. BIẾN TRỞ XOAY B10K (SLAVE 4)
        uint8_t pot_res = 0;
        if (Send_Query_Dual_Frame(ID_POT, 0x60, &pot_res, 25)) {
            online_pot = 1; timeout_pot = 0;
            actual_pot_value = pot_res;
            if (abs(actual_pot_value - last_pot_value) >= 2) {
                last_pot_value = actual_pot_value;
                state_changed = true;
            }
        } else {
            if (++timeout_pot > 3) online_pot = 0;
        }

        delay(2);

        // 1.3. CẢM BIẾN KHOẢNG CÁCH SIÊU ÂM (SLAVE 5)
        // Khi Online: quét liên tục; khi Offline: quét 400ms để nhường bus
        static uint32_t last_dist_poll = 0;
        uint32_t dist_interval = (online_dist) ? 35 : 400;
        if (now - last_dist_poll >= dist_interval) {
            last_dist_poll = now;
            uint8_t dist_res = 0;
            if (Send_Query_Dual_Frame(ID_DISTANCE, 0x70, &dist_res, 48)) {
                online_dist = 1; timeout_dist = 0;
                actual_dist_cm = dist_res;
                if (abs(actual_dist_cm - last_dist_cm) >= 2) {
                    last_dist_cm = actual_dist_cm;
                    state_changed = true;
                }
            } else {
                if (++timeout_dist > 3) online_dist = 0;
            }
            delay(2);
        }
    }

    // ----------------------------------------------------
    // TẦNG 3: THĂM DÒ ĐỊNH KỲ CHẬM (CHU KỲ 1.5s - 2.0s)
    // ----------------------------------------------------
    static uint32_t last_tier3_poll = 0;
    static uint8_t tier3_step = 0;

    // Chia đều các node Tầng 3 cách nhau 500ms để tránh chiếm đường truyền
    if (now - last_tier3_poll >= 500) {
        last_tier3_poll = now;

        if (tier3_step == 0) {
            // 3.1. CẢM BIẾN ĐỘ ẨM ĐẤT LM393 (SLAVE 10)
            uint8_t soil_res = 0;
            if (Send_Query_Dual_Frame(ID_SOIL, 0x70, &soil_res, 20)) {
                online_soil = 1; timeout_soil = 0;
                if (soil_res != soil_moisture_pct) {
                    soil_moisture_pct = soil_res;
                    state_changed = true;
                }
            } else {
                if (++timeout_soil > 3) online_soil = 0;
            }
            delay(2);
            tier3_step = 1;
        }
        else if (tier3_step == 1) {
            // 3.2. CẢM BIẾN NHIỆT ẨM DHT11 (SLAVE 11)
            uint8_t dht_t = 0, dht_h = 0;
            if (Send_Query_Dual_Frame(ID_DHT11, 0x71, &dht_t, 20)) {
                online_dht11 = 1; timeout_dht11 = 0;
                dht11_temp = dht_t;
                dht11_status = 1;
                delay(2);
                if (Send_Query_Dual_Frame(ID_DHT11, 0x72, &dht_h, 20)) {
                    dht11_humi = dht_h;
                }
                state_changed = true;
            } else {
                if (++timeout_dht11 > 3) online_dht11 = 0;
            }
            delay(2);
            tier3_step = 2;
        }
        else if (tier3_step == 2) {
            // 3.3. CẢM BIẾN NHIỆT ẨM DHT22 (SLAVE 12)
            uint8_t dht22_t = 0, dht22_h = 0;
            if (Send_Query_Dual_Frame(ID_DHT22, 0x71, &dht22_t, 20)) {
                online_dht22 = 1; timeout_dht22 = 0;
                dht22_temp_int = dht22_t;
                dht22_status = 1;
                delay(2);
                if (Send_Query_Dual_Frame(ID_DHT22, 0x72, &dht22_h, 20)) {
                    dht22_humi_int = dht22_h;
                }
                delay(2);
                uint8_t t_dec = 0;
                if (Send_Query_Dual_Frame(ID_DHT22, 0x74, &t_dec, 20)) {
                    dht22_temp_dec = t_dec;
                }
                state_changed = true;
            } else {
                if (++timeout_dht22 > 3) online_dht22 = 0;
            }
            delay(2);
            tier3_step = 0;
        }
    }

    // ----------------------------------------------------
    // BƯỚC 4: GỬI GÓI TIN TELEMETRY LÊN MÁY TÍNH
    // ----------------------------------------------------
    static uint32_t last_udp_telemetry = 0;
    static bool pending_state_changed = false;
    const uint32_t TELEMETRY_HEARTBEAT = 100; // 100ms (10Hz mượt mà)

    if (state_changed) {
        pending_state_changed = true;
    }

    if ((pending_state_changed && (now - last_udp_telemetry >= 40)) || (now - last_udp_telemetry >= TELEMETRY_HEARTBEAT)) {
        last_udp_telemetry = now;
        pending_state_changed = false;
        Send_Telemetry_To_PC();
    }
}

void loop() {
    if (is_ap_mode) {
        server.handleClient();
    }

    static uint32_t last_wifi_reconnect = 0;
    if (!is_ap_mode && WiFi.status() != WL_CONNECTED && (millis() - last_wifi_reconnect > 10000)) {
        last_wifi_reconnect = millis();
        WiFi.reconnect();
    }

    Master_Network_Loop();

    // Nháy LED trạng thái GPIO15 (nhanh khi có node online, chậm khi chờ)
    static uint32_t last_blink = 0;
    uint32_t blink_interval = (online_btn || online_pot || online_dist || online_led || online_motor) ? 120 : 600;

    if (millis() - last_blink >= blink_interval) {
        last_blink = millis();
        digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
    }
}

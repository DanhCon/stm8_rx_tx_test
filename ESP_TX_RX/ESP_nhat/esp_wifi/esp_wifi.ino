#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Preferences.h>

// ==========================================
// 1. QUẢN LÝ BỘ NHỚ & MẠNG (NVS & WEB SERVER)
// ==========================================
Preferences preferences;    
WebServer server(80);       

// IP MÁY TÍNH (Khai báo hệt như code cũ của bạn)
char pc_ip[20] = "255.255.255.255";  // global buffer, tránh dangling pointer
const int pc_port = 9999;
const int local_port = 8888;
WiFiUDP udp;
bool is_ap_mode = false; // Cờ báo thiết bị đang ở AP mode (Web cấu hình)

// ==========================================
// 2. CẤU HÌNH MẠNG NEURON (UART RING)
// ==========================================
#define NEURON_SERIAL Serial1
#define RX_PIN 5  
#define TX_PIN 4  
#define STATUS_LED_PIN 15 

// Giao thức Neuron Clone đầy đủ 4 Node
#define FRAME_HEADER 0xAA
#define ID_LED       0x01
#define ID_BUTTON    0x02
#define ID_BUZZER    0x03  
#define ID_POT       0x04  

#define CMD_LED_BLINK 0x10 
#define CMD_LED_ON    0x20 
#define CMD_LED_OFF   0x30 
#define CMD_BTN_READ  0x50 

#define RES_BTN_PRESSED  0x51
#define RES_BTN_RELEASED 0x50

#define CMD_BUZ_ON    0x40
#define CMD_BUZ_OFF   0x41
#define CMD_BUZ_BEEP  0x42

#define CMD_POT_READ  0x60

// Trạng thái kết nối của các Node
uint8_t slave_led_online = 0;
uint8_t slave_btn_online = 0;
uint8_t slave_buz_online = 0;
uint8_t slave_pot_online = 0;

uint8_t timeout_led = 0;
uint8_t timeout_btn = 0;
uint8_t timeout_buz = 0;
uint8_t timeout_pot = 0;

// Trạng thái Logic Hệ thống
uint8_t target_led_state = 0;   // Mục tiêu ESP32 muốn (1: Bật, 0: Tắt)
uint8_t actual_led_state = 0;   // Trạng thái thực tế LED báo về qua ACK
uint8_t actual_btn_state = 0;   // Trạng thái nút bấm thực tế (1: Đang nhấn, 0: Nhả)
uint32_t button_press_count = 0;// Đếm số lần nhấn nút

uint8_t target_buz_state = 0;   // Mục tiêu còi (0: Tắt, 1: Bật, 2: Beep)
uint8_t actual_buz_state = 0;   // Trạng thái thực tế còi phản hồi
uint8_t actual_pot_value = 0;   // Giá trị thực tế biến trở

// ==========================================
// 3. GIAO DIỆN WEB HTML (Giữ nguyên)
// ==========================================
const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 Gateway Setup</title>
    <style>
        body { font-family: Arial, sans-serif; background-color: #f4f4f9; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
        .container { background: white; padding: 30px; border-radius: 10px; box-shadow: 0px 4px 10px rgba(0,0,0,0.1); text-align: center; width: 90%; max-width: 400px;}
        input[type="text"], input[type="password"] { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ccc; border-radius: 5px; box-sizing: border-box;}
        input[type="submit"] { background: #007BFF; color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; font-size: 16px; width: 100%; font-weight: bold;}
        input[type="submit"]:hover { background: #0056b3; }
        .info { margin-top: 15px; font-size: 14px; color: #555; text-align: left; background: #eee; padding: 10px; border-radius: 5px;}
    </style>
</head>
<body>
    <div class="container">
        <h2>Cấu hình Gateway</h2>
        <form action="/save" method="POST">
            <input type="text" name="ssid" placeholder="Tên WiFi (SSID)" required>
            <input type="password" name="pass" placeholder="Mật khẩu WiFi">
            <input type="text" name="pcip" placeholder="IP Máy Tính (vd: 192.168.1.10)" required>
            <br><br>
            <input type="submit" value="Lưu và Khởi động lại">
        </form>
        <div class="info">
            <p>Trạng thái: <b>Đang cấu hình (AP Mode)</b></p>
            <p>IP Access Point: <b>%AP_IP%</b></p>
        </div>
    </div>
</body>
</html>
)=====";

// ==========================================
// 4. LOGIC XỬ LÝ WEB & CHECKSUM
// ==========================================
uint8_t Calc_Checksum(uint8_t id, uint8_t cmd) {
    return (uint8_t)(id + cmd); 
}

void handleRoot() {
    String html = String(INDEX_HTML);
    html.replace("%AP_IP%", WiFi.softAPIP().toString());
    server.send(200, "text/html", html);
}

void handleSave() {
    if (server.hasArg("ssid") && server.hasArg("pcip")) {
        preferences.begin("config", false);
        preferences.putString("ssid", server.arg("ssid"));
        preferences.putString("pass", server.arg("pass"));
        preferences.putString("pcip", server.arg("pcip"));
        preferences.end();
        
        server.send(200, "text/html", "<h2>Đã lưu thành công! Gateway đang khởi động lại...</h2>");
        delay(2000);
        ESP.restart(); 
    } else {
        server.send(400, "text/html", "Lỗi: Không tìm thấy thông tin.");
    }
}

// ==========================================
// 5. CÁC HÀM XỬ LÝ GIAO TIẾP
// ==========================================
void clear_rx_buffer() {
    while (NEURON_SERIAL.available()) NEURON_SERIAL.read();
}

void Check_UDP_Commands() {
    int packetSize = udp.parsePacket();
    if (packetSize) {
        char incomingPacket[255];
        int len = udp.read(incomingPacket, 255);
        if (len > 0) incomingPacket[len] = 0; 
        
        String cmd = String(incomingPacket);
        cmd.trim(); 
        
        Serial.printf("\n[UDP RX] Lệnh từ PC: '%s'\n", cmd.c_str());
        
        if (cmd.equals("ON")) target_led_state = 1;
        else if (cmd.equals("OFF")) target_led_state = 0;
        else if (cmd.equals("BUZ_ON")) target_buz_state = 1;
        else if (cmd.equals("BUZ_OFF")) target_buz_state = 0;
        else if (cmd.equals("BUZ_BEEP")) target_buz_state = 2;
    }
}

void Send_Telemetry_To_PC() {
    if (WiFi.status() != WL_CONNECTED) return; // Chỉ gửi khi có kết nối WiFi
    char udp_msg[768]; // Cấp phát bộ đệm đủ lớn cho cấu trúc phân tầng
    
    // Tính toán trước các đại lượng vật lý (Biến trở B10k: tối đa 10.0 kOhm, góc quay 270 độ)
    float resistance = ((float)actual_pot_value / 255.0) * 10.0;
    float angle = ((float)actual_pot_value / 255.0) * 270.0;
    int32_t rssi = WiFi.RSSI();

    // Đóng gói chuỗi JSON phân tầng chuẩn công nghiệp
    snprintf(udp_msg, sizeof(udp_msg),
        "{"
          "\"timestamp\":%lu,"
          "\"gateway\":{\"device_id\":\"NEURON_GW_ESP32\",\"wifi_rssi\":%d,\"uptime_s\":%lu},"
          "\"network_topology\":{"
            "\"led_node\":{\"online\":%d,\"timeout_cnt\":%d},"
            "\"button_node\":{\"online\":%d,\"timeout_cnt\":%d},"
            "\"buzzer_node\":{\"online\":%d,\"timeout_cnt\":%d},"
            "\"pot_node\":{\"online\":%d,\"timeout_cnt\":%d}"
          "},"
          "\"telemetry\":{"
            "\"button\":{\"raw_state\":%d,\"press_count\":%lu},"
            "\"potentiometer\":{\"raw_adc\":%d,\"resistance_kohm\":%.2f,\"mapped_angle_deg\":%.1f}"
          "},"
          "\"control_state\":{"
            "\"led\":{\"target\":%d,\"actual\":%d},"
            "\"buzzer\":{\"target\":%d,\"actual\":%d}"
          "}"
        "}",
        millis(),
        rssi, millis() / 1000,
        slave_led_online, timeout_led,
        slave_btn_online, timeout_btn,
        slave_buz_online, timeout_buz,
        slave_pot_online, timeout_pot,
        actual_btn_state, button_press_count,
        actual_pot_value, resistance, angle,
        target_led_state, actual_led_state,
        target_buz_state, actual_buz_state
    );

    // Truyền dữ liệu qua mạng UDP vật lý
    udp.beginPacket(pc_ip, pc_port);
    udp.print(udp_msg);
    udp.endPacket();
}

// ==========================================
// 6. SETUP THIẾT BỊ
// ==========================================
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000); // Chờ USB CDC trên ESP32-S3 kết nối
    delay(500); 

    Serial.println("\n[SYSTEM] NEURON IOT GATEWAY BOOTING");

    preferences.begin("config", true); 
    String saved_ssid = preferences.getString("ssid", "");
    String saved_pass = preferences.getString("pass", "");
    
    String nvs_ip = preferences.getString("pcip", "");
    if(nvs_ip != "") nvs_ip.toCharArray(pc_ip, sizeof(pc_ip));  // copy vào buffer, không dùng con trỏ tạm
    preferences.end();

    bool wifi_connected = false;

    if (saved_ssid != "") {
        Serial.printf("[WiFi] Đang kết nối: %s\n", saved_ssid.c_str());
        WiFi.begin(saved_ssid.c_str(), saved_pass.c_str());
        
        uint32_t start_time = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - start_time < 10000)) {
            delay(500); Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            wifi_connected = true;
            // Tự tính subnet broadcast (192.168.x.255) — không cần nhập IP PC thủ công
            IPAddress broadcast = WiFi.localIP();
            broadcast[3] = 255;
            broadcast.toString().toCharArray(pc_ip, sizeof(pc_ip));
            Serial.printf("\n[WiFi] KẾT NỐI THÀNH CÔNG! IP: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("[UDP] Broadcast tới: %s\n", pc_ip);
        }
    }

    if (!wifi_connected) {
        is_ap_mode = true;
        WiFi.mode(WIFI_AP);
        WiFi.softAP("Neuron_Gateway_Config");
        Serial.println("\n[AP Mode] Chưa kết nối WiFi. Phát AP 'Neuron_Gateway_Config'");
        Serial.println("[AP Mode] Truy cập IP: 192.168.4.1 để cấu hình WiFi (Mạng UART vẫn chạy song song)");

        server.on("/", HTTP_GET, handleRoot);
        server.on("/save", HTTP_POST, handleSave);
        server.begin();
    }

    udp.begin(local_port);
    NEURON_SERIAL.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    pinMode(STATUS_LED_PIN, OUTPUT);
}

// ==========================================
// 7. VÒNG LẶP ĐỊNH TUYẾN CHÍNH (GỐC CỦA BẠN)
// ==========================================
void Master_Network_Loop() {
    uint32_t wait_start;
    uint8_t parser_state = 0;
    uint8_t temp_id = 0, temp_cmd = 0;
    
    bool valid_btn_frame = false;
    bool valid_led_frame = false;
    bool valid_buz_frame = false;
    bool valid_pot_frame = false;
    bool state_changed = false; 
    
    static uint8_t last_btn_state = 255;
    static uint8_t last_led_state = 255;
    static uint8_t last_buz_state = 255;
    static uint8_t last_pot_value = 255;

    Check_UDP_Commands();

    /* --- BƯỚC 1: QUÉT TRẠNG THÁI NÚT BẤM (ID: 0x02) --- */
    clear_rx_buffer(); 
    NEURON_SERIAL.write(FRAME_HEADER);
    NEURON_SERIAL.write(ID_BUTTON);
    NEURON_SERIAL.write(CMD_BTN_READ);
    NEURON_SERIAL.write(Calc_Checksum(ID_BUTTON, CMD_BTN_READ));
    NEURON_SERIAL.flush(); 

    wait_start = millis();
    parser_state = 0;
    while ((millis() - wait_start) < 15) {
        while (NEURON_SERIAL.available()) {
            uint8_t c = NEURON_SERIAL.read();
            switch (parser_state) {
                case 0: if (c == FRAME_HEADER) parser_state = 1; break;
                case 1: temp_id = c; parser_state = 2; break;
                case 2: temp_cmd = c; parser_state = 3; break;
                case 3:
                    if (c == Calc_Checksum(temp_id, temp_cmd)) {
                        if (temp_id == ID_BUTTON) { 
                            valid_btn_frame = true;
                            actual_btn_state = (temp_cmd == RES_BTN_PRESSED) ? 1 : 0; 
                        }
                    }
                    parser_state = 0; break;
            }
        }
    }

    if (valid_btn_frame) {
        if (slave_btn_online == 0) state_changed = true; // Node vừa online lại
        slave_btn_online = 1; timeout_btn = 0;
        if (actual_btn_state == 1 && last_btn_state == 0) { 
            button_press_count++;
            target_led_state = !target_led_state; 
            target_buz_state = 2; // Beep còi khi nhấn nút
            Serial.printf("\n[EVENT] Nút được nhấn! Lượt thứ: %d\n", button_press_count);
            state_changed = true;
        }
        if (actual_btn_state != last_btn_state) {
            last_btn_state = actual_btn_state;
            state_changed = true;
        }
    } else {
        if (++timeout_btn > 3) {
            if (slave_btn_online == 1) state_changed = true; // Node vừa bị mất kết nối
            slave_btn_online = 0;
        }
    }
    
    delay(2);

    /* --- BƯỚC 2: RA LỆNH VÀ ĐỌC ACK TỪ LED (ID: 0x01) --- */
    clear_rx_buffer(); 
    uint8_t led_cmd = (target_led_state == 1) ? CMD_LED_ON : CMD_LED_OFF; 
    
    NEURON_SERIAL.write(FRAME_HEADER);
    NEURON_SERIAL.write(ID_LED);
    NEURON_SERIAL.write(led_cmd); 
    NEURON_SERIAL.write(Calc_Checksum(ID_LED, led_cmd));
    NEURON_SERIAL.flush(); 

    wait_start = millis();
    parser_state = 0;
    while ((millis() - wait_start) < 15) {
        while (NEURON_SERIAL.available()) {
            uint8_t c = NEURON_SERIAL.read();
            switch (parser_state) {
                case 0: if (c == FRAME_HEADER) parser_state = 1; break;
                case 1: temp_id = c; parser_state = 2; break;
                case 2: temp_cmd = c; parser_state = 3; break;
                case 3:
                    if (c == Calc_Checksum(temp_id, temp_cmd)) {
                        if (temp_id == ID_LED) {
                            valid_led_frame = true;
                            actual_led_state = (temp_cmd == CMD_LED_ON) ? 1 : 0; 
                        }
                    }
                    parser_state = 0; break;
            }
        }
    }

    if (valid_led_frame) {
        if (slave_led_online == 0) state_changed = true; // Node vừa online lại
        slave_led_online = 1; timeout_led = 0;
        if (actual_led_state != last_led_state) {
            last_led_state = actual_led_state;
            state_changed = true;
        }
    } else {
        if (++timeout_led > 3) {
            if (slave_led_online == 1) state_changed = true; // Node vừa bị mất kết nối
            slave_led_online = 0;
        }
    }
    
    delay(2);

    /* --- SLOT 3: ĐIỀU KHIỂN VÀ ĐỌC ACK TỪ CÒI BUZZER (ID: 0x03) --- */
    clear_rx_buffer();
    uint8_t buz_cmd = CMD_BUZ_OFF;
    if (target_buz_state == 1) buz_cmd = CMD_BUZ_ON;
    else if (target_buz_state == 2) buz_cmd = CMD_BUZ_BEEP;
    
    NEURON_SERIAL.write(FRAME_HEADER);
    NEURON_SERIAL.write(ID_BUZZER);
    NEURON_SERIAL.write(buz_cmd); 
    NEURON_SERIAL.write(Calc_Checksum(ID_BUZZER, buz_cmd));
    NEURON_SERIAL.flush(); 

    wait_start = millis();
    parser_state = 0;
    while ((millis() - wait_start) < 15) {
        while (NEURON_SERIAL.available()) {
            uint8_t c = NEURON_SERIAL.read();
            switch (parser_state) {
                case 0: if (c == FRAME_HEADER) parser_state = 1; break;
                case 1: temp_id = c; parser_state = 2; break;
                case 2: temp_cmd = c; parser_state = 3; break;
                case 3:
                    if (c == Calc_Checksum(temp_id, temp_cmd)) {
                        if (temp_id == ID_BUZZER) {
                            valid_buz_frame = true;
                            if (temp_cmd == CMD_BUZ_ON) actual_buz_state = 1;
                            else if (temp_cmd == CMD_BUZ_OFF) actual_buz_state = 0;
                            else actual_buz_state = 2;
                        }
                    }
                    parser_state = 0; break;
            }
        }
    }

    if (valid_buz_frame) {
        if (slave_buz_online == 0) state_changed = true; // Node vừa online lại
        slave_buz_online = 1; timeout_buz = 0;
        if (target_buz_state == 2) target_buz_state = 0; // Reset trạng thái Beep sau khi gửi xong
        if (actual_buz_state != last_buz_state) {
            last_buz_state = actual_buz_state;
            state_changed = true;
        }
    } else {
        if (++timeout_buz > 3) {
            if (slave_buz_online == 1) state_changed = true; // Node vừa bị mất kết nối
            slave_buz_online = 0;
        }
    }

    delay(2);

    /* --- SLOT 4: QUÉT GIÁ TRỊ BIẾN TRỞ ANALOG (ID: 0x04) --- */
    clear_rx_buffer();
    NEURON_SERIAL.write(FRAME_HEADER);
    NEURON_SERIAL.write(ID_POT);
    NEURON_SERIAL.write(CMD_POT_READ);
    NEURON_SERIAL.write(Calc_Checksum(ID_POT, CMD_POT_READ));
    NEURON_SERIAL.flush(); 

    wait_start = millis();
    parser_state = 0;
    while ((millis() - wait_start) < 15) {
        while (NEURON_SERIAL.available()) {
            uint8_t c = NEURON_SERIAL.read();
            switch (parser_state) {
                case 0: if (c == FRAME_HEADER) parser_state = 1; break;
                case 1: temp_id = c; parser_state = 2; break;
                case 2: temp_cmd = c; parser_state = 3; break;
                case 3:
                    if (c == Calc_Checksum(temp_id, temp_cmd)) {
                        if (temp_id == ID_POT) { 
                            valid_pot_frame = true;
                            actual_pot_value = temp_cmd; 
                        }
                    }
                    parser_state = 0; break;
            }
        }
    }

    if (valid_pot_frame) {
        if (slave_pot_online == 0) state_changed = true; // Node vừa online lại
        slave_pot_online = 1; timeout_pot = 0;
        if (abs(actual_pot_value - last_pot_value) > 3) {
            last_pot_value = actual_pot_value;
            state_changed = true;
        }
    } else {
        if (++timeout_pot > 3) {
            if (slave_pot_online == 1) state_changed = true; // Node vừa bị mất kết nối
            slave_pot_online = 0;
        }
    }

    /* --- BƯỚC 3: GỬI UDP THEO SỰ KIỆN (EVENT-DRIVEN) + HEARTBEAT --- */
    static uint32_t last_udp_send = 0;
    
    // Bình thường gửi Heartbeat 2.5 giây/lần (2500ms)
    // Khi có sự kiện (nút bấm, đổi trạng thái led/còi, xoay biến trở), gửi ngay lập tức!
    const uint32_t HEARTBEAT_INTERVAL = 2500;

    if (state_changed || (millis() - last_udp_send >= HEARTBEAT_INTERVAL)) {
        last_udp_send = millis();
        Send_Telemetry_To_PC(); // Bắn telemetry về PC
    }
} 

void loop() {
    if (is_ap_mode) {
        server.handleClient(); // Xử lý Web cấu hình ngầm nếu đang ở AP mode
    }

    Master_Network_Loop();
    
    static uint32_t last_blink = 0;
    uint32_t blink_interval = (slave_led_online || slave_btn_online || slave_buz_online || slave_pot_online) ? 100 : 500; 

    if (millis() - last_blink >= blink_interval) {
        last_blink = millis();
        digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
    }
}
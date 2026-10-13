#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <Preferences.h>

// ==============================================================================
// NEURON IOT GATEWAY V2 - PHIÊN BẢN NÂNG CẤP TOÀN DIỆN
// - Kế thừa sự ổn định từ ESP_nhat (Non-blocking AP, Event-driven, Timeout 15ms)
// - Cải tiến 1: Tự động bắt IP máy tính (Auto-discovery từ gói UDP)
// - Cải tiến 2: Gói chào hỏi HELLO handshake ngay khi vừa vào WiFi
// - Cải tiến 3: Lệnh xóa cấu hình WiFi từ xa RESET_CONFIG qua mạng
// - Cải tiến 4: Tích hợp đầy đủ Node 5 (Cảm biến khoảng cách HC-SR04)
// ==============================================================================

// ==========================================
// 1. QUẢN LÝ BỘ NHỚ & MẠNG (NVS & WEB SERVER)
// ==========================================
Preferences preferences;    
WebServer server(80);       

char pc_ip[20] = "255.255.255.255";  // Mặc định broadcast hoặc IP tự học từ PC
const int pc_port = 9999;           // Cổng Telemetry PC lắng nghe
const int local_port = 8888;        // Cổng nhận lệnh điều khiển trên ESP32
WiFiUDP udp;
bool is_ap_mode = false;            // Cờ báo thiết bị đang ở AP mode (Web cấu hình)

// ==========================================
// 2. CẤU HÌNH MẠNG NEURON (UART RING)
// ==========================================
#define NEURON_SERIAL Serial1
#define RX_PIN 5  
#define TX_PIN 4  
#define STATUS_LED_PIN 15 

// Giao thức Neuron Clone - Đầy đủ 5 Node STM8
#define FRAME_HEADER     0xAA
#define ID_LED           0x01
#define ID_BUTTON        0x02
#define ID_BUZZER        0x03  
#define ID_POT           0x04  
#define ID_DISTANCE      0x05  // Node 5: Cảm biến khoảng cách HC-SR04

// Lệnh điều khiển
#define CMD_LED_BLINK    0x10 
#define CMD_LED_ON       0x20 
#define CMD_LED_OFF      0x30 
#define CMD_BTN_READ     0x50 
#define RES_BTN_PRESSED  0x51
#define RES_BTN_RELEASED 0x50

#define CMD_BUZ_ON       0x40
#define CMD_BUZ_OFF      0x41
#define CMD_BUZ_BEEP     0x42

#define CMD_POT_READ     0x60
#define CMD_READ_DIST    0x70  // Lệnh đọc khoảng cách từ Slave 5

// Trạng thái kết nối của các Node
uint8_t slave_led_online  = 0;
uint8_t slave_btn_online  = 0;
uint8_t slave_buz_online  = 0;
uint8_t slave_pot_online  = 0;
uint8_t slave_dist_online = 0;

uint8_t timeout_led  = 0;
uint8_t timeout_btn  = 0;
uint8_t timeout_buz  = 0;
uint8_t timeout_pot  = 0;
uint8_t timeout_dist = 0;

// Trạng thái Logic & Giá trị thực tế
uint8_t target_led_state = 0;   // Mục tiêu LED (1: Bật, 0: Tắt, 2: Nháy)
uint8_t actual_led_state = 0;   
uint8_t last_led_state   = 0;

uint8_t actual_btn_state = 0;   // Trạng thái nút (1: Nhấn, 0: Thả)
uint8_t last_btn_state   = 0;   
uint32_t button_press_count = 0;

uint8_t target_buz_state = 0;   // Mục tiêu Còi (1: Bật, 0: Tắt, 2: Beep)
uint8_t actual_buz_state = 0;
uint8_t last_buz_state   = 0;

uint8_t actual_pot_value = 0;   // Giá trị ADC biến trở (0 - 255)
uint8_t last_pot_value   = 0;

uint8_t actual_dist_cm   = 0;   // Giá trị khoảng cách cm từ Slave 5 (0 - 255 cm)
uint8_t last_dist_cm     = 0;

// ==========================================
// 3. HÀM BỔ TRỢ (CHECKSUM & UART BUFFER)
// ==========================================
uint8_t Calc_Checksum(uint8_t id, uint8_t data) {
    return (uint8_t)((id + data) & 0xFF);
}

void clear_rx_buffer() {
    while (NEURON_SERIAL.available()) {
        NEURON_SERIAL.read();
    }
}

// ==========================================
// 4. WEB SERVER CẤU HÌNH WIFI (AP MODE)
// ==========================================
const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Cấu hình Neuron Gateway V2</title>
    <style>
        body { font-family: sans-serif; background: #0f172a; color: #f8fafc; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; }
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
        <h2>Neuron Gateway V2</h2>
        <form action="/save" method="POST">
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

        preferences.begin("config", false);
        preferences.putString("ssid", s);
        preferences.putString("pass", p);
        if (pip.length() > 0) preferences.putString("pcip", pip);
        preferences.end();

        server.send(200, "text/html", "<h3 style='font-family:sans-serif; text-align:center; padding-top:40px;'>Da luu thanh cong! ESP32 dang khoi dong lai...</h3>");
        delay(1000);
        ESP.restart();
    } else {
        server.send(400, "text/plain", "Thieu tham so SSID");
    }
}

// ==========================================
// 5. GIAO TIẾP MẠNG UDP VỚI MÁY TÍNH
// ==========================================
void Check_UDP_Commands() {
    int packetSize = udp.parsePacket();
    if (packetSize) {
        // TỰ ĐỘNG HỌC IP MÁY TÍNH: Bắt lấy IP vừa gửi lệnh để phát ngược lại
        IPAddress sender_ip = udp.remoteIP();
        if (sender_ip[0] != 0 && sender_ip[0] != 255) {
            sender_ip.toString().toCharArray(pc_ip, sizeof(pc_ip));
        }

        char rx_buf[128] = {0};
        int len = udp.read(rx_buf, sizeof(rx_buf) - 1);
        if (len <= 0) return;
        rx_buf[len] = '\0';
        String cmd = String(rx_buf);
        cmd.trim();

        Serial.printf("[UDP Command] Nhận lệnh từ %s: %s\n", pc_ip, cmd.c_str());

        // Lệnh điều khiển Đèn LED (hỗ trợ cả chuẩn app cũ "ON"/"OFF" và mới "LED_ON"/"LED_OFF")
        if (cmd.equals("LED_ON") || cmd.equals("ON"))          target_led_state = 1;
        else if (cmd.equals("LED_OFF") || cmd.equals("OFF"))   target_led_state = 0;
        else if (cmd.equals("LED_BLINK") || cmd.equals("BLINK")) target_led_state = 2;
        
        // Lệnh điều khiển Còi Buzzer
        else if (cmd.equals("BUZ_ON"))     target_buz_state = 1;
        else if (cmd.equals("BUZ_OFF"))    target_buz_state = 0;
        else if (cmd.equals("BUZ_BEEP"))   target_buz_state = 2;

        // Lệnh xóa cấu hình WiFi từ xa
        else if (cmd.equals("RESET_CONFIG")) {
            Serial.println("[CONFIG] Nhận lệnh RESET_CONFIG -> Xóa WiFi, khởi động lại...");
            preferences.begin("config", false);
            preferences.clear();
            preferences.end();
            delay(500);
            ESP.restart();
        }
    }
}

void Send_Telemetry_To_PC() {
    if (WiFi.status() != WL_CONNECTED) return; // Chỉ gửi khi có WiFi

    char udp_msg[896]; 
    
    // Tính toán trước các đại lượng vật lý
    float resistance = ((float)actual_pot_value / 255.0) * 10.0; // Biến trở B10k: tối đa 10k
    float angle      = ((float)actual_pot_value / 255.0) * 270.0;
    int32_t rssi     = WiFi.RSSI();

    // Đóng gói JSON phân tầng chuẩn công nghiệp (Hỗ trợ 5 node)
    snprintf(udp_msg, sizeof(udp_msg),
        "{"
          "\"timestamp\":%lu,"
          "\"gateway\":{\"device_id\":\"NEURON_GW_ESP32\",\"wifi_rssi\":%d,\"uptime_s\":%lu},"
          "\"network_topology\":{"
            "\"led_node\":{\"online\":%d,\"timeout_cnt\":%d},"
            "\"button_node\":{\"online\":%d,\"timeout_cnt\":%d},"
            "\"buzzer_node\":{\"online\":%d,\"timeout_cnt\":%d},"
            "\"pot_node\":{\"online\":%d,\"timeout_cnt\":%d},"
            "\"distance_node\":{\"online\":%d,\"timeout_cnt\":%d}"
          "},"
          "\"telemetry\":{"
            "\"button\":{\"raw_state\":%d,\"press_count\":%lu},"
            "\"potentiometer\":{\"raw_adc\":%d,\"resistance_kohm\":%.2f,\"mapped_angle_deg\":%.1f},"
            "\"distance\":{\"distance_cm\":%d}"
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
        slave_dist_online, timeout_dist,
        actual_btn_state, button_press_count,
        actual_pot_value, resistance, angle,
        actual_dist_cm,
        target_led_state, actual_led_state,
        target_buz_state, actual_buz_state
    );

    udp.beginPacket(pc_ip, pc_port);
    udp.print(udp_msg);
    udp.endPacket();
}

// ==========================================
// 6. KHỞI TẠO THIẾT BỊ
// ==========================================
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000); // Chờ USB CDC trên ESP32-S3
    delay(500); 

    Serial.println("\n==================================================");
    Serial.println("  NEURON IOT GATEWAY V2 - ESP32 MASTER BOOTING   ");
    Serial.println("  Auto-Discovery | 5-Node Ring | Fast-Handshake  ");
    Serial.println("==================================================");

    preferences.begin("config", true); 
    String saved_ssid = preferences.getString("ssid", "");
    String saved_pass = preferences.getString("pass", "");
    String nvs_ip     = preferences.getString("pcip", "");
    if (nvs_ip != "") nvs_ip.toCharArray(pc_ip, sizeof(pc_ip));
    preferences.end();

    bool wifi_connected = false;

    if (saved_ssid != "") {
        Serial.printf("[WiFi] Đang kết nối tới '%s'...\n", saved_ssid.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.begin(saved_ssid.c_str(), saved_pass.c_str());

        uint32_t start_connect = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start_connect < 8000) {
            delay(200);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            wifi_connected = true;
            IPAddress broadcast = WiFi.localIP();
            broadcast[3] = 255;

            // KIỂM TRA DẢI MẠNG: Nếu nvs_ip khác dải mạng WiFi hiện tại thì bỏ qua, dùng Broadcast
            IPAddress parsed_ip;
            bool same_subnet = false;
            if (nvs_ip != "" && parsed_ip.fromString(nvs_ip)) {
                if (parsed_ip[0] == WiFi.localIP()[0] && 
                    parsed_ip[1] == WiFi.localIP()[1] && 
                    parsed_ip[2] == WiFi.localIP()[2]) {
                    same_subnet = true;
                }
            }

            if (same_subnet) {
                nvs_ip.toCharArray(pc_ip, sizeof(pc_ip));
            } else {
                broadcast.toString().toCharArray(pc_ip, sizeof(pc_ip));
            }
            
            Serial.printf("\n[WiFi] KẾT NỐI THÀNH CÔNG! IP: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("[UDP] Telemetry Target: %s:%d\n", pc_ip, pc_port);

            // Gửi gói tin HELLO Handshake ngay để App nhận diện lập tức
            udp.begin(local_port);
            udp.beginPacket(pc_ip, pc_port);
            udp.print("{\"type\":\"HELLO\",\"device_id\":\"NEURON_GW_ESP32\"}");
            udp.endPacket();
        }
    }

    if (!wifi_connected) {
        is_ap_mode = true;
        WiFi.mode(WIFI_AP);
        WiFi.softAP("Neuron_Gateway_Config");
        Serial.println("\n[AP Mode] Chưa kết nối WiFi. Phát AP 'Neuron_Gateway_Config'");
        Serial.println("[AP Mode] Vào IP: 192.168.4.1 để cấu hình (Mạng UART vẫn chạy ngầm)");

        server.on("/", HTTP_GET, handleRoot);
        server.on("/save", HTTP_POST, handleSave);
        server.begin();
        udp.begin(local_port);
    }
    NEURON_SERIAL.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);
}

// ==========================================
// 7. VÒNG QUÉT CHÍNH VÒNG RING UART 5 NODE
// ==========================================
void Master_Network_Loop() {
    uint8_t tx_frame[4];
    uint32_t wait_start;
    uint8_t parser_state = 0;
    bool state_changed = false;

    // --- BƯỚC 1: XỬ LÝ LỆNH TỪ MÁY TÍNH ---
    Check_UDP_Commands();

    // ──────────────────────────────────────────
    // NODE 2: NÚT NHẤN (ID 0x02)
    // ──────────────────────────────────────────
    clear_rx_buffer();
    tx_frame[0] = FRAME_HEADER; tx_frame[1] = ID_BUTTON; tx_frame[2] = CMD_BTN_READ; 
    tx_frame[3] = Calc_Checksum(ID_BUTTON, CMD_BTN_READ);
    NEURON_SERIAL.write(tx_frame, 4);
    NEURON_SERIAL.flush();

    wait_start = millis(); parser_state = 0;
    uint8_t btn_frames_received = 0;
    uint8_t temp_btn_cmd = 0;
    bool real_btn_ack = false;
    while ((millis() - wait_start) < 20) {
        while (NEURON_SERIAL.available()) {
            uint8_t c = NEURON_SERIAL.read();
            switch (parser_state) {
                case 0: if (c == FRAME_HEADER) parser_state = 1; break;
                case 1: parser_state = (c == ID_BUTTON) ? 2 : 0; break;
                case 2:
                    temp_btn_cmd = c;
                    parser_state = 3;
                    break;
                case 3:
                    if (c == Calc_Checksum(ID_BUTTON, temp_btn_cmd)) {
                        btn_frames_received++;
                        // Nếu nhận mã 0x51 (Nhấn) -> chắc chắn từ STM8 vì Echo chỉ có 0x50
                        // Hoặc nếu nhận >= 2 frame -> có Frame 2 (ACK thực sự từ CPU STM8)
                        if (temp_btn_cmd == RES_BTN_PRESSED || btn_frames_received >= 2) {
                            real_btn_ack = true;
                            actual_btn_state = (temp_btn_cmd == RES_BTN_PRESSED) ? 1 : 0;
                        }
                    }
                    parser_state = 0;
                    break;
            }
            if (real_btn_ack) break;
        }
        if (real_btn_ack) break;
    }

    if (real_btn_ack) {
        if (slave_btn_online == 0) state_changed = true;
        slave_btn_online = 1; timeout_btn = 0;
        if (actual_btn_state == 1 && last_btn_state == 0) { 
            button_press_count++;
            Serial.printf("\n>>> [BUTTON EVENT] NÚT ĐƯỢC NHẤN (Lần %lu) -> Gửi UDP lên App! <<<\n", button_press_count);
            state_changed = true; 
        }
        if (actual_btn_state != last_btn_state) {
            last_btn_state = actual_btn_state;
            state_changed = true;
        }
    } else {
        if (++timeout_btn > 3) {
            if (slave_btn_online == 1) state_changed = true;
            slave_btn_online = 0;
        }
    }
    
    delay(2);

    // ──────────────────────────────────────────
    // NODE 1: ĐÈN LED (ID 0x01)
    // ──────────────────────────────────────────
    clear_rx_buffer();
    uint8_t cmd_led = (target_led_state == 2) ? CMD_LED_BLINK : ((target_led_state == 1) ? CMD_LED_ON : CMD_LED_OFF);
    tx_frame[0] = FRAME_HEADER; tx_frame[1] = ID_LED; tx_frame[2] = cmd_led; 
    tx_frame[3] = Calc_Checksum(ID_LED, cmd_led);
    NEURON_SERIAL.write(tx_frame, 4);
    NEURON_SERIAL.flush();

    wait_start = millis(); parser_state = 0;
    uint8_t led_frames_received = 0;
    uint8_t temp_led_cmd = 0;
    bool real_led_ack = false;
    while ((millis() - wait_start) < 20) {
        while (NEURON_SERIAL.available()) {
            uint8_t c = NEURON_SERIAL.read();
            switch (parser_state) {
                case 0: if (c == FRAME_HEADER) parser_state = 1; break;
                case 1: parser_state = (c == ID_LED) ? 2 : 0; break;
                case 2:
                    temp_led_cmd = c;
                    parser_state = 3;
                    break;
                case 3:
                    if (c == Calc_Checksum(ID_LED, temp_led_cmd)) {
                        led_frames_received++;
                        // Frame 1 = Echo từ repeater; Frame 2 = ACK thực sự từ CPU STM8
                        if (led_frames_received >= 2) {
                            real_led_ack = true;
                            actual_led_state = (temp_led_cmd == CMD_LED_ON) ? 1 : ((temp_led_cmd == CMD_LED_BLINK) ? 2 : 0);
                        }
                    }
                    parser_state = 0;
                    break;
            }
            if (real_led_ack) break;
        }
        if (real_led_ack) break;
    }

    if (real_led_ack) {
        if (slave_led_online == 0) state_changed = true;
        slave_led_online = 1; timeout_led = 0;
        if (actual_led_state != last_led_state) {
            last_led_state = actual_led_state;
            state_changed = true;
        }
    } else {
        if (++timeout_led > 3) {
            if (slave_led_online == 1) state_changed = true;
            slave_led_online = 0;
        }
    }
    
    delay(2);

    // ──────────────────────────────────────────
    // NODE 3: CÒI BUZZER (ID 0x03)
    // ──────────────────────────────────────────
    clear_rx_buffer();
    uint8_t cmd_buz = (target_buz_state == 2) ? CMD_BUZ_BEEP : ((target_buz_state == 1) ? CMD_BUZ_ON : CMD_BUZ_OFF);
    tx_frame[0] = FRAME_HEADER; tx_frame[1] = ID_BUZZER; tx_frame[2] = cmd_buz; 
    tx_frame[3] = Calc_Checksum(ID_BUZZER, cmd_buz);
    NEURON_SERIAL.write(tx_frame, 4);
    NEURON_SERIAL.flush();

    wait_start = millis(); parser_state = 0;
    uint8_t buz_frames_received = 0;
    uint8_t temp_buz_cmd = 0;
    bool real_buz_ack = false;
    while ((millis() - wait_start) < 20) {
        while (NEURON_SERIAL.available()) {
            uint8_t c = NEURON_SERIAL.read();
            switch (parser_state) {
                case 0: if (c == FRAME_HEADER) parser_state = 1; break;
                case 1: parser_state = (c == ID_BUZZER) ? 2 : 0; break;
                case 2:
                    temp_buz_cmd = c;
                    parser_state = 3;
                    break;
                case 3:
                    if (c == Calc_Checksum(ID_BUZZER, temp_buz_cmd)) {
                        buz_frames_received++;
                        if (buz_frames_received >= 2) {
                            real_buz_ack = true;
                            actual_buz_state = (temp_buz_cmd == CMD_BUZ_ON) ? 1 : ((temp_buz_cmd == CMD_BUZ_BEEP) ? 2 : 0);
                        }
                    }
                    parser_state = 0;
                    break;
            }
            if (real_buz_ack) break;
        }
        if (real_buz_ack) break;
    }

    if (real_buz_ack) {
        if (slave_buz_online == 0) state_changed = true;
        slave_buz_online = 1; timeout_buz = 0;
        if (target_buz_state == 2) target_buz_state = 0; // Reset Beep sau khi gửi
        if (actual_buz_state != last_buz_state) {
            last_buz_state = actual_buz_state;
            state_changed = true;
        }
    } else {
        if (++timeout_buz > 3) {
            if (slave_buz_online == 1) state_changed = true;
            slave_buz_online = 0;
        }
    }

    delay(2);

    // ──────────────────────────────────────────
    // NODE 4: BIẾN TRỞ (ID 0x04)
    // ──────────────────────────────────────────
    clear_rx_buffer();
    tx_frame[0] = FRAME_HEADER; tx_frame[1] = ID_POT; tx_frame[2] = CMD_POT_READ; 
    tx_frame[3] = Calc_Checksum(ID_POT, CMD_POT_READ);
    NEURON_SERIAL.write(tx_frame, 4);
    NEURON_SERIAL.flush();

    wait_start = millis(); parser_state = 0;
    uint8_t pot_frames_received = 0;
    uint8_t temp_pot_val = 0;
    bool real_pot_ack = false;
    while ((millis() - wait_start) < 20) {
        while (NEURON_SERIAL.available()) {
            uint8_t c = NEURON_SERIAL.read();
            switch (parser_state) {
                case 0: if (c == FRAME_HEADER) parser_state = 1; break;
                case 1: parser_state = (c == ID_POT) ? 2 : 0; break;
                case 2:
                    temp_pot_val = c;
                    parser_state = 3;
                    break;
                case 3:
                    if (c == Calc_Checksum(ID_POT, temp_pot_val)) {
                        pot_frames_received++;
                        // Frame 1 = Echo lệnh hỏi (0x60). Frame 2 = Giá trị ADC thực tế từ STM8
                        if (pot_frames_received >= 2) {
                            real_pot_ack = true;
                            actual_pot_value = temp_pot_val;
                        }
                    }
                    parser_state = 0;
                    break;
            }
            if (real_pot_ack) break;
        }
        if (real_pot_ack) break;
    }

    if (real_pot_ack) {
        if (slave_pot_online == 0) state_changed = true;
        slave_pot_online = 1; timeout_pot = 0;
        if (abs(actual_pot_value - last_pot_value) >= 1) {
            last_pot_value = actual_pot_value;
            state_changed = true;
        }
    } else {
        if (++timeout_pot > 3) {
            if (slave_pot_online == 1) state_changed = true;
            slave_pot_online = 0;
        }
    }

    delay(2);

    // ──────────────────────────────────────────
    // NODE 5: CẢM BIẾN KHOẢNG CÁCH HC-SR04 (ID 0x05)
    // ──────────────────────────────────────────
    static uint32_t last_dist_poll = 0;
    // Khi đang online: quét liên tục; khi offline: chỉ thử thăm dò mỗi 500ms để tránh tốn thời gian chờ
    if (slave_dist_online || (millis() - last_dist_poll >= 500)) {
        last_dist_poll = millis();
        clear_rx_buffer();
        tx_frame[0] = FRAME_HEADER; tx_frame[1] = ID_DISTANCE; tx_frame[2] = CMD_READ_DIST; 
        tx_frame[3] = Calc_Checksum(ID_DISTANCE, CMD_READ_DIST);
        NEURON_SERIAL.write(tx_frame, 4);
        NEURON_SERIAL.flush();

        wait_start = millis(); parser_state = 0;
        uint8_t dist_frames_received = 0;
        uint8_t temp_dist_val = 0;
        bool real_dist_ack = false;
        while ((millis() - wait_start) < 40) {
            while (NEURON_SERIAL.available()) {
                uint8_t c = NEURON_SERIAL.read();
                switch (parser_state) {
                    case 0: if (c == FRAME_HEADER) parser_state = 1; break;
                    case 1: parser_state = (c == ID_DISTANCE) ? 2 : 0; break;
                    case 2:
                        temp_dist_val = c;
                        parser_state = 3;
                        break;
                    case 3:
                        if (c == Calc_Checksum(ID_DISTANCE, temp_dist_val)) {
                            dist_frames_received++;
                            // Frame 1 = Echo lệnh hỏi (0x70). Frame 2 = Khoảng cách cm thực tế từ STM8
                            if (dist_frames_received >= 2) {
                                real_dist_ack = true;
                                actual_dist_cm = temp_dist_val;
                            }
                        }
                        parser_state = 0;
                        break;
                }
                if (real_dist_ack) break;
            }
            if (real_dist_ack) break;
        }

        if (real_dist_ack) {
            if (slave_dist_online == 0) state_changed = true;
            slave_dist_online = 1; timeout_dist = 0;
            if (abs(actual_dist_cm - last_dist_cm) > 2) {
                last_dist_cm = actual_dist_cm;
                state_changed = true;
            }
        } else {
            if (++timeout_dist > 3) {
                if (slave_dist_online == 1) state_changed = true;
                slave_dist_online = 0;
            }
        }
    }

    // --- BƯỚC 3: GỬI UDP THEO SỰ KIỆN HOẶC HEARTBEAT ĐỊNH KỲ ---
    static uint32_t last_udp_send = 0;
    const uint32_t HEARTBEAT_INTERVAL = 100; // 100ms (10 gói/giây - cập nhật mượt mà theo thời gian thực)

    if (state_changed || (millis() - last_udp_send >= HEARTBEAT_INTERVAL)) {
        last_udp_send = millis();
        Send_Telemetry_To_PC();
    }
} 

void loop() {
    if (is_ap_mode) {
        server.handleClient(); // Chạy Web cấu hình nếu đang ở chế độ AP
    }

    Master_Network_Loop();
    
    // Nháy LED trạng thái GPIO15 (nhanh khi có node kết nối, chậm khi cô đơn)
    static uint32_t last_blink = 0;
    uint32_t blink_interval = (slave_led_online || slave_btn_online || slave_buz_online || slave_pot_online || slave_dist_online) ? 100 : 500; 

    if (millis() - last_blink >= blink_interval) {
        last_blink = millis();
        digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
    }
}

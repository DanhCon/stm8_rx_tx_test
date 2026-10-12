#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Preferences.h>

// ==============================================================================
// CHƯƠNG TRÌNH TEST ĐỘC LẬP: ĐIỀU KHIỂN ĐÈN LED STM8 QUA HERCULES / SERIAL MONITOR
// Không cần kết nối App STEM.
// Hỗ trợ:
//   1. Gửi lệnh qua HERCULES SERIAL (hoặc Arduino IDE Serial Monitor)
//   2. Gửi lệnh qua HERCULES UDP (Port 8888)
// Cú pháp lệnh:
//   - Gửi "on" hoặc "ON" hoặc "1"       -> Bật đèn LED (0x20)
//   - Gửi "off" hoặc "OFF" hoặc "0"     -> Tắt đèn LED (0x30)
//   - Gửi "blink" hoặc "nhay"           -> Nháy đèn LED (0x10)
// ==============================================================================

// Cấu hình chân UART nối với bo STM8
#define NEURON_SERIAL Serial1
#define RX_PIN 5            // GPIO5 (RX của ESP32) <- Nối với TX (PD5) của STM8
#define TX_PIN 4            // GPIO4 (TX của ESP32) -> Nối với RX (PD6) của STM8
#define STATUS_LED_PIN 15   // Đèn LED tích hợp trên bo ESP32 (báo trạng thái)

// Giao thức Neuron chuẩn
#define FRAME_HEADER 0xAA
#define ID_LED       0x01
#define CMD_LED_BLINK 0x10
#define CMD_LED_ON   0x20
#define CMD_LED_OFF  0x30

// Mạng UDP
WiFiUDP udp;
const int UDP_PORT = 8888;
bool wifi_connected = false;

// Hàm tính Checksum Modulo 256
uint8_t Calc_Checksum(uint8_t id, uint8_t cmd) {
    return (uint8_t)((id + cmd) & 0xFF);
}

// Xóa bộ đệm RX
void Clear_RX_Buffer() {
    while (NEURON_SERIAL.available()) {
        NEURON_SERIAL.read();
    }
}

// Hàm gửi khung truyền 4 byte xuống STM8 và đọc ACK phản hồi
void Send_To_STM8(uint8_t cmd, const char* cmd_name) {
    uint8_t tx_frame[4];
    tx_frame[0] = FRAME_HEADER;
    tx_frame[1] = ID_LED;
    tx_frame[2] = cmd;
    tx_frame[3] = Calc_Checksum(ID_LED, cmd);

    Clear_RX_Buffer();

    Serial.printf("\n--------------------------------------------------\n");
    Serial.printf("[TX -> STM8] Gửi lệnh: %s (Mã 0x%02X)\n", cmd_name, cmd);
    Serial.printf("[TX Frame] [0x%02X 0x%02X 0x%02X 0x%02X] qua chân GPIO%d...\n", 
                  tx_frame[0], tx_frame[1], tx_frame[2], tx_frame[3], TX_PIN);

    // Bắn 4 byte ra chân TX (GPIO4)
    NEURON_SERIAL.write(tx_frame, 4);
    NEURON_SERIAL.flush();

    // Chờ tối đa 60ms để đọc ACK phản hồi từ STM8 qua chân RX (GPIO5)
    uint32_t t_start = millis();
    uint8_t rx_bytes[16];
    int rx_cnt = 0;

    while (millis() - t_start < 60 && rx_cnt < 16) {
        while (NEURON_SERIAL.available()) {
            rx_bytes[rx_cnt++] = NEURON_SERIAL.read();
        }
        delay(1);
    }

    if (rx_cnt > 0) {
        Serial.printf("[RX <- STM8] Nhận %d byte phản hồi: ", rx_cnt);
        for (int i = 0; i < rx_cnt; i++) {
            Serial.printf("0x%02X ", rx_bytes[i]);
        }
        Serial.printf("\n===> [XÁC NHẬN] STM8 Slave 1 ĐÃ TRẢ LỜI ACK THÀNH CÔNG! <===\n");
    } else {
        Serial.printf("[RX <- STM8] (Không có phản hồi từ chân GPIO%d)\n", RX_PIN);
        Serial.println("  * Lưu ý: Nếu đèn trên STM8 VẪN SÁNG thì đường truyền TX tốt,");
        Serial.printf("    chỉ là chưa cắm dây TX của STM8 (PD5) về RX của ESP32 (GPIO%d).\n", RX_PIN);
        Serial.println("  * Nếu đèn KHÔNG SÁNG: Kiểm tra chung GND và chân PC3 trên STM8.");
    }
    Serial.printf("--------------------------------------------------\n\n");
}

// Xử lý chuỗi lệnh từ Serial hoặc UDP
void Process_Command(String cmd) {
    cmd.trim();
    if (cmd.length() == 0) return;

    Serial.printf("\n>>> [NHẬN ĐƯỢC LỆNH]: '%s'\n", cmd.c_str());

    if (cmd.equalsIgnoreCase("ON") || cmd.equalsIgnoreCase("1") || cmd.equalsIgnoreCase("BAT")) {
        digitalWrite(STATUS_LED_PIN, HIGH); // Bật LED báo trên ESP32
        Send_To_STM8(CMD_LED_ON, "BẬT ĐÈN (ON)");
    } 
    else if (cmd.equalsIgnoreCase("OFF") || cmd.equalsIgnoreCase("0") || cmd.equalsIgnoreCase("TAT")) {
        digitalWrite(STATUS_LED_PIN, LOW);  // Tắt LED báo trên ESP32
        Send_To_STM8(CMD_LED_OFF, "TẮT ĐÈN (OFF)");
    } 
    else if (cmd.equalsIgnoreCase("BLINK") || cmd.equalsIgnoreCase("NHAY")) {
        Send_To_STM8(CMD_LED_BLINK, "NHÁY ĐÈN (BLINK 1s)");
    }
    else {
        Serial.printf("[LỆNH KHÔNG HỢP LỆ]: '%s'. Hãy gửi 'on', 'off' hoặc 'blink'\n", cmd.c_str());
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n========================================================");
    Serial.println("   ESP32 TEST ĐỘC LẬP VỚI HERCULES & STM8 SLAVE 1       ");
    Serial.println("   Không cần App - Test trực tiếp bằng lệnh ON / OFF   ");
    Serial.println("========================================================");

    // Cấu hình LED trạng thái trên ESP32
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    // Khởi tạo UART1 nối với STM8
    NEURON_SERIAL.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    Serial.printf("[UART1] Đã khởi tạo: TX = GPIO%d, RX = GPIO%d, Baud = 9600\n", TX_PIN, RX_PIN);

    // Thử kết nối WiFi lưu trong bộ nhớ (nếu có) để hỗ trợ cả Hercules UDP
    Preferences prefs;
    prefs.begin("config", true);
    String ssid = prefs.getString("ssid", "okok");
    String pass = prefs.getString("pass", "");
    prefs.end();

    if (ssid.length() > 0) {
        Serial.printf("[WiFi] Đang thử kết nối WiFi '%s'...\n", ssid.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());

        uint32_t t_wifi = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t_wifi < 6000) {
            delay(250);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            wifi_connected = true;
            Serial.printf("\n[WiFi OK] IP ESP32: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("[UDP Port] 8888 (Có thể dùng tab UDP trong Hercules)\n");
            udp.begin(UDP_PORT);
        } else {
            Serial.println("\n[WiFi] Không kết nối được WiFi -> Bỏ qua, bạn dùng Serial Hercules bình thường!");
        }
    }

    Serial.println("\n--- HƯỚNG DẪN TEST ---");
    Serial.println("1. Cách 1 (Dễ nhất): Trong Hercules tab 'Serial' (hoặc Serial Monitor):");
    Serial.println("   -> Gõ 'on' rồi ấn Send / Enter để BẬT ĐÈN.");
    Serial.println("   -> Gõ 'off' rồi ấn Send / Enter để TẮT ĐÈN.");
    if (wifi_connected) {
        Serial.printf("2. Cách 2: Trong Hercules tab 'UDP':\n");
        Serial.printf("   -> IP: %s, Port: %d. Gửi 'on' hoặc 'off'\n", WiFi.localIP().toString().c_str(), UDP_PORT);
    }
    Serial.println("----------------------\n");
}

void loop() {
    // 1. Đọc lệnh từ cổng USB Serial (Hercules Serial / Serial Monitor)
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        Process_Command(cmd);
    }

    // 2. Đọc lệnh từ mạng UDP (Hercules UDP) nếu có kết nối WiFi
    if (wifi_connected) {
        int packetSize = udp.parsePacket();
        if (packetSize) {
            char rx_buf[64] = {0};
            int len = udp.read(rx_buf, sizeof(rx_buf) - 1);
            if (len > 0) {
                rx_buf[len] = '\0';
                Process_Command(String(rx_buf));
            }
        }
    }
}

#include <Arduino.h>

/* =========================================================
   ESP32 TEST ĐỘC LẬP: MODULE KHOẢNG CÁCH STM8 (SLAVE 5)
   =========================================================
   - Mạng vòng UART:
     + TX_PIN (GPIO 4) -> Nối vào RX (PD6) của Module đầu tiên
     + RX_PIN (GPIO 5) -> Nối vào TX (PD5) của Module cuối cùng
   - Cách mắc mạch kiểm tra:
     + Cách 1: ESP32 -> Module Khoảng Cách (Slave 5) -> ESP32
     + Cách 2: ESP32 -> Đèn (Slave 1) -> Khoảng Cách (Slave 5) -> ESP32
       (Khi đó: Khoảng cách < 20cm sẽ tự động BẬT ĐÈN LED)
   ========================================================= */

#define NEURON_SERIAL Serial1
#define RX_PIN 5  
#define TX_PIN 4  

#define FRAME_HEADER     0xAA
#define ID_LED           0x01 // Node 1: Đèn LED
#define ID_DISTANCE      0x05 // Node 5: Cảm biến khoảng cách HC-SR04

#define CMD_LED_ON       0x20
#define CMD_LED_OFF      0x30
#define CMD_READ_DIST    0x70 // Lệnh truy vấn khoảng cách

// Hàm tính Checksum Modulo-256
uint8_t Calc_Checksum(uint8_t id, uint8_t data) {
    return (uint8_t)(id + data);
}

// Hàm gửi gói tin chuẩn 4 byte
void SendFrame(uint8_t id, uint8_t cmd) {
    uint8_t crc = Calc_Checksum(id, cmd);
    NEURON_SERIAL.write(FRAME_HEADER);
    NEURON_SERIAL.write(id);
    NEURON_SERIAL.write(cmd);
    NEURON_SERIAL.write(crc);
    NEURON_SERIAL.flush();
}

uint8_t current_dist_cm = 0;
uint8_t last_dist_cm = 255;
bool led_is_on = false;

// Bộ giải mã gói tin
uint8_t rx_state = 0;
uint8_t rx_id = 0;
uint8_t rx_data = 0;
uint8_t frames_received = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n==================================================");
    Serial.println("  ESP32 TEST: MODULE KHOẢNG CÁCH STM8 (SLAVE 5)   ");
    Serial.println("==================================================");
    
    NEURON_SERIAL.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    Serial.printf("[INIT] UART1: TX=GPIO%d, RX=GPIO%d, Baud=9600\n", TX_PIN, RX_PIN);
    Serial.println("[INFO] Đang quét cảm biến khoảng cách HC-SR04...");
    Serial.println("[INFO] Hãy đưa tay lại gần cảm biến (< 20cm) để test!");
    Serial.println("--------------------------------------------------");

    // Tắt đèn ban đầu
    SendFrame(ID_LED, CMD_LED_OFF);
}

void loop() {
    // Xóa bộ đệm cũ trước khi gửi lệnh hỏi mới
    while (NEURON_SERIAL.available()) {
        NEURON_SERIAL.read();
    }

    // 1. GỬI LỆNH HỎI KHOẢNG CÁCH ĐẾN SLAVE 5: [0xAA, 0x05, 0x70, 0x75]
    SendFrame(ID_DISTANCE, CMD_READ_DIST);

    // 2. MỞ CỬA SỔ ĐỌC PHẢN HỒI TRONG 45ms (Đủ cho siêu âm HC-SR04 đo tối đa 30ms + truyền UART)
    frames_received = 0;
    rx_state = 0;
    uint8_t dist_reply = 0;
    uint32_t start_wait = millis();

    while (millis() - start_wait < 45) {
        while (NEURON_SERIAL.available()) {
            uint8_t c = NEURON_SERIAL.read();

            switch (rx_state) {
                case 0: // Chờ Header 0xAA
                    if (c == FRAME_HEADER) rx_state = 1;
                    break;

                case 1: // Nhận Node ID
                    rx_id = c;
                    rx_state = 2;
                    break;

                case 2: // Nhận Data / Command
                    rx_data = c;
                    rx_state = 3;
                    break;

                case 3: // Kiểm tra Checksum
                    if (c == Calc_Checksum(rx_id, rx_data)) {
                        if (rx_id == ID_DISTANCE) {
                            frames_received++;
                            // Gói 1: Echo từ Repeater (0x70)
                            // Gói 2: Giá trị khoảng cách thật (cm) do CPU STM8 đo và gửi về
                            if (frames_received >= 2) {
                                dist_reply = rx_data;
                            }
                        }
                    }
                    rx_state = 0;
                    break;

                default:
                    rx_state = 0;
                    break;
            }
        }
    }

    // 3. HIỂN THỊ KẾT QUẢ & CHẨN ĐOÁN CHI TIẾT TRÊN SERIAL MONITOR
    if (frames_received == 0) {
        Serial.println("❌ [MẤT KẾT NỐI UART] Không nhận được byte nào! Kiểm tra dây: ESP32 TX(GPIO4)->STM8 RX(PD6) và STM8 TX(PD5)->ESP32 RX(GPIO5)");
    } else if (frames_received == 1) {
        Serial.println("⚠️ [CHỈ NHẬN ĐƯỢC ECHO] Vòng UART đã thông nhưng STM8 chưa trả lời! Hãy nạp lại file .s19 mới cho STM8.");
    } else {
        current_dist_cm = dist_reply;

        if (current_dist_cm > 0 && current_dist_cm < 255) {
            Serial.printf("📏 Khoảng cách đo được: %3d cm", current_dist_cm);

            // Cảnh báo vật cản gần (< 20cm)
            if (current_dist_cm < 20) {
                Serial.println("  ⚠️ [CẢNH BÁO: VẬT CẢN GẦN! -> BẬT ĐÈN]");
                if (!led_is_on) {
                    SendFrame(ID_LED, CMD_LED_ON);
                    led_is_on = true;
                }
            } else {
                Serial.println("  ✅ [AN TOÀN]");
                if (led_is_on) {
                    SendFrame(ID_LED, CMD_LED_OFF);
                    led_is_on = false;
                }
            }
        } else if (current_dist_cm == 255) {
            Serial.println("📏 [STM8 ONLINE - TIMEOUT 255] Ngoài tầm đo hoặc chưa ăn chân ECHO(PA1 - Chân 5) / TRIG(PA2 - Chân 6) / Nguồn 5V HC-SR04");
        } else {
            Serial.println("⚠️ [STM8 ONLINE - 0 cm] Hãy nạp lại bản sửa lỗi thanh ghi TIM2 trong Debug\\stm8_slave_khoang_cach.s19!");
        }
    }

    delay(150); // Chu kỳ đo ~200ms/lần
}

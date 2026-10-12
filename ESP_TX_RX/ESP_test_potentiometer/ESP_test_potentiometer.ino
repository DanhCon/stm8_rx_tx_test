#include <Arduino.h>

/* =========================================================
   CẤU HÌNH PHẦN CỨNG UART ESP32-S3
   - Serial1: giao tiếp với STM8
   - RX_PIN (GPIO 5): Nối vào chân TX STM8 (PD5)
   - TX_PIN (GPIO 4): Nối vào chân RX STM8 (PD6)
========================================================= */
#define NEURON_SERIAL Serial1
#define RX_PIN 5  
#define TX_PIN 4  

/* Giao thức Node Biến Trở (Slave 4) */
#define FRAME_HEADER     0xAA
#define ID_POT           0x04 // Node 4: Biến trở
#define CMD_POT_READ     0x60 // Lệnh hỏi giá trị ADC

// Thông số phần cứng biến trở B10k
#define POT_MAX_RESISTANCE 10.0  // 10 kOhm
#define POT_VREF           3.3   // Điện áp tham chiếu 3.3V
#define POT_MAX_ANGLE      270.0 // Góc xoay tối đa 270 độ

// Hàm tính Checksum Modulo-256
uint8_t Calc_Checksum(uint8_t id, uint8_t cmd) {
    return (uint8_t)(id + cmd);
}

// Hàm gửi gói tin hỏi giá trị biến trở: [0xAA] [0x04] [0x60] [0x64]
void SendPotQuery() {
    uint8_t crc = Calc_Checksum(ID_POT, CMD_POT_READ); // 0x04 + 0x60 = 0x64
    
    NEURON_SERIAL.write(FRAME_HEADER);
    NEURON_SERIAL.write(ID_POT);
    NEURON_SERIAL.write(CMD_POT_READ);
    NEURON_SERIAL.write(crc);
    NEURON_SERIAL.flush();
}

// Hàm vẽ thanh tiến trình trực quan trên Serial (ASCII Bar)
void PrintProgressBar(uint8_t percent) {
    Serial.print(" [");
    int bar_width = 20;
    int pos = (percent * bar_width) / 100;
    for (int i = 0; i < bar_width; i++) {
        if (i < pos) Serial.print("=");
        else if (i == pos) Serial.print(">");
        else Serial.print(" ");
    }
    Serial.print("] ");
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000); // Chờ USB CDC trên ESP32-S3
    delay(1000);

    Serial.println("\n========================================================");
    Serial.println("ESP32 TEST MODULE BIEN TRO B10k STM8 (SLAVE 4)");
    Serial.println("========================================================");

    // Khởi tạo UART1 ở 9600 bps
    NEURON_SERIAL.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    Serial.printf("[INIT] UART1: TX=GPIO%d, RX=GPIO%d, Baud=9600\n", TX_PIN, RX_PIN);
    Serial.println("[INFO] Hay xoay bien tro B10k tren module STM8...");
    Serial.println("--------------------------------------------------------");
}

void loop() {
    // 1. GỬI LỆNH HỎI GIÁ TRỊ BIẾN TRỞ
    SendPotQuery();

    delay(30); // Đợi STM8 chuyển đổi ADC và gửi phản hồi

    bool has_data = false;
    uint8_t rx_state = 0;
    uint8_t rx_id = 0;
    uint8_t rx_val = 0;
    uint8_t valid_adc = 0;
    bool frame_valid = false;

    // 2. GIẢI MÃ GÓI TIN NHẬN ĐƯỢC
    while (NEURON_SERIAL.available()) {
        has_data = true;
        uint8_t c = NEURON_SERIAL.read();

        switch (rx_state) {
            case 0:
                if (c == FRAME_HEADER) rx_state = 1;
                break;
            case 1:
                rx_id = c;
                rx_state = 2;
                break;
            case 2:
                rx_val = c;
                rx_state = 3;
                break;
            case 3:
                if (c == Calc_Checksum(rx_id, rx_val)) {
                    if (rx_id == ID_POT) {
                        valid_adc = rx_val;
                        frame_valid = true;
                    }
                }
                rx_state = 0;
                break;
            default:
                rx_state = 0;
                break;
        }
    }

    // 3. TÍNH TOÁN VÀ HIỂN THỊ CÁC ĐẠI LƯỢNG VẬT LÝ CỦA BIẾN TRỞ B10K
    if (frame_valid) {
        float percent = ((float)valid_adc / 255.0) * 100.0;
        float voltage = ((float)valid_adc / 255.0) * POT_VREF;
        float resistance = ((float)valid_adc / 255.0) * POT_MAX_RESISTANCE;
        float angle = ((float)valid_adc / 255.0) * POT_MAX_ANGLE;

        Serial.printf("ADC: %3d/255 | %5.2f V | R: %5.2f kOhm | Goc: %5.1f do | %3.0f%%", 
                      valid_adc, voltage, resistance, angle, percent);
        PrintProgressBar((uint8_t)percent);
        Serial.println();
    } else if (!has_data) {
        Serial.println("[CANH BAO] Khong co phan hoi tu STM8! (Kiem tra day TX/RX/GND hoac nguon)");
    }

    delay(200); // Cập nhật mượt mà 5 lần/giây (200ms)
}

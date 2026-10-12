#include <Arduino.h>

/* =========================================================
   CẤU HÌNH PHẦN CỨNG UART ESP32-S3
   - Serial1: giao tiếp với STM8
   - RX_PIN (GPIO 5): Nối vào chân TX của STM8 (PD5)
   - TX_PIN (GPIO 4): Nối vào chân RX của STM8 (PD6)
========================================================= */
#define NEURON_SERIAL Serial1
#define RX_PIN 5  
#define TX_PIN 4  

/* Định nghĩa giao thức điều khiển Node Đèn */
#define FRAME_HEADER 0xAA
#define ID_LED       0x01
#define CMD_LED_ON   0x20  // Bật đèn
#define CMD_LED_OFF  0x30  // Tắt đèn

// Hàm tính Checksum chuẩn Modulo-256
uint8_t Calc_Checksum(uint8_t id, uint8_t cmd) {
    return (uint8_t)(id + cmd);
}

// Hàm gửi gói tin chuẩn 4 byte: [0xAA] [ID] [CMD] [Checksum]
void SendCommand(uint8_t id, uint8_t cmd) {
    uint8_t crc = Calc_Checksum(id, cmd);
    
    NEURON_SERIAL.write(FRAME_HEADER);
    NEURON_SERIAL.write(id);
    NEURON_SERIAL.write(cmd);
    NEURON_SERIAL.write(crc);
    NEURON_SERIAL.flush(); // Đợi gửi hết ra chân phần cứng
}

void setup() {
    // Serial 0 để xem log trên máy tính
    Serial.begin(115200);
    while (!Serial && millis() < 3000); // Chờ cổng USB CDC trên ESP32-S3 kết nối
    delay(1000);
    Serial.println("\n======================================");
    Serial.println("ESP32 TEST BLINK LED STM8 (SLAVE 1)");
    Serial.println("======================================");

    // Khởi tạo UART1 giao tiếp với STM8
    NEURON_SERIAL.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    Serial.printf("[INIT] UART1 khoi tao: TX=GPIO%d, RX=GPIO%d, Baud=9600\n", TX_PIN, RX_PIN);
}

void loop() {
    // 1. GỬI LỆNH BẬT ĐÈN (0x20)
    Serial.println("-> Gui lenh: BAT DEN (0x20)");
    SendCommand(ID_LED, CMD_LED_ON);
    
    // Đọc phản hồi ACK từ STM8 (nếu có nối dây TX của STM8 về RX của ESP)
    delay(20);
    while (NEURON_SERIAL.available()) {
        uint8_t b = NEURON_SERIAL.read();
        Serial.printf("[ACK RX]: 0x%02X\n", b);
    }
    
    delay(500); // Sáng 500ms

    // 2. GỬI LỆNH TẮT ĐÈN (0x30)
    Serial.println("-> Gui lenh: TAT DEN (0x30)");
    SendCommand(ID_LED, CMD_LED_OFF);
    
    // Đọc phản hồi ACK từ STM8
    delay(20);
    while (NEURON_SERIAL.available()) {
        uint8_t b = NEURON_SERIAL.read();
        Serial.printf("[ACK RX]: 0x%02X\n", b);
    }

    delay(500); // Tắt 500ms
}

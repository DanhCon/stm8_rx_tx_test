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

/* Giao thức Node Nút Nhấn (Slave 2) */
#define FRAME_HEADER     0xAA
#define ID_BUTTON        0x02
#define CMD_BTN_READ     0x50

// Hàm gửi gói tin hỏi nút bấm: [0xAA] [0x02] [0x50] [0x52]
void SendButtonQuery() {
    uint8_t crc = (uint8_t)(ID_BUTTON + CMD_BTN_READ); // 0x02 + 0x50 = 0x52
    
    NEURON_SERIAL.write(FRAME_HEADER);
    NEURON_SERIAL.write(ID_BUTTON);
    NEURON_SERIAL.write(CMD_BTN_READ);
    NEURON_SERIAL.write(crc);
    NEURON_SERIAL.flush(); // Đẩy hết byte ra chân phần cứng
}

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(10); // Ngăn USB CDC bị nghẽn/treo chip nếu máy tính chưa kịp đọc
    delay(1000);

    Serial.println("\n==========================================");
    Serial.println("ESP32 TEST MODULE NUT NHAN STM8 (SLAVE 2)");
    Serial.println("==========================================");

    // Khởi tạo UART1 ở 9600 bps
    NEURON_SERIAL.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    Serial.printf("[INIT] UART1: TX=GPIO%d, RX=GPIO%d, Baud=9600\n", TX_PIN, RX_PIN);
    Serial.println("------------------------------------------");
}

void loop() {
    // 1. GỬI LỆNH HỎI THĂM TRẠNG THÁI NÚT
    Serial.println("-> Gui lenh: HOI TRANG THAI NUT (0x50)");
    SendButtonQuery();

    delay(30); // Đợi STM8 phản hồi (ở 9600 baud mất ~10ms)

    bool has_data = false;
    uint8_t btn_state = 0;

    // 2. ĐỌC VÀ IN TOÀN BỘ CÁC BYTE NHẬN ĐƯỢC
    while (NEURON_SERIAL.available()) {
        has_data = true;
        uint8_t b = NEURON_SERIAL.read();
        Serial.printf("[RX]: 0x%02X  ", b);

        if (b == 0x51) {
            btn_state = 0x51; // Phát hiện trạng thái ĐANG NHẤN
        } else if (b == 0x50 && btn_state != 0x51) {
            btn_state = 0x50; // Trạng thái ĐANG NHẢ
        }
    }

    // 3. ĐÁNH GIÁ KẾT QUẢ
    if (has_data) {
        Serial.println();
        if (btn_state == 0x51) {
            Serial.println("===> KET QUA: >>> NUT DANG NHAN (PRESSED) <<<");
        } else if (btn_state == 0x50) {
            Serial.println("===> KET QUA: Nut dang nha (RELEASED)");
        }
    } else {
        Serial.println("[CANH BAO] Khong co phan hoi tu STM8! (Kiem tra day TX/RX/GND hoac nguon)");
    }

    Serial.println("------------------------------------------");
    delay(500); // Quét lại sau 500ms
}

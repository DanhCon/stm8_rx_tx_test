#include <Arduino.h>

/* =========================================================
   CẤU HÌNH PHẦN CỨNG UART ESP32-S3
   - Serial1: Mạng vòng UART Neuron Ring
   - RX_PIN (GPIO 5): Nhận dữ liệu từ Node cuối vòng về
   - TX_PIN (GPIO 4): Bắn dữ liệu vào Node đầu vòng
========================================================= */
#define NEURON_SERIAL Serial1
#define RX_PIN 5  
#define TX_PIN 4  

/* Định nghĩa giao thức Neuron Clone */
#define FRAME_HEADER     0xAA

#define ID_LED           0x01 // Node 1: Đèn LED
#define ID_BUTTON        0x02 // Node 2: Nút Nhấn
#define ID_VRES          0x02 // Node 3: Biến trở
// Các mã lệnh (Command)
#define CMD_LED_ON       0x20 // Lệnh Bật LED
#define CMD_LED_OFF      0x30 // Lệnh Tắt LED
#define CMD_BTN_READ     0x50 // Lệnh hỏi trạng thái nút

// Trạng thái nút bấm phản hồi
#define RES_BTN_RELEASED 0x50 // Nút đang nhả
#define RES_BTN_PRESSED  0x51 // Nút đang nhấn

// Hàm tính Checksum chuẩn Modulo-256
uint8_t Calc_Checksum(uint8_t id, uint8_t cmd) {
    return (uint8_t)(id + cmd);
}

// Hàm gửi gói tin chuẩn 4 byte: [0xAA] [ID] [CMD] [Checksum]
void SendFrame(uint8_t id, uint8_t cmd) {
    uint8_t crc = Calc_Checksum(id, cmd);
    
    NEURON_SERIAL.write(FRAME_HEADER);
    NEURON_SERIAL.write(id);
    NEURON_SERIAL.write(cmd);
    NEURON_SERIAL.write(crc);
    NEURON_SERIAL.flush(); // Đẩy sạch ra chân TX
}

// Biến lưu trạng thái nút
uint8_t current_btn_state = RES_BTN_RELEASED;
uint8_t last_btn_state = RES_BTN_RELEASED;

// Biến giải mã gói tin nhận về (State Machine Parser)
uint8_t rx_state = 0;
uint8_t rx_id = 0;
uint8_t rx_cmd = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n==================================================");
    Serial.println("ESP32 TEST: NHAN NUT BAM -> DIEU KHIEN DEN SANG");
    Serial.println("==================================================");

    // Khởi tạo UART1 ở 9600 bps
    NEURON_SERIAL.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    Serial.printf("[INIT] UART1: TX=GPIO%d, RX=GPIO%d, Baud=9600\n", TX_PIN, RX_PIN);
    Serial.println("[INFO] Hay bam nut tren Module STM8 de xem Den phan ung...");
    Serial.println("--------------------------------------------------");

    // Đảm bảo ban đầu tắt đèn
    SendFrame(ID_LED, CMD_LED_OFF);
}

void loop() {
    static uint32_t last_poll_time = 0;

    // 1. LIÊN TỤC GIẢI MÃ DỮ LIỆU ĐẾN TỪ MẠNG VÒNG (Non-blocking)
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

            case 2: // Nhận Command / Data
                rx_cmd = c;
                rx_state = 3;
                break;

            case 3: // Kiểm tra Checksum Modulo-256
                if (c == Calc_Checksum(rx_id, rx_cmd)) {
                    // Nếu là gói trạng thái từ Node Nút bấm (ID: 0x02)
                    if (rx_id == ID_BUTTON) {
                        current_btn_state = rx_cmd;
                    }
                }
                rx_state = 0;
                break;

            default:
                rx_state = 0;
                break;
        }
    }

    // 2. CHU KỲ QUÉT (POLLING): Cứ mỗi 40ms hỏi thăm nút bấm 1 lần (25Hz)
    if (millis() - last_poll_time >= 40) {
        last_poll_time = millis();
        SendFrame(ID_BUTTON, CMD_BTN_READ);
    }

    // 3. ĐIỀU KHIỂN HƯỚNG SỰ KIỆN (EVENT-DRIVEN):
    // Chỉ gửi lệnh điều khiển đèn khi trạng thái nút THAY ĐỔI
    if (current_btn_state != last_btn_state) {
        if (current_btn_state == RES_BTN_PRESSED) {
            Serial.println(">>> [EVENT] PHAT HIEN NUT NHAN -> RA LENH BAT DEN (0x20) <<<");
            SendFrame(ID_LED, CMD_LED_ON); // Bật đèn
        } 
        else if (current_btn_state == RES_BTN_RELEASED) {
            Serial.println("--- [EVENT] NUT DA NHA        -> RA LENH TAT DEN (0x30) ---");
            SendFrame(ID_LED, CMD_LED_OFF); // Tắt đèn
        }
        last_btn_state = current_btn_state;
    }
}

#include <Arduino.h>

/* =========================================================
   CẤU HÌNH PHẦN CỨNG ESP32 UART
   Sử dụng Serial1 (UART1) để giao tiếp với mạng Neuron
========================================================= */
#define NEURON_SERIAL Serial1
#define RX_PIN 5  // Nối vào TX của STM8 (Chân 5 STM8)
#define TX_PIN 4  // Nối vào RX của STM8 (Chân 6 STM8)

/* =========================================================
   ĐỊNH NGHĨA GIAO THỨC NEURON CLONE
   Frame: [0xAA] [ID] [CMD] [0x0A]
========================================================= */
#define FRAME_HEADER 0xAA
#define FRAME_TAIL   0x0A

/* Địa chỉ các Node */
#define ID_LED       0x01
#define ID_BUTTON    0x02

/* Các mã lệnh (Command) */
#define CMD_LED_BLINK 0x10 // Nháy 1s báo kết nối
#define CMD_LED_ON    0x20 // Bật LED
#define CMD_LED_OFF   0x30 // Tắt LED
#define CMD_BTN_READ  0x50 // Lệnh hỏi trạng thái nút

/* Trạng thái trả về từ Button */
#define RES_BTN_PRESSED  0x51
#define RES_BTN_RELEASED 0x50

/* =========================================================
   BIẾN TOÀN CỤC HỆ THỐNG
========================================================= */
uint8_t button_state = RES_BTN_RELEASED; // Mặc định là nhả
uint8_t last_button_state = 0xFF;        // Lưu trạng thái cũ để so sánh

// Biến cho State Machine RX
uint8_t rx_state = 0;
uint8_t rx_id = 0;
uint8_t rx_cmd = 0;

/* =========================================================
   HÀM GỬI GÓI TIN CHUẨN
========================================================= */
void SendFrame(uint8_t target_id, uint8_t cmd) {
    NEURON_SERIAL.write(FRAME_HEADER);
    NEURON_SERIAL.write(target_id);
    NEURON_SERIAL.write(cmd);
    NEURON_SERIAL.write(FRAME_TAIL);
}

/* =========================================================
   HÀM NHẬN GÓI TIN KHÔNG NGHẼN (NON-BLOCKING)
   Hàm này phải được gọi liên tục trong loop()
========================================================= */
void ProcessIncomingData() {
    while (NEURON_SERIAL.available()) {
        uint8_t c = NEURON_SERIAL.read();

        switch (rx_state) {
            case 0: // WAIT HEADER
                if (c == FRAME_HEADER) rx_state = 1;
                break;
            case 1: // GET ID
                rx_id = c;
                rx_state = 2;
                break;
            case 2: // GET CMD
                rx_cmd = c;
                rx_state = 3;
                break;
            case 3: // WAIT TAIL
                if (c == FRAME_TAIL) {
                    // Đã nhận được 1 Frame hoàn chỉnh
                    if (rx_id == ID_BUTTON) {
                        button_state = rx_cmd; // Cập nhật trạng thái nút
                    }
                }
                rx_state = 0; // Reset Parser
                break;
            default:
                rx_state = 0;
                break;
        }
    }
}

/* =========================================================
   SETUP
========================================================= */
void setup() {
    // Serial giao tiếp với máy tính để Debug
    Serial.begin(115200);
    
    // Serial giao tiếp với mạng Neuron
    NEURON_SERIAL.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    
    Serial.println("ESP32 NEURON MASTER STARTING...");
    delay(1000); // Đợi mạng điện STM8 ổn định

    // Gửi lệnh kết nối (Nháy LED 1 giây) để báo hiệu mạng khởi động
    Serial.println("Sending Connection Signal to LED Module...");
    SendFrame(ID_LED, CMD_LED_BLINK);
}

/* =========================================================
   MAIN LOOP (SCHEDULER THỜI GIAN THỰC)
========================================================= */
void loop() {
    static unsigned long last_poll_time = 0;
    
    // 1. LIÊN TỤC XỬ LÝ DỮ LIỆU ĐẾN
    ProcessIncomingData();

    // 2. CHU KỲ QUÉT (POLLING) - Cứ mỗi 50ms hỏi nút bấm 1 lần (20Hz)
    if (millis() - last_poll_time >= 50) {
        last_poll_time = millis();
        
        // Hỏi thăm trạng thái của Slave số 2 (Button)
        SendFrame(ID_BUTTON, CMD_BTN_READ);
    }

    // 3. LOGIC ĐIỀU KHIỂN (EVENT-DRIVEN)
    // Chỉ ra lệnh cho LED khi trạng thái nút nhấn thay đổi,
    // Tránh spam đường truyền liên tục làm nghẽn STM8
    if (button_state != last_button_state) {
        if (button_state == RES_BTN_PRESSED) {
            Serial.println("Button PRESSED -> Sending Turn ON to LED");
            SendFrame(ID_LED, CMD_LED_ON);
        } else if (button_state == RES_BTN_RELEASED) {
            Serial.println("Button RELEASED -> Sending Turn OFF to LED");
            SendFrame(ID_LED, CMD_LED_OFF);
        }
        last_button_state = button_state; // Cập nhật cờ
    }
}
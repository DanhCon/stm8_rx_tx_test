#include <Arduino.h>

/* =========================================================
   ESP32 TEST ĐỘC LẬP: MODULE MÀN HÌNH LCD 1602 I2C (SLAVE 6)
   =========================================================
   - Mạng vòng UART:
     + TX_PIN (GPIO 4) -> RX (PD6) của STM8 Slave 6 LCD
     + RX_PIN (GPIO 5) -> TX (PD5) của STM8 Slave 6 LCD
     + Tốc độ: 9600 bps
   - Giao thức gửi chuỗi 16 ký tự:
     [0xAA] [0x06] [0x11 hoặc 0x12] [16 byte ASCII] [Checksum Modulo-256]
     + 0x11: Ghi Dòng 1 (tối đa 16 ký tự)
     + 0x12: Ghi Dòng 2 (tối đa 16 ký tự)
   - Lệnh 4 byte điều khiển nhanh:
     + 'clear' : Xóa trắng màn hình (CMD 0x01)
     + 'demo'  : Chạy demo đếm số thời gian thực
   ========================================================= */

#define NEURON_SERIAL Serial1
#define RX_PIN 5  
#define TX_PIN 4  

#define FRAME_HEADER     0xAA
#define ID_LCD           0x06 // Node 6: Module Màn hình LCD 1602

#define CMD_LCD_CLEAR    0x01
#define CMD_LCD_LINE1    0x11 // Ghi dòng 1 (16 ký tự)
#define CMD_LCD_LINE2    0x12 // Ghi dòng 2 (16 ký tự)

// Hàm tính Checksum 4-byte
uint8_t Calc_Checksum4(uint8_t id, uint8_t cmd) {
    return (uint8_t)(id + cmd);
}

// Hàm gửi lệnh 4-byte (Clear,...)
void Send_LCD_Cmd4(uint8_t cmd) {
    uint8_t crc = Calc_Checksum4(ID_LCD, cmd);
    NEURON_SERIAL.write(FRAME_HEADER);
    NEURON_SERIAL.write(ID_LCD);
    NEURON_SERIAL.write(cmd);
    NEURON_SERIAL.write(crc);
    NEURON_SERIAL.flush();
}

// Hàm gửi chuỗi văn bản lên dòng LCD (line: 1 hoặc 2)
void Send_LCD_Print(uint8_t line, const char* text) {
    uint8_t cmd = (line == 1) ? CMD_LCD_LINE1 : CMD_LCD_LINE2;
    uint8_t crc = (uint8_t)(ID_LCD + cmd);
    char padded[17];
    memset(padded, ' ', 16);
    padded[16] = '\0';

    // Copy tối đa 16 ký tự, phần còn lại đệm khoảng trắng
    int len = strlen(text);
    if (len > 16) len = 16;
    memcpy(padded, text, len);

    // Bắt đầu truyền Frame Text (Tổng 20 byte)
    NEURON_SERIAL.write(FRAME_HEADER);
    NEURON_SERIAL.write(ID_LCD);
    NEURON_SERIAL.write(cmd);

    // Gửi 16 ký tự và cộng dồn Checksum
    for (int i = 0; i < 16; i++) {
        uint8_t b = (uint8_t)padded[i];
        NEURON_SERIAL.write(b);
        crc = (uint8_t)(crc + b);
    }

    // Gửi Checksum Modulo-256
    NEURON_SERIAL.write(crc);
    NEURON_SERIAL.flush();
}

bool demo_mode = true;
uint32_t counter = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================================");
    Serial.println("  ESP32 TEST: MODULE MÀN HÌNH LCD 1602 I2C (SLAVE 6)    ");
    Serial.println("========================================================");

    NEURON_SERIAL.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    Serial.printf("[INIT] UART1: TX=GPIO%d, RX=GPIO%d, Baud=9600\n", TX_PIN, RX_PIN);
    Serial.println("\n--- HƯỚNG DẪN ĐIỀU KHIỂN QUA SERIAL MONITOR ---");
    Serial.println("1. Gõ: '1:Xin chao cac ban' -> In len Dong 1.");
    Serial.println("2. Gõ: '2:FabLab STEM IoT'  -> In len Dong 2.");
    Serial.println("3. Gõ: 'clear'              -> Xoa trang man hinh.");
    Serial.println("4. Gõ: 'demo'               -> Bat lai che do demo dem so tu dong.");
    Serial.println("--------------------------------------------------------\n");

    // In lời chào ban đầu lên LCD
    Send_LCD_Print(1, "ESP32 MASTER OK");
    Send_LCD_Print(2, "SLAVE 6: LCD1602");
    delay(2000);
}

void loop() {
    // 1. NHẬN LỆNH TỪ SERIAL MONITOR
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();

        if (input.equalsIgnoreCase("clear")) {
            demo_mode = false;
            Send_LCD_Cmd4(CMD_LCD_CLEAR);
            Serial.println(">>> Đã gửi lệnh xóa màn hình LCD!");
        } else if (input.equalsIgnoreCase("demo")) {
            demo_mode = true;
            Serial.println(">>> Bật lại chế độ demo đếm số tự động!");
        } else if (input.startsWith("1:")) {
            demo_mode = false;
            String text = input.substring(2);
            Send_LCD_Print(1, text.c_str());
            Serial.printf(">>> In Dòng 1: '%s'\n", text.c_str());
        } else if (input.startsWith("2:")) {
            demo_mode = false;
            String text = input.substring(2);
            Send_LCD_Print(2, text.c_str());
            Serial.printf(">>> In Dòng 2: '%s'\n", text.c_str());
        } else {
            // Mặc định in vào Dòng 1
            demo_mode = false;
            Send_LCD_Print(1, input.c_str());
            Serial.printf(">>> In Dòng 1: '%s'\n", input.c_str());
        }
    }

    // 2. CHẾ ĐỘ DEMO TỰ ĐỘNG (Đếm số mỗi giây)
    if (demo_mode) {
        static uint32_t last_demo_time = 0;
        if (millis() - last_demo_time >= 1000) {
            last_demo_time = millis();
            counter++;

            char line1[17];
            char line2[17];
            snprintf(line1, sizeof(line1), "DEMO COUNT: %4lu", counter);
            snprintf(line2, sizeof(line2), "UPTIME: %6lus", millis() / 1000);

            Send_LCD_Print(1, line1);
            Send_LCD_Print(2, line2);
        }
    }

    // 3. ĐỌC VÀ XẢ PHẢN HỒI ACK TỪ STM8
    while (NEURON_SERIAL.available()) {
        NEURON_SERIAL.read();
    }
}

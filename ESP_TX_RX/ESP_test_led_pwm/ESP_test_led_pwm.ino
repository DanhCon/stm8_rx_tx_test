#include <Arduino.h>

/* =========================================================
   ESP32 TEST: ĐIỀU CHỈNH ĐỘ SÁNG LED STM8 (PWM DIMMING)
   =========================================================
   - Nối chân:
     + TX_PIN (GPIO 4) -> RX (PD6) của Module Đèn
     + RX_PIN (GPIO 5) -> TX (PD5) của Module Đèn (hoặc Node cuối vòng)
   - Chế độ test:
     + Cách 1: Gõ số từ 0 đến 100 vào Serial Monitor để chỉnh độ sáng!
     + Cách 2: Tự động chạy hiệu ứng đèn thở (Breathing Effect) nếu không có lệnh.
   ========================================================= */

#define NEURON_SERIAL Serial1
#define RX_PIN 5  
#define TX_PIN 4  

#define FRAME_HEADER     0xAA
#define ID_LED           0x01 // Node 1: Đèn LED

uint8_t Calc_Checksum(uint8_t id, uint8_t cmd) {
    return (uint8_t)(id + cmd);
}

// Hàm gửi lệnh chỉnh độ sáng LED (percent: 0% đến 100%)
void Send_LED_Brightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    
    // Gửi lệnh dạng offset: 0x80 + percent (0x80 = 0%, 0xE4 = 100%)
    uint8_t cmd = 0x80 + percent;
    uint8_t crc = Calc_Checksum(ID_LED, cmd);

    NEURON_SERIAL.write(FRAME_HEADER);
    NEURON_SERIAL.write(ID_LED);
    NEURON_SERIAL.write(cmd);
    NEURON_SERIAL.write(crc);
    NEURON_SERIAL.flush();
}

bool auto_breath_mode = true;
int breath_brightness = 0;
int breath_step = 2; // Tốc độ tăng giảm

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================================");
    Serial.println("  ESP32 TEST: ĐIỀU CHỈNH ĐỘ SÁNG LED STM8 BẰNG PWM      ");
    Serial.println("========================================================");
    
    NEURON_SERIAL.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
    Serial.printf("[INIT] UART1: TX=GPIO%d, RX=GPIO%d, Baud=9600\n", TX_PIN, RX_PIN);
    Serial.println("\n--- HƯỚNG DẪN ĐIỀU KHIỂN ---");
    Serial.println("1. Gõ một số từ 0 đến 100 rồi nhấn Enter để chỉnh độ sáng.");
    Serial.println("   Ví dụ: '0' (tắt), '30' (sáng mờ), '75', '100' (sáng cực đại).");
    Serial.println("2. Gõ 'auto' để bật lại chế độ đèn thở tự động.");
    Serial.println("--------------------------------------------------------\n");
    Serial.println("[INFO] Đang chạy chế độ đèn thở tự động (Breathing)...");
}

void loop() {
    // 1. NHẬN LỆNH ĐỘ SÁNG TỪ SERIAL MONITOR (Người dùng nhập)
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();

        if (input.equalsIgnoreCase("auto")) {
            auto_breath_mode = true;
            Serial.println(">>> Đã bật lại chế độ Đèn Thở tự động!");
        } else {
            int val = input.toInt();
            if (val >= 0 && val <= 100) {
                auto_breath_mode = false;
                Send_LED_Brightness((uint8_t)val);
                Serial.printf(">>> [THỦ CÔNG] Đã đặt độ sáng LED: %d%%\n", val);
            } else {
                Serial.println("[!] Vui lòng nhập số từ 0 đến 100 (hoặc 'auto')!");
            }
        }
    }

    // 2. CHẾ ĐỘ ĐÈN THỞ TỰ ĐỘNG (Nâng dần từ 0% -> 100% rồi hạ dần về 0%)
    if (auto_breath_mode) {
        static uint32_t last_breath_time = 0;
        if (millis() - last_breath_time >= 30) { // Cập nhật mỗi 30ms
            last_breath_time = millis();

            breath_brightness += breath_step;
            if (breath_brightness >= 100) {
                breath_brightness = 100;
                breath_step = -2; // Bắt đầu giảm
            } else if (breath_brightness <= 0) {
                breath_brightness = 0;
                breath_step = 2;  // Bắt đầu tăng
            }

            Send_LED_Brightness((uint8_t)breath_brightness);
        }
    }

    // 3. ĐỌC PHẢN HỒI ACK TỪ STM8 (Để xả đệm)
    while (NEURON_SERIAL.available()) {
        NEURON_SERIAL.read();
    }
}

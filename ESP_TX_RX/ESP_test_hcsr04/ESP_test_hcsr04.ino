#include <Arduino.h>

/* =========================================================
   TEST CẢM BIẾN SIÊU ÂM HC-SR04 TRỰC TIẾP VỚI ESP32 DEVKIT V1
   =========================================================
   Sơ đồ nối chân (ESP32 DevKit V1 -> HC-SR04):
     - VCC      -> Chân VIN (5V) của ESP32
     - GND      -> Chân GND của ESP32
     - TRIG     -> GPIO 5  (Output - Phát xung)
     - ECHO     -> GPIO 18 (Input - Thu xung phản xạ)
   ========================================================= */

#define TRIG_PIN 5
#define ECHO_PIN 18

// Tốc độ âm thanh trong không khí: ~0.0343 cm/microsecond (343 m/s)
#define SOUND_SPEED 0.0343 

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n==================================================");
    Serial.println("   ESP32 DEVKIT V1 - TEST CẢM BIẾN HC-SR04        ");
    Serial.println("==================================================");
    Serial.printf("[INIT] TRIG = GPIO%d, ECHO = GPIO%d\n", TRIG_PIN, ECHO_PIN);
    Serial.println("[INFO] Đang đo khoảng cách theo thời gian thực...");
    Serial.println("--------------------------------------------------");

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    // Kéo TRIG xuống LOW ban đầu
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
}

void loop() {
    // 1. Phát xung kích hoạt (Trigger Pulse) 10 microsecond
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // 2. Đo thời gian chân ECHO ở mức HIGH (Timeout 30.000us ~ 30ms ~ tối đa 5 mét)
    unsigned long duration_us = pulseIn(ECHO_PIN, HIGH, 30000);

    // 3. Tính toán khoảng cách (cm)
    if (duration_us == 0) {
        Serial.println("[CẢNH BÁO] Không có phản hồi (Vượt quá tầm đo hoặc lỏng dây ECHO)!");
    } else {
        float distance_cm = (duration_us * SOUND_SPEED) / 2.0;

        // Lọc khoảng cách hợp lệ của HC-SR04 (2cm đến 400cm)
        if (distance_cm >= 2.0 && distance_cm <= 400.0) {
            Serial.printf("Khoảng cách: %.1f cm (Thời gian xung: %lu us)\n", distance_cm, duration_us);
        } else {
            Serial.printf("Ngoài tầm đo: %.1f cm\n", distance_cm);
        }
    }

    // Đo lại sau mỗi 200ms
    delay(200);
}

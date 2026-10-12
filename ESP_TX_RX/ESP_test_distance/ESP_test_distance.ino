#include <Arduino.h>

/* =========================================================
   ESP32-S3 TEST CHUYEN BIET CHO SLAVE 5: CAM BIEN KHOANG CACH HC-SR04
   - UART Ring: 9600 bps (8N1)
   - RX Pin: GPIO 5 (Noi vao TX cua STM8 cuoi cung hoac Slave 5)
   - TX Pin: GPIO 4 (Noi vao RX cua Slave dau tien hoac Slave 5)
   - Giao thuc: [0xAA] [SLAVE_ID] [CMD] [CRC]
========================================================= */
#define NEURON_SERIAL Serial1
#define RX_PIN 5
#define TX_PIN 4

#define FRAME_HEADER     0xAA
#define ID_DISTANCE      0x05   // Ho tro ca 0x05 va 0x04
#define CMD_READ_DIST    0x70   // Lenh yeu cau do khoang cach

uint8_t Calc_Checksum(uint8_t id, uint8_t data) {
    return (uint8_t)((id + data) & 0xFF);
}

void clear_rx_buffer() {
    while (NEURON_SERIAL.available()) {
        NEURON_SERIAL.read();
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000); // Cho USB CDC tren ESP32-S3

    NEURON_SERIAL.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

    Serial.println("\n==================================================");
    Serial.println("  ESP32-S3 TEST SLAVE 5 - HC-SR04 DISTANCE SENSOR ");
    Serial.println("  UART: 9600 bps | RX=GPIO5 | TX=GPIO4            ");
    Serial.println("==================================================\n");
}

void loop() {
    static uint32_t last_poll = 0;

    // Chu ky hoi cam bien: 200ms/lan (HC-SR04 khuyen nghi >= 60ms)
    if (millis() - last_poll >= 200) {
        last_poll = millis();

        clear_rx_buffer();

        // 1. GUI LENH HOI KHOANG CACH LEN RING
        uint8_t tx_frame[4];
        tx_frame[0] = FRAME_HEADER;
        tx_frame[1] = ID_DISTANCE;
        tx_frame[2] = CMD_READ_DIST;
        tx_frame[3] = Calc_Checksum(ID_DISTANCE, CMD_READ_DIST);

        NEURON_SERIAL.write(tx_frame, 4);
        NEURON_SERIAL.flush();

        // 2. CHO PHAN HOI TRONG VONG 50ms
        uint32_t wait_start = millis();
        uint8_t parser_state = 0;
        uint8_t resp_id = 0;
        uint8_t resp_dist = 0;
        bool valid_frame = false;

        while ((millis() - wait_start) < 50) {
            while (NEURON_SERIAL.available()) {
                uint8_t c = NEURON_SERIAL.read();
                switch (parser_state) {
                    case 0:
                        if (c == FRAME_HEADER) parser_state = 1;
                        break;
                    case 1:
                        resp_id = c;
                        parser_state = 2;
                        break;
                    case 2:
                        resp_dist = c;
                        parser_state = 3;
                        break;
                    case 3:
                        if (c == Calc_Checksum(resp_id, resp_dist)) {
                            if (resp_id == ID_DISTANCE || resp_id == 0x04) {
                                valid_frame = true;
                            }
                        }
                        parser_state = 0;
                        break;
                    default:
                        parser_state = 0;
                        break;
                }
                if (valid_frame) break;
            }
            if (valid_frame) break;
        }

        // 3. HIEN THI KET QUA
        if (valid_frame) {
            Serial.printf("[SLAVE 0x%02X] Khoang cach: %3d cm  | ", resp_id, resp_dist);

            // Ve thanh do truc quan (moi '=' tuong duong 5cm)
            int bars = resp_dist / 5;
            if (bars > 30) bars = 30;
            Serial.print("[");
            for (int i = 0; i < bars; i++) Serial.print("=");
            for (int i = bars; i < 30; i++) Serial.print(" ");
            Serial.println("]");
        } else {
            Serial.println("[TIMEOUT] Slave 5 khong phan hoi! (Kiem tra nguon HC-SR04, chan PA3 Trig, PD2 Echo, vong RX/TX)");
        }
    }
}

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* ==============================================================================
   FABLAB STEM - BỘ SƯU TẬP HIỆU ỨNG ĐỒ HỌA ĐỈNH CAO CHO MÀN HÌNH OLED 0.96" (I2C)
   SỬ DỤNG VI ĐIỀU KHIỂN ESP32 DEVKIT V1
   ==============================================================================
   
   1. ĐẤU DÂY VẬT LÝ:
      - GND ---> GND của ESP32
      - VCC ---> 3.3V hoặc 5V (VIN) của ESP32
      - SCL ---> GPIO 22
      - SDA ---> GPIO 21

   2. 5 HIỆU ỨNG ĐẶC SẮC ĐƯỢC TÍCH HỢP:
      [1] Khối lập phương 3D xoay không gian (3D Rotating Wireframe Cube)
      [2] Du hành vũ trụ siêu không gian (3D Starfield Warp Speed / Hyperspace)
      [3] Đôi mắt Robot Cyberpunk biểu cảm (Animated Expressive Robot Eyes)
      [4] Biểu đồ sóng âm & Equalizer nhảy theo điệu nhạc (Music Audio Visualizer)
      [5] Pháo hoa hạt ánh sáng trọng lực (Physics Particle Fireworks)

   3. ĐIỀU KHIỂN TƯƠNG TÁC (SERIAL MONITOR 115200 BAUD):
      - Gõ '1', '2', '3', '4', '5': Chọn trực tiếp hiệu ứng muốn xem.
      - Gõ 'auto': Bật chế độ tự động chuyển đổi hiệu ứng mỗi 7 giây.
      - Gõ 'speed+': Tăng tốc độ chuyển động.
      - Gõ 'speed-': Giảm tốc độ chuyển động.
   ============================================================================== */

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

#define I2C_SDA_PIN   21
#define I2C_SCL_PIN   22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
uint8_t oled_addr = 0x3C;

// Trạng thái điều khiển hiệu ứng
uint8_t current_effect = 1;
bool auto_cycle = true;
uint32_t last_cycle_time = 0;
const uint32_t CYCLE_INTERVAL = 7000; // 7 giây tự đổi hiệu ứng
float anim_speed = 1.0f;

// ==============================================================================
// [HIỆU ỨNG 1] KHỐI LẬP PHƯƠNG 3D XOAY KHÔNG GIAN (3D ROTATING CUBE)
// ==============================================================================
float cube_angle_x = 0;
float cube_angle_y = 0;
float cube_angle_z = 0;

// 8 đỉnh của khối lập phương trong không gian 3 chiều [-1, 1]
const float cube_vertices[8][3] = {
    {-20, -20, -20}, { 20, -20, -20}, { 20,  20, -20}, {-20,  20, -20},
    {-20, -20,  20}, { 20, -20,  20}, { 20,  20,  20}, {-20,  20,  20}
};

// 12 cạnh nối các đỉnh
const uint8_t cube_edges[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, // Mặt sau
    {4, 5}, {5, 6}, {6, 7}, {7, 4}, // Mặt trước
    {0, 4}, {1, 5}, {2, 6}, {3, 7}  // Nối các cạnh bên
};

void drawEffect3DCube() {
    display.clearDisplay();

    // Tiêu đề nhỏ góc trên
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("1. 3D CUBE MATRIX");

    // Ma trận xoay và phép chiếu phối cảnh 3D -> 2D
    float rad_x = cube_angle_x * 0.01745329f;
    float rad_y = cube_angle_y * 0.01745329f;
    float rad_z = cube_angle_z * 0.01745329f;

    float sin_x = sin(rad_x), cos_x = cos(rad_x);
    float sin_y = sin(rad_y), cos_y = cos(rad_y);
    float sin_z = sin(rad_z), cos_z = cos(rad_z);

    int16_t proj_x[8];
    int16_t proj_y[8];

    for (int i = 0; i < 8; i++) {
        float x = cube_vertices[i][0];
        float y = cube_vertices[i][1];
        float z = cube_vertices[i][2];

        // Xoay quanh trục X
        float y1 = y * cos_x - z * sin_x;
        float z1 = y * sin_x + z * cos_x;

        // Xoay quanh trục Y
        float x2 = x * cos_y + z1 * sin_y;
        float z2 = -x * sin_y + z1 * cos_y;

        // Xoay quanh trục Z
        float x3 = x2 * cos_z - y1 * sin_z;
        float y3 = x2 * sin_z + y1 * cos_z;

        // Chiếu phối cảnh (Perspective projection)
        float distance = 65.0f;
        float fov = distance / (distance + z2 + 10.0f);

        proj_x[i] = (int16_t)(64 + x3 * fov);
        proj_y[i] = (int16_t)(36 + y3 * fov);

        // Vẽ các đỉnh (Vertex node)
        display.fillCircle(proj_x[i], proj_y[i], 2, SSD1306_WHITE);
    }

    // Vẽ 12 đường nối khung dây
    for (int i = 0; i < 12; i++) {
        uint8_t p1 = cube_edges[i][0];
        uint8_t p2 = cube_edges[i][1];
        display.drawLine(proj_x[p1], proj_y[p1], proj_x[p2], proj_y[p2], SSD1306_WHITE);
    }

    display.display();

    cube_angle_x += 2.2f * anim_speed;
    cube_angle_y += 3.1f * anim_speed;
    cube_angle_z += 1.5f * anim_speed;
}

// ==============================================================================
// [HIỆU ỨNG 2] DU HÀNH SIÊU KHÔNG GIAN (3D STARFIELD WARP SPEED)
// ==============================================================================
#define NUM_STARS 60
struct Star {
    float x, y, z;
    float prev_proj_x, prev_proj_y;
};
Star stars[NUM_STARS];

void initStars() {
    for (int i = 0; i < NUM_STARS; i++) {
        stars[i].x = (float)(random(-128, 128));
        stars[i].y = (float)(random(-64, 64));
        stars[i].z = (float)(random(20, 150));
        stars[i].prev_proj_x = -1;
        stars[i].prev_proj_y = -1;
    }
}

void drawEffectStarfield() {
    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("2. WARP SPEED 3D");

    for (int i = 0; i < NUM_STARS; i++) {
        // Sao lao về phía trước màn hình
        stars[i].z -= 4.5f * anim_speed;

        if (stars[i].z <= 1.0f) {
            stars[i].x = (float)(random(-128, 128));
            stars[i].y = (float)(random(-64, 64));
            stars[i].z = 150.0f;
            stars[i].prev_proj_x = -1;
            stars[i].prev_proj_y = -1;
            continue;
        }

        float k = 50.0f / stars[i].z;
        int16_t px = (int16_t)(64 + stars[i].x * k);
        int16_t py = (int16_t)(36 + stars[i].y * k);

        if (px >= 0 && px < 128 && py >= 12 && py < 64) {
            // Vẽ vệt sáng kéo dài (motion streak)
            if (stars[i].prev_proj_x >= 0 && stars[i].prev_proj_y >= 12) {
                display.drawLine((int16_t)stars[i].prev_proj_x, (int16_t)stars[i].prev_proj_y, px, py, SSD1306_WHITE);
            }
            // Điểm sao sáng ở đầu vệt
            display.drawPixel(px, py, SSD1306_WHITE);
            if (stars[i].z < 50.0f) {
                display.drawPixel(px + 1, py, SSD1306_WHITE);
            }
            stars[i].prev_proj_x = px;
            stars[i].prev_proj_y = py;
        } else {
            // Ra khỏi màn hình -> tái tạo lại sao mới
            stars[i].x = (float)(random(-128, 128));
            stars[i].y = (float)(random(-64, 64));
            stars[i].z = 150.0f;
            stars[i].prev_proj_x = -1;
            stars[i].prev_proj_y = -1;
        }
    }

    display.display();
}

// ==============================================================================
// [HIỆU ỨNG 3] ĐÔI MẮT ROBOT BIỂU CẢM CYBERPUNK (ANIMATED ROBOT EYES)
// ==============================================================================
int16_t eye_pos_x = 0;     // Hướng nhìn ngang (-14 đến +14)
int16_t eye_pos_y = 0;     // Hướng nhìn dọc (-8 đến +8)
int16_t target_eye_x = 0;
int16_t target_eye_y = 0;
uint8_t eye_state = 0;     // 0: Bình thường, 1: Nháy mắt (Blink), 2: Vui mừng (Happy)
uint32_t last_eye_action = 0;
int16_t blink_height = 28;

void updateRobotEyesLogic() {
    uint32_t now = millis();

    // Định kỳ đổi biểu cảm hoặc liếc nhìn hướng khác
    if (now - last_eye_action > 2200) {
        last_eye_action = now;
        uint8_t action = random(0, 5);

        if (action == 0) {
            target_eye_x = random(-14, 15);
            target_eye_y = 0;
            eye_state = 0;
        } else if (action == 1) {
            target_eye_x = 0;
            target_eye_y = random(-6, 7);
            eye_state = 0;
        } else if (action == 2) {
            eye_state = 1; // Chớp mắt
        } else if (action == 3) {
            eye_state = 2; // Cười vui mắt cong
            target_eye_x = 0;
            target_eye_y = -4;
        } else {
            target_eye_x = 0;
            target_eye_y = 0;
            eye_state = 0;
        }
    }

    // Làm mượt cử động chuyển động của mắt (Smooth Easing)
    eye_pos_x += (target_eye_x - eye_pos_x) * 0.25f;
    eye_pos_y += (target_eye_y - eye_pos_y) * 0.25f;

    // Hiệu ứng chớp mắt
    if (eye_state == 1) {
        blink_height -= 8;
        if (blink_height <= 2) {
            blink_height = 2;
            eye_state = 0; // Mở mắt trở lại
        }
    } else {
        if (blink_height < 28) blink_height += 6;
        if (blink_height > 28) blink_height = 28;
    }
}

void drawEffectRobotEyes() {
    display.clearDisplay();

    updateRobotEyesLogic();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("3. CYBER ROBOT EYES");

    const int16_t eye_w = 36;
    int16_t eye_h = blink_height;
    int16_t rad = 8;
    if (eye_h < rad * 2) rad = eye_h / 2;

    int16_t center_y = 36 + eye_pos_y;
    int16_t left_eye_x = 24 + eye_pos_x;
    int16_t right_eye_x = 68 + eye_pos_x;
    int16_t top_y = center_y - (eye_h / 2);

    if (eye_state == 2) {
        // Biểu cảm vui vẻ (Cung mắt cười ngược)
        display.fillRoundRect(left_eye_x, top_y, eye_w, eye_h, rad, SSD1306_WHITE);
        display.fillRoundRect(right_eye_x, top_y, eye_w, eye_h, rad, SSD1306_WHITE);
        // Cắt bớt phần dưới tạo mắt cong cười
        display.fillCircle(left_eye_x + eye_w / 2, top_y + eye_h + 4, 18, SSD1306_BLACK);
        display.fillCircle(right_eye_x + eye_w / 2, top_y + eye_h + 4, 18, SSD1306_BLACK);
    } else {
        // Mắt bình thường hoặc chớp mắt
        display.fillRoundRect(left_eye_x, top_y, eye_w, eye_h, rad, SSD1306_WHITE);
        display.fillRoundRect(right_eye_x, top_y, eye_w, eye_h, rad, SSD1306_WHITE);

        // Con ngươi phát sáng bên trong khi mở to mắt
        if (eye_h >= 20) {
            display.fillCircle(left_eye_x + 10, top_y + 8, 3, SSD1306_BLACK);
            display.fillCircle(right_eye_x + 10, top_y + 8, 3, SSD1306_BLACK);
        }
    }

    display.display();
}

// ==============================================================================
// [HIỆU ỨNG 4] BIỂU ĐỒ SÓNG ÂM & EQUALIZER NHẢY ĐIỆU NHẠC (AUDIO VISUALIZER)
// ==============================================================================
#define NUM_BARS 16
float bar_heights[NUM_BARS] = {0};
float bar_peaks[NUM_BARS] = {0};
float wave_phase = 0;

void drawEffectAudioVisualizer() {
    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("4. AUDIO SPECTRUM");

    // 1. Vẽ đường sóng sin mượt mà ở nửa trên
    for (int x = 0; x < 127; x += 2) {
        float y1 = 20 + sin((x * 0.08f) + wave_phase) * 6.0f + cos((x * 0.04f) - wave_phase * 1.5f) * 3.0f;
        float y2 = 20 + sin(((x + 2) * 0.08f) + wave_phase) * 6.0f + cos(((x + 2) * 0.04f) - wave_phase * 1.5f) * 3.0f;
        display.drawLine(x, (int16_t)y1, x + 2, (int16_t)y2, SSD1306_WHITE);
    }
    wave_phase += 0.15f * anim_speed;

    // 2. Vẽ 16 cột Equalizer nhấp nhô sống động
    const uint8_t bar_w = 6;
    const uint8_t bar_gap = 2;
    const uint8_t base_y = 63;
    const uint8_t max_bar_h = 32;

    for (int i = 0; i < NUM_BARS; i++) {
        // Sinh độ cao nhịp ngẫu nhiên pha trộn hàm sin
        float target_h = (sin(wave_phase * 2.0f + i * 0.6f) + 1.0f) * 0.5f * (max_bar_h - 4) + random(2, 6);
        bar_heights[i] += (target_h - bar_heights[i]) * 0.4f * anim_speed;

        if (bar_heights[i] > bar_peaks[i]) {
            bar_peaks[i] = bar_heights[i]; // Đẩy đỉnh cao nhất lên
        } else {
            bar_peaks[i] -= 0.6f;          // Đỉnh rơi tự do theo trọng lực
            if (bar_peaks[i] < 0) bar_peaks[i] = 0;
        }

        int16_t bx = i * (bar_w + bar_gap);
        int16_t bh = (int16_t)bar_heights[i];
        int16_t bpeak = (int16_t)bar_peaks[i];

        // Vẽ cột nhạc
        display.fillRect(bx, base_y - bh, bar_w, bh, SSD1306_WHITE);
        // Vẽ điểm đỉnh (Peak Hold marker)
        if (bpeak > 1 && (base_y - bpeak - 2) >= 30) {
            display.drawFastHLine(bx, base_y - bpeak - 2, bar_w, SSD1306_WHITE);
        }
    }

    display.display();
}

// ==============================================================================
// [HIỆU ỨNG 5] PHÁO HOA HẠT ÁNH SÁNG TRỌNG LỰC (PHYSICS PARTICLE FIREWORKS)
// ==============================================================================
#define NUM_PARTICLES 45
struct Particle {
    float x, y;
    float vx, vy;
    uint8_t life;
};
Particle particles[NUM_PARTICLES];
bool rocket_active = true;
float rocket_x = 64;
float rocket_y = 63;
float rocket_vy = -4.5f;

void triggerExplosion(float x, float y) {
    for (int i = 0; i < NUM_PARTICLES; i++) {
        particles[i].x = x;
        particles[i].y = y;
        float angle = (float)random(0, 360) * 0.01745329f;
        float speed = (float)random(10, 35) / 10.0f;
        particles[i].vx = cos(angle) * speed;
        particles[i].vy = sin(angle) * speed;
        particles[i].life = (uint8_t)random(15, 30);
    }
}

void drawEffectFireworks() {
    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("5. FIREWORKS PARTICLE");

    if (rocket_active) {
        // Quả pháo hoa bắn từ đáy lên
        display.fillCircle((int16_t)rocket_x, (int16_t)rocket_y, 2, SSD1306_WHITE);
        display.drawFastVLine((int16_t)rocket_x, (int16_t)rocket_y + 2, 4, SSD1306_WHITE);

        rocket_y += rocket_vy * anim_speed;
        rocket_vy += 0.08f; // Trọng lực hãm pháo bay chậm lại

        // Nổ khi đạt độ cao thích hợp hoặc vận tốc về 0
        if (rocket_vy >= -0.8f || rocket_y <= 24) {
            rocket_active = false;
            triggerExplosion(rocket_x, rocket_y);
        }
    } else {
        // Các hạt tia sáng bung toả và rơi tự do
        bool has_alive = false;
        for (int i = 0; i < NUM_PARTICLES; i++) {
            if (particles[i].life > 0) {
                has_alive = true;
                particles[i].x += particles[i].vx * anim_speed;
                particles[i].y += particles[i].vy * anim_speed;
                particles[i].vy += 0.12f * anim_speed; // Gia tốc trọng lực kéo xuống
                particles[i].life--;

                int16_t px = (int16_t)particles[i].x;
                int16_t py = (int16_t)particles[i].y;

                if (px >= 0 && px < 128 && py >= 12 && py < 64) {
                    display.drawPixel(px, py, SSD1306_WHITE);
                    // Hạt còn sáng mạnh vẽ thêm đuôi
                    if (particles[i].life > 12) {
                        display.drawPixel(px + 1, py, SSD1306_WHITE);
                    }
                }
            }
        }

        // Tái tạo lại quả pháo mới
        if (!has_alive) {
            rocket_active = true;
            rocket_x = random(25, 103);
            rocket_y = 63;
            rocket_vy = (float)random(-48, -38) / 10.0f;
        }
    }

    display.display();
}

// ==============================================================================
// KHỞI TẠO VÀ ĐIỀU PHỐI VÒNG LẶP CHÍNH
// ==============================================================================
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);

    Serial.println("\n\n============================================================");
    Serial.println("  FABLAB STEM - SIÊU BỘ SƯU TẬP HIỆU ỨNG OLED 0.96\" I2C      ");
    Serial.println("============================================================");

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000); // Tốc độ cao 400kHz để hoạt họa siêu mượt

    // Dò tìm địa chỉ I2C
    Wire.beginTransmission(0x3C);
    if (Wire.endTransmission() == 0) oled_addr = 0x3C;
    else oled_addr = 0x3D;

    if (!display.begin(SSD1306_SWITCHCAPVCC, oled_addr)) {
        Serial.printf("[LỖI] Không khởi tạo được OLED tại 0x%02X!\n", oled_addr);
        while (1) delay(100);
    }

    initStars();

    Serial.println("\n--- BẢNG LỆNH CHỌN HIỆU ỨNG TỪ SERIAL MONITOR (115200) ---");
    Serial.println(" - Gõ '1'     -> Xem Khối lập phương 3D xoay không gian");
    Serial.println(" - Gõ '2'     -> Xem Du hành siêu không gian (Warp Speed)");
    Serial.println(" - Gõ '3'     -> Xem Đôi mắt Robot Cyberpunk biểu cảm");
    Serial.println(" - Gõ '4'     -> Xem Sóng âm thanh & Equalizer vũ điệu nhạc");
    Serial.println(" - Gõ '5'     -> Xem Pháo hoa hạt ánh sáng trọng lực rơi");
    Serial.println(" - Gõ 'auto'  -> Bật chế độ tự động chuyển hiệu ứng mỗi 7 giây");
    Serial.println(" - Gõ 'speed+' hoặc 'speed-' -> Tăng / giảm tốc độ chuyển động");
    Serial.println("------------------------------------------------------------\n");

    // Màn hình mở đầu ấn tượng
    display.clearDisplay();
    display.drawRoundRect(2, 2, 124, 60, 6, SSD1306_WHITE);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(18, 16);
    display.print("FABLAB STEM IoT");
    display.setCursor(10, 32);
    display.print("OLED VISUAL EFFECTS");
    display.setCursor(34, 46);
    display.print("READY >>>>");
    display.display();
    delay(2000);

    last_cycle_time = millis();
}

void loop() {
    // 1. Nhận lệnh tương tác qua Serial Monitor
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd == "1") {
            current_effect = 1; auto_cycle = false;
            Serial.println(">>> Đã chọn: [1] Khối lập phương 3D xoay!");
        } else if (cmd == "2") {
            current_effect = 2; auto_cycle = false; initStars();
            Serial.println(">>> Đã chọn: [2] Du hành siêu không gian 3D!");
        } else if (cmd == "3") {
            current_effect = 3; auto_cycle = false;
            Serial.println(">>> Đã chọn: [3] Đôi mắt Robot Cyberpunk!");
        } else if (cmd == "4") {
            current_effect = 4; auto_cycle = false;
            Serial.println(">>> Đã chọn: [4] Sóng âm thanh Equalizer!");
        } else if (cmd == "5") {
            current_effect = 5; auto_cycle = false;
            rocket_active = true; rocket_y = 63; rocket_vy = -4.5f;
            Serial.println(">>> Đã chọn: [5] Pháo hoa hạt ánh sáng!");
        } else if (cmd.equalsIgnoreCase("auto")) {
            auto_cycle = true; last_cycle_time = millis();
            Serial.println(">>> Đã BẬT chế độ tự động chuyển hiệu ứng mỗi 7 giây!");
        } else if (cmd.equalsIgnoreCase("speed+")) {
            anim_speed += 0.3f; if (anim_speed > 3.0f) anim_speed = 3.0f;
            Serial.printf(">>> Tốc độ hoạt họa: %.1fx\n", anim_speed);
        } else if (cmd.equalsIgnoreCase("speed-")) {
            anim_speed -= 0.3f; if (anim_speed < 0.4f) anim_speed = 0.4f;
            Serial.printf(">>> Tốc độ hoạt họa: %.1fx\n", anim_speed);
        }
    }

    // 2. Tự động đổi hiệu ứng mỗi 7 giây (nếu bật auto_cycle)
    if (auto_cycle && (millis() - last_cycle_time >= CYCLE_INTERVAL)) {
        last_cycle_time = millis();
        current_effect++;
        if (current_effect > 5) current_effect = 1;

        if (current_effect == 2) initStars();
        if (current_effect == 5) { rocket_active = true; rocket_y = 63; rocket_vy = -4.5f; }

        Serial.printf("[AUTO CYCLE] Chuyển sang hiệu ứng số: %d\n", current_effect);
    }

    // 3. Render hiệu ứng tương ứng (Khung hình 30-45 FPS siêu mượt)
    switch (current_effect) {
        case 1: drawEffect3DCube(); break;
        case 2: drawEffectStarfield(); break;
        case 3: drawEffectRobotEyes(); break;
        case 4: drawEffectAudioVisualizer(); break;
        case 5: drawEffectFireworks(); break;
        default: current_effect = 1; break;
    }

    // Nhịp nghỉ nhỏ để nhường thời gian cho background task WiFi/Serial của ESP32
    delay(15);
}

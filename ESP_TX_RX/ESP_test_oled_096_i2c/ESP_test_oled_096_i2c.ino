#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* ==============================================================================
   FABLAB STEM - CHƯƠNG TRÌNH TEST TOÀN DIỆN MÀN HÌNH OLED 0.96 INCH (SSD1306 I2C)
   SỬ DỤNG VI ĐIỀU KHIỂN ESP32 DEVKIT V1
   ==============================================================================
   
   1. SƠ ĐỒ ĐẤU DÂY VẬT LÝ (ESP32 DevKit V1 <---> OLED 0.96" SSD1306):
      + Chân GND của OLED   ---> Nối vào chân GND của ESP32
      + Chân VCC của OLED   ---> Nối vào chân 3.3V (hoặc VIN/5V) của ESP32
                                 (OLED 0.96" có IC hạ áp nội, chạy cực tốt ở 3.3V)
      + Chân SCL của OLED   ---> Nối vào chân GPIO 22 (SCL mặc định của ESP32)
      + Chân SDA của OLED   ---> Nối vào chân GPIO 21 (SDA mặc định của ESP32)

      ⚠️ CẢNH BÁO QUAN TRỌNG VỀ THỨ TỰ CHÂN:
      Trên thị trường có 2 loại module OLED 0.96":
        - Loại A: GND - VCC - SCL - SDA
        - Loại B: VCC - GND - SCL - SDA
      HÃY NHÌN KỸ CHỮ IN TRÊN MẶT TRƯỚC BO MẠCH OLED TRƯỚC KHI CẮM NGUỒN!

   2. THƯ VIỆN CẦN CÀI ĐẶT TRÊN ARDUINO IDE:
      - "Adafruit SSD1306" (bởi Adafruit)
      - "Adafruit GFX Library" (bởi Adafruit)

   3. CÁC TÍNH NĂNG NỔI BẬT CỦA CODE:
      - Tự động quét I2C Scanner tìm địa chỉ (0x3C hoặc 0x3D).
      - Hiển thị giao diện đồ họa Dashboard STEM chuyên nghiệp: Khung viền, 
        tiêu đề, thanh đo tiến trình (Progress Bar), đồng hồ đếm Uptime, trạng thái I2C.
      - Hỗ trợ nhập chuỗi từ Serial Monitor (115200 baud) để in tức thì lên OLED.
      - Hỗ trợ các lệnh: 'demo' (chạy trình diễn đồ họa), 'clear', 'invert', 'scan'.
   ============================================================================== */

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1     // Reset chung với chân RST của ESP32

#define I2C_SDA_PIN   21
#define I2C_SCL_PIN   22

// Khởi tạo đối tượng màn hình SSD1306
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

uint8_t oled_i2c_addr = 0x3C; // Địa chỉ mặc định phổ biến nhất
bool oled_connected = false;
bool demo_mode = true;
bool is_inverted = false;
uint32_t counter = 0;
String custom_message = "";

// Hàm quét I2C tự động tìm địa chỉ OLED
uint8_t scanI2C() {
    Serial.println("\n[I2C SCANNER] Đang quét các cổng I2C (SDA=GPIO21, SCL=GPIO22)...");
    uint8_t found_addr = 0;
    uint8_t count = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  -> [TÌM THẤY] Thiết bị I2C tại địa chỉ: 0x%02X", addr);
            if (addr == 0x3C) {
                Serial.println(" (OLED 0.96\" SSD1306 chuẩn)");
                found_addr = addr;
            } else if (addr == 0x3D) {
                Serial.println(" (OLED 0.96\" SSD1306 đổi jumper)");
                found_addr = addr;
            } else if (addr == 0x27 || addr == 0x3F) {
                Serial.println(" (Module LCD 1602 I2C)");
            } else {
                Serial.println(" (Thiết bị khác)");
                if (found_addr == 0) found_addr = addr;
            }
            count++;
        }
    }

    if (count == 0) {
        Serial.println("  -> [CẢNH BÁO] Không tìm thấy thiết bị I2C nào!");
        Serial.println("     Kiểm tra lại: Dây SDA (21), SCL (22), VCC (3.3V/5V) và GND.");
    } else {
        Serial.printf("[I2C SCANNER] Hoàn tất. Tìm thấy %d thiết bị.\n\n", count);
    }
    return found_addr;
}

// Khởi tạo màn hình OLED
bool initOLED(uint8_t addr) {
    if (addr == 0) addr = 0x3C;
    
    // Khởi tạo SSD1306 với nguồn nội bộ (SWITCHCAPVCC)
    if (!display.begin(SSD1306_SWITCHCAPVCC, addr)) {
        Serial.printf("[OLED] Không thể khởi tạo SSD1306 tại địa chỉ 0x%02X!\n", addr);
        return false;
    }

    Serial.printf("[OLED] Khởi tạo thành công OLED 0.96\" tại địa chỉ 0x%02X!\n", addr);
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    return true;
}

// Vẽ giao diện Dashboard STEM thời gian thực
void drawDashboard(uint32_t count, uint32_t uptime_sec) {
    display.clearDisplay();

    // 1. Viền tiêu đề trên cùng (Header Bar)
    display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(18, 2);
    display.print("FABLAB STEM IoT");

    // 2. Nội dung hiển thị chính
    display.setTextColor(SSD1306_WHITE);

    if (custom_message.length() > 0) {
        // Nếu người dùng vừa gửi tin nhắn từ Serial
        display.setCursor(0, 16);
        display.print("MESSAGE FROM PC:");
        display.setCursor(0, 28);
        display.setTextSize(2);
        display.print(custom_message.substring(0, 10)); // In tối đa 10 ký tự to
        display.setTextSize(1);
        display.setCursor(0, 48);
        display.printf("I2C: 0x%02X | Uptime:%lus", oled_i2c_addr, uptime_sec);
    } else {
        // Giao diện giám sát thông số mặc định
        display.setCursor(0, 16);
        display.printf("OLED 0.96\" SSD1306");

        display.setCursor(0, 26);
        display.printf("I2C ADDR: 0x%02X [OK]", oled_i2c_addr);

        display.setCursor(0, 36);
        display.printf("COUNT   : %lu", count);

        display.setCursor(0, 46);
        display.printf("UPTIME  : %lu s", uptime_sec);

        // 3. Thanh đo tiến trình động (Progress Bar ở đáy màn hình)
        uint8_t progress_width = (count % 100) * 124 / 100;
        display.drawRect(0, 56, 128, 7, SSD1306_WHITE);
        display.fillRect(2, 58, progress_width, 3, SSD1306_WHITE);
    }

    display.display();
}

// Trình diễn kiểm tra toàn bộ điểm ảnh & hình học đồ họa (Graphic Demo)
void runGraphicsTest() {
    Serial.println(">>> Đang chạy Demo kiểm tra đồ họa OLED...");

    // Test 1: Khung chữ to
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 10);
    display.print("ESP32 V1");
    display.setCursor(5, 35);
    display.print("OLED 0.96\"");
    display.display();
    delay(1500);

    // Test 2: Vẽ đường chéo và khung chữ nhật
    display.clearDisplay();
    for (int16_t i = 0; i < display.width(); i += 8) {
        display.drawLine(0, 0, i, display.height() - 1, SSD1306_WHITE);
    }
    display.display();
    delay(1000);

    // Test 3: Vẽ các vòng tròn đồng tâm
    display.clearDisplay();
    for (int16_t r = 2; r < display.height() / 2; r += 4) {
        display.drawCircle(display.width() / 2, display.height() / 2, r, SSD1306_WHITE);
    }
    display.display();
    delay(1000);

    // Test 4: Vẽ tam giác và hình chữ nhật bo góc
    display.clearDisplay();
    display.drawRoundRect(5, 5, 118, 54, 8, SSD1306_WHITE);
    display.fillTriangle(30, 45, 64, 15, 98, 45, SSD1306_WHITE);
    display.display();
    delay(1000);

    Serial.println(">>> Kết thúc Demo đồ họa, quay lại màn hình Dashboard!");
    display.clearDisplay();
}

void printHelpMenu() {
    Serial.println("==================================================================");
    Serial.println("          HƯỚNG DẪN ĐIỀU KHIỂN OLED 0.96\" QUA SERIAL MONITOR      ");
    Serial.println("==================================================================");
    Serial.println(" - Gõ bất kỳ chuỗi nào (vd: 'Hello ESP32') -> In to lên màn hình");
    Serial.println(" - Gõ: 'demo'   -> Chạy kiểm tra các hiệu ứng đồ họa (hình học, text)");
    Serial.println(" - Gõ: 'dash'   -> Quay lại giao diện giám sát hệ thống Dashboard");
    Serial.println(" - Gõ: 'invert' -> Đảo ngược màu đen / trắng toàn màn hình");
    Serial.println(" - Gõ: 'clear'  -> Xóa tin nhắn tạm, trở về đếm số tự động");
    Serial.println(" - Gõ: 'scan'   -> Quét lại địa chỉ thiết bị trên bus I2C");
    Serial.println("==================================================================\n");
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2500);

    Serial.println("\n\n========================================================");
    Serial.println("   ESP32 DEVKIT V1 - TEST TRỰC TIẾP MÀN HÌNH OLED 0.96\"   ");
    Serial.println("========================================================");

    // Bắt đầu bus I2C với tốc độ cao 400kHz cho màn hình OLED mượt mà
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(400000);

    // Tự động tìm địa chỉ I2C
    uint8_t found = scanI2C();
    if (found == 0x3C || found == 0x3D) {
        oled_i2c_addr = found;
    }

    oled_connected = initOLED(oled_i2c_addr);

    printHelpMenu();

    if (oled_connected) {
        // Chạy màn hình khởi động Logo ban đầu
        display.clearDisplay();
        display.drawRoundRect(2, 2, 124, 60, 6, SSD1306_WHITE);
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(18, 18);
        display.print("FABLAB STEM IoT");
        display.setCursor(22, 34);
        display.print("OLED 0.96\" READY");
        display.display();
        delay(2000);
    }
}

void loop() {
    // 1. NHẬN LỆNH TỪ SERIAL MONITOR (115200 BAUD)
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();

        if (input.length() > 0) {
            if (input.equalsIgnoreCase("demo")) {
                runGraphicsTest();
            } 
            else if (input.equalsIgnoreCase("invert")) {
                is_inverted = !is_inverted;
                display.invertDisplay(is_inverted);
                Serial.printf(">>> Đã đổi chế độ Invert: %s\n", is_inverted ? "BẬT" : "TẮT");
            } 
            else if (input.equalsIgnoreCase("clear") || input.equalsIgnoreCase("dash")) {
                custom_message = "";
                Serial.println(">>> Đã quay lại giao diện Dashboard chuẩn!");
            } 
            else if (input.equalsIgnoreCase("scan")) {
                uint8_t found = scanI2C();
                if (found != 0) {
                    oled_i2c_addr = found;
                    initOLED(oled_i2c_addr);
                }
            } 
            else {
                // Nhập text bất kỳ để hiển thị
                custom_message = input;
                Serial.printf(">>> Hiển thị văn bản: \"%s\"\n", custom_message.c_str());
            }
        }
    }

    // 2. CẬP NHẬT GIAO DIỆN MỖI 200MS (5 FPS MƯỢT MÀ, KHÔNG GÂY GIẬT CẢM BIẾN)
    if (oled_connected) {
        static uint32_t last_draw = 0;
        if (millis() - last_draw >= 200) {
            last_draw = millis();
            counter++;
            drawDashboard(counter, millis() / 1000);
        }
    } else {
        // Nếu lúc bật chưa cắm màn hình, thử quét dò lại mỗi 3 giây
        static uint32_t last_retry = 0;
        if (millis() - last_retry >= 3000) {
            last_retry = millis();
            uint8_t found = scanI2C();
            if (found == 0x3C || found == 0x3D) {
                oled_i2c_addr = found;
                oled_connected = initOLED(oled_i2c_addr);
            }
        }
    }
}

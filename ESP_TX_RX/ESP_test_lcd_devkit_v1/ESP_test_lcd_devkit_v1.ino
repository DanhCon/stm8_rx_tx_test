#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

/* ==============================================================================
   FABLAB STEM - CHƯƠNG TRÌNH KIỂM TRA TRỰC TIẾP MODULE MÀN HÌNH LCD 1602 I2C
   BẰNG ESP32 DEVKIT V1 (TRƯỚC KHI ĐẤU VÀO MẠNG VÒNG STM8 RING)
   ==============================================================================
   
   1. SƠ ĐỒ ĐẤU DÂY VẬT LÝ (ESP32 DevKit V1 <---> Module LCD 1602 kèm I2C PCF8574):
      + Chân GND của LCD   ---> Nối vào chân GND của ESP32 DevKit V1
      + Chân VCC của LCD   ---> Nối vào chân VIN (hoặc chân 5V) của ESP32
                               (LƯU Ý QUAN TRỌNG: LCD 1602 cần nguồn 5V để phân cực
                                tinh thể lỏng hiển thị chữ rõ nét. Nếu cắm vào 3.3V
                                màn hình sẽ rất mờ hoặc không lên chữ!).
      + Chân SDA của LCD   ---> Nối vào chân GPIO 21 (SDA mặc định của ESP32)
      + Chân SCL của LCD   ---> Nối vào chân GPIO 22 (SCL mặc định của ESP32)

   2. TÍNH NĂNG NỔI BẬT:
      + Tự động quét I2C Scanner để tìm đúng địa chỉ (0x27 chip PCF8574T hoặc 0x3F chip PCF8574AT).
      + Tự động khởi tạo LCD theo địa chỉ quét được (không cần sửa code thủ công).
      + Hiển thị thông số kiểm tra, thời gian hoạt động (Uptime) và bộ đếm thời gian thực.
      + Cho phép nhập chữ từ Serial Monitor (115200 baud) để in trực tiếp lên Dòng 1 hoặc Dòng 2.
      + Hỗ trợ các lệnh: 'clear' (xóa màn hình), 'off' (tắt đèn nền), 'on' (bật đèn nền), 'scan' (quét lại I2C).
   ============================================================================== */

#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// Con trỏ LCD khởi tạo động theo địa chỉ quét được
LiquidCrystal_I2C *lcd = nullptr;
uint8_t detected_i2c_addr = 0;
bool lcd_initialized = false;
bool demo_mode = true;
uint32_t counter = 0;

// Hàm tự động quét tìm địa chỉ I2C trên bus
uint8_t scanI2C() {
    Serial.println("\n[I2C SCANNER] Đang dò tìm thiết bị trên đường truyền I2C (SDA=21, SCL=22)...");
    uint8_t count = 0;
    uint8_t found_addr = 0;

    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();

        if (error == 0) {
            Serial.printf("  -> [THÀNH CÔNG] Đã tìm thấy thiết bị I2C tại địa chỉ: 0x%02X", address);
            if (address == 0x27) {
                Serial.println(" (Chip PCF8574T chuẩn)");
            } else if (address == 0x3F) {
                Serial.println(" (Chip PCF8574AT)");
            } else {
                Serial.println(" (Địa chỉ tùy biến)");
            }
            found_addr = address;
            count++;
        } else if (error == 4) {
            Serial.printf("  -> [LỖI] Xung đột không xác định tại địa chỉ 0x%02X\n", address);
        }
    }

    if (count == 0) {
        Serial.println("  -> [CẢNH BÁO] Không tìm thấy thiết bị I2C nào!");
        Serial.println("     Vui lòng kiểm tra lại:");
        Serial.println("     1. Dây SDA đã nối chắc vào GPIO 21 chưa?");
        Serial.println("     2. Dây SCL đã nối chắc vào GPIO 22 chưa?");
        Serial.println("     3. Chân VCC đã nối vào chân VIN (5V) và GND đã tiếp xúc tốt chưa?");
        Serial.println("     4. Đèn LED đỏ nguồn ở mặt sau module LCD có sáng không?");
    } else {
        Serial.printf("[I2C SCANNER] Quét xong. Tổng số thiết bị tìm thấy: %d\n\n", count);
    }
    return found_addr;
}

// Khởi tạo LCD dựa trên địa chỉ tìm được
bool setupLCD(uint8_t addr) {
    if (addr == 0) return false;

    if (lcd != nullptr) {
        delete lcd;
        lcd = nullptr;
    }

    lcd = new LiquidCrystal_I2C(addr, 16, 2);
    lcd->init();
    lcd->backlight();
    lcd->clear();

    lcd->setCursor(0, 0);
    lcd->print("FABLAB STEM IoT");
    lcd->setCursor(0, 1);
    char buf[17];
    snprintf(buf, sizeof(buf), "I2C ADDR: 0x%02X", addr);
    lcd->print(buf);

    Serial.printf("[LCD] Đã khởi tạo thành công LCD 1602 tại địa chỉ 0x%02X!\n", addr);
    return true;
}

void printHelpMenu() {
    Serial.println("==================================================================");
    Serial.println("          HƯỚNG DẪN TEST LCD 1602 QUA SERIAL MONITOR              ");
    Serial.println("==================================================================");
    Serial.println(" - Gõ: 1:Noi dung      -> In chuỗi vào Dòng 1 (tối đa 16 ký tự)");
    Serial.println(" - Gõ: 2:Noi dung      -> In chuỗi vào Dòng 2 (tối đa 16 ký tự)");
    Serial.println(" - Gõ: clear           -> Xóa toàn bộ nội dung màn hình");
    Serial.println(" - Gõ: off             -> Tắt đèn nền LCD");
    Serial.println(" - Gõ: on              -> Bật lại đèn nền LCD");
    Serial.println(" - Gõ: demo            -> Bật lại chế độ đếm số tự động");
    Serial.println(" - Gõ: scan            -> Quét lại bus I2C tìm thiết bị");
    Serial.println("------------------------------------------------------------------");
    Serial.println(" > LƯU Ý KHI MÀN HÌNH CHỈ SÁNG ĐÈN MÀ KHÔNG THẤY CHỮ:");
    Serial.println("   Dùng tua vít dẹt nhỏ xoay nhẹ biến trở màu xanh (chiết áp vi chỉnh");
    Serial.println("   độ tương phản Contrast) ở mặt sau module I2C cho đến khi các chữ");
    Serial.println("   hiển thị rõ nét, không bị chìm hoặc không bị hiện 16 ô vuông đen.");
    Serial.println("==================================================================\n");
}

void setup() {
    Serial.begin(115200);
    // Chờ cổng USB Serial ổn định (cho cả ESP32 thường và ESP32-S3 CDC)
    while (!Serial && millis() < 2500);

    Serial.println("\n\n========================================================");
    Serial.println("    ESP32 DEVKIT V1 - TEST TRỰC TIẾP MODULE LCD 1602    ");
    Serial.println("========================================================");

    // Khởi tạo I2C với chân GPIO 21 (SDA) và GPIO 22 (SCL)
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(100000); // Chuẩn tốc độ 100kHz I2C cho PCF8574

    // Quét tìm địa chỉ
    detected_i2c_addr = scanI2C();
    if (detected_i2c_addr != 0) {
        lcd_initialized = setupLCD(detected_i2c_addr);
    } else {
        Serial.println("[CẢNH BÁO] Chưa kết nối được LCD. Sẽ thử lại khi bạn cắm dây và gõ 'scan'!");
    }

    printHelpMenu();
    delay(2000);
}

void loop() {
    // 1. NHẬN VÀ XỬ LÝ LỆNH TỪ SERIAL MONITOR CỦA MÁY TÍNH
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();

        if (input.length() > 0) {
            if (input.equalsIgnoreCase("clear")) {
                demo_mode = false;
                if (lcd) lcd->clear();
                Serial.println(">>> Đã xóa màn hình LCD!");
            } 
            else if (input.equalsIgnoreCase("off")) {
                if (lcd) lcd->noBacklight();
                Serial.println(">>> Đã tắt đèn nền LCD!");
            } 
            else if (input.equalsIgnoreCase("on")) {
                if (lcd) lcd->backlight();
                Serial.println(">>> Đã bật đèn nền LCD!");
            } 
            else if (input.equalsIgnoreCase("scan")) {
                detected_i2c_addr = scanI2C();
                if (detected_i2c_addr != 0) {
                    lcd_initialized = setupLCD(detected_i2c_addr);
                }
            } 
            else if (input.equalsIgnoreCase("demo")) {
                demo_mode = true;
                if (lcd) lcd->clear();
                Serial.println(">>> Bật lại chế độ đếm số tự động!");
            } 
            else if (input.startsWith("1:")) {
                demo_mode = false;
                String text = input.substring(2);
                if (lcd) {
                    lcd->setCursor(0, 0);
                    // In đệm khoảng trắng đủ 16 ký tự để xóa chữ cũ
                    char padded[17];
                    memset(padded, ' ', 16);
                    padded[16] = '\0';
                    int len = text.length() > 16 ? 16 : text.length();
                    memcpy(padded, text.c_str(), len);
                    lcd->print(padded);
                }
                Serial.printf(">>> In Dòng 1: \"%s\"\n", text.c_str());
            } 
            else if (input.startsWith("2:")) {
                demo_mode = false;
                String text = input.substring(2);
                if (lcd) {
                    lcd->setCursor(0, 1);
                    char padded[17];
                    memset(padded, ' ', 16);
                    padded[16] = '\0';
                    int len = text.length() > 16 ? 16 : text.length();
                    memcpy(padded, text.c_str(), len);
                    lcd->print(padded);
                }
                Serial.printf(">>> In Dòng 2: \"%s\"\n", text.c_str());
            } 
            else {
                // Mặc định in vào Dòng 1
                demo_mode = false;
                if (lcd) {
                    lcd->setCursor(0, 0);
                    char padded[17];
                    memset(padded, ' ', 16);
                    padded[16] = '\0';
                    int len = input.length() > 16 ? 16 : input.length();
                    memcpy(padded, input.c_str(), len);
                    lcd->print(padded);
                }
                Serial.printf(">>> In Dòng 1: \"%s\"\n", input.c_str());
            }
        }
    }

    // 2. CHẾ ĐỘ DEMO TỰ ĐỘNG CẬP NHẬT MỖI GIÂY
    if (demo_mode && lcd_initialized && lcd != nullptr) {
        static uint32_t last_tick = 0;
        if (millis() - last_tick >= 1000) {
            last_tick = millis();
            counter++;

            char line1[17];
            char line2[17];
            snprintf(line1, sizeof(line1), "FABLAB STEM: %3lu", counter % 1000);
            snprintf(line2, sizeof(line2), "UPTIME: %5lus   ", millis() / 1000);

            lcd->setCursor(0, 0);
            lcd->print(line1);
            lcd->setCursor(0, 1);
            lcd->print(line2);
        }
    }

    // Nếu chưa khởi tạo được LCD (do lúc bật chưa cắm dây), thử dò lại định kỳ mỗi 3 giây
    if (!lcd_initialized) {
        static uint32_t last_retry = 0;
        if (millis() - last_retry >= 3000) {
            last_retry = millis();
            detected_i2c_addr = scanI2C();
            if (detected_i2c_addr != 0) {
                lcd_initialized = setupLCD(detected_i2c_addr);
            }
        }
    }
}

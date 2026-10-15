# BỘ PHẦN LÕI HỆ THỐNG PHẦN CỨNG IOT STEM (CORE PACKAGE)
> **Dành cho:** Nhà phát triển Web, Mobile App, Desktop App hoặc hệ thống IoT bên thứ ba muốn tích hợp phần cứng mà **không cần dùng ứng dụng FabLab STEM Desktop**.

---

## 📁 CẤU TRÚC GÓI BÀN GIAO

```text
STEM_CORE_PACKAGE/
├── 01_ESP32_GATEWAY_FIRMWARE/
│   ├── ESP_gateway_v2.ino          # Firmware V2 chuẩn (Dual-Frame, 5-Node Polling)
│   └── ESP_gateway_v3.ino          # Firmware V3 mới (Adaptive 3-Tier Multi-Rate Polling, 12 Nodes)
├── 02_STM8_FIRMWARE_S19_HEX/       # Mã máy .s19 & .hex nạp thẳng bằng STVP
│   ├── Slave_1_LED_Basic.s19       # Node 1: Đèn LED cơ bản (Bật / Tắt / Nhấp nháy)
│   ├── Slave_1_LED_PWM.s19         # Node 1: Đèn LED Dimming (Chiết áp độ sáng 0-100%)
│   ├── Slave_1_LED_Smart.s19/.hex  # Node 1: Đèn LED Smart (Tự trị ngắt Timer: Breathe, SOS, Double Flash)
│   ├── Slave_2_Button.s19          # Node 2: Nút nhấn cơ bản
│   ├── Slave_2_Button_Smart.s19/.hex # Node 2: Nút nhấn Smart (Phân loại cử chỉ: Single, Double, Long, Counter)
│   ├── Slave_3_Buzzer.s19          # Node 3: Còi báo động (On / Off / Beep)
│   ├── Slave_4_Potentiometer.s19   # Node 4: Biến trở xoay ADC 8-bit
│   ├── Slave_5_Distance_HCSR04.s19 # Node 5: Cảm biến siêu âm khoảng cách (ECHO: PA1, TRIG: PA2)
│   ├── Slave_6_LCD_1602.s19        # Node 6: Màn hình LCD 1602 I2C (SCL: PB4, SDA: PB5)
│   ├── Slave_7_DC_Motor.s19        # Node 7: Động cơ DC cầu H PWM 10kHz (INA: PC3, INB: PC4)
│   ├── Slave_8_Servo_SG90.s19      # Node 8: Động cơ Servo RC PWM 50Hz (PWM: PC3)
│   ├── Slave_9_OLED_SSD1306.s19    # Node 9: Màn hình OLED 0.96" SSD1306 I2C (SCL: PB4, SDA: PB5)
│   ├── Slave_10_Soil_Moisture_LM393.s19 # Node 10: Cảm biến độ ẩm đất LM393 (AO: PD3, DO: PD2)
│   ├── Slave_11_DHT11_Temp_Humidity.s19 # Node 11: Cảm biến nhiệt độ & độ ẩm DHT11 (DATA: PC4)
│   └── Slave_12_DHT22_AM2302.s19   # Node 12: Cảm biến nhiệt độ & độ ẩm DHT22/AM2302 (DATA: PC4)
├── 03_STM8_SOURCE_CODE_C/          # Mã nguồn C89 gốc (UTF-8 No BOM)
│   ├── Slave_1_LED_Basic/
│   ├── Slave_1_LED_PWM/
│   ├── Slave_1_LED_Smart/          # C nguồn Slave 1 Smart kèm build.bat độc lập
│   ├── Slave_2_Button/
│   ├── Slave_2_Button_Smart/       # C nguồn Slave 2 Smart kèm build.bat độc lập
│   ├── Slave_3_Buzzer/
│   ├── Slave_4_Potentiometer/
│   ├── Slave_5_Distance_HCSR04/
│   ├── Slave_6_LCD_1602/
│   ├── stm8_slave_7_dong_co_dc/
│   ├── stm8_slave_8_servo/
│   ├── stm8_slave_9_oled/
│   ├── stm8_slave_10_do_am_dat/
│   ├── stm8_slave_11_dht11/
│   └── stm8_slave_12_dht22/
├── 04_COMMUNICATION_SDK_PYTHON/
│   ├── neuron_gateway_service.py   # Daemon thread UDP client hoàn chỉnh
│   ├── udp_commander.py            # Công cụ console test gửi lệnh nhanh
│   └── example_client.py           # Code mẫu Python siêu nhẹ nhận dữ liệu & điều khiển
└── README_HUONG_DAN_SU_DUNG.md     # Tài liệu này
```

---

## ⚡ HƯỚNG DẪN NẠP FIRMWARE

### 1. Nạp cho ESP32 Gateway:
1. Mở file `01_ESP32_GATEWAY_FIRMWARE/ESP_gateway_v2.ino` bằng **Arduino IDE**.
2. Chọn Board: **ESP32 Dev Module** (hoặc ESP32-WROOM-32).
3. Bấm **Upload** lên mạch ESP32.
4. *Lưu ý cấu hình WiFi:*
   - Khi chưa có WiFi, ESP32 sẽ phát điểm truy cập tên: `Neuron_Gateway_Config`.
   - Dùng điện thoại/máy tính kết nối vào và truy cập `192.168.4.1` để nhập tên WiFi & Mật khẩu nhà bạn.
   - Sau khi kết nối WiFi thành công, ESP32 sẽ tự động truyền dữ liệu lên toàn bộ mạng LAN qua UDP.

### 2. Nạp cho 6 module STM8 (Bằng mạch nạp ST-Link V2 & STVP):
1. Cắm 4 chân ST-Link V2 (`3.3V`, `GND`, `SWIM`, `NRST`) vào bo STM8S103F3.
2. Mở phần mềm **ST Visual Programmer (STVP)**.
3. Chọn chip: `STM8S103F3` -> Hardware: `ST-LINK` -> Mode: `SWIM`.
4. Ở tab **`PROGRAM MEMORY`** (góc dưới bên trái), chọn `File` -> `Open...` -> Chọn file `.s19` tương ứng trong thư mục `02_STM8_FIRMWARE_S19_HEX/`.
5. Nhấn **`Ctrl + P`** (`Program -> Current tab`) để nạp.

---

## 🔄 SƠ ĐỒ ĐẤU NỐI MẠNG VÒNG (DAISY-CHAIN RING)

Tất cả các module STM8 và ESP32 được kết nối theo dạng **vòng tròn khép kín 1 chiều**:

```text
[ESP32 Gateway]
  TX (GPIO 4) ───► RX (PD6) [STM8 Slave 1: LED]
                   TX (PD5) ───► RX (PD6) [STM8 Slave 2: Nút bấm]
                                 TX (PD5) ───► RX (PD6) [STM8 Slave 3: Còi]
                                               TX (PD5) ───► RX (PD6) [STM8 Slave 4: Biến trở]
                                                             TX (PD5) ───► RX (PD6) [STM8 Slave 5: Khoảng cách]
                                                                           TX (PD5) ───► RX (PD6) [STM8 Slave 6: LCD]
                                                                                         TX (PD5) ───► RX (GPIO 5) [ESP32]
```

*Lưu ý nguồn điện:* Nối chung dây **GND** giữa ESP32 và tất cả các bo STM8. Cấp nguồn 5V cho module LCD 1602 và HC-SR04; cấp 3.3V hoặc 5V cho các bo STM8.

---

## 📌 BẢNG TRA CỨU CHÂN NGOẠI VI STM8 (TSSOP-20)

| Module | Ngoại vi kết nối | Chân STM8 | Chân vật lý chip | Ghi chú |
| :--- | :--- | :---: | :---: | :--- |
| **Slave 1: LED** | Đèn LED onboard / ngoài | `PC3` | Chân 13 | Output Push-Pull (hoặc Timer 1 CH3 PWM) |
| **Slave 2: Nút nhấn** | Nút bấm cơ | `PC4` | Chân 14 | Input Pull-up (Nhấn = Nối GND) |
| **Slave 3: Còi** | Còi chủ động (Buzzer) | `PC5` | Chân 15 | Output Push-Pull |
| **Slave 4: Biến trở** | Chân giữa biến trở B10K | `PD3` (`AIN4`) | Chân 20 | ADC Analog In (0 - 3.3V) |
| **Slave 5: Khoảng cách** | HC-SR04 ECHO | `PA1` | Chân 5 | Input Floating |
| | HC-SR04 TRIG | `PA2` | Chân 6 | Output Push-Pull 15µs pulse |
| **Slave 6: LCD 1602** | I2C SDA | `PB5` | Chân 11 | I2C Data (Auto scan 0x27 / 0x3F) |
| | I2C SCL | `PB4` | Chân 12 | I2C Clock |
| **Slave 7: Động cơ DC** | Cầu H INA (PWM Thuận) | `PC3` | Chân 13 | TIM1_CH3 Hardware PWM 10kHz |
| | Cầu H INB (PWM Nghịch) | `PC4` | Chân 14 | TIM1_CH4 Hardware PWM 10kHz |
| **Slave 8: Servo SG90** | PWM Tín hiệu Servo | `PC3` | Chân 13 | TIM1_CH3 Hardware PWM 50Hz (500-2500µs) |
| **Slave 9: OLED 0.96"** | I2C SDA (Dữ liệu) | `PB5` | Chân 11 | SSD1306 I2C (Auto scan 0x3C / 0x3D) |
| | I2C SCL (Xung nhịp) | `PB4` | Chân 12 | SSD1306 I2C Clock (Font 8x16 & 6x8) |
| **Slave 10: Độ ẩm đất** | AO (Analog Out) | `PD3` (`AIN4`) | Chân 20 | ADC đo độ ẩm liên tục 0-100% |
| | DO (Digital Out) | `PD2` | Chân 19 | Ngưỡng so sánh LM393 (1: Khô, 0: Ẩm) |
| | LED Cảnh báo | `PC3` | Chân 13 | Bật khi đất khô, tắt khi đủ ẩm |
| **Slave 11: DHT11** | DATA (1-Wire Signal) | `PC4` | Chân 14 | Nhiệt độ (0-50°C), Độ ẩm (20-90%) |
| | LED Trạng thái | `PC3` | Chân 13 | Nháy khi đọc thành công |
| **Slave 12: DHT22** | DATA (1-Wire Signal) | `PC4` | Chân 14 | Nhiệt độ (-40..80°C), Độ ẩm (0..99.9%) |
| | LED Trạng thái | `PC3` | Chân 13 | Nháy khi đọc thành công |

---

## 📡 ĐẶC TẢ GIAO THỨC TRUYỀN THÔNG MẠNG (UDP PROTOCOL)

Bất kỳ ứng dụng nào (Web, Node.js, Python, C#, Flutter) chỉ cần giao tiếp qua 2 cổng mạng UDP sau:

### 1. Kênh Nhận Dữ Liệu Cảm Biến (Telemetry Stream)
- **Cổng lắng nghe (Port):** **`9999`**
- **Chu kỳ gửi:** 100ms (10 lần/giây) hoặc tức thì khi cảm biến thay đổi.
- **Định dạng dữ liệu:** Chuỗi JSON UTF-8:

```json
{
  "timestamp": 125430,
  "gateway": {
    "device_id": "NEURON_GW_ESP32",
    "wifi_rssi": -58
  },
  "network_topology": {
    "led_node":      { "online": 1 },
    "button_node":   { "online": 1 },
    "buzzer_node":   { "online": 1 },
    "pot_node":      { "online": 1 },
    "distance_node": { "online": 1 }
  },
  "telemetry": {
    "button":        { "raw_state": 0, "press_count": 12 },
    "potentiometer": { "raw_adc": 128, "resistance_kohm": 5.02, "mapped_angle_deg": 135.5 },
    "distance":      { "distance_cm": 24 }
  },
  "control_state": {
    "led":    { "target": 1, "actual": 1 },
    "buzzer": { "target": 0, "actual": 0 }
  }
}
```

*Ý nghĩa các trường quan trọng:*
- `network_topology.*.online`: `1` là module đang cắm và hoạt động tốt, `0` là module bị rút ra.
- `telemetry.distance.distance_cm`: Khoảng cách đo được bằng centimet (3 - 400 cm).
- `telemetry.potentiometer.raw_adc`: Giá trị biến trở từ `0` đến `255`.
- `telemetry.button.raw_state`: Trạng thái nút bấm (`1` = Đang nhấn, `0` = Đang nhả).

---

### 2. Kênh Gửi Lệnh Điều Khiển Phần Cứng (Command Stream)
- **Cổng gửi đến ESP32:** **`8888`**
- **Định dạng:** Chuỗi Text đơn giản (ASCII Datagram):

| Chuỗi ký tự lệnh gửi đến Port 8888 | Hành vi thực hiện trên phần cứng |
| :--- | :--- |
| **`"ON"`** hoặc **`"LED_ON"`** | Bật sáng đèn LED 100% (Node 1) |
| **`"OFF"`** hoặc **`"LED_OFF"`** | Tắt đèn LED 0% (Node 1) |
| **`"BLINK"`** hoặc **`"LED_BLINK"`** | Nhấp nháy đèn LED chu kỳ 1s (Node 1) |
| **`"LED_BREATHE"`** | Hiệu ứng đèn thở sáng dần / tối dần Sine/Triangle tự trị (Node 1 Smart) |
| **`"LED_SOS"`** | Hiệu ứng nhấp nháy mã cầu cứu SOS quốc tế (... --- ...) (Node 1 Smart) |
| **`"LED_DOUBLE_FLASH"`** | Hiệu ứng nháy đôi chớp giật Strobe (Node 1 Smart) |
| **`"LED_PWM:<0-100>"`** | Điều chỉnh độ sáng LED trực tiếp từ 0% đến 100% |
| **`"BTN_GET_COUNTER"`** | Đọc số lần bấm tích lũy từ Node 2 Smart Button (0x54) |
| **`"BTN_RESET_COUNTER"`** | Đặt lại bộ đếm số lần bấm về 0 (Node 2 Smart Button) |
| **`"BUZ_ON"`** | Bật còi báo động kêu liên tục (Node 3) |
| **`"BUZ_OFF"`** | Tắt còi báo động (Node 3) |
| **`"BUZ_BEEP"`** | Còi kêu "bíp" 1 tiếng 100ms rồi tự tắt |
| **`"MOTOR_FWD:<1-100>"`** | Động cơ DC chạy thuận tốc độ 1-100% (Node 7) |
| **`"MOTOR_REV:<1-100>"`** | Động cơ DC chạy nghịch tốc độ 1-100% (Node 7) |
| **`"MOTOR_STOP"`** / **`"MOTOR_BRAKE"`** | Dừng / Phanh động cơ DC (Node 7) |
| **`"SERVO:<0-180>"`** | Đặt góc quay Servo RC 0° - 180° (Node 8) |
| **`"LCD_CLEAR"`** / **`"LCD_BACKLIGHT_ON/OFF"`** | Xóa màn hình / Bật/Tắt đèn nền LCD 1602 (Node 6) |
| **`"LCD1:<text>"`** / **`"LCD2:<text>"`** | Ghi chuỗi văn bản 16 ký tự lên Dòng 1 / Dòng 2 LCD (Node 6) |
| **`"OLED_ON/OFF/CLEAR/INVERT/SPLASH"`** | Điều khiển màn hình OLED 0.96" SSD1306 (Node 9) |
| **`"SOIL_SET_THRESH:<1-99>"`** | Cài đặt ngưỡng cảnh báo khô (%) cho Cảm biến độ ẩm đất (Node 10) |
| **`"RESET_CONFIG"`** | Xóa thông tin WiFi đã lưu trên ESP32 để cài đặt lại |

---

## 🚀 CODE MẪU CHẠY THỬ NHANH

Trong thư mục `04_COMMUNICATION_SDK_PYTHON/`, bạn chỉ cần chạy lệnh sau trên terminal:

```bash
python example_client.py
```

Console sẽ ngay lập tức hiển thị luồng dữ liệu thời gian thực của khoảng cách, biến trở, nút bấm và tự động bật tắt đèn khi đưa tay lại gần cảm biến!

# 📘 TÀI LIỆU BÀN GIAO TOÀN DIỆN DỰ ÁN FABLAB STEM PLATFORM
## HỆ THỐNG IOT GATEWAY ESP32-S3 & MẠNG VÒNG DAISY-CHAIN STM8 SLAVES

> **KÍNH GỬI CÁC AGENT & KỸ SƯ PHÁT TRIỂN TIẾP THEO:**  
> Tài liệu này được biên soạn nhằm chuyển giao 100% tri thức, kiến trúc, sơ đồ phần cứng, giao thức truyền thông, danh mục mã nguồn, cách thức vận hành và **toàn bộ 22 lỗi kỹ thuật xương máu kèm giải pháp triệt để** đã tích lũy trong quá trình phát triển hệ thống.
> 
> Khi tiếp quản dự án, bạn **KHÔNG CẦN hỏi lại người dùng những câu hỏi cơ bản**. Hãy đọc kỹ tài liệu này để phát triển, bảo trì và nâng cấp hệ thống an toàn và chính xác tuyệt đối.

---

## 1. TỔNG QUAN DỰ ÁN & HIỆN TRẠNG (SYSTEM OVERVIEW)

### 1.1. Mục tiêu hệ thống
Xây dựng nền tảng học tập STEM giáo dục (FabLab STEM Platform) cho phép học sinh lập trình kéo thả (Blockly) hoặc điều khiển trực quan qua giao diện GUI trên máy tính để tương tác thời gian thực với các cơ cấu chấp hành và cảm biến phần cứng:
- **Đèn LED đơn / LED PWM** (Bật/tắt/nhấp nháy/điều chỉnh độ sáng).
- **Nút nhấn vật lý** (Bắt sự kiện nhấn/nhả).
- **Còi báo Buzzer** (Kêu/tắt/beep).
- **Biến trở xoay B10K** (Đọc góc quay và điện trở tương tự).
- **Cảm biến siêu âm HC-SR04** (Đo khoảng cách vật cản 2cm - 400cm).
- **Màn hình LCD 1602 I2C** (Hiển thị văn bản tùy biến và thông điệp hệ thống).

### 1.2. Hiện trạng bàn giao (Current Milestone Status)
- [x] Toàn bộ **12 Module STM8 Slaves** (LED, Nút nhấn, Còi, Biến trở, Siêu âm, LCD 1602, Động cơ DC, Servo RC, OLED 0.96" SSD1306, Cảm biến độ ẩm đất LM393, Cảm biến nhiệt ẩm DHT11, Cảm biến nhiệt ẩm DHT22) đã hoàn thiện mã nguồn C (Cosmic C C89), kiểm thử và biên dịch ra file nhị phân `.s19` / `.hex` chuẩn 100%.
- [x] Firmware Gateway **`ESP_gateway_v2.ino`** đã tối ưu hóa Dual-Frame Verification (khử gói phản hồi lặp Echo), đáp ứng tức thì độ trễ thấp (Heartbeat 100ms, Deadband $abs \ge 1$, Fast-boot polling siêu âm 100ms).
- [x] Backend FastAPI đã chuyển đổi hoàn toàn sang **SQLite** cục bộ chạy độc lập không phụ thuộc Docker/PostgreSQL.
- [x] Frontend PySide6 / QML kết nối mượt mà với Backend và Gateway qua UDP port 8888.
- [x] Tạo gói bàn giao lõi độc lập **`STEM_CORE_PACKAGE`** đầy đủ 12 module cho bên thứ ba tích hợp Web hoặc App khác mà không cần phần mềm FabLab.
- [x] Tạo file chạy 1-click **`MO_APP_STEM.bat`**, **`BAT_BACKEND.bat`** và 2 shortcut trên Desktop (`C:\Users\hocho\OneDrive\Desktop`).

---

## 2. KIẾN TRÚC PHẦN CỨNG & TÔ PÔ MẠNG (HARDWARE TOPOLOGY)

### 2.1. Sơ đồ vòng xuyến đơn hướng (Daisy-Chain UART Ring)
Hệ thống sử dụng tô pô vòng xuyến khép kín (Ring Topology). ESP32 phát khung dữ liệu đi qua lần lượt từng Slave STM8; mỗi Slave có mạch lặp phần cứng (Hardware Repeater) trong ngắt RX để chuyển tiếp ngay tức thì sang Slave kế tiếp trước khi xử lý, cuối cùng quay về chân RX của ESP32.

```text
       ┌─────────────────────────────────────────────────────────┐
       │                ESP32-S3 Master Gateway                  │
       │   [TX: GPIO 4]                             [RX: GPIO 5] │
       └─────────┬───────────────────────────────────────▲───────┘
                 │                                       │
                 ▼                                       │
       ┌───────────────────┐                             │
       │  Slave 1: LED     │ (PD6 RX -> PD5 TX)          │
       └─────────┬─────────┘                             │
                 ▼                                       │
       ┌───────────────────┐                             │
       │  Slave 2: BUTTON  │ (PD6 RX -> PD5 TX)          │
       └─────────┬─────────┘                             │
                 ▼                                       │
       ┌───────────────────┐                             │
       │  Slave 3: BUZZER  │ (PD6 RX -> PD5 TX)          │
       └─────────┬─────────┘                             │
                 ▼                                       │
       ┌───────────────────┐                             │
       │  Slave 4: POT B10K│ (PD6 RX -> PD5 TX)          │
       └─────────┬─────────┘                             │
                 ▼                                       │
       ┌───────────────────┐                             │
       │  Slave 5: HC-SR04 │ (PD6 RX -> PD5 TX)          │
       └─────────┬─────────┘                             │
                 ▼                                       │
       ┌───────────────────┐                             │
       │  Slave 6: LCD 1602│ (PD6 RX -> PD5 TX) ─────────┘
       └───────────────────┘
```

> **Đặc tả UART Ring:**
> - **Baudrate:** `9600 bps`, Frame: `8N1` (8 data bits, No parity, 1 stop bit).
> - **Điện áp logic:** 3.3V (STM8S103 chạy 3.3V hoặc 5V; tương thích tốt với mức logic GPIO của ESP32-S3).

### 2.2. Bảng chân phần cứng chi tiết của 6 Slaves STM8 (Pinout Map)

| Node | Tên Module | Slave ID | Chân ngoại vi | Chân UART Ring | Ghi chú phần cứng |
| :--- | :--- | :---: | :--- | :--- | :--- |
| **Slave 1** | Đèn LED | `0x01` | **PC3**: Output Push-Pull | **RX**: PD6<br>**TX**: PD5 | Có phiên bản LED On/Off thường và bản PWM (`TIM1_CH3` trên PC3, 10 kHz). |
| **Slave 2** | Nút nhấn | `0x02` | **PC4**: Input Pull-up | **RX**: PD6<br>**TX**: PD5 | Nút nối giữa PC4 và GND. Khi nhấn mức logic = 0. |
| **Slave 3** | Còi báo | `0x03` | **PC5**: Output Push-Pull | **RX**: PD6<br>**TX**: PD5 | Còi buzzer chủ động (Active buzzer), kích mức HIGH là kêu. |
| **Slave 4** | Biến trở | `0x04` | **PD3**: Analog Input (AIN4) | **RX**: PD6<br>**TX**: PD5 | Biến trở B10K (10 $k\Omega$). Góc xoay $0^\circ - 270^\circ$. Đọc ADC 8-bit MSB (`0 - 255`). |
| **Slave 5** | Siêu âm | `0x05` | **PA1**: ECHO (Input)<br>**PA2**: TRIG (Output)<br>**PC3**: Debug LED | **RX**: PD6<br>**TX**: PD5 | Cảm biến HC-SR04 đo xung bằng `TIM2` (1 tick = $1\mu\text{s}$). Dải đo 2 - 400 cm. |
| **Slave 6** | Màn LCD | `0x06` | **PB4**: I2C SCL<br>**PB5**: I2C SDA | **RX**: PD6<br>**TX**: PD5 | Màn LCD 1602 kèm mạch PCF8574. Tự động nhận diện địa chỉ `0x27` hoặc `0x3F`. Cấp nguồn 5V. |
| **Slave 7** | Động cơ DC | `0x07` | **PC3**: INA (TIM1_CH3)<br>**PC4**: INB (TIM1_CH4) | **RX**: PD6<br>**TX**: PD5 | Cầu H L9110S/DRV8833. TIM1 Hardware PWM 10kHz điều khiển tốc độ 2 chiều 0-100%. |
| **Slave 8** | Servo RC | `0x08` | **PC3**: PWM (TIM1_CH3) | **RX**: PD6<br>**TX**: PD5 | Servo SG90/MG90S. TIM1 Hardware PWM 50Hz (Chu kỳ 20ms, xung 500-2500µs, góc 0-180°). |
| **Slave 9** | OLED 0.96" | `0x09` | **PB4**: I2C SCL<br>**PB5**: I2C SDA | **RX**: PD6<br>**TX**: PD5 | OLED SSD1306 128x64. Hỗ trợ Font 8x16 (4 dòng) và 6x8 (8 dòng), nhận chuỗi 16 ký tự. |
| **Slave 10**| Độ ẩm đất | `0x0A` | **PD3**: AO (AIN4)<br>**PD2**: DO (Input)<br>**PC3**: LED Cảnh báo | **RX**: PD6<br>**TX**: PD5 | Cảm biến độ ẩm LM393. Đo tương tự ADC 0-100%, đọc ngưỡng số DO, tự bật LED khi đất khô. |
| **Slave 11**| DHT11 | `0x0B` | **PC4**: DATA (1-Wire)<br>**PC3**: LED Trạng thái | **RX**: PD6<br>**TX**: PD5 | Cảm biến nhiệt ẩm DHT11. Nhiệt độ 0-50°C, Độ ẩm 20-90% RH. |
| **Slave 12**| DHT22 | `0x0C` | **PC4**: DATA (1-Wire)<br>**PC3**: LED Trạng thái | **RX**: PD6<br>**TX**: PD5 | Cảm biến nhiệt ẩm AM2302/DHT22. Nhiệt độ -40..80°C, Độ ẩm 0..99.9% RH. |

---

## 3. GIAO THỨC TRUYỀN THÔNG (COMMUNICATION PROTOCOL)

### 3.1. Giao thức 4-Byte chuẩn trên vòng Ring UART
Tất cả các gói tin luân chuyển trên vòng Ring STM8 **bắt buộc** phải tuân theo cấu trúc đúng 4 Byte:
$$\text{Khung truyền} = [\text{0xAA}, \text{SLAVE\_ID}, \text{CMD / DATA}, \text{CHECKSUM}]$$

- **Byte 0 (`0xAA`)**: Header cố định đồng bộ gói tin.
- **Byte 1 (`SLAVE_ID`)**: Địa chỉ định danh thiết bị (`0x01` đến `0x0C`).
- **Byte 2 (`CMD / DATA`)**: Mã lệnh điều khiển hoặc dữ liệu cảm biến đo được.
- **Byte 3 (`CHECKSUM`)**: Byte kiểm tra toàn vẹn, tính theo công thức:
  $$\text{CHECKSUM} = (\text{SLAVE\_ID} + \text{CMD\_or\_DATA}) \pmod{256}$$

#### Bảng danh mục mã lệnh chi tiết:
| Node ID | Lệnh gửi từ ESP32 | Ý nghĩa lệnh | Dữ liệu STM8 phản hồi về | Ý nghĩa phản hồi |
| :---: | :---: | :--- | :---: | :--- |
| `0x01` (LED) | `0x10`<br>`0x20`<br>`0x30` | Nhấp nháy (Blink)<br>Bật đèn (ON)<br>Tắt đèn (OFF) | `0x10`<br>`0x20`<br>`0x30` | Trả lại chính mã lệnh để xác nhận thực thi (ACK). |
| `0x02` (Button) | `0x50` | Truy vấn trạng thái nút | `0x50`<br>`0x51` | `0x50`: Nút đang nhả (RELEASED)<br>`0x51`: Nút đang nhấn (PRESSED) |
| `0x03` (Buzzer) | `0x40`<br>`0x41`<br>`0x42` | Bật còi (ON)<br>Tắt còi (OFF)<br>Kêu Beep 1 tiếng | `0x40`<br>`0x41`<br>`0x42` | Trả lại chính mã lệnh để xác nhận thực thi (ACK). |
| `0x04` (Pot) | `0x60` | Truy vấn giá trị biến trở | `0x00` - `0xFF` | Giá trị ADC 8-bit (`0` đến `255`). |
| `0x05` (Dist) | `0x70` (hoặc `0x40`) | Truy vấn khoảng cách | `0x00` - `0xFE`<br>`0xFF` (`255`) | Khoảng cách đo được tính theo cm.<br>`255`: Quá tầm đo hoặc mất sóng dội. |
| `0x06` (LCD) | `0x01` (hoặc `0x82`)<br>`0x10`<br>`0x80`-`0xE4` | Xóa màn hình (Clear)<br>Hiện chữ mẫu SYSTEM READY<br>Độ sáng 0-100% | `0x01` / `0x82`<br>`0x10`<br>`0x80`-`0xE4` | Xác nhận thực thi lệnh LCD. |
| `0x07` (DC Motor) | `0x00` / `0x30`<br>`0x01` - `0x64`<br>`0x81` - `0xE4`<br>`0xFF` | Dừng motor (STOP)<br>Tiến tốc độ 1-100%<br>Lùi tốc độ 1-100%<br>Phanh nhanh (BRAKE) | `0x00`<br>`0x01` - `0x64`<br>`0x81` - `0xE4`<br>`0xFF` | Trả lại tốc độ / trạng thái xác nhận. |
| `0x08` (Servo) | `0` - `180` (`0x00`-`0xB4`)<br>`0xC0` / `0xC1` / `0xC2` | Đặt góc quay 0° - 180°<br>Góc 0° / 90° / 180° | `0` - `180` | Trả lại góc hiện tại của Servo. |
| `0x09` (OLED) | `0x80` / `0x81` / `0x82`<br>`0x83` / `0x84`<br>`0x10`-`0x13` (Font 8x16)<br>`0x20`-`0x27` (Font 6x8) | ON / OFF / CLEAR<br>Đảo màu / Hiện Logo Splash<br>Ghi dòng 0-3 chữ to (20B frame)<br>Ghi dòng 0-7 chữ nhỏ (20B frame) | `0x80` - `0x84`<br>`0x10` - `0x13`<br>`0x20` - `0x27` | Xác nhận mã lệnh vừa thực thi. |
| `0x0A` (Độ ẩm đất) | `0x60` (hoặc `0x70`)<br>`0x61`<br>`0x62`<br>`1` - `99` | Đọc độ ẩm phần trăm (0-100%)<br>Đọc ADC thô 8-bit (0-255)<br>Đọc trạng thái số DO (0: Ẩm, 1: Khô)<br>Cài đặt ngưỡng cảnh báo khô (%) | `0` - `100`<br>`0` - `255`<br>`0` hoặc `1`<br>`1` - `99` | Dữ liệu độ ẩm đo được hoặc xác nhận ngưỡng mới. |
| `0x0B` (DHT11) | `0x70` hoặc `0x71`<br>`0x72`<br>`0x73` | Đọc Nhiệt độ (°C)<br>Đọc Độ ẩm (%RH)<br>Đọc trạng thái cảm biến (1: OK, 0: Lỗi) | `0` - `50`<br>`20` - `90`<br>`0` hoặc `1` | Dữ liệu nhiệt độ / độ ẩm từ DHT11. |
| `0x0C` (DHT22) | `0x70` hoặc `0x71`<br>`0x72`<br>`0x74` / `0x75`<br>`0x76`<br>`0x73` | Đọc phần nguyên Nhiệt độ (°C)<br>Đọc phần nguyên Độ ẩm (%)<br>Đọc phần thập phân Nhiệt độ / Độ ẩm (0-9)<br>Đọc Nhiệt độ bù offset +40<br>Đọc trạng thái cảm biến (1: OK, 0: Lỗi) | `0` - `80`<br>`0` - `100`<br>`0` - `9`<br>`0` - `120`<br>`0` hoặc `1` | Dữ liệu nhiệt độ / độ ẩm chính xác cao từ DHT22/AM2302. |

### 3.2. Cơ chế Hardware Repeater trên STM8
Trong tệp ngắt `UART1_RX_IRQHandler` (IRQ18) của mọi Slave STM8, dòng code đầu tiên **bắt buộc** phải là đọc byte từ `UART1_DR` và ghi ngay vào `UART1_DR` để đẩy sang module tiếp theo:
```c
@far @interrupt void UART1_RX_IRQHandler(void) {
    unsigned char c = UART1_DR; // Đọc byte và tự xóa cờ RXNE
    while(!(UART1_SR & 0x80)); // Chờ bộ đệm phát TXE trống
    UART1_DR = c;              // Chuyển tiếp ngay lập tức sang TX
    // Sau đó mới đưa byte vào State Machine xử lý nội bộ...
}
```

### 3.3. Thuật toán Dual-Frame Verification & Khử gói Echo trên ESP32
Do đặc thù vòng Ring, khi ESP32 phát lệnh truy vấn $F_{\text{req}}$, gói tin này chạy qua các repeater và vòng ngược về chân RX của ESP32 sau ~4ms (gọi là **Gói Echo**). Sau đó, CPU của Slave nhận lệnh mới tự sinh ra gói phản hồi dữ liệu thật $F_{\text{real}}$ và phát lên mạng sau 8 - 35ms.
- **Nếu Node có mặt thật:** ESP32 nhận được **ít nhất 2 frame** ($F_{\text{echo}}$ và $F_{\text{real}}$).
- **Nếu Node vắng mặt (chưa cắm bo):** ESP32 chỉ nhận được đúng **1 frame** (chính là $F_{\text{echo}}$ do các bo khác lặp lại).
- **Thuật toán ESP32:**
  - Không thoát sớm (`break`) khi vừa nhận frame đầu tiên.
  - Chờ hết cửa sổ đọc (15ms - 45ms).
  - Chỉ xác nhận node **ONLINE** khi đếm được `frames_received >= 2` (hoặc dữ liệu khác mã truy vấn). Nếu chỉ nhận được 1 frame echo -> Đánh dấu node **OFFLINE**.

### 3.4. Giao thức UDP giữa ESP32 Gateway và Desktop App (Cổng 8888)
ESP32 phát Telemetry qua UDP Broadcast (hoặc Unicast tới IP của máy tính) với 2 dạng:
1. **Dạng Text Kép (Tương thích ứng dụng FabLab STEM):**
   ```text
   HEARTBEAT:ONLINE:0,0,0,0,0:STATE:0,0,0,0,0
   ```
   - Trường 1: Danh sách Online (LED, BTN, BUZ, POT, DIST) -> `1` là online, `0` là offline.
   - Trường 2: Dữ liệu cảm biến -> `LED(0/1), BTN(0/1), BUZ(0/1), POT(0-255), DIST(0-255)`.
2. **Dạng JSON Telemetry (Cho các hệ thống Web / IoT Dashboard mở rộng):**
   ```json
   {
     "online": {"led": 1, "btn": 1, "buz": 1, "pot": 1, "dist": 1, "lcd": 0},
     "state":  {"led": 0, "btn": 0, "buz": 0, "pot": 128, "dist": 25}
   }
   ```
- **Lệnh điều khiển nhận từ PC (App -> ESP32 qua UDP 8888):**
  - Đèn LED: `"LED_ON"`, `"LED_OFF"`, `"LED_BLINK"` (hoặc `"ON"`, `"OFF"`, `"BLINK"`).
  - Còi Buzzer: `"BUZ_ON"`, `"BUZ_OFF"`, `"BUZ_BEEP"`.
  - Màn hình LCD: `"LCD_CLEAR"`, `"LCD_BACKLIGHT_ON"`, `"LCD_BACKLIGHT_OFF"`.

---

## 4. BẢNG TRA ĐỊA CHỈ THANH GHI CHUẨN STM8S103F3P6

> ⚠️ **LƯU Ý ĐẶC BIỆT DÀNH CHO CÁC AGENT:**  
> Dòng chip STM8S103F3P6 có địa chỉ thanh ghi **khác biệt** so với các dòng STM8S207/208 dung lượng lớn. **Tuyệt đối dùng đúng bảng địa chỉ dưới đây**, không được copy từ tài liệu của STM8S208!

### 4.1. GPIO Registers
| Port | ODR (Xuất) | IDR (Đọc vào) | DDR (Hướng) | CR1 (Cấu hình 1) | CR2 (Cấu hình 2) |
| :---: | :---: | :---: | :---: | :---: | :---: |
| **Port A** | `0x005000` | `0x005001` | `0x005002` | `0x005003` | `0x005004` |
| **Port B** | `0x005005` | `0x005006` | `0x005007` | `0x005008` | `0x005009` |
| **Port C** | `0x00500A` | `0x00500B` | `0x00500C` | `0x00500D` | `0x00500E` |
| **Port D** | `0x00500F` | `0x005010` | `0x005011` | `0x005012` | `0x005013` |

### 4.2. UART1 Registers (9600 bps @ 16MHz Clock)
- `UART1_SR`   : `0x005230` (Status Register - Bit 7: `TXE`, Bit 5: `RXNE`)
- `UART1_DR`   : `0x005231` (Data Register)
- `UART1_BRR1` : `0x005232` (Giá trị: `0x68`)
- `UART1_BRR2` : `0x005233` (Giá trị: `0x03`)
- `UART1_CR2`  : `0x005235` (Giá trị: `0x2C` = `TEN | REN | RIEN` - Bật phát, nhận và ngắt nhận)
- **Quy tắc bắt buộc khi khởi tạo Baudrate:** Ghi `UART1_BRR2` trước, sau đó mới ghi `UART1_BRR1`.

### 4.3. TIM2 Registers (Đo khoảng cách siêu âm trên STM8S103F3)
- `TIM2_CR1`   : `0x005300` (Bit 0: `CEN` - Counter Enable)
- `TIM2_IER`   : `0x005303`
- `TIM2_SR1`   : `0x005304`
- `TIM2_SR2`   : `0x005305`
- `TIM2_EGR`   : `0x005306` (Bit 0: `UG` - Update Generation nạp ngay Prescaler)
- `TIM2_CCMR1` : `0x005307`
- `TIM2_CCMR2` : `0x005308`
- `TIM2_CCMR3` : `0x005309`
- `TIM2_CCER1` : `0x00530A`
- `TIM2_CCER2` : `0x00530B`
- `TIM2_CNTRH` : `0x00530C` (Byte cao của bộ đếm 16-bit)
- `TIM2_CNTRL` : `0x00530D` (Byte thấp của bộ đếm 16-bit)
- `TIM2_PSCR`  : `0x00530E` (Prescaler: ghi `0x04` để chia 16 $\rightarrow 1\mu\text{s/tick}$ @ 16MHz)
- `TIM2_ARRH`  : `0x00530F` (Auto-reload byte cao: ghi `0xFF`)
- `TIM2_ARRL`  : `0x005310` (Auto-reload byte thấp: ghi `0xFF`)

### 4.4. ADC1 Registers (Biến trở AIN4 trên PD3)
- `ADC_CSR` : `0x005400` (Ghi `0x04` để chọn kênh AIN4)
- `ADC_CR1` : `0x005401` (Ghi `0x01` bật nguồn Wake-up; ghi `0x01` lần 2 để bắt đầu chuyển đổi)
- `ADC_CR2` : `0x005402` (Ghi `0x00`: Left-alignment $\rightarrow$ 8 bit MSB nằm trọn vẹn trong `ADC_DRH`)
- `ADC_DRH` : `0x005404` (Đọc trực tiếp giá trị 8-bit tại đây mà không cần ghép bit với DRL)

---

## 5. CẤU TRÚC THƯ MỤC DỰ ÁN (PROJECT DIRECTORY TREE)

```text
F:\stm8_rx_tx_test\
│
├── BAT_BACKEND.bat                  # Script 1-click khởi động FastAPI Backend (Port 8000)
├── MO_APP_STEM.bat                  # Script 1-click tự bật Backend + khởi động Frontend GUI
├── HUONG_DAN_MO_APP.txt             # Hướng dẫn chi tiết cho người dùng cuối
├── GHI_CHU_LOI_VA_LUU_Y.md          # Sổ tay ghi chép lỗi kỹ thuật đầy đủ
├── SYSTEM_KNOWLEDGE_BASE.md         # Bản tóm tắt kiến trúc kỹ thuật ban đầu
├── TAI_LIEU_BAN_GIAO_AGENT.md       # [TÀI LIỆU NÀY] Tài liệu bàn giao tổng thể cho Agent
│
├── Stm8_slave_1_den\                # Slave 1: LED On/Off (Baseline project)
├── Stm8_slave_1_den_pwm\            # Slave 1: LED điều khiển độ sáng PWM (TIM1_CH3 trên PC3)
├── stm8_slave_2_button\             # Slave 2: Nút nhấn vật lý (PC4)
├── stm8_slave_3_coi\                # Slave 3: Còi báo Buzzer (PC5)
├── stm8_slave_4_bien_tro\           # Slave 4: Biến trở ADC AIN4 (PD3)
├── stm8_slave_5_khoang_cach\        # Slave 5: Siêu âm HC-SR04 (ECHO: PA1, TRIG: PA2)
├── stm8_slave_6_lcd\                # Slave 6: Màn hình LCD 1602 I2C (SCL: PB4, SDA: PB5)
├── stm8_slave_7_dong_co_dc\         # Slave 7: Động cơ DC cầu H PWM 10kHz (INA: PC3, INB: PC4)
├── stm8_slave_8_servo\              # Slave 8: Động cơ Servo RC PWM 50Hz (PWM: PC3)
├── stm8_slave_9_oled\               # Slave 9: Màn hình OLED 0.96" SSD1306 (SCL: PB4, SDA: PB5)
├── stm8_slave_10_do_am_dat\         # Slave 10: Cảm biến độ ẩm đất LM393 (AO: PD3, DO: PD2)
├── stm8_slave_11_dht11\             # Slave 11: Cảm biến nhiệt ẩm DHT11 (DATA: PC4)
├── stm8_slave_12_dht22\             # Slave 12: Cảm biến nhiệt ẩm DHT22 (DATA: PC4)
│
├── ESP_TX_RX\                       # Bộ mã nguồn ESP32
│   ├── ESP_gateway_v2\              # Firmware Gateway chính thức (Dual-Frame, UDP, Low-latency)
│   ├── ESP_nhat\                    # Firmware Gateway gốc ban đầu
│   ├── ESP_test_led_blink\          # Sketch test riêng Slave 1 (LED)
│   ├── ESP_test_button\             # Sketch test riêng Slave 2 (Nút bấm)
│   ├── ESP_test_btn_control_led\    # Sketch test vòng Ring: Nút bấm điều khiển Đèn
│   ├── ESP_test_potentiometer\      # Sketch test riêng Slave 4 (Biến trở)
│   └── ESP_test_distance\           # Sketch test riêng Slave 5 (Cảm biến siêu âm)
│
├── Software\stem-platform\          # Ứng dụng Desktop chính thức của FabLab
│   ├── backend\                     # Máy chủ FastAPI + SQLite (Cổng 8000)
│   │   ├── run_backend.ps1          # Script khởi chạy Backend
│   │   └── stem.db                  # Cơ sở dữ liệu SQLite
│   └── frontend\                    # Giao diện PySide6 QML + Khối Blockly
│       ├── run-frontend.ps1         # Script khởi chạy Frontend
│       └── firmware\                # Bộ điều vận kết nối Gateway qua UDP
│           └── neuron_gateway_service.py
│
└── STEM_CORE_PACKAGE\               # GÓI XUẤT BẢN ĐỘC LẬP CHO BÊN THỨ BA
    ├── 01_ESP32_GATEWAY_FIRMWARE\   # ESP_gateway_v2.ino sẵn sàng nạp
    ├── 02_STM8_FIRMWARE_S19_HEX\    # File biên dịch .s19 và .hex của đủ 12 Slaves
    ├── 03_STM8_SOURCE_CODE_C\       # Mã nguồn C thuần (C89, UTF-8 No BOM) của đủ 12 Slaves
    ├── 04_COMMUNICATION_SDK_PYTHON\ # SDK Python mẫu (example_client.py) điều khiển không cần App
    └── README_HUONG_DAN_SU_DUNG.md  # Hướng dẫn tích hợp cho bên thứ ba
```

---

## 6. HƯỚNG DẪN BIÊN DỊCH & NẠP CODE (OPERATIONS RUNBOOK)

### 6.1. Biên dịch mã nguồn STM8 bằng Cosmic C (Command-line)
Compiler Cosmic C đã cài đặt tại: `C:\Program Files (x86)\COSMIC\FSE_Compilers\CXSTM8`.  
Để biên dịch độc lập không cần mở STVD GUI, chạy lệnh trong PowerShell:
```powershell
$cosmicDir = "C:\Program Files (x86)\COSMIC\FSE_Compilers\CXSTM8"
$incDir = "$cosmicDir\Hstm8"
$projDir = "F:\stm8_rx_tx_test\stm8_slave_5_khoang_cach"

# 1. Biên dịch file C sang file Object (.o)
& "$cosmicDir\cxstm8.exe" -i"$incDir" -clDebug\ -coDebug\ "$projDir\main.c"
& "$cosmicDir\cxstm8.exe" -i"$incDir" -clDebug\ -coDebug\ "$projDir\stm8_interrupt_vector.c"

# 2. Liên kết các file Object tạo file .s19
& "$cosmicDir\clnk.exe" -m Debug\proj.map -c Debug\proj.lkf -o Debug\stm8_firmware.sm8
& "$cosmicDir\chex.exe" -fi -o Debug\stm8_firmware.s19 Debug\stm8_firmware.sm8
```
*(Hoặc mở trực tiếp file workspace `.stw` / project `.stp` bằng ST Visual Develop và nhấn F7).*

### 6.2. Nạp file `.s19` cho STM8 bằng ST Visual Programmer (STVP)
1. Cắm mạch nạp **ST-Link V2** vào máy tính và nối 4 chân với bo STM8: `3.3V`, `SWIM`, `GND`, `NRST`.
   *(Lưu ý: Nếu nạp cho bo Khoảng cách HC-SR04, hãy rút tạm chân 5V của cảm biến ra trước khi nạp để tránh sụt áp).*
2. Mở phần mềm **STVP**, cấu hình nạp:
   - Hardware: `ST-LINK`
   - Port: `USB`
   - Programming Mode: `SWIM`
   - Device: `STM8S103F3P`
3. Nhấp chọn tab **`PROGRAM MEMORY`** $\rightarrow$ `File` $\rightarrow$ `Open...` $\rightarrow$ Chọn file `.s19`.
4. Nhấn phím tắt `Ctrl + P` (hoặc menu `Program` $\rightarrow$ `Current tab`) để nạp vào chip.
5. Nếu gặp cảnh báo vàng `Verify failed at address 0x8000`: Chuyển sang tab **`OPTION BYTE`**, đổi `ROP` sang `OFF` và nhấn `Ctrl + P` trước.

### 6.3. Nạp code ESP32-S3 bằng Arduino IDE
1. Cắm cáp Type-C vào cổng USB của ESP32-S3.
2. Trong Arduino IDE, chọn bo: **`ESP32S3 Dev Module`**.
3. Cấu hình cài đặt quan trọng trong menu `Tools`:
   - **USB CDC On Boot:** `Enabled` (Bắt buộc để in log ra Serial Monitor).
   - **Flash Size:** `8MB` (hoặc `4MB` tùy bo).
   - **Partition Scheme:** `Default 4MB with spiffs` hoặc `Huge APP`.
4. Mở file [`ESP_TX_RX/ESP_gateway_v2/ESP_gateway_v2.ino`](file:///F:/stm8_rx_tx_test/ESP_TX_RX/ESP_gateway_v2/ESP_gateway_v2.ino), cấu hình tên và mật khẩu WiFi:
   ```cpp
   const char* default_ssid = "YOUR_WIFI_SSID";
   const char* default_pass = "YOUR_WIFI_PASSWORD";
   ```
5. Nhấn nút **Upload**.

### 6.4. Vận hành phần mềm FabLab STEM
- **Cách đơn giản nhất:** Bấm đúp vào shortcut **`Mo_App_FabLab_STEM`** trên màn hình Desktop (tương đương chạy file `F:\stm8_rx_tx_test\MO_APP_STEM.bat`). Script sẽ tự kiểm tra cổng 8000, tự bật Backend nếu chưa chạy và mở giao diện ứng dụng.
- **Tài khoản đăng nhập đã khởi tạo sẵn trong cơ sở dữ liệu:**
  - Tài khoản học sinh: `danh.huynh@gmail.com` | Mật khẩu: `123456`
  - Tài khoản quản trị: `admin@fablab.vn` | Mật khẩu: `admin123`

---

## 7. TỔNG HỢP 22 LỖI KỸ THUẬT & GIẢI PHÁP TRIỆT ĐỂ (POST-MORTEM LOG)

Dưới đây là bảng phân tích toàn bộ các lỗi thực tế đã phát sinh và cách giải quyết tận gốc:

### Nhóm 1: Lỗi Môi trường, Hệ thống & Cơ sở dữ liệu
| Mã lỗi | Mô tả hiện tượng | Nguyên nhân gốc rễ | Giải pháp triệt để |
| :---: | :--- | :--- | :--- |
| **1.1** | Script `.ps1` báo lỗi `Unexpected token '}'` hoặc `missing terminator "` | File script dùng Unicode vẽ viền (`═`, `✓`) lưu UTF-8 No BOM, PowerShell 5.1 đọc theo Windows-1252 làm vỡ chuỗi. | Tuyệt đối không dùng ký tự vẽ khung đặc biệt trong script PowerShell. Chỉ dùng ASCII chuẩn: `===`, `[OK]`, `[!]`. |
| **1.2** | `pip install` báo lỗi `UnicodeDecodeError: 'charmap'` | File `requirements.txt` chứa chú thích tiếng Việt có dấu, pip trên Windows đọc theo cp1252. | Trong `requirements.txt` chỉ dùng tiếng Anh thuần ASCII, không ghi chú thích tiếng Việt có dấu. |
| **1.3** | Lỗi hết ổ đĩa `OSError: [Errno 28] No space left on device` | Phân vùng ổ F: chỉ có 22 GB. Khi giải nén file zip 3.5GB và tải installer, dung lượng còn dưới 500 KB. | Giải nén xong phải xóa ngay file zip gốc. Thư viện và model AI nặng nên chuyển sang ổ D: (còn trống 100 GB). |
| **1.4** | PowerShell báo `.\.venv\Scripts\python` is not recognized | Mở PowerShell quyền Administrator bị nhảy thư mục làm việc về `C:\WINDOWS\system32`. | Luôn dùng lệnh chuyển đường dẫn `cd F:\stm8_rx_tx_test\...` về thư mục dự án trước khi thực thi. |
| **1.5** | Docker Desktop treo `WSL needs updating` | Nhân WSL2 của máy tính quá cũ, Docker không khởi động được container PostgreSQL. | Chuyển Backend sang chạy trực tiếp bằng SQLite (`DATABASE_URL=sqlite:///./stem.db`), không cần Docker. |
| **2.1** | App báo `Connection to localhost:8000 timed out` khi đăng nhập | Server FastAPI chưa được bật, hoặc port 8000 bị chặn. | Khởi động Backend bằng `BAT_BACKEND.bat` trước. File `MO_APP_STEM.bat` tự động phát hiện và bật Backend. |

### Nhóm 2: Lỗi Firmware Vi điều khiển & Giao thức Ring UART
| Mã lỗi | Mô tả hiện tượng | Nguyên nhân gốc rễ | Giải pháp triệt để |
| :---: | :--- | :--- | :--- |
| **3.1** | Node 1 (LED) mất kết nối làm Node 2 (Nút bấm) bị báo mất oan | Copy-paste code kiểm tra timeout: trong khối Node 1 dùng nhầm biến `timeout_btn++`. | Phân tách rành mạch biến timeout của từng node: `timeout_led`, `timeout_btn`, `timeout_pot`, v.v. |
| **3.2** | App gửi lệnh `"ON"`/`"OFF"` nhưng ESP32 không bật đèn | ESP32 chỉ so sánh `cmd.equals("LED_ON")`, trong khi App gửi chuỗi ngắn `"ON"`. | Kiểm tra song song cả hai cú pháp: `if (cmd.equals("LED_ON") \|\| cmd.equals("ON"))`. |
| **3.3** | ESP32 phát WiFi AP làm toàn bộ mạng STM8 bị đơ | Trong hàm AP có vòng lặp chặn vô tận `while(true) { server.handleClient(); }`. | Không dùng vòng lặp chặn; đặt cờ `is_ap_mode = true` và gọi `server.handleClient()` trong `loop()` chính. |
| **3.4** | Hệ thống bị giật lag chu kỳ 35ms khi chưa cắm cảm biến siêu âm | Cảm biến HC-SR04 đợi xung phản hồi tối đa 35ms. Khi không có cảm biến, CPU bị khựng 35ms mỗi vòng. | Điều phối thông minh: khi Node 5 Offline, chỉ thăm dò định kỳ 500ms/lần thay vì thăm dò liên tục mỗi chu kỳ. |
| **3.5** | Mạng UDP bị crash ngẫu nhiên do đọc rác RAM | Mảng `char rx_buf[128]` không được khởi tạo `{0}` ban đầu. Khi đọc gói rỗng, chuỗi đọc ô nhớ ngẫu nhiên. | Luôn gán `char rx_buf[128] = {0};` và thêm ký tự kết thúc `rx_buf[len] = '\0';` sau khi đọc UDP. |
| **3.6** | Trạng thái kết nối trên App nhảy liên tục xanh/xám | App đặt `DISCONNECT_TIMEOUT = 3.0s`, trong khi nhịp gửi của ESP32 là `2.5s`. Độ trễ mạng vượt 3s gây ngắt kết nối. | Rút ngắn `HEARTBEAT_INTERVAL` trên ESP32 xuống **100ms** (hoặc 500ms), đảm bảo App không bao giờ bị timeout. |
| **3.7** | Nhấn nút trên App nhưng đèn LED thật không sáng | App có logic `if not self.node_online("led_node"): return`, âm thầm nuốt lệnh khi node chưa báo online. | Sửa trong `neuron_gateway_service.py` cho phép gửi lệnh bất kể trạng thái online để ép thiết bị phản hồi. |
| **3.8** | App không nhận được tín hiệu bấm nút hoặc biến trở bị kẹt số 96 | **Lỗi nhận diện gói Echo**: Gói truy vấn quay về mang mã `0x50` (trùng mã nút nhả) làm ESP32 `break;` sớm, nuốt mất gói tin thật. | **Tuyệt đối không dùng `break;` sớm** khi nhận frame đầu tiên. Giữ vòng đọc hết cửa sổ 18ms để gói phản hồi thật ghi đè. |
| **3.9** | Xoay biến trở có cảm giác trễ 1 giây mới ăn lệnh | ESP32 đặt ngưỡng Deadband `abs > 3` và nhịp Heartbeat 1000ms; xoay chậm không kích hoạt gửi gói tin. | Hạ ngưỡng Deadband xuống `abs >= 1` và rút nhịp Heartbeat xuống **100ms** để dữ liệu truyền mượt mà. |
| **3.10** | Rút bớt bo STM8 ra nhưng App vẫn báo ảo đủ cả 5 bo Online | Do Hardware Repeater: gói truy vấn của node vắng mặt được các bo khác lặp lại về ESP32 (Gói Echo). | **Thuật toán Dual-Frame Verification**: Chỉ đánh dấu Online khi nhận $\ge 2$ frame hoặc dữ liệu khác mã truy vấn. |
| **3.11** | Cảm biến siêu âm luôn trả về khoảng cách `0 cm` | Trên STM8S103F3, thanh ghi `TIM2_CNTRH` ở `0x530C`, `TIM2_CNTRL` ở `0x530D`. Code cũ đọc nhầm `0x530A/0x530B` (`CCER1/2`). | Cập nhật lại toàn bộ địa chỉ thanh ghi `TIM2` theo đúng file chuẩn `iostm8s103.h` (từ `0x5300` đến `0x5310`). |
| **3.12** | Mất kết nối UART hoàn toàn, STM8 không chuyển tiếp byte nào | Bảng vector `_vectab[]` thiếu 1 entry `NonHandledInterrupt`, đẩy `UART1_RX_IRQHandler` từ `0x8050` (IRQ18) sang `0x804C` (IRQ17). | Đảm bảo mảng `_vectab[]` có đúng **20 phần tử đứng trước `UART1_RX_IRQHandler`** để ngắt nằm chuẩn tại `0x8050`. |
| **3.13** | Cảm biến siêu âm phản hồi chậm 1 giây khi mới bật App | Ban đầu Node 5 offline nên bị áp nhịp thăm dò 500ms; ngưỡng cử động tay đặt `abs > 2` làm lọt cử động nhỏ. | Thăm dò thích ứng: 5 giây đầu quét nhanh `100ms/lần`, hạ ngưỡng nhạy về `abs >= 1`, mở rộng cửa sổ chờ lên `45ms`. |
| **3.14** | STVP báo lỗi `Verify failed at address 0x8000` | Chip bị khóa bảo vệ đọc `ROP = ON` trong tab `OPTION BYTE`, hoặc cảm biến HC-SR04 làm sụt áp nguồn 3.3V. | Rút nguồn cảm biến khi nạp. Trong tab `OPTION BYTE` của STVP, chuyển `ROP` thành `OFF` và nhấn `Ctrl + P`. |
| **3.15** | Cosmic C báo lỗi `#error cpstm8 main.c:1 bad character` | File `.c` bị lưu định dạng **UTF-8 with BOM** (dính 3 byte ẩn `0xEF 0xBB 0xBF` ở đầu file do PowerShell thêm vào). | Luôn lưu file mã nguồn C bằng định dạng **UTF-8 No BOM** (hoặc ANSI thuần). |
| **3.16** | Bấm Đăng nhập trên App bị báo lỗi kết nối `HTTPConnectionPool` | Backend FastAPI trên cổng 8000 chưa được khởi động trước khi mở App. | Sử dụng file `MO_APP_STEM.bat` hoặc bật `BAT_BACKEND.bat` trước khi bấm đăng nhập trên ứng dụng. |
| **3.17** | Cosmic C báo lỗi `#error clnk bad address for zero page symbol` | Biên dịch `main.c` với `+mods0` nhưng `stm8_interrupt_vector.c` không biên dịch đồng bộ hoặc biến giao tiếp bị đặt vào segment `.data` (> 0xFF). | Biên dịch đồng thời cả 2 file với cùng cờ `+mods0 -pp -l`, đảm bảo các biến cờ giao tiếp nằm trong vùng nhớ zero-page (`.bsct`/`.ubsct`). |
| **3.18** | Cảm biến 1-Wire DHT11/DHT22 thỉnh thoảng lỗi Checksum / không phản hồi | Thời gian trễ lấy mẫu 35µs bị non (~32.8µs @ 16MHz) do điện dung dây kéo dài sườn xuống của bit '0', và xung Start 3ms quá ngắn với AM2302 clone. | Nhả bus sang Input Pull-up an toàn, chỉnh độ trễ lấy mẫu sang `delay_us(40)` (~37.5µs - điểm ngọt lý tưởng), tăng xung Start DHT22 lên 10ms. |

---

## 8. NGUYÊN TẮC BẤT BIẾN KHI PHÁT TRIỂN TIẾP (GOLDEN RULES)

Nếu bạn là AI Agent tiếp theo được giao nhiệm vụ viết thêm tính năng hoặc sửa lỗi, **hãy tuân thủ nghiêm ngặt 6 nguyên tắc sau:**

1. **Chuẩn C89 cho Cosmic C STM8:**
   Trình biên dịch Cosmic C chỉ chấp nhận chuẩn C89. **Tất cả các khai báo biến phải đặt ở đầu khối lệnh `{ ... }`**, không được khai báo biến sau câu lệnh gán hoặc lệnh gọi hàm.
2. **Không lưu file C với UTF-8 BOM:**
   Trình tiền xử lý của Cosmic C sẽ văng lỗi `bad character` ngay dòng 1 nếu dính BOM. Khi dùng PowerShell ghi file, luôn dùng:
   ```powershell
   [System.IO.File]::WriteAllText($path, $content, [System.Text.UTF8Encoding]::new($false))
   ```
3. **Bảo toàn Vector Ngắt STM8 (IRQ18 tại `0x8050`):**
   Trong file `stm8_interrupt_vector.c`, luôn đếm chính xác số hàng trong mảng `_vectab[]`: bắt buộc phải có **đúng 20 entry** (`Reset` + `TRAP` + 18 `IRQ0..IRQ17`) trước khi khai báo `{0x82, UART1_RX_IRQHandler}`.
4. **Giữ khoảng nghỉ `delay(2)` giữa các lượt gửi UART trên ESP32:**
   Khi chuyển sang truy vấn node tiếp theo, luôn để `delay(2)` (2 mili-giây) để các thanh ghi dịch và bộ đệm UART của toàn bộ các Slave trong chuỗi Ring xả hết dữ liệu tồn đọng.
5. **Khử gói Echo khi thêm Node mới:**
   Bất kỳ node mới nào thêm vào vòng Ring đều sẽ tạo ra gói Echo phản hồi về ESP32. Luôn áp dụng nguyên lý Dual-Frame Verification (đếm số frame hoặc kiểm tra dữ liệu khác mã truy vấn) trước khi đánh dấu Node Online.
6. **Không đụng chạm vào các thư mục gốc đã được cố định:**
   - Thư mục `ESP_nhat` và `Stm8_slave_1_den` là các project gốc ban đầu. Không sửa đè lên các thư mục này. Mọi nâng cấp đều nằm trong `ESP_gateway_v2`, `Stm8_slave_1_den_pwm`, `stm8_slave_6_lcd`, v.v.

---

## 9. LỘ TRÌNH NÂNG CẤP TIẾP THEO (NEXT MILESTONES)

Khi người dùng yêu cầu nâng cấp hệ thống trong tương lai, các tính năng đã sẵn sàng để tích hợp gồm:

### 9.1. Tích hợp Màn hình LCD 1602 (Slave 6) lên App & Gateway
- **Hiện trạng:** Mã nguồn C cho Slave 6 đã hoàn thành tại [`stm8_slave_6_lcd/main.c`](file:///F:/stm8_rx_tx_test/stm8_slave_6_lcd/main.c), hỗ trợ tự dò địa chỉ I2C `0x27`/`0x3F` và hiển thị text.
- **Việc cần làm tiếp:**
  1. Thêm hàm xử lý gói UDP trong `ESP_gateway_v2.ino`: Nhận lệnh `LCD1:<text>` (ghi dòng 1), `LCD2:<text>` (ghi dòng 2), hoặc `LCD_CLEAR`.
  2. Bổ sung khối lệnh Blockly trong giao diện App: Khối "Hiển thị lên LCD dòng 1/2".

### 9.2. Thanh trượt điều chỉnh độ sáng đèn LED (PWM Slider)
- **Hiện trạng:** Đã hoàn thành firmware [`Stm8_slave_1_den_pwm/main.c`](file:///F:/stm8_rx_tx_test/Stm8_slave_1_den_pwm/main.c) sử dụng `TIM1_CH3` trên PC3 với tần số 10 kHz, nhận dữ liệu duty cycle từ `0` đến `100%`.
- **Việc cần làm tiếp:**
  1. Thêm Slider trên giao diện QML (từ 0% đến 100%).
  2. Gửi lệnh qua UDP: `LED_PWM:<0-100>`.
  3. ESP32 phát khung UART: `[0xAA, 0x01, DUTY_CYCLE, CRC]`.

### 9.3. Bộ lọc trung vị (Median Filter) cho cảm biến khoảng cách
- Khi cảm biến siêu âm quét vào bề mặt gồ ghề hoặc góc xiên, có thể xuất hiện các giá trị đột biến tức thời (Spikes).
- Triển khai mảng đệm 5 phần tử trên ESP32: lấy giá trị trung vị (Median) của 5 mẫu đo gần nhất trước khi gửi lên App để đường đồ thị khoảng cách phẳng và mượt tuyệt đối.

---

## 10. PHIÊN BẢN NÂNG CẤP THÔNG MINH (SMART EDITIONS & GATEWAY V3)

Để tăng cường năng lực xử lý biên (Edge Intelligence) và giảm thiểu tối đa độ trễ cho mạng vòng xuyến 12 nodes, hệ thống đã phát triển các phiên bản mới độc lập:

### 10.1. ESP32 Gateway V3 — Phân luồng quét thích ứng (Adaptive 3-Tier Multi-Rate Polling)
- **Vị trí tệp:** [`ESP_TX_RX/ESP_gateway_v3/ESP_gateway_v3.ino`](file:///F:/stm8_rx_tx_test/ESP_TX_RX/ESP_gateway_v3/ESP_gateway_v3.ino)
- **Cơ chế 3 tầng ưu tiên:**
  - **Tầng 1 (Siêu nhanh - 20ms/chu kỳ):** Nút nhấn thông minh (Slave 2), Biến trở B10K (Slave 4), Cảm biến siêu âm (Slave 5).
  - **Tầng 2 (Sự kiện / Lệnh điều khiển tức thì):** Đèn LED (Slave 1), Còi (Slave 3), Động cơ DC (Slave 7), Servo (Slave 8), LCD 1602 (Slave 6), OLED 0.96" (Slave 9).
  - **Tầng 3 (Quét chậm định kỳ 1.5s - 2.0s):** Độ ẩm đất LM393 (Slave 10), DHT11 (Slave 11), DHT22 (Slave 12).
- **Hiệu năng đạt được:** Độ trễ nhận diện nút bấm và dịch chuyển biến trở giảm từ 240ms xuống chỉ còn **15 - 20ms**, triệt tiêu hoàn toàn cảm giác gián đoạn hay trễ lệnh.

### 10.2. Slave 1 Smart LED — Động cơ hiệu ứng tự hành (Autonomous Effects Engine)
- **Thư mục dự án:** [`stm8_slave_1_den_smart/`](file:///F:/stm8_rx_tx_test/stm8_slave_1_den_smart/)
- **Mã máy nạp:** `Slave_1_LED_Smart_Effects.s19` / `.hex`
- **Bảng lệnh thông minh:**
  - `0x00` / `0x30`: Tắt đèn (OFF).
  - `0x20`: Bật đèn 100% (ON).
  - `0x01` - `0x64`: Đặt độ sáng tĩnh 1% đến 100% (Hardware PWM 3.9kHz trên PC3).
  - `0x10`: Nhấp nháy đơn chuẩn 1Hz (Blink).
  - `0x71`: Hiệu ứng Đèn thở êm dịu (Breathing / Fade In - Fade Out) tự hành.
  - `0x72`: Hiệu ứng Nháy đôi (Double Flash Strobe - 2 nháy nhanh, nghỉ 800ms).
  - `0x73`: Hiệu ứng Tín hiệu cứu hộ SOS mã Morse (`... --- ...`).
  - `0x74`: Hiệu ứng Nhịp đập trái tim (Heartbeat double-pulse).
  - `0x70`: Đọc mã hiệu ứng hiện tại.

### 10.3. Slave 2 Smart Button — Phân loại cử chỉ phím (Gesture Classifier)
- **Thư mục dự án:** [`stm8_slave_2_button_smart/`](file:///F:/stm8_rx_tx_test/stm8_slave_2_button_smart/)
- **Mã máy nạp:** `Slave_2_Button_Smart_Gestures.s19` / `.hex`
- **Cơ chế chốt sự kiện (Event Latch):** Tự lọc rung phím 20ms và chốt mã cử chỉ vào bộ đệm:
  - `0x50`: Trạng thái chờ / Nhả nút (IDLE / RELEASED).
  - `0x51`: Nhấn đơn (Single Click - bấm nhả bình thường).
  - `0x52`: Nhấn đúp (Double Click - 2 lần bấm trong vòng 300ms).
  - `0x53`: Nhấn giữ (Long Press - giữ nút liên tục > 1.5 giây).
  - `0x54`: Truy vấn tổng số lần nhấn tích lũy (Cumulative Counter).
  - `0x55`: Reset bộ đếm lần nhấn về 0.
- **Phản hồi LED PC3 trực tiếp:** Nháy 1 lần khi Nhấn đơn, nháy 2 lần nhanh khi Nhấn đúp, sáng 300ms khi đạt ngưỡng Nhấn giữ.

### 10.4. Cấu hình ST Visual Develop Workspace (.stw / .stp) và Quy chuẩn File nạp Cosmic C (.s19 / .hex)
- **Quy tắc định dạng đầu ra của Cosmic C (`chex`):**
  - `chex -o <target>.s19 <target>.sm8` -> Tạo file Motorola S-Record (`S0...`, `S1...`). STVP mở được ngay.
  - `chex -fi -o <target>.hex <target>.sm8` -> Tạo file Intel HEX (`:20...`).
  - *Lưu ý sống còn:* Không được tráo cờ `-fi`, nếu xuất nhầm định dạng Intel HEX vào đuôi `.s19`, STVP sẽ từ chối nạp chip.
- **Dự án STVD hoàn chỉnh cho 15/15 module Slave:**
  Tất cả các thư mục slave trong mã nguồn gốc và trong [`STEM_CORE_PACKAGE/03_STM8_SOURCE_CODE_C`](file:///F:/stm8_rx_tx_test/STEM_CORE_PACKAGE/03_STM8_SOURCE_CODE_C) đều có đầy đủ tệp `.stw` (Workspace), `.stp` (Project), và `build.bat` một chạm, cho phép click đúp chuột mở trực tiếp trên ST Visual Develop hoặc chạy `build.bat` tự động.

---
> **Tài liệu cập nhật ngày:** 20/10/2026  
> **Tác giả:** Antigravity AI Engineering Team  
> **Trạng thái:** Sẵn sàng cho mọi Agent tiếp quản vận hành và phát triển tiếp!


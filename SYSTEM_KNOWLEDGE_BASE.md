# SYSTEM KNOWLEDGE BASE & AGENT RUNBOOK: NEURON RING (ESP32-S3 + STM8S SLAVES)

> **DÀNH CHO CÁC AGENT & KỸ SƯ PHÁT TRIỂN TIẾP THEO:**
> Tài liệu này tổng hợp toàn bộ cấu trúc hệ thống, các lỗi sai chết người đã gặp, cách sửa chữa, và những nguyên tắc bất biến để không bao giờ lặp lại lỗi trên hệ thống này.

---

## 1. TỔNG QUAN KIẾN TRÚC HỆ THỐNG (SYSTEM ARCHITECTURE)

- **Master Gateway**: ESP32-S3 (ESP32-S3-WROOM-1, USB CDC, WiFi + WebServer + UDP Telemetry).
- **Slave Nodes**: STM8S103F3P6 (Tối thiểu 4 - 5 Module), biên dịch bằng **Cosmic C (C89)** trong **STVD (ST Visual Develop)**.
- **Tô pô mạng**: **Daisy-Chain UART Unidirectional Ring (Vòng xuyến đơn hướng)**.
  ```text
  [ESP32-S3 TX: GPIO 4] 
        │
        ▼
  [Slave 1: LED (PD6 RX -> PD5 TX)]
        │
        ▼
  [Slave 2: BUTTON (PD6 RX -> PD5 TX)]
        │
        ▼
  [Slave 3: BUZZER (PD6 RX -> PD5 TX)]
        │
        ▼
  [Slave 4: POT B10K (PD6 RX -> PD5 TX)]
        │
        ▼
  [Slave 5: HC-SR04 (PD6 RX -> PD5 TX)]
        │
        ▼
  [ESP32-S3 RX: GPIO 5]
  ```

- **Tốc độ truyền (Baudrate)**: `9600 bps`, `8N1` (8 Data bits, No Parity, 1 Stop bit).
- **Tần số thạch anh STM8**: 16MHz (Bộ dao động nội HSI, `CLK_CKDIVR = 0x00`).

---

## 2. NGUYÊN TẮC BẤT BIẾN CỦA GIAO THỨC NEURON RING (RING PROTOCOL RULES)

### Quy tắc 1: Khung truyền luôn cố định đúng 4 Byte (Strict 4-Byte Frame)
```text
Byte 0: 0xAA (Header cố định)
Byte 1: SLAVE_ID (0x01 .. 0x05)
Byte 2: CMD / DATA (Mã lệnh hoặc Giá trị đo)
Byte 3: CHECKSUM = (SLAVE_ID + CMD_or_DATA) & 0xFF  (Modulo-256)
```
- **TUYỆT ĐỐI KHÔNG ĐƯỢC:**
  - Không gửi thêm ký tự `\r` (`0x0D`) hoặc `\n` (`0x0A`).
  - Không gửi chuỗi ASCII (`printf`).
  - Không gửi frame biến động kích thước (như gửi 4 byte rồi gửi thêm 2 byte thô).
  - Bất kỳ byte rác nào lọt vào vòng tròn Ring sẽ làm hỏng state machine của tất cả các Slave phía sau!

### Quy tắc 2: Bắt buộc có Hardware Repeater trong ngắt UART RX
Tất cả các Slave STM8 **PHẢI** cấu hình ngắt nhận UART1 (`UART1_RX_IRQHandler` - IRQ18) và chuyển tiếp dữ liệu ngay lập tức:
```c
@far @interrupt void UART1_RX_IRQHandler(void) {
    unsigned char c = UART1_DR; // Đọc byte đến (tự xóa cờ RXNE)

    // HARDWARE REPEATER: Đẩy ngay sang TX cho Slave kế tiếp
    while(!(UART1_SR & 0x80)); // Chờ bộ đệm TXE trống
    UART1_DR = c;

    // Sau đó mới đưa vào State Machine Parser nội bộ
    ...
}
```
*Nếu bất kỳ Slave nào không có Hardware Repeater, toàn bộ các Slave phía sau sẽ bị ngắt liên lạc hoàn toàn với ESP32.*

---

## 3. DANH BẠ ĐỊA CHỈ & MÃ LỆNH CÁC SLAVE (NODE ADDRESSING)

| Node | Tên Module | Slave ID | Chân phần cứng | Mã lệnh gửi | Mã lệnh / Dữ liệu trả về |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Slave 1** | Đèn LED | `0x01` | PC3 (Output) | `0x10`: Blink<br>`0x20`: ON<br>`0x30`: OFF | Trả lại mã lệnh vừa thực hiện |
| **Slave 2** | Nút nhấn | `0x02` | PC4 (Input Pull-up, nhấn xuống GND) | `0x50`: Hỏi trạng thái | `0x51`: PRESSED (Đang nhấn)<br>`0x50`: RELEASED (Nhả) |
| **Slave 3** | Còi báo | `0x03` | PC5 (Output) | `0x40`: ON<br>`0x41`: OFF<br>`0x42`: Beep | Trả lại mã lệnh vừa thực hiện |
| **Slave 4** | Biến trở | `0x04` | PD3 (ADC Ch 4 / AIN4) | `0x60`: Hỏi giá trị | Giá trị ADC 8-bit (`0` đến `255`) |
| **Slave 5** | Siêu âm | `0x05` | PA1: Echo<br>PA2: Trig<br>PC3: LED | `0x70`: Hỏi khoảng cách (hoặc `0x40`) | Khoảng cách (`0` đến `255` cm) |
| **Slave 6** | Màn LCD 1602 | `0x06` | PB4: SCL<br>PB5: SDA | `0x01`: Xóa màn hình (hoặc `0x82`)<br>`0x10`: Hiện chữ mẫu<br>`0x80`-`0xE4`: Độ sáng 0-100% | Xác nhận lệnh thực hiện |

---

## 4. BẢNG TRA THANH GHI CHUẨN STM8S103F3 (RM0016 REGISTER MAP)

> ⚠️ **ĐÂY LÀ NGUYÊN NHÂN GÂY RA 80% LỖI TRONG DỰ ÁN CŨ DO COPY-PASTE SAI THANH GHI!**

### 4.1. GPIO Registers
| Port | ODR (Xuất) | IDR (Đọc vào) | DDR (Hướng) | CR1 (Chế độ) | CR2 (Ngắt/Tốc độ) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Port A** | `0x005000` | `0x005001` | `0x005002` | `0x005003` | `0x005004` |
| **Port B** | `0x005005` | `0x005006` | `0x005007` | `0x005008` | `0x005009` |
| **Port C** | `0x00500A` | `0x00500B` | `0x00500C` | `0x00500D` | `0x00500E` |
| **Port D** | `0x00500F` | `0x005010` | `0x005011` | `0x005012` | `0x005013` |

### 4.2. UART1 Registers (9600 bps @ 16MHz)
- `UART1_SR`  : `0x005230`
- `UART1_DR`  : `0x005231`
- `UART1_BRR1`: `0x005232` (Giá trị: `0x68`)
- `UART1_BRR2`: `0x005233` (Giá trị: `0x03`)
- `UART1_CR2` : `0x005235` (Giá trị: `0x2C` = `TEN | REN | RIEN`)
*QUY TẮC BẮT BUỘC*: **Phải ghi `UART1_BRR2 = 0x03` trước, rồi mới ghi `UART1_BRR1 = 0x68`!**

### 4.3. TIM2 Registers (Đo thời gian Microsecond trên STM8S103F3)
- `TIM2_CR1`  : `0x005300` (Bit 0: `CEN` - Counter Enable)
- `TIM2_IER`  : `0x005303`
- `TIM2_SR1`  : `0x005304`
- `TIM2_EGR`  : `0x005306` (Bit 0: `UG` - Update Generation để nạp ngay prescaler)
- `TIM2_CNTRH`: `0x00530C` (Byte cao của bộ đếm 16-bit)
- `TIM2_CNTRL`: `0x00530D` (Byte thấp của bộ đếm 16-bit)
- `TIM2_PSCR` : `0x00530E` (Prescaler: ghi `0x04` để chia 16 -> $1\mu\text{s/tick}$ @ 16MHz)
- `TIM2_ARRH` : `0x00530F` (Ghi `0xFF`)
- `TIM2_ARRL` : `0x005310` (Ghi `0xFF`)

### 4.4. ADC1 Registers (Biến trở AIN4 trên PD3)
- `ADC_CSR`: `0x005400` (`0x04` chọn kênh 4)
- `ADC_CR1`: `0x005401` (Bật nguồn `0x01` lần 1 để Wake-up, lần 2 để bắt đầu chuyển đổi)
- `ADC_CR2`: `0x005402` (`0x00`: Left-alignment -> 8 bit MSB nằm trọn vẹn trong `ADC_DRH`)
- `ADC_DRH`: `0x005404` (Đọc giá trị 8-bit trực tiếp tại đây)

---

## 5. NHỮNG LỖI SAI ĐÃ GẶP VÀ CÁCH SỬA CHI TIẾT (POST-MORTEM & FIXES)

### 🔴 Lỗi 1: Treo cổng Serial ESP32-S3 khi khởi động (Native USB CDC)
- **Hiện tượng**: Nạp code xong, Serial Monitor không in gì hoặc chỉ in: `ESP-ROM:esp32s3-20210327...` rồi tắt ngấm.
- **Nguyên nhân**: ESP32-S3 dùng chân USB gốc (D+/D-), cấu hình `USB CDC On Boot: Enabled`. Khi máy tính mở Serial Monitor, chip đã chạy qua `setup()` từ trước đó, làm mất toàn bộ log in ban đầu.
- **Cách sửa**: Bắt buộc phải thêm dòng này ngay sau `Serial.begin(115200);`:
  ```cpp
  Serial.begin(115200);
  while (!Serial && millis() < 3000); // Chờ USB CDC tối đa 3 giây
  ```

### 🔴 Lỗi 2: Cảm biến khoảng cách (Slave 5) luôn bị Timeout / Trả về 0
- **Hiện tượng**: HC-SR04 không đo được, UART luôn trả về khoảng cách 0 hoặc đơ.
- **Nguyên nhân**:
  1. `GPIOD_IDR` bị khai báo nhầm thành `0x5011` (thực chất là DDR). Do đó lệnh kiểm tra chân Echo `GPIOD_IDR & (1 << 2)` luôn bằng 0!
  2. `TIM2_CNTRH` bị khai báo là `0x530C` (thực chất là thanh ghi Prescaler `PSCR`), `TIM2_CNTRL` là `0x530D` (thực chất là `ARRH`). Bộ đếm thật ở `0x530A/0x530B` không hề được đọc!
  3. `UART1_BRR1` và `UART1_BRR2` bị tráo ngược địa chỉ làm sai lệch hoàn toàn Baudrate.
- **Cách sửa**: Sửa lại toàn bộ địa chỉ thanh ghi theo bảng chuẩn mục 4.

### 🔴 Lỗi 3: Nút bấm (Slave 2) trả về liên tục `[RX]: 0x00`
- **Hiện tượng**: ESP32 gửi lệnh `0x50` nhưng nhận lại toàn `0x00`.
- **Nguyên nhân**: Cắm nhầm dây nút bấm hoặc nhầm chân UART RX/TX. Khi chân tín hiệu bị chạm mass hoặc treo lơ lửng, UART nhận byte 0x00 liên tục.
- **Cách sửa**: Chân nút bấm STM8S103 là **PC4**, nối qua nút nhấn xuống **GND**.

### 🔴 Lỗi 4: Lỗi Dangling Pointer làm sập mạng UDP trên ESP32
- **Hiện tượng**: ESP32 bị crash reset định kỳ khi chạy WiFi UDP.
- **Nguyên nhân**: Trong `esp_wifi.ino`, dòng `const char* pc_ip = preferences.getString("pcip").c_str();` lấy con trỏ của một `String` tạm. Khi hết câu lệnh, `String` bị giải phóng, biến con trỏ thành vùng nhớ rác (Dangling Pointer).
- **Cách sửa**: Khai báo mảng tĩnh toàn cục `char pc_ip[20];` và sao chép an toàn bằng `strncpy(pc_ip, preferences.getString("pcip", "255.255.255.255").c_str(), sizeof(pc_ip));`.

### 🔴 Lỗi 5: Biến trở B10K hiển thị sai điện trở và góc xoay
- **Hiện tượng**: Công thức cũ tính theo biến trở 50k hoặc 1k.
- **Quy cách thực tế**: Biến trở là loại **B10k** (10 $k\Omega$, góc quay tuyến tính $270^\circ$).
- **Công thức chuẩn**:
  ```cpp
  float resistance_kohm = ((float)actual_pot_value / 255.0) * 10.0;
  float angle_deg       = ((float)actual_pot_value / 255.0) * 270.0;
  ```

### 🔴 Lỗi 6: Trình biên dịch Cosmic C báo lỗi cú pháp (C89 Standard)
- **Hiện tượng**: Lỗi biên dịch `syntax error` dù code C nhìn hoàn toàn bình thường.
- **Nguyên nhân**: Cosmic C tuân theo chuẩn C89 nghiêm ngặt. **Tất cả các biến phải được khai báo ở đầu khối lệnh (`{ ... }`), không được khai báo xen kẽ sau các câu lệnh thực thi.**
- **Cách sửa**: Đưa toàn bộ khai báo biến lên dòng đầu tiên của hàm hoặc block.

### 🔴 Lỗi 7: Tráo đổi định dạng file nạp .s19 và .hex của Cosmic C (chex)
- **Hiện tượng**: Mở file `.s19` (ví dụ OLED Slave 9, DC Motor Slave 7, Servo Slave 8, Soil Slave 10) trong **ST Visual Programmer (STVP)** bị báo lỗi `Invalid format / File error / Checksum mismatch`.
- **Nguyên nhân gốc rễ**: Lệnh tiện ích `chex` của bộ Cosmic C có quy ước:
  - `chex -o file.s19 file.sm8` -> Tạo file **Motorola S-Record** (bắt đầu bằng ký tự `S0...`, `S1...`).
  - `chex -fi -o file.hex file.sm8` -> Tham số `-fi` (format intel) tạo file **Intel HEX** (bắt đầu bằng dấu hai chấm `:20...`).
  Khi viết lệnh nhầm `-fi -o file.s19`, file có đuôi `.s19` nhưng bên trong lại là nội dung Intel HEX, khiến STVP không thể nhận diện được.
- **Cách sửa & Quy tắc bất biến**:
  Luôn dùng đúng 2 dòng lệnh chuẩn trong `build.bat` và STVD Post-Build:
  ```bat
  "%COSMIC_DIR%\chex.exe" -o Debug\<target>.s19 Debug\<target>.sm8
  "%COSMIC_DIR%\chex.exe" -fi -o Debug\<target>.hex Debug\<target>.sm8
  ```

### 🔴 Lỗi 8: Lỗi ký tự ẩn UTF-8 BOM (#error cpstm8 main.c:1 bad character)
- **Hiện tượng**: Biên dịch bằng `cxstm8` báo lỗi cú pháp ngay tại dòng 1 ký tự 1 dù code hoàn toàn đúng.
- **Nguyên nhân**: File mã nguồn `.c` hoặc `.h` bị lưu với định dạng **UTF-8 with BOM** (chứa 3 byte ẩn `0xEF 0xBB 0xBF` ở đầu file do PowerShell 5.1 `Set-Content -Encoding utf8`). Trình tiền xử lý `cpstm8` của Cosmic C chỉ chấp nhận UTF-8 **không có BOM (No BOM)** hoặc ANSI.
- **Cách sửa**: Luôn lưu file C bằng chuẩn UTF-8 No BOM (`[System.Text.UTF8Encoding]::new($false)`).

### 🔴 Lỗi 9: Màn hình OLED SSD1306 vẽ quá chậm (80ms/dòng) gây đè buffer ngắt
- **Hiện tượng**: Gửi chuỗi chữ xuống màn hình OLED bị khựng giật, chậm trễ hoặc nếu gửi 2 dòng liên tiếp thì dòng 2 bị mất đuôi / đứt đoạn.
- **Nguyên nhân gốc rễ**: 
  1. Hàm in chuỗi cũ gọi `I2C_Start()` và `I2C_Stop()` cho từng ký tự (16 lần cho nửa trên + 16 lần cho nửa dưới = 32 phiên truyền I2C cho 1 dòng chữ 16 ký tự). Thời gian vẽ mất tới ~80ms.
  2. Khi ESP32 phát tiếp Dòng 2 chỉ sau 2ms, STM8 vẫn đang bận vẽ Dòng 1, ngắt UART của Dòng 2 ập tới ghi đè lên bộ đệm `global_oled_buffer`.
- **Cách sửa chuẩn**:
  - Chuyển `OLED_PrintStr8x16` sang cơ chế **Burst Continuous Streaming**: Chỉ `I2C_Start()` đúng **1 lần** cho cả dòng 128 byte dữ liệu. Thời gian vẽ giảm 8 lần (từ 80ms xuống chỉ còn **~10ms**).
  - Bổ sung `delay(15)` an toàn giữa các lệnh Text 20-byte trong hàng đợi Tier 2 của ESP32.

### 🔴 Lỗi 10: Rác ký tự đuôi dòng trên màn hình OLED
- **Hiện tượng**: In một chuỗi ngắn (ví dụ "BAT DAU" - 7 ký tự) nhưng trên màn hình vẫn còn lại các ký tự thừa của chuỗi in trước đó ở các cột phía sau.
- **Nguyên nhân**: Hàm in cũ chỉ lặp đến khi gặp `\0` rồi dừng, không xóa trắng các cột pixel còn lại trên trang OLED (cột 56 đến 127).
- **Cách sửa**: Trong hàm `OLED_PrintStr8x16`, luôn lặp đủ **16 ký tự** (128 pixel). Nếu chuỗi ngắn hơn 16 ký tự, tự động đệm ký tự khoảng trắng `' '` để xóa sạch 100% rác phía sau.

### 🔴 Lỗi 11: Lỗi App 'BoardProxy' object has no attribute 'oled_clear'
- **Hiện tượng**: Chạy code Python trong App báo lỗi `AttributeError: 'BoardProxy' object has no attribute 'oled_clear'`.
- **Nguyên nhân**: Bộ hàm chuẩn trong sandbox Blockly của App từ đầu là `board.lcd_clear()` và `board.lcd_print_line()`. Khi người dùng gọi phương thức mới `board.oled_clear()` trên phiên bản App đang mở sẵn (chưa tải lại module từ đĩa) thì bị lỗi thuộc tính.
- **Cách sửa**: 
  - Khuyến nghị dùng bộ lệnh chuẩn gốc `board.lcd_clear()` và `board.lcd_print_line()` tương thích 100% mọi phiên bản.
  - Đồng thời bổ sung các bí danh `oled_clear = lcd_clear`, `oled_print_line = lcd_print_line` vào cả mã nguồn `frontend/plugins` lẫn bản đóng gói `dist/FabLab STEM`.

### 🔴 Lỗi 12: Xung đột mã lệnh LCD Clear (0x01 vs 0x82)
- **Hiện tượng**: Gửi lệnh `LCD_CLEAR` từ App nhưng màn hình LCD 1602 (Slave 6) không chịu xóa.
- **Nguyên nhân**: ESP32 gửi opcode `0x01`, nhưng firmware Slave 6 cũ chỉ bắt opcode `0x82`.
- **Cách sửa**: Firmware Slave 6 bắt đồng thời cả 2 mã `if (cmd == 0x01 || cmd == 0x82) { LCD_Clear(); }`.

---

## 6. DANH MỤC CÁC SKETCH TEST ĐỘC LẬP (ESP32 TEST SUITE)

Trong thư mục `F:\stm8_rx_tx_test\ESP_TX_RX\`:
1. `ESP_test_led_blink`: Test riêng Slave 1 (Đèn PC3).
2. `ESP_test_button`: Test riêng Slave 2 (Nút bấm PC4).
3. `ESP_test_btn_control_led`: Test kết hợp Slave 2 nhấn nút điều khiển Slave 1 bật đèn qua vòng Ring.
4. `ESP_test_potentiometer`: Test riêng Slave 4 (Biến trở B10k trên PD3).
5. `ESP_test_distance`: Test riêng Slave 5 (Khoảng cách HC-SR04 trên PA1/PA2).
6. `ESP_gateway_v2/ESP_gateway_v2.ino`: Gateway chính thức tích hợp tất cả các Slave, Dual-Frame Verification, UDP Telemetry Heartbeat 100ms, Deadband abs >= 1, Fast-boot siêu âm 100ms.
7. `ESP_gateway_v3/ESP_gateway_v3.ino`: Gateway V3 tối ưu 3-Tier Polling, hỗ trợ trọn bộ 12 Slave, bảo vệ PC IP Unicast và định tuyến targeted ID.
8. `ESP_nhat/esp_wifi/esp_wifi.ino`: Gateway gốc ban đầu (lưu trữ đối chiếu).

---

## 7. CẤU TRÚC DỰ ÁN STVD (.STW & .STP) CỦA TOÀN BỘ 15 SLAVE

Tất cả các thư mục Slave (cả bản gốc và bản mới trong thư mục gốc cũng như trong `STEM_CORE_PACKAGE/03_STM8_SOURCE_CODE_C`) đều được trang bị đầy đủ:
- **Tệp Workspace (`.stw`)**: Cho phép click đúp chuột mở trực tiếp dự án trong ST Visual Develop (STVD).
- **Tệp Project (`.stp`)**: Cấu hình Toolset Cosmic C, cờ biên dịch C89, đường dẫn Linker script `.lkf`, bản đồ vector ngắt 32 entry tại `0x8000`, và 2 bước Post-Build xuất ra cả `.s19` lẫn `.hex`.
- **Tệp `build.bat`**: Script biên dịch độc lập chỉ với 1 click chuột không cần mở IDE.
- **Thư mục `Debug/`**: Chứa sẵn file đã biên dịch (`.sm8`, `.map`, `.s19`, `.hex`).

| Module Slave | Tệp Workspace STVD (.stw) | Tệp Project STVD (.stp) | Linker Script (.lkf) |
| :--- | :--- | :--- | :--- |
| **Slave 1 (LED Basic)** | `Stm8_slave_1_den.stw` | `slave_1_den.stp` | `slave_1_den.lkf` |
| **Slave 1 (LED PWM)** | `Stm8_slave_1_den.stw` | `slave_1_den.stp` | `slave_1_den.lkf` |
| **Slave 1 (LED Smart)** | `stm8_slave_1_den_smart.stw` | `slave_1_smart.stp` | `slave_1_smart.lkf` |
| **Slave 2 (Button)** | `stm8_slave_2_button.stw` | `slave_2_button.stp` | `slave_2_button.lkf` |
| **Slave 2 (Button Smart)** | `stm8_slave_2_button_smart.stw` | `slave_2_smart.stp` | `slave_2_smart.lkf` |
| **Slave 3 (Buzzer)** | `stm8_slave_3_coi.stw` | `slave_3_coi.stp` | `slave_3_coi.lkf` |
| **Slave 4 (Potentiometer)** | `stm8_slave_4_bien_tro.stw` | `slave_4_bien_tro.stp` | `slave_4_bien_tro.lkf` |
| **Slave 5 (Distance HC-SR04)** | `stm8_slave_5_khoang_cach.stw`| `stm8_slave_khoang_cach.stp` | `stm8_slave_khoang_cach.lkf` |
| **Slave 6 (LCD 1602)** | `stm8_slave_6_lcd.stw` | `stm8_slave_khoang_cach.stp` | `stm8_slave_khoang_cach.lkf` |
| **Slave 7 (DC Motor)** | `stm8_slave_7_dong_co_dc.stw` | `slave_7_dc.stp` | `slave_7_dc.lkf` |
| **Slave 8 (Servo SG90)** | `stm8_slave_8_servo.stw` | `slave_8_servo.stp` | `slave_8_servo.lkf` |
| **Slave 9 (OLED SSD1306)** | `stm8_slave_9_oled.stw` | `slave_9_oled.stp` | `slave_9_oled.lkf` |
| **Slave 10 (Soil Moisture)**| `stm8_slave_10_do_am_dat.stw`| `slave_10_soil.stp` | `slave_10_soil.lkf` |
| **Slave 11 (DHT11)** | `stm8_slave_11_dht11.stw` | `slave_11_dht11.stp` | `slave_11_dht11.lkf` |
| **Slave 12 (DHT22)** | `stm8_slave_12_dht22.stw` | `slave_12_dht22.stp` | `slave_12_dht22.lkf` |

---

## 8. QUY TRÌNH & NGUYÊN TẮC VIẾT CODE C CHUẨN MỰC CHO STM8 (COSMIC C)

### 8.1. Tuân thủ chuẩn C89 nghiêm ngặt
1. **Khai báo biến ở đầu hàm / block:** Không khai báo biến sau câu lệnh thực thi.
2. **Không dùng `//` comment nếu trình biên dịch chưa bật cờ C99:** Mặc dù `cxstm8 -pp` hỗ trợ `//`, tốt nhất nên dùng `/* ... */` cho các chú thích đa dòng hoặc đầu hàm.
3. **Tuyệt đối không cấp phát động (`malloc / free`):** Chip STM8S103F3 chỉ có 1KB RAM (`0x0000 - 0x03FF`). Dùng mảng tĩnh (`static` hoặc toàn cục) với kích thước cố định.

### 8.2. Mẫu ngắt phần cứng Hardware Repeater chuẩn
Mọi slave trên vòng Ring bắt buộc phải có đoạn lặp phần cứng ở đầu ISR trước khi xử lý giao thức:
```c
@far @interrupt void UART1_RX_IRQHandler(void) {
    unsigned char c = UART1_DR; // Đọc byte để xóa cờ ngắt RXNE

    /* 1. Lặp tức thì sang TX cho node kế tiếp */
    while (!(UART1_SR & 0x80)); // Chờ bộ đệm TXE sẵn sàng
    UART1_DR = c;               // Đẩy byte ra vòng Ring

    /* 2. Parser máy trạng thái (State Machine) */
    ...
}
```

### 8.3. Bảng vector ngắt chuẩn 32 Entry
Bảng vector ngắt bắt buộc đặt tại `0x8000` với đúng 32 phần tử:
- Phần tử 0: `_stext` (Reset vector tại `0x8000`)
- Phần tử 1: `NonHandledInterrupt` (TRAP tại `0x8004`)
- Phần tử 2 đến 19 (18 phần tử): `IRQ0` đến `IRQ17`
- Phần tử 20: `UART1_RX_IRQHandler` (Chính xác tại `0x8050` - `IRQ18`)
- Phần tử 21 đến 31 (11 phần tử): `IRQ19` đến `IRQ29`

### 8.4. Cấu hình thanh ghi & Nhịp xung 16MHz
```c
CLK_CKDIVR = 0x00; // Bộ chia xung HSI = 1 -> Chạy tối đa 16MHz
UART1_BRR2 = 0x03; // Ghi BRR2 TRƯỚC
UART1_BRR1 = 0x68; // Ghi BRR1 SAU -> 9600 baud @ 16MHz
UART1_CR2  = 0x2C; // Bật TEN, REN và ngắt RIEN
_asm("rim");       // Cho phép ngắt toàn cục
```

---

> 📖 **XEM TÀI LIỆU CHI TIẾT ĐẦY ĐỦ NHẤT TẠI:**
> - [TAI_LIEU_BAN_GIAO_AGENT.md](file:///F:/stm8_rx_tx_test/TAI_LIEU_BAN_GIAO_AGENT.md): Tài liệu bàn giao kỹ thuật tổng thể cho Agent & Kỹ sư.
> - [GHI_CHU_LOI_VA_LUU_Y.md](file:///F:/stm8_rx_tx_test/GHI_CHU_LOI_VA_LUU_Y.md): Sổ tay 22 lỗi kỹ thuật & giải pháp chi tiết.

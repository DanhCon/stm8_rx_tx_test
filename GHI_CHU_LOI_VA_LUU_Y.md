# SỔ TAY KỸ THUẬT & GHI CHÚ CÁC LỖI ĐÃ GẶP (TROUBLESHOOTING LOG)
> **Dự án:** STM8 UART Ring + ESP32 IoT Gateway + FabLab STEM Platform  
> **Cập nhật lần cuối:** 09/10/2026  
> **Mục đích:** Đọc kỹ tài liệu này trước khi chỉnh sửa mã nguồn hoặc môi trường để tránh lặp lại các lỗi tương tự.

---

## MỤC LỤC
1. [Lỗi Môi trường & Hệ thống Windows (Python, PowerShell, Docker, Disk)](#1-lỗi-môi-trường--hệ-thống-windows)
2. [Lỗi Backend & Cơ sở dữ liệu (FastAPI, SQLite, PostgreSQL)](#2-lỗi-backend--cơ-sở-dữ-liệu)
3. [Lỗi Firmware Vi điều khiển (ESP32 & STM8 Ring)](#3-lỗi-firmware-vi-điều-khiển)
4. [Quy tắc vàng khi chỉnh sửa dự án sau này](#4-quy-tắc-vàng-khi-chỉnh-sửa-dự-án)

---

## 1. LỖI MÔI TRƯỜNG & HỆ THỐNG WINDOWS

### ❌ Lỗi 1.1: Script PowerShell (.ps1) báo lỗi cú pháp "Unexpected token"
* **Hiện tượng:**
  ```text
  Unexpected token '}' in expression or statement.
  Unexpected token 'else' in expression or statement.
  The string is missing the terminator: ".
  ```
* **Nguyên nhân gốc rễ:**
  File `.ps1` chứa các ký tự đồ họa / Unicode đặc biệt (`═`, `✓`, `✗`, `▶`) và được lưu ở chuẩn UTF-8 không có BOM (Byte Order Mark). Windows PowerShell 5.1 mặc định đọc file theo bảng mã ANSI/Windows-1252, làm các ký tự nhiều byte bị vỡ thành `â•â•`, phá vỡ dấu ngoặc kép và khối ngoặc `{ }`.
* **Cách phòng tránh & Khắc phục:**
  * **Tuyệt đối không dùng ký tự Unicode vẽ khung hoặc icon đồ họa trong script `.ps1`.**
  * Luôn dùng ký tự ASCII chuẩn: `===`, `[OK]`, `[X]`, `[!]`, `[>]`.

---

### ❌ Lỗi 1.2: Pip báo lỗi UnicodeDecodeError khi đọc `requirements.txt`
* **Hiện tượng:**
  ```text
  UnicodeDecodeError: 'charmap' codec can't decode byte 0x8d in position 450: character maps to <undefined>
  ```
* **Nguyên nhân gốc rễ:**
  File `requirements.txt` có chứa ghi chú tiếng Việt có dấu hoặc ký tự gạch nối dài Unicode (`—`). Trên Windows, `pip` mở file bằng bảng mã `cp1252` nên bị lỗi ngay khi gặp ký tự có dấu.
* **Cách phòng tránh & Khắc phục:**
  * Trong các file `requirements.txt`, **chỉ dùng tiếng Anh thuần ASCII không dấu** cho các dòng comment hoặc tốt nhất là chỉ để tên thư viện.

---

### ❌ Lỗi 1.3: Hết dung lượng ổ đĩa "No space left on device"
* **Hiện tượng:**
  ```text
  OSError: [Errno 28] No space left on device
  ```
* **Nguyên nhân gốc rễ:**
  Ổ `F:\` là phân vùng cố định kích thước 22 GB. Khi giải nén file `stem-platform.zip` (3.45 GB) và tải `Docker Desktop Installer.exe` (635 MB), dung lượng ổ `F:\` rơi vào trạng thái còn **480 KB**, khiến lệnh `pip install` các thư viện lớn (`PySide6`, `Torch`) bị dừng đột ngột.
* **Cách phòng tránh & Khắc phục:**
  * Sau khi giải nén xong các file `.zip` hoặc cài xong các file `.exe` cài đặt lớn trên ổ `F:\`, **hãy xóa ngay file nén gốc** để giải phóng dung lượng.
  * Nếu cài thêm các mô hình AI nặng, cân nhắc chuyển thư mục sang ổ `D:\` (còn trống gần 100 GB).

---

### ❌ Lỗi 1.4: Không tìm thấy lệnh `.\.venv\Scripts\python`
* **Hiện tượng:**
  ```text
  The term '.\.venv\Scripts\python' is not recognized as the name of a cmdlet...
  ```
* **Nguyên nhân gốc rễ:**
  Khi mở PowerShell bằng quyền Administrator, thư mục làm việc mặc định bị nhảy về `C:\WINDOWS\system32`. Ở đây không có thư mục `.venv` của dự án.
* **Cách phòng tránh & Khắc phục:**
  * Luôn kiểm tra đường dẫn terminal trước khi gõ lệnh. Phải `cd F:\stm8_rx_tx_test\...` về đúng thư mục dự án trước khi chạy code.

---

### ❌ Lỗi 1.5: Docker Desktop bị treo "WSL needs updating"
* **Hiện tượng:**
  Docker Desktop báo lỗi nhân WSL2 quá cũ, không khởi động được container.
* **Cách phòng tránh & Khắc phục:**
  * Cài đặt gói cập nhật nhân Linux chính thức từ Microsoft: `wsl_update_x64.msi`.
  * Hoặc sử dụng giải pháp chạy Backend trực tiếp bằng Python + SQLite (không cần Docker).

---

## 2. LỖI BACKEND & CƠ SỞ DỮ LIỆU

### ❌ Lỗi 2.1: App báo lỗi "Connection to localhost:8000 timed out" khi đăng nhập
* **Hiện tượng:**
  ```text
  HTTPConnectionPool(host='localhost', port=8000): Max retries exceeded... Connection to localhost timed out.
  ```
* **Nguyên nhân gốc rễ:**
  Server Backend FastAPI chưa được bật, hoặc Docker chưa chạy được cơ sở dữ liệu PostgreSQL.
* **Giải pháp đã thực hiện:**
  * Chuyển cấu hình `DATABASE_URL` trong `backend/.env` sang **SQLite** cục bộ:
    ```env
    DATABASE_URL=sqlite:///./stem.db
    ```
  * Sửa `backend/app/core/database.py` để tương thích cả SQLite (`connect_args={"check_same_thread": False}`) lẫn PostgreSQL.
  * Map kiểu dữ liệu `JSONB` từ PostgreSQL sang `JSON` đa cơ sở dữ liệu trong SQLAlchemy.
  * Cài đặt bổ sung thư viện `email-validator` (bắt buộc cho model User của Pydantic).

---

## 3. LỖI FIRMWARE VI ĐIỀU KHIỂN (ESP32 & STM8)

### ❌ Lỗi 3.1: Nhầm biến đếm Timeout giữa các Node (Copy-Paste Bug)
* **Hiện tượng:**
  Khi Node 1 (Đèn LED) mất kết nối UART, biến đếm của Node 2 (Nút bấm) lại bị tăng, khiến nút bấm bị báo mất kết nối oan.
* **Đoạn code bị lỗi trước đây:**
  ```cpp
  // Trong khối xử lý Node 1 (LED):
  } else {
      if (++timeout_btn > 3) { // <-- SAI: dùng nhầm timeout_btn
          slave_led_online = 0;
      }
  }
  ```
* **Cách sửa chuẩn:**
  ```cpp
  } else {
      if (++timeout_led > 3) { // <-- ĐÚNG: dùng timeout_led
          slave_led_online = 0;
      }
  }
  ```

---

### ❌ Lỗi 3.2: Lệch cú pháp lệnh điều khiển UDP giữa App và Firmware
* **Hiện tượng:**
  Trên App và script test `udp_commander.py` gửi lệnh `"ON"` / `"OFF"`, nhưng trong firmware ESP32 chỉ kiểm tra `cmd.equals("LED_ON")`, dẫn đến ESP32 nhận được lệnh nhưng đèn không sáng.
* **Cách sửa chuẩn:**
  Luôn hỗ trợ cả hai cú pháp song song:
  ```cpp
  if (cmd.equals("LED_ON") || cmd.equals("ON"))          target_led_state = 1;
  else if (cmd.equals("LED_OFF") || cmd.equals("OFF"))   target_led_state = 0;
  else if (cmd.equals("LED_BLINK") || cmd.equals("BLINK")) target_led_state = 2;
  ```

---

### ❌ Lỗi 3.3: Treo vòng UART Ring khi ESP32 ở chế độ phát WiFi AP
* **Hiện tượng:**
  Khi chưa có WiFi, ESP32 rơi vào vòng lặp vô tận `while(true) { server.handleClient(); }`, làm toàn bộ mạng UART STM8 ngừng hoạt động.
* **Cách sửa chuẩn:**
  Không dùng vòng lặp chặn (`blocking while`). Đặt cờ `is_ap_mode = true` và đưa `server.handleClient()` vào hàm `loop()` để mạng UART vẫn quét bình thường.

---

### ❌ Lỗi 3.4: Bị delay vòng lặp khi chưa cắm cảm biến khoảng cách HC-SR04 (Node 5)
* **Hiện tượng:**
  Cảm biến siêu âm cần 35ms để chờ sóng dội về. Nếu hệ thống chỉ cắm 4 bo STM8 (chưa cắm bo thứ 5), vòng lặp chính sẽ bị khựng 35ms ở mỗi chu kỳ.
* **Cách sửa chuẩn (Điều phối thông minh):**
  ```cpp
  static uint32_t last_dist_poll = 0;
  // Khi Node 5 đang Online: quét liên tục
  // Khi Node 5 đang Offline: chỉ thử thăm dò 500ms/lần để không làm chậm 4 node còn lại
  if (slave_dist_online || (millis() - last_dist_poll >= 500)) {
      // Thực hiện truy vấn Node 5...
  }
  ```

---

### ❌ Lỗi 3.5: Rủi ro ô nhớ rác ở bộ đệm `rx_buf` của UDP
* **Hiện tượng:**
  Mảng `char rx_buf[128]` không được gán `= {0}` ban đầu. Nếu `len <= 0`, việc gọi `String(rx_buf)` sẽ đọc các ô nhớ rác ngẫu nhiên trên RAM.
* **Cách sửa chuẩn:**
  ```cpp
  char rx_buf[128] = {0};
  int len = udp.read(rx_buf, sizeof(rx_buf) - 1);
  if (len <= 0) return;
  rx_buf[len] = '\0';
  ```

### ❌ Lỗi 3.6: Heartbeat 2.5s quá sát ngưỡng ngắt kết nối (3.0s) của App gây chập chờn
* **Hiện tượng:**
  Trên App thấy chấm kết nối nhảy liên tục giữa `Hardware` (xanh lá) và `Simulation` (xám), trong log ghi `ESP32 DISCONNECTED (timeout)` rồi lại `ESP32 CONNECTED`.
* **Nguyên nhân:**
  Trong file Python `neuron_gateway_service.py` đặt thời gian timeout ngắt kết nối là `DISCONNECT_TIMEOUT = 3.0` (giây). Trong khi code ESP đặt nhịp gửi định kỳ `HEARTBEAT_INTERVAL = 2500` (ms). Khi mạng WiFi có độ trễ hoặc vòng lặp UART trễ 500ms, tổng thời gian vượt quá 3s khiến App tưởng ESP32 bị mất nguồn.
### ❌ Lỗi 3.7: App chặn gửi lệnh điều khiển (send_led, send_buzzer) khi node chưa báo online
* **Hiện tượng:**
  Chạy khối "Single LED ON" trên App, App báo "Hoàn thành" nhưng đèn LED thật không sáng.
* **Nguyên nhân:**
  Trong file `frontend/firmware/neuron_gateway_service.py`, hàm `send_led()` và `send_buzzer()` có đoạn mã `if not self.node_online("led_node"): return`. Khi chu kỳ UART trên ESP32 chưa kịp nhận phản hồi hoặc vòng ring chưa khép kín, cờ `node_online` bằng `False`, dẫn đến App **âm thầm nuốt lệnh** và không gửi bất kỳ gói tin UDP nào đến ESP32.
### ❌ Lỗi 3.8: Lỗi `break` sớm khi nhận gói Echo làm nuốt mất gói tin phản hồi thật của cảm biến (Nút nhấn & Biến trở)
* **Hiện tượng:**
  Chạy code test nút bấm riêng (`ESP_test_btn_control_led`) thì nút điều khiển đèn sáng tắt mượt mà, nhưng chạy firmware Gateway (`ESP_gateway_v2`) kết nối App thì App không nhận được tín hiệu bấm nút.
* **Nguyên nhân gốc rễ (Bẫy kiến trúc vòng Ring UART):**
  Trong mạng vòng Ring khép kín:
  1. Khi ESP32 phát gói truy vấn: `[0xAA, 0x02, 0x50, 0x52]` (`0x50` = `CMD_BTN_READ`).
  2. Gói tin này chạy qua các repeater và vòng ngược về chân RX của ESP32 (gọi là gói Echo).
  3. Mã truy vấn `CMD_BTN_READ` (`0x50`) lại **trùng đúng với mã nút nhả** `RES_BTN_RELEASED` (`0x50`)!
  4. ESP32 nhận gói Echo (`0x50`) trước, tưởng là nút nhả -> gán `actual_btn_state = 0` -> gặp lệnh `if (valid_btn_frame) break;` và **thoát ngay khỏi vòng chờ (chỉ sau ~4ms)**.
  5. Trong khi đó, bo STM8 Nút Nhấn cần vài mili-giây sau mới gửi gói phản hồi thật: `[0xAA, 0x02, 0x51, 0x53]` (`0x51` = Đang nhấn).
  6. Gói `0x51` thật về đến ESP32 lúc ESP32 đã thoát sang node tiếp theo và bị lệnh `clear_rx_buffer()` xóa sạch khỏi RAM.
  *(Biến trở Node 4 cũng bị tương tự vì gói Echo mang mã `0x60`, làm biến trở luôn đọc ra `96` thay vì giá trị ADC xoay thật).*
* **Cách sửa chuẩn:**
  **Tuyệt đối không dùng lệnh `break;` sớm** trong cửa sổ đọc phản hồi UART 15ms-18ms của các cảm biến. Phải giữ vòng lặp đọc hết cửa sổ thời gian để gói phản hồi thật (Frame 2) ghi đè lên gói Echo truy vấn (Frame 1).

---

### ❌ Lỗi 3.9: Độ trễ cảm biến (Biến trở / Nút bấm) do ngưỡng Deadband và nhịp Heartbeat UDP quá dài (1s)
* **Hiện tượng:**
  Xoay biến trở hoặc thao tác cảm biến nhưng giao diện App và code Python phản hồi chậm, có cảm giác bị giật hoặc trễ gần 1 giây mới ăn lệnh.
* **Nguyên nhân:**
  1. Trong firmware ESP32 đặt điều kiện thay đổi giá trị: `if (abs(actual_pot_value - last_pot_value) > 3)`. Khi xoay biến trở từ từ, mức thay đổi nhỏ hơn hoặc bằng 3 ADC counts nên không kích hoạt cờ `state_changed`.
  2. Khi không có cờ `state_changed`, ESP32 phải chờ hết `HEARTBEAT_INTERVAL = 1000` (1 giây) mới gửi gói tin UDP định kỳ lên PC.
  3. Trong code Python đặt `time.sleep(0.1)` (100ms) tạo thêm độ trễ vòng lặp.
* **Cách sửa chuẩn:**
  1. Giảm ngưỡng nhạy cảm biến trên ESP32 xuống: `if (abs(actual_pot_value - last_pot_value) >= 1)`.
  2. Rút ngắn nhịp phát sóng định kỳ `HEARTBEAT_INTERVAL` xuống **100ms** (10 gói/giây) để luồng dữ liệu truyền liên tục và mượt mà.
  3. Trong code Python giảm `time.sleep` xuống **0.02s - 0.03s** (20ms - 30ms, tương đương 30-50 FPS) để phản ứng tức thì.

---

### ❌ Lỗi 3.10: ESP32 nhận diện sai trạng thái Online (Báo ảo cả 5 Node đều Online khi chỉ cắm 1-2 Node)
* **Hiện tượng:**
  Trong mạng vòng Ring khép kín (ví dụ chỉ cắm Đèn và Nút rồi nối về ESP32), trên App hoặc Telemetry vẫn báo Còi, Biến trở, Khoảng cách đều đang `Online = 1`.
* **Nguyên nhân:**
  Trong mạng Daisy-chain Ring, mọi module STM8 đều có ngắt phần cứng tự động chuyển tiếp (Hardware Repeater) byte nhận được sang TX. Khi ESP32 phát gói truy vấn thăm dò Node vắng mặt, gói truy vấn này được các module khác chuyển tiếp một vòng quay về chân RX của ESP32 (Gói Echo). Vì Gói Echo có đủ Header `0xAA`, Node ID và Checksum hợp lệ, bộ parser cũ của ESP32 tưởng Node đó vừa phản hồi nên đánh dấu Online.
* **Cách sửa chuẩn (Thuật toán Dual-Frame Verification):**
  Phân biệt giữa Node vắng mặt và Node có mặt thật:
  - **Node vắng mặt:** Chỉ nhận được đúng **1 frame** (chính là Gói Echo do các node khác lặp lại về).
  - **Node có mặt thật:** Nhận được **ít nhất 2 frame**: Gói 1 (Echo từ repeater) + Gói 2 (ACK hoặc dữ liệu cảm biến thực tế do CPU của node đó tự tạo ra và phát lên mạng).
  - ESP32 đếm số frame nhận về trong cửa sổ 20ms: Chỉ khi `frames_received >= 2` (hoặc nhận được dữ liệu cảm biến đặc thù từ CPU) mới xác nhận Node đó **ONLINE**. Nếu chỉ nhận được 1 frame echo -> Xác định chính xác Node đó **OFFLINE**.

---

### ❌ Lỗi 3.11: Lệch địa chỉ thanh ghi `TIM2` trên STM8S103F3P6 làm cảm biến khoảng cách HC-SR04 luôn trả về `0 cm`
* **Hiện tượng:**
  Nạp code cho STM8 Khoảng cách và chạy `ESP_test_distance.ino`, trên Serial Monitor chỉ hiện dòng tiêu đề `---` rồi đứng im không in ra khoảng cách nào.
* **Nguyên nhân gốc rễ:**
  1. Trên dòng chip **STM8S103F3P6** (theo file chuẩn `iostm8s103.h` của Cosmic C), vùng nhớ của `TIM2` có 2 byte Reserved ở `0x5301` và `0x5302`, làm các thanh ghi từ `TIM2_IER` trở đi dịch xuống 2 byte so với dòng STM8S207/208:
     - `TIM2_CR1`   = `0x5300`
     - `TIM2_EGR`   = `0x5306` *(không phải `0x5304`)*
     - `TIM2_CNTRH` = `0x530C` *(không phải `0x530A`)*
     - `TIM2_CNTRL` = `0x530D` *(không phải `0x530B`)*
     - `TIM2_PSCR`  = `0x530E` *(không phải `0x530C`)*
     - `TIM2_ARRH`  = `0x530F` *(không phải `0x530D`)*
     - `TIM2_ARRL`  = `0x5310` *(không phải `0x530E`)*
  2. Khi đọc nhầm `0x530A`/`0x530B` (`TIM2_CCER1`/`CCER2`), giá trị bộ đếm luôn bằng `0` -> `echo_time = 0` -> hàm `HCSR04_ReadDistance()` luôn trả về `0`.
  3. Trên `ESP_test_distance.ino`, câu lệnh `if (current_dist_cm > 0 && current_dist_cm < 255)` bỏ qua giá trị `0` và không in thông báo khi mất kết nối (`frames_received == 0`), làm màn hình Serial đứng im.
* **Cách sửa chuẩn:**
  - Sửa lại đúng bảng địa chỉ `TIM2` theo `iostm8s103.h` trong [`stm8_slave_5_khoang_cach/main.c`](file:///F:/stm8_rx_tx_test/stm8_slave_5_khoang_cach/main.c) và biên dịch lại file `.s19`.
  - Bổ sung log chẩn đoán đầy đủ các trường hợp (`frames_received == 0`, `1`, `255`, `0`) trong [`ESP_test_distance.ino`](file:///F:/stm8_rx_tx_test/ESP_TX_RX/ESP_test_distance/ESP_test_distance.ino).

---

### ❌ Lỗi 3.12: Thiếu 1 phần tử trong mảng `_vectab[]` làm lệch địa chỉ ngắt `UART1 RX` (`IRQ18`) khiến STM8 không lặp lại (Repeater) byte nào
* **Hiện tượng:**
  Đo dây vật lý giữa ESP32 và STM8 hoàn toàn thông mạch (`TX GPIO4 -> PD6`, `PD5 -> RX GPIO5`), nhưng ESP32 báo `❌ [MẤT KẾT NỐI UART] Không nhận được byte nào!`.
* **Nguyên nhân gốc rễ:**
  Trong bảng vector ngắt `_vectab[]` của [`stm8_slave_5_khoang_cach/stm8_interrupt_vector.c`](file:///F:/stm8_rx_tx_test/stm8_slave_5_khoang_cach/stm8_interrupt_vector.c) (và `stm8_slave_6_lcd`), trước dòng `{0x82, UART1_RX_IRQHandler}` chỉ có **18** dòng `NonHandledInterrupt` thay vì **19** dòng (`TRAP` + `IRQ0..IRQ17`).
  - Hệ quả: Hàm `UART1_RX_IRQHandler` bị đặt nhầm vào ô số 19 (`0x804C` = `IRQ17 - UART1 TX`), còn ô số 20 (`0x8050` = `IRQ18 - UART1 RX`) lại trỏ vào `NonHandledInterrupt`.
  - Ngay khi ESP32 gửi byte đầu tiên `0xAA` sang, ngắt `IRQ18` kích hoạt nhưng nhảy vào `NonHandledInterrupt` (không đọc `UART1_DR` để xóa cờ ngắt và không đẩy byte sang `TX`), làm treo CPU STM8 trong vòng lặp ngắt vô tận và không có bất kỳ byte nào quay về ESP32.
* **Cách sửa chuẩn:**
  Đảm bảo mảng `_vectab[]` có đúng **20 phần tử đứng trước `UART1_RX_IRQHandler`** (gồm 1 `Reset` + 1 `TRAP` + 18 `IRQ0..IRQ17`), để `UART1_RX_IRQHandler` nằm chính xác tại địa chỉ `0x8050` (`IRQ18`).

---

### ❌ Lỗi 3.13: Cảm biến khoảng cách siêu âm phản hồi chậm khi mới bật App và trễ nhịp cử động tay
* **Hiện tượng:**
  Khi mở App lên, mất hơn 1 giây cảm biến siêu âm mới hiện số. Khi đưa tay lại gần từ từ, cảm giác số liệu nhảy bị khựng hoặc trễ nhịp so với Biến trở.
* **Nguyên nhân gốc rễ:**
  1. Khi chưa xác nhận Online, ESP32 chỉ thăm dò mỗi 500ms một lần (`poll_interval = 500ms`) để tránh nghẽn các node khác khi rút cảm biến ra.
  2. Ngưỡng lọc cử động tay đặt `abs(actual_dist_cm - last_dist_cm) > 2`: Khi nhích tay 1cm, ESP32 không bật cờ `state_changed`, bắt App phải đợi hết nhịp Heartbeat 100ms.
  3. Cửa sổ đọc UART 40ms sát ngưỡng sóng dội tối đa 35ms khi không có vật cản trước mặt.
* **Cách khắc phục chuẩn:**
  1. Thăm dò thông minh thích ứng: Trong 5 giây đầu khởi động, thăm dò siêu nhanh `100ms/lần` để bắt nhịp ngay tức thì khi vừa bật App. Sau 5 giây nếu không cắm cảm biến thì mới lùi về `500ms`.
  2. Hạ ngưỡng nhạy thành `abs >= 1`: Nhích nhẹ 1cm là ESP32 lập tức phát gói tin UDP lên App ngay trong mili-giây.
  3. Mở rộng cửa sổ chờ lên `45ms` để hứng trọn gói tin phản hồi khi vật cản ở xa mà không bị mất gói.

---

### ❌ Lỗi 3.14: Phần mềm STVP báo lỗi "Verify failed at address 0x8000" khi nạp file .s19
* **Hiện tượng:**
  Bấm `Program -> Current tab` (Ctrl+P) trên phần mềm ST Visual Programmer (STVP) thì hiện hộp thoại cảnh báo vàng: `Verify failed at address 0x8000`.
* **Nguyên nhân gốc rễ:**
  1. Chip STM8 đang bị khóa bảo vệ đọc/ghi (`ROP - Read Out Protection = ON`) ở tab `OPTION BYTE`.
  2. Bấm nạp khi đang đứng ở tab `OPTION BYTE` thay vì tab `PROGRAM MEMORY`.
  3. Đang cắm cảm biến HC-SR04 hút dòng lớn từ mạch nạp ST-Link V2 làm sụt áp nguồn `3.3V` hoặc chân `NRST` bị lỏng tiếp xúc.
* **Cách khắc phục chuẩn:**
  1. Rút tạm chân nguồn của cảm biến HC-SR04 ra khi nạp code bằng ST-Link V2.
  2. Ở STVP: Chuyển sang tab `OPTION BYTE`, nhìn dòng đầu tiên `ROP (Read Out Protection)`: nếu đang là `ON` thì đổi thành `OFF`, nhấn `Ctrl+P`. (Nếu đang là `OFF` mà vẫn lỗi: đổi sang `ON` -> `Ctrl+P` để xóa chip -> đổi ngược lại `OFF` -> `Ctrl+P`).
  3. Chuyển lại tab `PROGRAM MEMORY`, chọn `File -> Open...` chọn file `.s19` và bấm `Ctrl+P`.

---

### ❌ Lỗi 3.15: Trình biên dịch Cosmic C báo lỗi "#error cpstm8 main.c:1 bad character"
* **Hiện tượng:**
  Khi biên dịch mã nguồn C của STM8 bằng `cxstm8.exe`, dòng 1 luôn báo lỗi `bad character` dù code nhìn hoàn toàn bình thường.
* **Nguyên nhân gốc rễ:**
  File `.c` bị lưu ở định dạng **UTF-8 with BOM** (dính 3 byte ẩn `0xEF 0xBB 0xBF` ở đầu file do PowerShell 5.1 `Set-Content -Encoding utf8` hoặc một số trình soạn thảo tự thêm vào). Trình tiền xử lý `cpstm8.exe` của Cosmic C chỉ hiểu mã nguồn ANSI thuần hoặc UTF-8 **không có BOM (No BOM)**.
* **Cách khắc phục chuẩn:**
  - Luôn lưu file `.c` bằng chuẩn UTF-8 No BOM (`[System.Text.UTF8Encoding]::new($false)` trong PowerShell hoặc dùng `replace_file_content` của IDE).

---

### ❌ Lỗi 3.16: App báo lỗi "HTTPConnectionPool(host='localhost', port=8000): Max retries exceeded" khi bấm Đăng nhập
* **Hiện tượng:**
  Mở giao diện App, nhập tài khoản bấm "Đăng nhập" thì hiện dòng chữ đỏ báo timeout kết nối đến `localhost:8000`.
* **Nguyên nhân gốc rễ:**
  Server Backend FastAPI (chạy trên cổng 8000) chưa được khởi động trước khi đăng nhập.
* **Cách khắc phục chuẩn:**
  - Chạy file `MO_APP_STEM.bat` (script này tự kiểm tra cổng 8000, nếu chưa có Backend thì tự bật Backend trước rồi mới mở App).
  - Hoặc bấm đúp vào shortcut `Bat_Backend_STEM` ngoài Desktop để bật riêng cửa sổ Backend.

---

## 4. QUY TẮC VÀNG KHI CHỈNH SỬA DỰ ÁN

1. **Về Firmware STM8 & ESP32:**
   * Giao thức 4-Byte chuẩn: `[0xAA, SLAVE_ID, DATA/CMD, CHECKSUM]`.
   * Công thức Checksum: `(SLAVE_ID + DATA) % 256`.
   * Sau khi gửi mỗi slot UART từ ESP32, **luôn giữ `delay(2)`** để các repeater STM8 kịp xả thanh ghi đệm.
2. **Về App Frontend & Backend:**
   * Backend dùng SQLite: luôn chạy `powershell -ExecutionPolicy Bypass -File run_backend.ps1`.
   * Frontend: chạy `powershell -ExecutionPolicy Bypass -File run-frontend.ps1`.
   * Tài khoản test đã tạo sẵn:
     * Student: `danh.huynh@gmail.com` / `123456`
     * Admin: `admin@fablab.vn` / `admin123`

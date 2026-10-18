/* =========================================================
   STM8S103F3P6 - SLAVE 2: MODULE NÚT NHẤN THÔNG MINH (SMART GESTURES)
   =========================================================
   - Chân kết nối phần cứng:
     + BUTTON (Input Pull-up)     : PC4 (Chân số 14 - Nối nút xuống GND)
     + LED Phản hồi cử chỉ        : PC3 (Chân số 13 - Output Push-Pull)
     + UART1 TX                   : PD5 (Chân số 2)
     + UART1 RX                   : PD6 (Chân số 3)
     + Nguồn                      : VCC (3.3V hoặc 5V) và GND
   - Tính năng thông minh (Tự hành 100% bằng STM8 Edge Logic):
     + Lọc rung phím số học (Hardware Debounce) 20ms
     + Nhận diện Nhấn đơn (Single Click)      : Mã 0x51
     + Nhận diện Nhấn đúp (Double Click)      : Mã 0x52 (2 lần nhấn trong 300ms)
     + Nhận diện Nhấn giữ (Long Press)        : Mã 0x53 (Giữ nút > 1.5 giây)
     + Bộ đếm tổng số lần nhấn tích lũy       : Mã 0x54
     + Trạng thái nhả / Chờ                   : Mã 0x50
     + Chốt sự kiện thông minh (Event Latch)  : Không bị rơi rớt cử chỉ dù Master quét chậm
     + Phản hồi thị giác qua LED PC3:
       * Nhấn đơn : Nháy 1 lần (60ms)
       * Nhấn đúp : Nháy 2 lần nhanh (40ms On - 40ms Off - 40ms On)
       * Nhấn giữ : Sáng đèn 300ms ngay khi vừa đủ 1.5 giây
   - Tốc độ UART: 9600 bps, 8N1
   - Giao thức   : Khung truyền 4 Byte [0xAA, ID, CMD, CRC]
   ========================================================= */

#define CLK_CKDIVR    *(volatile unsigned char*)(0x0050C6)

/* =========================================================
   GPIOC (PC3 = LED Phản hồi, PC4 = BUTTON)
========================================================= */
#define GPIOC_ODR     *(volatile unsigned char*)(0x00500A)
#define GPIOC_IDR     *(volatile unsigned char*)(0x00500B)
#define GPIOC_DDR     *(volatile unsigned char*)(0x00500C)
#define GPIOC_CR1     *(volatile unsigned char*)(0x00500D)
#define GPIOC_CR2     *(volatile unsigned char*)(0x00500E)

/* =========================================================
   UART1 THANH GHI (9600 BAUD @ 16MHz)
========================================================= */
#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)
#define UART1_BRR1    *(volatile unsigned char*)(0x005232)
#define UART1_BRR2    *(volatile unsigned char*)(0x005233)
#define UART1_CR2     *(volatile unsigned char*)(0x005235)

#define SLAVE_ID      0x02

/* Mã cử chỉ thông minh */
#define GESTURE_IDLE          0x50
#define GESTURE_SINGLE_CLICK  0x51
#define GESTURE_DOUBLE_CLICK  0x52
#define GESTURE_LONG_PRESS    0x53
#define CMD_GET_COUNTER       0x54
#define CMD_RESET_COUNTER     0x55
#define GESTURE_RAW_PRESSED   0x56

/* Biến nhận diện lệnh từ ngắt UART */
extern volatile unsigned char global_btn_cmd;
extern volatile unsigned char global_btn_ready;

/* Chốt sự kiện cử chỉ gần nhất để Master đọc */
static volatile unsigned char latched_gesture = 0;
static volatile unsigned char click_counter = 0;

void delay_ms(unsigned int ms) {
    volatile unsigned int i, j;
    for(i = 0; i < ms; i++) {
        for(j = 0; j < 3000; j++); 
    }
}

/* Hàm truyền 1 Byte UART */
void UART_SendByte(unsigned char data) {
    while(!(UART1_SR & 0x80)); 
    UART1_DR = data;
}

void main(void)
{
    /* Trạng thái máy trạng thái cử chỉ */
    unsigned char btn_state;
    unsigned int timer_ms;
    unsigned int gap_ms;
    unsigned char debounce_cnt;
    unsigned char last_raw_pressed;
    unsigned char current_raw_pressed;
    unsigned int led_blink_timer;
    unsigned char led_blink_pattern;

    /* Cấu hình xung nhịp CPU 16MHz */
    CLK_CKDIVR = 0x00;

    /* Cấu hình PC3 là Output Push-Pull (LED phản hồi cử chỉ) */
    GPIOC_DDR |= (1 << 3);
    GPIOC_CR1 |= (1 << 3);
    GPIOC_ODR &= ~(1 << 3); /* Tắt LED ban đầu */

    /* Cấu hình PC4 là Input Pull-up (Chân nút nhấn) */
    GPIOC_DDR &= ~(1 << 4);
    GPIOC_CR1 |= (1 << 4);
    GPIOC_CR2 &= ~(1 << 4);

    /* Cấu hình UART1 (9600 bps @ 16MHz) */
    UART1_BRR2 = 0x03;
    UART1_BRR1 = 0x68;
    UART1_CR2  = 0x2C; /* TEN, REN, RIEN */

    /* Cho phép ngắt toàn cục */
    _asm("rim");

    btn_state = 0; /* 0: IDLE, 1: PRESSED_1, 2: GAP_WAIT, 3: PRESSED_2, 4: HOLD_WAIT */
    timer_ms = 0;
    gap_ms = 0;
    debounce_cnt = 0;
    last_raw_pressed = 0;
    current_raw_pressed = 0;
    led_blink_timer = 0;
    led_blink_pattern = 0;

    while(1)
    {
        /* 1. XỬ LÝ LỆNH TRUY VẤN TỪ GATEWAY ESP32 */
        if (global_btn_ready)
        {
            unsigned char cmd;
            unsigned char ack_val;
            unsigned char checksum;

            cmd = global_btn_cmd;
            global_btn_ready = 0;

            if (cmd == 0x50) {
                /* TRUY VẤN CỬ CHỈ: Ưu tiên trả về sự kiện chốt (Single / Double / Long) */
                if (latched_gesture != 0) {
                    ack_val = latched_gesture;
                    latched_gesture = 0; /* Đọc xong tự động xóa chốt */
                } else {
                    /* Nếu không có sự kiện chốt, trả về trạng thái vật lý thực tế: 0x56 nếu giữ, 0x50 nếu nhả */
                    ack_val = current_raw_pressed ? GESTURE_RAW_PRESSED : GESTURE_IDLE;
                }
            }
            else if (cmd == CMD_GET_COUNTER) {
                /* Đọc tổng số lần nhấn tích lũy */
                ack_val = click_counter;
            }
            else if (cmd == CMD_RESET_COUNTER) {
                /* Đặt lại bộ đếm */
                click_counter = 0;
                ack_val = 0;
            }
            else {
                /* Lệnh khác: Trả về trạng thái chốt hoặc phím */
                if (latched_gesture != 0) {
                    ack_val = latched_gesture;
                    latched_gesture = 0;
                } else {
                    ack_val = current_raw_pressed ? GESTURE_RAW_PRESSED : GESTURE_IDLE;
                }
            }

            /* Nghỉ 2ms để xả bộ đệm Repeater trên vòng Ring */
            delay_ms(2);

            /* Gửi phản hồi chuẩn [0xAA, ID, ACK, CHECKSUM] */
            checksum = (unsigned char)(SLAVE_ID + ack_val);
            UART_SendByte(0xAA);
            UART_SendByte(SLAVE_ID);
            UART_SendByte(ack_val);
            UART_SendByte(checksum);
        }

        /* 2. BỘ LỌC RUNG PHÍM SỐ HỌC (2ms / chu kỳ quét) */
        delay_ms(2);

        /* Nút nhấn tích cực LOW: PC4 = 0 khi bấm */
        if ((GPIOC_IDR & (1 << 4)) == 0) {
            if (debounce_cnt < 10) debounce_cnt++;
        } else {
            if (debounce_cnt > 0) debounce_cnt--;
        }

        if (debounce_cnt >= 8) {
            current_raw_pressed = 1;
        } else if (debounce_cnt <= 2) {
            current_raw_pressed = 0;
        }

        /* 3. MÁY TRẠNG THÁI PHÂN LOẠI CỬ CHỈ THÔNG MINH (GESTURE CLASSIFIER) */
        switch(btn_state)
        {
            case 0: /* IDLE: Chờ phát hiện nhấn phím */
                if (current_raw_pressed && !last_raw_pressed) {
                    btn_state = 1; /* Chuyển sang PRESSED_1 */
                    timer_ms = 0;
                }
                break;

            case 1: /* PRESSED_1: Đang nhấn lần 1 */
                timer_ms += 2;
                if (!current_raw_pressed) {
                    /* Nhả phím lần 1 */
                    if (timer_ms < 1500) {
                        btn_state = 2; /* Chờ xem có nhấn lần 2 trong 300ms không */
                        gap_ms = 0;
                    } else {
                        btn_state = 0;
                    }
                } else {
                    /* Vẫn đang giữ nút: Kiểm tra ngưỡng Nhấn Giữ (Long Press > 1.5s) */
                    if (timer_ms >= 1500) {
                        latched_gesture = GESTURE_LONG_PRESS;
                        click_counter++;
                        /* Phản hồi LED: Bật sáng 300ms */
                        led_blink_pattern = 3;
                        led_blink_timer = 300;
                        GPIOC_ODR |= (1 << 3);
                        btn_state = 4; /* Chờ nhả phím sau Long Press */
                    }
                }
                break;

            case 2: /* GAP_WAIT: Đã nhả lần 1, đang chờ khoảng trống 300ms */
                gap_ms += 2;
                if (current_raw_pressed && !last_raw_pressed) {
                    /* Bấm lần 2 xuất hiện trước khi hết 300ms -> NHẤN ĐÚP! */
                    btn_state = 3; /* PRESSED_2 */
                    timer_ms = 0;
                }
                else if (gap_ms >= 300) {
                    /* Hết 300ms mà không có lần nhấn thứ 2 -> XÁC NHẬN NHẤN ĐƠN (Single Click) */
                    latched_gesture = GESTURE_SINGLE_CLICK;
                    click_counter++;
                    /* Phản hồi LED: Nháy 1 lần 60ms */
                    led_blink_pattern = 1;
                    led_blink_timer = 60;
                    GPIOC_ODR |= (1 << 3);
                    btn_state = 0;
                }
                break;

            case 3: /* PRESSED_2: Đang nhấn lần 2 */
                timer_ms += 2;
                if (!current_raw_pressed) {
                    /* Nhả phím lần 2 -> XÁC NHẬN NHẤN ĐÚP (Double Click) */
                    latched_gesture = GESTURE_DOUBLE_CLICK;
                    click_counter++;
                    /* Phản hồi LED: Bắt đầu nháy đúp */
                    led_blink_pattern = 2;
                    led_blink_timer = 160;
                    btn_state = 0;
                } else {
                    if (timer_ms >= 1500) {
                        latched_gesture = GESTURE_LONG_PRESS;
                        click_counter++;
                        led_blink_pattern = 3;
                        led_blink_timer = 300;
                        GPIOC_ODR |= (1 << 3);
                        btn_state = 4;
                    }
                }
                break;

            case 4: /* HOLD_WAIT: Chờ người dùng thả tay sau khi đã bắt Long Press */
                if (!current_raw_pressed) {
                    btn_state = 0;
                }
                break;

            default:
                btn_state = 0;
                break;
        }

        last_raw_pressed = current_raw_pressed;

        /* 4. XỬ LÝ PHẢN HỒI THỊ GIÁC LED PC3 */
        if (led_blink_timer > 0) {
            led_blink_timer -= 2;
            if (led_blink_pattern == 1 || led_blink_pattern == 3) {
                /* Nháy đơn hoặc Long press: giữ LED bật đến hết timer */
                if (led_blink_timer == 0) {
                    GPIOC_ODR &= ~(1 << 3);
                    led_blink_pattern = 0;
                }
            }
            else if (led_blink_pattern == 2) {
                /* Nháy đúp: 160ms -> 120-160ms Bật, 80-120ms Tắt, 40-80ms Bật, 0-40ms Tắt */
                if (led_blink_timer > 120) {
                    GPIOC_ODR |= (1 << 3);
                } else if (led_blink_timer > 80) {
                    GPIOC_ODR &= ~(1 << 3);
                } else if (led_blink_timer > 40) {
                    GPIOC_ODR |= (1 << 3);
                } else {
                    GPIOC_ODR &= ~(1 << 3);
                }
                if (led_blink_timer == 0) led_blink_pattern = 0;
            }
        }
    }
}
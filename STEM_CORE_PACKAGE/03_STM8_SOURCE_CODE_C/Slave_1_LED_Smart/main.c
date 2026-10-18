/* =========================================================
   STM8S103F3P6 - SLAVE 1: MODULE ĐÈN LED THÔNG MINH (SMART EFFECTS)
   =========================================================
   - Chân kết nối phần cứng:
     + LED (Output PWM / TIM1_CH3) : PC3 (Chân số 13)
     + UART1 TX                   : PD5 (Chân số 2)
     + UART1 RX                   : PD6 (Chân số 3)
     + Nguồn                      : VCC (3.3V hoặc 5V) và GND
   - Tính năng thông minh (Tự hành 100% bằng STM8 Timer):
     + Điều chỉnh độ sáng PWM 8-bit (0 - 100%) tần số 3.9kHz
     + Hiệu ứng Đèn thở (Breathing / Fade In - Fade Out)
     + Hiệu ứng Nháy đôi (Double Flash Strobe)
     + Hiệu ứng Tín hiệu cứu hộ SOS Morse Code (... --- ...)
     + Hiệu ứng Nhịp đập trái tim (Heartbeat pulse)
     + Nhấp nháy đơn chuẩn 1Hz (Blink)
   - Tốc độ UART: 9600 bps, 8N1
   - Giao thức   : Khung truyền 4 Byte [0xAA, ID, CMD, CRC]
   ========================================================= */

#define CLK_CKDIVR    *(volatile unsigned char*)(0x0050C6)

/* =========================================================
   GPIOC (PC3 = TIM1_CH3 / LED)
========================================================= */
#define GPIOC_ODR     *(volatile unsigned char*)(0x00500A)
#define GPIOC_DDR     *(volatile unsigned char*)(0x00500C)
#define GPIOC_CR1     *(volatile unsigned char*)(0x00500D)

/* =========================================================
   TIM1 (PWM ĐIỀU CHỈNH ĐỘ SÁNG TẠI PC3)
========================================================= */
#define TIM1_CR1      *(volatile unsigned char*)(0x005250)
#define TIM1_CCMR3    *(volatile unsigned char*)(0x00525A)
#define TIM1_CCER2    *(volatile unsigned char*)(0x00525D)
#define TIM1_PSCRH    *(volatile unsigned char*)(0x005260)
#define TIM1_PSCRL    *(volatile unsigned char*)(0x005261)
#define TIM1_ARRH     *(volatile unsigned char*)(0x005262)
#define TIM1_ARRL     *(volatile unsigned char*)(0x005263)
#define TIM1_CCR3H    *(volatile unsigned char*)(0x005269)
#define TIM1_CCR3L    *(volatile unsigned char*)(0x00526A)
#define TIM1_BKR      *(volatile unsigned char*)(0x00526D)

/* =========================================================
   UART1 THANH GHI (9600 BAUD @ 16MHz)
========================================================= */
#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)
#define UART1_BRR1    *(volatile unsigned char*)(0x005232)
#define UART1_BRR2    *(volatile unsigned char*)(0x005233)
#define UART1_CR2     *(volatile unsigned char*)(0x005235)

#define SLAVE_ID      0x01

/* Các chế độ hoạt động thông minh của LED */
#define MODE_OFF          0
#define MODE_STATIC       1
#define MODE_BLINK        2
#define MODE_BREATHE      3
#define MODE_DOUBLE_FLASH 4
#define MODE_SOS          5
#define MODE_HEARTBEAT    6

/* Biến nhận diện lệnh từ ngắt UART */
extern volatile unsigned char global_led_cmd;
extern volatile unsigned char global_led_ready;

/* Trạng thái hoạt động hiện tại */
static unsigned char current_mode = MODE_OFF;
static unsigned char current_brightness = 100; /* 0-100% */

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

/* Khởi tạo Hardware PWM trên PC3 (TIM1_CH3) */
void LED_PWM_Init(void) {
    /* Prescaler 16: 16MHz / 16 = 1MHz (1us/tick) */
    TIM1_PSCRH = 0x00;
    TIM1_PSCRL = 0x0F;

    /* ARR = 255: PWM Frequency = 1MHz / 256 = ~3.9kHz (Không nháy mắt) */
    TIM1_ARRH = 0x00;
    TIM1_ARRL = 255;

    /* Khởi tạo Duty Cycle = 0 (Tắt) */
    TIM1_CCR3H = 0x00;
    TIM1_CCR3L = 0x00;

    /* Cấu hình PWM Mode 1 trên Kênh 3 kèm Preload */
    TIM1_CCMR3 = 0x68;

    /* Kích hoạt xuất xung Kênh 3 (PC3) */
    TIM1_CCER2 |= 0x01;

    /* Bật Main Output Enable (MOE = 1) */
    TIM1_BKR |= 0x80;

    /* Kích hoạt Timer Counter (CEN = 1) */
    TIM1_CR1 |= 0x01;
}

/* Đặt trực tiếp giá trị Duty Cycle (0 - 255) */
void LED_SetDuty(unsigned char duty) {
    TIM1_CCR3H = 0x00;
    TIM1_CCR3L = duty;
}

/* Bảng thời gian chuỗi mã Morse SOS:
   1 = Bật 150ms, 0 = Tắt 150ms
   S: . . .   -> 1 0 1 0 1 (0 0)
   O: - - -   -> 1 1 1 0 1 1 1 0 1 1 1 (0 0)
   S: . . .   -> 1 0 1 0 1 (0 0 0 0 0 0 0) Nghỉ dài */
static const unsigned char sos_pattern[34] = {
    1, 0, 1, 0, 1, 0, 0,                      /* S: ... */
    1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0,    /* O: --- */
    1, 0, 1, 0, 1,                            /* S: ... */
    0, 0, 0, 0, 0, 0, 0, 0, 0                 /* Nghỉ chu kỳ */
};

void main(void)
{
    unsigned int tick_ms;
    unsigned int breathe_step;
    unsigned char breathe_dir;
    unsigned int effect_timer;
    unsigned char sos_idx;

    /* Cấu hình xung nhịp CPU 16MHz */
    CLK_CKDIVR = 0x00;

    /* Cấu hình PC3 là Output Push-Pull */
    GPIOC_DDR |= (1 << 3);
    GPIOC_CR1 |= (1 << 3);
    GPIOC_ODR &= ~(1 << 3);

    /* Khởi tạo PWM */
    LED_PWM_Init();

    /* Cấu hình UART1 (9600 bps @ 16MHz) */
    UART1_BRR2 = 0x03;
    UART1_BRR1 = 0x68;
    UART1_CR2  = 0x2C; /* TEN, REN, RIEN */

    /* Cho phép ngắt toàn cục */
    _asm("rim");

    tick_ms = 0;
    breathe_step = 0;
    breathe_dir = 1; /* 1: Sáng dần, 0: Tối dần */
    effect_timer = 0;
    sos_idx = 0;

    while(1)
    {
        /* 1. XỬ LÝ LỆNH TỪ UART RING MASTER */
        if (global_led_ready)
        {
            unsigned char cmd;
            unsigned char ack_val;
            unsigned char checksum;

            cmd = global_led_cmd;
            global_led_ready = 0;

            /* Giải mã lệnh thông minh:
               - 0x00 hoặc 0x30: Tắt đèn (OFF)
               - 0x20: Bật 100% (ON)
               - 0x01 - 0x64: Đặt độ sáng 1% - 100%
               - 0x10: Chế độ nhấp nháy 1Hz (Blink)
               - 0x71: Hiệu ứng Đèn thở (Breathing Light)
               - 0x72: Hiệu ứng Nháy đôi (Double Flash)
               - 0x73: Hiệu ứng Báo nguy SOS Morse
               - 0x74: Hiệu ứng Nhịp đập trái tim (Heartbeat)
               - 0x70: Hỏi trạng thái hiện tại
            */
            if (cmd == 0x00 || cmd == 0x30) {
                current_mode = MODE_OFF;
                LED_SetDuty(0);
                ack_val = 0x00;
            }
            else if (cmd == 0x20) {
                current_mode = MODE_STATIC;
                current_brightness = 100;
                LED_SetDuty(255);
                ack_val = 0x20;
            }
            else if (cmd >= 0x01 && cmd <= 0x64) {
                current_mode = MODE_STATIC;
                current_brightness = cmd;
                /* Ánh xạ 1-100 sang 0-255 */
                LED_SetDuty((unsigned char)(((unsigned int)cmd * 255) / 100));
                ack_val = cmd;
            }
            else if (cmd == 0x10) {
                current_mode = MODE_BLINK;
                effect_timer = 0;
                ack_val = 0x10;
            }
            else if (cmd == 0x71) {
                current_mode = MODE_BREATHE;
                breathe_step = 0;
                breathe_dir = 1;
                effect_timer = 0;
                ack_val = 0x71;
            }
            else if (cmd == 0x72) {
                current_mode = MODE_DOUBLE_FLASH;
                effect_timer = 0;
                ack_val = 0x72;
            }
            else if (cmd == 0x73) {
                current_mode = MODE_SOS;
                effect_timer = 0;
                sos_idx = 0;
                ack_val = 0x73;
            }
            else if (cmd == 0x74) {
                current_mode = MODE_HEARTBEAT;
                effect_timer = 0;
                ack_val = 0x74;
            }
            else if (cmd == 0x70) {
                /* Trả về mã hiệu ứng hiện tại */
                if (current_mode == MODE_OFF) ack_val = 0x00;
                else if (current_mode == MODE_STATIC) ack_val = current_brightness;
                else if (current_mode == MODE_BLINK) ack_val = 0x10;
                else if (current_mode == MODE_BREATHE) ack_val = 0x71;
                else if (current_mode == MODE_DOUBLE_FLASH) ack_val = 0x72;
                else if (current_mode == MODE_SOS) ack_val = 0x73;
                else if (current_mode == MODE_HEARTBEAT) ack_val = 0x74;
                else ack_val = 0x00;
            }
            else {
                ack_val = cmd;
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

        /* 2. ĐỘNG CƠ XỬ LÝ HIỆU ỨNG TỰ HÀNH CỤC BỘ (AUTONOMOUS EFFECT ENGINE) */
        delay_ms(2); /* Bước nhịp 2ms */
        effect_timer += 2;

        switch(current_mode)
        {
            case MODE_OFF:
                LED_SetDuty(0);
                break;

            case MODE_STATIC:
                /* Đã đặt trực tiếp khi nhận lệnh */
                break;

            case MODE_BLINK:
                /* 500ms Sáng, 500ms Tắt (Chu kỳ 1s) */
                if (effect_timer < 500) {
                    LED_SetDuty(255);
                } else if (effect_timer < 1000) {
                    LED_SetDuty(0);
                } else {
                    effect_timer = 0;
                }
                break;

            case MODE_BREATHE:
                /* Đèn thở êm dịu: Tăng/giảm độ sáng mỗi 12ms */
                if (effect_timer >= 12) {
                    effect_timer = 0;
                    if (breathe_dir) {
                        breathe_step += 3;
                        if (breathe_step >= 255) {
                            breathe_step = 255;
                            breathe_dir = 0;
                        }
                    } else {
                        if (breathe_step > 3) {
                            breathe_step -= 3;
                        } else {
                            breathe_step = 0;
                            breathe_dir = 1;
                        }
                    }
                    /* Điều chỉnh độ sáng phi tuyến tính nhẹ để mắt người nhìn êm */
                    LED_SetDuty((unsigned char)breathe_step);
                }
                break;

            case MODE_DOUBLE_FLASH:
                /* Chớp 2 nháy nhanh (80ms On, 80ms Off, 80ms On, 760ms Off) */
                if (effect_timer < 80) {
                    LED_SetDuty(255);
                } else if (effect_timer < 160) {
                    LED_SetDuty(0);
                } else if (effect_timer < 240) {
                    LED_SetDuty(255);
                } else if (effect_timer < 1000) {
                    LED_SetDuty(0);
                } else {
                    effect_timer = 0;
                }
                break;

            case MODE_SOS:
                /* Phát mã Morse SOS tự động */
                if (effect_timer >= 150) {
                    effect_timer = 0;
                    if (sos_pattern[sos_idx]) {
                        LED_SetDuty(255);
                    } else {
                        LED_SetDuty(0);
                    }
                    sos_idx++;
                    if (sos_idx >= 34) {
                        sos_idx = 0;
                    }
                }
                break;

            case MODE_HEARTBEAT:
                /* Nhịp đập trái tim (Thình-thịch: 80ms On, 100ms Off, 120ms On, 700ms Off) */
                if (effect_timer < 80) {
                    LED_SetDuty(160);
                } else if (effect_timer < 180) {
                    LED_SetDuty(0);
                } else if (effect_timer < 300) {
                    LED_SetDuty(255);
                } else if (effect_timer < 1000) {
                    LED_SetDuty(0);
                } else {
                    effect_timer = 0;
                }
                break;

            default:
                break;
        }
    }
}
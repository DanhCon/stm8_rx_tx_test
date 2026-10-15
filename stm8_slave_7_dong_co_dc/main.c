/* =========================================================
   STM8S103F3P6 - SLAVE 7: MODULE ĐIỀU KHIỂN ĐỘNG CƠ DC
   =========================================================
   - Module mạch cầu H tương thích: L9110S, DRV8833, L298N mini
   - Chân kết nối phần cứng:
     + INA (Chiều thuận / PWM1) : PC3 (TIM1_CH3 - Chân số 13)
     + INB (Chiều nghịch / PWM2): PC4 (TIM1_CH4 - Chân số 14)
     + UART1 TX                 : PD5 (Chân số 2)
     + UART1 RX                 : PD6 (Chân số 3)
   - Bộ định thời: TIM1 Hardware PWM tần số 10kHz (Êm ái, không rít)
   - Tốc độ UART : 9600 bps, 8N1
   - Giao thức   : Khung truyền 4 Byte [0xAA, ID, CMD/SPEED, CRC]
   ========================================================= */

#define CLK_CKDIVR    *(volatile unsigned char*)(0x0050C6)

/* =========================================================
   GPIOC (ĐIỀU KHIỂN CẦU H: INA = PC3, INB = PC4)
========================================================= */
#define GPIOC_ODR     *(volatile unsigned char*)(0x00500A)
#define GPIOC_DDR     *(volatile unsigned char*)(0x00500C)
#define GPIOC_CR1     *(volatile unsigned char*)(0x00500D)

/* =========================================================
   TIM1 (HARDWARE PWM CHO ĐỘNG CƠ DC TẠI PC3 & PC4)
========================================================= */
#define TIM1_CR1      *(volatile unsigned char*)(0x005250)
#define TIM1_CCMR3    *(volatile unsigned char*)(0x00525A)
#define TIM1_CCMR4    *(volatile unsigned char*)(0x00525B)
#define TIM1_CCER2    *(volatile unsigned char*)(0x00525D)
#define TIM1_PSCRH    *(volatile unsigned char*)(0x005260)
#define TIM1_PSCRL    *(volatile unsigned char*)(0x005261)
#define TIM1_ARRH     *(volatile unsigned char*)(0x005262)
#define TIM1_ARRL     *(volatile unsigned char*)(0x005263)
#define TIM1_CCR3H    *(volatile unsigned char*)(0x005269)
#define TIM1_CCR3L    *(volatile unsigned char*)(0x00526A)
#define TIM1_CCR4H    *(volatile unsigned char*)(0x00526B)
#define TIM1_CCR4L    *(volatile unsigned char*)(0x00526C)
#define TIM1_BKR      *(volatile unsigned char*)(0x00526D)

/* =========================================================
   UART1 THANH GHI (9600 BAUD @ 16MHz)
========================================================= */
#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)
#define UART1_BRR1    *(volatile unsigned char*)(0x005232)
#define UART1_BRR2    *(volatile unsigned char*)(0x005233)
#define UART1_CR2     *(volatile unsigned char*)(0x005235)

#define SLAVE_ID      0x07

/* Biến nhận diện lệnh từ ngắt UART */
extern volatile unsigned char global_motor_cmd;
extern volatile unsigned char global_motor_ready;

/* Trạng thái động cơ hiện tại: 0 = STOP, 1 = FORWARD, 2 = REVERSE, 3 = BRAKE */
static unsigned char current_dir = 0;
static unsigned char current_speed = 0;

void delay_ms(unsigned int ms) {
    volatile unsigned int i, j;
    for(i = 0; i < ms; i++) {
        for(j = 0; j < 3000; j++); 
    }
}

/* Hàm truyền 1 Byte chặn dòng */
void UART_SendByte(unsigned char data) {
    while(!(UART1_SR & 0x80)); 
    UART1_DR = data;
}

/* Khởi tạo Hardware PWM 10kHz trên PC3 (TIM1_CH3) và PC4 (TIM1_CH4) */
void Motor_PWM_Init(void) {
    /* 1. Prescaler 16: 16MHz / 16 = 1MHz (1us/tick) */
    TIM1_PSCRH = 0x00;
    TIM1_PSCRL = 0x0F; 

    /* 2. Auto-Reload ARR = 100 (Tần số PWM = 1MHz / 100 = 10kHz) */
    TIM1_ARRH = 0x00;
    TIM1_ARRL = 100;

    /* 3. Khởi tạo Duty Cycle = 0 (Động cơ đứng yên) */
    TIM1_CCR3H = 0x00;
    TIM1_CCR3L = 0x00;
    TIM1_CCR4H = 0x00;
    TIM1_CCR4L = 0x00;

    /* 4. Cấu hình PWM Mode 1 trên Kênh 3 & Kênh 4 kèm Preload */
    TIM1_CCMR3 = 0x68; /* OC3M = 110, OC3PE = 1 */
    TIM1_CCMR4 = 0x68; /* OC4M = 110, OC4PE = 1 */

    /* 5. Kích hoạt xuất xung Kênh 3 (PC3) và Kênh 4 (PC4) */
    TIM1_CCER2 = 0x11; /* CC3E = 1, CC4E = 1 */

    /* 6. Bật Main Output Enable (MOE = 1) */
    TIM1_BKR |= 0x80;

    /* 7. Kích hoạt Timer Counter (CEN = 1) */
    TIM1_CR1 |= 0x01;
}

/* =========================================================
   HÀM ĐIỀU KHIỂN TRẠNG THÁI & TỐC ĐỘ ĐỘNG CƠ
   - dir: 0 = STOP, 1 = FORWARD, 2 = REVERSE, 3 = BRAKE
   - speed: 0 đến 100 (%)
========================================================= */
void Motor_Set(unsigned char dir, unsigned char speed) {
    if (speed > 100) speed = 100;

    current_dir = dir;
    current_speed = speed;

    if (dir == 1) {
        /* TIẾN: INA (PC3) = PWM, INB (PC4) = 0 */
        TIM1_CCR4L = 0;
        TIM1_CCR3L = speed;
    } 
    else if (dir == 2) {
        /* LÙI: INA (PC3) = 0, INB (PC4) = PWM */
        TIM1_CCR3L = 0;
        TIM1_CCR4L = speed;
    } 
    else if (dir == 3) {
        /* PHANH NHANH: INA = 100, INB = 100 */
        TIM1_CCR3L = 100;
        TIM1_CCR4L = 100;
    } 
    else {
        /* DỪNG (THẢ TRÔI): INA = 0, INB = 0 */
        TIM1_CCR3L = 0;
        TIM1_CCR4L = 0;
    }
}

void main(void)
{
    /* Cấu hình xung nhịp CPU 16MHz không chia */
    CLK_CKDIVR = 0x00; 

    /* Cấu hình PC3 & PC4 là Output Push-Pull tốc độ cao */
    GPIOC_DDR |= (1 << 3) | (1 << 4);  
    GPIOC_CR1 |= (1 << 3) | (1 << 4);  
    GPIOC_ODR &= ~((1 << 3) | (1 << 4)); 

    /* Khởi tạo bộ phát xung PWM điều khiển động cơ */
    Motor_PWM_Init();

    /* Cấu hình UART1 (9600 bps @ 16MHz) */
    UART1_BRR2 = 0x03;
    UART1_BRR1 = 0x68;
    UART1_CR2  = 0x2C; /* Bật TEN, REN và ngắt RIEN */

    /* Cho phép ngắt toàn cục */
    _asm("rim");

    while(1)
    {
        /* Kiểm tra khi có lệnh điều khiển gửi từ Gateway */
        if (global_motor_ready) 
        {
            /* KHAI BÁO TOÀN BỘ BIẾN Ở ĐẦU BLOCK (CHUẨN ANSI C89) */
            unsigned char cmd;
            unsigned char ack_data;
            unsigned char checksum;

            cmd = global_motor_cmd;
            global_motor_ready = 0;

            /* XỬ LÝ MÃ LỆNH ĐỘNG CƠ:
               1. Dừng: 0x00 hoặc 0x30
               2. Tiến với tốc độ 1% - 100%: 0x01 đến 0x64
                  + Lệnh tắt 0x10: Tiến 50%
                  + Lệnh tắt 0x11: Tiến 100%
               3. Lùi với tốc độ 1% - 100%: 0x81 đến 0xE4 (tốc độ = cmd - 0x80)
                  + Lệnh tắt 0x20: Lùi 50%
                  + Lệnh tắt 0x21: Lùi 100%
               4. Phanh nhanh: 0xFF
               5. Hỏi trạng thái: 0x70
            */
            if (cmd == 0x00 || cmd == 0x30) {
                Motor_Set(0, 0); /* STOP */
                ack_data = 0x00;
            }
            else if (cmd == 0x10) {
                Motor_Set(1, 50); /* TIẾN 50% */
                ack_data = 50;
            }
            else if (cmd == 0x11) {
                Motor_Set(1, 100); /* TIẾN 100% */
                ack_data = 100;
            }
            else if (cmd >= 0x01 && cmd <= 0x64) {
                Motor_Set(1, cmd); /* TIẾN THEO TỐC ĐỘ 1-100% */
                ack_data = cmd;
            }
            else if (cmd == 0x20) {
                Motor_Set(2, 50); /* LÙI 50% */
                ack_data = 0x80 + 50;
            }
            else if (cmd == 0x21) {
                Motor_Set(2, 100); /* LÙI 100% */
                ack_data = 0x80 + 100;
            }
            else if (cmd >= 0x81 && cmd <= 0xE4) {
                unsigned char rev_speed;
                rev_speed = cmd - 0x80;
                Motor_Set(2, rev_speed); /* LÙI THEO TỐC ĐỘ 1-100% */
                ack_data = cmd;
            }
            else if (cmd == 0xFF) {
                Motor_Set(3, 100); /* PHANH ĐIỆN TỪ */
                ack_data = 0xFF;
            }
            else if (cmd == 0x70) {
                /* TRUY VẤN: Trả về trạng thái hiện tại */
                if (current_dir == 1) ack_data = current_speed;
                else if (current_dir == 2) ack_data = 0x80 + current_speed;
                else ack_data = 0x00;
            }
            else {
                ack_data = cmd;
            }

            /* Nghỉ 2ms để mạch lặp Repeater trên vòng Ring xả sạch byte Echo */
            delay_ms(2);

            /* PHÁT GÓI PHẢN HỒI THẬT (FRAME 2 - REAL ACK) VỀ CHO GATEWAY */
            checksum = (unsigned char)(SLAVE_ID + ack_data);

            UART_SendByte(0xAA);
            UART_SendByte(SLAVE_ID);
            UART_SendByte(ack_data);
            UART_SendByte(checksum);
        }
    }
}

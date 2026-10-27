/* =========================================================
   STM8S103F3P6 - SLAVE 8: MODULE ĐIỀU KHIỂN ĐỘNG CƠ SERVO RC (SG90 / MG90S)
   =========================================================
   - Chân kết nối phần cứng:
     + PWM Signal (Dây cam / vàng) : PC3 (TIM1_CH3 - Chân số 13)
     + VCC (Dây đỏ)                : Nguồn 5V
     + GND (Dây nâu / đen)         : GND chung hệ thống
     + UART1 TX                    : PD5 (Chân số 2)
     + UART1 RX                    : PD6 (Chân số 3)
   - Bộ định thời: TIM1 Hardware PWM 50Hz (Chu kỳ 20ms, 1 tick = 1us)
   - Tốc độ UART : 9600 bps, 8N1
   - Giao thức   : Khung truyền 4 Byte [0xAA, ID, GÓC_QUAY, CRC]
   ========================================================= */

#define CLK_CKDIVR    *(volatile unsigned char*)(0x0050C6)

/* =========================================================
   GPIOC (XUẤT XUNG PWM CHO SERVO TẠI PC3)
========================================================= */
#define GPIOC_ODR     *(volatile unsigned char*)(0x00500A)
#define GPIOC_DDR     *(volatile unsigned char*)(0x00500C)
#define GPIOC_CR1     *(volatile unsigned char*)(0x00500D)

/* =========================================================
   TIM1 (BỘ PHÁT XUNG 50Hz CHUẨN RC CHO SERVO)
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

#define SLAVE_ID      0x08

/* Biến nhận diện lệnh từ ngắt UART */
extern volatile unsigned char global_servo_cmd;
extern volatile unsigned char global_servo_ready;

/* Góc servo hiện tại (mặc định khởi động ở 90 độ - vị trí chính giữa) */
static unsigned char current_angle = 90;

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

/* Khởi tạo Hardware PWM 50Hz (Chu kỳ 20ms) trên PC3 (TIM1_CH3) */
void Servo_PWM_Init(void) {
    /* 1. Prescaler 16: 16MHz / 16 = 1MHz (1 tick = 1 microsecond) */
    TIM1_PSCRH = 0x00;
    TIM1_PSCRL = 0x0F; 

    /* 2. Auto-Reload ARR = 20,000 (0x4E20) -> Chu kỳ đúng 20ms = 50Hz */
    TIM1_ARRH = 0x4E;
    TIM1_ARRL = 0x20;

    /* 3. Khởi tạo độ rộng xung ban đầu 1500us (Góc 90 độ) */
    TIM1_CCR3H = 0x05; /* 1500 = 0x05DC */
    TIM1_CCR3L = 0xDC;

    /* 4. Cấu hình PWM Mode 1 trên Kênh 3 kèm Preload Enable */
    TIM1_CCMR3 = 0x68; /* OC3M = 110, OC3PE = 1 */

    /* 5. Kích hoạt xuất xung Kênh 3 (PC3) */
    TIM1_CCER2 |= 0x01; /* CC3E = 1 */

    /* 6. Bật Main Output Enable (MOE = 1) */
    TIM1_BKR |= 0x80;

    /* 7. Kích hoạt Timer Counter (CEN = 1) */
    TIM1_CR1 |= 0x01;
}

/* =========================================================
   HÀM ĐẶT GÓC QUAY SERVO: angle từ 0 đến 180 độ
   - 0 độ   : Xung 500us   (CCR3 = 500)
   - 90 độ  : Xung 1500us  (CCR3 = 1500)
   - 180 độ : Xung 2500us  (CCR3 = 2500)
   - Công thức tuyến tính: pulse = 500 + (angle * 2000) / 180
                          pulse = 500 + (angle * 100) / 9
========================================================= */
void Servo_Set_Angle(unsigned char angle) {
    unsigned int pulse;

    if (angle > 180) angle = 180;
    current_angle = angle;

    /* Tính độ rộng xung microsecond */
    pulse = 500 + ((unsigned int)angle * 100) / 9;

    /* Nạp vào thanh ghi so sánh Capture/Compare 3 */
    TIM1_CCR3H = (unsigned char)((pulse >> 8) & 0xFF);
    TIM1_CCR3L = (unsigned char)(pulse & 0xFF);
}

void main(void)
{
    /* Cấu hình xung nhịp CPU 16MHz không chia */
    CLK_CKDIVR = 0x00; 

    /* Cấu hình PC3 là Output Push-Pull cho Servo */
    GPIOC_DDR |= (1 << 3);  
    GPIOC_CR1 |= (1 << 3);  
    GPIOC_ODR &= ~(1 << 3); 

    /* Khởi tạo Hardware PWM 50Hz cho Servo */
    Servo_PWM_Init();

    /* Đặt góc xuất phát 90 độ (Chính giữa) */
    Servo_Set_Angle(90);

    /* Cấu hình UART1 (9600 bps @ 16MHz) */
    UART1_BRR2 = 0x03;
    UART1_BRR1 = 0x68;
    UART1_CR2  = 0x2C; /* Bật TEN, REN và ngắt RIEN */

    /* Cho phép ngắt toàn cục */
    _asm("rim");

    while(1)
    {
        /* Kiểm tra khi có lệnh điều khiển gửi từ Gateway */
        if (global_servo_ready) 
        {
            /* KHAI BÁO TOÀN BỘ BIẾN Ở ĐẦU BLOCK (CHUẨN ANSI C89) */
            unsigned char cmd;
            unsigned char ack_angle;
            unsigned char checksum;

            cmd = global_servo_cmd;
            global_servo_ready = 0;

            /* XỬ LÝ MÃ LỆNH SERVO:
               1. Đặt góc trực tiếp 0 - 180 độ: 0x00 đến 0xB4
               2. Phím tắt preset:
                  + 0xC0 (192): Góc 0 độ (Góc cực trái)
                  + 0xC1 (193): Góc 90 độ (Chính giữa)
                  + 0xC2 (194): Góc 180 độ (Góc cực phải)
               3. Truy vấn góc hiện tại: 0x70
            */
            if (cmd <= 180) {
                Servo_Set_Angle(cmd);
                ack_angle = cmd;
            }
            else if (cmd == 0xC0) {
                Servo_Set_Angle(0);
                ack_angle = 0;
            }
            else if (cmd == 0xC1) {
                Servo_Set_Angle(90);
                ack_angle = 90;
            }
            else if (cmd == 0xC2) {
                Servo_Set_Angle(180);
                ack_angle = 180;
            }
            else if (cmd == 0x70) {
                /* TRUY VẤN: Trả về góc hiện tại */
                ack_angle = current_angle;
            }
            else {
                ack_angle = current_angle;
            }

            /* Nghỉ 2ms để mạch lặp Repeater trên vòng Ring xả sạch byte Echo */
            delay_ms(2);

            /* PHÁT GÓI PHẢN HỒI THẬT (FRAME 2 - REAL ACK) VỀ CHO GATEWAY */
            checksum = (unsigned char)(SLAVE_ID + ack_angle);

            UART_SendByte(0xAA);
            UART_SendByte(SLAVE_ID);
            UART_SendByte(ack_angle);
            UART_SendByte(checksum);
        }
    }
}

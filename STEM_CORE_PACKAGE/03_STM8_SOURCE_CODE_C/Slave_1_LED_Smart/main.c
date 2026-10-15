/* =========================================================
   STM8S103F3P6 - SLAVE 1: SMART LED ACTUATOR
   Autonomous Effects (Breathe, SOS, Double Flash, Blink, PWM)
   Hardware Repeater + Non-blocking TIM4 Background Engine
========================================================= */

#define CLK_CKDIVR    *(volatile unsigned char*)(0x0050C6)

/* =========================================================
   GPIOC (LED TAI PC3 - TIM1_CH3)
========================================================= */
#define GPIOC_ODR     *(volatile unsigned char*)(0x00500A)
#define GPIOC_DDR     *(volatile unsigned char*)(0x00500C)
#define GPIOC_CR1     *(volatile unsigned char*)(0x00500D)

/* =========================================================
   TIM1 (HARDWARE PWM 10kHz CHO LED TAI PC3)
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
   TIM4 (1ms PERIODIC SYSTEM TICK ENGINE)
========================================================= */
#define TIM4_CR1      *(volatile unsigned char*)(0x005340)
#define TIM4_IER      *(volatile unsigned char*)(0x005343)
#define TIM4_SR       *(volatile unsigned char*)(0x005344)
#define TIM4_EGR      *(volatile unsigned char*)(0x005345)
#define TIM4_CNTR     *(volatile unsigned char*)(0x005346)
#define TIM4_PSCR     *(volatile unsigned char*)(0x005347)
#define TIM4_ARR      *(volatile unsigned char*)(0x005348)

/* =========================================================
   UART1 (9600 BAUD @ 16MHz)
========================================================= */
#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)
#define UART1_BRR1    *(volatile unsigned char*)(0x005232)
#define UART1_BRR2    *(volatile unsigned char*)(0x005233)
#define UART1_CR2     *(volatile unsigned char*)(0x005235)

#define SLAVE_ID          0x01

#define MODE_BLINK        0x10
#define MODE_ON           0x20
#define MODE_OFF          0x30
#define MODE_BREATHE      0x40
#define MODE_SOS          0x41
#define MODE_DOUBLE_FLASH 0x42
#define MODE_DIRECT_PWM   0x43

extern volatile unsigned char global_led_cmd;
extern volatile unsigned char current_mode;
extern volatile unsigned char current_duty;

void delay_ms(unsigned int ms) {
    volatile unsigned int i, j;
    for(i = 0; i < ms; i++) {
        for(j = 0; j < 3000; j++); 
    }
}

void UART_SendByte(unsigned char data) {
    while(!(UART1_SR & 0x80)); 
    UART1_DR = data;
}

/* Khoi tao TIM1 Hardware PWM 10kHz tren PC3 */
void TIM1_PWM_Init(void) {
    TIM1_PSCRH = 0x00;
    TIM1_PSCRL = 0x0F; /* 16MHz / 16 = 1MHz */

    TIM1_ARRH = 0x00;
    TIM1_ARRL = 100;   /* 1MHz / 100 = 10kHz */

    TIM1_CCR3H = 0x00;
    TIM1_CCR3L = 0x00; /* Duty = 0% */

    TIM1_CCMR3 = 0x68; /* PWM Mode 1, Preload */
    TIM1_CCER2 |= 0x01;/* Channel 3 Output Enable */
    TIM1_BKR |= 0x80;  /* Main Output Enable */
    TIM1_CR1 |= 0x01;  /* Counter Enable */
}

/* Khoi tao TIM4 1ms Periodic Tick cho Autonomous Effects */
void TIM4_Tick_Init(void) {
    TIM4_PSCR = 0x06;  /* Prescaler 2^6 = 64 -> 16MHz / 64 = 250kHz */
    TIM4_ARR = 249;    /* 250 / 250kHz = 1ms */
    TIM4_EGR = 0x01;   /* Update Generation (load prescaler) */
    TIM4_SR &= (unsigned char)(~0x01); /* Clear UIF */
    TIM4_IER = 0x01;   /* Update Interrupt Enable */
    TIM4_CR1 = 0x01;   /* Counter Enable */
}

void main(void)
{
    unsigned char cmd;
    unsigned char ack_crc;

    CLK_CKDIVR = 0x00; /* 16MHz HSI */

    /* Cấu hình PC3 là Output Push-Pull cho TIM1_CH3 */
    GPIOC_DDR |= (1 << 3);
    GPIOC_CR1 |= (1 << 3);
    GPIOC_ODR &= ~(1 << 3);

    TIM1_PWM_Init();
    TIM4_Tick_Init();

    /* UART1: BRR2 ghi truoc, BRR1 ghi sau */
    UART1_BRR2 = 0x03;
    UART1_BRR1 = 0x68;
    UART1_CR2 = 0x2C;  /* TEN, REN, RIEN */

    _asm("rim");

    while(1)
    {
        if (global_led_cmd != 0x00)
        {
            cmd = global_led_cmd;
            global_led_cmd = 0x00;

            /* Delay 2ms trước khi gửi phản hồi để toàn chuỗi Ring đồng bộ */
            delay_ms(2);

            ack_crc = (unsigned char)(SLAVE_ID + cmd);

            /* Gửi Frame xác nhận chuẩn 4 Byte */
            UART_SendByte(0xAA);
            UART_SendByte(SLAVE_ID);
            UART_SendByte(cmd);
            UART_SendByte(ack_crc);

            /* Chuyển đổi trạng thái hiệu ứng */
            if (cmd == MODE_ON)
            {
                current_mode = MODE_ON;
            }
            else if (cmd == MODE_OFF)
            {
                current_mode = MODE_OFF;
            }
            else if (cmd == MODE_BLINK)
            {
                current_mode = MODE_BLINK;
            }
            else if (cmd == MODE_BREATHE)
            {
                current_mode = MODE_BREATHE;
            }
            else if (cmd == MODE_SOS)
            {
                current_mode = MODE_SOS;
            }
            else if (cmd == MODE_DOUBLE_FLASH)
            {
                current_mode = MODE_DOUBLE_FLASH;
            }
            else if (cmd >= 0x80 && cmd <= 0xE4)
            {
                current_duty = (unsigned char)(cmd - 0x80);
                if (current_duty > 100) current_duty = 100;
                current_mode = MODE_DIRECT_PWM;
            }
            else if (cmd <= 100)
            {
                current_duty = cmd;
                current_mode = MODE_DIRECT_PWM;
            }
        }
    }
}
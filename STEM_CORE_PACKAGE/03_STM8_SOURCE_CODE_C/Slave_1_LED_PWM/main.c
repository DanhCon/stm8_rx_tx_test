#define CLK_CKDIVR    *(volatile unsigned char*)(0x0050C6)

/* =========================================================
   GPIOC (CO CAU CHAP HANH - LED TAI PC3)
========================================================= */
#define GPIOC_ODR     *(volatile unsigned char*)(0x00500A)
#define GPIOC_DDR     *(volatile unsigned char*)(0x00500C)
#define GPIOC_CR1     *(volatile unsigned char*)(0x00500D)

/* =========================================================
   TIM1 (BO TAO XUNG HARDWARE PWM CHO LED TAI PC3 - TIM1_CH3)
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
   UART1 THANH GHI
========================================================= */
#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)
#define UART1_BRR1    *(volatile unsigned char*)(0x005232)
#define UART1_BRR2    *(volatile unsigned char*)(0x005233)
#define UART1_CR2     *(volatile unsigned char*)(0x005235)

extern volatile unsigned char global_led_cmd;

void delay_ms(unsigned int ms) {
    volatile unsigned int i, j;
    for(i = 0; i < ms; i++) {
        for(j = 0; j < 3000; j++); 
    }
}

/* Ham truyen 1 Byte chan dong (Blocking TX) */
void UART_SendByte(unsigned char data) {
    while(!(UART1_SR & 0x80)); 
    UART1_DR = data;
}

/* Khoi tao Hardware PWM 10kHz tren PC3 (TIM1_CH3) */
void TIM1_PWM_Init(void) {
    /* 1. Prescaler 16: 16MHz / 16 = 1MHz (1us/tick) */
    TIM1_PSCRH = 0x00;
    TIM1_PSCRL = 0x0F; 

    /* 2. Auto-Reload ARR = 100 (Tan so PWM = 1MHz / 100 = 10kHz, min mang, khong nhap nhay) */
    TIM1_ARRH = 0x00;
    TIM1_ARRL = 100;

    /* 3. Duty cycle ban dau = 0 (Tat LED) */
    TIM1_CCR3H = 0x00;
    TIM1_CCR3L = 0x00;

    /* 4. Che do PWM Mode 1 tren Channel 3, bat Preload (OC3M=110, OC3PE=1) */
    TIM1_CCMR3 = 0x68;

    /* 5. Cho phep xuat xung ra kenh 3 (PC3) (CC3E = 1) */
    TIM1_CCER2 |= 0x01; 

    /* 6. Bat Main Output Enable (MOE = 1) - Bat buoc voi TIM1 */
    TIM1_BKR |= 0x80;

    /* 7. Bat Timer Counter (CEN = 1) */
    TIM1_CR1 |= 0x01;
}

/* Dat do sang LED: percent tu 0% (tat han) den 100% (sang toi da) */
void Set_LED_Brightness(unsigned char percent) {
    if (percent > 100) percent = 100;
    TIM1_CCR3H = 0x00;
    TIM1_CCR3L = percent;
}

void main(void)
{
    CLK_CKDIVR = 0x00; /* 16MHz */

    /* CAU HINH PC3 LA OUTPUT PUSH-PULL CHO PWM */
    GPIOC_DDR |= (1 << 3);  
    GPIOC_CR1 |= (1 << 3);  
    GPIOC_ODR &= ~(1 << 3); 

    /* KHOI TAO HARDWARE PWM CHO LED */
    TIM1_PWM_Init();

    /* CAU HINH UART1 (9600 bps @ 16MHz) */
    UART1_BRR2 = 0x03;
    UART1_BRR1 = 0x68;
    UART1_CR2 = 0x2C; /* Bat TEN, REN va RIEN */

    _asm("rim");

    while(1)
    {
        /* Neu co lenh tu Master gui xuong */
        if (global_led_cmd != 0x00) 
        {
            /* KHAI BAO TAT CA BIEN O DAU BLOCK (CHUAN ANSI C89 CHO COSMIC) */
            unsigned char cmd_to_execute;
            unsigned char my_id;
            unsigned char ack_checksum;
            unsigned char duty;

            /* BAT DAU THUC THI LOGIC */
            cmd_to_execute = global_led_cmd;
            global_led_cmd = 0x00; /* Xoa co ngay lap tuc */
            
            my_id = 0x01;
            ack_checksum = (unsigned char)(my_id + cmd_to_execute);

            /* --- BUOC 1: GUI ACK VE MASTER TRUOC --- */
            UART_SendByte(0xAA);
            UART_SendByte(my_id);
            UART_SendByte(cmd_to_execute);
            UART_SendByte(ack_checksum);

            /* --- BUOC 2: THUC THI DIEU CHINH DO SANG PWM --- */
            if (cmd_to_execute == 0x20) 
            {
                /* Lenh Bat LED 100% (Tuong thich nguoc chuan cu) */
                Set_LED_Brightness(100);
            }
            else if (cmd_to_execute == 0x30)
            {
                /* Lenh Tat LED 0% (Tuong thich nguoc chuan cu) */
                Set_LED_Brightness(0);
            }
            else if (cmd_to_execute == 0x10) 
            {
                /* Lenh Nhay LED bao ket noi */
                Set_LED_Brightness(100);
                delay_ms(1000);
                Set_LED_Brightness(0);
            }
            else if (cmd_to_execute >= 0x80 && cmd_to_execute <= 0xE4)
            {
                /* LENH DIEU CHINH DO SANG DANG OFFSET: 0x80 + duty (0 - 100%) */
                duty = (unsigned char)(cmd_to_execute - 0x80);
                Set_LED_Brightness(duty);
            }
            else if (cmd_to_execute <= 100)
            {
                /* LENH DIEU CHINH DO SANG TRUC TIEP: 0 - 100% */
                Set_LED_Brightness(cmd_to_execute);
            }
        }
    }
}

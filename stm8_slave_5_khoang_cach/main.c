#define CLK_CKDIVR    *(volatile unsigned char*)(0x0050C6)

/* =========================================================
   GPIOC (LED T?I PC3)
========================================================= */
#define GPIOC_ODR      *(volatile unsigned char*)(0x00500A)
#define GPIOC_DDR      *(volatile unsigned char*)(0x00500C)
#define GPIOC_CR1      *(volatile unsigned char*)(0x00500D)

/* =========================================================
   GPIOA (HC-SR04 TRIG T?I PA3)
========================================================= */
#define GPIOA_ODR      *(volatile unsigned char*)(0x005000)
#define GPIOA_DDR      *(volatile unsigned char*)(0x005002)
#define GPIOA_CR1      *(volatile unsigned char*)(0x005003)

/* =========================================================
   GPIOD (HC-SR04 ECHO T?I PD2)
========================================================= */
#define GPIOD_IDR      *(volatile unsigned char*)(0x005011)
#define GPIOD_DDR      *(volatile unsigned char*)(0x00500F)
#define GPIOD_CR1      *(volatile unsigned char*)(0x005010)

/* =========================================================
   UART1
========================================================= */
#define UART1_SR       *(volatile unsigned char*)(0x005230)
#define UART1_DR       *(volatile unsigned char*)(0x005231)
#define UART1_BRR1     *(volatile unsigned char*)(0x005233) /* S?a chu?n d?a ch? BRR1/BRR2 */
#define UART1_BRR2     *(volatile unsigned char*)(0x005232)
#define UART1_CR2      *(volatile unsigned char*)(0x005235)

/* =========================================================
   TIM2 (ÐO TH?I GIAN PULSE HC-SR04)
========================================================= */
#define TIM2_CR1       *(volatile unsigned char*)(0x005300)
#define TIM2_PSCR      *(volatile unsigned char*)(0x00530E)
#define TIM2_CNTRH     *(volatile unsigned char*)(0x00530C)
#define TIM2_CNTRL     *(volatile unsigned char*)(0x00530D)

/* Bi?n toàn c?c khai báo extern t? file Vector ng?t */
extern volatile unsigned char global_led_cmd;

/* Hàm delay microsecond b?ng Loop NOP */
void delay_us(unsigned char us) {
    volatile unsigned char i;
    for (i = 0; i < us; i++) {
        _asm("nop");
        _asm("nop");
    }
}

/* Hàm delay millisecond blocking */
void delay_ms(unsigned int ms) {
    volatile unsigned int i, j;
    for(i = 0; i < ms; i++) {
        for(j = 0; j < 3000; j++); 
    }
}

/* Hàm truy?n 1 Byte ch?n dòng (Blocking TX) */
void UART_SendByte(unsigned char data) {
    while(!(UART1_SR & 0x80)); 
    UART1_DR = data;
}

/* Kh?i t?o TIM2: Prescaler 16 -> 1 tick = 1us ? Clock 16MHz */
void TIM2_Init(void) {
    TIM2_PSCR = 0x04; /* 16MHz / 2^4 = 1MHz (1us/tick) */
    TIM2_CR1 |= 0x01; /* Enable Counter */
}

/* Ðo kho?ng cách HC-SR04 (Ðon v?: cm) */
unsigned int HCSR04_ReadDistance(void) {
    unsigned int echo_time = 0;
    unsigned int timeout = 0;

    /* 1. T?o xung TRIG 10us t?i PA3 */
    GPIOA_ODR &= ~(1 << 3);
    delay_us(2);
    GPIOA_ODR |= (1 << 3);
    delay_us(10);
    GPIOA_ODR &= ~(1 << 3);

    /* 2. Ch? chân ECHO (PD2) lên HIGH */
    timeout = 5000;
    while (!(GPIOD_IDR & (1 << 2))) {
        timeout--;
        if (timeout == 0) return 0; /* L?i không có c?m bi?n */
    }

    /* 3. Reset d?m TIM2 v? 0 */
    TIM2_CNTRH = 0;
    TIM2_CNTRL = 0;

    /* 4. Ch? chân ECHO (PD2) xu?ng LOW */
    timeout = 30000; /* Max Timeout ~5m */
    while (GPIOD_IDR & (1 << 2)) {
        timeout--;
        if (timeout == 0) break;
    }

    /* 5. Ð?c giá tr? Timer (B?t bu?c d?c HIGH tru?c, LOW sau) */
    echo_time = ((unsigned int)TIM2_CNTRH << 8) | TIM2_CNTRL;

    return (echo_time / 58);
}

void main(void)
{
    CLK_CKDIVR = 0x00; /* Master Clock = 16MHz */

    /* C?u hình PC3 - Output Push-Pull (LED) */
    GPIOC_DDR |= (1 << 3);  
    GPIOC_CR1 |= (1 << 3);  
    GPIOC_ODR &= ~(1 << 3); 

    /* C?u hình PA3 - Output Push-Pull (TRIG) */
    GPIOA_DDR |= (1 << 3);
    GPIOA_CR1 |= (1 << 3);
    GPIOA_ODR &= ~(1 << 3);

    /* C?u hình PD2 - Input Floating (ECHO) */
    GPIOD_DDR &= ~(1 << 2);
    GPIOD_CR1 &= ~(1 << 2);

    /* C?u hình UART1 (9600 bps @ 16MHz) */
    UART1_BRR2 = 0x03;
    UART1_BRR1 = 0x68;
    UART1_CR2 = 0x2C; /* TEN=1, REN=1, RIEN=1 */

    TIM2_Init();

    _asm("rim"); /* B?t ng?t toàn c?c */

    while(1)
    {
        if (global_led_cmd != 0x00) 
        {
            unsigned char cmd_to_execute;
            unsigned char my_id;
            unsigned char ack_checksum;
            unsigned int distance_cm;

            cmd_to_execute = global_led_cmd;
            global_led_cmd = 0x00; /* Xóa c? l?nh */
            
            my_id = 0x01; /* Slave ID = 1 */
            ack_checksum = (unsigned char)(my_id + cmd_to_execute);

            /* --- BU?C 1: PH?N H?I ACK KH?NG V? MASTER --- */
            UART_SendByte(0xAA);
            UART_SendByte(my_id);
            UART_SendByte(cmd_to_execute);
            UART_SendByte(ack_checksum);

            /* --- BU?C 2: TH?C THI LOGIC --- */
            if (cmd_to_execute == 0x10) 
            {
                GPIOC_ODR |= (1 << 3);  
                delay_ms(500);         
                GPIOC_ODR &= ~(1 << 3); 
            }
            else if (cmd_to_execute == 0x20)
            {
                GPIOC_ODR |= (1 << 3);
            }
            else if (cmd_to_execute == 0x30)
            {
                GPIOC_ODR &= ~(1 << 3);
            }
            /* L?NH 0x40: ÐO VÀ TR? V? KHO?NG CÁCH */
            else if (cmd_to_execute == 0x40)
            {
                distance_cm = HCSR04_ReadDistance();
                
                /* G?i Byte Cao và Byte Th?p c?a kho?ng cách (cm) */
                UART_SendByte((unsigned char)(distance_cm >> 8));
                UART_SendByte((unsigned char)(distance_cm & 0xFF));
            }
        }
    }
}
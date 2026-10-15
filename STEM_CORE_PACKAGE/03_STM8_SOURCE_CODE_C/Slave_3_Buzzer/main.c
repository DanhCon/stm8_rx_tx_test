#define CLK_CKDIVR    *(volatile unsigned char*)(0x0050C6)

/* =========================================================
   GPIOC (CO C?U CH?P HÀNH - BUZZER T?I PC5)
========================================================= */
#define GPIOC_ODR     *(volatile unsigned char*)(0x00500A)
#define GPIOC_DDR     *(volatile unsigned char*)(0x00500C)
#define GPIOC_CR1     *(volatile unsigned char*)(0x00500D)

/* =========================================================
   UART1 THÀNH GHI
========================================================= */
#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)
#define UART1_BRR1    *(volatile unsigned char*)(0x005232)
#define UART1_BRR2    *(volatile unsigned char*)(0x005233)
#define UART1_CR2     *(volatile unsigned char*)(0x005235)

extern volatile unsigned char global_buz_cmd;

void delay_ms(unsigned int ms) {
    volatile unsigned int i, j;
    for(i = 0; i < ms; i++) {
        for(j = 0; j < 3000; j++); 
    }
}

/* Hàm truy?n UART Polling */
void UART_SendByte(unsigned char data) {
    while(!(UART1_SR & 0x80)); 
    UART1_DR = data;
}

void main(void)
{
    CLK_CKDIVR = 0x00; /* Max speed: 16MHz */

    /* ---------------------------------------------------
       KH?I T?O C?U HÌNH NGO?I VI (PC5 LÀ OUTPUT PUSH-PULL)
    --------------------------------------------------- */
    GPIOC_DDR |= (1 << 5);  /* DDR = 1 -> Chi?u ngõ ra */
    GPIOC_CR1 |= (1 << 5);  /* CR1 = 1 -> Ch? d? Push-Pull */
    GPIOC_ODR &= ~(1 << 5); /* M?c d?nh t?t còi (Kéo xu?ng 0V) */

    /* ---------------------------------------------------
       KH?I T?O UART1 (9600 bps @ 16MHz)
    --------------------------------------------------- */
    UART1_BRR2 = 0x03;
    UART1_BRR1 = 0x68;
    UART1_CR2 = 0x2C; /* TEN, REN, RIEN */

    _asm("rim");

    while(1)
    {
        /* B?t c? s? ki?n t? ISR */
        if (global_buz_cmd != 0x00) 
        {
            /* Chu?n khai báo C89 cho Cosmic Compiler */
            unsigned char cmd_to_execute;
            unsigned char my_id;
            unsigned char ack_checksum;

            /* L?y d? li?u và reset c? l?p t?c d? tránh l?i re-entrancy */
            cmd_to_execute = global_buz_cmd;
            global_buz_cmd = 0x00; 
            
            my_id = 0x03;
            ack_checksum = (unsigned char)(my_id + cmd_to_execute);

            /* --- BU?C 1: ÐÓNG GÓI VÀ G?I ACK V? MASTER --- */
            UART_SendByte(0xAA);
            UART_SendByte(my_id);
            UART_SendByte(cmd_to_execute);
            UART_SendByte(ack_checksum);

            /* --- BU?C 2: ÐI?U KHI?N ÐI?N ÁP V?T LÝ PC5 --- */
            if (cmd_to_execute == 0x42) 
            {
                /* L?nh Bíp báo hi?u k?t n?i h? th?ng */
                GPIOC_ODR |= (1 << 5);  
                delay_ms(300);         
                GPIOC_ODR &= ~(1 << 5); 
            }
            else if (cmd_to_execute == 0x40)
            {
                /* L?nh B?t Còi liên t?c */
                GPIOC_ODR |= (1 << 5);
            }
            else if (cmd_to_execute == 0x41)
            {
                /* L?nh T?t Còi */
                GPIOC_ODR &= ~(1 << 5);
            }
        }
    }
}
#define CLK_CKDIVR    *(volatile unsigned char*)(0x0050C6)

/* =========================================================
   GPIOC (CO C?U CH?P HÀNH - LED T?I PC3)
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

extern volatile unsigned char global_led_cmd;

void delay_ms(unsigned int ms) {
    volatile unsigned int i, j;
    for(i = 0; i < ms; i++) {
        for(j = 0; j < 3000; j++); 
    }
}

/* Hàm truy?n 1 Byte ch?n d?ng (Blocking TX) */
void UART_SendByte(unsigned char data) {
    while(!(UART1_SR & 0x80)); 
    UART1_DR = data;
}

void main(void)
{
    CLK_CKDIVR = 0x00; /* 16MHz */

    /* C?U HÌNH PC3 LÀ OUTPUT PUSH-PULL */
    GPIOC_DDR |= (1 << 3);  
    GPIOC_CR1 |= (1 << 3);  
    GPIOC_ODR &= ~(1 << 3); /* T?t LED ban d?u */

    /* C?U HÌNH UART1 (9600 bps @ 16MHz) */
    UART1_BRR2 = 0x03;
    UART1_BRR1 = 0x68;
    UART1_CR2 = 0x2C; /* B?t TEN, REN và RIEN */

    _asm("rim");

    while(1)
    {
        /* N?u có l?nh t? Master g?i xu?ng */
        if (global_led_cmd != 0x00) 
        {
            /* KHAI BÁO BI?N ? Ð?U BLOCK (CHU?N C89 CHO COSMIC) */
            unsigned char cmd_to_execute;
            unsigned char my_id;
            unsigned char ack_checksum;

            /* B?T Ð?U TH?C THI LOGIC */
            cmd_to_execute = global_led_cmd;
            global_led_cmd = 0x00; /* Xóa c? ngay l?p t?c */
            
            my_id = 0x01;
            ack_checksum = (unsigned char)(my_id + cmd_to_execute);

            /* --- BU?C 1: G?I ACK V? MASTER TRU?C --- 
               Ph?i g?i ngay d? ESP32 không b? Timeout (15ms) */
            UART_SendByte(0xAA);
            UART_SendByte(my_id);          /* Ðã s?a l?i gõ du d?u g?ch du?i */
            UART_SendByte(cmd_to_execute);
            UART_SendByte(ack_checksum);

            /* --- BU?C 2: TH?C THI V?T LÝ --- */
            if (cmd_to_execute == 0x10) 
            {
                /* L?nh Nháy LED báo k?t n?i */
                GPIOC_ODR |= (1 << 3);  
                delay_ms(1000);         
                GPIOC_ODR &= ~(1 << 3); 
            }
            else if (cmd_to_execute == 0x20)
            {
                /* L?nh B?t LED */
                GPIOC_ODR |= (1 << 3);
            }
            else if (cmd_to_execute == 0x30)
            {
                /* L?nh T?t LED */
                GPIOC_ODR &= ~(1 << 3);
            }
        }
    }
}
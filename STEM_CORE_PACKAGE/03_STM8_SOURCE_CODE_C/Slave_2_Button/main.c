#define CLK_CKDIVR    *(volatile unsigned char*)(0x0050C6)

/* =========================================================
   GPIOC (NÚT NH?N T?I PC4)
========================================================= */
#define GPIOC_IDR     *(volatile unsigned char*)(0x00500B)
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

extern volatile unsigned char global_send_btn;

/* Hàm truy?n 1 Byte ch?n d?ng (Blocking TX) */
void UART_SendByte(unsigned char data)
{
    while(!(UART1_SR & 0x80)); /* Ch? c? TXE (Transmit Data Register Empty) */
    UART1_DR = data;
}

void main(void)
{
    /* C?u hình xung nh?p CPU 16MHz không chia */
    CLK_CKDIVR = 0x00;

    /* ---------------------------------------------------
       KH?I T?O NÚT NH?N (PC4)
       - DDR = 0: Input Mode
       - CR1 = 1: Pull-up n?i b? (Tránh nhi?u di?n t? khi th? n?i)
    --------------------------------------------------- */
    GPIOC_DDR &= ~(1 << 4);
    GPIOC_CR1 |= (1 << 4);  

    /* ---------------------------------------------------
       KH?I T?O UART1 (9600 bps @ 16MHz)
    --------------------------------------------------- */
    UART1_BRR2 = 0x03;
    UART1_BRR1 = 0x68;
    
    /* B?t TEN (0x08), REN (0x04) và Ng?t Nh?n RIEN (0x20) */
    UART1_CR2 = 0x2C;

    /* M? c?ng ng?t toàn c?c ALU */
    _asm("rim");

    while(1)
    {
        /* Máy tr?ng thái Event-Driven ch? Master g?i h?n */
        if (global_send_btn == 1) 
        {
            /* 1. L?Y M?U NGO?I VI (SAMPLING) */
            /* M?ch Pull-up: Nh?n xu?ng = 0, Nh? ra = 1 */
            unsigned char is_pressed = ((GPIOC_IDR & (1 << 4)) == 0); 
            
            /* D?ch thành mã l?nh giao th?c Neuron Clone */
            unsigned char btn_cmd = is_pressed ? 0x51 : 0x50; 

            /* 2. ÐÓNG GÓI VÀ TÍNH TOÁN CHECKSUM CHO PH?N H?I */
            unsigned char my_id = 0x02;
            unsigned char calculated_crc = (unsigned char)(my_id + btn_cmd);

            /* 3. Ð?Y GÓI TIN LÊN BUS (UPSTREAM) */
            UART_SendByte(0xAA);           // Byte 1: Header
            UART_SendByte(my_id);          // Byte 2: ID c?a kh?i này
            UART_SendByte(btn_cmd);        // Byte 3: Tr?ng thái v?t lý
            UART_SendByte(calculated_crc); // Byte 4: Checksum Modulo-256

            /* Xóa c?, gi?i phóng CPU d? ng? dông ch? chu k? Polling 50ms ti?p theo */
            global_send_btn = 0; 
        }
    }
}
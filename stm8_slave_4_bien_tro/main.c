#define CLK_CKDIVR    *(volatile unsigned char*)(0x0050C6)

/* =========================================================
   THÀNH GHI ADC1 (Tuong t? s?)
========================================================= */
#define ADC_CSR       *(volatile unsigned char*)(0x005400)
#define ADC_CR1       *(volatile unsigned char*)(0x005401)
#define ADC_CR2       *(volatile unsigned char*)(0x005402)
#define ADC_DRH       *(volatile unsigned char*)(0x005404)
#define ADC_DRL       *(volatile unsigned char*)(0x005405)

/* =========================================================
   THÀNH GHI UART1
========================================================= */
#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)
#define UART1_BRR1    *(volatile unsigned char*)(0x005232)
#define UART1_BRR2    *(volatile unsigned char*)(0x005233)
#define UART1_CR2     *(volatile unsigned char*)(0x005235)

extern volatile unsigned char global_req_adc;

void UART_SendByte(unsigned char data) {
    while(!(UART1_SR & 0x80)); 
    UART1_DR = data;
}

void main(void)
{
    CLK_CKDIVR = 0x00; /* 16MHz */

    /* ---------------------------------------------------
       KH?I T?O ADC CHO CHÂN PD3 (KÊNH AIN4)
    --------------------------------------------------- */
    ADC_CSR = 0x04;    /* Ch?n kênh CH[3:0] = 0100 (AIN4) */
    ADC_CR2 = 0x00;    /* ALIGN = 0 (Left Alignment) -> Ép 8 bit cao vào DRH */
    ADC_CR1 |= 0x01;   /* ADON = 1: B?t ngu?n kh?i ADC l?n 1 (Wake-up) */
    
    /* ---------------------------------------------------
       KH?I T?O UART1 (9600 bps)
    --------------------------------------------------- */
    UART1_BRR2 = 0x03;
    UART1_BRR1 = 0x68;
    UART1_CR2 = 0x2C; /* B?t TEN, REN, RIEN */

    _asm("rim");

    while(1)
    {
        if (global_req_adc == 1) 
        {
            /* Chu?n C89 cho Cosmic */
            unsigned char adc_8bit;
            unsigned char my_id;
            unsigned char ack_checksum;

            global_req_adc = 0; /* Xóa c? ngay */

            /* 1. KÍCH HO?T L?Y M?U VÀ CH? K?T QU? (ADC CONVERSION) */
            ADC_CR1 |= 0x01; /* ADON = 1 l?n 2: Ra l?nh b?t d?u chuy?n d?i */
            while(!(ADC_CSR & 0x80)); /* Ch? c? EOC (End Of Conversion) lên 1 */
            ADC_CSR &= ~0x80; /* Xóa c? EOC b?ng ph?n m?m */

            /* Ð?c 8 bit cao t? DRH. ? ch? d? Left-Aligned, thao tác này t? d?ng v?t b? 2 bit LSB nhi?u */
            adc_8bit = ADC_DRH; 

            /* 2. ÐÓNG GÓI VÀ B?N PH?N H?I LÊN M?NG */
            my_id = 0x04;
            ack_checksum = (unsigned char)(my_id + adc_8bit);

            UART_SendByte(0xAA);
            UART_SendByte(my_id);
            UART_SendByte(adc_8bit);   /* Byte CMD lúc này mang giá tr? di?n áp (0-255) */
            UART_SendByte(ack_checksum);
        }
    }
}
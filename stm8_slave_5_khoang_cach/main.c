/* =========================================================
   STM8S103F3P6 - SLAVE 5 (HOAC SLAVE 4): CAM BIEN KHOANG CACH HC-SR04
   Trang bi: 
     - TRIG : PA3 (Output Push-Pull)
     - ECHO : PD2 (Input Floating)
     - LED  : PC3 (Output Push-Pull, Onboard Debug LED)
     - UART1: 9600 bps (TX=PD5, RX=PD6)
     - TIM2 : Do thoi gian Pulse (1 tick = 1us @ 16MHz)
   Giao thuc: 4-Byte Standard Ring Frame
     - Request  : [0xAA] [SLAVE_ID] [0x70] [CRC]
     - Response : [0xAA] [SLAVE_ID] [DIST_CM] [CRC]
========================================================= */

#define CLK_CKDIVR    *(volatile unsigned char*)(0x0050C6)

/* =========================================================
   GPIOC (LED TAI PC3)
========================================================= */
#define GPIOC_ODR      *(volatile unsigned char*)(0x00500A)
#define GPIOC_DDR      *(volatile unsigned char*)(0x00500C)
#define GPIOC_CR1      *(volatile unsigned char*)(0x00500D)

/* =========================================================
   GPIOA (HC-SR04 TRIG TAI PA3 - OUTPUT)
========================================================= */
#define GPIOA_ODR      *(volatile unsigned char*)(0x005000)
#define GPIOA_DDR      *(volatile unsigned char*)(0x005002)
#define GPIOA_CR1      *(volatile unsigned char*)(0x005003)

/* =========================================================
   GPIOD (HC-SR04 ECHO TAI PD2 - INPUT)
   Luu y dia chi chuan STM8S103:
     ODR = 0x500F, IDR = 0x5010, DDR = 0x5011, CR1 = 0x5012
========================================================= */
#define GPIOD_ODR      *(volatile unsigned char*)(0x00500F)
#define GPIOD_IDR      *(volatile unsigned char*)(0x005010)
#define GPIOD_DDR      *(volatile unsigned char*)(0x005011)
#define GPIOD_CR1      *(volatile unsigned char*)(0x005012)
#define GPIOD_CR2      *(volatile unsigned char*)(0x005013)

/* =========================================================
   UART1 (9600 BAUD @ 16MHz)
========================================================= */
#define UART1_SR       *(volatile unsigned char*)(0x005230)
#define UART1_DR       *(volatile unsigned char*)(0x005231)
#define UART1_BRR1     *(volatile unsigned char*)(0x005232)
#define UART1_BRR2     *(volatile unsigned char*)(0x005233)
#define UART1_CR2      *(volatile unsigned char*)(0x005235)

/* =========================================================
   TIM2 (DO THOI GIAN PULSE HC-SR04)
   Dia chi chuan RM0016 STM8S103:
     CR1:   0x5300
     EGR:   0x5304
     CNTRH: 0x530A
     CNTRL: 0x530B
     PSCR:  0x530C
     ARRH:  0x530D
     ARRL:  0x530E
========================================================= */
#define TIM2_CR1       *(volatile unsigned char*)(0x005300)
#define TIM2_EGR       *(volatile unsigned char*)(0x005304)
#define TIM2_CNTRH     *(volatile unsigned char*)(0x00530A)
#define TIM2_CNTRL     *(volatile unsigned char*)(0x00530B)
#define TIM2_PSCR      *(volatile unsigned char*)(0x00530C)
#define TIM2_ARRH      *(volatile unsigned char*)(0x00530D)
#define TIM2_ARRL      *(volatile unsigned char*)(0x00530E)

/* Bien toan cuc chia se tu ISR ngat UART */
extern volatile unsigned char global_req_dist;
extern volatile unsigned char global_slave_id;

/* Ham delay microsecond bang NOP */
void delay_us(unsigned char us) {
    volatile unsigned char i;
    for (i = 0; i < us; i++) {
        _asm("nop");
        _asm("nop");
        _asm("nop");
        _asm("nop");
    }
}

/* Ham truyen 1 Byte (Blocking TX) */
void UART_SendByte(unsigned char data) {
    while(!(UART1_SR & 0x80)); 
    UART1_DR = data;
}

/* Khoi tao TIM2: Prescaler 16 -> 1 tick = 1us @ 16MHz */
void TIM2_Init(void) {
    TIM2_PSCR = 0x04; /* 16MHz / 2^4 = 1MHz (1us/tick) */
    TIM2_ARRH = 0xFF; /* Auto reload max 65535us (~65.5ms) */
    TIM2_ARRL = 0xFF;
    TIM2_EGR  = 0x01; /* Kich hoat UG de nap prescaler ngay */
    TIM2_CR1 |= 0x01; /* Bat Counter CEN=1 */
}

/* Do khoang cach HC-SR04 (Don vi: cm, toi da 255cm) */
unsigned int HCSR04_ReadDistance(void) {
    unsigned int echo_time;
    unsigned int timeout;
    unsigned char cnt_h;
    unsigned char cnt_l;

    /* 0. Cho chan ECHO xuong LOW truoc khi ban xung (tranh loi ket) */
    timeout = 10000;
    while ((GPIOD_IDR & (1 << 2)) && timeout) {
        timeout--;
    }

    /* 1. Tao xung TRIG 15us tai PA3 */
    GPIOA_ODR &= ~(1 << 3);
    delay_us(2);
    GPIOA_ODR |= (1 << 3);
    delay_us(15);
    GPIOA_ODR &= ~(1 << 3);

    /* 2. Cho chan ECHO (PD2) len muc HIGH */
    timeout = 10000;
    while (!(GPIOD_IDR & (1 << 2))) {
        timeout--;
        if (timeout == 0) {
            return 255; /* Timeout: Khong thay phan hoi tu cam bien */
        }
    }

    /* 3. Reset bo dem TIM2 ve 0 */
    TIM2_CNTRH = 0x00;
    TIM2_CNTRL = 0x00;

    /* 4. Cho chan ECHO (PD2) xuong muc LOW */
    /* Gioi han thoi gian toi da: CNTRH > 120 tuc ~30.7ms (~5.3m) */
    while (GPIOD_IDR & (1 << 2)) {
        if (TIM2_CNTRH > 120) {
            break; /* Timeout ngoai tam do */
        }
    }

    /* 5. Doc gia tri Timer (Bat buoc doc HIGH truoc, LOW sau) */
    cnt_h = TIM2_CNTRH;
    cnt_l = TIM2_CNTRL;
    echo_time = ((unsigned int)cnt_h << 8) | cnt_l;

    /* 6. Tinh khoang cach: Distance (cm) = echo_time / 58 */
    if (echo_time < 116) {
        return 0; /* Duoi 2cm: qua gan hoac nhieu */
    }

    return (echo_time / 58);
}

void main(void)
{
    CLK_CKDIVR = 0x00; /* Master Clock = 16MHz HSI */

    /* Cau hinh PC3 - Output Push-Pull (LED Onboard) */
    GPIOC_DDR |= (1 << 3);  
    GPIOC_CR1 |= (1 << 3);  
    GPIOC_ODR &= ~(1 << 3); 

    /* Cau hinh PA3 - Output Push-Pull (TRIG) */
    GPIOA_DDR |= (1 << 3);
    GPIOA_CR1 |= (1 << 3);
    GPIOA_ODR &= ~(1 << 3);

    /* Cau hinh PD2 - Input Floating (ECHO) */
    GPIOD_DDR &= ~(1 << 2);
    GPIOD_CR1 &= ~(1 << 2);
    GPIOD_CR2 &= ~(1 << 2);

    /* Cau hinh UART1 (9600 bps @ 16MHz) - Ghi BRR2 truoc, BRR1 sau */
    UART1_BRR2 = 0x03;
    UART1_BRR1 = 0x68;
    UART1_CR2  = 0x2C; /* TEN=1, REN=1, RIEN=1 */

    TIM2_Init();

    _asm("rim"); /* Bat ngat toan cuc */

    while(1)
    {
        if (global_req_dist == 1) 
        {
            unsigned char my_id;
            unsigned int dist_val;
            unsigned char dist_byte;
            unsigned char ack_checksum;

            global_req_dist = 0; /* Xoa co ngay */
            
            my_id = global_slave_id;
            if (my_id == 0) my_id = 0x05; /* Mac dinh Node 5 */

            /* Doc cam bien khoang cach */
            dist_val = HCSR04_ReadDistance();
            if (dist_val > 255) {
                dist_byte = 255;
            } else {
                dist_byte = (unsigned char)dist_val;
            }

            /* Checksum = (ID + DATA) % 256 */
            ack_checksum = (unsigned char)(my_id + dist_byte);

            /* Nhay dao trang thai LED PC3 bao hieu vua do xong */
            GPIOC_ODR ^= (1 << 3);

            /* Dong goi va gui phan hoi chuan 4 Byte len Ring */
            UART_SendByte(0xAA);
            UART_SendByte(my_id);
            UART_SendByte(dist_byte);
            UART_SendByte(ack_checksum);
        }
    }
}

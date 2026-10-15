/* =========================================================
   STM8S103F3 / STM8S003F3 - SLAVE 6: MODULE MAN HINH LCD 1602 I2C
   =========================================================
   - Ket noi phan cung:
     + I2C SCL : PB4 (Noi chan SCL cua module I2C LCD)
     + I2C SDA : PB5 (Noi chan SDA cua module I2C LCD)
     + Nguon   : 5V va GND (LCD 1602 can nguon 5V de hien thi ro net)
     + UART1   : 9600 bps (TX = PD5, RX = PD6)
   - Tu dong nhan dien dia chi I2C cua PCF8574:
     + 0x4E (PCF8574T - 0x27) hoac 0x7E (PCF8574AT - 0x3F)
   - Ho tro:
     + Nhan chuoi van ban tuy y tu App (16 ky tu/dong)
     + Nhan lenh dieu khien mau (Clear, ON, OFF, Event...)
   ========================================================= */

#define CLK_CKDIVR    *(volatile unsigned char*)(0x0050C6)

/* =========================================================
   GPIOB (I2C CHO LCD1602: SCL = PB4, SDA = PB5)
========================================================= */
#define GPIOB_ODR     *(volatile unsigned char*)(0x005005)
#define GPIOB_IDR     *(volatile unsigned char*)(0x005006)
#define GPIOB_DDR     *(volatile unsigned char*)(0x005007)
#define GPIOB_CR1     *(volatile unsigned char*)(0x005008)
#define GPIOB_CR2     *(volatile unsigned char*)(0x005009)

/* =========================================================
   UART1 THANH GHI
========================================================= */
#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)
#define UART1_BRR1    *(volatile unsigned char*)(0x005232)
#define UART1_BRR2    *(volatile unsigned char*)(0x005233)
#define UART1_CR2     *(volatile unsigned char*)(0x005235)

extern volatile unsigned char global_lcd_cmd;
extern volatile unsigned char global_lcd_event;
extern volatile unsigned char global_lcd_line;
extern volatile unsigned char global_lcd_buffer[17];
extern volatile unsigned char global_lcd_text_ready;

/* Dia chi I2C cua chip PCF8574 (Mac dinh 0x4E, se tu dong scan sang 0x7E neu can) */
unsigned char LCD_I2C_ADDR = 0x4E; 

/* =========================================================
   CAC HAM DELAY
========================================================= */
void delay_us(unsigned char us) {
    volatile unsigned char i;
    for (i = 0; i < us; i++) {
        _asm("nop");
        _asm("nop");
        _asm("nop");
        _asm("nop");
    }
}

void delay_ms(unsigned int ms) {
    volatile unsigned int i, j;
    for(i = 0; i < ms; i++) {
        for(j = 0; j < 3000; j++); 
    }
}

/* =========================================================
   DRIVER SOFTWARE I2C (BIT-BANGING TRUONG TON KHONG BAO GIO TREO)
========================================================= */
void I2C_SCL_HIGH(void) { GPIOB_ODR |= (1 << 4); delay_us(6); }
void I2C_SCL_LOW(void)  { GPIOB_ODR &= ~(1 << 4); delay_us(6); }
void I2C_SDA_HIGH(void) { GPIOB_ODR |= (1 << 5); delay_us(6); }
void I2C_SDA_LOW(void)  { GPIOB_ODR &= ~(1 << 5); delay_us(6); }
unsigned char I2C_SDA_READ(void) { return (GPIOB_IDR & (1 << 5)) ? 1 : 0; }

void I2C_Init(void) {
    GPIOB_DDR |= (1 << 4) | (1 << 5);
    GPIOB_CR1 |= (1 << 4) | (1 << 5); /* Push-pull cho xung sac net */
    I2C_SDA_HIGH();
    I2C_SCL_HIGH();
}

void I2C_Start(void) {
    I2C_SDA_HIGH();
    I2C_SCL_HIGH();
    I2C_SDA_LOW();
    I2C_SCL_LOW();
}

void I2C_Stop(void) {
    I2C_SDA_LOW();
    I2C_SCL_HIGH();
    I2C_SDA_HIGH();
}

unsigned char I2C_WriteByte(unsigned char data) {
    unsigned char i, ack;
    for (i = 0; i < 8; i++) {
        if (data & 0x80) I2C_SDA_HIGH();
        else I2C_SDA_LOW();
        data <<= 1;
        I2C_SCL_HIGH();
        I2C_SCL_LOW();
    }
    /* Doc xung ACK tu PCF8574 */
    I2C_SDA_HIGH();
    GPIOB_DDR &= ~(1 << 5); /* Chuyen SDA sang Input */
    I2C_SCL_HIGH();
    ack = I2C_SDA_READ();
    I2C_SCL_LOW();
    GPIOB_DDR |= (1 << 5);  /* Tra SDA ve Output */
    return (ack == 0) ? 1 : 0; /* ack=0 tuc ACK thanh cong */
}

/* =========================================================
   DRIVER LCD 1602 GIAO TIEP PCF8574 (CHE DO 4-BIT)
========================================================= */
#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE    0x04

void LCD_Write_I2C(unsigned char val) {
    I2C_Start();
    I2C_WriteByte(LCD_I2C_ADDR);
    I2C_WriteByte(val);
    I2C_Stop();
}

void LCD_Send_Nibble(unsigned char nibble, unsigned char rs) {
    unsigned char data = (nibble & 0xF0) | LCD_BACKLIGHT | (rs ? 0x01 : 0x00);
    
    LCD_Write_I2C(data | LCD_ENABLE);
    delay_us(10);
    LCD_Write_I2C(data & ~LCD_ENABLE);
    delay_us(40);
}

void LCD_Send_Byte(unsigned char byte, unsigned char rs) {
    LCD_Send_Nibble(byte & 0xF0, rs);
    LCD_Send_Nibble((byte << 4) & 0xF0, rs);
}

void LCD_Command(unsigned char cmd) {
    LCD_Send_Byte(cmd, 0);
    if (cmd == 0x01 || cmd == 0x02) delay_ms(2);
}

void LCD_Data(unsigned char dat) {
    LCD_Send_Byte(dat, 1);
}

void LCD_Clear(void) {
    LCD_Command(0x01);
    delay_ms(2);
}

void LCD_SetCursor(unsigned char row, unsigned char col) {
    unsigned char addr = (row == 0) ? (0x80 + col) : (0xC0 + col);
    LCD_Command(addr);
}

void LCD_Print(const char *str) {
    while (*str) {
        LCD_Data((unsigned char)(*str++));
    }
}

void LCD_Init(void) {
    I2C_Init();
    delay_ms(50); /* Cho nguon LCD on dinh */

    /* Tu dong do dia chi PCF8574 */
    I2C_Start();
    if (!I2C_WriteByte(0x4E)) {
        LCD_I2C_ADDR = 0x7E; /* Chip PCF8574AT */
    } else {
        LCD_I2C_ADDR = 0x4E; /* Chip PCF8574T */
    }
    I2C_Stop();

    /* Chuoi khoi tao 4-bit HD44780 chuan */
    LCD_Send_Nibble(0x30, 0);
    delay_ms(5);
    LCD_Send_Nibble(0x30, 0);
    delay_us(150);
    LCD_Send_Nibble(0x30, 0);
    delay_ms(1);
    LCD_Send_Nibble(0x20, 0); /* Chuyen sang 4-bit */
    delay_ms(1);

    LCD_Command(0x28); /* 4-bit, 2 dong, 5x8 font */
    LCD_Command(0x0C); /* Bat hien thi, tat con tro */
    LCD_Command(0x06); /* Tu dong tang con tro */
    LCD_Clear();
}

/* =========================================================
   UART1 TRANSMIT (BLOCKING TX)
========================================================= */
void UART_SendByte(unsigned char data) {
    while(!(UART1_SR & 0x80)); 
    UART1_DR = data;
}

void main(void)
{
    CLK_CKDIVR = 0x00; /* 16MHz */

    /* KHOI TAO UART1 (9600 bps @ 16MHz) */
    UART1_BRR2 = 0x03;
    UART1_BRR1 = 0x68;
    UART1_CR2  = 0x2C; /* TEN, REN, RIEN */

    /* KHOI TAO MAN HINH LCD 1602 */
    LCD_Init();

    /* HIEN THI MAN HINH CHAO MUNG BAN DAU */
    LCD_SetCursor(0, 0);
    LCD_Print(" FABLAB NEURON  ");
    LCD_SetCursor(1, 0);
    LCD_Print(" NODE 6: LCD OK ");

    _asm("rim"); /* Bat ngat toan cuc */

    while(1)
    {
        /* 1. XU LY GOI TEXT TUY Y (16 KY TU TRUC TIEP TU APP) */
        if (global_lcd_text_ready == 1)
        {
            unsigned char line = global_lcd_line;
            unsigned char ack_cmd = (line == 1) ? 0x11 : 0x12;
            unsigned char ack_crc = (unsigned char)(0x06 + ack_cmd);
            
            global_lcd_text_ready = 0;

            /* Ban ACK 4-byte ve Master de xac nhan da nhan xong */
            UART_SendByte(0xAA);
            UART_SendByte(0x06);
            UART_SendByte(ack_cmd);
            UART_SendByte(ack_crc);

            /* In thang 16 ky tu ra dong tuong ung */
            LCD_SetCursor(line - 1, 0);
            LCD_Print((const char*)global_lcd_buffer);
        }

        /* 2. XU LY LENH MAU DON GIAN TU MASTER ESP32 */
        if (global_lcd_event == 1) 
        {
            unsigned char cmd_to_execute;
            unsigned char my_id;
            unsigned char ack_checksum;

            cmd_to_execute = global_lcd_cmd;
            global_lcd_event = 0; 
            
            my_id = 0x06;
            ack_checksum = (unsigned char)(my_id + cmd_to_execute);

            /* Ban ACK ve Master de xac nhan Online */
            UART_SendByte(0xAA);
            UART_SendByte(my_id);
            UART_SendByte(cmd_to_execute);
            UART_SendByte(ack_checksum);

            /* Cap nhat man hinh theo lenh */
            if (cmd_to_execute == 0x01) 
            {
                LCD_Clear();
            }
            else if (cmd_to_execute == 0x10)
            {
                LCD_SetCursor(0, 0);
                LCD_Print("  FABLAB  STEM  ");
                LCD_SetCursor(1, 0);
                LCD_Print("  SYSTEM READY  ");
            }
            else if (cmd_to_execute == 0x20)
            {
                LCD_SetCursor(0, 0);
                LCD_Print("THIET BI NGOAI: ");
                LCD_SetCursor(1, 0);
                LCD_Print("DEN LED: [ON]   ");
            }
            else if (cmd_to_execute == 0x30)
            {
                LCD_SetCursor(0, 0);
                LCD_Print("THIET BI NGOAI: ");
                LCD_SetCursor(1, 0);
                LCD_Print("DEN LED: [OFF]  ");
            }
            else if (cmd_to_execute == 0x51)
            {
                LCD_SetCursor(0, 0);
                LCD_Print("CANH BAO SU KIEN");
                LCD_SetCursor(1, 0);
                LCD_Print("NUT BAM: NHAN!  ");
            }
            else if (cmd_to_execute == 0x50)
            {
                LCD_SetCursor(0, 0);
                LCD_Print("CANH BAO SU KIEN");
                LCD_SetCursor(1, 0);
                LCD_Print("NUT BAM: NHA    ");
            }
            else if (cmd_to_execute >= 0x80 && cmd_to_execute <= 0xE4)
            {
                unsigned char percent = (unsigned char)(cmd_to_execute - 0x80);
                char buf[17];
                LCD_SetCursor(0, 0);
                LCD_Print("DIEU KHIEN SANG:");
                LCD_SetCursor(1, 0);
                buf[0] = 'M'; buf[1] = 'U'; buf[2] = 'C'; buf[3] = ':'; buf[4] = ' ';
                buf[5] = (percent / 100) ? ('0' + (percent / 100)) : ' ';
                buf[6] = (percent >= 10) ? ('0' + ((percent % 100) / 10)) : ' ';
                buf[7] = '0' + (percent % 10);
                buf[8] = '%'; buf[9] = ' '; buf[10] = ' '; buf[11] = ' ';
                buf[12] = ' '; buf[13] = ' '; buf[14] = ' '; buf[15] = ' '; buf[16] = '\0';
                LCD_Print(buf);
            }
        }
    }
}

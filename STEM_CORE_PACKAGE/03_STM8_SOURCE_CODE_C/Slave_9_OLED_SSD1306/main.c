/* =========================================================
   STM8S103F3P6 - SLAVE 9: MODULE MÀN HÌNH OLED 0.96" (SSD1306 I2C)
   =========================================================
   - Chân kết nối phần cứng:
     + I2C SCL : PB4 (Chân số 12 - Open-drain)
     + I2C SDA : PB5 (Chân số 11 - Open-drain)
     + UART1 TX: PD5 (Chân số 2)
     + UART1 RX: PD6 (Chân số 3)
     + Nguồn   : VCC (3.3V hoặc 5V) và GND
   - Độ phân giải: 128 x 64 Pixels
   - Tự động nhận diện địa chỉ I2C: 0x3C (0x78) hoặc 0x3D (0x7A)
   - Bộ quản lý hiển thị Direct Streaming:
     + Không dùng 1KB RAM Frame Buffer (phù hợp chip 1KB RAM)
     + Hỗ trợ Font to 8x16 (4 dòng x 16 ký tự, dễ đọc)
     + Hỗ trợ Font nhỏ 6x8 (8 dòng x 16-21 ký tự)
   - Tốc độ UART: 9600 bps, 8N1
   ========================================================= */

#define CLK_CKDIVR    *(volatile unsigned char*)(0x0050C6)

/* =========================================================
   GPIOB (I2C CHO OLED SSD1306: SCL = PB4, SDA = PB5)
========================================================= */
#define GPIOB_ODR     *(volatile unsigned char*)(0x005005)
#define GPIOB_IDR     *(volatile unsigned char*)(0x005006)
#define GPIOB_DDR     *(volatile unsigned char*)(0x005007)
#define GPIOB_CR1     *(volatile unsigned char*)(0x005008)
#define GPIOB_CR2     *(volatile unsigned char*)(0x005009)

/* =========================================================
   UART1 THANH GHI (9600 BAUD @ 16MHz)
========================================================= */
#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)
#define UART1_BRR1    *(volatile unsigned char*)(0x005232)
#define UART1_BRR2    *(volatile unsigned char*)(0x005233)
#define UART1_CR2     *(volatile unsigned char*)(0x005235)

#define SLAVE_ID      0x09

#include "ssd1306_fonts.h"

/* Biến nhận diện lệnh từ ngắt UART */
extern volatile unsigned char global_oled_cmd;
extern volatile unsigned char global_oled_event;
extern volatile unsigned char global_oled_line_cmd;
extern volatile unsigned char global_oled_buffer[17];
extern volatile unsigned char global_oled_text_ready;

/* Địa chỉ ghi I2C thực tế (0x78 cho 0x3C hoặc 0x7A cho 0x3D) */
static unsigned char oled_i2c_addr = 0x78; 
static unsigned char is_inverted = 0;

void delay_ms(unsigned int ms) {
    volatile unsigned int i, j;
    for(i = 0; i < ms; i++) {
        for(j = 0; j < 3000; j++); 
    }
}

static void i2c_delay(void) {
    volatile unsigned char d;
    for(d = 0; d < 3; d++);
}

/* =========================================================
   GIAO DIỆN SOFTWARE I2C (OPEN-DRAIN TRÊN PB4 & PB5)
========================================================= */
static void I2C_SCL_High(void) { GPIOB_ODR |= (1 << 4); }
static void I2C_SCL_Low(void)  { GPIOB_ODR &= ~(1 << 4); }
static void I2C_SDA_High(void) { GPIOB_ODR |= (1 << 5); }
static void I2C_SDA_Low(void)  { GPIOB_ODR &= ~(1 << 5); }
static unsigned char I2C_SDA_Read(void) { return (GPIOB_IDR & (1 << 5)) ? 1 : 0; }

void I2C_Init(void) {
    /* PB4 & PB5 trên STM8S103 là chân True Open-Drain phần cứng */
    GPIOB_DDR |= (1 << 4) | (1 << 5); /* Output */
    GPIOB_CR1 &= ~((1 << 4) | (1 << 5)); /* Open-Drain */
    GPIOB_CR2 |= (1 << 4) | (1 << 5); /* Tốc độ cao 10MHz */
    
    I2C_SCL_High();
    I2C_SDA_High();
}

void I2C_Start(void) {
    I2C_SDA_High();
    I2C_SCL_High();
    i2c_delay();
    I2C_SDA_Low();
    i2c_delay();
    I2C_SCL_Low();
    i2c_delay();
}

void I2C_Stop(void) {
    I2C_SDA_Low();
    I2C_SCL_Low();
    i2c_delay();
    I2C_SCL_High();
    i2c_delay();
    I2C_SDA_High();
    i2c_delay();
}

unsigned char I2C_WriteByte(unsigned char data) {
    unsigned char i;
    unsigned char ack;
    
    for(i = 0; i < 8; i++) {
        if(data & 0x80) I2C_SDA_High();
        else            I2C_SDA_Low();
        i2c_delay();
        
        I2C_SCL_High();
        i2c_delay();
        I2C_SCL_Low();
        i2c_delay();
        
        data <<= 1;
    }
    
    /* Đọc xung ACK từ OLED */
    I2C_SDA_High();
    i2c_delay();
    I2C_SCL_High();
    i2c_delay();
    ack = I2C_SDA_Read();
    I2C_SCL_Low();
    i2c_delay();
    
    return (ack == 0) ? 1 : 0; /* 1 = ACK thành công */
}

/* =========================================================
   DRIVER ĐIỀU KHIỂN SSD1306
========================================================= */
void OLED_WriteCmd(unsigned char cmd) {
    I2C_Start();
    I2C_WriteByte(oled_i2c_addr);
    I2C_WriteByte(0x00); /* 0x00 = Control byte báo lệnh (Command) */
    I2C_WriteByte(cmd);
    I2C_Stop();
}

void OLED_WriteData(unsigned char dat) {
    I2C_Start();
    I2C_WriteByte(oled_i2c_addr);
    I2C_WriteByte(0x40); /* 0x40 = Control byte báo dữ liệu (Data) */
    I2C_WriteByte(dat);
    I2C_Stop();
}

/* Đặt con trỏ trang (page 0-7) và cột (col 0-127) */
void OLED_SetCursor(unsigned char page, unsigned char col) {
    if (page > 7) page = 7;
    if (col > 127) col = 127;
    
    I2C_Start();
    I2C_WriteByte(oled_i2c_addr);
    I2C_WriteByte(0x00);
    I2C_WriteByte(0xB0 | page);
    I2C_WriteByte(0x00 | (col & 0x0F));
    I2C_WriteByte(0x10 | ((col >> 4) & 0x0F));
    I2C_Stop();
}

/* Xóa sạch toàn bộ màn hình */
void OLED_Clear(void) {
    unsigned char page;
    unsigned char col;
    
    for(page = 0; page < 8; page++) {
        OLED_SetCursor(page, 0);
        I2C_Start();
        I2C_WriteByte(oled_i2c_addr);
        I2C_WriteByte(0x40);
        for(col = 0; col < 128; col++) {
            I2C_WriteByte(0x00);
        }
        I2C_Stop();
    }
}

/* Xóa riêng 1 trang đơn (1 dòng 6x8) */
void OLED_ClearPage(unsigned char page) {
    unsigned char col;
    if(page > 7) return;
    OLED_SetCursor(page, 0);
    I2C_Start();
    I2C_WriteByte(oled_i2c_addr);
    I2C_WriteByte(0x40);
    for(col = 0; col < 128; col++) {
        I2C_WriteByte(0x00);
    }
    I2C_Stop();
}

/* Khởi tạo màn hình OLED SSD1306 */
void OLED_Init(void) {
    /* Quét kiểm tra địa chỉ I2C: 0x78 (0x3C) trước, nếu không được thử 0x7A (0x3D) */
    I2C_Start();
    if(I2C_WriteByte(0x78)) {
        oled_i2c_addr = 0x78;
    } else {
        oled_i2c_addr = 0x7A;
    }
    I2C_Stop();

    delay_ms(20);

    /* Chuỗi lệnh khởi tạo chuẩn SSD1306 128x64 */
    OLED_WriteCmd(0xAE); /* Tắt hiển thị Display OFF */
    OLED_WriteCmd(0xD5); /* Set Display Clock Divide */
    OLED_WriteCmd(0x80);
    OLED_WriteCmd(0xA8); /* Multiplex Ratio 64 */
    OLED_WriteCmd(0x3F);
    OLED_WriteCmd(0xD3); /* Display Offset 0 */
    OLED_WriteCmd(0x00);
    OLED_WriteCmd(0x40); /* Start Line 0 */
    OLED_WriteCmd(0x8D); /* Charge Pump */
    OLED_WriteCmd(0x14); /* Bật Charge Pump nội */
    OLED_WriteCmd(0x20); /* Memory Addressing Mode */
    OLED_WriteCmd(0x02); /* Chế độ Page Addressing Mode */
    OLED_WriteCmd(0xA1); /* Segment Re-map */
    OLED_WriteCmd(0xC8); /* COM Output Scan Direction */
    OLED_WriteCmd(0xDA); /* COM Pins Config */
    OLED_WriteCmd(0x12);
    OLED_WriteCmd(0x81); /* Contrast Control */
    OLED_WriteCmd(0xCF);
    OLED_WriteCmd(0xD9); /* Pre-charge Period */
    OLED_WriteCmd(0xF1);
    OLED_WriteCmd(0xDB); /* VCOMH Deselect */
    OLED_WriteCmd(0x40);
    OLED_WriteCmd(0xA4); /* Output follows RAM content */
    OLED_WriteCmd(0xA6); /* Normal Display (Không đảo màu) */
    OLED_WriteCmd(0xAF); /* Bật hiển thị Display ON */

    OLED_Clear();
}

/* In chuỗi văn bản Font 6x8 (line 0-7, tối đa 21 ký tự = 126 pixel + 2 pixel đệm)
   Tối ưu hóa Burst Streaming: Chỉ Start/Stop I2C đúng 1 lần cho cả dòng */
void OLED_PrintStr6x8(unsigned char line, unsigned char col, const char* str) {
    unsigned char i;
    unsigned char b;
    unsigned char c;
    unsigned char len;

    if (line > 7) line = 7;

    len = 0;
    while(str[len] != '\0' && len < 21) {
        len++;
    }

    OLED_SetCursor(line, col);

    I2C_Start();
    I2C_WriteByte(oled_i2c_addr);
    I2C_WriteByte(0x40); /* Control byte: Data stream */
    for(i = 0; i < 21; i++) {
        c = (i < len) ? (unsigned char)str[i] : ' ';
        if (c < 32 || c > 126) c = ' ';

        for(b = 0; b < 5; b++) {
            I2C_WriteByte(font_6x8[c - 32][b]);
        }
        I2C_WriteByte(0x00); /* 1 cột trống ngăn cách giữa 2 ký tự */
    }
    I2C_WriteByte(0x00); /* 2 pixel đệm cuối trang để đủ 128 pixel */
    I2C_WriteByte(0x00);
    I2C_Stop();
}

/* In chuỗi văn bản Font 8x16 (line 0-3, tối đa 16 ký tự = 128 pixel toàn màn hình)
   Tối ưu hóa Burst Streaming: Chỉ Start/Stop I2C đúng 1 lần cho nửa trên và 1 lần cho nửa dưới.
   Tự động xóa sạch khoảng trống đuôi dòng nếu chuỗi ngắn hơn 16 ký tự. */
void OLED_PrintStr8x16(unsigned char line, unsigned char col, const char* str) {
    unsigned char i;
    unsigned char b;
    unsigned char c;
    unsigned char top_page;
    unsigned char bot_page;
    unsigned char len;

    if (line > 3) line = 3;
    top_page = line * 2;
    bot_page = top_page + 1;

    /* Xác định độ dài chuỗi (tối đa 16 ký tự) */
    len = 0;
    while(str[len] != '\0' && len < 16) {
        len++;
    }

    /* 1. In nửa trên (Page top_page) - Burst stream liên tục 128 byte */
    OLED_SetCursor(top_page, col);
    I2C_Start();
    I2C_WriteByte(oled_i2c_addr);
    I2C_WriteByte(0x40); /* Control byte: Data stream */
    for(i = 0; i < 16; i++) {
        c = (i < len) ? (unsigned char)str[i] : ' ';
        if (c < 32 || c > 126) c = ' ';

        for(b = 0; b < 8; b++) {
            I2C_WriteByte(font_8x16[c - 32][b]);
        }
    }
    I2C_Stop();

    /* 2. In nửa dưới (Page bot_page) - Burst stream liên tục 128 byte */
    OLED_SetCursor(bot_page, col);
    I2C_Start();
    I2C_WriteByte(oled_i2c_addr);
    I2C_WriteByte(0x40); /* Control byte: Data stream */
    for(i = 0; i < 16; i++) {
        c = (i < len) ? (unsigned char)str[i] : ' ';
        if (c < 32 || c > 126) c = ' ';

        for(b = 8; b < 16; b++) {
            I2C_WriteByte(font_8x16[c - 32][b]);
        }
    }
    I2C_Stop();
}

/* Hiển thị màn hình chào mừng FabLab STEM ban đầu */
void OLED_ShowSplash(void) {
    OLED_Clear();
    OLED_PrintStr8x16(0, 0, "[ FABLAB  STEM ]");
    OLED_PrintStr8x16(1, 0, "OLED 0.96 READY ");
    OLED_PrintStr8x16(2, 0, (oled_i2c_addr == 0x78) ? "I2C: 0x3C OK    " : "I2C: 0x3D OK    ");
    OLED_PrintStr8x16(3, 0, "Ring Slave 0x09 ");
}

/* Hàm truyền 1 Byte UART chặn dòng */
void UART_SendByte(unsigned char data) {
    while(!(UART1_SR & 0x80)); 
    UART1_DR = data;
}

void main(void)
{
    /* Cấu hình xung nhịp CPU 16MHz không chia */
    CLK_CKDIVR = 0x00; 

    /* Khởi tạo I2C phần mềm cho OLED */
    I2C_Init();

    /* Khởi tạo màn hình OLED SSD1306 */
    OLED_Init();

    /* Hiển thị màn hình chào mừng ban đầu */
    OLED_ShowSplash();

    /* Cấu hình UART1 (9600 bps @ 16MHz) */
    UART1_BRR2 = 0x03;
    UART1_BRR1 = 0x68;
    UART1_CR2  = 0x2C; /* Bật TEN, REN và ngắt RIEN */

    /* Cho phép ngắt toàn cục */
    _asm("rim");

    while(1)
    {
        /* 1. XỬ LÝ LỆNH GỬI CHUỖI TEXT (FRAME 20-BYTE) */
        if (global_oled_text_ready) 
        {
            unsigned char line_cmd;
            unsigned char ack_byte;
            unsigned char checksum;

            line_cmd = global_oled_line_cmd;
            global_oled_text_ready = 0;

            /* Xử lý hiển thị theo Font to (8x16) hoặc Font nhỏ (6x8) */
            if (line_cmd >= 0x10 && line_cmd <= 0x13) {
                /* FONT TO 8x16: 4 dòng (0x10 -> Dòng 0, 0x11 -> Dòng 1, ...) */
                unsigned char line_idx;
                line_idx = line_cmd - 0x10;
                OLED_PrintStr8x16(line_idx, 0, (const char*)global_oled_buffer);
                ack_byte = line_cmd;
            }
            else if (line_cmd >= 0x20 && line_cmd <= 0x27) {
                /* FONT NHỎ 6x8: 8 dòng (0x20 -> Dòng 0, 0x21 -> Dòng 1, ...) */
                unsigned char line_idx;
                line_idx = line_cmd - 0x20;
                OLED_ClearPage(line_idx);
                OLED_PrintStr6x8(line_idx, 0, (const char*)global_oled_buffer);
                ack_byte = line_cmd;
            }
            else {
                ack_byte = line_cmd;
            }

            /* Nghỉ 2ms để xả bộ đệm mạch Repeater trên vòng Ring */
            delay_ms(2);

            /* Gửi phản hồi ACK xác nhận đã in */
            checksum = (unsigned char)(SLAVE_ID + ack_byte);
            UART_SendByte(0xAA);
            UART_SendByte(SLAVE_ID);
            UART_SendByte(ack_byte);
            UART_SendByte(checksum);
        }

        /* 2. XỬ LÝ LỆNH NHANH (FRAME 4-BYTE) */
        if (global_oled_event) 
        {
            unsigned char cmd;
            unsigned char ack_byte;
            unsigned char checksum;

            cmd = global_oled_cmd;
            global_oled_event = 0;

            if (cmd == 0x80) {
                OLED_WriteCmd(0xAF); /* BẬT MÀN HÌNH */
                ack_byte = 0x80;
            }
            else if (cmd == 0x81) {
                OLED_WriteCmd(0xAE); /* TẮT MÀN HÌNH */
                ack_byte = 0x81;
            }
            else if (cmd == 0x82) {
                OLED_Clear();        /* XÓA MÀN HÌNH */
                ack_byte = 0x82;
            }
            else if (cmd == 0x83) {
                /* ĐẢO MÀU MÀN HÌNH */
                is_inverted = !is_inverted;
                OLED_WriteCmd(is_inverted ? 0xA7 : 0xA6);
                ack_byte = is_inverted ? 0x01 : 0x00;
            }
            else if (cmd == 0x84) {
                OLED_ShowSplash();   /* HIỆN SPLASH SCREEN */
                ack_byte = 0x84;
            }
            else if (cmd == 0x70) {
                /* TRUY VẤN STATUS */
                ack_byte = (oled_i2c_addr == 0x78) ? 0x3C : 0x3D;
            }
            else {
                ack_byte = cmd;
            }

            delay_ms(2);

            checksum = (unsigned char)(SLAVE_ID + ack_byte);
            UART_SendByte(0xAA);
            UART_SendByte(SLAVE_ID);
            UART_SendByte(ack_byte);
            UART_SendByte(checksum);
        }
    }
}

/* =========================================================
   BASIC INTERRUPT VECTOR TABLE FOR STM8S103F3 (COSMIC C)
   Slave ID: 0x06 - Module Man Hinh LCD 1602 I2C
========================================================= */

typedef void @far (*interrupt_handler_t)(void);

struct interrupt_vector {
    unsigned char interrupt_instruction;
    interrupt_handler_t interrupt_handler;
};

/* Thanh ghi UART1 dung trong ISR */
#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)

/* =========================================================
   BIEN TOAN CUC CHIA SE VOI MAIN.C
========================================================= */
volatile unsigned char global_lcd_cmd = 0; 
volatile unsigned char global_lcd_event = 0;
volatile unsigned char global_lcd_line = 0;       /* Dong 1 hoac Dong 2 */
volatile unsigned char global_lcd_buffer[17];      /* 16 ky tu ASCII + null */
volatile unsigned char global_lcd_text_ready = 0;  /* Co bao chuoi moi da san sang */

/* Bien noi bo cho State Machine Parser trong ISR */
static volatile unsigned char rx_state = 0;
static volatile unsigned char rx_node_id = 0;
static volatile unsigned char rx_cmd = 0;
static volatile unsigned char rx_char_idx = 0;
static volatile unsigned char rx_crc_calc = 0;
static volatile unsigned char rx_text_buf[16];

/* Trinh xu ly ngat mac dinh cho cac Vector khong dung */
@far @interrupt void NonHandledInterrupt(void) {
    return;
}

/* =========================================================
   UART1 RX ISR (IRQ18) - REPEATER & PARSER DYNAMIC STRING
========================================================= */
@far @interrupt void UART1_RX_IRQHandler(void)
{
    unsigned char c;
    unsigned char i;

    /* Doc UART1_DR tu dong xoa co ngat RXNE */
    c = UART1_DR; 

    /* 1. HARDWARE REPEATER: Lap tuc chuyen tiep qua TX de duy tri vong Ring */
    while(!(UART1_SR & 0x80)); /* Cho dem TXE trong */
    UART1_DR = c;

    /* 2. PROTOCOL PARSER: Ho tro ca Frame 4-byte va Frame Chuoi Text 20-byte */
    switch(rx_state) {
        case 0: /* Cho Header 0xAA */
            if(c == 0xAA) {
                rx_state = 1; 
            }
            break;
            
        case 1: /* Nhan NODE_ID */
            rx_node_id = c; 
            rx_state = (c == 0x06) ? 2 : 0; 
            break;
            
        case 2: /* Nhan Lenh (CMD) */
            rx_cmd = c; 
            if(rx_cmd == 0x11 || rx_cmd == 0x12) {
                /* 0x11: Ghi Dong 1 (16 ky tu) | 0x12: Ghi Dong 2 (16 ky tu) */
                rx_char_idx = 0;
                rx_crc_calc = (unsigned char)(0x06 + rx_cmd);
                rx_state = 4; /* Chuyen sang nhan 16 byte ky tu */
            } else {
                rx_state = 3; /* Lenh 4-byte thong thuong */
            }
            break;
            
        case 3: /* Lenh 4-byte: Kiem tra Checksum Modulo-256 */
            if(c == (unsigned char)(rx_node_id + rx_cmd)) {
                global_lcd_cmd = rx_cmd;
                global_lcd_event = 1; 
            }
            rx_state = 0; 
            break;

        case 4: /* Nhan 16 ky tu ASCII cua dong LCD */
            rx_text_buf[rx_char_idx++] = c;
            rx_crc_calc = (unsigned char)(rx_crc_calc + c);
            if(rx_char_idx >= 16) {
                rx_state = 5; /* Du 16 ky tu -> Cho byte Checksum */
            }
            break;

        case 5: /* Kiem tra Checksum cua goi tin Text */
            if(c == rx_crc_calc) {
                global_lcd_line = (rx_cmd == 0x11) ? 1 : 2;
                for(i = 0; i < 16; i++) {
                    global_lcd_buffer[i] = rx_text_buf[i];
                }
                global_lcd_buffer[16] = '\0';
                global_lcd_text_ready = 1;
            }
            rx_state = 0;
            break;
            
        default: 
            rx_state = 0; 
            break;
    }
}

extern void _stext(); /* Diem vao khoi dong Cosmic C */

/* BANG VECTOR NGAT PHAN CUNG (IRQ18 cho UART1 RX) */
struct interrupt_vector const _vectab[] = {
    {0x82, (interrupt_handler_t)_stext}, 
    {0x82, NonHandledInterrupt}, /* TRAP */
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    
    /* IRQ18 - UART1 RX (Ngat nhan du lieu mang vong) */
    {0x82, UART1_RX_IRQHandler},
    
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}
};


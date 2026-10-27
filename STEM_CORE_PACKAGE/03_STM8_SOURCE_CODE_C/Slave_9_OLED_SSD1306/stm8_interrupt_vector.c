/* =========================================================
   BASIC INTERRUPT VECTOR TABLE FOR STM8S103F3 (COSMIC C)
   Slave ID: 0x09 - Module Màn hình OLED 0.96 inch SSD1306
   ========================================================= */

typedef void @far (*interrupt_handler_t)(void);

struct interrupt_vector {
    unsigned char interrupt_instruction;
    interrupt_handler_t interrupt_handler;
};

#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)

#define SLAVE_ID      0x09

/* Biến toàn cục chia sẻ với main.c */
volatile unsigned char global_oled_cmd = 0; 
volatile unsigned char global_oled_event = 0;
volatile unsigned char global_oled_line_cmd = 0;   /* Mã dòng: 0x10-0x13 (Font 8x16) hoặc 0x20-0x27 (Font 6x8) */
volatile unsigned char global_oled_buffer[17];     /* 16 ký tự text + ký tự null */
volatile unsigned char global_oled_text_ready = 0; /* Cờ báo có chuỗi mới cần in */

/* Biến nội bộ State Machine Parser trong ISR */
static volatile unsigned char rx_state = 0;
static volatile unsigned char rx_node_id = 0;
static volatile unsigned char rx_cmd = 0;
static volatile unsigned char rx_char_idx = 0;
static volatile unsigned char rx_crc_calc = 0;
static volatile unsigned char rx_text_buf[16];

@far @interrupt void NonHandledInterrupt(void) {
    return;
}

/* =========================================================
   UART1 RX ISR (IRQ18 - 0x8050)
   - HARDWARE REPEATER: Giữ nguyên tính toàn vẹn vòng Ring
   - PARSER: Hỗ trợ Frame 4-byte (Lệnh nhanh) và Frame 20-byte (Text)
   ========================================================= */
@far @interrupt void UART1_RX_IRQHandler(void)
{
    unsigned char c;
    unsigned char i;

    /* Đọc byte để xóa cờ ngắt RXNE */
    c = UART1_DR; 

    /* 1. HARDWARE REPEATER: Đẩy ngay sang TX cho Slave kế tiếp */
    while(!(UART1_SR & 0x80)); 
    UART1_DR = c;

    /* 2. PROTOCOL PARSER */
    switch(rx_state) {
        case 0: /* Chờ Header 0xAA */
            if(c == 0xAA) {
                rx_state = 1; 
            }
            break;
            
        case 1: /* Nhận NODE_ID */
            rx_node_id = c; 
            rx_state = (c == SLAVE_ID) ? 2 : 0; 
            break;
            
        case 2: /* Nhận Lệnh (CMD) */
            rx_cmd = c; 
            /* Kiểm tra nếu là lệnh gửi chuỗi văn bản (Font 8x16 hoặc Font 6x8) */
            if((rx_cmd >= 0x10 && rx_cmd <= 0x13) || (rx_cmd >= 0x20 && rx_cmd <= 0x27)) {
                rx_char_idx = 0;
                rx_crc_calc = (unsigned char)(SLAVE_ID + rx_cmd);
                rx_state = 4; /* Chuyển sang nhận 16 byte ký tự */
            } else {
                rx_state = 3; /* Lệnh 4-byte thông thường */
            }
            break;
            
        case 3: /* Lệnh 4-byte: Kiểm tra Checksum Modulo-256 */
            if(c == (unsigned char)(rx_node_id + rx_cmd)) {
                global_oled_cmd = rx_cmd;
                global_oled_event = 1; 
            }
            rx_state = 0; 
            break;

        case 4: /* Nhận 16 byte ký tự ASCII */
            rx_text_buf[rx_char_idx++] = c;
            rx_crc_calc = (unsigned char)(rx_crc_calc + c);
            if(rx_char_idx >= 16) {
                rx_state = 5; /* Đã đủ 16 ký tự -> Chờ byte Checksum */
            }
            break;

        case 5: /* Kiểm tra Checksum gói Text 20-byte */
            if(c == rx_crc_calc) {
                global_oled_line_cmd = rx_cmd;
                for(i = 0; i < 16; i++) {
                    global_oled_buffer[i] = rx_text_buf[i];
                }
                global_oled_buffer[16] = '\0';
                global_oled_text_ready = 1;
            }
            rx_state = 0;
            break;
            
        default: 
            rx_state = 0; 
            break;
    }
}

extern void _stext();

/* BẢNG VECTOR NGẮT: ĐÚNG 20 PHẦN TỬ TRƯỚC IRQ18 ĐỂ ĐẶT CHUẨN TẠI 0x8050 */
struct interrupt_vector const _vectab[] = {
    {0x82, (interrupt_handler_t)_stext}, 
    {0x82, NonHandledInterrupt}, /* trap */
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    
    /* IRQ18 - UART1 RX (0x8050) */
    {0x82, UART1_RX_IRQHandler},
    
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}
};

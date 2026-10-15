/* =========================================================
   BASIC INTERRUPT VECTOR TABLE FOR STM8S103F3 (COSMIC C)
   Slave ID: 0x0A - Module Cảm Biến Độ Ẩm Đất LM393
   ========================================================= */

typedef void @far (*interrupt_handler_t)(void);

struct interrupt_vector {
    unsigned char interrupt_instruction;
    interrupt_handler_t interrupt_handler;
};

#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)

#define SLAVE_ID      0x0A

/* Biến giao tiếp với hàm main */
volatile unsigned char global_soil_cmd = 0x00;
volatile unsigned char global_soil_ready = 0;

/* Biến nội bộ State Machine Parser */
static volatile unsigned char rx_state = 0;
static volatile unsigned char rx_node_id = 0;
static volatile unsigned char rx_cmd = 0;

@far @interrupt void NonHandledInterrupt(void) {
    return;
}

/* =========================================================
   UART1 RX ISR (IRQ18 - 0x8050)
   - HARDWARE REPEATER: Đẩy ngay sang TX cho Slave kế tiếp
   - PROTOCOL PARSER: 4-Byte Standard Ring Frame
   ========================================================= */
@far @interrupt void UART1_RX_IRQHandler(void)
{
    unsigned char c = UART1_DR; /* Đọc byte để tự xóa cờ RXNE */

    /* 1. HARDWARE REPEATER: Chuyển tiếp ngay lập tức sang TX */
    while(!(UART1_SR & 0x80)); 
    UART1_DR = c;

    /* 2. PROTOCOL PARSER: [0xAA, ID, CMD, CHECKSUM] */
    switch(rx_state) {
        case 0: /* Chờ Header 0xAA */
            if(c == 0xAA) rx_state = 1; 
            break;
            
        case 1: /* Nhận NODE_ID */
            rx_node_id = c; 
            rx_state = (c == SLAVE_ID) ? 2 : 0; 
            break;
            
        case 2: /* Nhận CMD / DATA */
            rx_cmd = c; 
            rx_state = 3; 
            break;
            
        case 3: /* Kiểm tra Checksum Modulo-256 */
            if(c == (unsigned char)(rx_node_id + rx_cmd)) {
                global_soil_cmd = rx_cmd;
                global_soil_ready = 1;
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

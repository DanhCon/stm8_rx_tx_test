/* =========================================================
   BASIC INTERRUPT VECTOR TABLE & ISR FOR STM8 (SLAVE 2 - BUTTON)
========================================================= */

typedef void @far (*interrupt_handler_t)(void);

struct interrupt_vector {
    unsigned char interrupt_instruction;
    interrupt_handler_t interrupt_handler;
};

#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)

/* Bi?n c? giao ti?p v?i hàm main */
volatile unsigned char global_send_btn = 0; 

/* Bi?n n?i b? cho State Machine Parser */
volatile unsigned char rx_state = 0;
volatile unsigned char rx_node_id = 0;
volatile unsigned char rx_cmd = 0;

@far @interrupt void NonHandledInterrupt(void) {
    return;
}

/* =========================================================
   UART1 RX ISR - REPEATER & PARSER (CRITICAL TIMING)
========================================================= */
@far @interrupt void UART1_RX_IRQHandler(void)
{
    unsigned char c = UART1_DR; /* Ð?c d? ph?n c?ng xóa c? RXNE */

    /* ---------------------------------------------------
       1. HARDWARE REPEATER: Xuyên tâm d? li?u
       Ép byte v?a nh?n sang TX d? chuy?n ti?p cho Node sau
    --------------------------------------------------- */
    while(!(UART1_SR & 0x80)); /* Ð?i b? d?m truy?n tr?ng (TXE = 1) */
    UART1_DR = c;

    /* ---------------------------------------------------
       2. PROTOCOL PARSER: Phân tích cú pháp 4-byte
       Frame: [0xAA] [ID] [CMD] [Checksum]
    --------------------------------------------------- */
    switch(rx_state) {
        case 0: /* CH? HEADER */
            if(c == 0xAA) rx_state = 1; 
            break;
            
        case 1: /* L?Y ID */
            rx_node_id = c; 
            rx_state = 2; 
            break;
            
        case 2: /* L?Y L?NH (COMMAND) */
            rx_cmd = c; 
            rx_state = 3; 
            break;
            
        case 3: /* L?Y CHECKSUM VÀ KI?M TRA TOÀN V?N */
            /* Checksum = (ID + CMD) Modulo 256. 
               Ki?u unsigned char t? d?ng tràn s? ?ng v?i Modulo 256 */
            if(c == (unsigned char)(rx_node_id + rx_cmd)) {
                
                /* N?u Checksum dúng, l?c xem l?nh có dành cho mình không */
                if(rx_node_id == 0x02 && rx_cmd == 0x50) {
                    global_send_btn = 1; /* B?t c?, yêu c?u main tr? l?i */
                }
                
            }
            rx_state = 0; /* Khôi ph?c tr?ng thái ch? Frame m?i */
            break;
            
        default: 
            rx_state = 0; 
            break;
    }
}

extern void _stext();

/* B?NG VECTOR NG?T (Ch? gi? l?i IRQ18 cho UART1 RX) */
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
    
    /* IRQ18 - UART1 RX */
    {0x82, UART1_RX_IRQHandler},
    
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}, {0x82, NonHandledInterrupt},
    {0x82, NonHandledInterrupt}
};
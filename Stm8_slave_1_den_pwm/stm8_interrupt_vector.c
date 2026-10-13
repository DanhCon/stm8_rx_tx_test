/* =========================================================
   BASIC INTERRUPT VECTOR TABLE FOR STM8 (SLAVE 1 - LED)
========================================================= */

typedef void @far (*interrupt_handler_t)(void);

struct interrupt_vector {
    unsigned char interrupt_instruction;
    interrupt_handler_t interrupt_handler;
};

#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)

/* Bi?n c? giao ti?p v?i hàm main */
volatile unsigned char global_led_cmd = 0x00; 

/* Bi?n n?i b? cho State Machine Parser */
volatile unsigned char rx_state = 0;
volatile unsigned char rx_node_id = 0;
volatile unsigned char rx_cmd = 0;

@far @interrupt void NonHandledInterrupt(void) {
    return;
}

/* =========================================================
   UART1 RX ISR - REPEATER & PARSER
========================================================= */
@far @interrupt void UART1_RX_IRQHandler(void)
{
    unsigned char c = UART1_DR; /* Ð?c d? ph?n c?ng xóa c? RXNE */

    /* 1. HARDWARE REPEATER: Ép byte v?a nh?n sang TX d? chuy?n ti?p */
    while(!(UART1_SR & 0x80)); 
    UART1_DR = c;

    /* 2. PROTOCOL PARSER: Phân tích cú pháp Checksum */
    switch(rx_state) {
        case 0: /* CH? HEADER */
            if(c == 0xAA) rx_state = 1; 
            break;
            
        case 1: /* L?Y ID */
            rx_node_id = c; 
            rx_state = 2; 
            break;
            
        case 2: /* L?Y L?NH */
            rx_cmd = c; 
            rx_state = 3; 
            break;
            
        case 3: /* KI?M TRA CHECKSUM TOÀN V?N */
            /* Ép ki?u unsigned char d? t? d?ng Modulo 256 */
            if(c == (unsigned char)(rx_node_id + rx_cmd)) {
                
                /* L?c dúng ID 0x01 c?a kh?i LED */
                if(rx_node_id == 0x01) {
                    global_led_cmd = rx_cmd; /* Chuy?n l?nh ra main */
                }
            }
            rx_state = 0; 
            break;
            
        default: 
            rx_state = 0; 
            break;
    }
}

extern void _stext();

/* B?NG VECTOR NG?T */
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
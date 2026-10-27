/* =========================================================
   BASIC INTERRUPT VECTOR TABLE FOR STM8 (SLAVE 7 - DC MOTOR)
========================================================= */

typedef void @far (*interrupt_handler_t)(void);

struct interrupt_vector {
    unsigned char interrupt_instruction;
    interrupt_handler_t interrupt_handler;
};

#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)

#define SLAVE_ID      0x07

/* Bien giao tiep voi ham main */
volatile unsigned char global_motor_cmd = 0x00;
volatile unsigned char global_motor_ready = 0;

/* Bien noi bo cho State Machine Parser */
volatile unsigned char rx_state = 0;
volatile unsigned char rx_node_id = 0;
volatile unsigned char rx_cmd = 0;

@far @interrupt void NonHandledInterrupt(void) {
    return;
}

/* =========================================================
   UART1 RX ISR - REPEATER & PARSER (IRQ18 - 0x8050)
========================================================= */
@far @interrupt void UART1_RX_IRQHandler(void)
{
    unsigned char c = UART1_DR; /* Doc byte de tu xoa co RXNE */

    /* 1. HARDWARE REPEATER: Day ngay byte sang TX cho node ke tiep */
    while(!(UART1_SR & 0x80)); 
    UART1_DR = c;

    /* 2. PROTOCOL PARSER: 4-Byte Frame [0xAA, ID, CMD/DATA, CHECKSUM] */
    switch(rx_state) {
        case 0: /* CHO HEADER 0xAA */
            if(c == 0xAA) rx_state = 1; 
            break;
            
        case 1: /* NHAN NODE ID */
            rx_node_id = c; 
            rx_state = 2; 
            break;
            
        case 2: /* NHAN CMD / DATA */
            rx_cmd = c; 
            rx_state = 3; 
            break;
            
        case 3: /* KIEM TRA CHECKSUM */
            if(c == (unsigned char)(rx_node_id + rx_cmd)) {
                if(rx_node_id == SLAVE_ID) {
                    global_motor_cmd = rx_cmd;
                    global_motor_ready = 1;
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

/* BANG VECTOR NGAT: DUNG 20 PHAN TU TRUOC IRQ18 */
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

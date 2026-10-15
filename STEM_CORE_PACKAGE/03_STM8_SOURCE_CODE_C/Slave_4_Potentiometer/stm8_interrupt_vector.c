typedef void @far (*interrupt_handler_t)(void);

struct interrupt_vector {
    unsigned char interrupt_instruction;
    interrupt_handler_t interrupt_handler;
};

#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)

/* C? giao ti?p: Master dang h?i giá tr? bi?n tr? */
volatile unsigned char global_req_adc = 0; 

volatile unsigned char rx_state = 0;
volatile unsigned char rx_node_id = 0;
volatile unsigned char rx_cmd = 0;

@far @interrupt void NonHandledInterrupt(void) { return; }

@far @interrupt void UART1_RX_IRQHandler(void)
{
    unsigned char c = UART1_DR; 

    /* HARDWARE REPEATER: Ép d? li?u qua TX l?p t?c */
    while(!(UART1_SR & 0x80)); 
    UART1_DR = c;

    /* PROTOCOL PARSER */
    switch(rx_state) {
        case 0: if(c == 0xAA) rx_state = 1; break;
        case 1: rx_node_id = c; rx_state = 2; break;
        case 2: rx_cmd = c; rx_state = 3; break;
        case 3: 
            /* KI?M TRA CHECKSUM MODULO-256 */
            if(c == (unsigned char)(rx_node_id + rx_cmd)) {
                
                /* B?t L?nh 0x60 g?i dích danh Node 0x04 */
                if(rx_node_id == 0x04 && rx_cmd == 0x60) {
                    global_req_adc = 1; 
                }
            }
            rx_state = 0; 
            break;
        default: rx_state = 0; break;
    }
}

extern void _stext();

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
/* =========================================================
   BASIC INTERRUPT VECTOR TABLE FOR STM8S103F3 (COSMIC C)
   Slave ID: 0x01 - LED & HC-SR04 Ultrasonic
========================================================= */

typedef void @far (*interrupt_handler_t)(void);

struct interrupt_vector {
    unsigned char interrupt_instruction;
    interrupt_handler_t interrupt_handler;
};

/* Thanh ghi UART1 dùng trong ISR */
#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)

/* =========================================================
   BI?N TOÀN C?C CHIA S? V?I MAIN.C
========================================================= */
volatile unsigned char global_led_cmd = 0x00; 

/* Bi?n n?i b? cho State Machine Parser trong ISR */
static volatile unsigned char rx_state = 0;
static volatile unsigned char rx_node_id = 0;
static volatile unsigned char rx_cmd = 0;

/* Trình x? lý ng?t m?c d?nh cho các Vector không dùng */
@far @interrupt void NonHandledInterrupt(void) {
    return;
}

/* =========================================================
   UART1 RX ISR (IRQ18) - REPEATER & PARSER
========================================================= */
@far @interrupt void UART1_RX_IRQHandler(void)
{
    unsigned char c;

    /* Ð?c UART1_DR t? d?ng xóa c? ng?t RXNE */
    c = UART1_DR; 

    /* 1. HARDWARE REPEATER */
    while(!(UART1_SR & 0x80)); /* Ch? c? TXE */
    UART1_DR = c;

    /* 2. PROTOCOL PARSER */
    switch(rx_state) {
        case 0: /* Ch? Header 0xAA */
            if(c == 0xAA) {
                rx_state = 1; 
            }
            break;
            
        case 1: /* Nh?n NODE_ID */
            rx_node_id = c; 
            rx_state = 2; 
            break;
            
        case 2: /* Nh?n L?nh (CMD) */
            rx_cmd = c; 
            rx_state = 3; 
            break;
            
        case 3: /* Ki?m tra Checksum */
            if(c == (unsigned char)(rx_node_id + rx_cmd)) {
                if(rx_node_id == 0x01) {
                    global_led_cmd = rx_cmd; 
                }
            }
                rx_state = 0; 
            break;
            
        default: 
            rx_state = 0; 
            break;
    }
}

/* Hàm b?t d?u c?a chuong trình do Compiler t?o ra */
extern void _stext();

/* =========================================================
   B?NG VECTOR NG?T (INTERRUPT VECTOR TABLE)
========================================================= */
struct interrupt_vector const _vectab[] = {
    {0x82, (interrupt_handler_t)_stext}, /* Reset Vector */
    {0x82, NonHandledInterrupt},         /* TRAP */
    {0x82, NonHandledInterrupt},         /* TLI */
    {0x82, NonHandledInterrupt},         /* AWU */
    {0x82, NonHandledInterrupt},         /* CLK */
    {0x82, NonHandledInterrupt},         /* EXTI0 */
    {0x82, NonHandledInterrupt},         /* EXTI1 */
    {0x82, NonHandledInterrupt},         /* EXTI2 */
    {0x82, NonHandledInterrupt},         /* EXTI3 */
    {0x82, NonHandledInterrupt},         /* EXTI4 */
    {0x82, NonHandledInterrupt},         
    {0x82, NonHandledInterrupt},         
    {0x82, NonHandledInterrupt},         /* SPI */
    {0x82, NonHandledInterrupt},         /* TIM1 Ovf */
    {0x82, NonHandledInterrupt},         /* TIM1 Cap/Comp */
    {0x82, NonHandledInterrupt},         /* TIM2 Ovf */
    {0x82, NonHandledInterrupt},         /* TIM2 Cap/Comp */
    {0x82, NonHandledInterrupt},         
    {0x82, NonHandledInterrupt},         
    
    /* IRQ18 - UART1 RX Interrupt */
    {0x82, UART1_RX_IRQHandler},
    
    {0x82, NonHandledInterrupt},         /* UART1 TX */
    {0x82, NonHandledInterrupt},         /* I2C */
    {0x82, NonHandledInterrupt},         
    {0x82, NonHandledInterrupt},         
    {0x82, NonHandledInterrupt},         /* ADC1 */
    {0x82, NonHandledInterrupt},         /* TIM4 */
    {0x82, NonHandledInterrupt},         /* Flash */
    {0x82, NonHandledInterrupt},         
    {0x82, NonHandledInterrupt},         
    {0x82, NonHandledInterrupt},         
    {0x82, NonHandledInterrupt},         
    {0x82, NonHandledInterrupt}          
};
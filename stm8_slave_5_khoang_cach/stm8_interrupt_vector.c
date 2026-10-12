/* =========================================================
   BASIC INTERRUPT VECTOR TABLE FOR STM8S103F3 (COSMIC C)
   Slave ID: 0x05 - Cam bien khoang cach sieu am HC-SR04
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
volatile unsigned char global_req_dist = 0; 
volatile unsigned char global_slave_id = 0x05;

/* Bien noi bo cho State Machine Parser trong ISR */
static volatile unsigned char rx_state = 0;
static volatile unsigned char rx_node_id = 0;
static volatile unsigned char rx_cmd = 0;

/* Trinh xu ly ngat mac dinh cho cac Vector khong dung */
@far @interrupt void NonHandledInterrupt(void) {
    return;
}

/* =========================================================
   UART1 RX ISR (IRQ18) - REPEATER & PARSER
========================================================= */
@far @interrupt void UART1_RX_IRQHandler(void)
{
    unsigned char c;

    /* Doc UART1_DR tu dong xoa co ngat RXNE */
    c = UART1_DR; 

    /* 1. HARDWARE REPEATER: Lap tuc chuyen tiep qua TX de duy tri vong Ring */
    while(!(UART1_SR & 0x80)); /* Cho dem TXE trong */
    UART1_DR = c;

    /* 2. PROTOCOL PARSER */
    switch(rx_state) {
        case 0: /* Cho Header 0xAA */
            if(c == 0xAA) {
                rx_state = 1; 
            }
            break;
            
        case 1: /* Nhan NODE_ID */
            rx_node_id = c; 
            rx_state = 2; 
            break;
            
        case 2: /* Nhan Lenh (CMD) */
            rx_cmd = c; 
            rx_state = 3; 
            break;
            
        case 3: /* Kiem tra Checksum Modulo-256 */
            if(c == (unsigned char)(rx_node_id + rx_cmd)) {
                /* Chap nhan neu Master hoi ID 0x05 (hoac 0x04) voi lenh 0x70 (hoac 0x40) */
                if((rx_node_id == 0x05 || rx_node_id == 0x04) && (rx_cmd == 0x70 || rx_cmd == 0x40)) {
                    global_slave_id = rx_node_id;
                    global_req_dist = 1; 
                }
            }
            rx_state = 0; 
            break;
            
        default: 
            rx_state = 0; 
            break;
    }
}

/* Ham bat dau cua chuong trinh do Compiler tao ra */
extern void _stext();

/* =========================================================
   BANG VECTOR NGAT (INTERRUPT VECTOR TABLE)
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

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
    {0x82, (interrupt_handler_t)_stext}, /* 0x8000: Reset Vector */
    {0x82, NonHandledInterrupt},         /* 0x8004: TRAP */
    {0x82, NonHandledInterrupt},         /* 0x8008: IRQ0  - TLI */
    {0x82, NonHandledInterrupt},         /* 0x800C: IRQ1  - AWU */
    {0x82, NonHandledInterrupt},         /* 0x8010: IRQ2  - CLK */
    {0x82, NonHandledInterrupt},         /* 0x8014: IRQ3  - EXTI0 */
    {0x82, NonHandledInterrupt},         /* 0x8018: IRQ4  - EXTI1 */
    {0x82, NonHandledInterrupt},         /* 0x801C: IRQ5  - EXTI2 */
    {0x82, NonHandledInterrupt},         /* 0x8020: IRQ6  - EXTI3 */
    {0x82, NonHandledInterrupt},         /* 0x8024: IRQ7  - EXTI4 */
    {0x82, NonHandledInterrupt},         /* 0x8028: IRQ8  - Reserved */
    {0x82, NonHandledInterrupt},         /* 0x802C: IRQ9  - Reserved */
    {0x82, NonHandledInterrupt},         /* 0x8030: IRQ10 - SPI */
    {0x82, NonHandledInterrupt},         /* 0x8034: IRQ11 - TIM1 Ovf */
    {0x82, NonHandledInterrupt},         /* 0x8038: IRQ12 - TIM1 Cap/Comp */
    {0x82, NonHandledInterrupt},         /* 0x803C: IRQ13 - TIM2 Ovf */
    {0x82, NonHandledInterrupt},         /* 0x8040: IRQ14 - TIM2 Cap/Comp */
    {0x82, NonHandledInterrupt},         /* 0x8044: IRQ15 - Reserved */
    {0x82, NonHandledInterrupt},         /* 0x8048: IRQ16 - Reserved */
    {0x82, NonHandledInterrupt},         /* 0x804C: IRQ17 - UART1 TX */
    
    /* 0x8050: IRQ18 - UART1 RX Interrupt */
    {0x82, UART1_RX_IRQHandler},
    
    {0x82, NonHandledInterrupt},         /* 0x8054: IRQ19 - I2C */
    {0x82, NonHandledInterrupt},         /* 0x8058: IRQ20 - Reserved */
    {0x82, NonHandledInterrupt},         /* 0x805C: IRQ21 - Reserved */
    {0x82, NonHandledInterrupt},         /* 0x8060: IRQ22 - ADC1 */
    {0x82, NonHandledInterrupt},         /* 0x8064: IRQ23 - TIM4 */
    {0x82, NonHandledInterrupt},         /* 0x8068: IRQ24 - Flash */
    {0x82, NonHandledInterrupt},         /* 0x806C: IRQ25 - Reserved */
    {0x82, NonHandledInterrupt},         /* 0x8070: IRQ26 - Reserved */
    {0x82, NonHandledInterrupt},         /* 0x8074: IRQ27 - Reserved */
    {0x82, NonHandledInterrupt},         /* 0x8078: IRQ28 - Reserved */
    {0x82, NonHandledInterrupt}          /* 0x807C: IRQ29 - Reserved */
};

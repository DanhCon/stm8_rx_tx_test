/* =========================================================
   BASIC INTERRUPT VECTOR TABLE FOR STM8 (SLAVE 1 - SMART LED)
   Auto-Effects via TIM4 Timer & Hardware Repeater on UART1
========================================================= */

typedef void @far (*interrupt_handler_t)(void);

struct interrupt_vector {
    unsigned char interrupt_instruction;
    interrupt_handler_t interrupt_handler;
};

#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)

#define TIM4_SR       *(volatile unsigned char*)(0x005344)
#define TIM1_CCR3L    *(volatile unsigned char*)(0x00526A)

#define MODE_BLINK        0x10
#define MODE_ON           0x20
#define MODE_OFF          0x30
#define MODE_BREATHE      0x40
#define MODE_SOS          0x41
#define MODE_DOUBLE_FLASH 0x42
#define MODE_DIRECT_PWM   0x43

/* Shared variables */
volatile unsigned char global_led_cmd = 0x00;
volatile unsigned char current_mode = 0x30; /* Default OFF */
volatile unsigned char current_duty = 0;
volatile unsigned int ms_tick = 0;

/* Internal parser state */
static volatile unsigned char rx_state = 0;
static volatile unsigned char rx_node_id = 0;
static volatile unsigned char rx_cmd = 0;

@far @interrupt void NonHandledInterrupt(void) {
    return;
}

/* =========================================================
   UART1 RX ISR (IRQ18 at 0x8050) - REPEATER & PARSER
========================================================= */
@far @interrupt void UART1_RX_IRQHandler(void)
{
    unsigned char c;
    c = UART1_DR; /* Read clears RXNE flag */

    /* 1. HARDWARE REPEATER: Forward immediately to TX */
    while(!(UART1_SR & 0x80));
    UART1_DR = c;

    /* 2. PROTOCOL PARSER: 4-Byte Frame [0xAA, ID, CMD, CRC] */
    switch(rx_state) {
        case 0:
            if(c == 0xAA) rx_state = 1;
            break;
            
        case 1:
            rx_node_id = c;
            rx_state = 2;
            break;
            
        case 2:
            rx_cmd = c;
            rx_state = 3;
            break;
            
        case 3:
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

/* =========================================================
   TIM4 UPDATE/OVERFLOW ISR (IRQ23 at 0x8064)
   Generates 1ms Tick and Executes All Autonomous LED Effects
========================================================= */
@far @interrupt void TIM4_UPD_OVF_IRQHandler(void)
{
    unsigned int t;

    TIM4_SR &= (unsigned char)(~0x01); /* Clear UIF flag */
    ms_tick++;

    switch(current_mode)
    {
        case MODE_OFF:
            TIM1_CCR3L = 0;
            break;

        case MODE_ON:
            TIM1_CCR3L = 100;
            break;

        case MODE_BLINK:
            /* 500ms ON / 500ms OFF */
            t = ms_tick % 1000;
            if(t < 500) TIM1_CCR3L = 100;
            else TIM1_CCR3L = 0;
            break;

        case MODE_BREATHE:
            /* Triangle fade: 20ms per step -> 0 to 100 in 2000ms, 100 to 0 in 2000ms */
            t = ms_tick % 4000;
            if(t < 2000) {
                TIM1_CCR3L = (unsigned char)(t / 20);
            } else {
                TIM1_CCR3L = (unsigned char)(100 - ((t - 2000) / 20));
            }
            break;

        case MODE_SOS:
            /* Morse SOS: ... --- ... (cycle 4500ms)
               Dots: 150ms ON, 150ms OFF
               Dashes: 450ms ON, 150ms OFF
               Letter space: 300ms OFF, Word space: 750ms OFF
            */
            t = ms_tick % 4500;
            if((t < 150) || 
               (t >= 300 && t < 450) || 
               (t >= 600 && t < 750) ||
               (t >= 1050 && t < 1500) || 
               (t >= 1650 && t < 2100) || 
               (t >= 2250 && t < 2700) ||
               (t >= 3000 && t < 3150) || 
               (t >= 3300 && t < 3450) || 
               (t >= 3600 && t < 3750))
            {
                TIM1_CCR3L = 100;
            }
            else
            {
                TIM1_CCR3L = 0;
            }
            break;

        case MODE_DOUBLE_FLASH:
            /* Double strobe flash (cycle 1000ms) */
            t = ms_tick % 1000;
            if((t < 70) || (t >= 150 && t < 220))
            {
                TIM1_CCR3L = 100;
            }
            else
            {
                TIM1_CCR3L = 0;
            }
            break;

        case MODE_DIRECT_PWM:
            TIM1_CCR3L = current_duty;
            break;

        default:
            TIM1_CCR3L = 0;
            break;
    }
}

extern void _stext();

/* =========================================================
   INTERRUPT VECTOR TABLE
   Strict 20 Entries Before IRQ18 (UART1_RX)
   IRQ23 at 0x8064 (TIM4)
========================================================= */
struct interrupt_vector const _vectab[] = {
    {0x82, (interrupt_handler_t)_stext}, /* 0x8000: Reset */
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
    
    /* 0x8050: IRQ18 - UART1 RX */
    {0x82, UART1_RX_IRQHandler},
    
    {0x82, NonHandledInterrupt},         /* 0x8054: IRQ19 - I2C */
    {0x82, NonHandledInterrupt},         /* 0x8058: IRQ20 - Reserved */
    {0x82, NonHandledInterrupt},         /* 0x805C: IRQ21 - Reserved */
    {0x82, NonHandledInterrupt},         /* 0x8060: IRQ22 - ADC1 */
    
    /* 0x8064: IRQ23 - TIM4 Update/Overflow */
    {0x82, TIM4_UPD_OVF_IRQHandler},
    
    {0x82, NonHandledInterrupt},         /* 0x8068: IRQ24 - Flash */
    {0x82, NonHandledInterrupt},         /* 0x806C: IRQ25 - Reserved */
    {0x82, NonHandledInterrupt},         /* 0x8070: IRQ26 - Reserved */
    {0x82, NonHandledInterrupt},         /* 0x8074: IRQ27 - Reserved */
    {0x82, NonHandledInterrupt},         /* 0x8078: IRQ28 - Reserved */
    {0x82, NonHandledInterrupt}          /* 0x807C: IRQ29 - Reserved */
};
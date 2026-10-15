/* =========================================================
   BASIC INTERRUPT VECTOR TABLE FOR STM8 (SLAVE 2 - SMART BUTTON)
   Debounce & Smart Gestures (Single, Double, Long, Counter)
========================================================= */

typedef void @far (*interrupt_handler_t)(void);

struct interrupt_vector {
    unsigned char interrupt_instruction;
    interrupt_handler_t interrupt_handler;
};

#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)

#define TIM4_SR       *(volatile unsigned char*)(0x005344)
#define GPIOC_IDR     *(volatile unsigned char*)(0x00500B)

#define SLAVE_ID      0x02

/* Gesture Codes */
#define GESTURE_NONE          0x50
#define GESTURE_SINGLE_CLICK  0x51
#define GESTURE_DOUBLE_CLICK  0x52
#define GESTURE_LONG_PRESS    0x53
#define CMD_QUERY_COUNTER     0x54

/* State machine states */
#define STATE_IDLE            0
#define STATE_PRESSED_FIRST   1
#define STATE_WAIT_DOUBLE     2
#define STATE_PRESSED_SECOND  3
#define STATE_LONG_HELD       4

/* Shared flags with main.c */
volatile unsigned char global_query_cmd = 0;
volatile unsigned char latched_gesture = GESTURE_NONE;
volatile unsigned char button_press_count = 0;
volatile unsigned char current_raw_state = 0;

/* Internal ISR timing & debounce */
static volatile unsigned char debounce_state = 0;
static volatile unsigned char debounce_cnt = 0;
static volatile unsigned char btn_fsm_state = STATE_IDLE;
static volatile unsigned int press_timer_ms = 0;
static volatile unsigned int double_timer_ms = 0;

/* UART State Machine */
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

    /* 1. HARDWARE REPEATER: Push immediately to TX */
    while(!(UART1_SR & 0x80));
    UART1_DR = c;

    /* 2. PROTOCOL PARSER: 4-byte Frame [0xAA, ID, CMD, CRC] */
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
                if(rx_node_id == SLAVE_ID) {
                    global_query_cmd = rx_cmd;
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
   TIM4 UPDATE ISR (IRQ23 at 0x8064)
   1ms Periodic Interrupt: Debounce & Gesture Classifier
========================================================= */
@far @interrupt void TIM4_UPD_OVF_IRQHandler(void)
{
    unsigned char pin_pressed;
    unsigned char press_edge;
    unsigned char release_edge;

    TIM4_SR &= (unsigned char)(~0x01); /* Clear UIF */

    /* Read PC4: Pressed = 0 (low), Released = 1 (high pull-up) */
    pin_pressed = ((GPIOC_IDR & (1 << 4)) == 0) ? 1 : 0;
    current_raw_state = pin_pressed;

    press_edge = 0;
    release_edge = 0;

    /* Debounce Filter: Require 15 consecutive samples (15ms stable) */
    if (pin_pressed != debounce_state) {
        debounce_cnt++;
        if (debounce_cnt >= 15) {
            debounce_state = pin_pressed;
            debounce_cnt = 0;
            if (debounce_state == 1) {
                press_edge = 1;
                button_press_count++; /* Cumulative counter increments */
            } else {
                release_edge = 1;
            }
        }
    } else {
        debounce_cnt = 0;
    }

    /* Gesture Classification State Machine */
    switch (btn_fsm_state)
    {
        case STATE_IDLE:
            if (press_edge) {
                press_timer_ms = 0;
                btn_fsm_state = STATE_PRESSED_FIRST;
            }
            break;

        case STATE_PRESSED_FIRST:
            press_timer_ms++;
            if (press_timer_ms >= 1500) {
                /* Long press held > 1.5 seconds */
                latched_gesture = GESTURE_LONG_PRESS; /* 0x53 */
                btn_fsm_state = STATE_LONG_HELD;
            } else if (release_edge) {
                /* Released before 1.5s -> wait for potential second click */
                double_timer_ms = 0;
                btn_fsm_state = STATE_WAIT_DOUBLE;
            }
            break;

        case STATE_WAIT_DOUBLE:
            double_timer_ms++;
            if (press_edge) {
                /* Second click occurred within 300ms window */
                btn_fsm_state = STATE_PRESSED_SECOND;
            } else if (double_timer_ms >= 300) {
                /* 300ms expired without second click -> Single Click confirmed */
                latched_gesture = GESTURE_SINGLE_CLICK; /* 0x51 */
                btn_fsm_state = STATE_IDLE;
            }
            break;

        case STATE_PRESSED_SECOND:
            /* Second click confirmed upon release */
            if (release_edge) {
                latched_gesture = GESTURE_DOUBLE_CLICK; /* 0x52 */
                btn_fsm_state = STATE_IDLE;
            }
            break;

        case STATE_LONG_HELD:
            /* Wait for release after long press */
            if (release_edge) {
                btn_fsm_state = STATE_IDLE;
            }
            break;

        default:
            btn_fsm_state = STATE_IDLE;
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
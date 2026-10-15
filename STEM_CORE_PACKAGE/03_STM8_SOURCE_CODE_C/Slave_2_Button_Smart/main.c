/* =========================================================
   STM8S103F3P6 - SLAVE 2: SMART BUTTON SENSOR
   Hardware Debounce & Gesture Classifier (0x51, 0x52, 0x53, 0x54)
   Hardware Repeater + Non-blocking TIM4 Background Engine
========================================================= */

#define CLK_CKDIVR    *(volatile unsigned char*)(0x0050C6)

/* =========================================================
   GPIOC (NUT NHAN TAI PC4 - INPUT PULL-UP)
========================================================= */
#define GPIOC_IDR     *(volatile unsigned char*)(0x00500B)
#define GPIOC_DDR     *(volatile unsigned char*)(0x00500C)
#define GPIOC_CR1     *(volatile unsigned char*)(0x00500D)

/* =========================================================
   TIM4 (1ms PERIODIC SYSTEM TICK ENGINE)
========================================================= */
#define TIM4_CR1      *(volatile unsigned char*)(0x005340)
#define TIM4_IER      *(volatile unsigned char*)(0x005343)
#define TIM4_SR       *(volatile unsigned char*)(0x005344)
#define TIM4_EGR      *(volatile unsigned char*)(0x005345)
#define TIM4_CNTR     *(volatile unsigned char*)(0x005346)
#define TIM4_PSCR     *(volatile unsigned char*)(0x005347)
#define TIM4_ARR      *(volatile unsigned char*)(0x005348)

/* =========================================================
   UART1 THANH GHI (9600 BAUD @ 16MHz)
========================================================= */
#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)
#define UART1_BRR1    *(volatile unsigned char*)(0x005232)
#define UART1_BRR2    *(volatile unsigned char*)(0x005233)
#define UART1_CR2     *(volatile unsigned char*)(0x005235)

#define SLAVE_ID          0x02

#define GESTURE_NONE          0x50
#define GESTURE_SINGLE_CLICK  0x51
#define GESTURE_DOUBLE_CLICK  0x52
#define GESTURE_LONG_PRESS    0x53
#define CMD_QUERY_COUNTER     0x54
#define CMD_RESET_COUNTER     0x55

extern volatile unsigned char global_query_cmd;
extern volatile unsigned char latched_gesture;
extern volatile unsigned char button_press_count;
extern volatile unsigned char current_raw_state;

void delay_ms(unsigned int ms) {
    volatile unsigned int i, j;
    for(i = 0; i < ms; i++) {
        for(j = 0; j < 3000; j++); 
    }
}

void UART_SendByte(unsigned char data) {
    while(!(UART1_SR & 0x80)); 
    UART1_DR = data;
}

/* Khoi tao TIM4 1ms Periodic Tick cho Debounce & Gesture Classifier */
void TIM4_Tick_Init(void) {
    TIM4_PSCR = 0x06;  /* Prescaler 2^6 = 64 -> 16MHz / 64 = 250kHz */
    TIM4_ARR = 249;    /* 250 / 250kHz = 1ms */
    TIM4_EGR = 0x01;   /* Update Generation */
    TIM4_SR &= (unsigned char)(~0x01); /* Clear UIF */
    TIM4_IER = 0x01;   /* Update Interrupt Enable */
    TIM4_CR1 = 0x01;   /* Counter Enable */
}

void main(void)
{
    unsigned char cmd;
    unsigned char resp_byte;
    unsigned char calculated_crc;

    CLK_CKDIVR = 0x00; /* 16MHz HSI */

    /* Cấu hình PC4: Input Pull-up (DDR=0, CR1=1) */
    GPIOC_DDR &= ~(1 << 4);
    GPIOC_CR1 |= (1 << 4);

    TIM4_Tick_Init();

    /* UART1: BRR2 ghi truoc, BRR1 ghi sau */
    UART1_BRR2 = 0x03;
    UART1_BRR1 = 0x68;
    UART1_CR2 = 0x2C;  /* TEN, REN, RIEN */

    _asm("rim");

    while(1)
    {
        if (global_query_cmd != 0)
        {
            cmd = global_query_cmd;
            global_query_cmd = 0;

            /* Delay 2ms trước khi phản hồi để xả trọn chuỗi Ring */
            delay_ms(2);

            if (cmd == 0x50)
            {
                /* Master truy vấn trạng thái cử chỉ (Gestures) */
                if (latched_gesture != GESTURE_NONE)
                {
                    resp_byte = latched_gesture;
                    latched_gesture = GESTURE_NONE; /* Xóa sau khi đọc */
                }
                else if (current_raw_state == 1)
                {
                    resp_byte = GESTURE_SINGLE_CLICK; /* Đang đè nút */
                }
                else
                {
                    resp_byte = GESTURE_NONE; /* Nhả phím / Idle (0x50) */
                }
            }
            else if (cmd == CMD_QUERY_COUNTER)
            {
                /* Master truy vấn bộ đếm tích lũy (0x54) */
                resp_byte = button_press_count;
            }
            else if (cmd == CMD_RESET_COUNTER)
            {
                /* Master yêu cầu reset bộ đếm (0x55) */
                button_press_count = 0;
                resp_byte = 0x00;
            }
            else
            {
                resp_byte = cmd;
            }

            calculated_crc = (unsigned char)(SLAVE_ID + resp_byte);

            /* Gửi Frame phản hồi 4-Byte chuẩn */
            UART_SendByte(0xAA);
            UART_SendByte(SLAVE_ID);
            UART_SendByte(resp_byte);
            UART_SendByte(calculated_crc);
        }
    }
}
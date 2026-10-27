/* =========================================================
   STM8S103F3P6 - SLAVE 10: MODULE CẢM BIẾN ĐỘ ẨM ĐẤT LM393
   =========================================================
   - Chân kết nối phần cứng:
     + AO (Analog Output) : PD3 (Chân 20 - Kênh ADC AIN4)
     + DO (Digital Output): PD2 (Chân 19 - Input Pull-up)
     + LED Cảnh báo khô   : PC3 (Chân 13 - Output Push-Pull)
     + UART1 TX           : PD5 (Chân 2)
     + UART1 RX           : PD6 (Chân 3)
     + Nguồn              : VCC (3.3V hoặc 5V) và GND
   - Cơ chế đo lường:
     + Đọc ADC 8-bit MSB trên PD3 (lấy trung bình 8 mẫu chống nhiễu)
     + Chuyển đổi tuyến tính sang độ ẩm phần trăm (0% đến 100%)
     + Đọc trạng thái kỹ thuật số DO từ mạch so sánh LM393
     + Tự động bật LED PC3 khi độ ẩm xuống dưới ngưỡng cảnh báo
   - Tốc độ UART: 9600 bps, 8N1
   - Giao thức   : Khung truyền chuẩn 4 Byte [0xAA, ID, DATA, CRC]
   ========================================================= */

#define CLK_CKDIVR    *(volatile unsigned char*)(0x0050C6)

/* =========================================================
   GPIOC (LED BÁO ĐỘNG TẠI PC3)
========================================================= */
#define GPIOC_ODR     *(volatile unsigned char*)(0x00500A)
#define GPIOC_DDR     *(volatile unsigned char*)(0x00500C)
#define GPIOC_CR1     *(volatile unsigned char*)(0x00500D)

/* =========================================================
   GPIOD (DO TẠI PD2, AO TẠI PD3 - AIN4)
========================================================= */
#define GPIOD_ODR     *(volatile unsigned char*)(0x00500F)
#define GPIOD_IDR     *(volatile unsigned char*)(0x005010)
#define GPIOD_DDR     *(volatile unsigned char*)(0x005011)
#define GPIOD_CR1     *(volatile unsigned char*)(0x005012)
#define GPIOD_CR2     *(volatile unsigned char*)(0x005013)

/* =========================================================
   ADC1 THANH GHI (ĐO TƯƠNG TỰ TRÊN PD3/AIN4)
========================================================= */
#define ADC_CSR       *(volatile unsigned char*)(0x005400)
#define ADC_CR1       *(volatile unsigned char*)(0x005401)
#define ADC_CR2       *(volatile unsigned char*)(0x005402)
#define ADC_DRH       *(volatile unsigned char*)(0x005404)
#define ADC_DRL       *(volatile unsigned char*)(0x005405)

/* =========================================================
   UART1 THANH GHI (9600 BAUD @ 16MHz)
========================================================= */
#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)
#define UART1_BRR1    *(volatile unsigned char*)(0x005232)
#define UART1_BRR2    *(volatile unsigned char*)(0x005233)
#define UART1_CR2     *(volatile unsigned char*)(0x005235)

#define SLAVE_ID      0x0A

/* Biến nhận diện lệnh từ ngắt UART */
extern volatile unsigned char global_soil_cmd;
extern volatile unsigned char global_soil_ready;

/* Ngưỡng cảnh báo đất khô (%): Mặc định dưới 30% thì bật LED PC3 */
static unsigned char threshold_moisture = 30;

void delay_ms(unsigned int ms) {
    volatile unsigned int i, j;
    for(i = 0; i < ms; i++) {
        for(j = 0; j < 3000; j++); 
    }
}

/* Hàm truyền 1 Byte chặn dòng */
void UART_SendByte(unsigned char data) {
    while(!(UART1_SR & 0x80)); 
    UART1_DR = data;
}

/* Khởi tạo khối chuyển đổi ADC1 cho chân PD3 (kênh AIN4) */
void ADC_Init(void) {
    ADC_CSR = 0x04;    /* Chọn kênh CH[3:0] = 0100 (AIN4) */
    ADC_CR2 = 0x00;    /* ALIGN = 0 (Left-alignment: 8 bit MSB nằm trong DRH) */
    ADC_CR1 |= 0x01;   /* ADON = 1: Bật nguồn ADC (Wake-up) */
    delay_ms(1);       /* Chờ bộ ổn áp ADC khởi động */
}

/* Đọc giá trị ADC 8-bit thô (0 đến 255) kèm lọc trung bình 8 lần */
unsigned char Read_ADC_Smoothed(void) {
    unsigned int sum;
    unsigned char i;

    sum = 0;
    for(i = 0; i < 8; i++) {
        ADC_CR1 |= 0x01; /* Bắt đầu chuyển đổi ADC */
        while(!(ADC_CSR & 0x80)); /* Chờ cờ EOC (End Of Conversion) */
        ADC_CSR &= ~0x80; /* Xóa cờ EOC */
        sum += ADC_DRH;   /* Cộng dồn 8 bit cao */
    }

    return (unsigned char)(sum / 8);
}

/* Chuyển đổi giá trị ADC thô (0-255) sang phần trăm độ ẩm đất (0-100%)
   - Cảm biến LM393: Đất khô điện trở lớn -> Điện áp AO cao (ADC ~ 245)
   - Đất ngập nước  : Điện trở nhỏ       -> Điện áp AO thấp (ADC ~ 55) */
unsigned char ADC_To_Moisture(unsigned char adc) {
    unsigned int m;

    if (adc >= 245) return 0;   /* Khô cằn hoàn toàn */
    if (adc <= 55)  return 100; /* Bão hòa nước */

    /* Công thức tuyến tính: Moisture% = (245 - ADC) * 100 / 190 */
    m = ((unsigned int)(245 - adc) * 100) / 190;
    if (m > 100) m = 100;

    return (unsigned char)m;
}

void main(void)
{
    /* Cấu hình xung nhịp CPU 16MHz không chia */
    CLK_CKDIVR = 0x00; 

    /* Cấu hình LED cảnh báo (PC3 - Output Push-Pull) */
    GPIOC_DDR |= (1 << 3);  
    GPIOC_CR1 |= (1 << 3);  
    GPIOC_ODR &= ~(1 << 3); /* Mặc định tắt LED */

    /* Cấu hình chân DO (PD2 - Input Pull-up) */
    GPIOD_DDR &= ~(1 << 2);
    GPIOD_CR1 |= (1 << 2);

    /* Cấu hình chân AO (PD3 - Input Floating / Analog) */
    GPIOD_DDR &= ~(1 << 3);
    GPIOD_CR1 &= ~(1 << 3);

    /* Khởi tạo ADC */
    ADC_Init();

    /* Cấu hình UART1 (9600 bps @ 16MHz) */
    UART1_BRR2 = 0x03;
    UART1_BRR1 = 0x68;
    UART1_CR2  = 0x2C; /* Bật TEN, REN và ngắt RIEN */

    /* Cho phép ngắt toàn cục */
    _asm("rim");

    while(1)
    {
        /* Kiểm tra khi có lệnh truy vấn từ Master Gateway */
        if (global_soil_ready) 
        {
            /* KHAI BÁO TẤT CẢ BIẾN Ở ĐẦU BLOCK (CHUẨN ANSI C89) */
            unsigned char cmd;
            unsigned char raw_adc;
            unsigned char moisture_pct;
            unsigned char do_state;
            unsigned char ack_data;
            unsigned char checksum;

            cmd = global_soil_cmd;
            global_soil_ready = 0;

            /* Đọc cảm biến */
            raw_adc = Read_ADC_Smoothed();
            moisture_pct = ADC_To_Moisture(raw_adc);
            do_state = (GPIOD_IDR & (1 << 2)) ? 1 : 0; /* 1 = Khô, 0 = Đủ ẩm */

            /* Tự động bật LED cảnh báo nếu đất khô (Độ ẩm < ngưỡng hoặc DO báo khô) */
            if (moisture_pct < threshold_moisture || do_state == 1) {
                GPIOC_ODR |= (1 << 3);  /* BẬT LED BÁO ĐỘNG */
            } else {
                GPIOC_ODR &= ~(1 << 3); /* TẮT LED */
            }

            /* XỬ LÝ MÃ LỆNH:
               1. 0x60 hoặc 0x70: Đọc độ ẩm phần trăm (0 - 100%)
               2. 0x61: Đọc giá trị ADC thô (0 - 255)
               3. 0x62: Đọc trạng thái kỹ thuật số DO (0 = Ẩm, 1 = Khô)
               4. Lệnh đặt ngưỡng cảnh báo: Nếu gửi giá trị 1-99 thì cập nhật ngưỡng
            */
            if (cmd == 0x60 || cmd == 0x70) {
                ack_data = moisture_pct;
            }
            else if (cmd == 0x61) {
                ack_data = raw_adc;
            }
            else if (cmd == 0x62) {
                ack_data = do_state;
            }
            else if (cmd >= 1 && cmd <= 99) {
                threshold_moisture = cmd;
                ack_data = threshold_moisture;
            }
            else {
                ack_data = moisture_pct;
            }

            /* Nghỉ 2ms để mạch lặp Repeater trên vòng Ring xả sạch byte Echo */
            delay_ms(2);

            /* PHÁT GÓI PHẢN HỒI THẬT (FRAME 2 - REAL SENSOR DATA) VỀ GATEWAY */
            checksum = (unsigned char)(SLAVE_ID + ack_data);

            UART_SendByte(0xAA);
            UART_SendByte(SLAVE_ID);
            UART_SendByte(ack_data);
            UART_SendByte(checksum);
        }
    }
}

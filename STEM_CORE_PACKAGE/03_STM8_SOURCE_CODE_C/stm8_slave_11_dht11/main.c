/* =========================================================
   STM8S103F3P6 - SLAVE 11: MODULE CẢM BIẾN NHIỆT ĐỘ & ĐỘ ẨM DHT11
   =========================================================
   - Chân kết nối phần cứng:
     + DATA (Chân tín hiệu 1-Wire) : PC4 (Chân số 14)
       (Lưu ý: Có trở kéo lên 4.7k - 10k lên VCC)
     + LED Trạng thái / Cảnh báo   : PC3 (Chân số 13 - Output)
     + UART1 TX                    : PD5 (Chân số 2)
     + UART1 RX                    : PD6 (Chân số 3)
     + Nguồn                       : VCC (3.3V hoặc 5V) và GND
   - Dải đo DHT11:
     + Nhiệt độ: 0°C đến 50°C (Sai số ±2°C)
     + Độ ẩm   : 20% đến 90% RH (Sai số ±5% RH)
   - Tốc độ UART: 9600 bps, 8N1
   - Giao thức   : Khung truyền 4 Byte [0xAA, ID, DATA, CRC]
   ========================================================= */

#define CLK_CKDIVR    *(volatile unsigned char*)(0x0050C6)

/* =========================================================
   GPIOC (PC3 = LED, PC4 = DHT11 DATA)
========================================================= */
#define GPIOC_ODR     *(volatile unsigned char*)(0x00500A)
#define GPIOC_IDR     *(volatile unsigned char*)(0x00500B)
#define GPIOC_DDR     *(volatile unsigned char*)(0x00500C)
#define GPIOC_CR1     *(volatile unsigned char*)(0x00500D)
#define GPIOC_CR2     *(volatile unsigned char*)(0x00500E)

/* =========================================================
   UART1 THANH GHI (9600 BAUD @ 16MHz)
========================================================= */
#define UART1_SR      *(volatile unsigned char*)(0x005230)
#define UART1_DR      *(volatile unsigned char*)(0x005231)
#define UART1_BRR1    *(volatile unsigned char*)(0x005232)
#define UART1_BRR2    *(volatile unsigned char*)(0x005233)
#define UART1_CR2     *(volatile unsigned char*)(0x005235)

#define SLAVE_ID      0x0B

/* Biến nhận diện lệnh từ ngắt UART */
extern volatile unsigned char global_dht_cmd;
extern volatile unsigned char global_dht_ready;

/* Bộ đệm lưu trữ giá trị đo gần nhất */
static unsigned char current_temp = 25;
static unsigned char current_humi = 60;
static unsigned char sensor_status = 1; /* 1 = Hoạt động tốt, 0 = Lỗi */

void delay_ms(unsigned int ms) {
    volatile unsigned int i, j;
    for(i = 0; i < ms; i++) {
        for(j = 0; j < 3000; j++); 
    }
}

/* Delay microsecond xấp xỉ cho CPU 16MHz */
void delay_us(unsigned int us) {
    volatile unsigned int i;
    for(i = 0; i < us; i++) {
        _asm("nop");
        _asm("nop");
        _asm("nop");
        _asm("nop");
    }
}

/* Hàm truyền 1 Byte UART */
void UART_SendByte(unsigned char data) {
    while(!(UART1_SR & 0x80)); 
    UART1_DR = data;
}

/* =========================================================
   HÀM ĐỌC DỮ LIỆU CẢM BIẾN DHT11 (CHUẨN 1-WIRE)
   Trả về: 1 nếu thành công và đúng Checksum, 0 nếu Timeout/Lỗi
========================================================= */
unsigned char DHT11_Read(unsigned char *temp, unsigned char *humi) {
    unsigned char dht_bytes[5];
    unsigned int timeout;
    unsigned char i, j;

    /* 1. GỬI TÍN HIỆU START TỪ HOST */
    /* Cấu hình PC4 là Output Push-Pull */
    GPIOC_DDR |= (1 << 4);
    GPIOC_CR1 |= (1 << 4);

    /* Kéo chân DATA xuống mức LOW ít nhất 18ms */
    GPIOC_ODR &= ~(1 << 4);
    delay_ms(20);

    /* 2. CHUYỂN PC4 SANG INPUT PULL-UP (Nhả bus an toàn, tránh xung đột) */
    GPIOC_DDR &= ~(1 << 4);
    GPIOC_CR1 |= (1 << 4);
    delay_us(30);

    /* Chờ DHT11 kéo LOW (khoảng 80us) */
    timeout = 0;
    while((GPIOC_IDR & (1 << 4)) != 0) {
        if (++timeout > 2000) return 0;
    }

    /* Chờ DHT11 kéo HIGH (khoảng 80us) */
    timeout = 0;
    while((GPIOC_IDR & (1 << 4)) == 0) {
        if (++timeout > 2000) return 0;
    }

    /* Chờ DHT11 kéo LOW kết thúc xung mở đầu */
    timeout = 0;
    while((GPIOC_IDR & (1 << 4)) != 0) {
        if (++timeout > 2000) return 0;
    }

    /* 3. ĐỌC 40 BIT DỮ LIỆU (5 BYTES) */
    for(i = 0; i < 5; i++) {
        unsigned char b = 0;
        for(j = 0; j < 8; j++) {
            /* Chờ kết thúc xung 50us LOW bắt đầu mỗi bit */
            timeout = 0;
            while((GPIOC_IDR & (1 << 4)) == 0) {
                if (++timeout > 2000) return 0;
            }

            /* Chân DATA đã lên HIGH. Đợi 40us để lấy mẫu phân biệt bit 0 / bit 1
               - Bit 0: Xung HIGH chỉ kéo dài ~28us -> Sau 40us chân đã tụt LOW
               - Bit 1: Xung HIGH kéo dài ~70us    -> Sau 40us chân vẫn đang HIGH */
            delay_us(40);

            if(GPIOC_IDR & (1 << 4)) {
                b = (b << 1) | 1;
                /* Chờ cho đến khi xung HIGH kết thúc */
                timeout = 0;
                while((GPIOC_IDR & (1 << 4)) != 0) {
                    if (++timeout > 2000) return 0;
                }
            } else {
                b = (b << 1);
            }
        }
        dht_bytes[i] = b;
    }

    /* 4. KIỂM TRA CHECKSUM TOÀN VẸN CỦA DHT11 */
    if (dht_bytes[4] == (unsigned char)(dht_bytes[0] + dht_bytes[1] + dht_bytes[2] + dht_bytes[3])) {
        *humi = dht_bytes[0]; /* Độ ẩm nguyên (20-90%) */
        *temp = dht_bytes[2]; /* Nhiệt độ nguyên (0-50°C) */
        return 1;
    }

    return 0;
}

void main(void)
{
    unsigned int poll_counter;

    /* Cấu hình xung nhịp CPU 16MHz không chia */
    CLK_CKDIVR = 0x00; 

    /* Cấu hình LED trạng thái (PC3 - Output Push-Pull) */
    GPIOC_DDR |= (1 << 3);  
    GPIOC_CR1 |= (1 << 3);  
    GPIOC_ODR &= ~(1 << 3); 

    /* Cấu hình chân DATA DHT11 (PC4 - Input Pull-up ban đầu) */
    GPIOC_DDR &= ~(1 << 4);
    GPIOC_CR1 |= (1 << 4);

    /* Cấu hình UART1 (9600 bps @ 16MHz) */
    UART1_BRR2 = 0x03;
    UART1_BRR1 = 0x68;
    UART1_CR2  = 0x2C; /* Bật TEN, REN và ngắt RIEN */

    /* Cho phép ngắt toàn cục */
    _asm("rim");

    /* Chờ cảm biến ổn định nguồn 1 giây */
    delay_ms(1000);

    /* Đọc lần đầu tiên */
    if (DHT11_Read(&current_temp, &current_humi)) {
        sensor_status = 1;
    } else {
        sensor_status = 0;
    }

    poll_counter = 0;

    while(1)
    {
        /* Đọc cảm biến định kỳ mỗi ~1.5 giây */
        if (++poll_counter >= 1500) {
            unsigned char t, h;
            poll_counter = 0;
            if (DHT11_Read(&t, &h)) {
                current_temp = t;
                current_humi = h;
                sensor_status = 1;
                /* Nháy nhẹ LED PC3 báo đọc thành công */
                GPIOC_ODR |= (1 << 3);
                delay_ms(10);
                GPIOC_ODR &= ~(1 << 3);
            } else {
                sensor_status = 0;
            }
        }
        delay_ms(1);

        /* Kiểm tra khi có lệnh truy vấn từ Master Gateway */
        if (global_dht_ready) 
        {
            /* KHAI BÁO TOÀN BỘ BIẾN Ở ĐẦU BLOCK (CHUẨN ANSI C89) */
            unsigned char cmd;
            unsigned char ack_data;
            unsigned char checksum;

            cmd = global_dht_cmd;
            global_dht_ready = 0;

            /* XỬ LÝ MÃ LỆNH:
               1. 0x70 hoặc 0x71: Đọc Nhiệt độ (°C)
               2. 0x72          : Đọc Độ ẩm (%RH)
               3. 0x73          : Đọc trạng thái cảm biến (1 = OK, 0 = Lỗi)
            */
            if (cmd == 0x70 || cmd == 0x71) {
                ack_data = current_temp;
            }
            else if (cmd == 0x72) {
                ack_data = current_humi;
            }
            else if (cmd == 0x73) {
                ack_data = sensor_status;
            }
            else {
                ack_data = current_temp;
            }

            /* Nghỉ 2ms để xả bộ đệm Repeater trên vòng Ring */
            delay_ms(2);

            /* PHÁT GÓI PHẢN HỒI THẬT (FRAME 2) VỀ GATEWAY */
            checksum = (unsigned char)(SLAVE_ID + ack_data);

            UART_SendByte(0xAA);
            UART_SendByte(SLAVE_ID);
            UART_SendByte(ack_data);
            UART_SendByte(checksum);
        }
    }
}

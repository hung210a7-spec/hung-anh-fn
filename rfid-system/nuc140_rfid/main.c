/*
 * =============================================================================
 *  HỆ THỐNG KIỂM SOÁT RA VÀO RFID
 *  Vi điều khiển: Nuvoton Nu-LB-NUC140 (NUC140VE3AN)
 *  RFID Module  : RC522 (MFRC522)
 *  IDE          : Keil MDK-ARM
 *  BSP          : NUC100Series BSP v3.x
 *
 * =============================================================================
 *  SƠ ĐỒ NỐI CHÂN (PIN CONNECTION DIAGRAM)
 * =============================================================================
 *
 *  RC522 Module          Nu-LB-NUC140 Board
 *  ─────────────         ────────────────────────────────
 *  VCC  (3.3V)    ───→   VCC 3.3V  (chân nguồn 3.3V trên board)
 *  GND            ───→   GND       (chân GND trên board)
 *  RST            ───→   PA.0      (GPIO - chân reset mềm)
 *  SDA (CS/SS)    ───→   PC.0      (SPI0_SS0  - Chip Select)
 *  SCK            ───→   PC.1      (SPI0_CLK  - Clock)
 *  MISO           ───→   PC.2      (SPI0_MISO0- Data ra từ RC522)
 *  MOSI           ───→   PC.3      (SPI0_MOSI0- Data vào RC522)
 *  IRQ            ───→   Không nối (không dùng)
 *
 *  ⚠️  LƯU Ý: RC522 hoạt động ở 3.3V. KHÔNG dùng 5V sẽ hỏng module!
 *
 * =============================================================================
 *  OUTPUT:
 *  - UART0 (115200 baud) → Serial Monitor trong Keil MDK
 *  - LCD 128x64 (trên board) → Hiển thị UID thẻ
 * =============================================================================
 */

#include <stdio.h>
#include <string.h>
#include "NUC100Series.h"

/* ============================================================
 *  ĐỊNH NGHĨA CHÂN PIN
 * ============================================================ */
/* RC522 Reset Pin - dùng PA.0 */
#define RC522_RST_PIN       PA0
#define RC522_RST_LOW()     PA0 = 0
#define RC522_RST_HIGH()    PA0 = 1

/* RC522 Chip Select - SPI0_SS0 = PC.0 (điều khiển bằng tay) */
#define RC522_CS_LOW()      SPI_SET_SS_LOW(SPI0)
#define RC522_CS_HIGH()     SPI_SET_SS_HIGH(SPI0)

/* ============================================================
 *  ĐỊA CHỈ THANH GHI RC522 (MFRC522)
 * ============================================================ */
#define RC522_REG_COMMAND        0x01
#define RC522_REG_COM_IRQ        0x04
#define RC522_REG_DIV_IRQ        0x05
#define RC522_REG_ERROR          0x06
#define RC522_REG_STATUS2        0x08
#define RC522_REG_FIFO_DATA      0x09
#define RC522_REG_FIFO_LEVEL     0x0A
#define RC522_REG_CONTROL        0x0C
#define RC522_REG_BIT_FRAMING    0x0D
#define RC522_REG_MODE           0x11
#define RC522_REG_TX_CONTROL     0x14
#define RC522_REG_TX_AUTO        0x15
#define RC522_REG_CRC_RESULT_H   0x21
#define RC522_REG_CRC_RESULT_L   0x22
#define RC522_REG_T_MODE         0x2A
#define RC522_REG_T_PRESCALER    0x2B
#define RC522_REG_T_RELOAD_H     0x2C
#define RC522_REG_T_RELOAD_L     0x2D
#define RC522_REG_VERSION        0x37

/* RC522 Commands */
#define RC522_CMD_IDLE           0x00
#define RC522_CMD_CALC_CRC       0x03
#define RC522_CMD_TRANSCEIVE     0x0C
#define RC522_CMD_SOFT_RESET     0x0F

/* RFID Commands (ISO 14443A) */
#define PICC_REQIDL              0x26  /* Tìm thẻ ở chế độ idle */
#define PICC_REQALL              0x52  /* Tìm tất cả thẻ */
#define PICC_ANTICOLL            0x93  /* Chống xung đột */

/* Trạng thái trả về */
#define STATUS_OK                0
#define STATUS_ERROR             1
#define STATUS_TIMEOUT           2

/* ============================================================
 *  DELAY ĐƠN GIẢN
 * ============================================================ */
void delay_ms(uint32_t ms)
{
    uint32_t i, j;
    for (i = 0; i < ms; i++)
        for (j = 0; j < 4800; j++); /* ~1ms tại 48MHz */
}

/* ============================================================
 *  KHỞI TẠO HỆ THỐNG CLOCK
 * ============================================================ */
void SYS_Init(void)
{
    /* Mở khóa thanh ghi bảo vệ */
    SYS_UnlockReg();

    /* Bật oscillator ngoài HXT (12MHz) */
    CLK_EnableXtalRC(CLK_PWRCON_XTL12M_EN_Msk);
    CLK_WaitClockReady(CLK_CLKSTATUS_XTL12M_STB_Msk);

    /* Cấu hình PLL: 12MHz × 4 = 48MHz */
    CLK_SetCoreClock(48000000);

    /* Bật clock cho các peripheral */
    CLK_EnableModuleClock(SPI0_MODULE);
    CLK_EnableModuleClock(UART0_MODULE);

    /* Cấu hình clock source */
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART_S_HXT, CLK_CLKDIV_UART(1));
    CLK_SetModuleClock(SPI0_MODULE,  CLK_CLKSEL1_SPI0_S_HCLK, 0);

    /* ==================================================
     *  CẤU HÌNH CHÂN PIN (Multi-Function Pin)
     * ==================================================
     *  PA.0  → GPIO Output (RC522 RST)
     *  GPA_MFP: PA.0 = GPIO
     *
     *  PC.0  → SPI0_SS0  (RC522 CS)
     *  PC.1  → SPI0_CLK  (RC522 SCK)
     *  PC.2  → SPI0_MISO0(RC522 MISO)
     *  PC.3  → SPI0_MOSI0(RC522 MOSI)
     *
     *  PB.0  → UART0_RXD
     *  PB.1  → UART0_TXD
     * ================================================== */

    /* PA.0 = GPIO cho RST */
    SYS->GPA_MFP &= ~SYS_GPA_MFP_PA0_MFP_Msk;

    /* PC.0~3 = SPI0 */
    SYS->GPC_MFP = (SYS->GPC_MFP & ~(SYS_GPC_MFP_PC0_MFP_Msk |
                                       SYS_GPC_MFP_PC1_MFP_Msk |
                                       SYS_GPC_MFP_PC2_MFP_Msk |
                                       SYS_GPC_MFP_PC3_MFP_Msk))
                 | SYS_GPC_MFP_PC0_SPI0_SS0
                 | SYS_GPC_MFP_PC1_SPI0_CLK
                 | SYS_GPC_MFP_PC2_SPI0_MISO0
                 | SYS_GPC_MFP_PC3_SPI0_MOSI0;

    /* PB.0=RX, PB.1=TX cho UART0 */
    SYS->GPB_MFP = (SYS->GPB_MFP & ~(SYS_GPB_MFP_PB0_MFP_Msk |
                                       SYS_GPB_MFP_PB1_MFP_Msk))
                 | SYS_GPB_MFP_PB0_UART0_RXD
                 | SYS_GPB_MFP_PB1_UART0_TXD;

    /* PA.0 = Ngõ ra (RST cho RC522) */
    GPIO_SetMode(PA, BIT0, GPIO_PMD_OUTPUT);
    RC522_RST_HIGH(); /* Không reset */

    SYS_LockReg();
}

/* ============================================================
 *  KHỞI TẠO UART0 (115200 baud, 8N1)
 * ============================================================ */
void UART0_Init(void)
{
    UART_Open(UART0, 115200);
}

/* ============================================================
 *  KHỞI TẠO SPI0 CHO RC522
 *  - Mode 0 (CPOL=0, CPHA=0)
 *  - MSB first
 *  - 8-bit
 *  - Clock: 1MHz (RC522 hỗ trợ tối đa 10MHz)
 * ============================================================ */
void SPI0_Init(void)
{
    SPI_Open(SPI0, SPI_MASTER, SPI_MODE_0, 8, 1000000);
    SPI_EnableAutoSS(SPI0, SPI_SS_ACTIVE_LOW, SPI_SS_ACTIVE_LOW);
    SPI_DisableAutoSS(SPI0); /* CS điều khiển tay */
}

/* ============================================================
 *  TRUYỀN NHẬN SPI 1 BYTE
 * ============================================================ */
uint8_t SPI_TransferByte(uint8_t data)
{
    SPI_WRITE_TX0(SPI0, data);
    SPI_TRIGGER(SPI0);
    while (SPI_IS_BUSY(SPI0));
    return (uint8_t)SPI_READ_RX0(SPI0);
}

/* ============================================================
 *  ĐỌC/GHI THANH GHI RC522
 * ============================================================ */
uint8_t RC522_ReadReg(uint8_t reg)
{
    uint8_t val;
    RC522_CS_LOW();
    SPI_TransferByte((reg << 1) | 0x80); /* Byte địa chỉ: Read + addr */
    val = SPI_TransferByte(0x00);        /* Đọc dữ liệu */
    RC522_CS_HIGH();
    return val;
}

void RC522_WriteReg(uint8_t reg, uint8_t data)
{
    RC522_CS_LOW();
    SPI_TransferByte((reg << 1) & 0x7E); /* Byte địa chỉ: Write + addr */
    SPI_TransferByte(data);
    RC522_CS_HIGH();
}

void RC522_SetBitMask(uint8_t reg, uint8_t mask)
{
    RC522_WriteReg(reg, RC522_ReadReg(reg) | mask);
}

void RC522_ClearBitMask(uint8_t reg, uint8_t mask)
{
    RC522_WriteReg(reg, RC522_ReadReg(reg) & (~mask));
}

/* ============================================================
 *  KHỞI TẠO VÀ RESET RC522
 * ============================================================ */
void RC522_Reset(void)
{
    RC522_RST_LOW();
    delay_ms(10);
    RC522_RST_HIGH();
    delay_ms(50);
    RC522_WriteReg(RC522_REG_COMMAND, RC522_CMD_SOFT_RESET);
    delay_ms(10);
}

void RC522_Init(void)
{
    RC522_Reset();

    /* Cấu hình timer */
    RC522_WriteReg(RC522_REG_T_MODE,      0x8D);
    RC522_WriteReg(RC522_REG_T_PRESCALER, 0x3E);
    RC522_WriteReg(RC522_REG_T_RELOAD_L,  0x1E);
    RC522_WriteReg(RC522_REG_T_RELOAD_H,  0x00);

    /* Cấu hình TX */
    RC522_WriteReg(RC522_REG_TX_AUTO,  0x40);
    RC522_WriteReg(RC522_REG_MODE,     0x3D);

    /* Bật antenna */
    RC522_SetBitMask(RC522_REG_TX_CONTROL, 0x03);

    /* In version lên UART */
    printf("RC522 Version: 0x%02X\r\n", RC522_ReadReg(RC522_REG_VERSION));
    /* Kết quả đúng: 0x91 (v1.0) hoặc 0x92 (v2.0) */
}

/* ============================================================
 *  TRUYỀN/NHẬN LỆNH VỚI THẺ RFID
 * ============================================================ */
uint8_t RC522_ToCard(uint8_t cmd, uint8_t *send_data, uint8_t send_len,
                     uint8_t *recv_data, uint32_t *recv_bits)
{
    uint8_t status = STATUS_ERROR;
    uint8_t irq_en  = 0x00;
    uint8_t wait_for = 0x00;
    uint8_t last_bits;
    uint8_t n;
    uint32_t i;

    if (cmd == RC522_CMD_TRANSCEIVE) {
        irq_en   = 0x77;
        wait_for = 0x30;
    }

    RC522_WriteReg(RC522_REG_COM_IRQ, 0x00);
    RC522_WriteReg(RC522_REG_COMMAND, RC522_CMD_IDLE);
    RC522_SetBitMask(RC522_REG_COM_IRQ,  irq_en | 0x80);
    RC522_ClearBitMask(RC522_REG_FIFO_LEVEL, 0x80); /* Xóa FIFO */

    /* Ghi dữ liệu vào FIFO */
    for (i = 0; i < send_len; i++)
        RC522_WriteReg(RC522_REG_FIFO_DATA, send_data[i]);

    RC522_WriteReg(RC522_REG_COMMAND, cmd);

    if (cmd == RC522_CMD_TRANSCEIVE)
        RC522_SetBitMask(RC522_REG_BIT_FRAMING, 0x80); /* StartSend */

    /* Chờ kết quả (timeout ~25ms) */
    i = 2000;
    do {
        n = RC522_ReadReg(RC522_REG_COM_IRQ);
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & wait_for));

    RC522_ClearBitMask(RC522_REG_BIT_FRAMING, 0x80);

    if (i != 0) {
        if (!(RC522_ReadReg(RC522_REG_ERROR) & 0x1B)) {
            status = STATUS_OK;
            if (n & irq_en & 0x01)
                status = STATUS_TIMEOUT;

            if (cmd == RC522_CMD_TRANSCEIVE) {
                n         = RC522_ReadReg(RC522_REG_FIFO_LEVEL);
                last_bits = RC522_ReadReg(RC522_REG_CONTROL) & 0x07;
                *recv_bits = (last_bits != 0) ? (n - 1) * 8 + last_bits : n * 8;

                if (n == 0) n = 1;
                for (i = 0; i < n; i++)
                    recv_data[i] = RC522_ReadReg(RC522_REG_FIFO_DATA);
            }
        }
    } else {
        status = STATUS_TIMEOUT;
    }

    return status;
}

/* ============================================================
 *  TÌM THẺ (REQUEST)
 * ============================================================ */
uint8_t RC522_Request(uint8_t req_mode, uint8_t *tag_type)
{
    uint8_t  status;
    uint32_t backBits;

    RC522_WriteReg(RC522_REG_BIT_FRAMING, 0x07);
    tag_type[0] = req_mode;
    status = RC522_ToCard(RC522_CMD_TRANSCEIVE, tag_type, 1, tag_type, &backBits);

    if ((status != STATUS_OK) || (backBits != 0x10))
        status = STATUS_ERROR;

    return status;
}

/* ============================================================
 *  CHỐNG XUNG ĐỘT - LẤY UID THẺ
 * ============================================================ */
uint8_t RC522_Anticoll(uint8_t *ser_num)
{
    uint8_t  status;
    uint8_t  serial_check = 0;
    uint32_t unLen;
    uint8_t  ser_num_check[2];
    uint8_t  i;

    RC522_WriteReg(RC522_REG_BIT_FRAMING, 0x00);
    ser_num[0] = PICC_ANTICOLL;
    ser_num[1] = 0x20;
    status = RC522_ToCard(RC522_CMD_TRANSCEIVE, ser_num, 2, ser_num, &unLen);

    if (status == STATUS_OK) {
        /* Kiểm tra checksum UID (5 byte: 4 UID + 1 XOR) */
        for (i = 0; i < 4; i++)
            serial_check ^= ser_num[i];
        if (serial_check != ser_num[4])
            status = STATUS_ERROR;
    }

    return status;
}

/* ============================================================
 *  HIỂN THỊ UID LÊN UART (Serial Monitor)
 * ============================================================ */
void Display_CardUID(uint8_t *uid)
{
    printf("\r\n");
    printf("==================================\r\n");
    printf("  THE RFID DA QUET THANH CONG!   \r\n");
    printf("==================================\r\n");
    printf("  UID: %02X:%02X:%02X:%02X\r\n",
           uid[0], uid[1], uid[2], uid[3]);
    printf("==================================\r\n");
}

/* ============================================================
 *  HIỂN THỊ LÊN LCD (Bitbang SPI qua GPIO)
 *
 *  LCD trên Nu-LB-NUC140 dùng controller ST7579
 *  Kết nối (theo schematic NuVoton):
 *    GPD.8  = LCD_CS
 *    GPD.9  = LCD_CLK
 *    GPD.10 = LCD_DATA
 *    GPD.11 = LCD_RS (A0)
 *
 *  Đây là bản đơn giản hóa - vẽ UID lên LCD bằng printf
 *  qua UART. Nếu muốn LCD thật, cần driver ST7579 đầy đủ.
 *
 *  ➡️  Mình có thể viết LCD driver đầy đủ ở bước tiếp theo
 * ============================================================ */

/* ============================================================
 *  HÀM MAIN
 * ============================================================ */
int main(void)
{
    uint8_t tag_type[2];
    uint8_t uid[5];
    uint8_t status;
    uint8_t last_uid[4] = {0, 0, 0, 0};

    /* Khởi tạo hệ thống */
    SYS_Init();
    UART0_Init();
    SPI0_Init();

    printf("\r\n");
    printf("==========================================\r\n");
    printf("  HE THONG KIEM SOAT RA VAO RFID         \r\n");
    printf("  Nu-LB-NUC140 + RC522                   \r\n");
    printf("==========================================\r\n");

    /* Khởi tạo RC522 */
    RC522_Init();
    printf("San sang doc the RFID...\r\n");
    printf("Vui long quet the vao dau doc!\r\n\r\n");

    while (1)
    {
        /* Bước 1: Tìm thẻ */
        status = RC522_Request(PICC_REQIDL, tag_type);

        if (status == STATUS_OK)
        {
            /* Bước 2: Lấy UID thẻ */
            status = RC522_Anticoll(uid);

            if (status == STATUS_OK)
            {
                /* Tránh hiện thị lặp lại cùng 1 thẻ */
                if (uid[0] != last_uid[0] || uid[1] != last_uid[1] ||
                    uid[2] != last_uid[2] || uid[3] != last_uid[3])
                {
                    /* Lưu UID vừa đọc */
                    memcpy(last_uid, uid, 4);

                    /* Hiển thị lên UART */
                    Display_CardUID(uid);
                }
            }
        }
        else
        {
            /* Không thấy thẻ → reset UID cuối */
            memset(last_uid, 0, 4);
        }

        delay_ms(200);
    }
}

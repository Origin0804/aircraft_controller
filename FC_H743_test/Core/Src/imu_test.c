/**
  ******************************************************************************
  * @file    imu_test.c
  * @brief   ICM-42670-P 六轴数据读取并打印到 USART1（SPI1, CS=PC4 软件片选）
  * @note    只依赖 SPI1 + USART1 + GPIO，不碰 I2C/SDMMC/TIM/ADC。
  ******************************************************************************
  */
#include "main.h"
#include "spi.h"
#include "usart.h"
#include "imu_test.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ---------------- ICM-42670-P 寄存器（Bank 0） ----------------
 * WHO_AM_I 的地址与取值是确定的（0x75 -> 0x67，本板自检也按此判据）。
 * 其余地址沿用主工程 FC_H743/Core/Src/fc/fc_imu.c 的记录，但那份记录里
 * 温度寄存器写的是 0x09，与常见资料的 TEMP_DATA1=0x1D 冲突，所以本文件不
 * 假定答案：启动时先把 0x00~0x2F 整段转储出来，并对两个候选地址各读一次，
 * 实机比对后再把表定死。 */
#define ICM_WHO_AM_I       0x75
#define ICM_PWR_MGMT0      0x1F
#define ICM_GYRO_CONFIG0   0x20
#define ICM_ACCEL_CONFIG0  0x21
#define ICM_ACCEL_DATA     0x0B
#define ICM_GYRO_DATA      0x11
#define ICM_TEMP_CAND_A    0x09   /* fc_imu.c 记录的地址 */
#define ICM_TEMP_CAND_B    0x1D   /* 常见资料里 TEMP_DATA1 的地址 */

#define ICM_DEVICE_ID      0x67

#define ICM_ACCEL_LSB_G    4096.0f   /* ±8g      */
#define ICM_GYRO_LSB_DPS   16.4f     /* ±2000dps */

#define ICM_PWR_LN         0x0F      /* accel 低噪(0b11) | gyro 低噪(0b11<<2) */
#define ICM_GYRO_CFG       0x05      /* FS=±2000dps | ODR=1.6kHz */
#define ICM_ACCEL_CFG      0x25      /* FS=±8g      | ODR=1.6kHz */

#define ICM_SPI_TIMEOUT    100U

/* ---------------- 诊断结果镜像到 RAM（供 SWD 读取） ----------------
 * 动机：串口不总是通（本板控制台走 CH340N，接不上时整轮诊断等于白跑）。
 * 于是把打印出来的每一个字同时写进一块 RAM，之后用 ST-Link 直接抓：
 *
 *   openocd -f interface/stlink.cfg -f target/stm32h7x.cfg \
 *     -c "init; halt; dump_image /tmp/diag.bin 0x<g_diag地址> 8200; resume; shutdown"
 *
 * 【为什么用普通全局变量，而不是写死 0x24000000(AXI SRAM)】
 * 本工程开了 D-Cache。AXI SRAM 是默认 Write-Back 可缓存区，往那儿写的字可能
 * 还留在 D-Cache 里没落到 SRAM，而 SWD 读的是物理内存 —— 调试器会读到旧内容，
 * 整条"无串口取结果"的路就废了。改用普通全局变量：链接脚本把它放进
 * DTCM(0x20000000)，**TCM 不经过 D-Cache**，写进去立刻可见。
 * 地址不写死，用 arm-none-eabi-nm 从 elf 里取符号 g_diag 即可。 */
#define DIAG_MAGIC  0x47414944u     /* 'D''I''A''G' 小端 */
#define DIAG_CAP    8180u
#define DIAG_KEEP   6144u           /* 写满后保留的"最近日志"长度 */

typedef struct {
    uint32_t magic;
    uint32_t len;
    char     text[DIAG_CAP];
} diag_t;

static volatile diag_t g_diag;

/* 【为什么要做"保留最近"而不是"满了就停"】
 * 诊断文本本身就有 10KB 以上，比 DIAG_CAP 还长，原来的"满了就不再写"会让 SWD
 * 读到的永远是【开机头 8KB】—— 恰好是探针阶段，而最需要看的 10Hz 数据流在最后，
 * 一个字都进不来。改成写满就把最旧的丢掉、把尾巴挪到开头继续追加，
 * 读到的就始终是最近 DIAG_KEEP 字节，数据流才有机会被看到。 */
static void diag_append(const char *s, int n)
{
    if (n <= 0) return;
    if ((uint32_t)n > DIAG_CAP)                 /* 单条就超长：只留最后 DIAG_CAP 字节 */
    {
        s += ((uint32_t)n - DIAG_CAP);
        n  = (int)DIAG_CAP;
    }

    uint32_t len = g_diag.len;
    if (len + (uint32_t)n > DIAG_CAP)
    {
        if (len > DIAG_KEEP)
        {
            uint32_t drop = len - DIAG_KEEP;
            memmove((char *)g_diag.text, (const char *)g_diag.text + drop, DIAG_KEEP);
            len = DIAG_KEEP;
        }
        else
        {
            len = 0;                            /* 极端情况：直接从头来 */
        }
    }

    uint32_t room = DIAG_CAP - len;
    uint32_t k = ((uint32_t)n < room) ? (uint32_t)n : room;
    for (uint32_t i = 0; i < k; ++i) g_diag.text[len + i] = s[i];
    g_diag.len = len + k;
}

static void diag_reset(void)
{
    g_diag.len   = 0;
    g_diag.magic = DIAG_MAGIC;
}

/* ---------------- 串口输出（同时镜像进 RAM） ---------------- */
static void uprint(const char *s)
{
    if (!s) return;
    size_t n = strlen(s);
    diag_append(s, (int)n);
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)n, 200);
}

static void uprintf(const char *fmt, ...)
{
    char buf[192];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0)
    {
        if (n > (int)sizeof(buf) - 1) n = (int)sizeof(buf) - 1;
        diag_append(buf, n);
        (void)HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)n, 200);
    }
}

/* ---------------- IMU_INT1(PC5) 体检 ----------------
 * 目的：在完全绕开 SPI 的前提下，判断"芯片到底活没活"。
 * 芯片有供电且正常工作的话，INT1 是它的输出脚，会主动驱动这根线；
 * 芯片没供电/已损坏/INT1 没接过来，这根线就是悬空的。
 * 判据同 MISO：上拉/下拉读数【相同】= 有人在驱动；【不同】= 悬空。
 * 注意：ICM-42670-P 复位后 INT_CONFIG 默认 INT1 为开漏，若中断未使能则它可能
 * 一直处于高阻态 —— 所以本测试"读到被驱动"是强证据，读到"悬空"只能算存疑。 */
static void imu_int_probe(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin  = GPIO_PIN_5;
    g.Mode = GPIO_MODE_INPUT;

    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_Delay(2);
    int pu = (int)HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5);

    g.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_Delay(2);
    int pd = (int)HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5);

    /* 还原成 gpio.c 原本的配置（上升沿中断 + 无上下拉）。
       本模式下 EXTI9_5 的 NVIC 已被关掉，不会产生中断风暴。 */
    g.Mode = GPIO_MODE_IT_RISING;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);

    uprint("--- IMU_INT1(PC5) 体检 ---\r\n");
    uprintf("  上拉读=%d  下拉读=%d\r\n", pu, pd);
    if (pu == pd)
    {
        uprint("  -> 两者相同 = PC5 被主动驱动 => IMU 芯片是活的，问题在 SPI 那几根线\r\n");
    }
    else
    {
        uprint("  -> 两者不同 = PC5 悬空 => 芯片没在驱动它（没供电/已损坏/INT1 没接过来）\r\n");
        uprint("     （注意：开漏 INT 未使能时本来就可能是高阻，此结果只能存疑，不能据此断定芯片坏）\r\n");
    }
    uprint("\r\n");
}

/* ---------------- SPI 基础（CS = PC4 低有效，位 7 = 读标志） ---------------- */
static void cs_low(void)  { HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET); }
static void cs_high(void) { HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET); }

/* 【STM32H7 主模式 SPI 的坑】HAL 在 SPI_CloseTransfer() 里读 SR 的 UDR 位判错，
 * 而该位在 CMSIS 中的注释是 "UDR at Slave transmission"（从模式欠载），主模式下
 * 会因使能 SPI 时 TXFIFO 尚空而被置起，于是每次传输都返回 HAL_ERROR —— 但字节
 * 其实已经移位完成。反证：若 SPI 内核时钟真的没跑，HAL 会因等不到 EOT 返回
 * HAL_TIMEOUT(3)，而我们拿到的是 HAL_ERROR(1)，说明传输是走完了的。
 * 因此这里不拿 HAL 返回值当失败判据：记录状态供打印，清掉 UDR 标志，
 * 照常取用接收缓冲。数据是否有效由调用方按内容判断（全 0/全 FF = 没通信）。 */
static HAL_StatusTypeDef g_last_st;
static uint32_t          g_last_err;

static void spi_note(HAL_StatusTypeDef st)
{
    g_last_st  = st;
    g_last_err = hspi1.ErrorCode;
    hspi1.ErrorCode = HAL_SPI_ERROR_NONE;
    __HAL_SPI_CLEAR_UDRFLAG(&hspi1);
}

static int spi_write(uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { (uint8_t)(reg & 0x7F), val };
    cs_low();
    HAL_StatusTypeDef st = HAL_SPI_Transmit(&hspi1, tx, 2, ICM_SPI_TIMEOUT);
    cs_high();
    spi_note(st);
    return 1;
}

static int spi_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    /* 必须容得下 len+1（首字节是"寄存器地址|0x80"）。原来开 48 而转储要读 48 字节，
       49 > 48 直接返回失败，导致寄存器转储永远报"无响应"——是缓冲区太小，不是 IMU 的问题。 */
    uint8_t tx[64];
    uint8_t rx[64];
    if ((uint16_t)(len + 1) > sizeof(tx)) return 0;

    tx[0] = (uint8_t)(reg | 0x80);
    memset(&tx[1], 0, len);
    cs_low();
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&hspi1, tx, rx, (uint16_t)(len + 1), ICM_SPI_TIMEOUT);
    cs_high();
    spi_note(st);

    /* 【重要教训】传输没完成时 rx 里是栈残留垃圾，绝不是寄存器内容。
     * 早先这里无条件 return 1，导致把稳定的垃圾值误当成"芯片应答"——三个不同寄存器
     * （0x00/0x01/0x75）都读出同一个 0x48、转储里 16 字节模式逐行完全重复，
     * 就是栈残留的典型特征，白绕了一整轮。现在只有 HAL 明确返回 OK 才算数据有效。 */
    if (st != HAL_OK)
    {
        memset(buf, 0, len);
        return 0;
    }
    memcpy(buf, &rx[1], len);
    return 1;
}

static int spi_read8(uint8_t reg, uint8_t *v) { return spi_read(reg, v, 1); }

static int16_t be16(const uint8_t *p) { return (int16_t)((p[0] << 8) | p[1]); }

/* ---------------- MISO 线体检 ----------------
 * 读到的数据全 0x00 时，要区分两种硬件原因：
 *   (a) MISO 被硬短到 GND（虚焊/桥连，或 IMU 贴反后 SDO 落在 GND 焊盘上）
 *   (b) MISO 只是浮空、IMU 不应答（未供电 / 焊点开路）
 * 做法：把 PA6 从 SPI 复用临时改成"输入 + 内部上拉"（H7 内部上拉约 40kΩ）。
 *   上拉仍读 0 → 有东西把它硬拽到地，是硬短路      -> (a)
 *   上拉读 1   → 线能浮起来，没有短路，只是没人驱动 -> (b)
 * 测完必须把 PA6 换回 AF5 复用，否则 SPI 收不到数据。 */
static void miso_line_probe(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin  = GPIO_PIN_6;
    g.Mode = GPIO_MODE_INPUT;

    /* 步骤 1：CS 拉高（IMU 不被选中），只开内部上拉。
     * 40k 上拉都拉不到 1 → 线上有硬短路到地。 */
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_Delay(2);
    int idle_pu = (int)HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6);

    /* 步骤 2：CS 拉低选中 IMU，再用上拉/下拉做差分。
     * IMU 活着且被选中时 SDO 会主动驱动，两次读数必然相同（驱动器压过 40k 弱上下拉）；
     * 若两次不同，说明线上没人驱动，是纯悬空。 */
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_Delay(2);
    int cs_pu = (int)HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6);

    g.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_Delay(2);
    int cs_pd = (int)HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6);

    /* 复原：CS 拉高空闲，PA6 换回 SPI1 复用（AF5） */
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &g);

    uprint("--- MISO(PA6) 线体检 ---\r\n");
    uprintf("  CS 高、上拉   (idle_pu) : %d\r\n", idle_pu);
    uprintf("  CS 低、上拉   (cs_pu)   : %d\r\n", cs_pu);
    uprintf("  CS 低、下拉   (cs_pd)   : %d\r\n", cs_pd);

    if (idle_pu == 0)
    {
        uprint("  -> 上拉都拉不起来 = MISO 硬短到地。查 PA6 对地桥连，\r\n"
               "     以及 IMU 的 SDO 焊盘是否压在 GND 上。\r\n");
    }
    else if (cs_pu != cs_pd)
    {
        uprint("  -> 上拉/下拉读数不同 = 线上没人驱动，MISO 悬空。\r\n"
               "     CS 已拉低从机仍不接管数据线，三种可能：\r\n"
               "       (1) IMU 没供电  —— 量 IMU VDD 对 GND 是否 3.3V\r\n"
               "       (2) CS 没送到   —— 量 PC4 到 IMU 的 CS 脚是否通\n"
               "       (3) SDO 开路    —— 量 IMU 的 SDO 脚到 PA6 是否通\r\n");
    }
    else
    {
        uprint("  -> 上拉/下拉读数相同 = MISO 被主动驱动，IMU 是活的！\r\n"
               "     那问题不在连线，而在 SPI 时序/模式或寄存器表。\r\n");
    }
    uprint("\r\n");
}

/* ---------------- GPIO bit-bang SPI（绕开 STM32 SPI 外设） ----------------
 * 目的：把"SPI 外设配置问题"和"线/焊接/芯片问题"分开。
 * 用的是同一组引脚(PA5/PA6/PA7)，但走普通 GPIO 推挽输出，不经过 SPI 外设的
 * 移位器和时钟发生器。若 bit-bang 能读出 0x67 而 SPI 外设读不到，就说明是
 * 外设配置/HAL 的问题；若两者都读不到，就是引脚、焊接或芯片的问题。
 * 时序按 SPI 模式 0（CPOL=0/CPHA=0）：SCK 空闲低，上升沿采样。 */
static void bb_delay(void)
{
    /* 480MHz 下约 1µs，对应几百 kHz 的 SCK —— ICM-42670 支持到 DC，越慢越稳 */
    for (volatile int i = 0; i < 60; ++i) { }
}

static void bb_pins_mode(int gpio_mode)
{
    GPIO_InitTypeDef g = {0};
    if (gpio_mode)
    {
        /* bit-bang：SCK/MOSI 推挽输出，MISO 输入 */
        g.Mode  = GPIO_MODE_OUTPUT_PP;
        g.Pull  = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_LOW;
        g.Pin   = GPIO_PIN_5 | GPIO_PIN_7;
        HAL_GPIO_Init(GPIOA, &g);

        g.Pin  = GPIO_PIN_6;
        g.Mode = GPIO_MODE_INPUT;
        HAL_GPIO_Init(GPIOA, &g);
    }
    else
    {
        /* 换回 SPI1 复用（AF5） */
        g.Pin       = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
        g.Mode      = GPIO_MODE_AF_PP;
        g.Pull      = GPIO_NOPULL;
        g.Speed     = GPIO_SPEED_FREQ_LOW;
        g.Alternate = GPIO_AF5_SPI1;
        HAL_GPIO_Init(GPIOA, &g);
    }
}

/* 时钟/数据脚，做成变量是为了能测"SCK 与 MOSI 在 PCB 上接反"这种可能 */
static uint16_t g_bb_sck  = GPIO_PIN_5;
static uint16_t g_bb_mosi = GPIO_PIN_7;

static uint8_t bb_xfer(uint8_t b)
{
    uint8_t r = 0;
    for (int i = 7; i >= 0; --i)
    {
        HAL_GPIO_WritePin(GPIOA, g_bb_mosi,
                          (b & (1u << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        bb_delay();
        HAL_GPIO_WritePin(GPIOA, g_bb_sck, GPIO_PIN_SET);    /* 上升沿：双方采样 */
        bb_delay();
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == GPIO_PIN_SET) r = (uint8_t)(r | (1u << i));
        HAL_GPIO_WritePin(GPIOA, g_bb_sck, GPIO_PIN_RESET);  /* 下降沿：从机换下一位 */
        bb_delay();
    }
    return r;
}

static uint8_t bb_read_reg(uint8_t reg)
{
    uint8_t v;
    bb_pins_mode(1);
    HAL_GPIO_WritePin(GPIOA, g_bb_sck, GPIO_PIN_RESET);      /* SCK 空闲低 */
    cs_low();
    bb_delay();
    (void)bb_xfer((uint8_t)(reg | 0x80));                    /* 地址字节，位7=1 读 */
    v = bb_xfer(0x00);                                       /* 数据字节 */
    cs_high();
    bb_pins_mode(0);
    return v;
}

/* 把 SCK 与 MOSI 对调再读一次：用于排除"PCB 上这两根线接反" */
static uint8_t bb_read_reg_swapped(uint8_t reg)
{
    uint8_t v;
    g_bb_sck  = GPIO_PIN_7;
    g_bb_mosi = GPIO_PIN_5;
    v = bb_read_reg(reg);
    g_bb_sck  = GPIO_PIN_5;
    g_bb_mosi = GPIO_PIN_7;
    return v;
}

/* ---------------- SPI 模式切换 ----------------
 * 有些 IMU 只支持 SPI 模式 3（CPOL=1/CPHA=2EDGE）。模式不对时不会完全读不到，
 * 而是采样沿错位，读出一个【稳定但错误】的值 —— 与"读到 0x48 而型号应为 0x47/0x67"
 * 这种只差最低位的现象高度吻合。所以两种模式都试一遍再下结论。 */
static void spi_set_mode(uint32_t pol, uint32_t pha)
{
    hspi1.Init.CLKPolarity = pol;
    hspi1.Init.CLKPhase    = pha;
    (void)HAL_SPI_Init(&hspi1);
}

/* 宽范围转储：不同型号的寄存器布局不同，铺开看更容易识别 */
static void dump_wide(const char *tag)
{
    uint8_t b[16];
    uprintf("--- %s 寄存器 0x00~0x3F ---\r\n", tag);
    for (int row = 0; row < 4; ++row)
    {
        if (!spi_read((uint8_t)(row * 16), b, sizeof(b)))
        {
            uprint("  ! 读失败\r\n");
            return;
        }
        uprintf("  %02X:", row * 16);
        for (int c = 0; c < 16; ++c) uprintf(" %02X", b[c]);
        uprint("\r\n");
    }
}

/* ---------------- 寄存器转储（核对寄存器表用） ---------------- */
static void dump_regs(void)
{
    uint8_t b[48];
    uprint("--- 寄存器转储 0x00~0x2F（用于核对寄存器表）---\r\n");
    if (!spi_read(0x00, b, sizeof(b)))
    {
        uprint("  ! 转储失败：SPI 读无响应\r\n");
        return;
    }
    for (int row = 0; row < 3; ++row)
    {
        uprintf("  %02X:", row * 16);
        for (int c = 0; c < 16; ++c) uprintf(" %02X", b[row * 16 + c]);
        uprint("\r\n");
    }
}

/* ---------------- 把 SPI/CS/INT 的引脚恢复成正常复用状态 ----------------
 * 做"线的通断体检"时必须把引脚临时改成 GPIO 输入，测完一定要还原，
 * 否则后续 SPI 收发全废（这是之前排查时踩过的坑）。 */
static void spi_pins_af(void)
{
    GPIO_InitTypeDef g = {0};

    /* PA5/PA6/PA7 -> SPI1 AF5 */
    g.Pin       = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &g);

    /* PC4 -> 推挽输出，空闲高（片选无效） */
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);
    g.Pin   = IMU_CS_Pin;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(IMU_CS_GPIO_Port, &g);

    /* PC5 -> 输入（INT1 是 IMU 的输出） */
    g.Pin  = IMU_INT1_Pin;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(IMU_INT1_GPIO_Port, &g);
}

/* ---------------- 单根线的"通/断"体检（三态对比） ----------------
 * 用 40k 内部上拉/下拉各读一次，看这根线到底有没有接到东西：
 *   上=1 下=1 -> 有外部强上拉，或被器件主动驱动高   => 接上了
 *   上=0 下=0 -> 被驱动低 / 强下拉                  => 接上了
 *   上=1 下=0 -> 读数跟着内部上拉走 = 悬空           => 虚焊/开路（重点怀疑对象）
 *   上=0 下=1 -> 电气异常（少见）
 * 注意：SCK/MOSI 由 MCU 驱动，测的时候它们会被临时改成输入，属于"从线这一侧看"。 */
static void line_probe(GPIO_TypeDef *port, uint16_t pin, const char *name)
{
    GPIO_InitTypeDef g = {0};
    g.Pin  = pin;
    g.Mode = GPIO_MODE_INPUT;

    g.Pull = GPIO_PULLUP;   HAL_GPIO_Init(port, &g); HAL_Delay(3);
    int pu = (int)HAL_GPIO_ReadPin(port, pin);
    g.Pull = GPIO_PULLDOWN; HAL_GPIO_Init(port, &g); HAL_Delay(3);
    int pd = (int)HAL_GPIO_ReadPin(port, pin);
    g.Pull = GPIO_NOPULL;   HAL_GPIO_Init(port, &g); HAL_Delay(3);
    int np = (int)HAL_GPIO_ReadPin(port, pin);

    const char *verdict;
    if      (pu == 1 && pd == 1) verdict = "外部上拉/被驱动高 -> 接上了";
    else if (pu == 0 && pd == 0) verdict = "被驱动低/强下拉   -> 接上了";
    else if (pu == 1 && pd == 0) verdict = "悬空! 无外部连接  -> 虚焊/开路";
    else                         verdict = "电气异常";

    uprintf("  %-10s 无/上/下 = %d/%d/%d   %s\r\n", name, np, pu, pd, verdict);
}

/* ---------------- 5 根 IMU 线的通断体检 ---------------- */
static void imu_lines_probe(void)
{
    uprint("--- IMU 5 根线通断体检（三态：无上拉 / 内部上拉 / 内部下拉）---\r\n");
    line_probe(GPIOA, GPIO_PIN_5,     "SCK PA5");
    line_probe(GPIOA, GPIO_PIN_6,     "MISO PA6");
    line_probe(GPIOA, GPIO_PIN_7,     "MOSI PA7");
    line_probe(IMU_CS_GPIO_Port,   IMU_CS_Pin,   "CS PC4");
    line_probe(IMU_INT1_GPIO_Port, IMU_INT1_Pin, "INT1 PC5");
    spi_pins_af();
    uprint("  （判读重点：SCK/MOSI/CS 若报“悬空”，就是 MCU 到 IMU 之间断了；\r\n"
           "    这三根是 MCU 主动驱动的，正常应该看到“被驱动低/强下拉”或外部上拉）\r\n\r\n");
}

/* ---------------- SPI 速率扫描 ----------------
 * 15MHz 对 ICM-42670-P 是合法值，但焊点差、走线长、探头电容大时高速先崩。
 * 若低速能读通而高速不能，就是信号完整性/焊接问题，不是芯片坏。 */
static void spi_speed_sweep(void)
{
    static const struct { uint32_t pre; const char *name; } tbl[] = {
        { SPI_BAUDRATEPRESCALER_8,   "15.0MHz" },
        { SPI_BAUDRATEPRESCALER_16,  " 7.5MHz" },
        { SPI_BAUDRATEPRESCALER_32,  "3.75MHz" },
        { SPI_BAUDRATEPRESCALER_64,  "1.88MHz" },
        { SPI_BAUDRATEPRESCALER_128, " 937kHz" },
        { SPI_BAUDRATEPRESCALER_256, " 469kHz" },
    };

    uprint("--- SPI 速率扫描（模式0，读 WHO_AM_I 0x75）---\r\n");
    for (unsigned i = 0; i < sizeof(tbl) / sizeof(tbl[0]); ++i)
    {
        hspi1.Init.BaudRatePrescaler = tbl[i].pre;
        (void)HAL_SPI_Init(&hspi1);
        HAL_Delay(2);

        uint8_t v = 0;
        (void)spi_read8(ICM_WHO_AM_I, &v);
        uprintf("  %-8s -> 0x%02X  (HAL st=%d err=0x%08lX)%s\r\n",
                tbl[i].name, v, (int)g_last_st, (unsigned long)g_last_err,
                (v == ICM_DEVICE_ID) ? "   <== 命中!" : "");
    }

    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;   /* 复原 15MHz */
    (void)HAL_SPI_Init(&hspi1);
    uprint("  （若只有低速能读通：信号完整性/焊接问题，降速先用着；\r\n"
           "    若各速率全是同一个值：与速率无关，是线/芯片问题）\r\n\r\n");
}

/* ---------------- bit-bang 的模式 3 版本 ----------------
 * 已有 bb_read_reg() 走模式 0（SCK 空闲低、上升沿采样）。
 * 这里补模式 3（SCK 空闲高、下降沿采样）—— 模式错了会读出"稳定但错误"的值。 */
static uint8_t bb_xfer_m3(uint8_t b)
{
    uint8_t r = 0;
    for (int i = 7; i >= 0; --i)
    {
        HAL_GPIO_WritePin(GPIOA, g_bb_mosi,
                          (b & (1u << i)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        bb_delay();
        HAL_GPIO_WritePin(GPIOA, g_bb_sck, GPIO_PIN_RESET);   /* 下降沿采样 */
        bb_delay();
        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == GPIO_PIN_SET) r = (uint8_t)(r | (1u << i));
        HAL_GPIO_WritePin(GPIOA, g_bb_sck, GPIO_PIN_SET);
        bb_delay();
    }
    return r;
}

static uint8_t bb_read_reg_m3(uint8_t reg)
{
    uint8_t v;
    bb_pins_mode(1);
    HAL_GPIO_WritePin(GPIOA, g_bb_sck, GPIO_PIN_SET);          /* SCK 空闲高 */
    cs_low();
    bb_delay();
    (void)bb_xfer_m3((uint8_t)(reg | 0x80));
    v = bb_xfer_m3(0x00);
    cs_high();
    bb_pins_mode(0);
    return v;
}

/* ---------------- 写/回读 + 软复位 ----------------
 * 读不到 ID 时，"能不能写进去"是另一条独立证据：
 * 能写能读回说明 MOSI/MISO/SCK/CS 四根线都是通的、芯片也在跑。
 *
 * 【寄存器地址全部按官方手册 DS-000451 Rev 1.1 核对过，别再凭印象写】
 *   0x00 MCLK_RDY        R    上电 0x00，OTP 载入后变 0x01（位表却把它标在 bit3 —— 手册自相矛盾，
 *                             所以这里只报原始值，不拿它当通过/失败判据）
 *   0x01 DEVICE_CONFIG   R/W  复位值 0x04（bit2 SPI_AP_4WIRE=1 就是四线 SPI）
 *                             ★ bit0 = SPI_MODE：0=Mode0/3，1=Mode1/2 —— 往这里写 1 会把
 *                               SPI 通信切到 Mode1/2，是个能把好芯片"写坏"的坑，绝不能碰
 *   0x02 SIGNAL_PATH_RESET R/W 复位值 0x00，★ bit4 = SOFT_RESET_DEVICE_CONFIG（真正的软复位）
 *   0x03 DRIVE_CONFIG1   R/W  I3C 摆率，本板用 SPI，改它无害 —— 拿来做写/回读测试
 *   0x06 INT_CONFIG      R/W  INT1/INT2 驱动方式与极性（复位默认开漏，故 INT1 体检结论要存疑）
 *   0x75 WHO_AM_I        R    固定 0x67
 *   0x79..0x7E BLK_SEL_W/MADDR_W/M_W/BLK_SEL_R/MADDR_R/M_R = 分页访问 MREG 的机制 */
static void rw_and_reset_probe(void)
{
    uprint("--- 写/回读 + 软复位 ---\r\n");

    /* 1) 先报几个只读/状态寄存器，用来给"读回来的东西可不可信"打底 */
    uint8_t mclk = 0xFF, devcfg = 0xFF, intcfg = 0xFF;
    (void)spi_read8(0x00, &mclk);
    (void)spi_read8(0x01, &devcfg);
    (void)spi_read8(0x06, &intcfg);
    uprintf("  0x00 MCLK_RDY      = 0x%02X  （上电 0x00，OTP 载入后应为 0x01）\r\n", mclk);
    uprintf("  0x01 DEVICE_CONFIG = 0x%02X  （复位值 0x04；bit2=1 表示四线 SPI）\r\n", devcfg);
    uprintf("  0x06 INT_CONFIG    = 0x%02X  （复位默认开漏：INT1 体检读到“悬空”不能作为芯片坏的证据）\r\n", intcfg);

    /* 2) 写/回读测试：用 I3C 摆率寄存器（SPI 模式下无用），翻低 3 位再复原。
     *    只读寄存器不会跟着变，所以这一条能真正证明"写路径通"。 */
    uint8_t orig = 0xFF;
    (void)spi_read8(0x03, &orig);
    uint8_t want = (uint8_t)(orig ^ 0x07);
    (void)spi_write(0x03, want);
    HAL_Delay(2);
    uint8_t rb = 0xFF;
    (void)spi_read8(0x03, &rb);
    (void)spi_write(0x03, orig);                 /* 复原 */
    uprintf("  0x03 写 0x%02X -> 回读 0x%02X （原值 0x%02X）  %s\r\n",
            want, rb, orig,
            (rb == want) ? "-> 写生效，四根线都通" : "-> 回读不符，写没进去");

    /* 3) 真正的软复位：SIGNAL_PATH_RESET(0x02) bit4 */
    uprintf("  软复位：写 0x02=0x10 后等 50ms 再读 ID\r\n");
    (void)spi_write(0x02, 0x10);
    HAL_Delay(50);

    uint8_t id = 0xFF;
    (void)spi_read8(ICM_WHO_AM_I, &id);
    uprintf("  软复位后再读 0x75 = 0x%02X  %s\r\n", id,
            (id == ICM_DEVICE_ID) ? "-> 复位后 ID 正确！" : "-> 仍不是 0x67");

    uint8_t mclk2 = 0xFF;
    (void)spi_read8(0x00, &mclk2);
    uprintf("  复位后 MCLK_RDY    = 0x%02X\r\n", mclk2);
    uprint("\r\n");
}

/* ================= 时钟体检 + PLL 重试 =================
 * 为什么必须查这个：SPI1 的内核时钟源是 PLL1Q（RCC->D2CCIP1R.SPI123SEL=0）。
 * 若 PLL1 没跑起来，SPI1 就没有时钟 —— 传输会一直等 EOT 直到超时，
 * 表现为"SPI 寄存器配置完全正确、但每次都 HAL_TIMEOUT、读回全 0"。
 * 而本工程 SystemClock_Config 失败时走 Error_Handler()，它【只打印不挂死】
 * （注释说明是为了 TF 卡缺失时还能有输出），于是固件会带着 HSI 继续跑，
 * 症状就变成"地磁能读、IMU 永远读不到"——因为 I2C 的内核时钟来自 D2PCLK1。
 */
static const char *sws_name(uint32_t sws)
{
    switch (sws) { case 0: return "HSI"; case 1: return "CSI"; case 2: return "HSE"; case 3: return "PLL1"; default: return "?"; }
}

static void clock_report(void)
{
    uint32_t cr = RCC->CR, cfgr = RCC->CFGR;
    uint32_t sws = (cfgr >> 3) & 7u;

    uprint("--- 时钟体检 ---\r\n");
    uprintf("  CR  = 0x%08X  HSEON=%d HSERDY=%d  PLL1ON=%d PLL1RDY=%d\r\n",
            cr, (int)((cr >> 16) & 1u), (int)((cr >> 17) & 1u),
            (int)((cr >> 24) & 1u), (int)((cr >> 25) & 1u));
    uprintf("  SWS = %d (%s)   SYSCLK=%lu Hz  HCLK=%lu Hz  PCLK1=%lu Hz  PCLK2=%lu Hz\r\n",
            (int)sws, sws_name(sws),
            (unsigned long)HAL_RCC_GetSysClockFreq(), (unsigned long)HAL_RCC_GetHCLKFreq(),
            (unsigned long)HAL_RCC_GetPCLK1Freq(),    (unsigned long)HAL_RCC_GetPCLK2Freq());
    uprintf("  D2CCIP1R = 0x%08X  SPI123SEL=%d (=0 表示 SPI1 内核取 PLL1Q)\r\n",
            RCC->D2CCIP1R, (int)((RCC->D2CCIP1R >> 12) & 7u));
    uprintf("  PLL1DIVR = 0x%08X  N=%d P=%d Q=%d\r\n",
            RCC->PLL1DIVR, (int)(RCC->PLL1DIVR & 0x1FFu),
            (int)((RCC->PLL1DIVR >> 9) & 0x7Fu), (int)((RCC->PLL1DIVR >> 16) & 0x7Fu));

    if (((cr >> 25) & 1u) == 0u)
    {
        uprint("  ★ PLL1 没在跑 —— SPI1 没有内核时钟，所有 SPI 传输必然超时。\r\n"
               "    这就是“读不到 IMU”的根因，跟 IMU 焊得好不好无关。\r\n"
               "    （I2C 内核来自 D2PCLK1，跟着 SYSCLK 走，所以地磁能读通。）\r\n");
    }
    else
    {
        uprint("  PLL1 在跑，SPI1 有内核时钟 —— SPI 侧时钟没问题。\r\n");
    }
    uprint("\r\n");
}

/* 依次试几种 PLL 参数，看哪种真的能锁定（PLL1RDY 置位）。
 * A = 工程原参数：VCO = 25/5*192 = 960MHz, SYSCLK 480MHz
 *     ★ H743 手册 Table 50（6.3.10 节，H742/743）给的宽范围 VCO 上限是 836MHz，
 *       960 超限。手册第 7 节那张表写 960MHz，那是 H750/753 那一段（别混用）。
 * B = 退一档：VCO = 800MHz, SYSCLK 400MHz（VCO 在范围内） */
static void pll_retry_report(void)
{
    static const struct { uint32_t m, n, p, q; const char *tag; } cfg[4] = {
        { 5u, 192u, 2u, 8u, "A 原参数 M=5 N=192 P=2 -> VCO 960MHz / SYSCLK 480MHz" },
        { 5u, 160u, 2u, 8u, "B 退一档 M=5 N=160 P=2 -> VCO 800MHz / SYSCLK 400MHz" },
        { 5u,  96u, 2u, 4u, "C 低 VCO  M=5 N=96  P=2 -> VCO 480MHz / SYSCLK 240MHz" },
        { 5u,  48u, 2u, 2u, "D 很低   M=5 N=48  P=2 -> VCO 240MHz / SYSCLK 120MHz" },
    };

    uprint("--- PLL 参数重试（看哪种能锁）---\r\n");

    for (unsigned i = 0; i < 4; ++i)
    {
        RCC_OscInitTypeDef osc = {0};
        osc.OscillatorType     = RCC_OSCILLATORTYPE_HSE;
        osc.HSEState           = RCC_HSE_ON;
        osc.PLL.PLLState       = RCC_PLL_ON;
        osc.PLL.PLLSource      = RCC_PLLSOURCE_HSE;
        osc.PLL.PLLM           = cfg[i].m;
        osc.PLL.PLLN           = cfg[i].n;
        osc.PLL.PLLP           = cfg[i].p;
        osc.PLL.PLLQ           = cfg[i].q;
        osc.PLL.PLLR           = 2u;
        osc.PLL.PLLRGE         = RCC_PLL1VCIRANGE_2;
        osc.PLL.PLLVCOSEL      = RCC_PLL1VCOWIDE;
        osc.PLL.PLLFRACN       = 0u;

        HAL_StatusTypeDef st = HAL_RCC_OscConfig(&osc);
        uint32_t rdy = (RCC->CR >> 25) & 1u;

        uprintf("  %s\r\n    -> HAL=%d (%s)  PLL1RDY=%d\r\n",
                cfg[i].tag, (int)st,
                (st == HAL_OK) ? "HAL_OK" : (st == HAL_TIMEOUT ? "HAL_TIMEOUT" : "其它"),
                (int)rdy);

        if (st == HAL_OK && rdy)
        {
            /* 锁上了：把 SYSCLK 切到 PLL，并重配 SPI1（它的内核时钟变了） */
            RCC_ClkInitTypeDef clk = {0};
            clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                 RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 |
                                 RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
            clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
            clk.SYSCLKDivider  = RCC_SYSCLK_DIV1;
            clk.AHBCLKDivider  = RCC_HCLK_DIV2;
            clk.APB3CLKDivider = RCC_APB3_DIV2;
            clk.APB1CLKDivider = RCC_APB1_DIV2;
            clk.APB2CLKDivider = RCC_APB2_DIV2;
            clk.APB4CLKDivider = RCC_APB4_DIV2;

            HAL_StatusTypeDef st2 = HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4);
            uprintf("    切 SYSCLK 到 PLL: HAL=%d  现在 SWS=%d (%s)  SYSCLK=%lu Hz\r\n",
                    (int)st2, (int)((RCC->CFGR >> 3) & 7u),
                    sws_name((RCC->CFGR >> 3) & 7u),
                    (unsigned long)HAL_RCC_GetSysClockFreq());
            uprint("    ★ 这种参数能用，工程应按它改 SystemClock_Config。\r\n");
            break;
        }
        uprint("    -> 这种锁不上，试下一种\r\n");
    }
    uprint("\r\n");
}

/* ================= 寄存器级 SPI（完全绕开 HAL） =================
 * 为什么需要它：HAL 的阻塞传输在 H7 上有一套很挑的状态机（等 TXP/EOT、还把
 * UDR 当错误），实测每次都是 HAL_TIMEOUT(3) 且读回全 0。到底是
 *   (a) HAL/超时逻辑的问题，还是 (b) SPI 外设/连线的问题，
 * 光看 HAL 的返回值分不清。这里只用 SR 的 TXP/RXP/EOT 三个标志轮询，
 * 每一步失败都报出 SR 原值，让结论落在硬件上。
 * 失败位置的含义：
 *   卡在 TXP  -> SPI 外设没在跑（使能/内核时钟/主模式问题）
 *   卡在 RXP  -> 时钟没出去或 MISO 没人驱动（线/从机问题）
 *   卡在 EOT  -> 数据搬完了但 EOT 不来（一般也是从机没回数据）
 */
static uint32_t g_ll_sr_fail;      /* 失败时的 SR，供打印 */
static int      g_ll_fail_stage;   /* 1=TXP 2=RXP */
static int      g_ll_eot;          /* 收完后 EOT 有没有来（只观察） */

static int spi_ll_xfer(const uint8_t *tx, uint8_t *rx, uint16_t n)
{
    SPI_TypeDef *S = SPI1;

    g_ll_sr_fail = 0; g_ll_fail_stage = 0; g_ll_eot = 0;

    S->CR1 |= SPI_CR1_SPE;                          /* 使能 */
    MODIFY_REG(S->CR2, SPI_CR2_TSIZE, n);           /* 本次传输的数据量 */
    SET_BIT(S->CR1, SPI_CR1_CSTART);                /* ★ H7 必需：主机传输启动位。
                                                     *   漏了它，TXDR 写进去也不会有任何时钟输出，
                                                     *   RXP 永远不置位 —— 表现为"卡在等接收"。
                                                     *   HAL 在 stm32h7xx_hal_spi.c:1418 设了这一位。 */

    for (uint16_t i = 0; i < n; ++i)
    {
        uint32_t guard = 400000u;
        while ((S->SR & SPI_SR_TXP) == 0u) { if (--guard == 0u) { g_ll_sr_fail = S->SR; g_ll_fail_stage = 1; return 0; } }
        *(__IO uint8_t *)&S->TXDR = tx[i];

        guard = 400000u;
        while ((S->SR & SPI_SR_RXP) == 0u) { if (--guard == 0u) { g_ll_sr_fail = S->SR; g_ll_fail_stage = 2; return 0; } }
        rx[i] = *(__IO uint8_t *)&S->RXDR;
    }

    /* EOT 只看不判死：H7 上 EOT 在某些 FIFO/TSIZE 组合下不会来，
     * 但数据其实已经全部收完（HAL 就是因为死等 EOT 才超时，
     * 然后 spi_read() 又把已经收到的数据 memset 掉了）。 */
    {
        uint32_t guard = 400000u;
        while (((S->SR & SPI_SR_EOT) == 0u) && (--guard != 0u)) { }
        g_ll_eot = ((S->SR & SPI_SR_EOT) != 0u) ? 1 : 0;
    }
    return 1;
}

static int spi_ll_read8(uint8_t reg, uint8_t *v)
{
    uint8_t tx[2], rx[2];
    int ok;

    tx[0] = (uint8_t)(reg | 0x80);
    tx[1] = 0x00;

    cs_low();
    ok = spi_ll_xfer(tx, rx, 2);
    cs_high();
    CLEAR_BIT(SPI1->CR1, SPI_CR1_CSTART);      /* 收尾：结束本次主机传输 */

    *v = rx[1];
    return ok;
}

/* 对照实验 4：寄存器级 SPI 读 WHO_AM_I，并把每一步的 SR 打出来 */
static void spi_ll_probe(void)
{
    uprint("--- 对照实验 4：寄存器级 SPI（绕开 HAL 状态机）---\r\n");
    uprintf("  开始前 SR = 0x%08X\n", (unsigned)SPI1->SR);

    for (int i = 0; i < 3; ++i)
    {
        uint8_t v = 0xEE;
        int ok = spi_ll_read8(ICM_WHO_AM_I, &v);
        if (ok)
            uprintf("  第 %d 次: OK  WHO_AM_I=0x%02X  EOT=%d SR=0x%08X%s\r\n",
                    i + 1, v, g_ll_eot, (unsigned)SPI1->SR,
                    (v == ICM_DEVICE_ID) ? "  <== 0x67 命中!" : "");
        else
            uprintf("  第 %d 次: FAIL 阶段=%d  卡住时 SR=0x%08X  (TXP=%d RXP=%d EOT=%d UDR=%d)\r\n",
                    i + 1, g_ll_fail_stage, (unsigned)g_ll_sr_fail,
                    (int)((g_ll_sr_fail >> 1) & 1u), (int)(g_ll_sr_fail & 1u),
                    (int)((g_ll_sr_fail >> 3) & 1u), (int)((g_ll_sr_fail >> 5) & 1u));
        HAL_Delay(5);
    }
    uprint("  判读: 读到 0x67 -> HAL 的问题（照抄这段寄存器级收发即可）；\r\n"
           "        卡 TXP     -> SPI 外设没跑；\r\n"
           "        卡 RXP/EOT -> 时钟没出去或 MISO 没人驱动（线/从机）。\r\n\r\n");
}

/* ================= 保留数据的 SPI 读（不因 HAL 报错而丢弃） =================
 * 动机（这是本次排查最关键的怀疑点）：
 * H7 的 HAL 在阻塞传输的最后会死等 EOT 标志，EOT 不来就返回 HAL_TIMEOUT；
 * 而 spi_read() 的逻辑是「非 HAL_OK 就把缓冲区 memset 清零」——于是
 * 【"恒读 0x00"完全可能只是这个 memset 造成的假象，而不是 IMU 真的没回数据】。
 * 这里改成：无论 HAL 返回什么，都把移位进来的字节留下来并报出状态，
 * 让"有没有真的收到数据"这件事由数据本身说话。
 */
static int spi_read_keep(uint8_t reg, uint8_t *buf, uint16_t len, HAL_StatusTypeDef *st_out)
{
    uint8_t tx[8];
    uint8_t rx[8];

    if ((uint16_t)(len + 1u) > sizeof(tx)) len = (uint16_t)(sizeof(tx) - 1u);

    tx[0] = (uint8_t)(reg | 0x80);
    memset(&tx[1], 0, len);
    memset(rx, 0, sizeof(rx));

    cs_low();
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&hspi1, tx, rx, (uint16_t)(len + 1u), ICM_SPI_TIMEOUT);
    cs_high();
    spi_note(st);

    memcpy(buf, &rx[1], len);          /* ★ 不看状态，照样取用 */
    if (st_out) *st_out = st;
    return 1;
}

static void spi_keep_probe(void)
{
    uprint("=== 关键测试：不丢弃数据的 SPI 读 ===\r\n");
    uprint("(原来的 spi_read 在 HAL 非 OK 时会 memset 清零，可能把好数据抹掉)\r\n");
    for (int i = 0; i < 5; ++i)
    {
        uint8_t v = 0xEE;
        HAL_StatusTypeDef st = HAL_OK;
        (void)spi_read_keep(ICM_WHO_AM_I, &v, 1, &st);
        uprintf("  第 %d 次: WHO_AM_I(0x75)=0x%02X  HAL st=%d  Err=0x%08lX%s\r\n",
                i + 1, v, (int)st, (unsigned long)g_last_err,
                (v == ICM_DEVICE_ID) ? "   <== 0x67 命中!!" : "");
        HAL_Delay(5);
    }

    /* 再读两个"复位值已知"的寄存器，交叉印证 */
    {
        static const struct { uint8_t reg; uint8_t exp; const char *name; } chk[] = {
            { 0x00, 0x01, "MCLK_RDY"      },
            { 0x01, 0x04, "DEVICE_CONFIG" },
        };
        for (unsigned i = 0; i < sizeof(chk)/sizeof(chk[0]); ++i)
        {
            uint8_t v = 0xEE; HAL_StatusTypeDef st = HAL_OK;
            (void)spi_read_keep(chk[i].reg, &v, 1, &st);
            uprintf("  0x%02X %-14s = 0x%02X (期望 0x%02X, st=%d)%s\r\n",
                    chk[i].reg, chk[i].name, v, chk[i].exp, (int)st,
                    (v == chk[i].exp) ? "  OK" : "");
        }
    }
    uprint("  判读: 出现 0x67/0x04/0x01 -> IMU 其实一直在应答，是 HAL+清零逻辑骗了我们；\r\n"
           "        仍是全 0x00/0xFF -> 确实没有数据回来。\r\n\r\n");
}

/* 重初始化 SPI，并把内核时钟源重新钉回 per_ck。
 * 必须这样成对做：MX_SPI1_Init -> MspInit 里会把 SPI123SEL 设回 PLL1Q，
 * 而本板 PLL1 因为 HSE 没起振而锁不上 —— 那是个"死源"。
 * 之前 sck/fifo 两个测试各自 DeInit+Init 却没重设源，等于又在没时钟的配置上测，
 * 结果必然还是"SCK 不跳"，把结论带偏。 */
static void spi_reinit_per_ck(void)
{
    (void)HAL_SPI_DeInit(&hspi1);
    MX_SPI1_Init();                                   /* 这里会把源设回 PLL1Q */
    __HAL_SPI_DISABLE(&hspi1);
    MODIFY_REG(RCC->D1CCIPR,  RCC_D1CCIPR_CKPERSEL, 0u);                       /* per_ck=HSI */
    MODIFY_REG(RCC->D2CCIP1R, RCC_D2CCIP1R_SPI123SEL,
               (4u << RCC_D2CCIP1R_SPI123SEL_Pos));                            /* =per_ck */
}

/* ================= SCK 到底有没有在跳 =================
 * 这是"纯软件也能判定主机有没有输出时钟"的办法：
 * 引脚处于 AF 复用时，GPIOx->IDR 依然反映引脚的真实电平。
 * 把 SPI 降到最慢档（内核 120MHz / 256 ≈ 469kHz，一位约 2µs），
 * 一边往 TXDR 塞字节一边高频读 PA5 的 IDR，数电平跳变次数。
 *   跳变很多 -> 主机在发时钟，问题在 MISO/从机侧
 *   几乎不跳 -> 主机根本没输出时钟（外设/内核时钟问题），
 *               那再怎么补焊、换 IMU 都不可能有数据
 */
static void sck_activity_probe(void)
{
    uprint("--- SCK 活动检测（采样 PA5 的 IDR 数跳变）---\r\n");

    /* 重来一遍，并把内核时钟钉在 per_ck（HSI），不依赖 PLL */
    spi_reinit_per_ck();
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    (void)HAL_SPI_Init(&hspi1);
    MODIFY_REG(RCC->D2CCIP1R, RCC_D2CCIP1R_SPI123SEL, (4u << RCC_D2CCIP1R_SPI123SEL_Pos));
    uprintf("  本次测试用的内核源: SPI123SEL=%d (4=per_ck)  CFG1=0x%08X CFG2=0x%08X\r\n",
            (int)((RCC->D2CCIP1R >> 12) & 7u), (unsigned)SPI1->CFG1, (unsigned)SPI1->CFG2);

    SPI1->CR1 |= SPI_CR1_SPE;
    MODIFY_REG(SPI1->CR2, SPI_CR2_TSIZE, 16u);
    SET_BIT(SPI1->CR1, SPI_CR1_CSTART);

    cs_low();

    int last = (int)((GPIOA->IDR >> 5) & 1u);
    int edges = 0;
    long samples = 0;

    for (uint16_t i = 0; i < 16u; ++i)
    {
        uint32_t guard = 400000u;
        while (((SPI1->SR & SPI_SR_TXP) == 0u) && (--guard != 0u)) { }
        *(__IO uint8_t *)&SPI1->TXDR = (uint8_t)(i * 17u + 1u);

        for (volatile int k = 0; k < 4000; ++k)
        {
            int now = (int)((GPIOA->IDR >> 5) & 1u);
            ++samples;
            if (now != last) { ++edges; last = now; }
        }
    }

    cs_high();
    CLEAR_BIT(SPI1->CR1, SPI_CR1_CSTART);

    uprintf("  采样 %ld 次，PA5 跳变 %d 次   SR=0x%08X\r\n",
            samples, edges, (unsigned)SPI1->SR);

    if (edges > 4)
        uprint("  -> SCK 在跳：主机确实在输出时钟，问题在 MISO/从机侧。\r\n");
    else
        uprint("  -> SCK 没跳：主机根本没输出时钟 —— 这是外设/时钟层面的问题，\r\n"
               "     跟 IMU 焊得怎么样、换不换芯片都无关。\r\n");

    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    (void)HAL_SPI_Init(&hspi1);
    uprint("\r\n");
}

/* ================= 终极测试：把 RX FIFO 里的字节读出来 =================
 * 前面出现矛盾：SPI 报告"传完了 16 字节"(TXC/CTSIZE)，但 PA5 的 IDR 一次都没跳。
 * 到底外设有没有在移位？直接看它收到了什么：
 *   收到的是 0x00        -> 移位在跑，但 MISO 恒低（没时钟出去或从机没驱动）
 *   收到的 = 发出去的     -> 存在内部回环
 *   CTSIZE 始终 0        -> 移位根本没跑
 * 同时把 EOT/RXP/TXC/CTSIZE 的过程打出来，HAL 等的就是 EOT。
 */
static void fifo_probe(void)
{
    uprint("--- 终极测试：RX FIFO 内容 + 标志过程 ---\r\n");

    spi_reinit_per_ck();
    uprintf("  本次测试用的内核源: SPI123SEL=%d (4=per_ck)\r\n",
            (int)((RCC->D2CCIP1R >> 12) & 7u));

    SPI_TypeDef *S = SPI1;
    uint8_t txb[2] = { (uint8_t)(ICM_WHO_AM_I | 0x80), 0x00 };
    uint8_t rxb[2] = { 0xEE, 0xEE };
    int got = 0;

    cs_low();
    S->CR1 |= SPI_CR1_SPE;
    MODIFY_REG(S->CR2, SPI_CR2_TSIZE, 2u);
    SET_BIT(S->CR1, SPI_CR1_CSTART);

    for (int i = 0; i < 2; ++i)
    {
        uint32_t guard = 400000u;
        while (((S->SR & SPI_SR_TXP) == 0u) && (--guard != 0u)) { }
        *(__IO uint8_t *)&S->TXDR = txb[i];
    }

    /* 轮询最多 40 万次，记录 CTSIZE 与 EOT 的变化 */
    uint32_t s_first = S->SR, s_last = s_first;
    int eot_seen = 0, rxp_seen = 0, txc_seen = 0;
    for (uint32_t k = 0; k < 400000u; ++k)
    {
        uint32_t sr = S->SR;
        s_last = sr;
        if (sr & SPI_SR_EOT) eot_seen = 1;
        if (sr & SPI_SR_RXP) rxp_seen = 1;
        if (sr & SPI_SR_TXC) txc_seen = 1;
        if ((sr & SPI_SR_RXP) && got < 2) rxb[got++] = *(__IO uint8_t *)&S->RXDR;
    }

    cs_high();
    CLEAR_BIT(S->CR1, SPI_CR1_CSTART);

    uprintf("  TX   = %02X %02X\r\n", txb[0], txb[1]);
    uprintf("  RX   = %02X %02X   (读到 %d 字节)\r\n", rxb[0], rxb[1], got);
    uprintf("  标志: EOT=%d RXP=%d TXC=%d   SR首=0x%08X SR末=0x%08X\r\n",
            eot_seen, rxp_seen, txc_seen, (unsigned)s_first, (unsigned)s_last);
    uprintf("  CTSIZE末=%d  CR1=0x%08X\r\n",
            (int)((s_last >> 16) & 0x1Fu), (unsigned)S->CR1);

    /* 【原来的判读有假匹配】旧代码是 rxb[0]==txb[0] || rxb[1]==txb[1]，
     * 而第 2 个发送字节恒为 0x00，于是 0x00==0x00 必然成立，
     * 把"MISO 恒 0"误报成"内部回环"，白怀疑了一轮 SPI 外设配置。
     * 现在要求两个非零字节都原样回来才算回环，并把恒 0 单独判出来。 */
    if (got >= 2 && rxb[1] == ICM_DEVICE_ID)
        uprint("  ★★★ 读到 0x67！IMU 是活的！\r\n");
    else if (got >= 2 && txb[0] != 0x00 && rxb[0] == txb[0] && rxb[1] == txb[1])
        uprint("  -> 收到的是自己发的字节：内部回环，MOSI/MISO 或外设配置有问题。\r\n");
    else if (got >= 1 && rxb[0] == 0x00 && rxb[1] == 0x00)
        uprint("  -> MISO 恒 0x00：从机根本没驱动数据线（未焊/未供电/贴反/损坏）。\r\n");
    else if (got >= 1)
        uprint("  -> 收到了数据但不是 0x67（看上面数值判断）。\r\n");
    else
        uprint("  -> 一个字节都没收到：移位没跑或时钟没出去。\r\n");
    uprint("\r\n");
}

/* ================= 绕开 PLL：把 SPI1 内核时钟切到 per_ck =================
 * 实测发现本板 PLL1 是【时好时坏】的：同一份固件，有的上电锁得上（480MHz），
 * 有的锁不上（PLL1RDY=0，SYSCLK 退回 HSI 64MHz）。而 SPI1 的内核时钟源默认是
 * PLL1Q —— PLL1 一挂，SPI 就完全没有时钟，SCK 不跳、收发全废，
 * 这跟 IMU 焊没焊、换没换芯片毫无关系（I2C 内核跟着 SYSCLK 走，所以地磁照读）。
 *
 * 先把 SPI 从 PLL 上解耦：SPI123SEL 改成 per_ck，per_ck 源选 HSI(64MHz)。
 * SPI 是同步总线，时钟绝对精度无所谓，只要"有线时钟"就能通信。
 *   120MHz/8 = 15MHz  ->  64MHz/8 = 8MHz（仍远低于 ICM 的 24MHz 上限）
 * 这样即使 PLL 没锁上，IMU 也能读。PLL 本身为什么锁不上是另一个要查的问题
 * （VCAP 1.2V 去耦电容、HSE 晶振负载电容、供电质量都要看）。
 */
static void spi_kernel_clock_fix(void)
{
    uprint("--- 把 SPI1 内核时钟从 PLL1Q 切到 per_ck(HSI) ---\r\n");
    uprintf("  切换前 D2CCIP1R=0x%08X SPI123SEL=%d\r\n",
            RCC->D2CCIP1R, (int)((RCC->D2CCIP1R >> 12) & 7u));

    /* 【顺序很关键】必须先 DeInit/Init —— spi.c 的 MspInit 里会调
     * HAL_RCCEx_PeriphCLKConfig 把 SPI123SEL 写回 PLL1Q。
     * 先改源后 init，就会被 init 覆盖掉（我第一次就踩了这个，日志里
     * "切换后=4" 却在下一次体检又变回 0）。 */
    (void)HAL_SPI_DeInit(&hspi1);
    MX_SPI1_Init();

    /* 现在再改内核时钟源，并且不再调用 HAL_SPI_Init（CFG1/CFG2 已经是对的，
     * 换源只是换了分频器的输入，不需要重配外设） */
    __HAL_SPI_DISABLE(&hspi1);
    MODIFY_REG(RCC->D1CCIPR, RCC_D1CCIPR_CKPERSEL, 0u);              /* per_ck = HSI */
    MODIFY_REG(RCC->D2CCIP1R, RCC_D2CCIP1R_SPI123SEL,
               (4u << RCC_D2CCIP1R_SPI123SEL_Pos));                  /* 4 = per_ck */

    uprintf("  切换后 D2CCIP1R=0x%08X SPI123SEL=%d  D1CCIPR=0x%08X\r\n",
            RCC->D2CCIP1R, (int)((RCC->D2CCIP1R >> 12) & 7u), RCC->D1CCIPR);
    uprint("  预分频 8 -> 约 8MHz（per_ck=HSI 64MHz）\r\n\r\n");
}

/* ================= HSE 体检 =================
 * 本板 HSE 的表现不稳定：有的上电 HSERDY=1、PLL 能锁；有的 HSERDY=0（HSE 根本没起振）。
 * HSE 起不来 -> PLL 锁不上 -> SYSCLK 退回 HSI -> SPI1 内核时钟(PLL1Q)没了 -> 永远读不到 IMU。
 * 这里把 HSE 的状态和"它到底会不会就绪"直接量出来。 */
static void hse_report(void)
{
    uint32_t cr = RCC->CR;

    uprint("--- HSE 体检 ---\r\n");
    uprintf("  CR=0x%08X  HSEON=%d HSERDY=%d  HSEBYP=%d  PLL1ON=%d PLL1RDY=%d\r\n",
            cr, (int)((cr >> 16) & 1u), (int)((cr >> 17) & 1u), (int)((cr >> 18) & 1u),
            (int)((cr >> 24) & 1u), (int)((cr >> 25) & 1u));

    if (((cr >> 17) & 1u) == 0u)
    {
        uprint("  ★ HSERDY=0：25MHz 晶振没起振（或被 HAL 判超时后放弃）。\r\n"
               "    查：晶振 X1 焊接、C1/C2 负载电容值是否与晶振规格匹配、R20(0Ω)、\r\n"
               "    以及 HSE 增益配置（HSEGMC）。这会连带把 PLL/SPI/SDMMC 全部带崩。\r\n");
    }
    else
    {
        uprint("  HSE 已就绪。\r\n");
    }
    uprint("\r\n");
}

/* ================= 【干净窗口】真正的初始化 + 采样 =================
 * 为什么要单独做这一步：本文件下面那些探针（imu_lines_probe / miso_line_probe /
 * pll_retry_report）会把 PA5/6/7 改成 GPIO、把 SPI 内核源改回 PLL1Q，之后所有
 * SPI 读写必然失败 —— 那是【诊断自身的副作用】，不是硬件问题。
 * 实测（IMU 焊好后的那一轮）：探针之前 spi_keep_probe 读到 0x67，探针之后
 * WHO_AM_I 连读 5 次全 0x00，但同一时刻 GPIO bit-bang 仍能读到 0x67 ——
 * 芯片、供电、SCK/MOSI/MISO 一直是好的，是诊断把自己搞坏了。
 * 所以把【能不能真读到数据】放到最前面，在引脚和时钟都还没被动过时做。
 * 顺带把每次写的 HAL 返回值打出来：原来的 spi_write 无条件 return 1，
 * 写到底成没成功根本看不出来。 */
static void imu_clean_init_and_sample(void)
{
    uprint("--- 【干净窗口】IMU 初始化 + 采样（在所有探针之前）---\r\n");

    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    spi_reinit_per_ck();        /* DeInit + MX_SPI1_Init（引脚回 AF5），再把内核源钉到 per_ck */
    __HAL_SPI_ENABLE(&hspi1);   /* 上面那个函数结尾把 SPI 关掉了，这里开回来 */

    uint8_t v = 0;
    (void)spi_read8(ICM_WHO_AM_I, &v);
    uprintf("  WHO_AM_I(0x%02X) = 0x%02X  期望 0x67   HAL st=%d\r\n",
            ICM_WHO_AM_I, v, (int)g_last_st);

    /* (a) 先用一个【无状态机限制】的寄存器单独验证写通路：DRIVE_CONFIG1(0x03)。
     *     它只是 I3C 摆率配置，芯片不会因为上电时序/工作模式拒绝写入，
     *     所以能把 写通路坏 和 PWR_MGMT0 被芯片拒绝 这两种原因分开。
     *     （手册给的复位值是 0x2B，正好也能顺手核对读通路。） */
    {
        uint8_t b = 0, a = 0, want = 0;
        (void)spi_read8(0x03, &b);
        want = (uint8_t)(b ^ 0x07);
        (void)spi_write(0x03, want);
        HAL_Delay(2);
        (void)spi_read8(0x03, &a);
        uprintf("  写通路自检 0x03: 原=0x%02X 写=0x%02X 回读=0x%02X  %s\r\n",
                b, want, a, ((a & 0x07) == (want & 0x07)) ? "写通路 OK" : "写通路不通");
        (void)spi_write(0x03, b);                       /* 还原，别影响后面 */
    }

    /* (b) 真正的目标：使能陀螺+加速度。PWR_MGMT0: IDLE=0, GYRO_MODE=11, ACCEL_MODE=11 */
    {
        uint8_t tx[2] = { (uint8_t)(ICM_PWR_MGMT0 & 0x7F), ICM_PWR_LN };
        cs_low();
        HAL_StatusTypeDef st = HAL_SPI_Transmit(&hspi1, tx, 2, ICM_SPI_TIMEOUT);
        cs_high();
        spi_note(st);
        uprintf("  写 PWR_MGMT0(0x%02X) <- 0x%02X   HAL st=%d (0=OK 1=ERR 2=BUSY 3=TIMEOUT)\r\n",
                ICM_PWR_MGMT0, ICM_PWR_LN, (int)st);
    }
    HAL_Delay(60);

    uint8_t p = 0;
    (void)spi_read8(ICM_PWR_MGMT0, &p);
    uprintf("  回读 PWR_MGMT0 = 0x%02X  %s\r\n", p,
            (p == ICM_PWR_LN) ? "OK，写进去了" : "仍不一致");

    (void)spi_write(ICM_GYRO_CONFIG0,  ICM_GYRO_CFG);
    (void)spi_write(ICM_ACCEL_CONFIG0, ICM_ACCEL_CFG);
    HAL_Delay(60);

    /* (c) 连采 5 帧。静止时 az 应约 +9.8 或 -9.8 m/s^2；
     *     若三个轴恒为 0x8000(-32768)，说明传感器没被使能，数据寄存器还是无效值。 */
    for (int i = 0; i < 5; ++i)
    {
        uint8_t acc[6] = {0}, gyr[6] = {0}, tp[2] = {0};
        (void)spi_read(ICM_ACCEL_DATA,  acc, 6);
        (void)spi_read(ICM_GYRO_DATA,   gyr, 6);
        (void)spi_read(ICM_TEMP_CAND_A, tp,  2);

        int16_t rax = be16(&acc[0]), ray = be16(&acc[2]), raz = be16(&acc[4]);
        int16_t rgx = be16(&gyr[0]), rgy = be16(&gyr[2]), rgz = be16(&gyr[4]);
        uprintf("  #%d RAW A(%6d,%6d,%6d) G(%6d,%6d,%6d) T=%+6.2fC  az=%+6.2f m/s2\r\n",
                i + 1, rax, ray, raz, rgx, rgy, rgz,
                (float)be16(tp) / 128.0f + 25.0f,
                (float)raz / ICM_ACCEL_LSB_G * 9.80665f);
        HAL_Delay(100);
    }
    uprint("\r\n");
}

/* ---------------- 主入口 ---------------- */
void Imu_Stream_Test(void)
{
    diag_reset();                       /* 先立 magic，后面所有输出都会同时进 RAM */

    uprint("\r\n\r\n=== ICM-42670-P IMU STREAM TEST ===\r\n");
    uprintf("SPI1  SCK=PA5 MISO=PA6 MOSI=PA7 CS=PC4   115200 8N1\r\n");
    uprint("RAM 镜像: 符号 g_diag (DTCM, 不经 D-Cache) -> 可用 SWD 直接读\r\n\r\n");

    /* 0) 先查时钟：SPI1 的内核时钟是 PLL1Q，PLL1 没跑就永远读不到 IMU。
     *    这一步必须排在所有 SPI 测试之前，否则后面的结论全是假的。 */
    hse_report();
    spi_kernel_clock_fix();

    /* 0) 【最重要的一步放在最前面】此时引脚和时钟都还没被任何探针动过，
     *    这里能读到什么才是硬件的真实状态。下面的探针会把 PA5/6/7 改成 GPIO，
     *    之后再读 SPI 一定失败，那些 0x00 不算数。 */
    imu_clean_init_and_sample();

    sck_activity_probe();
    fifo_probe();
    spi_keep_probe();

    clock_report();
    pll_retry_report();
    clock_report();                     /* 重试之后再报一次，看有没有变化 */
    spi_pins_af();                      /* 重试若切换了时钟，把引脚状态理一遍 */
    (void)HAL_SPI_Init(&hspi1);

    /* 0') 五根线的通断体检：这一步不需要芯片配合，纯电气，
     *     能先把"虚焊/开路"和"芯片不应答"分开。 */
    imu_lines_probe();

    /* 1) 先看这条 SPI 链路本身：连读 5 次 WHO_AM_I，把每次原始值都打出来。
     *    全 0x00 / 全 0xFF / 每次都不一样，分别指向不同的硬件问题，
     *    比只打一个结果有用得多。 */
    /* 0) 先给 MISO 这根线做体检（结论由函数内部打印） */
    miso_line_probe();

    /* 0b) 再看 INT1 有没有被主动驱动 —— 判断芯片活没活的旁证 */
    imu_int_probe();

    uprint("--- WHO_AM_I(0x75) 连读 5 次（期望 0x67）---\r\n");
    uint8_t id = 0;
    int     id_hits = 0;
    for (int i = 0; i < 5; ++i)
    {
        uint8_t v = 0;
        (void)spi_read8(ICM_WHO_AM_I, &v);
        uprintf("  第 %d 次: 0x%02X\r\n", i + 1, v);
        if (v == ICM_DEVICE_ID) id_hits++;
        id = v;
        HAL_Delay(5);
    }

    /* 报出最后一次传输的 HAL 状态与 ErrorCode，用于判断 HAL_ERROR 是不是那个假 UDR */
    uprintf("  末次传输: HAL st=%d  ErrorCode=0x%08lX  (0x100=UDR)\r\n",
            (int)g_last_st, (unsigned long)g_last_err);

    if (id_hits)
    {
        uprint("  -> ID 正确，SPI 通路 OK\r\n");
    }
    else if (id == 0x00)
    {
        uprint("  -> 恒读 0x00：MISO 一直被拉低。IMU 未供电/未焊好/贴反（VDD-GND 接反）时典型表现。\r\n");
    }
    else if (id == 0xFF)
    {
        uprint("  -> 恒读 0xFF：MISO 浮空被上拉，从机根本没驱动这根线。查 IMU 供电与 MISO 焊点。\r\n");
    }
    else
    {
        uprint("  -> 能通信但 ID 不符（值不稳定）：查 SPI 时序/焊点，并确认型号确为 ICM-42670-P。\r\n");
    }

    /* 1a) 逐字节读几个【手册给出复位值】的寄存器，作为"SPI 读是否可信"的判据。
     *     三者全部对得上，才说明读回来的东西有意义；有一个不符，就说明通信不可信，
     *     后面所有读数（包括那些 0x00）都不能当证据。 */
    uprint("\r\n--- 单字节读『复位值已知』的寄存器（对照 DS-000451）---\r\n");
    {
        static const struct { uint8_t reg; uint8_t exp; const char *name; } chk[] = {
            { 0x00, 0x01, "MCLK_RDY"      },   /* 上电后应为 1：内部时钟运行中 */
            { 0x01, 0x04, "DEVICE_CONFIG" },   /* 复位值 0x04 */
            { 0x75, 0x67, "WHO_AM_I"      },   /* 固定 0x67 */
        };
        int bad = 0;
        for (unsigned i = 0; i < sizeof(chk) / sizeof(chk[0]); ++i)
        {
            uint8_t v = 0;
            (void)spi_read8(chk[i].reg, &v);
            if (v != chk[i].exp) ++bad;
            uprintf("  0x%02X %-14s 读=0x%02X  期望=0x%02X  %s\r\n",
                    chk[i].reg, chk[i].name, v, chk[i].exp,
                    (v == chk[i].exp) ? "OK" : "不符");
        }
        if (bad == 3)
        {
            uprint("  -> 三个全不符：SPI 读回来的值不可信，通信本身有问题，\r\n"
                   "     此时任何「读不到」的结论都不能算数。\r\n");
        }
        else if (bad > 0)
        {
            uprint("  -> 部分不符：通信部分可用，按对上/对不上的规律继续缩小范围。\r\n");
        }
        else
        {
            uprint("  -> 三个全对：SPI 通信可信，芯片状态正常。\r\n");
        }
    }
    uprint("\r\n");

    /* 1a-2) 写/回读 + 软复位（地址全部按官方手册 DS-000451 Rev 1.1 核对） */
    rw_and_reset_probe();

    /* 1a-3) 速率扫描：15MHz 合法但焊接差时高速先崩，低速能通则问题在信号完整性 */
    spi_speed_sweep();

    /* 1b) 模式 0 与模式 3 对比：模式错位会读出一个"稳定但错误"的 ID */
    uprint("\r\n=== SPI 模式对比 ===\r\n");
    dump_wide("模式0(CPOL=0,CPHA=1EDGE)");

    spi_set_mode(SPI_POLARITY_HIGH, SPI_PHASE_2EDGE);      /* 切到模式 3 */
    HAL_Delay(5);
    {
        uint8_t id3 = 0;
        (void)spi_read8(ICM_WHO_AM_I, &id3);
        uprintf("\r\n模式3(CPOL=1,CPHA=2EDGE) 下 WHO_AM_I(0x75) = 0x%02X\r\n", id3);
        if (id3 == ICM_DEVICE_ID)
        {
            uprint("  -> 模式 3 读到 0x67！芯片确是 ICM-42670-P，只是必须用 SPI 模式 3。\r\n");
        }
        else if (id3 == 0x00 || id3 == 0xFF)
        {
            uprint("  -> 模式 3 完全无响应，说明本芯片用的是模式 0。\r\n");
        }
        else
        {
            uprintf("  -> 模式 3 读到 0x%02X，也不是 0x67。两种模式都读不出正确 ID，\r\n", id3);
            uprint("     那就要认真考虑『板上这颗不是 ICM-42670-P』这个可能了。\r\n");
        }
    }
    dump_wide("模式3");
    spi_set_mode(SPI_POLARITY_LOW, SPI_PHASE_1EDGE);       /* 恢复模式 0 */
    HAL_Delay(5);
    uprint("\r\n");

    /* 2) 对照实验：用 GPIO bit-bang 再读 WHO_AM_I，完全绕开 SPI 外设。
     *    这一条能把"SPI 配置不对"和"引脚/焊接/芯片不对"彻底分开。 */
    spi_ll_probe();

    uprint("\r\n--- 对照实验：GPIO bit-bang 读 WHO_AM_I（绕开 SPI 外设）---\r\n");
    for (int i = 0; i < 3; ++i)
    {
        uprintf("  第 %d 次: 0x%02X\r\n", i + 1, bb_read_reg(ICM_WHO_AM_I));
    }
    uprint("  判读：bit-bang 读到 0x67 -> SPI 外设配置/HAL 的问题（改软件即可）；\r\n"
           "        仍是 0x00       -> GPIO 直驱也推不动，是引脚/焊接/芯片的问题。\r\n");

    /* 3) 再把 SCK 与 MOSI 对调读一次，排除"这两根线在 PCB 上接反"。
     *    本板从未实机验证过，PCB 走线本身也在嫌疑范围内。 */
    uprint("\r\n--- 对照实验 2：bit-bang 且 SCK/MOSI 对调 ---\r\n");
    for (int i = 0; i < 3; ++i)
    {
        uprintf("  第 %d 次: 0x%02X\r\n", i + 1, bb_read_reg_swapped(ICM_WHO_AM_I));
    }
    uprint("  判读：这里读到 0x67 -> PCB 上 SCK 与 MOSI 接反了（改板或飞线）。\r\n"
           "        和上一组一样是 0x00 -> 与接反无关。\r\n\r\n");

    /* 3b) bit-bang 模式 3（SCK 空闲高、下降沿采样）：SPI 外设两种模式都试过了，
     *     bit-bang 也把两种模式补齐，避免"外设配置"和"模式假设"两种原因混在一起。 */
    uprint("--- 对照实验 3：bit-bang 模式3（SCK 空闲高 / 下降沿采样）---\r\n");
    for (int i = 0; i < 3; ++i)
    {
        uprintf("  第 %d 次: 0x%02X\r\n", i + 1, bb_read_reg_m3(ICM_WHO_AM_I));
    }
    uprint("  判读：模式3 的 bit-bang 读到 0x67 -> 芯片只认 SPI 模式 3；\r\n"
           "        两种模式都 0x00 -> 与模式无关，是线/供电/芯片问题。\r\n\r\n");

    /* 2) 先上电使能，否则数据寄存器不会更新 */
    uprint("\r\n--- 使能 accel+gyro（PWR_MGMT0 <- 0x0F）---\r\n");
    (void)spi_write(ICM_PWR_MGMT0, ICM_PWR_LN);
    HAL_Delay(50);

    dump_regs();

    /* 3) 配置量程与 ODR */
    uprint("\r\n--- 配置量程/ODR ---\r\n");
    (void)spi_write(ICM_GYRO_CONFIG0,  ICM_GYRO_CFG);
    (void)spi_write(ICM_ACCEL_CONFIG0, ICM_ACCEL_CFG);
    HAL_Delay(50);

    uint8_t chk = 0;
    (void)spi_read8(ICM_PWR_MGMT0, &chk);
    uprintf("回读 PWR_MGMT0 = 0x%02X （写入 0x%02X）%s\r\n",
            chk, ICM_PWR_LN, (chk == ICM_PWR_LN) ? "" : "  <- 不一致，地址可能不对");

    /* 【流开始前重建 SPI 并重新使能传感器】
     * 上面的探针把 PA5/6/7 改成过 GPIO、把内核源改回过 PLL1Q，所以 1269 行那次
     * PWR_MGMT0 写入其实是被诊断自己的副作用搞失败的。不在这里重建一遍，
     * 下面 10Hz 流读到的只会是全 0x8000（传感器未使能的无效值）。
     * 同样的初始化在探针之前的 imu_clean_init_and_sample() 里已经成功过一次。 */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    spi_reinit_per_ck();
    __HAL_SPI_ENABLE(&hspi1);
    (void)spi_write(ICM_PWR_MGMT0, ICM_PWR_LN);
    (void)spi_write(ICM_GYRO_CONFIG0,  ICM_GYRO_CFG);
    (void)spi_write(ICM_ACCEL_CONFIG0, ICM_ACCEL_CFG);
    HAL_Delay(60);
    {
        uint8_t p2 = 0;
        (void)spi_read8(ICM_PWR_MGMT0, &p2);
        uprintf("  [流前重建] PWR_MGMT0 回读 = 0x%02X （写入 0x%02X）%s\r\n",
                p2, ICM_PWR_LN, (p2 == ICM_PWR_LN) ? " OK" : " 仍不一致");
    }

    uprint("\r\n--- 开始输出数据（约 10 Hz）---\r\n");
    uprint("列含义: raw=原始 int16; a=m/s^2; w=deg/s; T=degC\r\n");
    uprint("若 az 静止时约为 +9.8（或 -9.8），说明加速度计量程与地址都正确。\r\n");
    uprint("\r\n=== BOOT DIAG END（以下为数据流，SWD 读 RAM 时看这一段之前即可）===\r\n\r\n");

    /* 4) 持续读取并打印 */
    uint32_t n = 0;
    while (1)
    {
        /* spi_read 现在恒返回可用数据（HAL 的 UDR 是假错），有没有真的读到东西
         * 看数值本身：恒 0x00 / 恒 0xFF 就是没通信。 */
        uint8_t acc[6] = {0}, gyr[6] = {0}, ta[2] = {0}, tb[2] = {0};
        (void)spi_read(ICM_ACCEL_DATA,  acc, 6);
        (void)spi_read(ICM_GYRO_DATA,   gyr, 6);
        (void)spi_read(ICM_TEMP_CAND_A, ta,  2);
        (void)spi_read(ICM_TEMP_CAND_B, tb,  2);

        int16_t rax = be16(&acc[0]), ray = be16(&acc[2]), raz = be16(&acc[4]);
        int16_t rgx = be16(&gyr[0]), rgy = be16(&gyr[2]), rgz = be16(&gyr[4]);

        const float g0 = 9.80665f;
        float ax = (float)rax / ICM_ACCEL_LSB_G * g0;
        float ay = (float)ray / ICM_ACCEL_LSB_G * g0;
        float az = (float)raz / ICM_ACCEL_LSB_G * g0;
        float wx = (float)rgx / ICM_GYRO_LSB_DPS;
        float wy = (float)rgy / ICM_GYRO_LSB_DPS;
        float wz = (float)rgz / ICM_GYRO_LSB_DPS;
        float tA = (float)be16(ta) / 128.0f + 25.0f;
        float tB = (float)be16(tb) / 128.0f + 25.0f;

        uprintf("[%lu] A(%6d,%6d,%6d) a(%+6.2f,%+6.2f,%+6.2f) | "
                "G(%6d,%6d,%6d) w(%+7.2f,%+7.2f,%+7.2f) | "
                "T@09=%+6.2f T@1D=%+6.2f C\r\n",
                (unsigned long)n++, rax, ray, raz, ax, ay, az,
                rgx, rgy, rgz, wx, wy, wz, tA, tB);

        HAL_Delay(100);
    }
}

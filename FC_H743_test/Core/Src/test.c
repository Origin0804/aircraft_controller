/**
  ******************************************************************************
  * @file    test.c
  * @brief   FC_H743 批量外设自检
  * @note    焊完板后跑一遍，结果打印到 USART1 (115200) 控制台。
  *          每项格式：[id] name ... OK/FAIL (detail)
  ******************************************************************************
  */
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "sdmmc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "test.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ---------------- 控制台输出（USART1） ---------------- */
static int g_test_idx = 1;
static int g_test_pass = 0;
static int g_test_fail = 0;

static void t_print(const char *s)
{
    if (s) HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 200);
}

static void t_printf(const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)n, 200);
}

static void t_result(const char *name, int ok, const char *detail)
{
    t_printf("[%02d] %-22s ... %s  %s\r\n", g_test_idx++, name, ok ? "OK" : "FAIL", detail);
    if (ok) g_test_pass++; else g_test_fail++;
}

/* ---------------- SPI 读（IMU, SPI1, CS=PC4） ---------------- */
static uint8_t spi_read_reg(uint8_t reg)
{
    uint8_t tx[2] = { reg | 0x80, 0x00 };
    uint8_t rx[2] = { 0, 0 };
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, 20);
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);
    return rx[1];
}

/* ---------------- I2C 读 ---------------- */
static uint8_t i2c_read8(I2C_HandleTypeDef *h, uint16_t addr, uint8_t reg)
{
    uint8_t v = 0xFF;
    if (HAL_I2C_Mem_Read(h, addr, reg, I2C_MEMADD_SIZE_8BIT, &v, 1, 20) == HAL_OK) return v;
    return 0xFF;
}

/* ---------------- ADC 电压/电流 ---------------- */
static void adc_read(float *volt, float *cur)
{
    HAL_ADC_Start(&hadc1);
    for (int i = 0; i < 2; ++i)
    {
        if (HAL_ADC_PollForConversion(&hadc1, 20) != HAL_OK) break;
        uint32_t r = HAL_ADC_GetValue(&hadc1) & 0xFFFF;
        float v = (float)r / 65535.0f * 3.3f;
        if (i == 0) *volt = v * 4.0f;          /* 分压 1:4 */
        else        *cur  = v * 10.0f;          /* 电流 0.1V/A */
    }
    HAL_ADC_Stop(&hadc1);
}

/* ---------------- 单项测试 ---------------- */
static void test_console(void)
{
    t_result("Console USART1", 1, "->115200");
}

static void test_imu(void)
{
    uint8_t id = spi_read_reg(0x75);          /* WHO_AM_I */
    t_result("IMU ICM42670 SPI1", (id == 0x67), "");
    t_printf("        WHO_AM_I=0x%02X\r\n", id);
}

static void test_mag(void)
{
    /* 注意：HAL I2C 地址必须为 7 位地址左移 1 位后的 8 位格式 */
    uint8_t id = i2c_read8(&hi2c1, 0x0E << 1, 0x00);   /* IST8310 WIA */
    t_result("Mag IST8310 I2C1", (id == 0x10), "");
    t_printf("        WIA=0x%02X\r\n", id);
}

static void test_baro(void)
{
    uint8_t id = i2c_read8(&hi2c2, 0x76 << 1, 0x0D);   /* SPA06 CHIP_ID */
    t_result("Baro SPA06 I2C2", (id == 0x11 || id == 0x10), "");
    t_printf("        CHIP_ID=0x%02X\r\n", id);
}

static void test_adc(void)
{
    float v = 0, c = 0;
    adc_read(&v, &c);
    t_result("ADC V/I", (v > 0.1f), "");
    t_printf("        V=%.2fV  I=%.2fA\r\n", v, c);
}

static void test_sd(void)
{
    HAL_SD_CardInfoTypeDef info;

    /* 卡检测脚 PA8：本板 TF_CD=0 表示卡在位（机械开关）。
       先读一次，用于区分"没插卡"和"插了卡但通信失败"。 */
    uint8_t cd = HAL_GPIO_ReadPin(TF_CD_GPIO_Port, TF_CD_Pin);

    /* 重试 3 次（MX_SDMMC1_SD_Init 已先试过 1 次），兼容上电慢的卡。
       注意：HAL 失败路径的 ErrorCode 是 |= 累积的，每次尝试前必须清零，
       否则错误码会混入上一次失败的原因，无法用于诊断。 */
    HAL_StatusTypeDef st = HAL_ERROR;
    for (int i = 0; i < 3 && st != HAL_OK; ++i)
    {
        hsd1.ErrorCode = HAL_SD_ERROR_NONE;
        st = HAL_SD_Init(&hsd1);
        if (st != HAL_OK)
        {
            HAL_Delay(20);
        }
    }

    if (st == HAL_OK && HAL_SD_GetCardInfo(&hsd1, &info) == HAL_OK)
    {
        t_result("SDMMC SD card", 1, "");
        t_printf("        Capacity=%.0fMB Block=%lu\r\n",
                 (float)((uint64_t)info.BlockNbr * info.LogBlockSize) / (1024.0f * 1024.0f),
                 (unsigned long)info.LogBlockSize);
    }
    else if (cd == GPIO_PIN_RESET)
    {
        /* 卡在位但初始化失败：大概率是 CMD/D0-D3/CK 焊点或上拉问题，不是卡的错 */
        t_result("SDMMC SD card", 0, "card-in but init fail");
        t_printf("        TF_CD=0 err=0x%08lX 查CMD/D0-D3/CK焊点及上拉\r\n",
                 (unsigned long)hsd1.ErrorCode);
    }
    else
    {
        t_result("SDMMC SD card", 0, "no card");
        t_printf("        TF_CD=1 err=0x%08lX\r\n", (unsigned long)hsd1.ErrorCode);
    }
}

static void test_uart_tx(void)
{
    int ok1 = (HAL_UART_Transmit(&huart1, (uint8_t *)"T", 1, 100) == HAL_OK);
    int ok3 = (HAL_UART_Transmit(&huart3, (uint8_t *)"T", 1, 100) == HAL_OK);
    int ok4 = (HAL_UART_Transmit(&huart4, (uint8_t *)"T", 1, 100) == HAL_OK);
    t_result("UART TX (1/3/4)", (ok1 && ok3 && ok4), "");
    t_printf("        USART1=%d USART3=%d UART4=%d (RX 需外部环回)\r\n", ok1, ok3, ok4);
}

static void test_pwm(void)
{
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);
    __HAL_TIM_MOE_ENABLE(&htim1);

    /* 以 1000/1500/2000us 顺序摆动，便于用舵机测试仪/示波器核对。
     * 注意：TIM_CHANNEL_x 是 CCER 位偏移(0/4/8/12)，不能直接用 1..4 代替，
     * 否则 __HAL_TIM_SET_COMPARE 的匹配分支全部落空、写入被归并到 CCR4。 */
    const uint16_t seq[3] = { 1000, 1500, 2000 };
    const uint32_t ch[4] = { TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4 };
    for (int s = 0; s < 3; ++s)
    {
        for (int c = 0; c < 4; ++c)
        {
            __HAL_TIM_SET_COMPARE(&htim1, ch[c], seq[s]);
            __HAL_TIM_SET_COMPARE(&htim4, ch[c], seq[s]);
        }
        HAL_Delay(150);
    }
    for (int c = 0; c < 4; ++c)
    {
        __HAL_TIM_SET_COMPARE(&htim1, ch[c], 1500);
        __HAL_TIM_SET_COMPARE(&htim4, ch[c], 1500);
    }
    t_result("PWM TIM1/TIM4", 1, "started");
}

static void test_gpio(void)
{
    /* 蜂鸣器短鸣，LED 亮 */
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
    HAL_Delay(60);
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(WS2812_DIN_GPIO_Port, WS2812_DIN_Pin, GPIO_PIN_SET);
    HAL_Delay(30);
    HAL_GPIO_WritePin(WS2812_DIN_GPIO_Port, WS2812_DIN_Pin, GPIO_PIN_RESET);

    uint8_t tf = HAL_GPIO_ReadPin(TF_CD_GPIO_Port, TF_CD_Pin);
    t_result("GPIO IO (beep/LED/card)", 1, "");
    t_printf("        TF_CD=%u (0=有卡)  MAG_DRDY=%u  IMU_INT=%u\r\n",
             (unsigned)tf,
             (unsigned)HAL_GPIO_ReadPin(MAG_DRDY_GPIO_Port, MAG_DRDY_Pin),
             (unsigned)HAL_GPIO_ReadPin(IMU_INT1_GPIO_Port, IMU_INT1_Pin));
}

/* ---------------- 运行时兜底 ---------------- */
/* CubeMX 重新生成代码时 SPI1 DataSize 可能回退为其默认值 4BIT（对 H7 SPI 是个坑），
 * 4 位帧会让 IMU 寄存器读写完全错位。本文件不在 CubeMX 再生成范围内，
 * 在此强制纠正一次，保证 IMU 测试始终有效。 */
static void ensure_spi8(void)
{
    if (hspi1.Init.DataSize != SPI_DATASIZE_8BIT)
    {
        hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
        (void)HAL_SPI_Init(&hspi1);
    }
}

/* ---------------- 主入口 ---------------- */
void Test_Run(void)
{
    g_test_idx = 1; g_test_pass = 0; g_test_fail = 0;

    ensure_spi8();

    t_print("\r\n");
    t_print("=== FC_H743 PERIPHERAL SELF-TEST ===\r\n");

    test_console();
    HAL_Delay(50);
    test_imu();
    test_mag();
    test_baro();
    test_adc();
    test_sd();
    test_uart_tx();
    test_pwm();
    test_gpio();

    t_printf("\r\n=== RESULT: %d/%d PASS, %d FAIL ===\r\n\r\n",
             g_test_pass, g_test_pass + g_test_fail, g_test_fail);
}

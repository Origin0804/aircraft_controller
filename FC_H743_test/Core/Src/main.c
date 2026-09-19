/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "sdmmc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "test.h"
#include "imu_test.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 测试模式开关【改下面 TEST_MODE 一个宏即可切换】：
 *   TEST_MODE_FULL_SELFTEST (0) = 完整 9 项外设自检（Test_Run）
 *   TEST_MODE_SERIAL_ONLY   (1) = 只初始化 USART1 + GPIO，主循环打心跳
 *   TEST_MODE_IMU_STREAM    (2) = SPI1 读 ICM-42670-P，六轴数据流打印到 USART1
 *   TEST_MODE_I2C_SCAN      (3) = I2C 总线诊断：空闲电平 + 全地址扫描
 *   TEST_MODE_MAG_STREAM    (4) = I2C2 读 IST8310 地磁并流式打印
 *   TEST_MODE_BUZZER        (5) = 蜂鸣器测试（PC7，直流 + 方波两种驱动各试一遍）
 * 每个模式只初始化自己需要的外设：无关外设的初始化失败既会浪费排查时间，
 * 也可能在 USART1 就绪前把流程带进 Error_Handler。 */
#define TEST_MODE_FULL_SELFTEST  0
#define TEST_MODE_SERIAL_ONLY    1
#define TEST_MODE_IMU_STREAM     2
#define TEST_MODE_I2C_SCAN       3
#define TEST_MODE_MAG_STREAM     4
#define TEST_MODE_BUZZER         5

#define TEST_MODE  TEST_MODE_IMU_STREAM
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
#if (TEST_MODE == TEST_MODE_SERIAL_ONLY)
static void Serial_Only_Test(void);
#endif
#if (TEST_MODE == TEST_MODE_I2C_SCAN)
static void I2c_Scan_Test(void);
#endif
#if (TEST_MODE == TEST_MODE_MAG_STREAM)
static void Mag_Stream_Test(void);
#endif
#if (TEST_MODE == TEST_MODE_BUZZER)
static void Buzzer_Test(void);
#endif
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  /* GPIO 保留：只配引脚电平/方向，不会失败也不会挂死，且维持 IMU_CS 拉高等默认状态 */
  MX_GPIO_Init();

  /* 控制台提前到所有外设之前就绪：这样后续任何外设初始化失败，
     Error_Handler 都还有串口可以打印（原顺序里 SDMMC 排在 USART1 前面，
     失败时 huart1 尚未初始化，Error_Handler 只能静默返回）。 */
  MX_USART1_UART_Init();

#if (TEST_MODE == TEST_MODE_FULL_SELFTEST)
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_SPI1_Init();
  MX_SDMMC1_SD_Init();
  MX_USART3_UART_Init();
  MX_UART4_Init();
  MX_ADC1_Init();
  MX_TIM1_Init();
  MX_TIM4_Init();
#elif (TEST_MODE == TEST_MODE_IMU_STREAM)
  MX_SPI1_Init();
#elif (TEST_MODE == TEST_MODE_I2C_SCAN) || (TEST_MODE == TEST_MODE_MAG_STREAM)
  MX_I2C1_Init();
  MX_I2C2_Init();
#endif

  /* USER CODE BEGIN 2 */
#if (TEST_MODE == TEST_MODE_SERIAL_ONLY)
  /* 只验证串口：UART 之前再无任何可能失败的外设初始化 */
  Serial_Only_Test();
#elif (TEST_MODE == TEST_MODE_IMU_STREAM)
  /* 关掉 IMU_INT1(PC5) 的 EXTI：gpio.c 把它配成"上升沿中断 + 无上下拉"、
     优先级最高的 0。IMU 未焊/INT 脚悬空时会疯狂触发中断饿死 SysTick，
     导致 HAL_Delay 永久卡住、串口静止。本测试只轮询，不需要中断。 */
  HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);
  /* 读 ICM-42670-P 并流式打印 */
  Imu_Stream_Test();
#elif (TEST_MODE == TEST_MODE_I2C_SCAN)
  /* I2C 总线诊断：空闲电平 + 全地址扫描 */
  I2c_Scan_Test();
#elif (TEST_MODE == TEST_MODE_MAG_STREAM)
  /* 读 IST8310 地磁并流式打印 */
  Mag_Stream_Test();
#elif (TEST_MODE == TEST_MODE_BUZZER)
  /* 蜂鸣器测试：直流与方波各试一遍 */
  Buzzer_Test();
#else
  /* 焊后外设批量自检：结果输出到 USART1 控制台 */
  /* 【必须】关掉 IMU_INT1(PC5) 的 EXTI 中断。gpio.c 把它配成"上升沿中断 + 无上下拉"，
     优先级还是最高的 0（高于 SysTick 的 15）。IMU 未焊或已拆时 PC5 完全悬空，
     会疯狂触发中断并饿死 SysTick —— HAL_Delay 永久卡住、整个自检中途静止，
     症状和之前那个 SysTick 缺失一模一样，极难排查。自检只轮询该脚电平，不需要中断。 */
  HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);
  Test_Run();
#endif
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 8;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
#if (TEST_MODE == TEST_MODE_BUZZER)
/* ---------------- 蜂鸣器测试（PC7） ----------------
 * 分两个阶段，因为【有源】和【无源】蜂鸣器的驱动方式完全不同：
 *   有源蜂鸣器：内部自带振荡电路，引脚给直流（拉高）就响
 *   无源蜂鸣器：只是一个换能器，必须用方波驱动才会响（常用谐振频率 2~4kHz）
 * 原来的自检只做"拉高 60ms"，如果板上是无源蜂鸣器就永远不会响，容易被误判成坏。
 * 这里两种都试，听哪一段响就知道是哪种；两段都不响再查焊接/驱动电路。 */
static void bz_print(const char *s)
{
    if (s) (void)HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 200);
}

static void bz_printf(const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) (void)HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)n, 200);
}

/* 把 GPIOC 的配置寄存器直接打出来。
 * 判据：ODR 是"我们让它输出的值"，IDR 是"引脚上实际的电平"。
 *   ODR=1 且 IDR=0  -> 引脚被外部拽住（短路/重载）—— 硬件问题
 *   MODER 不是"输出" -> 配置根本没生效 —— 软件问题
 * 只靠万用表分不清这两种，寄存器一看就明白。 */
static void gpio_dump_pc7(const char *tag)
{
    uint32_t moder = GPIOC->MODER;
    uint32_t pupdr = GPIOC->PUPDR;

    bz_printf("  [%s] GPIOC 寄存器:\r\n", tag);
    bz_printf("    MODER   = 0x%08lX   PC7[15:14]=%lu (00输入 01输出 10复用 11模拟)\r\n",
              (unsigned long)moder, (unsigned long)((moder >> 14) & 0x3UL));
    bz_printf("    OTYPER  = 0x%08lX   PC7[7]=%lu (0推挽 1开漏)\r\n",
              (unsigned long)GPIOC->OTYPER, (unsigned long)((GPIOC->OTYPER >> 7) & 1UL));
    bz_printf("    PUPDR   = 0x%08lX   PC7[15:14]=%lu (00无 01上拉 10下拉)\r\n",
              (unsigned long)pupdr, (unsigned long)((pupdr >> 14) & 0x3UL));
    bz_printf("    ODR     = 0x%08lX   PC7[7]=%lu  <- 我们让它输出的值\r\n",
              (unsigned long)GPIOC->ODR, (unsigned long)((GPIOC->ODR >> 7) & 1UL));
    bz_printf("    IDR     = 0x%08lX   PC7[7]=%lu  <- 引脚上实际读到的电平\r\n",
              (unsigned long)GPIOC->IDR, (unsigned long)((GPIOC->IDR >> 7) & 1UL));

    if (((moder >> 14) & 0x3UL) != 0x1UL)
    {
        bz_print("    !! MODER 不是「输出」—— PC7 的配置根本没生效，是软件问题\r\n");
    }
    else if (((GPIOC->ODR >> 7) & 1UL) == 1UL && ((GPIOC->IDR >> 7) & 1UL) == 0UL)
    {
        bz_print("    !! ODR=1 但 IDR=0 —— 引脚被外部拽在低电平（短路或重载）\r\n");
    }
    else
    {
        bz_print("    -- ODR 与 IDR 一致，配置与引脚状态正常\r\n");
    }
}

/* 约 185µs @480MHz —— 半周期，对应约 2.7kHz 方波（无源蜂鸣器常见谐振频率附近） */
static void bz_half_period(void)
{
    for (volatile int i = 0; i < 18000; ++i) { }
}

static void Buzzer_Test(void)
{
    bz_print("\r\n\r\n=== 蜂鸣器测试 (PC7) ===\r\n");
    bz_print("每轮 4 段，时间都拉长，方便一边看串口一边拿万用表量：\r\n");
    bz_print("  [A] 持续高 5 秒 —— 有源蜂鸣器应【持续响】；此时量 PC7 应为 3.3V\r\n");
    bz_print("  [B] 持续低 2 秒 —— 应该安静；此时量 PC7 应为 0V\r\n");
    bz_print("  [C] 方波 3 秒   —— 无源蜂鸣器才会响\r\n");
    bz_print("  [D] 持续低 2 秒 —— 安静\r\n\r\n");

    uint32_t n = 0;
    while (1)
    {
        bz_printf("--- 第 %lu 轮 ---\r\n", (unsigned long)n++);

        /* [A] 持续高 5 秒：时间足够从容测量，有源蜂鸣器应持续发声 */
        bz_print("[A] PC7 = 高，持续 15 秒 ...【现在应该响】\r\n"
                 "    依次量这四点（都在本段内完成）：\r\n"
                 "      1) PC7(MCU 引脚)         应为 3.3V\r\n"
                 "      2) R19 靠 BUZZERIO 那一端 应为 3.3V   <- 关键：若这里 0V 而 1) 是 3.3V，\r\n"
                 "                                            则 PC7 到 R19 之间是断的\r\n"
                 "      3) R19 靠基极那一端       应约 0.75V\r\n"
                 "      4) Q1 集电极             应接近 0V（导通）\r\n");
        HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
        HAL_Delay(200);
        gpio_dump_pc7("高电平期间");
        HAL_Delay(14800);

        /* [B] 持续低 5 秒：对照段 */
        bz_print("[B] PC7 = 低，持续 5 秒 ...应该安静，量 PC7 应为 0V\r\n");
        HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
        HAL_Delay(200);
        gpio_dump_pc7("低电平期间");
        HAL_Delay(4800);

        /* [C] 约 2.7kHz 方波 3 秒：只有无源蜂鸣器会响
         * 直接用 BSRR 寄存器翻转，比 HAL_GPIO_WritePin 快得多，频率才够准 */
        bz_print("[C] 方波约 2.7kHz，持续 3 秒 ...（无源蜂鸣器才会响）\r\n");
        for (int c = 0; c < 8000; ++c)
        {
            BUZZER_GPIO_Port->BSRR = BUZZER_Pin;
            bz_half_period();
            BUZZER_GPIO_Port->BSRR = (uint32_t)BUZZER_Pin << 16U;
            bz_half_period();
        }
        HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);

        /* [D] 收尾安静段 */
        bz_print("[D] PC7 = 低，持续 2 秒 ...\r\n\r\n");
        HAL_Delay(2000);
    }
}
#endif

#if (TEST_MODE == TEST_MODE_MAG_STREAM)
/* ---------------- IST8310 地磁数据流 ----------------
 * 总线：I2C2（PB10=SCL / PB11=SDA）—— 由网表确认：BARO_SCL/BARO_SDA 上挂着
 * U1.46/47、U4.1/U4.16（IST8310）、U3.3/U3.4（气压计）、R2/R3 上拉。
 *
 * 【地址 = 0x0C，实板实测值，不要按手册改成 0x0E】
 * 实测确认（2026-09-13）：7 位地址 0x0C、WIA(0x00) = 0x10，读出数据、转动板子
 * 有变化。手册标称 0x0E，README 记的"I2C1 / 0x0E"是错的 —— 两者都与本板实测
 * 不符。实测优先于手册：把实测值当 bug"修"掉，会把一个本来好使的路径改坏。
 * 下面 ist_pick_addr() 先用 0x0C；只有 0x0C 读不通时才扫总线找替代，用于诊断。
 *
 * 寄存器表说明：上次 dump 显示 0x0A(CNTL1)=0x00、0x03~0x08 数据寄存器全 0
 * —— 芯片处于"不测量"状态，必须先写配置才会出数。本文件不假定配置值正确，
 * 而是【写前 dump 一次、写后 dump 一次】，用回读值自己验证有没有生效。 */
#define IST_ADDR7_DEFAULT  0x0C   /* 本板实测地址（手册标称 0x0E） */
#define IST_WIA_ID         0x10   /* IST8310  */
#define IST_WIA_ID_J       0xA3   /* IST8310J */

static uint8_t g_ist_addr7 = IST_ADDR7_DEFAULT;   /* 0x0C 读不通时才由扫描覆盖 */
#define IST_WIA      0x00
#define IST_STAT1    0x02
#define IST_DATA_XL  0x03   /* X 低/高字节，随后 Y、Z，小端 int16 */
#define IST_STAT2    0x09
#define IST_CNTL1    0x0A
#define IST_CNTL2    0x0B
#define IST_AVGCNTL  0x41
#define IST_PDCNTL   0x42

static void mag_print(const char *s)
{
    if (s) (void)HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 200);
}

static void mag_printf(const char *fmt, ...)
{
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) (void)HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)n, 200);
}

static int ist_rd(uint8_t reg, uint8_t *v)
{
    return (HAL_I2C_Mem_Read(&hi2c2, (uint16_t)(g_ist_addr7 << 1), reg,
                             I2C_MEMADD_SIZE_8BIT, v, 1, 20) == HAL_OK);
}

static int ist_wr(uint8_t reg, uint8_t val)
{
    return (HAL_I2C_Mem_Write(&hi2c2, (uint16_t)(g_ist_addr7 << 1), reg,
                              I2C_MEMADD_SIZE_8BIT, &val, 1, 20) == HAL_OK);
}

/* 地址选择：**先用本板实测的 0x0C**，读通就直接用（与一直好使的那条路径完全一致，
 * 不引入任何行为变化）；只有 0x0C 读不通时才扫总线找替代，纯粹为诊断服务。
 * 这样"复焊后地磁还在不在"这件事能一次问清，同时不动原来能用的配置。 */
static void ist_pick_addr(void)
{
    uint8_t wia = 0xFF;

    if (ist_rd(IST_WIA, &wia) && (wia == IST_WIA_ID || wia == IST_WIA_ID_J))
    {
        mag_printf("地址 0x%02X（本板实测值）直接读通，WIA=0x%02X\r\n\r\n", g_ist_addr7, wia);
        return;
    }

    mag_printf("地址 0x%02X 读不通（WIA=0x%02X）-> 扫总线找地磁：\r\n", g_ist_addr7, wia);

    int found = 0, hit = 0;
    for (uint16_t a = 0x08; a <= 0x77; ++a)
    {
        hi2c2.ErrorCode = HAL_I2C_ERROR_NONE;   /* HAL 的 ErrorCode 是 |= 累积的 */
        if (HAL_I2C_IsDeviceReady(&hi2c2, (uint16_t)(a << 1), 2, 5) != HAL_OK) continue;
        ++found;

        uint8_t w = 0xFF;
        HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c2, (uint16_t)(a << 1), IST_WIA,
                                                I2C_MEMADD_SIZE_8BIT, &w, 1, 20);
        mag_printf("  应答 0x%02X  WIA(0x00)=0x%02X  (st=%d)\r\n", a, w, (int)st);

        if (st == HAL_OK && (w == IST_WIA_ID || w == IST_WIA_ID_J))
        {
            g_ist_addr7 = (uint8_t)a;
            ++hit;
            mag_printf("     ^^ 认定为 IST8310（WIA=0x%02X）\r\n", w);
        }
    }

    if (found == 0)
        mag_print("  没有任何地址应答 -> 总线/供电/焊接问题（不是地址问题）\r\n");
    else if (hit == 0)
        mag_print("  有器件应答但没有一个 WIA 像 IST8310\r\n");

    mag_printf("--> 接下来使用地址 0x%02X\r\n\r\n", g_ist_addr7);
}

/* 全量 dump 0x00~0x7F：用来找数据寄存器到底在哪、以及看芯片的整体状态 */
static void ist_dump_all(void)
{
    mag_print("--- 全寄存器 dump 0x00~0x7F ---\r\n");
    for (uint16_t r = 0x00; r <= 0x7F; ++r)
    {
        uint8_t v = 0xFF;
        (void)ist_rd((uint8_t)r, &v);
        if ((r % 16) == 0) mag_printf("  %02X:", r);
        mag_printf(" %02X", v);
        if ((r % 16) == 15) mag_print("\r\n");
    }
}

/* 写测试：写入和当前值【不同】的数，看回读有没有跟着变。
 * 之前那次"写入=回读"是无效验证 —— 两者本来就相同，证明不了写生效。 */
static void ist_write_test(void)
{
    uint8_t rb = 0xFF;

    mag_print("\r\n--- 写测试（CNTL1 = 0x0A）---\r\n");
    (void)ist_rd(IST_CNTL1, &rb);
    mag_printf("  当前 CNTL1        = 0x%02X\r\n", rb);

    (void)ist_wr(IST_CNTL1, 0x00);          /* 先写 standby */
    HAL_Delay(5);
    (void)ist_rd(IST_CNTL1, &rb);
    mag_printf("  写 0x00 后回读    = 0x%02X  %s\r\n", rb,
               (rb == 0x00) ? "-> 写生效" : "-> 回读没变，写根本没进去");

    (void)ist_wr(IST_CNTL1, 0x0B);          /* 恢复连续测量 */
    HAL_Delay(5);
    (void)ist_rd(IST_CNTL1, &rb);
    mag_printf("  写 0x0B 后回读    = 0x%02X  %s\r\n", rb,
               (rb == 0x0B) ? "-> 写生效" : "-> 回读没变");
}

/* 逐个试几种测量模式，看哪种能让数据寄存器出数、让 STAT1 的 DRDY 置位 */
static void ist_try_mode(uint8_t cntl1, const char *name)
{
    uint8_t rb = 0xFF, s1 = 0xFF, s2 = 0xFF, d[6] = {0};

    (void)ist_wr(IST_CNTL1, cntl1);
    HAL_Delay(50);

    (void)ist_rd(IST_CNTL1, &rb);
    (void)ist_rd(IST_STAT1, &s1);
    (void)ist_rd(IST_STAT2, &s2);
    (void)HAL_I2C_Mem_Read(&hi2c2, (uint16_t)(g_ist_addr7 << 1), IST_DATA_XL,
                           I2C_MEMADD_SIZE_8BIT, d, 6, 20);

    int16_t mx = (int16_t)((d[1] << 8) | d[0]);
    int16_t my = (int16_t)((d[3] << 8) | d[2]);
    int16_t mz = (int16_t)((d[5] << 8) | d[4]);

    mag_printf("  CNTL1<-0x%02X %-12s 回读=%02X STAT1=%02X STAT2=%02X raw=(%6d,%6d,%6d)\r\n",
               cntl1, name, rb, s1, s2, mx, my, mz);
}

static void Mag_Stream_Test(void)
{
    mag_print("\r\n\r\n=== IST8310 地磁诊断+数据流 ===\r\n");
    mag_print("I2C2  PB10=SCL / PB11=SDA   115200 8N1\r\n");
    mag_print("地址用本板实测值 0x0C（手册标称 0x0E；0x0C 不通时才自动扫总线）\r\n\r\n");

    /* 先确认器件在不在（不动原来能用的地址配置） */
    ist_pick_addr();

    uint8_t wia = 0;
    (void)ist_rd(IST_WIA, &wia);
    mag_printf("WIA(0x00) = 0x%02X   期望 0x10（或 IST8310J 的 0xA3）  -> %s\r\n",
               wia, (wia == IST_WIA_ID || wia == IST_WIA_ID_J) ? "OK" : "FAIL");

    ist_dump_all();
    ist_write_test();

    /* 软件复位：有些情况下芯片需要一次 SRST 才会真正开始测量 */
    mag_print("\r\n--- 软复位 CNTL2(0x0B) bit0 ---\r\n");
    uint8_t c2 = 0;
    (void)ist_rd(IST_CNTL2, &c2);
    (void)ist_wr(IST_CNTL2, (uint8_t)(c2 | 0x01));
    HAL_Delay(50);
    (void)ist_wr(IST_CNTL2, c2);
    (void)ist_wr(IST_AVGCNTL, 0x24);
    (void)ist_wr(IST_PDCNTL,  0xC0);
    HAL_Delay(50);

    mag_print("\r\n--- 逐个试测量模式 ---\r\n");
    ist_try_mode(0x00, "standby");
    ist_try_mode(0x01, "single");
    ist_try_mode(0x0B, "continuous");

    mag_print("\r\n【请转动板子】观察下面 X/Y/Z 是否变化\r\n\r\n");

    uint32_t n = 0;
    while (1)
    {
        uint8_t d[6] = {0};
        uint8_t s1 = 0, s2 = 0;

        HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c2, (uint16_t)(g_ist_addr7 << 1),
                                                IST_DATA_XL, I2C_MEMADD_SIZE_8BIT,
                                                d, 6, 20);
        (void)ist_rd(IST_STAT1, &s1);
        (void)ist_rd(IST_STAT2, &s2);

        if (st != HAL_OK)
        {
            mag_printf("[%lu] 读失败 st=%d err=0x%08lX\r\n",
                       (unsigned long)n++, (int)st, (unsigned long)hi2c2.ErrorCode);
            HAL_Delay(500);
            continue;
        }

        /* 小端：先低字节后高字节 */
        int16_t mx = (int16_t)((d[1] << 8) | d[0]);
        int16_t my = (int16_t)((d[3] << 8) | d[2]);
        int16_t mz = (int16_t)((d[5] << 8) | d[4]);

        mag_printf("[%lu] raw=(%6d,%6d,%6d)  %7.1f,%7.1f,%7.1f uT  STAT1=0x%02X STAT2=0x%02X\r\n",
                   (unsigned long)n++, mx, my, mz,
                   mx * 0.3f, my * 0.3f, mz * 0.3f, s1, s2);
        HAL_Delay(100);
    }
}
#endif

#if (TEST_MODE == TEST_MODE_I2C_SCAN)
/* ---------------- I2C 总线诊断 ----------------
 * 目的：地磁已焊、上拉也在，却读不到 —— 要区分是"总线本身不通"还是
 * "地址/型号不对"。做法两步：先看空闲电平（该高的地方是不是被拽低），
 * 再枚举全部 7 位地址看谁应答。
 * 分析要点：
 *   无任何应答        -> 总线级问题（没供电/走线断/SDA-SCL 反了/上拉没接到）
 *   应答地址一大片    -> SDA 被短到地，不是真有那么多器件
 *   在别的地址应答    -> 我们地址假设错了，或板上型号不是 IST8310
 *   正好 0x0E 应答    -> 总线是好的，问题在寄存器/初始化流程 */
static void scan_print(const char *s)
{
    if (s) (void)HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 200);
}

static void scan_printf(const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) (void)HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)n, 200);
}

/* 原样改回 i2c.c 里的配置，否则后续 I2C 传输会失效 */
static void i2c_pins_restore(GPIO_TypeDef *port, uint16_t pins)
{
    GPIO_InitTypeDef g = {0};
    g.Pin       = pins;
    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF4_I2C1;   /* H7 上 I2C1/I2C2 同为 AF4 */
    HAL_GPIO_Init(port, &g);
}

/* AF_OD + 内部上拉：给总线补一个约 40k 的弱上拉。
 * 用于验证"板上的外部上拉到底有没有接到 MCU 这一侧"—— 若外部上拉正常，
 * 补不补内部上拉都该扫得到；若只有补了才扫得到，说明外部上拉没接过来。 */
static void i2c_pins_pullup(GPIO_TypeDef *port, uint16_t pins)
{
    GPIO_InitTypeDef g = {0};
    g.Pin       = pins;
    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(port, &g);
}

/* 把 HAL 的失败原因翻译清楚。
 * 注意：HAL 超时时返回的是 HAL_ERROR 且 ErrorCode 里带 TIMEOUT 位，
 * 而不是返回 HAL_TIMEOUT —— 之前按 st==HAL_TIMEOUT 判断，导致真正的超时
 * 全部掉进 else 被误报成"BERR/ARLO"，把排查方向带偏了。 */
static const char *i2c_err_name(HAL_StatusTypeDef st, uint32_t err)
{
    if ((err & HAL_I2C_ERROR_AF)      != 0U) return "NACK(器件不应答)";
    if ((err & HAL_I2C_ERROR_TIMEOUT) != 0U) return "TIMEOUT(事务没走完)";
    if ((err & HAL_I2C_ERROR_BERR)    != 0U) return "BERR(总线错误)";
    if ((err & HAL_I2C_ERROR_ARLO)    != 0U) return "ARLO(仲裁丢失)";
    if (st == HAL_BUSY)                      return "BUSY(总线被占/BUSY 清不掉)";
    if (st == HAL_TIMEOUT)                   return "TIMEOUT(HAL 层)";
    return "其它";
}

/* 用指定上下拉读一次引脚电平，用于三态对比测量 */
static int i2c_line_level(GPIO_TypeDef *port, uint16_t pin, uint32_t pull)
{
    GPIO_InitTypeDef g = {0};
    g.Pin  = pin;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = pull;
    HAL_GPIO_Init(port, &g);
    HAL_Delay(2);
    return (int)HAL_GPIO_ReadPin(port, pin);
}

/* ---------------- GPIO bit-bang I2C（绕开 STM32 I2C 外设） ----------------
 * 两个目的：
 *  1) 排除"I2C 外设自身不产生 SCL"—— I2C2 上拉完好、总线上没有器件，
 *     正常应立刻回 NACK，实测却是 TIMEOUT（NACK 和 STOPF 都没出现），
 *     说明地址位根本没移出去。这一步能判断问题在不在外设。
 *  2) 它用【开漏 + MCU 内部上拉】并把速度压到约 10kHz —— 内部 40k 上拉在
 *     这个速度下足够，于是【可以绕过板上外部上拉缺失】的情况。
 *     若这样能读到地磁，就说明地磁芯片本身是好的、问题在总线电气。
 * SDA 用开漏输出并回读 IDR：ODR=1 即释放总线，从机拉低时能读到 0。
 * 【注意】地磁在 I2C2 = PB10/PB11（网表确认），不是 I2C1 的 PB6/PB7。 */
#define BB_PORT      GPIOB
#define BB_SCL_PIN   GPIO_PIN_10
#define BB_SDA_PIN   GPIO_PIN_11

static void bb_i2c_delay(void)
{
    for (volatile int i = 0; i < 5000; ++i) { }   /* 约 50µs @480MHz -> 约 10kHz */
}

static void bb_scl(int v) { HAL_GPIO_WritePin(BB_PORT, BB_SCL_PIN, v ? GPIO_PIN_SET : GPIO_PIN_RESET); }
static void bb_sda(int v) { HAL_GPIO_WritePin(BB_PORT, BB_SDA_PIN, v ? GPIO_PIN_SET : GPIO_PIN_RESET); }
static int  bb_scl_read(void) { return (int)HAL_GPIO_ReadPin(BB_PORT, BB_SCL_PIN); }
static int  bb_sda_read(void) { return (int)HAL_GPIO_ReadPin(BB_PORT, BB_SDA_PIN); }

static void bb_i2c_pins_bitbang(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin   = BB_SCL_PIN | BB_SDA_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_OD;    /* 开漏 + 内部上拉：等效于一条弱上拉的 I2C */
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BB_PORT, &g);
    bb_scl(1);
    bb_sda(1);                        /* 释放两根线 */
}

static void bb_i2c_restore(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin       = BB_SCL_PIN | BB_SDA_PIN;
    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF4_I2C1;      /* 交还给 I2C 外设 */
    HAL_GPIO_Init(BB_PORT, &g);
}

static void bb_i2c_start(void)
{
    bb_sda(1); bb_scl(1); bb_i2c_delay();
    bb_sda(0); bb_i2c_delay();        /* SCL 高时 SDA 下降 = START */
    bb_scl(0); bb_i2c_delay();
}

static void bb_i2c_stop(void)
{
    bb_sda(0); bb_i2c_delay();
    bb_scl(1); bb_i2c_delay();        /* SCL 高时 SDA 上升 = STOP */
    bb_sda(1); bb_i2c_delay();
}

/* 返回 0 = 收到 ACK，1 = NACK（没器件应答） */
static int bb_i2c_write_byte(uint8_t b)
{
    for (int i = 7; i >= 0; --i)
    {
        bb_sda((b >> i) & 1u);
        bb_i2c_delay();
        bb_scl(1); bb_i2c_delay();
        bb_scl(0); bb_i2c_delay();
    }
    bb_sda(1);                        /* 释放 SDA，交给从机应答 */
    bb_i2c_delay();
    bb_scl(1); bb_i2c_delay();
    int nack = bb_sda_read();         /* 从机拉低 = ACK */
    bb_scl(0); bb_i2c_delay();
    return nack;
}

static uint8_t bb_i2c_read_byte(int send_ack)
{
    uint8_t v = 0;
    bb_sda(1);                        /* 释放，由从机驱动 */
    for (int i = 7; i >= 0; --i)
    {
        bb_i2c_delay();
        bb_scl(1); bb_i2c_delay();
        if (bb_sda_read()) v = (uint8_t)(v | (1u << i));
        bb_scl(0);
    }
    bb_sda(send_ack ? 0 : 1);         /* 主机的应答位 */
    bb_i2c_delay();
    bb_scl(1); bb_i2c_delay();
    bb_scl(0); bb_i2c_delay();
    bb_sda(1);
    return v;
}

/* 按 IST8310 的时序读 WIA(0x00)，每一步的 ACK 都单独报出来。
 * 【两个候选地址各试一遍】（手册 0x0E / 本板曾实测 0x0C），
 * 这样"地址猜错"和"芯片没响应"能被彻底分开。 */
static void bb_i2c_mag_test(void)
{
    static const uint8_t cand[2] = { 0x0E, 0x0C };

    scan_print("\r\n=== bit-bang I2C 读地磁 WIA（PB10=SCL / PB11=SDA）===\r\n");
    scan_print("(开漏+内部上拉，约10kHz；绕开 I2C 外设，也绕开板上的外部上拉)\r\n");

    bb_i2c_pins_bitbang();
    bb_i2c_delay();

    bb_scl(1); bb_sda(1); bb_i2c_delay();
    scan_printf("  释放后空闲: SCL=%d SDA=%d (都应为1)\r\n", bb_scl_read(), bb_sda_read());

    for (int c = 0; c < 2; ++c)
    {
        uint8_t addr7 = cand[c];
        scan_printf("  --- 候选地址 0x%02X ---\r\n", addr7);

        for (int t = 0; t < 2; ++t)
        {
            bb_i2c_start();
            int n1 = bb_i2c_write_byte((uint8_t)(addr7 << 1));           /* 器件地址 + 写 */
            int n2 = n1 ? 1 : bb_i2c_write_byte(0x00);                   /* 寄存器 0x00 */
            if (!n1 && !n2) bb_i2c_start();                              /* repeated start */
            int n3 = (n1 || n2) ? 1 : bb_i2c_write_byte((uint8_t)((addr7 << 1) | 1));
            uint8_t v = (n1 || n2 || n3) ? 0 : bb_i2c_read_byte(0);      /* 末字节回 NACK */
            bb_i2c_stop();

            scan_printf("    第%d次: 地址%s 寄存器%s 读%s -> WIA=0x%02X\r\n", t + 1,
                        n1 ? "NACK" : "ACK", n2 ? "NACK" : "ACK", n3 ? "NACK" : "ACK", v);
            HAL_Delay(5);
        }
    }

    bb_i2c_restore();

    scan_print("  判读: 某个地址 WIA=0x10 -> 地磁芯片是好的，故障在总线电气（上拉/走线）\r\n");
    scan_print("        两个地址都 NACK   -> 地磁没供电，或 SDA/SCL 没接过去\r\n\r\n");
}

/* 把某个 I2C2 地址上的器件寄存器读出来看看到底是什么。
 * 结论【由读到的 WIA 决定】，不要写死 —— 地址写死会把"地址猜错"伪装成"芯片坏了"。
 * 网表已确认：I2C2(BARO_SCL/SDA) 上只挂 U3 气压计和 U4 地磁。 */
static void i2c2_dev_dump(uint8_t addr7)
{
    scan_printf("\r\n=== 读 I2C2 上 0x%02X 器件的寄存器 0x00~0x0F ===\r\n", addr7);

    uint8_t wia = 0xFF;
    int ok_any = 0;
    for (uint8_t reg = 0x00; reg <= 0x0F; ++reg)
    {
        uint8_t v = 0xFF;
        HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c2, (uint16_t)(addr7 << 1), reg,
                                                I2C_MEMADD_SIZE_8BIT, &v, 1, 20);
        if (st == HAL_OK) ok_any = 1;
        if (reg == 0x00) wia = v;
        scan_printf("  0x%02X = 0x%02X  (st=%d)\r\n", reg, v, (int)st);
    }

    if (!ok_any)
    {
        scan_printf("  -> 0x%02X 上没有任何一次读取成功，此地址无器件。\r\n", addr7);
    }
    else if (wia == 0x10 || wia == 0xA3)
    {
        scan_printf("  -> WIA=0x%02X，确认是 IST8310 地磁，地址就用 0x%02X。\r\n", wia, addr7);
    }
    else
    {
        scan_printf("  -> WIA=0x%02X 不是 IST8310（0x10 / 0xA3），需要确认这颗是什么器件。\r\n", wia);
    }
}

static void i2c_bus_check(I2C_HandleTypeDef *hi2c, const char *name,
                          GPIO_TypeDef *port, uint16_t scl_pin, uint16_t sda_pin)
{
    uint16_t pins = (uint16_t)(scl_pin | sda_pin);

    /* 先扫描：此时引脚处于 AF_OD + NOPULL 的干净状态，与 test_mag() 的真实条件一致。
       注意不要把"空闲电平检查"放在前面 —— 那需要把引脚临时改成 GPIO，
       可能扰动 I2C 外设状态，污染随后的扫描结果。 */
    scan_printf("\r\n--- %s 地址扫描 0x08~0x77 ---\r\n", name);

    int found = 0, n_nack = 0, n_timeout = 0, n_other = 0;
    HAL_StatusTypeDef last_st = HAL_OK;
    uint32_t          last_err = 0;

    for (uint16_t a = 0x08; a <= 0x77; ++a)
    {
        hi2c->ErrorCode = HAL_I2C_ERROR_NONE;   /* HAL 的 ErrorCode 是 |= 累积的，每次必须清 */
        HAL_StatusTypeDef st = HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(a << 1), 2, 5);

        if (st == HAL_OK)
        {
            scan_printf("  应答: 0x%02X\r\n", a);
            ++found;
        }
        else
        {
            uint32_t err = hi2c->ErrorCode;
            if ((err & HAL_I2C_ERROR_AF) != 0U)      ++n_nack;
            else if ((err & HAL_I2C_ERROR_TIMEOUT) != 0U) ++n_timeout;
            else                                      ++n_other;
            last_st  = st;
            last_err = err;
        }
    }

    scan_printf("  汇总: 应答=%d  NACK=%d  TIMEOUT=%d  其它=%d\r\n",
                found, n_nack, n_timeout, n_other);
    scan_printf("  末次失败: st=%d err=0x%08lX  -> %s\r\n",
                (int)last_st, (unsigned long)last_err, i2c_err_name(last_st, last_err));

    if (found > 8)
    {
        scan_print("  -> 应答地址过多：典型是 SDA 被短到地，不是真有那么多器件。\r\n");
    }
    else if (n_nack == 112 && n_nack > 0)
    {
        scan_print("  -> 全部 NACK：总线电气正常，但从机一个都不应答。\r\n"
                   "     指向器件侧：没供电、或 SDA/SCL 没接到该器件。\r\n");
    }
    else if (n_timeout > 0 || n_other > 0)
    {
        scan_print("  -> 出现 BUSY/TIMEOUT：事务根本没走完，与接了什么器件无关。\r\n"
                   "     指向总线电气（上拉/走线/电容）或 I2C 外设自身。\r\n");
    }

    /* 空闲电平：三态对比测量。
     * 【重要】不能只用"内部上拉"档 —— 那样读到 1 只能证明线没被短到地，
     * 完全掩盖了"板上外部上拉到底在不在"这个关键问题。加一路【内部下拉】：
     *   下拉仍读到 1  -> 外部上拉确实存在且有力
     *   上拉 1、下拉 0 -> 只有 MCU 内部上拉能拉高，板上外部上拉没接到这两根网线
     *   上拉也读到 0   -> 线被硬拽到地 */
    int scl_np = i2c_line_level(port, scl_pin, GPIO_NOPULL);
    int scl_pu = i2c_line_level(port, scl_pin, GPIO_PULLUP);
    int scl_pd = i2c_line_level(port, scl_pin, GPIO_PULLDOWN);
    int sda_np = i2c_line_level(port, sda_pin, GPIO_NOPULL);
    int sda_pu = i2c_line_level(port, sda_pin, GPIO_PULLUP);
    int sda_pd = i2c_line_level(port, sda_pin, GPIO_PULLDOWN);
    i2c_pins_restore(port, pins);

    scan_printf("  空闲电平 SCL(无/上/下)=%d/%d/%d   SDA(无/上/下)=%d/%d/%d\r\n",
                scl_np, scl_pu, scl_pd, sda_np, sda_pu, sda_pd);
    if (scl_pd == 1 && sda_pd == 1)
        scan_print("  -> 内部下拉也拉不动：外部上拉确实存在且有力。\r\n");
    else if (scl_pu == 1 && sda_pu == 1 && scl_pd == 0 && sda_pd == 0)
        scan_print("  -> 只有内部上拉能拉高：板上外部上拉没接到这两根网线上。\r\n");
    if (scl_pu == 0 || sda_pu == 0)
        scan_print("  ! 有线上拉都拉不起来：被硬拽到地。\r\n");

    scan_printf("  %s 寄存器: CR1=0x%08lX TIMINGR=0x%08lX ISR=0x%08lX State=%d\r\n",
                name, (unsigned long)hi2c->Instance->CR1,
                (unsigned long)hi2c->Instance->TIMINGR,
                (unsigned long)hi2c->Instance->ISR, (int)hi2c->State);
}

static void I2c_Scan_Test(void)
{
    scan_print("\r\n\r\n=== I2C BUS SCAN ===\r\n");
    scan_print("地磁 IST8310 预期地址 0x0E（WIA@0x00 应为 0x10）\r\n");

    /* 时钟配置直接打出来，免得依赖 SWD：
     * D2CCIP1R 的 I2C123SEL(bit[6:4]) 决定 I2C 内核时钟源，
     * APB1LENR 的 bit21/bit22 是 I2C1EN/I2C2EN。内核时钟缺失会让事务直接 TIMEOUT。 */
    scan_printf("RCC: D2CCIP1R=0x%08lX  APB1LENR=0x%08lX\r\n",
                (unsigned long)RCC->D2CCIP1R, (unsigned long)RCC->APB1LENR);

    /* 第一遍：用固件原本的配置（AF_OD + NOPULL，100kHz）。
     * 此时总线上只有【板上外部上拉】在工作，最贴近真实使用条件。 */
    i2c_bus_check(&hi2c1, "I2C1 (PB6=SCL/PB7=SDA, 地磁)", GPIOB, GPIO_PIN_6,  GPIO_PIN_7);
    i2c_bus_check(&hi2c2, "I2C2 (PB10=SCL/PB11=SDA)",     GPIOB, GPIO_PIN_10, GPIO_PIN_11);

    /* 第二遍：补上 MCU 内部上拉（约 40k）并把总线降速到约 25kHz 再扫。
     * 目的：第一遍的空闲电平是"临时开内部上拉"读的，读到 1 只证明线没被短到地，
     * 【不能证明板上的外部上拉接到了 PB6/PB7 这根网线上】。正式传输时引脚是
     * NOPULL，只有外部上拉在拉高 —— 若外部上拉没接到 MCU 侧，总线在传输期间升不到
     * 高电平，就会表现为"全地址无应答"。
     * 内部上拉太弱（40k）撑不起 100kHz 的上升沿，所以同时把 PRESC 从 3 提到 15
     * （120MHz/(16*4) -> 约 25kHz）。
     * 判读：这一遍能扫到地磁 -> 外部上拉没接到 MCU 侧（硬件问题）；
     *       仍然没有        -> 地磁侧没供电或 SDA/SCL 根本没接过来。 */
    scan_print("\r\n=== 第二遍：启用 MCU 内部上拉 + 降速到约 25kHz ===\r\n");
    scan_print("（若这一遍能扫到，说明板上外部上拉没接到 MCU 这一侧）\r\n");

    hi2c1.Init.Timing = 0xF07075B1;   /* PRESC 3->15，其余字段不变 */
    (void)HAL_I2C_Init(&hi2c1);
    i2c_pins_pullup(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);

    hi2c2.Init.Timing = 0xF07075B1;
    (void)HAL_I2C_Init(&hi2c2);
    i2c_pins_pullup(GPIOB, GPIO_PIN_10 | GPIO_PIN_11);

    i2c_bus_check(&hi2c1, "I2C1 [内部上拉]", GPIOB, GPIO_PIN_6,  GPIO_PIN_7);
    i2c_bus_check(&hi2c2, "I2C2 [内部上拉]", GPIOB, GPIO_PIN_10, GPIO_PIN_11);

    /* 两个候选地址都读一遍，用 WIA 判定地磁到底在哪个地址（不要预先认定） */
    i2c2_dev_dump(0x0E);
    i2c2_dev_dump(0x0C);

    /* bit-bang 兜底验证：绕开 I2C 外设（当前跑的是 I2C1 的引脚 PB6/PB7） */
    bb_i2c_mag_test();

    scan_print("\r\n=== SCAN DONE ===\r\n");
    while (1) { }
}
#endif

#if (TEST_MODE == TEST_MODE_SERIAL_ONLY)
/**
  * @brief  串口单项验证（只依赖 USART1）
  * @note   先打一条横幅，之后每 500ms 打一行心跳。
  *         心跳是关键：自检那种"上电只打印一次"的输出必须掐准复位时机才看得到，
  *         心跳则随时打开串口助手都有数据，能立刻区分"串口不通"和"错过了打印"。
  *         只有 RxFIFO 里 TX 相关的路径参与，输出走 PA9。
  */
static void Serial_Only_Test(void)
{
  static const char banner[] =
      "\r\n\r\n"
      "=== SERIAL ONLY TEST ===\r\n"
      "USART1  PA9(TX)/PA10(RX)  115200 8N1\r\n"
      "若看到本行及下面的 heartbeat，说明 CH340+USART1 链路正常\r\n"
      "\r\n";
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)banner,
                          (uint16_t)(sizeof(banner) - 1), 1000);

  uint32_t n = 0;
  while (1)
  {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "heartbeat %lu  (USART1 TX OK)\r\n",
                       (unsigned long)n++);
    if (len > 0)
    {
      (void)HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)len, 1000);
    }
    HAL_Delay(500);
  }
}
#endif
/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* 测试固件策略【勿删】：初始化失败不静默死机。
   * 最现实的失败场景是未插 TF 卡时 MX_SDMMC1_SD_Init 报错——它发生在
   * USART1 初始化之前，若在这里 __disable_irq()+while(1) 整板将无任何输出。
   * 返回（而非挂死）后主流程继续，test_sd() 会打印 FAIL 与错误码。
   * 注意：绝不能 __disable_irq() 后返回，否则下一次 HAL_Delay 会永久卡死。 */
  if (huart1.Instance == USART1 && huart1.gState == HAL_UART_STATE_READY)
  {
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)"!INIT-ERR!\r\n", 12, 100);
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

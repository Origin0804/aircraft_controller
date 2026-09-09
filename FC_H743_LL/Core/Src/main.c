/**
  ******************************************************************************
  * @file    Core/Src/main.c
  * @author  Origin's assistant (Hanako)
  * @brief   FC_H743 飞控主程序（LL 库版）- STM32H743VIT6
  * @note    初始化配置与 FC_H743_LL.ioc 一致，引脚映射见 main.h 头部说明。
  *          本工程为 LL 库工程，SDMMC1 因官方无 LL 驱动封装按标准混合模式走 HAL。
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32h7xx_hal_rcc.h"   /* SDMMC 时钟宏（HAL 混合） */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/* SDMMC（HAL 混合，官方 LL 工程中 SDMMC 无 LL 封装） */
SD_HandleTypeDef hsd1;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_SPI1_Init(void);
static void MX_SDMMC1_SD_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM4_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_UART4_Init(void);
static void CPU_CACHE_Enable(void);
void Error_Handler(void);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  The application entry point.
  */
int main(void)
{
  /* 开启 I/D-Cache（代码在 AXI SRAM/片内执行无碍） */
  CPU_CACHE_Enable();

  /* 480MHz 系统时钟（HSE 25MHz 晶振） */
  SystemClock_Config();

  /* 外设初始化 */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_SPI1_Init();
  MX_SDMMC1_SD_Init();
  MX_TIM1_Init();
  MX_TIM4_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  MX_UART4_Init();

  /* 用户应用代码区开始 */

  /* 用户应用代码区结束 */

  /* Infinite loop */
  while (1)
  {
  }
}

/**
  * @brief  系统时钟配置：HSE 25MHz -> SYSCLK 480MHz
  *         与 ioc RCC 段一致：
  *           PLL1: M=5, N=192, P=2 (VCO 960MHz, SYSCLK 480MHz), Q=8 (SDMMC 120MHz), R=2
  *           PLL2: M=5, N=96,  P=2 (ADC 源 240MHz), Q=2, R=10 (48MHz 预留)
  *           AHB=DIV1 (480MHz), APB1=DIV2 (240MHz), APB2=DIV2 (240MHz), APB4=DIV2 (240MHz)
  *           Flash latency = 4WS @ 480MHz VOS1（官方 PWR_VOS0_480MHZ 例程同值）
  */
void SystemClock_Config(void)
{
  /* 电压等级 VOS1（ioc: PWR_REGULATOR_VOLTAGE_SCALE1） */
  LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
  while (LL_PWR_IsActiveFlag_VOS() == 0)
  {
  }

  /* 使能 HSE 25MHz 晶振（非 bypass） */
  LL_RCC_HSE_Enable();
  while (LL_RCC_HSE_IsReady() != 1)
  {
  }

  /* Flash 等待周期 4WS @ 480MHz */
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_4);

  /* PLL1 = SYSCLK 主 PLL */
  LL_RCC_PLL_SetSource(LL_RCC_PLLSOURCE_HSE);
  LL_RCC_PLL1P_Enable();
  LL_RCC_PLL1Q_Enable();
  LL_RCC_PLL1R_Enable();
  LL_RCC_PLL1FRACN_Disable();
  LL_RCC_PLL1_SetVCOInputRange(LL_RCC_PLLINPUTRANGE_4_8);   /* 25MHz/5 = 5MHz */
  LL_RCC_PLL1_SetVCOOutputRange(LL_RCC_PLLVCORANGE_WIDE);   /* VCO = 960MHz */
  LL_RCC_PLL1_SetM(5);
  LL_RCC_PLL1_SetN(192);
  LL_RCC_PLL1_SetP(2);
  LL_RCC_PLL1_SetQ(8);
  LL_RCC_PLL1_SetR(2);
  LL_RCC_PLL1_Enable();
  while (LL_RCC_PLL1_IsReady() != 1)
  {
  }

  /* PLL2 = ADC 时钟源（PLL2P = 240MHz） */
  LL_RCC_PLL2_SetVCOInputRange(LL_RCC_PLLINPUTRANGE_4_8);   /* 25MHz/5 = 5MHz */
  LL_RCC_PLL2_SetVCOOutputRange(LL_RCC_PLLVCORANGE_WIDE);   /* VCO = 480MHz */
  LL_RCC_PLL2_SetM(5);
  LL_RCC_PLL2_SetN(96);
  LL_RCC_PLL2_SetP(2);
  LL_RCC_PLL2_SetQ(2);
  LL_RCC_PLL2_SetR(10);
  LL_RCC_PLL2P_Enable();
  LL_RCC_PLL2_Enable();
  while (LL_RCC_PLL2_IsReady() != 1)
  {
  }

  /* 总线分频：D1CPRE=1, AHB=1, APB1/2/4=2 */
  LL_RCC_SetSysPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAHBPrescaler(LL_RCC_AHB_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2);
  LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);
  LL_RCC_SetAPB4Prescaler(LL_RCC_APB4_DIV_2);

  /* 外设时钟源（与 ioc 一致）：
   *   ADC    = PLL2P (240MHz)
   *   SDMMC  = PLL1Q (120MHz)
   *   SPI123 = PLL1Q (120MHz, ioc RCC_SPI123CLKSOURCE_PLL, 复位默认即 PLL1Q)
   *   I2C123 = PCLK1 (240MHz, D2PCLK1, 复位默认)
   */
  LL_RCC_SetADCClockSource(LL_RCC_ADC_CLKSOURCE_PLL2P);
  LL_RCC_SetSDMMCClockSource(LL_RCC_SDMMC_CLKSOURCE_PLL1Q);

  /* 切换系统时钟到 PLL1 */
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL1);
  while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL1)
  {
  }

  /* SysTick 1ms（480MHz），优先级 15（ioc NVIC.SysTick=true:15:0） */
  LL_Init1msTick(480000000);
  NVIC_SetPriority(SysTick_IRQn, 15);

  /* 更新 CMSIS 时钟变量 */
  SystemCoreClock = 480000000;
}

/**
  * @brief  GPIO 初始化（LL）
  * 全部引脚按 ioc 分配，见 main.h 头部说明。
  */
static void MX_GPIO_Init(void)
{
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* --- 时钟使能 --- */
  LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOA |
                           LL_AHB4_GRP1_PERIPH_GPIOB |
                           LL_AHB4_GRP1_PERIPH_GPIOC |
                           LL_AHB4_GRP1_PERIPH_GPIOD |
                           LL_AHB4_GRP1_PERIPH_GPIOE);

  /* --- 模拟输入 --- */
  /* PA0 = ADC1_IN16, PA1 = ADC1_IN17 */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_0 | LL_GPIO_PIN_1;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* --- 普通输入 --- */
  /* PA2 = MAG_DRDY */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_2;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PA8 = TF_CD（TF 卡检测） */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_8;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PC5 = IMU_INT1（EXTI5，下降沿触发；GPIO 层按 H7 规范只配输入，EXTI 由 LL_EXTI 配置） */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_5;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* --- 输出 --- */
  /* PC4 = IMU_CS（初始拉高，片选低有效） */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_4;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_4);

  /* PC7 = BUZZER（初始低） */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_7;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_7);

  /* PE2 = WS2812_DIN（初始低） */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_2;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOE, &GPIO_InitStruct);
  LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_2);

  /* --- AF 复用 --- */
  /* PA5/6/7 = SPI1 SCK/MISO/MOSI (AF5) */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_5 | LL_GPIO_PIN_6 | LL_GPIO_PIN_7;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_5;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PA9/PA10 = USART1 TX/RX (AF7) */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_9 | LL_GPIO_PIN_10;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_7;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PA11/PA12 = UART4 RX/TX (AF8) */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_11 | LL_GPIO_PIN_12;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_8;
  LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* PB6/PB7 = I2C1 SCL/SDA (AF4, 开漏) */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_6 | LL_GPIO_PIN_7;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_4;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* PB10/PB11 = I2C2 SCL/SDA (AF4, 开漏) */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_10 | LL_GPIO_PIN_11;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_4;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* PC8..PC11 = SDMMC1 D0-D3, PC12 = CK（AF12, 高速） */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_8 | LL_GPIO_PIN_9 | LL_GPIO_PIN_10 |
                        LL_GPIO_PIN_11 | LL_GPIO_PIN_12;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_12;
  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* PD2 = SDMMC1 CMD (AF12) */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_2;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_12;
  LL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* PD8/PD9 = USART3 TX/RX (AF7) */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_8 | LL_GPIO_PIN_9;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_7;
  LL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* PD12..PD15 = TIM4 CH1-CH4 (AF2) */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_12 | LL_GPIO_PIN_13 |
                        LL_GPIO_PIN_14 | LL_GPIO_PIN_15;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_2;
  LL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* PE9/PE11/PE13/PE14 = TIM1 CH1-CH4 (AF1) */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_9 | LL_GPIO_PIN_11 |
                        LL_GPIO_PIN_13 | LL_GPIO_PIN_14;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_1;
  LL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /* --- EXTI5 (PC5 IMU_INT1) --- */
  {
    LL_EXTI_InitTypeDef EXTI_InitStruct = {0};
    EXTI_InitStruct.Line_0_31 = LL_EXTI_LINE_5;
    EXTI_InitStruct.Mode = LL_EXTI_MODE_IT;
    EXTI_InitStruct.Trigger = LL_EXTI_TRIGGER_FALLING;  /* IMU 中断线：下降沿（开漏输出），按硬件可改 */
    LL_EXTI_Init(&EXTI_InitStruct);
  }
  NVIC_SetPriority(EXTI9_5_IRQn, 5);
  NVIC_EnableIRQ(EXTI9_5_IRQn);
}

/**
  * @brief  ADC1 初始化（LL）：PA0=IN16, PA1=IN17, 序列长度 2
  *         ADC 时钟 = PLL2P 240MHz / 8 = 30MHz（数据手册 36MHz 上限内）
  */
static void MX_ADC1_Init(void)
{
  LL_ADC_CommonInitTypeDef ADC_CommonInitStruct = {0};
  LL_ADC_InitTypeDef ADC_InitStruct = {0};
  LL_ADC_REG_InitTypeDef ADC_REG_InitStruct = {0};

  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_ADC12);

  ADC_CommonInitStruct.CommonClock = LL_ADC_CLOCK_ASYNC_DIV8;
  ADC_CommonInitStruct.Multimode = LL_ADC_MULTI_INDEPENDENT;
  ADC_CommonInitStruct.MultiDMATransfer = LL_ADC_MULTI_REG_DMA_EACH_ADC;
  ADC_CommonInitStruct.MultiTwoSamplingDelay = 0;  /* 5 cycles，默认 */
  LL_ADC_CommonInit(ADC12_COMMON, &ADC_CommonInitStruct);

  ADC_InitStruct.Resolution = LL_ADC_RESOLUTION_12B;
  ADC_InitStruct.LowPowerMode = LL_ADC_LP_MODE_NONE;
  ADC_InitStruct.LeftBitShift = 0;  /* 不左移 */
  LL_ADC_Init(ADC1, &ADC_InitStruct);

  ADC_REG_InitStruct.TriggerSource = LL_ADC_REG_TRIG_SOFTWARE;
  ADC_REG_InitStruct.SequencerLength = LL_ADC_REG_SEQ_SCAN_ENABLE_2RANKS;
  ADC_REG_InitStruct.SequencerDiscont = LL_ADC_REG_SEQ_DISCONT_DISABLE;
  ADC_REG_InitStruct.ContinuousMode = 0;  /* 单次转换 */
  ADC_REG_InitStruct.DataTransferMode = 0;  /* 无 DMA 传输（DMNGT=00） */
  ADC_REG_InitStruct.Overrun = LL_ADC_REG_OVR_DATA_OVERWRITTEN;
  LL_ADC_REG_Init(ADC1, &ADC_REG_InitStruct);

  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_1, LL_ADC_CHANNEL_16);
  LL_ADC_REG_SetSequencerRanks(ADC1, LL_ADC_REG_RANK_2, LL_ADC_CHANNEL_17);

  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_16, LL_ADC_SAMPLINGTIME_64CYCLES_5);
  LL_ADC_SetChannelSamplingTime(ADC1, LL_ADC_CHANNEL_17, LL_ADC_SAMPLINGTIME_64CYCLES_5);

  LL_ADC_Enable(ADC1);
}

/**
  * @brief  I2C1 初始化（LL）：PB6 SCL, PB7 SDA, 400kHz @ 240MHz
  */
static void MX_I2C1_Init(void)
{
  LL_I2C_InitTypeDef I2C_InitStruct = {0};

  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1);

  I2C_InitStruct.PeripheralMode = LL_I2C_MODE_I2C;
  I2C_InitStruct.Timing = __LL_I2C_CONVERT_TIMINGS(5, 17, 15, 47, 51); /* 400kHz: PRESC=5, SCLDEL=17, SDADEL=15, SCLH=47, SCLL=51 */
  I2C_InitStruct.AnalogFilter = LL_I2C_ANALOGFILTER_ENABLE;
  I2C_InitStruct.DigitalFilter = 0;
  I2C_InitStruct.OwnAddress1 = 0;
  I2C_InitStruct.TypeAcknowledge = LL_I2C_ACK;
  I2C_InitStruct.OwnAddrSize = LL_I2C_OWNADDRESS1_7BIT;
  LL_I2C_Init(I2C1, &I2C_InitStruct);

  LL_I2C_Enable(I2C1);
}

/**
  * @brief  I2C2 初始化（LL）：PB10 SCL, PB11 SDA, 400kHz @ 240MHz
  */
static void MX_I2C2_Init(void)
{
  LL_I2C_InitTypeDef I2C_InitStruct = {0};

  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C2);

  I2C_InitStruct.PeripheralMode = LL_I2C_MODE_I2C;
  I2C_InitStruct.Timing = __LL_I2C_CONVERT_TIMINGS(5, 17, 15, 47, 51);
  I2C_InitStruct.AnalogFilter = LL_I2C_ANALOGFILTER_ENABLE;
  I2C_InitStruct.DigitalFilter = 0;
  I2C_InitStruct.OwnAddress1 = 0;
  I2C_InitStruct.TypeAcknowledge = LL_I2C_ACK;
  I2C_InitStruct.OwnAddrSize = LL_I2C_OWNADDRESS1_7BIT;
  LL_I2C_Init(I2C2, &I2C_InitStruct);

  LL_I2C_Enable(I2C2);
}

/**
  * @brief  SPI1 初始化（LL）：PA5 SCK / PA6 MISO / PA7 MOSI
  *         主模式, 8bit, CPOL=0/CPHA=0, 软件 NSS（片选 PC4）
  *         时钟 = PLL1Q 120MHz / 16 = 7.5MHz
  */
static void MX_SPI1_Init(void)
{
  LL_SPI_InitTypeDef SPI_InitStruct = {0};

  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);

  SPI_InitStruct.TransferDirection = LL_SPI_FULL_DUPLEX;
  SPI_InitStruct.Mode = LL_SPI_MODE_MASTER;
  SPI_InitStruct.DataWidth = LL_SPI_DATAWIDTH_8BIT;
  SPI_InitStruct.ClockPolarity = LL_SPI_POLARITY_LOW;
  SPI_InitStruct.ClockPhase = LL_SPI_PHASE_1EDGE;
  SPI_InitStruct.NSS = LL_SPI_NSS_SOFT;
  SPI_InitStruct.BaudRate = SPI_BAUDRATEPRESCALER_16;
  SPI_InitStruct.BitOrder = LL_SPI_MSB_FIRST;
  LL_SPI_Init(SPI1, &SPI_InitStruct);

  LL_SPI_Enable(SPI1);
}

/**
  * @brief  SDMMC1 初始化（HAL 混合）：TF 卡, 4bit, ClockDiv=4 (120MHz/(4+2)=20MHz)
  * @note   H7 官方无 SDMMC LL 封装（ll_sdmmc 仅低层寄存函数与结构体），
  *         按官方 LL 工程的标准做法走 HAL 混合。GPIO 由 MX_GPIO_Init 的 LL 完成。
  */
static void MX_SDMMC1_SD_Init(void)
{
  hsd1.Instance = SDMMC1;
  hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
  hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
  hsd1.Init.BusWide = SDMMC_BUS_WIDE_4B;
  hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
  hsd1.Init.ClockDiv = 4;
  if (HAL_SD_Init(&hsd1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  SDMMC1 MSP 初始化（HAL 弱函数重定义）：此处只需开外设时钟，
  *         GPIO 已由 LL 配置完毕，不重复配置。
  */
void HAL_SD_MspInit(SD_HandleTypeDef *hsd)
{
  if (hsd->Instance == SDMMC1)
  {
    LL_AHB3_GRP1_EnableClock(LL_AHB3_GRP1_PERIPH_SDMMC1);
  }
}

/**
  * @brief  TIM1 初始化（LL）：PE9/PE11/PE13/PE14 PWM CH1-CH4
  *         时钟 = APB2(240MHz)x2 = 480MHz, PSC=600 -> ~799kHz, ARR=1999 -> ~399Hz
  *         初始占空比 1000/2000 = 50%
  */
static void MX_TIM1_Init(void)
{
  LL_TIM_InitTypeDef TIM_InitStruct = {0};
  LL_TIM_OC_InitTypeDef TIM_OC_InitStruct = {0};

  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1);

  TIM_InitStruct.Prescaler = 600;
  TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
  TIM_InitStruct.Autoreload = 1999;
  TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  TIM_InitStruct.RepetitionCounter = 0;
  LL_TIM_Init(TIM1, &TIM_InitStruct);

  TIM_OC_InitStruct.OCMode = LL_TIM_OCMODE_PWM1;
  TIM_OC_InitStruct.OCState = LL_TIM_OCSTATE_DISABLE;
  TIM_OC_InitStruct.OCPolarity = LL_TIM_OCPOLARITY_HIGH;
  TIM_OC_InitStruct.CompareValue = 1000;
  LL_TIM_OC_Init(TIM1, LL_TIM_CHANNEL_CH1, &TIM_OC_InitStruct);
  LL_TIM_OC_Init(TIM1, LL_TIM_CHANNEL_CH2, &TIM_OC_InitStruct);
  LL_TIM_OC_Init(TIM1, LL_TIM_CHANNEL_CH3, &TIM_OC_InitStruct);
  LL_TIM_OC_Init(TIM1, LL_TIM_CHANNEL_CH4, &TIM_OC_InitStruct);

  LL_TIM_OC_EnablePreload(TIM1, LL_TIM_CHANNEL_CH1);
  LL_TIM_OC_EnablePreload(TIM1, LL_TIM_CHANNEL_CH2);
  LL_TIM_OC_EnablePreload(TIM1, LL_TIM_CHANNEL_CH3);
  LL_TIM_OC_EnablePreload(TIM1, LL_TIM_CHANNEL_CH4);
  LL_TIM_CC_EnableChannel(TIM1, LL_TIM_CHANNEL_CH1 | LL_TIM_CHANNEL_CH2 |
                                 LL_TIM_CHANNEL_CH3 | LL_TIM_CHANNEL_CH4);

  /* 高级定时器：使能主输出 */
  LL_TIM_EnableAllOutputs(TIM1);

  LL_TIM_EnableCounter(TIM1);
}

/**
  * @brief  TIM4 初始化（LL）：PD12..PD15 PWM CH1-CH4
  *         时钟 = APB1(240MHz)x2 = 480MHz, PSC=600, ARR=1999 -> ~399Hz
  */
static void MX_TIM4_Init(void)
{
  LL_TIM_InitTypeDef TIM_InitStruct = {0};
  LL_TIM_OC_InitTypeDef TIM_OC_InitStruct = {0};

  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM4);

  TIM_InitStruct.Prescaler = 600;
  TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
  TIM_InitStruct.Autoreload = 1999;
  TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
  TIM_InitStruct.RepetitionCounter = 0;
  LL_TIM_Init(TIM4, &TIM_InitStruct);

  TIM_OC_InitStruct.OCMode = LL_TIM_OCMODE_PWM1;
  TIM_OC_InitStruct.OCState = LL_TIM_OCSTATE_DISABLE;
  TIM_OC_InitStruct.OCPolarity = LL_TIM_OCPOLARITY_HIGH;
  TIM_OC_InitStruct.CompareValue = 1000;
  LL_TIM_OC_Init(TIM4, LL_TIM_CHANNEL_CH1, &TIM_OC_InitStruct);
  LL_TIM_OC_Init(TIM4, LL_TIM_CHANNEL_CH2, &TIM_OC_InitStruct);
  LL_TIM_OC_Init(TIM4, LL_TIM_CHANNEL_CH3, &TIM_OC_InitStruct);
  LL_TIM_OC_Init(TIM4, LL_TIM_CHANNEL_CH4, &TIM_OC_InitStruct);

  LL_TIM_OC_EnablePreload(TIM4, LL_TIM_CHANNEL_CH1);
  LL_TIM_OC_EnablePreload(TIM4, LL_TIM_CHANNEL_CH2);
  LL_TIM_OC_EnablePreload(TIM4, LL_TIM_CHANNEL_CH3);
  LL_TIM_OC_EnablePreload(TIM4, LL_TIM_CHANNEL_CH4);
  LL_TIM_CC_EnableChannel(TIM4, LL_TIM_CHANNEL_CH1 | LL_TIM_CHANNEL_CH2 |
                                 LL_TIM_CHANNEL_CH3 | LL_TIM_CHANNEL_CH4);

  LL_TIM_EnableCounter(TIM4);
}

/**
  * @brief  USART1 初始化（LL）：PA9 TX, PA10 RX, 115200 @ 240MHz (PCLK2)
  */
static void MX_USART1_UART_Init(void)
{
  LL_USART_InitTypeDef USART_InitStruct = {0};

  LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);

  USART_InitStruct.PrescalerValue = LL_USART_PRESCALER_DIV1;
  USART_InitStruct.BaudRate = 115200;
  USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
  USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
  USART_InitStruct.Parity = LL_USART_PARITY_NONE;
  USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
  USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
  LL_USART_Init(USART1, &USART_InitStruct);

  LL_USART_Enable(USART1);

  /* 回环测试：使能 RXNE 中断，收到即原样回传（echo） */
  LL_USART_EnableIT_RXNE(USART1);
  NVIC_SetPriority(USART1_IRQn, 5);
  NVIC_EnableIRQ(USART1_IRQn);
}

/**
  * @brief  USART3 初始化（LL）：PD8 TX, PD9 RX, 57600 @ 240MHz (PCLK1)
  */
static void MX_USART3_UART_Init(void)
{
  LL_USART_InitTypeDef USART_InitStruct = {0};

  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART3);

  USART_InitStruct.PrescalerValue = LL_USART_PRESCALER_DIV1;
  USART_InitStruct.BaudRate = 57600;
  USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
  USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
  USART_InitStruct.Parity = LL_USART_PARITY_NONE;
  USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
  USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
  LL_USART_Init(USART3, &USART_InitStruct);

  LL_USART_Enable(USART3);

  /* 回环测试：使能 RXNE 中断，收到即原样回传（echo） */
  LL_USART_EnableIT_RXNE(USART3);
  NVIC_SetPriority(USART3_IRQn, 5);
  NVIC_EnableIRQ(USART3_IRQn);
}

/**
  * @brief  UART4 初始化（LL）：PA12 TX, PA11 RX, 57600 @ 240MHz (PCLK1)
  */
static void MX_UART4_Init(void)
{
  LL_USART_InitTypeDef USART_InitStruct = {0};

  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART4);

  USART_InitStruct.PrescalerValue = LL_USART_PRESCALER_DIV1;
  USART_InitStruct.BaudRate = 57600;
  USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
  USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
  USART_InitStruct.Parity = LL_USART_PARITY_NONE;
  USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
  USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
  LL_USART_Init(UART4, &USART_InitStruct);

  LL_USART_Enable(UART4);

  /* 回环测试：使能 RXNE 中断，收到即原样回传（echo） */
  LL_USART_EnableIT_RXNE(UART4);
  NVIC_SetPriority(UART4_IRQn, 5);
  NVIC_EnableIRQ(UART4_IRQn);
}

/**
  * @brief  CPU L1 Cache 使能
  */
static void CPU_CACHE_Enable(void)
{
  SCB_EnableICache();
  SCB_EnableDCache();
}

/**
  * @brief  错误处理
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* 用户可以在此打印文件与行号，例如 printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  (void)file;
  (void)line;
  while (1)
  {
  }
}
#endif /* USE_FULL_ASSERT */

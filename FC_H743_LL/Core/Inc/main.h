/**
  ******************************************************************************
  * @file    Core/Inc/main.h
  * @brief   Header for main.c module - FC_H743_LL (STM32H743VIT6 飞控, LL 库工程)
  * @note    硬件连接参照 FC_H743.ioc / hw_ref/h743vitx_pins.json
  ******************************************************************************
  * 引脚分配总览（与 ioc 一致）：
  *   PA0/PA1   : ADC1_IN16 / ADC1_IN17（电压/电流采样）
  *   PA2       : MAG_DRDY (GPIO 输入)
  *   PA5/6/7   : SPI1 SCK / MISO / MOSI（IMU，NSS 走 PC4 软件片选）
  *   PA8       : TF_CD (GPIO 输入, SD 卡检测)
  *   PA9/PA10  : USART1 TX/RX @115200（数传/调试）
  *   PA11/PA12 : UART4 RX/TX @57600（GPS）
  *   PB6/PB7   : I2C1 SCL/SDA（磁力计等）
  *   PB10/PB11 : I2C2 SCL/SDA（气压计等）
  *   PC4       : IMU_CS (GPIO 输出, 初始高)
  *   PC5       : IMU_INT1 (GPIO 输入 + EXTI5 中断)
  *   PC7       : BUZZER (GPIO 输出)
  *   PC8..PC12,PD2 : SDMMC1 D0-D3/CK/CMD（TF 卡, 4bit, ClockDiv=4）
  *   PD8/PD9   : USART3 TX/RX @57600（数传2）
  *   PD12..15  : TIM4 CH1-CH4 PWM (PSC=600, ARR=1999)
  *   PE2       : WS2812_DIN (GPIO 输出)
  *   PE9/11/13/14 : TIM1 CH1-CH4 PWM (PSC=600, ARR=1999)
  *   PH0/PH1   : HSE 25MHz 晶振
  ******************************************************************************
  */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* HAL 基础（SDMMC 走 HAL 混合，官方 LL 工程对无 LL 封装外设的标准做法）
 * 注意：定义 USE_HAL_DRIVER 时 stm32h7xx.h 会自动引入 hal.h，
 *       因此 hal.h 必须放在最前（与 CubeMX 生成工程一致）。 */
#include "stm32h7xx_hal.h"

/* LL 驱动 */
#include "stm32h7xx_ll_adc.h"
#include "stm32h7xx_ll_bus.h"
#include "stm32h7xx_ll_cortex.h"
#include "stm32h7xx_ll_exti.h"
#include "stm32h7xx_ll_gpio.h"
#include "stm32h7xx_ll_i2c.h"
#include "stm32h7xx_ll_pwr.h"
#include "stm32h7xx_ll_rcc.h"
#include "stm32h7xx_ll_sdmmc.h"
#include "stm32h7xx_ll_spi.h"
#include "stm32h7xx_ll_system.h"
#include "stm32h7xx_ll_tim.h"
#include "stm32h7xx_ll_usart.h"
#include "stm32h7xx_ll_utils.h"

/* SDMMC 使用 HAL 驱动 */
#include "stm32h7xx_hal_sd.h"

/* 外部变量 */
extern SD_HandleTypeDef hsd1;

void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/**
  ******************************************************************************
  * @file    fc/fc_board.h
  * @brief   FC_H743 飞控板级定义（STM32H743VIT6 自研板 / 电赛四轴）
  * @note    本头文件是板级"事实来源"。
  *          引脚分配与 CubeMX(.ioc) 一致，传感器型号来自板级 BOM/原理图网表：
  *            IMU    = ICM-42670-P  (SPI1, CS=PC4, INT=PC5)
  *            磁力计  = IST8310      (I2C1, SCL=PB6, SDA=PB7, DRDY=PA2)
  *            气压计  = SPA06-003    (I2C2, SCL=PB10, SDA=PB11)
  *            电压/电流 = ADC1 (PA0=电压, PA1=电流)
  *            SD卡   = SDMMC1 4bit, 检测=PA8
  *            遥控   = SBUS (UART4 RX 反相)
  *            数传   = USART3 (PD8/PD9 @57600)
  *            USB    = USART1 + CH340N + Type-C
  *            电机   = TIM1 CH1-4 (PE9/11/13/14) = M1..M4
  *            舵机/备用 = TIM4 CH1-4 (PD12-15)
  *            蜂鸣器 = PC7 ; WS2812 = PE2
  ******************************************************************************
  */
#ifndef FC_BOARD_H
#define FC_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"   /* HAL, 引脚宏 IMU_CS/MAG_DRDY/... 已由 CubeMX 定义 */

/* ============================ 板级版本 ============================ */
#define FC_BOARD_REV       "1.0"
#define FC_BOARD_NAME      "FC_H743"

/* ============================ 核心时钟 ============================ */
#define FC_SYSCLK_HZ       480000000U
#define FC_CONTROL_HZ      1000U          /* 姿态控制环 1kHz */
#define FC_ALL_TIME_HZ     1000000U

/* ============================ 传感器 ============================ */
/* 已知型号（非自动识别路径） */
#define FC_IMU_TYPE_ICM42670   1
#define FC_MAG_TYPE_IST8310    1
#define FC_BARO_TYPE_SPA06     1

/* 传感器 I2C 地址 / SPI 标识 */
#define BARO_SPA06_I2C_ADDR    (0x76 << 1)   /* 0x38 8bit 写地址 */
#define MAG_IST8310_I2C_ADDR   (0x0E << 1)   /* 0x1C */
#define IMU_ICM42670_WHO_AM_I  0x67

/* ============================ 电机 / 输出 ============================ */
/* M1..M4 -> TIM1 CH1..CH4 (PE9,PE11,PE13,PE14) */
#define FC_MOTOR_TIMER          (&htim1)
/* 伺服/备用输出 -> TIM4 (PD12..15) */
#define FC_SERVO_TIMER          (&htim4)

/* PWM 输出规范（模拟 PWM 电调）: 400Hz, 1000~2000us */
#define FC_PWM_FREQ_HZ          400U
#define FC_PWM_MIN_US           1000U
#define FC_PWM_MID_US           1500U
#define FC_PWM_MAX_US           2000U

/* ============================ ADC / 电源 ============================ */
#define FC_VOLTAGE_ADC_CH       16    /* PA0 */
#define FC_CURRENT_ADC_CH       17    /* PA1 */
/* 分压比 & 采样电阻（按原理图 R17/R18=100K 与 R19=1K，等比 10.1:1；现场标定后修正） */
#define FC_VOLTAGE_DIVIDER      10.1f
#define FC_CURRENT_SENSOR_RS    0.001f   /* 采样电阻 1mOhm, 待标定 */
#define FC_CURRENT_GAIN         1.0f

/* ============================ 控制参数默认值 ============================ */
#define FC_PID_RATE_P   0.02f
#define FC_PID_RATE_I   0.000f
#define FC_PID_RATE_D   0.000f

/* ============================ HAL 句柄（CubeMX 提供） ============================ */
extern SPI_HandleTypeDef hspi1;
extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim4;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart4;
extern SD_HandleTypeDef hsd1;

/* ============================ 板上外设辅助接口 ============================ */
/* WS2812 (PE2) */
#define FC_LED_WS2812_ON()      HAL_GPIO_WritePin(WS2812_DIN_GPIO_Port, WS2812_DIN_Pin, GPIO_PIN_SET)
#define FC_LED_WS2812_OFF()     HAL_GPIO_WritePin(WS2812_DIN_GPIO_Port, WS2812_DIN_Pin, GPIO_PIN_RESET)
/* 蜂鸣器 (PC7) */
#define FC_BUZZER_ON()          HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET)
#define FC_BUZZER_OFF()         HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET)
/* IMU 片选 (PC4, 低有效) */
#define FC_IMU_CS_LOW()         HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET)
#define FC_IMU_CS_HIGH()        HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET)
/* IMU 数据就绪 (PC5) */
#define FC_IMU_INT_READ()       HAL_GPIO_ReadPin(IMU_INT1_GPIO_Port, IMU_INT1_Pin)
/* 磁力计 DRDY (PA2) */
#define FC_MAG_DRDY_READ()      HAL_GPIO_ReadPin(MAG_DRDY_GPIO_Port, MAG_DRDY_Pin)
/* SD 卡检测 (PA8) */
#define FC_TF_PRESENT()         (HAL_GPIO_ReadPin(TF_CD_GPIO_Port, TF_CD_Pin) == GPIO_PIN_RESET)

/* 板级初始化（由 fc_board.c 实现）：开外设、初始化 PWM、ADC 等 */
void FC_Board_Init(void);

/* 输出设置（混控/控制层调用） */
void FC_PWM_SetMotor(uint8_t idx, uint16_t us);
void FC_PWM_SetServo(uint8_t idx, uint16_t us);

/* 细分初始化（已在 FC_Board_Init 内调用，可按需单独调用） */
void FC_Board_EnableCache(void);
void FC_Board_FixAdcClock(void);
void FC_Board_PwmInit(void);
void FC_Board_FixUart4Baud(void);

#ifdef __cplusplus
}
#endif

#endif /* FC_BOARD_H */

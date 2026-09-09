/**
  ******************************************************************************
  * @file    fc/fc_power.h
  * @brief   电源采样（ADC1 双通道扫描: IN16=电压 PA0, IN17=电流 PA1）
  * @note    换算比率为占位初值，需按实际分压/采样电阻校准。
  ******************************************************************************
  */
#ifndef FC_POWER_H
#define FC_POWER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fc_types.h"
#include <stdbool.h>

/* ---- 校准参数（按硬件调整） ---- */
/* 电压：V_bat = V_adc * FC_POWER_VOLTAGE_RATIO
   默认假设 1:4 分压（例如 10k+30k），满电 3S(12.6V) -> ADC 3.15V */
#define FC_POWER_VOLTAGE_RATIO   4.0f

/* 电流：I = V_adc * FC_POWER_CURRENT_SCALE + FC_POWER_CURRENT_OFFSET
   默认假设 0.1 V/A（如 ACS758-50B 40mV/A 级别的放大输出），需实测校准 */
#define FC_POWER_CURRENT_SCALE   10.0f
#define FC_POWER_CURRENT_OFFSET  0.0f

/* ADC 参考（CubeMX 默认 VREF+ = VDD = 3.3V） */
#define FC_ADC_VREF              3.3f
#define FC_ADC_FULL_SCALE        65535.0f   /* 16bit */

/* 初始化（校准 ADC） */
void FC_Power_Init(void);

/* 采样一次电压/电流（V / A），成功返回 true */
bool FC_Power_Read(fc_power_t *pw);

#ifdef __cplusplus
}
#endif

#endif /* FC_POWER_H */

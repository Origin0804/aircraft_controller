/**
  ******************************************************************************
  * @file    fc/fc_baro.h
  * @brief   气压计 SPA06-003 驱动（I2C2, 7位地址 0x76）
  * @note    寄存器按 SPL06 家族兼容布局（SPL06-007 同款布局）。
  ******************************************************************************
  */
#ifndef FC_BARO_H
#define FC_BARO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fc_types.h"
#include <stdbool.h>

/* 初始化并自检，失败返回 false */
bool FC_Baro_Init(void);

/* 读取一组数据：pressure(Pa), temperature(degC)。返回 false 表示数据无效 */
bool FC_Baro_Read(fc_baro_t *baro);

#ifdef __cplusplus
}
#endif

#endif /* FC_BARO_H */

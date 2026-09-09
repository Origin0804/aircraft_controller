/**
  ******************************************************************************
  * @file    fc/fc_mag.h
  * @brief   磁力计 IST8310 驱动（I2C1, 7位地址 0x0E, DRDY=PA2 可选）
  ******************************************************************************
  */
#ifndef FC_MAG_H
#define FC_MAG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fc_types.h"
#include <stdbool.h>

/* 初始化并自检（WHO_AM_I == 0x10），失败返回 false */
bool FC_Mag_Init(void);

/*
 * 触发一次单次测量并读取（单位 mG）。
 * IST8310 为单次测量模式：每次读取后需重新触发，故内部维护状态机。
 * 返回 true 表示得到一组新数据。
 */
bool FC_Mag_Read(fc_mag_t *mag);

#ifdef __cplusplus
}
#endif

#endif /* FC_MAG_H */

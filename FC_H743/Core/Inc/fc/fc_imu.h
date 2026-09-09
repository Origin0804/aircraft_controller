/**
  ******************************************************************************
  * @file    fc/fc_imu.h
  * @brief   ICM-42670-P 六轴 IMU 驱动（SPI1, CS=PC4, 软件片选）
  ******************************************************************************
  */
#ifndef FC_IMU_H
#define FC_IMU_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fc_types.h"
#include <stdbool.h>

/* 初始化并自检（读 WHO_AM_I，配置量程/ODR/低噪模式） */
bool FC_IMU_Init(void);

/* 读取一组数据（加速度 m/s^2, 陀螺 deg/s, 温度 degC） */
bool FC_IMU_Read(fc_imu_t *imu);

/* 读取芯片 ID（0x67 = ICM-42670-P） */
uint8_t FC_IMU_WhoAmI(void);

#ifdef __cplusplus
}
#endif

#endif /* FC_IMU_H */

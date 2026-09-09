/**
  ******************************************************************************
  * @file    fc/fc_ahrs.h
  * @brief   AHRS 姿态解算（Mahony 互补滤波，四元数）
  ******************************************************************************
  */
#ifndef FC_AHRS_H
#define FC_AHRS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fc_types.h"

/* 初始化姿态（单位四元数，水平） */
void FC_AHRS_Init(void);

/* 姿态更新：imu 提供加速度(m/s^2)与陀螺(deg/s)，dt 为秒 */
void FC_AHRS_Update(const fc_imu_t *imu, float dt, fc_attitude_t *att);

/* 手动设姿态（用于上电/水平标定） */
void FC_AHRS_SetAttitude(const float *q);

#ifdef __cplusplus
}
#endif

#endif /* FC_AHRS_H */

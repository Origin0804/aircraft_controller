/**
  ******************************************************************************
  * @file    fc/fc_telemetry.h
  * @brief   数传/遥测（USART3 发送状态帧；TX，复用 HAL 阻塞发送）
  ******************************************************************************
  */
#ifndef FC_TELEMETRY_H
#define FC_TELEMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fc_types.h"

void FC_Telemetry_Init(void);

/* 周期状态上报（ASCII 行，便于地面站/GPS 数传调试） */
void FC_Telemetry_Status(const fc_attitude_t *att, const fc_rc_t *rc,
                         const fc_power_t *pw, const fc_baro_t *baro);

#ifdef __cplusplus
}
#endif

#endif /* FC_TELEMETRY_H */

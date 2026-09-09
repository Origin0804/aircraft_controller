/**
  ******************************************************************************
  * @file    fc/fc_control.h
  * @brief   姿态控制（角速度内环 PID + 角度外环 P）
  ******************************************************************************
  */
#ifndef FC_CONTROL_H
#define FC_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fc_types.h"

/* 控制输出：roll/pitch/yaw 为归一化[-1,1]的控制量，throttle 为[0,1] */
typedef struct {
    float roll;
    float pitch;
    float yaw;
    float throttle;
} fc_control_cmd_t;

void FC_Control_Init(void);

/* 根据遥控与姿态计算控制量（内部按 AUX2 判定 Acro/Angle） */
void FC_Control_Update(const fc_attitude_t *att, const fc_rc_t *rc, fc_control_cmd_t *cmd);

#ifdef __cplusplus
}
#endif

#endif /* FC_CONTROL_H */

/**
  ******************************************************************************
  * @file    fc/fc_mixer.h
  * @brief   X 型四轴混控：throttle/roll/pitch/yaw -> 4 路电机 PWM
  ******************************************************************************
  */
#ifndef FC_MIXER_H
#define FC_MIXER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fc_types.h"
#include "fc_control.h"

/* 停止所有电机（写最小油门） */
void FC_Mixer_Stop(void);

/* 由控制量计算并输出电机 PWM（us） */
void FC_Mixer_Apply(const fc_control_cmd_t *cmd, fc_output_t *out);

#ifdef __cplusplus
}
#endif

#endif /* FC_MIXER_H */

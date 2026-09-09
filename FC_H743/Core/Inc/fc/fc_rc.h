/**
  ******************************************************************************
  * @file    fc/fc_rc.h
  * @brief   SBUS 遥控接收解码（UART4 反相, 100000, 8E2）
  ******************************************************************************
  */
#ifndef FC_RC_H
#define FC_RC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fc_types.h"

/* SBUS 通道顺序（前 4 通道 + 2 个开关） */
enum {
    FC_RC_ROLL = 0,
    FC_RC_PITCH,
    FC_RC_THROTTLE,
    FC_RC_YAW,
    FC_RC_ARM,       /* AUX1：>0.75 解锁 */
    FC_RC_MODE,      /* AUX2：飞行模式 */
};

/* 初始化：复位解析状态（UART 配置由 CubeMX 负责） */
void FC_RC_Init(void);

/* 逐字节入口：由 usart.c 的 HAL_UART_RxCpltCallback（UART4 分支）调用 */
void FC_RC_OnByte(uint8_t b);

/* 取最新解码结果 */
void FC_RC_Get(fc_rc_t *rc);

/* 是否已解锁（按 AUX 通道） */
bool FC_RC_Armed(const fc_rc_t *rc);

#ifdef __cplusplus
}
#endif

#endif /* FC_RC_H */

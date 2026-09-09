/**
  ******************************************************************************
  * @file    fc/fc_alt.h
  * @brief   气压计定高（Alt Hold）：高度误差 P + 垂直速度 D -> 油门偏移
  ******************************************************************************
  */
#ifndef FC_ALT_H
#define FC_ALT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fc_types.h"

void FC_Alt_Init(void);

/*
 * @param baro    气压计数据（pressure_pa）
 * @param dt      采样间隔（s）
 * @param enable  是否启用定高
 * @param offset  输出油门偏移量（[-1,1] 往返，用于叠加到 cmd->throttle）
 */
void FC_Alt_Update(const fc_baro_t *baro, float dt, bool enable, float *offset);

#ifdef __cplusplus
}
#endif

#endif /* FC_ALT_H */

/**
  ******************************************************************************
  * @file    fc/fc_sd.h
  * @brief   SD 黑匣子（裸扇区顺序写，无文件系统；16B 记录 × 32/512B 块）
  * @note    数据为固定长度结构体，离线用脚本按字节解析。
  ******************************************************************************
  */
#ifndef FC_SD_H
#define FC_SD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fc_types.h"

/* 16 字节记录（无对齐填充） */
typedef struct {
    uint32_t ts;          /* 毫秒 */
    int16_t  acc[3];      /* m/s^2 * 100 */
    int16_t  gyro[3];     /* deg/s * 10 */
} fc_sd_rec_t;            /* 4 + 6 + 6 = 16 字节 */

/* 初始化 SDMMC 并定位起始扇区；无卡则返回 false */
bool FC_SD_Init(void);

/* 记一条记录（内部缓冲，满 512B 自动刷块）；无卡时静默 */
void FC_SD_Log(const fc_attitude_t *att, const fc_imu_t *imu);

/* 可否记录（卡已就绪） */
bool FC_SD_Ready(void);

#ifdef __cplusplus
}
#endif

#endif /* FC_SD_H */

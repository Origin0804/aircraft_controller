/**
  ******************************************************************************
  * @file    fc/fc_gps.h
  * @brief   GPS NMEA 解析（串口无关：任意 UART 字节流喂入 FC_GPS_OnByte）
  ******************************************************************************
  */
#ifndef FC_GPS_H
#define FC_GPS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fc_types.h"

void FC_GPS_Init(void);

/* 逐字节入口：由 GPS 所在 UART 的回调喂入（串口待定，先留接口） */
void FC_GPS_OnByte(uint8_t b);

/* 取最新解析结果 */
void FC_GPS_Get(fc_gps_t *gps);

#ifdef __cplusplus
}
#endif

#endif /* FC_GPS_H */

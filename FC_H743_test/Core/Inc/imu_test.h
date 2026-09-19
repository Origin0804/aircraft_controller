/**
  ******************************************************************************
  * @file    imu_test.h
  * @brief   ICM-42670-P IMU 数据读取，结果打印到 USART1
  ******************************************************************************
  */
#ifndef IMU_TEST_H
#define IMU_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

/* 入口：读 WHO_AM_I、转储寄存器、配置量程、然后持续打印六轴数据。
 * 内部是死循环，不返回（与 Serial_Only_Test 一致）。 */
void Imu_Stream_Test(void);

#ifdef __cplusplus
}
#endif

#endif /* IMU_TEST_H */

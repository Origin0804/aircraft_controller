/**
  ******************************************************************************
  * @file    fc/fc_imu.c
  * @brief   ICM-42670-P IMU 驱动（SPI1 软件片选 PC4）
  * @note    寄存器表参照 TDK ICM-42670 数据手册：
  *            WHO_AM_I=0x75(0x67), PWR_MGMT0=0x1F, GYRO_CONFIG0=0x20,
  *            ACCEL_CONFIG0=0x21, TEMP_DATA=0x09, ACCEL_DATA=0x0B, GYRO_DATA=0x11
  *          配置：陀螺 ±2000dps(16.4 LSB/dps)、加速度 ±8g(4096 LSB/g)、ODR 1.6kHz、低噪模式
  ******************************************************************************
  */
#include "fc/fc_imu.h"
#include "fc/fc_board.h"
#include "stm32h7xx_hal.h"
#include <string.h>

/* 寄存器 */
#define ICM_WHO_AM_I       0x75
#define ICM_PWR_MGMT0      0x1F
#define ICM_GYRO_CONFIG0   0x20
#define ICM_ACCEL_CONFIG0  0x21
#define ICM_TEMP_DATA      0x09
#define ICM_ACCEL_DATA     0x0B
#define ICM_GYRO_DATA      0x11

#define ICM_DEVICE_ID      0x67

/* 量程灵敏度 */
#define ICM_GYRO_LSB_DPS   16.4f    /* ±2000dps */
#define ICM_ACCEL_LSB_G    4096.0f  /* ±8g */

/* PWR_MGMT0：accel 低噪(3) + gyro 低噪(3<<2) */
#define ICM_PWR_LN         0x0F
/* 陀螺: FS=2000dps(0) | ODR=1.6kHz(5) */
#define ICM_GYRO_CFG       0x05
/* 加速度: FS=8g(1<<5) | ODR=1.6kHz(5) */
#define ICM_ACCEL_CFG      0x25

#define ICM_SPI_TIMEOUT    100U

/* ------------------------------------------------------------------ */
/* SPI 传输基础（CS = PC4 低有效，位 7 = 读标志）                       */
/* ------------------------------------------------------------------ */
static void imu_cs_low(void)  { FC_IMU_CS_LOW(); }
static void imu_cs_high(void) { FC_IMU_CS_HIGH(); }

static bool imu_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { reg, val };
    imu_cs_low();
    HAL_StatusTypeDef st = HAL_SPI_Transmit(&hspi1, tx, 2, ICM_SPI_TIMEOUT);
    imu_cs_high();
    return (st == HAL_OK);
}

static bool imu_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint8_t tx[32];
    uint8_t rx[32];
    if (len + 1 > sizeof(tx)) return false;

    tx[0] = reg | 0x80;                 /* 位 7 = 1 表示读 */
    memset(&tx[1], 0, len);
    imu_cs_low();
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&hspi1, tx, rx, len + 1, ICM_SPI_TIMEOUT);
    imu_cs_high();
    if (st != HAL_OK) return false;
    memcpy(buf, &rx[1], len);
    return true;
}

/* ------------------------------------------------------------------ */
uint8_t FC_IMU_WhoAmI(void)
{
    uint8_t id = 0;
    for (int i = 0; i < 5; ++i) {
        if (imu_read_regs(ICM_WHO_AM_I, &id, 1)) break;
        HAL_Delay(2);
    }
    return id;
}

bool FC_IMU_Init(void)
{
    /* 自检 */
    if (FC_IMU_WhoAmI() != ICM_DEVICE_ID)
    {
        return false;   /* IMU 未识别 */
    }

    /* 低噪模式使能（gyro + accel） */
    if (!imu_write_reg(ICM_PWR_MGMT0, ICM_PWR_LN)) return false;
    HAL_Delay(1);

    /* 量程 + ODR */
    if (!imu_write_reg(ICM_GYRO_CONFIG0, ICM_GYRO_CFG)) return false;
    if (!imu_write_reg(ICM_ACCEL_CONFIG0, ICM_ACCEL_CFG)) return false;
    HAL_Delay(1);

    return true;
}

bool FC_IMU_Read(fc_imu_t *imu)
{
    uint8_t acc[6], gyro[6], tmp[2];
    if (!imu) return false;

    if (!imu_read_regs(ICM_ACCEL_DATA, acc, 6)) return false;
    if (!imu_read_regs(ICM_GYRO_DATA,  gyro, 6)) return false;
    if (!imu_read_regs(ICM_TEMP_DATA,  tmp, 2)) return false;

    /* 大端 int16 */
    int16_t ax = (int16_t)((acc[0] << 8) | acc[1]);
    int16_t ay = (int16_t)((acc[2] << 8) | acc[3]);
    int16_t az = (int16_t)((acc[4] << 8) | acc[5]);
    int16_t gx = (int16_t)((gyro[0] << 8) | gyro[1]);
    int16_t gy = (int16_t)((gyro[2] << 8) | gyro[3]);
    int16_t gz = (int16_t)((gyro[4] << 8) | gyro[5]);
    int16_t tr = (int16_t)((tmp[0] << 8) | tmp[1]);

    /* 加速度 -> m/s^2（先转为 g，再乘 g0） */
    const float g0 = 9.80665f;
    imu->accel[0] = (float)ax / ICM_ACCEL_LSB_G * g0;
    imu->accel[1] = (float)ay / ICM_ACCEL_LSB_G * g0;
    imu->accel[2] = (float)az / ICM_ACCEL_LSB_G * g0;

    /* 陀螺 -> deg/s */
    imu->gyro[0] = (float)gx / ICM_GYRO_LSB_DPS;
    imu->gyro[1] = (float)gy / ICM_GYRO_LSB_DPS;
    imu->gyro[2] = (float)gz / ICM_GYRO_LSB_DPS;

    /* 温度 -> degC */
    imu->temperature = ((float)tr / 128.0f) + 25.0f;

    imu->fresh = true;
    return true;
}

/**
  ******************************************************************************
  * @file    fc/fc_baro.c
  * @brief   SPA06-003 / SPL06-001 气压计驱动（I2C2 轮询）
  * @note    寄存器/校准/补偿参照 Paparazzi UAS spa06.c（Goertek SPA06-003/SPL06-001）。
  *          关键寄存器：PRESSURE(0x00, 3B) TEMPERATURE(0x03, 3B)
  *            PRS_CFG 0x06, TMP_CFG 0x07, MODE_STATUS 0x08, INT_FIFO_CFG 0x09,
  *            RST 0x0C, CHIP_ID 0x0D(SPA06=0x11), COEF 0x10..0x24, COEF_SRCE 0x28。
  ******************************************************************************
  */
#include "fc/fc_baro.h"
#include "fc/fc_board.h"
#include "stm32h7xx_hal.h"

/* 7 位 I2C 地址（SDO 接地 -> 0x76；HAL 用 7 位地址，不左移） */
#define SPL_ADDR            0x76

/* 寄存器 */
#define SPL_REG_PRS_B2      0x00
#define SPL_REG_TMP_B2      0x03
#define SPL_REG_PRS_CFG     0x06
#define SPL_REG_TMP_CFG     0x07
#define SPL_REG_MODE_STAT   0x08
#define SPL_REG_INT_FIFO    0x09
#define SPL_REG_RST         0x0C
#define SPL_REG_CHIP_ID     0x0D
#define SPL_REG_COEF_START  0x10
#define SPL_REG_COEF_SRCE   0x28

#define SPL_CHIP_SPL06      0x10
#define SPL_CHIP_SPA06      0x11

#define SPL_RESET_CMD       0x09
#define SPL_MEAS_CONT       0x07

/* 过采样：压力 64x（高精度），温度 1x */
#define SPL_OS_64X_P        0x06
#define SPL_OS_1X_T         0x00
#define SPL_RATE_4HZ        0x20
#define SPL_PRES_BIT_SHIFT  0x04   /* 过采样 >8x 需位移位 */

/* 对应 scale factors（Paparazzi 表：64x=1040384, 1x=524288） */
#define SCALE_FACTOR_P      1040384.0f
#define SCALE_FACTOR_T      524288.0f

#define SPL_TIMEOUT         20U

static float c0, c1, c00, c10, c01, c11, c20, c21, c30, c31, c40;

static bool spl_write(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(&hi2c2, SPL_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                             &val, 1, SPL_TIMEOUT) == HAL_OK;
}

static bool spl_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c2, SPL_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                            buf, len, SPL_TIMEOUT) == HAL_OK;
}

/* 符号扩展（可变位长 two's complement） */
static int32_t spl_twos(uint32_t raw, uint8_t len)
{
    if (raw & (1U << (len - 1)))
    {
        return (int32_t)raw - ((int32_t)1 << len);
    }
    return (int32_t)raw;
}

static void spl_read_coeffs(bool is_spa06)
{
    uint8_t c[8];

    /* chunk0 @0x10: c0,c1,c00,c10 */
    if (spl_read(SPL_REG_COEF_START, c, 8))
    {
        c0  = (float)spl_twos(((uint32_t)c[0] << 4) | ((c[1] >> 4) & 0x0F), 12);
        c1  = (float)spl_twos((((uint32_t)c[1] & 0x0F) << 8) | c[2], 12);
        c00 = (float)spl_twos(((uint32_t)c[3] << 12) | ((uint32_t)c[4] << 4) | ((c[5] >> 4) & 0x0F), 20);
        c10 = (float)spl_twos((((uint32_t)c[5] & 0x0F) << 16) | ((uint32_t)c[6] << 8) | c[7], 20);
    }

    /* chunk1 @0x18: c01,c11,c20,c21 */
    if (spl_read(SPL_REG_COEF_START + 8, c, 8))
    {
        c01 = (float)spl_twos(((uint32_t)c[0] << 8) | c[1], 16);
        c11 = (float)spl_twos(((uint32_t)c[2] << 8) | c[3], 16);
        c20 = (float)spl_twos(((uint32_t)c[4] << 8) | c[5], 16);
        c21 = (float)spl_twos(((uint32_t)c[6] << 8) | c[7], 16);
    }

    /* chunk2 @0x20: c30 (+ SPA06 特有 c31/c40) */
    if (is_spa06)
    {
        uint8_t c5[5];
        if (spl_read(SPL_REG_COEF_START + 16, c5, 5))
        {
            c30 = (float)spl_twos(((uint32_t)c5[0] << 8) | c5[1], 16);
            c31 = (float)spl_twos(((uint32_t)c5[2] << 4) | ((c5[3] >> 4) & 0x0F), 12);
            c40 = (float)spl_twos((((uint32_t)c5[3] & 0x0F) << 8) | c5[4], 12);
        }
    }
    else
    {
        uint8_t c2[2];
        if (spl_read(SPL_REG_COEF_START + 16, c2, 2))
        {
            c30 = (float)spl_twos(((uint32_t)c2[0] << 8) | c2[1], 16);
        }
        c31 = 0.0f;
        c40 = 0.0f;
    }
}

bool FC_Baro_Init(void)
{
    /* 软复位 */
    if (!spl_write(SPL_REG_RST, SPL_RESET_CMD)) return false;
    HAL_Delay(40);

    /* 检测器件 */
    uint8_t id = 0;
    if (!spl_read(SPL_REG_CHIP_ID, &id, 1)) return false;
    bool is_spa06 = (id == SPL_CHIP_SPA06);
    bool is_spl06 = (id == SPL_CHIP_SPL06);
    if (!is_spa06 && !is_spl06) return false;

    /* 读取温度系数来源并镜像到 TMP_CFG bit7 */
    uint8_t coef_srce = 0;
    if (!spl_read(SPL_REG_COEF_SRCE, &coef_srce, 1)) return false;
    uint8_t tmp_ext = coef_srce & 0x80;

    spl_read_coeffs(is_spa06);

    /* 配置 */
    if (!spl_write(SPL_REG_PRS_CFG,    SPL_RATE_4HZ | SPL_OS_64X_P)) return false;
    if (!spl_write(SPL_REG_TMP_CFG,    SPL_RATE_4HZ | SPL_OS_1X_T | tmp_ext)) return false;
    if (!spl_write(SPL_REG_INT_FIFO,   SPL_PRES_BIT_SHIFT)) return false;
    if (!spl_write(SPL_REG_MODE_STAT,  SPL_MEAS_CONT)) return false;

    return true;
}

bool FC_Baro_Read(fc_baro_t *baro)
{
    if (!baro) return false;

    uint8_t d[6];
    if (!spl_read(SPL_REG_PRS_B2, d, 6))
    {
        return false;
    }

    int32_t p_raw = spl_twos(((uint32_t)d[0] << 16) | ((uint32_t)d[1] << 8) | d[2], 24);
    int32_t t_raw = spl_twos(((uint32_t)d[3] << 16) | ((uint32_t)d[4] << 8) | d[5], 24);

    float p_sc = (float)p_raw / SCALE_FACTOR_P;
    float t_sc = (float)t_raw / SCALE_FACTOR_T;

    /* SPA06/SPL06 补偿多项式 */
    float p_pa = c00 + p_sc * (c10 + p_sc * (c20 + p_sc * (c30 + p_sc * c40)))
               + t_sc * c01 + t_sc * p_sc * (c11 + p_sc * (c21 + p_sc * c31));
    float t_deg = c0 * 0.5f + c1 * t_sc;

    baro->pressure_pa = p_pa;
    baro->temperature = t_deg;
    baro->pressure_std = p_pa;
    baro->fresh = true;
    return true;
}

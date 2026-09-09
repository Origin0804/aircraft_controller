/**
  ******************************************************************************
  * @file    fc/fc_mag.c
  * @brief   IST8310 磁力计驱动（I2C1 轮询，阻塞式读写）
  * @note    寄存器/初始化序列参照 Betaflight compass_ist8310.c：
  *            WAI(0x00)=0x10, DATA(0x03) 6字节,
  *            AVG(0x41)=0x24, PDCNTL(0x42)=0xC0, CNTRL1(0x0A)=0x01 单次测量
  *          数据为 14bit 大端；Y 轴按右手系取反。
  ******************************************************************************
  */
#include "fc/fc_mag.h"
#include "fc/fc_board.h"
#include "stm32h7xx_hal.h"

/* 7 位 I2C 地址（CAD1/CAD0 悬空 -> 0x0E；HAL 用 7 位地址，不左移） */
#define IST_ADDR            0x0E

#define IST_REG_WAI         0x00
#define IST_REG_STAT1       0x02
#define IST_REG_DATA        0x03
#define IST_REG_CNTRL1      0x0A
#define IST_REG_AVERAGE     0x41
#define IST_REG_PDCNTL      0x42

#define IST_WAI_VALID       0x10
#define IST_AVG_16          0x24
#define IST_PULSE_NORMAL    0xC0
#define IST_ODR_SINGLE      0x01

#define IST_LSB_TO_MG       3.0f    /* 14bit, 3 mG/LSB */

#define IST_TIMEOUT         20U

static bool ist_write(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(&hi2c1, IST_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                             &val, 1, IST_TIMEOUT) == HAL_OK;
}

static bool ist_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c1, IST_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                            buf, len, IST_TIMEOUT) == HAL_OK;
}

bool FC_Mag_Init(void)
{
    uint8_t id = 0;
    if (!ist_read(IST_REG_WAI, &id, 1) || id != IST_WAI_VALID)
    {
        return false;
    }

    if (!ist_write(IST_REG_AVERAGE, IST_AVG_16))      return false;
    HAL_Delay(6);
    if (!ist_write(IST_REG_PDCNTL, IST_PULSE_NORMAL)) return false;
    HAL_Delay(6);
    if (!ist_write(IST_REG_CNTRL1, IST_ODR_SINGLE))   return false;

    return true;
}

bool FC_Mag_Read(fc_mag_t *mag)
{
    if (!mag) return false;

    uint8_t buf[6];
    if (!ist_read(IST_REG_DATA, buf, 6))
    {
        return false;
    }

    /* 14bit 大端；Y 轴反相以符合右手系 */
    mag->field[0] =  (float)(int16_t)((buf[1] << 8) | buf[0]) * IST_LSB_TO_MG;
    mag->field[1] = -(float)(int16_t)((buf[3] << 8) | buf[2]) * IST_LSB_TO_MG;
    mag->field[2] =  (float)(int16_t)((buf[5] << 8) | buf[4]) * IST_LSB_TO_MG;
    mag->fresh = true;

    /* 重新触发下一次单次测量 */
    (void)ist_write(IST_REG_CNTRL1, IST_ODR_SINGLE);
    return true;
}

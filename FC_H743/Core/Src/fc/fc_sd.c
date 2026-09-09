/**
  ******************************************************************************
  * @file    fc/fc_sd.c
  * @brief   SD 黑匣子（裸扇区顺序写，无文件系统）
  * @note    记录打包进 512B 块，每 32 条刷一次块；写前做 D-cache 一致性维护。
  *          无卡时安全静默，不阻塞控制回路。
  ******************************************************************************
  */
#include "fc/fc_sd.h"
#include "fc/fc_board.h"
#include "stm32h7xx_hal.h"
#include <string.h>

#define SD_REC_PER_BLOCK   (512U / sizeof(fc_sd_rec_t))
#define SD_START_SECTOR    32U      /* 从 0x20 号扇区开始，避开 MBR/分区表区 */

static uint8_t    sd_block[512] __attribute__((aligned(32)));
static uint16_t   sd_rec_idx = 0;
static uint32_t   sd_sector = SD_START_SECTOR;
static uint32_t   sd_block_size = 512;
static bool       sd_ready = false;

bool FC_SD_Init(void)
{
    sd_ready = false;

    /* 卡初始化（MX_SDMMC1_SD_Init 已调用过 HAL_SD_Init，此处再确认） */
    if (HAL_SD_Init(&hsd1) != HAL_OK)
    {
        return false;
    }

    /* 读取卡信息（容量/块大小） */
    HAL_SD_CardInfoTypeDef info;
    if (HAL_SD_GetCardInfo(&hsd1, &info) == HAL_OK)
    {
        sd_block_size = info.LogBlockSize;      /* 通常 512 */
        if (sd_block_size != 512) return false; /* 本项目按 512B 扇区处理 */
    }
    else
    {
        return false;
    }

    sd_rec_idx = 0;
    sd_sector  = SD_START_SECTOR;
    sd_ready   = true;
    return true;
}

bool FC_SD_Ready(void)
{
    return sd_ready;
}

void FC_SD_Log(const fc_attitude_t *att, const fc_imu_t *imu)
{
    if (!sd_ready || !att || !imu) return;

    fc_sd_rec_t *r = (fc_sd_rec_t *)&sd_block[sd_rec_idx * sizeof(fc_sd_rec_t)];
    r->ts     = HAL_GetTick();
    r->acc[0] = (int16_t)(imu->accel[0] * 100.0f);
    r->acc[1] = (int16_t)(imu->accel[1] * 100.0f);
    r->acc[2] = (int16_t)(imu->accel[2] * 100.0f);
    r->gyro[0]= (int16_t)(imu->gyro[0] * 10.0f);
    r->gyro[1]= (int16_t)(imu->gyro[1] * 10.0f);
    r->gyro[2]= (int16_t)(imu->gyro[2] * 10.0f);

    sd_rec_idx++;
    if (sd_rec_idx >= SD_REC_PER_BLOCK)
    {
        /* 写前清理 D-cache，保证 DMA/外设读到最新数据 */
        SCB_CleanDCache_by_Addr((uint32_t *)sd_block, sizeof(sd_block));
        if (HAL_SD_WriteBlocks(&hsd1, sd_block, sd_sector, 1, 1000) == HAL_OK)
        {
            sd_sector++;
        }
        sd_rec_idx = 0;
    }
}

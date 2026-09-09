/**
  ******************************************************************************
  * @file    fc/fc_power.c
  * @brief   电源采样（ADC1 双通道堵塞扫描，无中断）
  * @note    ADC1 为 16bit 双通道扫描：rank1=IN16(电压 PA0), rank2=IN17(电流 PA1)，
  *          EOC 单转换、DR 数据管理、时钟 DIV1(30MHz)。按 rank 顺序依次取数。
  ******************************************************************************
  */
#include "fc/fc_power.h"
#include "fc/fc_board.h"
#include "stm32h7xx_hal.h"

static void power_apply(uint16_t raw, uint8_t index, fc_power_t *pw)
{
    float v_adc = (float)(raw & 0xFFFF) / FC_ADC_FULL_SCALE * FC_ADC_VREF;

    if (index == 0)        /* rank1 = IN16 = 电压 */
    {
        pw->voltage = v_adc * FC_POWER_VOLTAGE_RATIO;
    }
    else                   /* rank2 = IN17 = 电流 */
    {
        pw->current = v_adc * FC_POWER_CURRENT_SCALE + FC_POWER_CURRENT_OFFSET;
    }
}

void FC_Power_Init(void)
{
    /* ADC1 单端校准 */
    (void)HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED, ADC_SINGLE_ENDED);
}

bool FC_Power_Read(fc_power_t *pw)
{
    if (!pw) return false;

    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return false;
    }

    /* 2 通道扫描：逐次取 EOC 结果 */
    for (int i = 0; i < 2; ++i)
    {
        if (HAL_ADC_PollForConversion(&hadc1, 20) != HAL_OK)
        {
            HAL_ADC_Stop(&hadc1);
            return false;
        }
        power_apply((uint16_t)HAL_ADC_GetValue(&hadc1), (uint8_t)i, pw);
    }

    HAL_ADC_Stop(&hadc1);
    pw->fresh = true;
    return true;
}

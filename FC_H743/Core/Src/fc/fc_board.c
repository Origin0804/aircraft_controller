/**
  ******************************************************************************
  * @file    fc/fc_board.c
  * @brief   FC_H743 板级初始化与运行时修正
  * @note    本文件不修改 CubeMX 生成的 MX_*_Init；所有修正基于现有句柄/寄存器，
  *          可在 CubeMX 重新生成后安全保留（幂等）。
  ******************************************************************************
  */
#include "fc/fc_board.h"
#include "fc/fc_math.h"
#include "fc/fc_types.h"
#include "stm32h7xx_hal_rcc.h"
#include "stm32h7xx_hal_rcc_ex.h"

/* ------------------------------------------------------------------ */
/* 1) Cache                                                           */
/* ------------------------------------------------------------------ */
void FC_Board_EnableCache(void)
{
    SCB_EnableICache();
    SCB_EnableDCache();
}

/* ------------------------------------------------------------------ */
/* 2) PLL2 + ADC 时钟修正（CubeMX 只配了分频、未使能 PLL2）            */
/*    使能 PLL2：M=5,N=96,P=16 -> PLL2P=30MHz (VCO 480MHz)            */
/* ------------------------------------------------------------------ */
void FC_Board_FixAdcClock(void)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
    PeriphClkInit.AdcClockSelection    = RCC_ADCCLKSOURCE_PLL2;

    /* PLL2 配置（输入 HSE/5 = 5MHz, VCO = 480MHz, P=16 -> 30MHz 供 ADC） */
    PeriphClkInit.PLL2.PLL2M      = 5;
    PeriphClkInit.PLL2.PLL2N      = 96;
    PeriphClkInit.PLL2.PLL2P      = 16;
    PeriphClkInit.PLL2.PLL2Q      = 2;
    PeriphClkInit.PLL2.PLL2R      = 10;
    PeriphClkInit.PLL2.PLL2RGE    = RCC_PLL2VCIRANGE_3;   /* 4-8MHz 输入范围 */
    PeriphClkInit.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    PeriphClkInit.PLL2.PLL2FRACN  = 0;

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ------------------------------------------------------------------ */
/* 3) PWM 输出（TIM1 电机 M1-M4 / TIM4 舵机备用）                      */
/*    TIM1/TIM4 已由 CubeMX 配好 PSC=239, ARR=2499/49999, 1MHz 计时。  */
/* ------------------------------------------------------------------ */
void FC_Board_PwmInit(void)
{
    /* TIM1（高级定时器）：必须先开主输出 MOE 才有信号 */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
    __HAL_TIM_MOE_ENABLE(&htim1);

    /* TIM4（普通定时器） */
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);
    __HAL_TIM_ENABLE(&htim4);

    /* 初始：电机停（1000us），舵机中线（1500us） */
    FC_PWM_SetMotor(0, FC_PWM_MIN_US);
    FC_PWM_SetMotor(1, FC_PWM_MIN_US);
    FC_PWM_SetMotor(2, FC_PWM_MIN_US);
    FC_PWM_SetMotor(3, FC_PWM_MIN_US);
    for (int i = 0; i < 4; ++i)
        FC_PWM_SetServo(i, FC_PWM_MID_US);
}

/* ------------------------------------------------------------------ */
/* 4) UART4 波特率修正：SBUS 需要 100000（保留 CubeMX 的 RX 反相）      */
/* ------------------------------------------------------------------ */
void FC_Board_FixUart4Baud(void)
{
    huart4.Init.BaudRate = 100000;      /* SBUS = 100000 baud */
    /* AdvancedInit.RxPinLevelInvert 仍保留在句柄里（RX 反相） */
    if (HAL_UART_Init(&huart4) != HAL_OK)
    {
        Error_Handler();
    }
}

/* ------------------------------------------------------------------ */
/* 5) 板级总初始化                                                    */
/* ------------------------------------------------------------------ */
void FC_Board_Init(void)
{
    FC_Board_EnableCache();
    FC_Board_PwmInit();
    FC_Board_FixUart4Baud();

    /* 关闭蜂鸣器 / LED */
    FC_BUZZER_OFF();
    FC_LED_WS2812_OFF();
}

/* ------------------------------------------------------------------ */
/* 6) 输出设置（供混控/控制层调用）                                    */
/*    电机 PWM = TIM1 CH1-4；舵机 = TIM4 CH1-4                         */
/* ------------------------------------------------------------------ */
void FC_PWM_SetMotor(uint8_t idx, uint16_t us)
{
    if (idx >= FC_MOTOR_COUNT) return;
    if (us < FC_PWM_MIN_US) us = FC_PWM_MIN_US;
    if (us > FC_PWM_MAX_US) us = FC_PWM_MAX_US;
    switch (idx) {
        case 0: __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, us); break;
        case 1: __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, us); break;
        case 2: __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, us); break;
        case 3: __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, us); break;
        default: break;
    }
}

void FC_PWM_SetServo(uint8_t idx, uint16_t us)
{
    if (idx >= 4) return;
    if (us < 500) us = 500;
    if (us > 2500) us = 2500;
    switch (idx) {
        case 0: __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, us); break;
        case 1: __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, us); break;
        case 2: __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, us); break;
        case 3: __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, us); break;
        default: break;
    }
}

/**
  ******************************************************************************
  * @file    Core/Src/stm32h7xx_it.c
  * @brief   Interrupt Service Routines - FC_H743_LL
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32h7xx_it.h"

/******************************************************************************/
/*           Cortex-M7 Processor Interruption and Exception Handlers          */
/******************************************************************************/

/**
  * @brief  This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief  This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief  This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief  This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief  This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  while (1)
  {
  }
}

/**
  * @brief  This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
}

/**
  * @brief  This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  This function handles System tick timer（混合工程：同时喂 HAL 与 LL tick）.
  */
void SysTick_Handler(void)
{
  /* H7 LL 的 tick 由硬件计数直读（LL_GetTick），无须 ISR 增量；
   * HAL 侧（SDMMC 卡初始化超时）需要 HAL_IncTick。 */
  HAL_IncTick();
}

/******************************************************************************/
/* STM32H7xx Peripheral Interrupt Handlers                                    */
/******************************************************************************/

/**
  * @brief  This function handles EXTI9_5 interrupt（PC5 IMU_INT1，下降沿，与 EXTI5~9 共享）.
  */
void EXTI9_5_IRQHandler(void)
{
  if (LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_5) != 0)
  {
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_5);
    /* TODO: IMU 数据就绪处理，用户代码 */
  }
}

/**
  * @brief  USART1 中断（PA9/PA10）：回环测试，收一个字节原样回传
  */
void USART1_IRQHandler(void)
{
  if (LL_USART_IsActiveFlag_RXNE(USART1) != 0)
  {
    uint8_t data = LL_USART_ReceiveData8(USART1);
    if (LL_USART_IsActiveFlag_ORE(USART1) != 0) { LL_USART_ClearFlag_ORE(USART1); }
    if (LL_USART_IsActiveFlag_FE(USART1)  != 0) { LL_USART_ClearFlag_FE(USART1);  }
    if (LL_USART_IsActiveFlag_NE(USART1)  != 0) { LL_USART_ClearFlag_NE(USART1);  }
    if (LL_USART_IsActiveFlag_PE(USART1)  != 0) { LL_USART_ClearFlag_PE(USART1);  }
    /* 等待发送缓冲区空，再回传 */
    while (LL_USART_IsActiveFlag_TXE(USART1) == 0) { }
    LL_USART_TransmitData8(USART1, data);
  }
}

/**
  * @brief  USART3 中断（PD8/PD9）：回环测试
  */
void USART3_IRQHandler(void)
{
  if (LL_USART_IsActiveFlag_RXNE(USART3) != 0)
  {
    uint8_t data = LL_USART_ReceiveData8(USART3);
    if (LL_USART_IsActiveFlag_ORE(USART3) != 0) { LL_USART_ClearFlag_ORE(USART3); }
    if (LL_USART_IsActiveFlag_FE(USART3)  != 0) { LL_USART_ClearFlag_FE(USART3);  }
    if (LL_USART_IsActiveFlag_NE(USART3)  != 0) { LL_USART_ClearFlag_NE(USART3);  }
    if (LL_USART_IsActiveFlag_PE(USART3)  != 0) { LL_USART_ClearFlag_PE(USART3);  }
    while (LL_USART_IsActiveFlag_TXE(USART3) == 0) { }
    LL_USART_TransmitData8(USART3, data);
  }
}

/**
  * @brief  UART4 中断（PA12/PA11）：回环测试
  */
void UART4_IRQHandler(void)
{
  if (LL_USART_IsActiveFlag_RXNE(UART4) != 0)
  {
    uint8_t data = LL_USART_ReceiveData8(UART4);
    if (LL_USART_IsActiveFlag_ORE(UART4) != 0) { LL_USART_ClearFlag_ORE(UART4); }
    if (LL_USART_IsActiveFlag_FE(UART4)  != 0) { LL_USART_ClearFlag_FE(UART4);  }
    if (LL_USART_IsActiveFlag_NE(UART4)  != 0) { LL_USART_ClearFlag_NE(UART4);  }
    if (LL_USART_IsActiveFlag_PE(UART4)  != 0) { LL_USART_ClearFlag_PE(UART4);  }
    while (LL_USART_IsActiveFlag_TXE(UART4) == 0) { }
    LL_USART_TransmitData8(UART4, data);
  }
}

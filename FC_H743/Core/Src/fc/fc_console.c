/**
  ******************************************************************************
  * @file    fc/fc_console.c
  * @brief   控制台输出（USART1 115200，USB/CH340；仅 TX）
  ******************************************************************************
  */
#include "fc/fc_console.h"
#include "fc/fc_board.h"
#include "stm32h7xx_hal.h"
#include <string.h>
#include <stdio.h>

void FC_Console_Puts(const char *s)
{
    if (!s) return;
    HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 100);
}

void FC_Console_Printf(const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0)
    {
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)n, 100);
    }
}

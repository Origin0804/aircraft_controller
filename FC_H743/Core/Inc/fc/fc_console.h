/**
  ******************************************************************************
  * @file    fc/fc_console.h
  * @brief   控制台输出（USART1 = 115200，USB/CH340；仅 TX，无中断依赖）
  ******************************************************************************
  */
#ifndef FC_CONSOLE_H
#define FC_CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

/* 格式化输出到控制台（USART1 阻塞发送） */
void FC_Console_Printf(const char *fmt, ...);

/* 字符串输出 */
void FC_Console_Puts(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* FC_CONSOLE_H */

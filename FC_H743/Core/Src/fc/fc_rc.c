/**
  ******************************************************************************
  * @file    fc/fc_rc.c
  * @brief   SBUS 解码状态机（UART4 接收，字节入口 FC_RC_OnByte）
  * @note    UART4 的波特率/奇偶/停止位与 RX 反相在 CubeMX 中配置；
  *          UART4 需在 CubeMX 中开启"USART4 全局中断"（NVIC），
  *          生成的 UART4_IRQHandler 会调用 HAL_UART_IRQHandler，
  *          再由 usart.c 的 HAL_UART_RxCpltCallback 将每个字节送入本模块。
  *          本模块不触碰中断/NVIC/寄存器，仅做协议解析，便于维护。
  ******************************************************************************
  */
#include "fc/fc_rc.h"
#include "fc/fc_board.h"
#include "stm32h7xx_hal.h"
#include <string.h>

#define SBUS_FRAME_LEN   25

/* 解码状态机 */
static uint8_t  sbus_buf[SBUS_FRAME_LEN];
static uint16_t sbus_pos = 0;

/* 最新完整帧（控制层读取） */
static fc_rc_t g_rc;

static void sbus_parse_frame(void)
{
    /* 22 字节 = 16 通道 × 11bit（帧体从 buf[1] 起，buf[23]=标志, buf[24]=0x00） */
    uint16_t ch[16];
    for (int i = 0; i < 16; ++i)
    {
        uint16_t bit  = (uint16_t)(i * 11);
        uint16_t byte = bit / 8;
        uint16_t off  = bit % 8;
        ch[i] = (uint16_t)(sbus_buf[1 + byte] | (sbus_buf[2 + byte] << 8));
        ch[i] >>= off;
        ch[i] &= 0x07FF;   /* 11 bit */
    }

    for (int i = 0; i < FC_RC_CHANNEL_MAX; ++i)
    {
        g_rc.chan[i] = (float)ch[i] / 2047.0f;
    }
    g_rc.frame_lost = (sbus_buf[23] & 0x04) ? true : false;
    g_rc.failsafe   = (sbus_buf[23] & 0x08) ? true : false;
    g_rc.connected  = true;
    g_rc.frame_count++;
}

static void sbus_push_byte(uint8_t b)
{
    if (sbus_pos == 0)
    {
        if (b == 0x0F)
        {
            sbus_buf[0] = b;
            sbus_pos = 1;
        }
        return;
    }

    sbus_buf[sbus_pos++] = b;
    if (sbus_pos >= SBUS_FRAME_LEN)
    {
        if (sbus_buf[SBUS_FRAME_LEN - 1] == 0x00)
        {
            sbus_parse_frame();
        }
        sbus_pos = 0;
    }
}

/* 复位（可重复调用）。UART 配置/接收中断由 CubeMX 生成代码负责。 */
void FC_RC_Init(void)
{
    sbus_pos = 0;
    memset(&g_rc, 0, sizeof(g_rc));
}

/* 逐字节入口：由 usart.c 的 HAL_UART_RxCpltCallback（UART4 分支）调用 */
void FC_RC_OnByte(uint8_t b)
{
    sbus_push_byte(b);
}

void FC_RC_Get(fc_rc_t *rc)
{
    if (rc) *rc = g_rc;
}

bool FC_RC_Armed(const fc_rc_t *rc)
{
    return (rc && rc->connected && (rc->chan[FC_RC_ARM] > 0.75f));
}

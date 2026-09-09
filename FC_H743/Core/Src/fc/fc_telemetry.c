/**
  ******************************************************************************
  * @file    fc/fc_telemetry.c
  * @brief   数传/遥测（USART3 状态帧 TX；格式：$ATT,roll,pitch,yaw,alt,...）
  ******************************************************************************
  */
#include "fc/fc_telemetry.h"
#include "fc/fc_rc.h"
#include "fc/fc_board.h"
#include "stm32h7xx_hal.h"
#include <stdio.h>
#include <string.h>

static uint16_t session_id = 0;

void FC_Telemetry_Init(void)
{
    session_id = 0;
}

void FC_Telemetry_Status(const fc_attitude_t *att, const fc_rc_t *rc,
                         const fc_power_t *pw, const fc_baro_t *baro)
{
    char buf[160];

    if (att && rc && pw && baro)
    {
        int n = snprintf(buf, sizeof(buf),
                         "$ATT,%u,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.2f,%.1f,%u\r\n",
                         ++session_id,
                         att->euler[0], att->euler[1], att->euler[2],
                         pw->voltage, pw->current,
                         baro->pressure_pa / 100.0f,
                         baro->temperature,
                         rc->chan[FC_RC_THROTTLE]);
        if (n > 0)
        {
            HAL_UART_Transmit(&huart3, (uint8_t *)buf, (uint16_t)n, 100);
        }
    }
}

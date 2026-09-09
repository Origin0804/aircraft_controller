/**
  ******************************************************************************
  * @file    fc/fc_main.c
  * @brief   FC 主程序入口与主循环调度（1kHz）
  ******************************************************************************
  */
#include "fc/fc_board.h"
#include "fc/fc_types.h"
#include "fc/fc_math.h"
#include "fc/fc_imu.h"
#include "fc/fc_ahrs.h"
#include "fc/fc_rc.h"
#include "fc/fc_control.h"
#include "fc/fc_mixer.h"
#include "fc/fc_mag.h"
#include "fc/fc_baro.h"
#include "fc/fc_power.h"
#include "fc/fc_console.h"
#include "fc/fc_telemetry.h"
#include "fc/fc_alt.h"
#include "fc/fc_gps.h"
#include "fc/fc_sd.h"

/* 全局状态 */
static fc_imu_t          g_imu;
static fc_attitude_t     g_att;
static fc_rc_t           g_rc;
static fc_control_cmd_t  g_cmd;
static fc_output_t       g_out;
static fc_mag_t          g_mag;
static fc_baro_t         g_baro;
static fc_power_t        g_power;
static float             g_alt_offset;   /* 定高油门偏移 */
static fc_gps_t          g_gps;          /* GPS（串口待接入） */

/* ------------------------------------------------------------------ */
void FC_Init(void)
{
    FC_Board_Init();

    /* IMU 自检与配置 */
    FC_IMU_Init();
    FC_AHRS_Init();

    /* 遥控输入（SBUS）与控制律 */
    FC_RC_Init();
    FC_Control_Init();

    /* 磁力计 / 气压计 / 电源 / 数传 / 定高 */
    FC_Mag_Init();
    FC_Baro_Init();
    FC_Power_Init();
    FC_Telemetry_Init();
    FC_Alt_Init();
    FC_GPS_Init();
    FC_SD_Init();   /* 无卡时静默降级 */

    /* 停转（安全） */
    FC_Mixer_Stop();

    /* 上电提示音 */
    FC_BUZZER_ON(); HAL_Delay(80); FC_BUZZER_OFF();

    /* 自检状态输出 */
    FC_Console_Puts("\r\n[FC] boot - IMU ok, mag/baro/power init done\r\n");

    /* TODO: GPS / SD 初始化 */
}

/* ------------------------------------------------------------------ */
/* 1kHz 控制任务                                                       */
/* ------------------------------------------------------------------ */
static void fc_task_1khz(void)
{
    static uint16_t low_cnt = 0;

    /* 姿态 */
    if (FC_IMU_Read(&g_imu))
    {
        FC_AHRS_Update(&g_imu, 0.001f, &g_att);
    }

    /* 低频传感器采样（约 100Hz）：电源 / 磁力计 / 气压计 + 定高 */
    if (++low_cnt >= 10)
    {
        low_cnt = 0;
        FC_Power_Read(&g_power);
        FC_Mag_Read(&g_mag);
        FC_Baro_Read(&g_baro);
        bool alt_en = (g_rc.chan[6] > 0.6f);   /* AUX3 使能定高 */
        FC_Alt_Update(&g_baro, 0.01f, alt_en, &g_alt_offset);
        FC_GPS_Get(&g_gps);
    }

    /* SD 黑匣子：约 100Hz 记一条姿态/IMU */
    if (low_cnt == 0 && FC_SD_Ready())
    {
        FC_SD_Log(&g_att, &g_imu);
    }

    /* 遥控 */
    FC_RC_Get(&g_rc);

    /* 安全：未连接/失联/未解锁 -> 停转 */
    bool armed = FC_RC_Armed(&g_rc) && !g_rc.failsafe && !g_rc.frame_lost;
    if (!armed)
    {
        FC_Mixer_Stop();
        return;
    }

    /* 控制 -> 混控 -> 电机 */
    FC_Control_Update(&g_att, &g_rc, &g_cmd);
    g_cmd.throttle += g_alt_offset;   /* 定高叠加 */
    g_cmd.throttle = fc_constrainf(g_cmd.throttle, 0.0f, 1.0f);
    FC_Mixer_Apply(&g_cmd, &g_out);

    /* 遥测：约 10Hz 上报 */
    if (g_rc.frame_count % 100 == 0)
    {
        FC_Telemetry_Status(&g_att, &g_rc, &g_power, &g_baro);
    }

    /* TODO: 定高/定点等外环；100Hz 数传；1Hz 状态 */
}

/* ------------------------------------------------------------------ */
void FC_Loop(void)
{
    static uint32_t last_tick = 0;
    uint32_t now = HAL_GetTick();

    /* 1kHz 门控（HAL tick 1ms） */
    if (now == last_tick) return;
    last_tick = now;

    fc_task_1khz();
}

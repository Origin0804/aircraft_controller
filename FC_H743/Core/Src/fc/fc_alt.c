/**
  ******************************************************************************
  * @file    fc/fc_alt.c
  * @brief   气压计定高：低通滤波气压 -> 相对高度，垂直速度微分，PID->油门偏移
  * @note    高度按 (101325-P)/12 近似（约 11.8 Pa/m）定标；增益为初值需调参。
  ******************************************************************************
  */
#include "fc/fc_alt.h"
#include "fc/fc_math.h"
#include <math.h>

#define ALT_LP_ALPHA     0.10f    /* 气压低通系数（越慢越稳） */
#define ALT_P            0.20f    /* 高度误差 -> 油门 */
#define ALT_V            0.15f    /* 垂直速度 -> 油门 */
#define ALT_MAX_OFFSET   0.35f
#define ALT_MAX_VEL      4.0f     /* m/s */

static bool  alt_inited;
static float alt_p_lp;         /* 低通气压 Pa */
static float alt_prev_m;       /* 上次高度 m */
static float alt_ref;          /* 定高目标 m */
static bool  alt_was_en;


void FC_Alt_Init(void)
{
    alt_inited = false;
    alt_p_lp   = 0.0f;
    alt_prev_m = 0.0f;
    alt_ref    = 0.0f;
    alt_was_en = false;
}

void FC_Alt_Update(const fc_baro_t *baro, float dt, bool enable, float *offset)
{
    if (!baro || !offset) return;
    *offset = 0.0f;
    if (dt <= 0.0f) dt = 0.01f;

    /* 低通滤波气压 */
    if (!alt_inited)
    {
        alt_p_lp   = baro->pressure_pa;
        alt_prev_m = 0.0f;
        alt_inited = true;
    }
    alt_p_lp += ALT_LP_ALPHA * (baro->pressure_pa - alt_p_lp);

    /* 近似相对高度 (m) */
    float alt_m = (101325.0f - alt_p_lp) / 12.0f;

    /* 垂直速度（微分 + 限幅） */
    float vel = (alt_m - alt_prev_m) / dt;
    alt_prev_m = alt_m;
    vel = fc_constrainf(vel, -ALT_MAX_VEL, ALT_MAX_VEL);

    /* 进入定高瞬间锁存目标 */
    if (enable && !alt_was_en)
    {
        alt_ref = alt_m;
    }
    alt_was_en = enable;

    if (enable)
    {
        float err = alt_ref - alt_m;
        *offset = fc_constrainf(ALT_P * err - ALT_V * vel, -ALT_MAX_OFFSET, ALT_MAX_OFFSET);
    }
    else
    {
        *offset = 0.0f;
    }
}

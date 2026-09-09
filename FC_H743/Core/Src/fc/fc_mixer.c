/**
  ******************************************************************************
  * @file    fc/fc_mixer.c
  * @brief   X 型四轴混控（M1..M4 -> TIM1 CH1..CH4）
  * @note    电机布局（俯视，机头朝 +X）：
  *            M1 右前(CW)  M2 左前(CCW)
  *            M3 左后(CW)  M4 右后(CCW)
  *          混控符号若与实机转向/旋向不符，在此统一翻转即可。
  ******************************************************************************
  */
#include "fc/fc_mixer.h"
#include "fc/fc_board.h"
#include "fc/fc_math.h"

#define MIXER_AUTHORITY_US  500.0f   /* 归一化控制量(±1)对应的油门偏移 */

void FC_Mixer_Stop(void)
{
    for (int i = 0; i < FC_MOTOR_COUNT; ++i)
    {
        FC_PWM_SetMotor(i, FC_PWM_MIN_US);
    }
}

void FC_Mixer_Apply(const fc_control_cmd_t *cmd, fc_output_t *out)
{
    if (!cmd) return;

    /* 基础油门 1000..2000us */
    float throttle_us = FC_PWM_MIN_US + cmd->throttle * (FC_PWM_MAX_US - FC_PWM_MIN_US);

    float r = cmd->roll  * MIXER_AUTHORITY_US;
    float p = cmd->pitch * MIXER_AUTHORITY_US;
    float y = cmd->yaw   * MIXER_AUTHORITY_US;

    /* X 型混控 */
    float m[4];
    m[0] = throttle_us + r + p + y;   /* M1 右前 */
    m[1] = throttle_us - r + p - y;   /* M2 左前 */
    m[2] = throttle_us - r - p + y;   /* M3 左后 */
    m[3] = throttle_us + r - p - y;   /* M4 右后 */

    for (int i = 0; i < FC_MOTOR_COUNT; ++i)
    {
        uint16_t us = (uint16_t)fc_constrainf(m[i],
                                              (float)FC_PWM_MIN_US,
                                              (float)FC_PWM_MAX_US);
        FC_PWM_SetMotor(i, us);
        if (out) out->motor[i] = (float)us;
    }
    if (out) out->armed = 1;
}

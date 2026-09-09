/**
  ******************************************************************************
  * @file    fc/fc_control.c
  * @brief   姿态控制：角度外环(P) + 角速度内环(PID)，输出归一化控制量
  * @note    增益为初值，装机后需调参（先小油门、逐轴验证极性）。
  ******************************************************************************
  */
#include "fc/fc_control.h"
#include "fc/fc_rc.h"
#include "fc/fc_math.h"

/* ---- 参数（可调） ---- */
#define ANGLE_MAX_DEG       30.0f   /* 角度模式最大倾角 */
#define ACRO_RATE_DEGS      250.0f  /* 手动模式最大角速度 */
#define YAW_RATE_DEGS       120.0f  /* 偏航最大角速度 */

#define ANGLE_P             1.5f    /* 角度误差 -> 期望角速度 (deg/s per deg) */
#define RATE_P              0.10f   /* 角速度环 P */
#define RATE_I              0.00f
#define RATE_D              0.00f
#define RATE_IMAX           30.0f

/* 归一化：多少 deg/s 的环输出 = 满控制量 */
#define CTRL_AUTHORITY_DEGS 150.0f

static fc_pid_t pid_roll;
static fc_pid_t pid_pitch;
static fc_pid_t pid_yaw;

void FC_Control_Init(void)
{
    pid_roll.p = pid_pitch.p = pid_yaw.p = RATE_P;
    pid_roll.i = pid_pitch.i = pid_yaw.i = RATE_I;
    pid_roll.d = pid_pitch.d = pid_yaw.d = RATE_D;
    pid_roll.out = pid_pitch.out = pid_yaw.out = 0.0f;
    pid_roll.integral = pid_pitch.integral = pid_yaw.integral = 0.0f;
    pid_roll.prev_error = pid_pitch.prev_error = pid_yaw.prev_error = 0.0f;
}

static float pid_update(fc_pid_t *p, float error, float dt)
{
    p->integral += error * dt;
    if (p->integral >  RATE_IMAX) p->integral =  RATE_IMAX;
    if (p->integral < -RATE_IMAX) p->integral = -RATE_IMAX;

    float deriv = (error - p->prev_error) / dt;
    p->prev_error = error;
    p->out = p->p * error + p->i * p->integral + p->d * deriv;
    return p->out;
}

void FC_Control_Update(const fc_attitude_t *att, const fc_rc_t *rc, fc_control_cmd_t *cmd)
{
    if (!att || !rc || !cmd) return;

    /* 遥控杆量：0.5 居中 -> -1..1 */
    float stick_roll  = (rc->chan[FC_RC_ROLL]     - 0.5f) * 2.0f;
    float stick_pitch = (rc->chan[FC_RC_PITCH]    - 0.5f) * 2.0f;
    float stick_yaw   = (rc->chan[FC_RC_YAW]      - 0.5f) * 2.0f;

    float roll_rate_cmd, pitch_rate_cmd;

    bool angle_mode = (rc->chan[FC_RC_MODE] > 0.5f);

    if (angle_mode)
    {
        /* 角度外环：期望角 -> 期望角速度 */
        float target_roll  = stick_roll  * ANGLE_MAX_DEG;
        float target_pitch = stick_pitch * ANGLE_MAX_DEG;
        roll_rate_cmd  = ANGLE_P * (target_roll  - att->euler[0]);
        pitch_rate_cmd = ANGLE_P * (target_pitch - att->euler[1]);
    }
    else
    {
        roll_rate_cmd  = stick_roll  * ACRO_RATE_DEGS;
        pitch_rate_cmd = stick_pitch * ACRO_RATE_DEGS;
    }
    float yaw_rate_cmd = stick_yaw * YAW_RATE_DEGS;

    /* 角速度内环 PID */
    float roll_out  = pid_update(&pid_roll,  roll_rate_cmd  - att->gyro_body[0], 0.001f);
    float pitch_out = pid_update(&pid_pitch, pitch_rate_cmd - att->gyro_body[1], 0.001f);
    float yaw_out   = pid_update(&pid_yaw,   yaw_rate_cmd   - att->gyro_body[2], 0.001f);

    /* 归一化控制量 */
    cmd->roll  = fc_constrainf(roll_out  / CTRL_AUTHORITY_DEGS, -1.0f, 1.0f);
    cmd->pitch = fc_constrainf(pitch_out / CTRL_AUTHORITY_DEGS, -1.0f, 1.0f);
    cmd->yaw   = fc_constrainf(yaw_out   / CTRL_AUTHORITY_DEGS, -1.0f, 1.0f);
    cmd->throttle = fc_constrainf(rc->chan[FC_RC_THROTTLE], 0.0f, 1.0f);
}

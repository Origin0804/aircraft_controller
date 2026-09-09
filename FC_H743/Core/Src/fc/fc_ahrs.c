/**
  ******************************************************************************
  * @file    fc/fc_ahrs.c
  * @brief   Mahony 互补滤波姿态解算（四元数）
  *          陀螺积分为主，加速度计观测修正 roll/pitch 漂移（磁力计修正 yaw 后续接入）
  ******************************************************************************
  */
#include "fc/fc_ahrs.h"
#include "fc/fc_math.h"
#include <math.h>

/* 增益：Kp 越大越信加速度计；Ki 消除陀螺零偏漂移 */
#define AHRS_KP  0.6f
#define AHRS_KI  0.02f

static float ahrs_q[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
static float ahrs_integral[3] = { 0.0f, 0.0f, 0.0f };

void FC_AHRS_Init(void)
{
    quat_identity(ahrs_q);
    ahrs_integral[0] = ahrs_integral[1] = ahrs_integral[2] = 0.0f;
}

void FC_AHRS_SetAttitude(const float *q)
{
    quat_copy(ahrs_q, q);
    quat_normalize(ahrs_q);
}

void FC_AHRS_Update(const fc_imu_t *imu, float dt, fc_attitude_t *att)
{
    if (!imu || !att || dt <= 0.0f) return;
    if (dt > 0.05f) dt = 0.05f;   /* 限制最大步长，防止异常 */

    float g0 = 9.80665f;

    /* 归一化加速度（单位 g 方向） */
    float ax = imu->accel[0] / g0;
    float ay = imu->accel[1] / g0;
    float az = imu->accel[2] / g0;
    float an = sqrtf(ax*ax + ay*ay + az*az);
    if (an < 1e-6f) an = 1.0f;
    ax /= an; ay /= an; az /= an;

    /* 用当前四元数把世界系重力 [0,0,1] 转到机体系，得到重力估计 */
    float g_world[3] = { 0.0f, 0.0f, 1.0f };
    float g_body[3];
    quat_rotate(ahrs_q, g_world, g_body);

    /* 误差 = accel × g_body */
    float ex = ay * g_body[2] - az * g_body[1];
    float ey = az * g_body[0] - ax * g_body[2];
    float ez = ax * g_body[1] - ay * g_body[0];

    /* 积分项（消除长期漂移），做限幅 */
    ahrs_integral[0] += ex * AHRS_KI * dt;
    ahrs_integral[1] += ey * AHRS_KI * dt;
    ahrs_integral[2] += ez * AHRS_KI * dt;

    /* 修正后的角速度（deg/s -> rad/s） */
    float gx = (imu->gyro[0] * FC_DEG2RAD) + AHRS_KP * ex + ahrs_integral[0];
    float gy = (imu->gyro[1] * FC_DEG2RAD) + AHRS_KP * ey + ahrs_integral[1];
    float gz = (imu->gyro[2] * FC_DEG2RAD) + AHRS_KP * ez + ahrs_integral[2];

    /* 陀螺增量积分 */
    float gyro_dt[3] = { gx * dt, gy * dt, gz * dt };
    quat_integrate(ahrs_q, gyro_dt);

    /* 回填输出 */
    quat_copy(att->q, ahrs_q);
    quat_to_euler(ahrs_q, &att->euler[0], &att->euler[1], &att->euler[2]);
    att->gyro_body[0] = imu->gyro[0];
    att->gyro_body[1] = imu->gyro[1];
    att->gyro_body[2] = imu->gyro[2];
    att->accel_body[0] = imu->accel[0];
    att->accel_body[1] = imu->accel[1];
    att->accel_body[2] = imu->accel[2];
}

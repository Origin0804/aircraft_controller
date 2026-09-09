/**
  ******************************************************************************
  * @file    fc/fc_math.c
  * @brief   FC 飞控基础数学实现
  ******************************************************************************
  */
#include "fc/fc_math.h"
#include <math.h>

/* 四元数 -> 欧拉角 (deg, XYZ, 用于 RC/地面站显示；姿态解算用四元数本身) */
void quat_to_euler(const float *q, float *roll, float *pitch, float *yaw)
{
    float w = q[0], x = q[1], y = q[2], z = q[3];

    /* roll (绕 X) */
    float sinr_cosp = 2.0f * (w * x + y * z);
    float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    *roll = atan2f(sinr_cosp, cosr_cosp) * FC_RAD2DEG;

    /* pitch (绕 Y) */
    float sinp = 2.0f * (w * y - z * x);
    if (fabsf(sinp) >= 1.0f)
        *pitch = copysignf(90.0f, sinp);
    else
        *pitch = asinf(sinp) * FC_RAD2DEG;

    /* yaw (绕 Z) */
    float siny_cosp = 2.0f * (w * z + x * y);
    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    *yaw = atan2f(siny_cosp, cosy_cosp) * FC_RAD2DEG;
}

/* 用四元数旋转向量: out = R(q) * v */
void quat_rotate(const float *q, const float *v, float *out)
{
    float w = q[0], x = q[1], y = q[2], z = q[3];
    float vx = v[0], vy = v[1], vz = v[2];

    /* t = 2 * cross(q_v, v) */
    float tx = 2.0f * (y * vz - z * vy);
    float ty = 2.0f * (z * vx - x * vz);
    float tz = 2.0f * (x * vy - y * vx);

    /* out = v + w*t + cross(q_v, t) */
    out[0] = vx + w * tx + (y * tz - z * ty);
    out[1] = vy + w * ty + (z * tx - x * tz);
    out[2] = vz + w * tz + (x * ty - y * tx);
}

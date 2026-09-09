/**
  ******************************************************************************
  * @file    fc/fc_math.h
  * @brief   FC 飞控基础数学：向量、四元数、三角函数辅助
  ******************************************************************************
  */
#ifndef FC_MATH_H
#define FC_MATH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include "fc_types.h"

#define FC_PI        3.14159265358979323846f
#define FC_TWO_PI    6.28318530717958647692f
#define FC_DEG2RAD   (FC_PI / 180.0f)
#define FC_RAD2DEG   (180.0f / FC_PI)

/* 数值限定 */
static inline float fc_constrainf(float v, float lo, float hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}
static inline float fc_wrap_pi(float a) {
    while (a >  FC_PI) a -= FC_TWO_PI;
    while (a < -FC_PI) a += FC_TWO_PI;
    return a;
}
static inline float fc_wrap_180(float d) { return fc_wrap_pi(d * FC_DEG2RAD) * FC_RAD2DEG; }

/* 快速平方根 (用于归一化；可用 __sqrtf) */
static inline float fc_sqrtf(float x) { return sqrtf(x); }

/* ------------- 向量 (3 分量) ------------- */
static inline void v3_set(float *o, float x, float y, float z) { o[0]=x; o[1]=y; o[2]=z; }
static inline void v3_zero(float *o) { o[0]=o[1]=o[2]=0.0f; }
static inline void v3_copy(float *o, const float *a) { o[0]=a[0]; o[1]=a[1]; o[2]=a[2]; }
static inline void v3_add(float *o, const float *a, const float *b) { o[0]=a[0]+b[0]; o[1]=a[1]+b[1]; o[2]=a[2]+b[2]; }
static inline void v3_sub(float *o, const float *a, const float *b) { o[0]=a[0]-b[0]; o[1]=a[1]-b[1]; o[2]=a[2]-b[2]; }
static inline void v3_scale(float *o, const float *a, float s) { o[0]=a[0]*s; o[1]=a[1]*s; o[2]=a[2]*s; }
static inline float v3_dot(const float *a, const float *b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
static inline float v3_norm(const float *a) { return fc_sqrtf(v3_dot(a,a)); }
static inline void v3_cross(float *o, const float *a, const float *b) {
    o[0]=a[1]*b[2]-a[2]*b[1];
    o[1]=a[2]*b[0]-a[0]*b[2];
    o[2]=a[0]*b[1]-a[1]*b[0];
}
static inline void v3_normalize(float *a) {
    float n = v3_norm(a);
    if (n > 1e-15f) { float inv = 1.0f/n; a[0]*=inv; a[1]*=inv; a[2]*=inv; }
}

/* ------------- 四元数 (w,x,y,z) ------------- */
static inline void quat_identity(float *q) { q[0]=1.0f; q[1]=q[2]=q[3]=0.0f; }
static inline void quat_copy(float *o, const float *q) { o[0]=q[0]; o[1]=q[1]; o[2]=q[2]; o[3]=q[3]; }
static inline void quat_normalize(float *q) {
    float n = fc_sqrtf(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
    if (n > 1e-15f) { float inv = 1.0f/n; q[0]*=inv; q[1]*=inv; q[2]*=inv; q[3]*=inv; }
}

/* 四元数乘法: out = a ⊗ b */
static inline void quat_mul(float *out, const float *a, const float *b) {
    float w = a[0]*b[0] - a[1]*b[1] - a[2]*b[2] - a[3]*b[3];
    float x = a[0]*b[1] + a[1]*b[0] + a[2]*b[3] - a[3]*b[2];
    float y = a[0]*b[2] - a[1]*b[3] + a[2]*b[0] + a[3]*b[1];
    float z = a[0]*b[3] + a[1]*b[2] - a[2]*b[1] + a[3]*b[0];
    out[0]=w; out[1]=x; out[2]=y; out[3]=z;
}

/* 用角速度增量更新姿态: out = q ⊗ dq (世界系增稳) */
static inline void quat_integrate(float *q, const float *gyro_rad_dt) {
    /* gyro_rad_dt = 角速度(rad/s) * dt, 机体系 */
    float dq[4];
    dq[0] = 1.0f;
    dq[1] = 0.5f * gyro_rad_dt[0];
    dq[2] = 0.5f * gyro_rad_dt[1];
    dq[3] = 0.5f * gyro_rad_dt[2];
    quat_normalize(dq);
    float tmp[4];
    quat_mul(tmp, q, dq);
    quat_copy(q, tmp);
    quat_normalize(q);
}

/* 四元数 -> 欧拉角 (deg, XYZ) */
void quat_to_euler(const float *q, float *roll, float *pitch, float *yaw);

/* 用四元数旋转向量 v (out = q v q^-1), 机体/世界均可 */
void quat_rotate(const float *q, const float *v, float *out);

#ifdef __cplusplus
}
#endif

#endif /* FC_MATH_H */

/**
  ******************************************************************************
  * @file    fc/fc_types.h
  * @brief   FC_H743 飞控通用数据类型与结构体
  ******************************************************************************
  */
#ifndef FC_TYPES_H
#define FC_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* 板级常量（供结构体/混控使用） */
#define FC_MOTOR_COUNT      4
#define FC_RC_CHANNEL_MAX   8

/* ------------- 标量 ------------- */
typedef float     fc_scalar;
typedef double    fc_double;

/* ------------- 传感器原始数值 ------------- */
typedef struct {
    float accel[3];        /* m/s^2 (body 轴) 或 g */
    float gyro[3];         /* deg/s (body 轴) */
    float temperature;     /* degC */
    float dt;              /* 采样间隔 (s) */
    bool  fresh;           /* 是否有新数据 */
} fc_imu_t;

typedef struct {
    float   field[3];      /* mG（磁场强度 X/Y/Z） */
    float   temperature;   /* degC */
    bool    fresh;
} fc_mag_t;

typedef struct {
    float   pressure_pa;   /* 气压 Pa */
    float   temperature;   /* degC */
    float   pressure_std;  /* 归一化气压（海平面 101325） */
    bool    fresh;
} fc_baro_t;

/* 电源采样 */
typedef struct {
    float   voltage;       /* V */
    float   current;       /* A */
    float   consumed;      /* mAh（累计估算） */
    bool    fresh;
} fc_power_t;

/* ------------- 姿态 / 位置 ------------- */
typedef struct {
    float q[4];            /* 四元数 (w,x,y,z) */
    float euler[3];        /* roll, pitch, yaw (deg) */
    float gyro_body[3];    /* deg/s 机体角速度 */
    float accel_body[3];   /* m/s^2 */
} fc_attitude_t;

typedef struct {
    double lat, lon;              /* deg */
    float  altitude;              /* m (相对) */
    float  heading;               /* deg 航向 */
    float  hdop;                  /* 精度因子 */
    uint8_t num_sats;
    bool   fix_3d;
    bool   fresh;
} fc_gps_t;

/* ------------- 遥控输入 ------------- */
typedef struct {
    float chan[FC_RC_CHANNEL_MAX];  /* 归一化 0..1 (或中心 0.5) */
    bool  connected;
    bool  frame_lost;
    bool  failsafe;
    uint32_t frame_count;
} fc_rc_t;

/* ------------- 飞行模式 ------------- */
typedef enum {
    FC_MODE_ACRO = 0,     /* 手动(增稳无) */
    FC_MODE_ANGLE,        /* 角度自稳 */
    FC_MODE_HORIZON,      /* 自稳+增稳混合 */
    FC_MODE_ALT_HOLD,     /* 定高 */
    FC_MODE_GPS_HOLD,     /* GPS 定点悬停 */
    FC_MODE_RTH,          /* 返航 */
    FC_MODE_COUNT
} fc_flight_mode_t;

/* ------------- 输出 / 混控 ------------- */
typedef struct {
    float motor[FC_MOTOR_COUNT];   /* 0..1000 (us) */
    float servo[4];                /* 备用舵机输出 us */
    uint8_t armed;
} fc_output_t;

/* ------------- PID ------------- */
typedef struct {
    float p, i, d;
    float out;
    float integral;
    float prev_error;
} fc_pid_t;

#ifdef __cplusplus
}
#endif

#endif /* FC_TYPES_H */

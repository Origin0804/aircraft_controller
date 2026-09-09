/**
  ******************************************************************************
  * @file    fc/fc_gps.c
  * @brief   GPS NMEA 解析（GPGGA/GPRMC），串口无关
  * @note    仅解析位置/速度/track/卫星数/高度；串口由上层决定并喂字节。
  ******************************************************************************
  */
#include "fc/fc_gps.h"
#include <stdlib.h>
#include <string.h>

#define NMEA_MAX_LINE   96

static char     nmea_line[NMEA_MAX_LINE];
static uint16_t nmea_pos = 0;
static fc_gps_t g_gps;

/* NMEA 纬度/经度 ddmm.mmmm -> deg */
static float nmea_deg(double ddmm, char ns)
{
    int deg = (int)(ddmm / 100.0);
    float min = (float)(ddmm - deg * 100.0);
    float v = (float)deg + min / 60.0f;
    if (ns == 'S' || ns == 'W') v = -v;
    return v;
}

static void nmea_parse(const char *line)
{
    const char *f[16];
    int n = 0;

    /* 按 ',' 分字段 */
    f[n++] = line;
    for (const char *p = line; *p && n < 16; ++p)
    {
        if (*p == ',') { f[n++] = p + 1; }
    }

    if (n < 3) return;

    if (strncmp(f[0], "GPGGA", 5) == 0 && n >= 10)
    {
        /* f[2]=lat, f[3]=NS, f[4]=lon, f[5]=EW, f[6]=fix, f[7]=sats, f[8]=hdop, f[9]=alt */
        double lat = atof(f[2]);
        double lon = atof(f[4]);
        if (lat > 1.0 && lon > 1.0)
        {
            g_gps.lat = nmea_deg(lat, f[3][0]);
            g_gps.lon = nmea_deg(lon, f[5][0]);
        }
        g_gps.num_sats = (uint8_t)atoi(f[7]);
        g_gps.hdop     = (float)atof(f[8]);
        g_gps.altitude = (float)atof(f[9]);
        g_gps.fix_3d   = (atoi(f[6]) >= 2);
        g_gps.fresh    = true;
    }
    else if (strncmp(f[0], "GPRMC", 5) == 0 && n >= 9)
    {
        /* f[1]=status(A/V), f[2]=lat, f[3]=NS, f[4]=lon, f[5]=EW, f[7]=speed, f[8]=track */
        if (f[1][0] == 'A')
        {
            double lat = atof(f[2]);
            double lon = atof(f[4]);
            if (lat > 1.0 && lon > 1.0)
            {
                g_gps.lat = nmea_deg(lat, f[3][0]);
                g_gps.lon = nmea_deg(lon, f[5][0]);
            }
            g_gps.heading = (float)atof(f[8]);
        }
    }
}

void FC_GPS_Init(void)
{
    nmea_pos = 0;
    memset(&g_gps, 0, sizeof(g_gps));
    nmea_line[0] = '\0';
}

void FC_GPS_OnByte(uint8_t b)
{
    if (b == '$')
    {
        nmea_pos = 0;   /* 新句开始 */
    }
    else if (b == '\r' || b == '\n')
    {
        if (nmea_pos > 0)
        {
            nmea_line[nmea_pos] = '\0';
            nmea_parse(nmea_line);
        }
        nmea_pos = 0;
    }
    else if (nmea_pos < NMEA_MAX_LINE - 1)
    {
        nmea_line[nmea_pos++] = (char)b;
    }
}

void FC_GPS_Get(fc_gps_t *gps)
{
    if (gps) *gps = g_gps;
}

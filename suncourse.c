#include "dark_mode_switcher.h"

#include <math.h>

/*
Based on SUNRISET.C
Released to the public domain by Paul Schlyter, December 1992
Source: https://stjarnhimlen.se/comp/sunriset.c
*/

typedef union {
    FILETIME FtPart;
    ULONGLONG QuadPart;
} FILETIME_QUAD;

#define RADEG (180.0 / 3.14159265358979323846)

static inline double sind(double x)
{
    return sin(x / RADEG);
}

static inline double cosd(double x)
{
    return cos(x / RADEG);
}

static inline double atan2d(double y, double x)
{
    return RADEG * atan2(y, x);
}

static inline double revolution(double x)
{
    return x - 360.0 * floor(x / 360.0);
}

static inline double rev180(double x)
{
    return (x - 360.0 * floor(x / 360.0 + 0.5));
}

static inline double GMST0(double d)
{
    return revolution((180.0 + 356.0470 + 282.9404) + (0.9856002585 + 4.70935E-5) * d);
}

static inline void sunpos(double d, double* lon, double* r)
{
    const double M = revolution(356.0470 + 0.9856002585 * d);
    const double e = 0.016709 - 1.151E-9 * d;
    const double E = M + e * RADEG * sind(M) * (1.0 + e * cosd(M));
    const double x = cosd(E) - e;
    const double y = sqrt(1.0 - e * e) * sind(E);
    *r = sqrt(x * x + y * y);
    const double v = atan2d(y, x);
    const double w = 282.9404 + 4.70935E-5 * d;
    *lon = v + w;
    if (*lon >= 360.0)
        *lon -= 360.0;
}

static inline void sun_RA_dec(double d, double* RA, double* dec, double* r)
{
    double lon;
    sunpos(d, &lon, r);
    double y = *r * sind(lon);
    const double obl_ecl = 23.4393 - 3.563E-7 * d;
    const double z = y * sind(obl_ecl);
    y = y * cosd(obl_ecl);
    const double x = *r * cosd(lon);
    *RA = atan2d(y, x);
    *dec = atan2d(z, sqrt(x * x + y * y));
}

static sun_course calc_delta_values(double* p_rise, double* p_set, double lat, double lon, double d)
{
    const double sidtime = revolution(GMST0(d) + 180.0 + lon);
    double sr, sRA, sdec;
    sun_RA_dec(d, &sRA, &sdec, &sr);
    const double tsouth = 12.0 - rev180(sidtime - sRA) / 15.0;
    const double sradius = 0.2666 / sr;
    const double altit = -35.0 / 60.0 - sradius;
    const double cost = (sind(altit) - sind(lat) * sind(sdec)) / (cosd(lat) * cosd(sdec));
    double td = 0.0; // cost >= 1.0
    int rc = sun_below_horizon;
    if (cost <= -1.0) {
        td = 12.0;
        rc = sun_above_horizon;
    } else if (cost < 1.0) // -1.0 < cost < 1.0
    {
        td = RADEG * acos(cost) / 15.0;
        rc = sun_normal;
    }

    *p_rise = tsouth - td;
    *p_set = tsouth + td;
    return rc;
}

static inline int days_since_2000_Jan_0(int y, int m, int d)
{
    const int a = (m - 14) / 12;
    return (1461 * (y + 4800 + a)) / 4 + (367 * (m - 2 - 12 * a)) / 12 - (3 * ((y + 4900 + a) / 100)) / 4 + d - 2483619;
}

static inline bool hours_to_ftq(FILETIME_QUAD* p_utc, const SYSTEMTIME* p_now, double hours_delta)
{
    SYSTEMTIME st = {.wYear = p_now->wYear, .wMonth = p_now->wMonth, .wDay = p_now->wDay}; // 00:00 of the date
    if (!SystemTimeToFileTime(&st, &p_utc->FtPart))
        return false;

    p_utc->QuadPart += (LONGLONG)(hours_delta * 3.6E10); // offset converted from decimal hours into 100-nanosecond intervals
    return true;
}

static inline bool ftq_to_tz_local(SYSTEMTIME* p_local, const FILETIME_QUAD* p_utc)
{
    SYSTEMTIME st;
    return FileTimeToSystemTime(&p_utc->FtPart, &st) && SystemTimeToTzSpecificLocalTimeEx(NULL, &st, p_local);
}

sun_course get_sun_rise_set(SYSTEMTIME* p_rise, SYSTEMTIME* p_set, SYSTEMTIME* p_rise_next, SYSTEMTIME* p_set_next, double lat, double lon)
{
    SYSTEMTIME now;
    GetSystemTime(&now);
    const double d = days_since_2000_Jan_0(now.wYear, now.wMonth, now.wDay) + 0.5 - lon / 360.0;
    double delta_rise, delta_set;
    sun_course rc = calc_delta_values(&delta_rise, &delta_set, lat, lon, d);
    FILETIME_QUAD ftq_rise, ftq_set;
    if (!hours_to_ftq(&ftq_rise, &now, delta_rise) || !hours_to_ftq(&ftq_set, &now, delta_set))
        return sun_error;

    FILETIME_QUAD ftq_now = {0};
    if (!SystemTimeToFileTime(&now, &ftq_now.FtPart))
        return sun_error;

    const FILETIME_QUAD ftq_tomorrow = {.QuadPart = ftq_now.QuadPart + 864000000000ULL}; // add 1 day as 100-nanosecond intervals
    SYSTEMTIME tomorrow;
    if (!FileTimeToSystemTime(&ftq_tomorrow.FtPart, &tomorrow))
        return sun_error;

    double delta_rise_tomorrow, delta_set_tomorrow;
    const sun_course rc_tomorrow = calc_delta_values(&delta_rise_tomorrow, &delta_set_tomorrow, lat, lon, d + 1);
    FILETIME_QUAD ftq_rise_tomorrow, ftq_set_tomorrow;
    if (!hours_to_ftq(&ftq_rise_tomorrow, &tomorrow, delta_rise_tomorrow) || !hours_to_ftq(&ftq_set_tomorrow, &tomorrow, delta_set_tomorrow))
        return sun_error;

    if (!ftq_to_tz_local(p_rise, &ftq_rise) || !ftq_to_tz_local(p_set, &ftq_set) ||
        !ftq_to_tz_local(p_rise_next, &ftq_rise_tomorrow) || !ftq_to_tz_local(p_set_next, &ftq_set_tomorrow))
        return sun_error;

    if (ftq_now.QuadPart < ftq_set.QuadPart) {
        *p_set_next = *p_set;
        rc |= (rc << 3);
    } else
        rc |= (rc_tomorrow << 3);

    if (ftq_now.QuadPart < ftq_rise.QuadPart)
        *p_rise_next = *p_rise;

    return rc;
}

DWORD get_use_light(const SYSTEMTIME* p_now, const SYSTEMTIME* p_rise, const SYSTEMTIME* p_set)
{
    const int now = p_now->wHour * 10000 + p_now->wMinute * 100 + p_now->wSecond;
    const int rise = p_rise->wHour * 10000 + p_rise->wMinute * 100 + p_rise->wSecond;
    const int set = p_set->wHour * 10000 + p_set->wMinute * 100 + p_set->wSecond;
    return rise <= now && now < set;
}

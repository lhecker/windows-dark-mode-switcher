#include "suncourse.h"

#include <math.h>

/*
Based on SUNRISET.C
Released to the public domain by Paul Schlyter, December 1992
Source: https://stjarnhimlen.se/comp/sunriset.c
*/

#define RADEG (180.0 / 3.14159265358979323846)

static double sind(double x)
{
    return sin(x / RADEG);
}

static double cosd(double x)
{
    return cos(x / RADEG);
}

static double atan2d(double y, double x)
{
    return RADEG * atan2(y, x);
}

static double revolution(double x)
{
    return x - 360.0 * floor(x / 360.0);
}

static double rev180(double x)
{
    return (x - 360.0 * floor(x / 360.0 + 0.5));
}

static double GMST0(double d)
{
    return revolution((180.0 + 356.0470 + 282.9404) + (0.9856002585 + 4.70935E-5) * d);
}

static void sunpos(double d, double* lon, double* r)
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

static void sun_RA_dec(double d, double* RA, double* dec, double* r)
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

static void calc_delta_values(double* p_rise, double* p_set, double lat, double lon, double d)
{
    const double sidtime = revolution(GMST0(d) + 180.0 + lon);
    double sr, sRA, sdec;
    sun_RA_dec(d, &sRA, &sdec, &sr);
    const double tsouth = 12.0 - rev180(sidtime - sRA) / 15.0;
    const double sradius = 0.2666 / sr;
    const double altit = -35.0 / 60.0 - sradius;
    const double cost = (sind(altit) - sind(lat) * sind(sdec)) / (cosd(lat) * cosd(sdec));

    if (cost <= -1.0) {
        // sun is always above horizon
        *p_rise = 0;
        *p_set = 24;
    } else if (cost >= 1.0) {
        // sun is always below horizon
        *p_rise = 0;
        *p_set = 0;
    } else {
        double td = RADEG * acos(cost) / 15.0;
        *p_rise = tsouth - td;
        *p_set = tsouth + td;
    }
}

static int days_since_2000_01_01(FILETIME_QUAD ft)
{
    const ULONGLONG intervalsTo2000 = 125911584000000000ULL;
    const ULONGLONG intervalsPerDay = 864000000000ULL;
    ULONGLONG intervalsSince2000 = ft.QuadPart - intervalsTo2000;
    return (int)(intervalsSince2000 / intervalsPerDay);
}

bool suncourse_is_daytime(float lat, float lon, FILETIME_QUAD* next_update)
{
    SYSTEMTIME now;
    GetSystemTime(&now);

    FILETIME_QUAD now_ft = {};
    SystemTimeToFileTime(&now, &now_ft.FtPart);

    // FILETIME contains the 100-nanosecond intervals since January 1, 1601 (UTC).
    // Convert it to days since January 0, 2000 (UTC).
    const double d = days_since_2000_01_01(now_ft) + 0.5 - lon / 360.0;

    double rise_hours, set_hours;
    calc_delta_values(&rise_hours, &set_hours, lat, lon, d);

    const double now_h = now.wHour + now.wMinute / 60.0 + now.wSecond / 3600.0;
    const bool is_daytime = rise_hours <= now_h && now_h < set_hours;

    // If it's daytime, the next change is the sunset and vice versa.
    double next_h = is_daytime ? set_hours : rise_hours;
    // Add 30 seconds as wiggle room for the timer.
    next_h += 30.0 / 3600.0;
    // Ensure it's [0, 24).
    next_h = fmod(next_h, 24.0);

    double hours;
    double minutes = modf(next_h, &hours) * 60;

    SYSTEMTIME next = now;
    next.wHour = (WORD)lround(hours);
    next.wMinute = (WORD)lround(minutes);
    next.wSecond = 0;
    next.wMilliseconds = 0;

    FILETIME_QUAD next_ft = {};
    SystemTimeToFileTime(&next, &next_ft.FtPart);

    // If the time ended before `now` it's tomorrow.
    if (next_ft.QuadPart < now_ft.QuadPart) {
        next_ft.QuadPart += 864000000000;
    }

    *next_update = next_ft;
    return is_daytime;
}

#include "suncourse.h"

#include <assert.h>
#include <math.h>

static double rad(double x)
{
    return x * 0.017453292519943295;
}

static double deg(double x)
{
    return x * 57.295779513082320876;
}

typedef struct SunsetSunrise {
    double sunrise;
    double sunset;
} SunsetSunrise;

// These formulas are based on the NOAA Solar Calculator:
//   https://www.esrl.noaa.gov/gmd/grad/solcalc/calcdetails.html
// It in turn is based on the methods described in:
//   Astronomical Algorithms, by Jean Meeus
// It's used here, because it appears as if most applications use this formula nowadays.
// I've mostly converted it to use radians instead of degrees,
// but it could probably still be cleaned up further.
static SunsetSunrise noaa_sunset_sunrise(double lat, double lon, double julian_day)
{
    double julian_century = (julian_day - 2451545) / 36525;
    double geom_mean_long_sun = rad(fmod(280.46646 + julian_century * (36000.76983 + julian_century * 0.0003032), 360));
    double geom_mean_anom_sun = rad(357.52911 + julian_century * (35999.05029 - 0.0001537 * julian_century));
    double eccent_earth_orbit = 0.016708634 - julian_century * (0.000042037 + 0.0000001267 * julian_century);
    double sun_eq_of_ctr = sin(geom_mean_anom_sun) * rad(1.914602 - julian_century * (0.004817 + 0.000014 * julian_century)) + sin(2 * geom_mean_anom_sun) * rad(0.019993 - 0.000101 * julian_century) + sin(3 * geom_mean_anom_sun) * rad(0.000289);
    double sun_true_long = geom_mean_long_sun + sun_eq_of_ctr;
    double sun_app_long = sun_true_long - rad(0.00569) - rad(0.00478) * sin(rad(125.04 - 1934.136 * julian_century));
    double mean_obliq_ecliptic = rad(23 + (26 + (21.448 - julian_century * (46.815 + julian_century * (0.00059 - julian_century * 0.001813))) / 60) / 60);
    double obliq_corr = mean_obliq_ecliptic + rad(0.00256) * cos(rad(125.04 - 1934.136 * julian_century));
    double sun_declin = asin(sin(obliq_corr) * sin(sun_app_long));
    double var_y = tan(obliq_corr / 2) * tan(obliq_corr / 2);
    double eq_of_time = 4 * deg(var_y * sin(2 * geom_mean_long_sun) - 2 * eccent_earth_orbit * sin(geom_mean_anom_sun) + 4 * eccent_earth_orbit * var_y * sin(geom_mean_anom_sun) * cos(2 * geom_mean_long_sun) - 0.5 * var_y * var_y * sin(4 * geom_mean_long_sun) - 1.25 * eccent_earth_orbit * eccent_earth_orbit * sin(2 * geom_mean_anom_sun));
    double ha_sunrise = deg(acos(cos(rad(90.833)) / (cos(rad(lat)) * cos(sun_declin)) - tan(rad(lat)) * tan(sun_declin)));
    double solar_noon = (720 - 4 * lon - eq_of_time) / 60;
    double sunrise = solar_noon - ha_sunrise * 4 / 60;
    double sunset = solar_noon + ha_sunrise * 4 / 60;
    return (SunsetSunrise){sunrise, sunset};
}

bool suncourse_is_daytime(float lat, float lon, FILETIME_QUAD* next_update)
{
    FILETIME_QUAD now = {};
    GetSystemTimeAsFileTime(&now.FtPart);

    SYSTEMTIME now_st;
    FileTimeToSystemTime(&now.FtPart, &now_st);

    const ULONGLONG days_since_1601 = now.QuadPart / 864000000000;
    // 2305813.5 is the Julian Day of 1601-01-01 00:00:00 UTC.
    const double julian_day = days_since_1601 + 2305813.5;

    const SunsetSunrise ss = noaa_sunset_sunrise(lat, lon, julian_day);
    const double now_h = now_st.wHour + now_st.wMinute / 60.0 + now_st.wSecond / 3600.0;
    const bool is_daytime = ss.sunrise <= now_h && now_h < ss.sunset;

    // If it's daytime, the next change is the sunset and vice versa.
    double next_h = is_daytime ? ss.sunset : ss.sunrise;
    // Add 30 seconds as wiggle room for the timer.
    next_h += 30.0 / 3600.0;
    // Ensure it's [0, 24).
    next_h = fmod(next_h, 24.0);

    double hours;
    double minutes = modf(next_h, &hours) * 60;

    SYSTEMTIME next_st = now_st;
    next_st.wHour = (WORD)lround(hours);
    next_st.wMinute = (WORD)lround(minutes);
    next_st.wSecond = 0;
    next_st.wMilliseconds = 0;

    FILETIME_QUAD next = {};
    SystemTimeToFileTime(&next_st, &next.FtPart);

    // If the time ended before `now` it's tomorrow.
    if (next.QuadPart < now.QuadPart) {
        next.QuadPart += 864000000000;
    }

    *next_update = next;
    return is_daytime;
}

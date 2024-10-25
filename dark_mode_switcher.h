#ifndef HEADER_DARK_MODE_SWITCHER_025E8CEE_239D_451E_AA1C_F6DC3F5CCA4E_1_0
#define HEADER_DARK_MODE_SWITCHER_025E8CEE_239D_451E_AA1C_F6DC3F5CCA4E_1_0

/// @file dark_mode_switcher.h

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <stdbool.h>
#include <stddef.h>

/// @brief Convert a string to time values.
///
/// @details The function only updates the wHour and wMinute members.
///
/// @param p_time  Pointer to a SYSTEMTIME object that receives the time values..
/// @param str     String that represents a time in HH:mm format.
///
/// @return If the function succeeds `true` is returned, `false` otherwise.
bool string_to_time(SYSTEMTIME* p_time, const wchar_t* str);

/// @brief Convert a string to a double that represents a geographic coordinate.
///
/// @param p_deg  Pointer to a double that receives the converted value.
/// @param str    String that represents a coordinate in decimal degrees.
/// @param hi     Upper limit of allowed values.
/// @param lo     Lower limit of allowed values.
///
/// @return If the function succeeds `true` is returned, `false` otherwise.
bool string_to_coordinate(double* p_deg, const wchar_t* str, double hi, double lo);

/// @brief Convert time values to a string of format "YYYY-MM-DDTHH:mm:SS".
///
/// @details The format of the string meets the requirements for scheduled tasks.
///
/// @param buf       Buffer that receives the converted string.
/// @param capacity  Size of the buffer in characters.
/// @param p_time    Pointer to a SYSTEMTIME object that contains the time values.
///
/// @return Length of the converted string. The return value is 0 if the capacity < 20.
size_t time_to_string(wchar_t* buf, size_t capacity, const SYSTEMTIME* p_time);

/// @brief Flags for the return code of `get_sun_rise_set()`.
typedef enum {
    sun_error = 0x00, // the function failed due to a conversion error
    // if the function succeeds the return code consists of a value for today's sun course ...
    sun_normal = 0x01,        // position with sunrise and sunset at this time
    sun_above_horizon = 0x02, // position with no sunset in polar summer at this time
    sun_below_horizon = 0x04, // position with no sunrise in polar winter at this time
    // ... and a value for the upcoming sun course
    sun_normal_next = 0x08,        // position with sunrise and sunset next time
    sun_above_horizon_next = 0x10, // position with no sunset in polar summer next time
    sun_below_horizon_next = 0x20  // position with no sunrise in polar winter next time
} sun_course;

/// @brief Calculate sunrise and sunset times.
///
/// @details The received values are localized to the active time zone.
///          Note: `..._next` parameters receive whatever comes after the current time,
///          this can still be one or both of today's values.
///
/// @param p_rise       [out] Pointer to a SYSTEMTIME object that receives today's sunrise values.
/// @param p_set        [out] Pointer to a SYSTEMTIME object that receives today's sunset values.
/// @param p_rise_next  [out] Pointer to a SYSTEMTIME object that receives values for the upcoming sunrise.
/// @param p_set_next   [out] Pointer to a SYSTEMTIME object that receives values for the upcoming sunset.
/// @param lat          [in] Latitude of the user's position in decimal degrees.
/// @param lon          [in] Longitude of the user's position in decimal degrees.
///
/// @return Combination of the `sun_course` flags.
sun_course get_sun_rise_set(SYSTEMTIME* p_rise, SYSTEMTIME* p_set, SYSTEMTIME* p_rise_next, SYSTEMTIME* p_set_next, double lat, double lon);

/// @brief Calculate whether the current time is within daylight hours.
///
/// @details The function returns a value that can be used to update `AppsUseLightTheme`
///          and `SystemUsesLightTheme` registry settings.
///
/// @param p_now   Pointer to a SYSTEMTIME object that contains the current local time values.
/// @param p_rise  Pointer to a SYSTEMTIME object that contains today's local sunrise values.
/// @param p_set   Pointer to a SYSTEMTIME object that contains today's local sunset values.
///
/// @return 1 if the current time is within daylight hours, 0 otherwise.
DWORD get_use_light(const SYSTEMTIME* p_now, const SYSTEMTIME* p_rise, const SYSTEMTIME* p_set);

#endif // header guard

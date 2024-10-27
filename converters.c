#include "dark_mode_switcher.h"

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

bool string_to_time(SYSTEMTIME* p_time, const wchar_t* str)
{
    if (wcslen(str) != 5 || str[2] != L':') {
        return false;
    }

    p_time->wHour = (WORD)((str[0] - L'0') * 10 + (str[1] - L'0'));
    p_time->wMinute = (WORD)((str[3] - L'0') * 10 + (str[4] - L'0'));
    if (p_time->wHour >= 24 || p_time->wMinute >= 60) {
        return false;
    }

    return true;
}

bool string_to_coordinate(double* p_deg, const wchar_t* str, double hi, double lo)
{
    errno = 0;
    wchar_t* endptr;
    const double deg = wcstod(str, &endptr);
    if (errno || *endptr)
        return false;

    const int fpclass = fpclassify(deg);
    if ((fpclass != FP_NORMAL && fpclass != FP_ZERO) || deg > hi || deg < lo)
        return false;

    *p_deg = deg;
    return true;
}

static inline void digits_to_substring(wchar_t* dest, int num, int len)
{
    while (len--) {
        const int tenth = num / 10;
        dest[len] = (wchar_t)(num - tenth * 10 + L'0');
        num = tenth;
    }
}

size_t time_to_string(wchar_t* buf, size_t capacity, const SYSTEMTIME* p_time)
{
    if (capacity < 20) {
        buf[0] = L'\0';
        return 0;
    }

    digits_to_substring(buf, p_time->wYear, 4);
    buf[4] = L'-';
    digits_to_substring(buf + 5, p_time->wMonth, 2);
    buf[7] = L'-';
    digits_to_substring(buf + 8, p_time->wDay, 2);
    buf[10] = L'T';
    digits_to_substring(buf + 11, p_time->wHour, 2);
    buf[13] = L':';
    digits_to_substring(buf + 14, p_time->wMinute, 2);
    buf[16] = L':';
    digits_to_substring(buf + 17, p_time->wSecond, 2);
    buf[19] = L'\0';

    return 19;
}

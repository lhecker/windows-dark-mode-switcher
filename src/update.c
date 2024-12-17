#include "update.h"

#include "settings.h"
#include "suncourse.h"

static HANDLE s_timer;

static bool custom_is_daytime(FILETIME_QUAD* next_update)
{
    FILETIME_QUAD now_ft = {};
    GetSystemTimeAsFileTime(&now_ft.FtPart);

    SYSTEMTIME now;
    FileTimeToSystemTime(&now_ft.FtPart, &now);
    SystemTimeToTzSpecificLocalTimeEx(NULL, &now, &now);

    const DWORD time = now.wHour * 100 + now.wMinute;
    const bool is_daytime = time >= s_settings.sunrise && time < s_settings.sunset;
    const DWORD next_time = is_daytime ? s_settings.sunset : s_settings.sunrise;

    SYSTEMTIME next = now;
    next.wHour = (WORD)(next_time / 100);
    next.wMinute = (WORD)(next_time % 100);
    next.wSecond = 0;
    next.wMilliseconds = 0;

    FILETIME_QUAD next_ft = {};
    TzSpecificLocalTimeToSystemTimeEx(NULL, &next, &next);
    SystemTimeToFileTime(&next, &next_ft.FtPart);

    // If the time ended before `now` it's tomorrow.
    if (next_ft.QuadPart < now_ft.QuadPart) {
        next_ft.QuadPart += 864000000000;
    }

    *next_update = next_ft;
    return is_daytime;
}

static void update_system(DWORD light)
{
    DWORD app_light = 0;
    DWORD system_light = 0;
    DWORD length;

    length = sizeof(app_light);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", RRF_RT_REG_DWORD | RRF_ZEROONFAILURE, NULL, &app_light, &length);
    length = sizeof(system_light);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"SystemUsesLightTheme", RRF_RT_REG_DWORD | RRF_ZEROONFAILURE, NULL, &system_light, &length);

    if (app_light != light || system_light != light) {
        RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", REG_DWORD, &light, sizeof(light));
        RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"SystemUsesLightTheme", REG_DWORD, &light, sizeof(light));
        SendNotifyMessageW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet");
    }
}

static void WINAPI timer_callback(LPVOID arg, DWORD timer_low, DWORD timer_high)
{
    update_run(UpdateOverride_None);
}

void update_init()
{
    s_timer = CreateWaitableTimerExW(NULL, NULL, 0, TIMER_ALL_ACCESS);
}

void update_run(UpdateOverride override)
{
    FILETIME_QUAD next_update = {};

    switch (override) {
    case UpdateOverride_None:
        switch (s_settings.switching_type) {
        case SettingsSwitchingType_Disabled:
            break;
        case SettingsSwitchingType_Custom:
            update_system(custom_is_daytime(&next_update));
            break;
        case SettingsSwitchingType_Geographic:
            update_system(suncourse_is_daytime(s_settings.latitude, s_settings.longitude, &next_update));
            break;
        }
        break;
    case UpdateOverride_Light:
        update_system(1);
        break;
    case UpdateOverride_Dark:
        update_system(0);
        break;
    }

    if (next_update.QuadPart) {
        SetWaitableTimer(s_timer, (LARGE_INTEGER*)&next_update, 0, timer_callback, NULL, FALSE);
    } else {
        CancelWaitableTimer(s_timer);
    }
}

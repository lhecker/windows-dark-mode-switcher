#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define IFR_WIN32(x)                        \
    do {                                    \
        DWORD err = (x);                    \
        if (err != 0) {                     \
            return HRESULT_FROM_WIN32(err); \
        }                                   \
    } while (0)

#define IFR_WIN32_LRESULT(x)                           \
    do {                                               \
        LRESULT ok = (x);                              \
        if (!ok) {                                     \
            return HRESULT_FROM_WIN32(GetLastError()); \
        }                                              \
    } while (0)

DWORD mainCRTStartup(void)
{
    DWORD light = 0;
    DWORD size = sizeof(light);
    IFR_WIN32(RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"SystemUsesLightTheme", RRF_RT_REG_DWORD | RRF_ZEROONFAILURE, NULL, &light, &size));

    light = !light;
    IFR_WIN32(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", REG_DWORD, &light, sizeof(light)));
    IFR_WIN32(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"SystemUsesLightTheme", REG_DWORD, &light, sizeof(light)));

    IFR_WIN32_LRESULT(SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet", SMTO_ABORTIFHUNG, 5000, NULL));
    return 0;
}

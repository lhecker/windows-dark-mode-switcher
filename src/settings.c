#include "settings.h"

#include <CommCtrl.h>
#include <roapi.h>
#include <windows.devices.geolocation.h>

#include <wchar.h>

#include "menu.h"
#include "resource.h"
#include "update.h"
#include "winrt_helpers.h"

static const GUID IID_IGeolocator = {0xA9C3BF62, 0x4524, 0x4989, {0x8A, 0xA9, 0xDE, 0x01, 0x9D, 0x2E, 0x55, 0x1F}};                 // A9C3BF62-4524-4989-8AA9-DE019D2E551F
static const GUID IID_IGeolocator2 = {0xD1B42E6D, 0x8891, 0x43B4, {0xAD, 0x36, 0x27, 0xC6, 0xFE, 0x9A, 0x97, 0xB1}};                // D1B42E6D-8891-43B4-AD36-27C6FE9A97B1
static const GUID IID_IAsyncOperation_Geoposition = {0x7668A704, 0x244E, 0x5E12, {0x8D, 0xCB, 0x92, 0xA3, 0x29, 0x9E, 0xBA, 0x26}}; // 7668A704-244E-5E12-8DCB-92A3299EBA26

static HWND s_hwnd_settings;
Settings s_settings;

static DWORD reg_read_dword(HKEY key, const wchar_t* name, DWORD default_value)
{
    if (!key) {
        return default_value;
    }

    DWORD value;
    DWORD size = sizeof(value);
    if (RegGetValueW(key, NULL, name, RRF_RT_REG_DWORD, NULL, &value, &size) != ERROR_SUCCESS) {
        return default_value;
    }

    return value;
}

static void reg_write_dword(HKEY key, const wchar_t* name, DWORD value)
{
    RegSetValueExW(key, name, 0, REG_DWORD, (const BYTE*)&value, sizeof(value));
}

static float reg_read_float(HKEY key, const wchar_t* name, float default_value)
{
    if (!key) {
        return default_value;
    }

    float value = 0.f;
    DWORD size = sizeof(value);
    if (RegGetValueW(key, NULL, name, RRF_RT_REG_BINARY, NULL, &value, &size) != ERROR_SUCCESS) {
        return default_value;
    }

    return value;
}

static void reg_write_float(HKEY key, const wchar_t* name, float value)
{
    RegSetValueExW(key, name, 0, REG_BINARY, (const BYTE*)&value, sizeof(value));
}

static DWORD sanitize_time(DWORD time)
{
    DWORD hour = time / 100;
    DWORD minute = time % 100;
    hour = min(hour, 23);
    minute = min(minute, 59);
    return hour * 100 + minute;
}

static void set_dlg_item_float(HWND hwnd, int item, float value)
{
    wchar_t buffer[64];
    swprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), L"%.6f", value);
    SetDlgItemTextW(hwnd, item, &buffer[0]);
}

static void set_dlg_item_time(HWND hwnd, int item, SYSTEMTIME* local_time, DWORD time)
{
    hwnd = GetDlgItem(hwnd, item);
    local_time->wHour = (WORD)(time / 100);
    local_time->wMinute = (WORD)(time % 100);
    local_time->wSecond = 0;
    local_time->wMilliseconds = 0;
    DateTime_SetFormat(hwnd, L"HH:mm");
    DateTime_SetSystemtime(hwnd, GDT_VALID, local_time);
}

void settings_init()
{
    HKEY key;
    RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\DarkModeSwitcher", 0, KEY_READ, &key);

    s_settings.switching_type = reg_read_dword(key, L"SwitchingType", SettingsSwitchingType_Disabled);
    s_settings.sunrise = reg_read_dword(key, L"Sunrise", 600);
    s_settings.sunset = reg_read_dword(key, L"Sunset", 1800);
    s_settings.latitude = reg_read_float(key, L"Latitude", 41.892090f);
    s_settings.longitude = reg_read_float(key, L"Longitude", 12.486438f);

    if (key) {
        RegCloseKey(key);
    }

    s_settings.switching_type = min(s_settings.switching_type, SettingsSwitchingType_Geographic);
    s_settings.sunrise = sanitize_time(s_settings.sunrise);
    s_settings.sunset = sanitize_time(s_settings.sunset);
    s_settings.latitude = clamp(s_settings.latitude, -90.0f, 90.0f);
    s_settings.longitude = clamp(s_settings.longitude, -180.0f, 180.0f);
}

static void save_settings()
{
    HKEY key;
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\DarkModeSwitcher", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
    if (!key) {
        return;
    }

    reg_write_dword(key, L"SwitchingType", s_settings.switching_type);
    reg_write_dword(key, L"Sunrise", s_settings.sunrise);
    reg_write_dword(key, L"Sunset", s_settings.sunset);
    reg_write_float(key, L"Longitude", s_settings.longitude);
    reg_write_float(key, L"Latitude", s_settings.latitude);

    RegCloseKey(key);
}

static void apply_settings_to_controls(HWND hwnd)
{
    SYSTEMTIME time;
    GetLocalTime(&time);

    CheckDlgButton(hwnd, IDC_ENABLE_AUTOMATIC_SWITCHING, s_settings.switching_type != SettingsSwitchingType_Disabled);
    CheckRadioButton(hwnd, IDC_ENABLE_CUSTOM_HOURS, IDC_ENABLE_GEOGRAPHIC, s_settings.switching_type == SettingsSwitchingType_Geographic ? IDC_ENABLE_GEOGRAPHIC : IDC_ENABLE_CUSTOM_HOURS);
    set_dlg_item_time(hwnd, IDC_SUNRISE, &time, s_settings.sunrise);
    set_dlg_item_time(hwnd, IDC_SUNSET, &time, s_settings.sunset);
    set_dlg_item_float(hwnd, IDC_LONGITUDE, s_settings.longitude);
    set_dlg_item_float(hwnd, IDC_LATITUDE, s_settings.latitude);
}

static void apply_controls_to_settings(HWND hwnd)
{
    if (!IsDlgButtonChecked(hwnd, IDC_ENABLE_AUTOMATIC_SWITCHING)) {
        s_settings.switching_type = SettingsSwitchingType_Disabled;
    } else if (IsDlgButtonChecked(hwnd, IDC_ENABLE_GEOGRAPHIC)) {
        s_settings.switching_type = SettingsSwitchingType_Geographic;
    } else {
        s_settings.switching_type = SettingsSwitchingType_Custom;
    }

    SYSTEMTIME time;
    wchar_t buffer[64];

    DateTime_GetSystemtime(GetDlgItem(hwnd, IDC_SUNRISE), &time);
    s_settings.sunrise = time.wHour * 100 + time.wMinute;
    DateTime_GetSystemtime(GetDlgItem(hwnd, IDC_SUNSET), &time);
    s_settings.sunset = time.wHour * 100 + time.wMinute;
    GetDlgItemTextW(hwnd, IDC_LONGITUDE, buffer, ARRAYSIZE(buffer));
    s_settings.longitude = wcstof(buffer, NULL);
    GetDlgItemTextW(hwnd, IDC_LATITUDE, buffer, ARRAYSIZE(buffer));
    s_settings.latitude = wcstof(buffer, NULL);

    if (s_settings.sunrise > s_settings.sunset) {
        DWORD tmp = s_settings.sunrise;
        s_settings.sunrise = s_settings.sunset;
        s_settings.sunset = tmp;
    }
}

static void update_enabled_disabled_dialog_items(HWND hwnd)
{
    const BOOL enabled = IsDlgButtonChecked(hwnd, IDC_ENABLE_AUTOMATIC_SWITCHING);
    const BOOL custom = enabled && IsDlgButtonChecked(hwnd, IDC_ENABLE_CUSTOM_HOURS);
    const BOOL geographic = enabled && IsDlgButtonChecked(hwnd, IDC_ENABLE_GEOGRAPHIC);

    EnableWindow(GetDlgItem(hwnd, IDC_ENABLE_CUSTOM_HOURS), enabled);
    EnableWindow(GetDlgItem(hwnd, IDC_ENABLE_GEOGRAPHIC), enabled);
    EnableWindow(GetDlgItem(hwnd, IDC_SUNRISE), custom);
    EnableWindow(GetDlgItem(hwnd, IDC_SUNSET), custom);
    EnableWindow(GetDlgItem(hwnd, IDC_LONGITUDE), geographic);
    EnableWindow(GetDlgItem(hwnd, IDC_LATITUDE), geographic);
    EnableWindow(GetDlgItem(hwnd, IDC_GEOLOCATION_USE_CURRENT), geographic);
}

static HRESULT geolocation_callback(void* context, __FIAsyncOperation_1_Windows__CDevices__CGeolocation__CGeoposition* operation, AsyncStatus status)
{
    if (status != Completed) {
        return S_OK;
    }

    __x_ABI_CWindows_CDevices_CGeolocation_CIGeoposition* location = NULL;
    __x_ABI_CWindows_CDevices_CGeolocation_CIGeocoordinate* coord = NULL;
    double latitude, longitude;
    HRESULT hr = S_OK;

    IFC(operation->lpVtbl->GetResults(operation, &location));
    IFC(location->lpVtbl->get_Coordinate(location, &coord));
    IFC(coord->lpVtbl->get_Latitude(coord, &latitude));
    IFC(coord->lpVtbl->get_Longitude(coord, &longitude));

    float coordinates[2] = {(float)latitude, (float)longitude};
    SendMessageW(context, WM_GEOLOCATION_UPDATED, 0, (LPARAM)&coordinates);

cleanup:
    SAFE_RELEASE(coord);
    SAFE_RELEASE(location);
    return hr;
}

static void update_geolocation(HWND hwnd)
{
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        RoInitialize(RO_INIT_SINGLETHREADED);
    }

    IInspectable* geolocator_inspectable = NULL;
    __x_ABI_CWindows_CDevices_CGeolocation_CIGeolocator* geolocator = NULL;
    __x_ABI_CWindows_CDevices_CGeolocation_CIGeolocator2* geolocator2 = NULL;
    __FIAsyncOperation_1_Windows__CDevices__CGeolocation__CGeoposition* async_op = NULL;
    HSTRING_HEADER header;
    HRESULT hr = S_OK;

    IFC(RoActivateInstance(hstring_reference(&header, RuntimeClass_Windows_Devices_Geolocation_Geolocator), &geolocator_inspectable));
    IFC(geolocator_inspectable->lpVtbl->QueryInterface(geolocator_inspectable, &IID_IGeolocator, (void**)&geolocator));
    IFC(geolocator_inspectable->lpVtbl->QueryInterface(geolocator_inspectable, &IID_IGeolocator2, (void**)&geolocator2));

    IFC(geolocator2->lpVtbl->AllowFallbackToConsentlessPositions(geolocator2));

    IFC(geolocator->lpVtbl->GetGeopositionAsync(geolocator, &async_op));
    IFC(async_op->lpVtbl->put_Completed(async_op, create_wrapper_for_IAsyncOperationCompletedHandler(IID_IAsyncOperation_Geoposition, geolocation_callback, hwnd)));

cleanup:
    SAFE_RELEASE(async_op);
    SAFE_RELEASE(geolocator2);
    SAFE_RELEASE(geolocator);
    SAFE_RELEASE(geolocator_inspectable);
}

static INT_PTR settings_dialog_callback(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_INITDIALOG:
        apply_settings_to_controls(hwnd);
        update_enabled_disabled_dialog_items(hwnd);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wparam)) {
        case IDOK:
            apply_controls_to_settings(hwnd);
            save_settings();
            menu_apply_override(UpdateOverride_None);
            [[fallthrough]];
        case IDCANCEL:
            s_hwnd_settings = NULL;
            DestroyWindow(hwnd);
            return TRUE;
        case IDC_ENABLE_AUTOMATIC_SWITCHING:
        case IDC_ENABLE_CUSTOM_HOURS:
        case IDC_ENABLE_GEOGRAPHIC:
            update_enabled_disabled_dialog_items(hwnd);
            return TRUE;
        case IDC_GEOLOCATION_USE_CURRENT:
            EnableWindow(GetDlgItem(hwnd, IDC_GEOLOCATION_USE_CURRENT), FALSE);
            update_geolocation(hwnd);
            return TRUE;
        default:
            return FALSE;
        }
    case WM_GEOLOCATION_UPDATED: {
        float* coordinates = (float*)lparam;
        set_dlg_item_float(hwnd, IDC_LATITUDE, coordinates[0]);
        set_dlg_item_float(hwnd, IDC_LONGITUDE, coordinates[1]);
        EnableWindow(GetDlgItem(hwnd, IDC_GEOLOCATION_USE_CURRENT), TRUE);
        return 0;
    }
    default:
        return FALSE;
    }
}

void settings_show_dialog(HWND hwnd)
{
    if (s_hwnd_settings) {
        SetForegroundWindow(s_hwnd_settings);
    } else {
        s_hwnd_settings = CreateDialogParamW(NULL, MAKEINTRESOURCEW(IDD_SETTINGS), hwnd, &settings_dialog_callback, 0);
        ShowWindow(s_hwnd_settings, SW_SHOW);
    }
}

bool settings_dialog_dispatch(MSG* msg)
{
    return s_hwnd_settings ? IsDialogMessageW(s_hwnd_settings, msg) : false;
}

#include "menu.h"

#include "resource.h"
#include "settings.h"
#include "update.h"

#include <shellapi.h>

static HMENU s_menu;
static NOTIFYICONDATAW s_notification_data = {
    .cbSize = sizeof(NOTIFYICONDATAW),
    .uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP,
    .uCallbackMessage = WM_NOTIFICATION_ICON_CALLBACK,
    .szTip = L"Dark Mode Switcher",
    .uVersion = NOTIFYICON_VERSION_4
};
static UINT s_wm_taskbar_created;
static int s_last_update_override = -1;

static int map_override_to_item(UpdateOverride override)
{
    switch (override) {
    case UpdateOverride_Light:
        return ID_CONTEXTMENU_FORCELIGHTMODE;
    case UpdateOverride_Dark:
        return ID_CONTEXTMENU_FORCEDARKMODE;
    default:
        return ID_CONTEXTMENU_SWITCHAUTOMATICALLY;
    }
}

void menu_apply_override(UpdateOverride override)
{
    if (s_last_update_override >= 0) {
        CheckMenuItem(s_menu, map_override_to_item(s_last_update_override), MF_BYCOMMAND | MF_UNCHECKED);
    }
    CheckMenuItem(s_menu, map_override_to_item(override), MF_BYCOMMAND | MF_CHECKED);
    s_last_update_override = override;
    update_run(override);
}

static LRESULT CALLBACK window_callback(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_NOTIFICATION_ICON_CALLBACK:
        switch (LOWORD(lparam)) {
        case WM_CONTEXTMENU: {
            const bool right_align = GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0;
            UINT flags = TPM_RIGHTBUTTON;
            flags |= right_align ? TPM_RIGHTALIGN : TPM_LEFTALIGN;

            SetForegroundWindow(hwnd);
            TrackPopupMenuEx(s_menu, flags, LOWORD(wparam), HIWORD(wparam), hwnd, NULL);
            PostMessageW(hwnd, WM_NULL, 0, 0);
            break;
        }
        default:
            break;
        }
        return 0;
    case WM_COMMAND: {
        switch (LOWORD(wparam)) {
        case ID_CONTEXTMENU_SWITCHAUTOMATICALLY:
            if (s_settings.switching_type == SettingsSwitchingType_Disabled) {
                settings_show_dialog(hwnd);
            } else {
                menu_apply_override(UpdateOverride_None);
            }
            break;
        case ID_CONTEXTMENU_FORCEDARKMODE:
            menu_apply_override(UpdateOverride_Dark);
            break;
        case ID_CONTEXTMENU_FORCELIGHTMODE:
            menu_apply_override(UpdateOverride_Light);
            break;
        case ID_CONTEXTMENU_SETTINGS:
            settings_show_dialog(hwnd);
            return 0;
        case ID_CONTEXTMENU_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        default:
            return 0;
        }
    }
    case WM_TIMECHANGE:
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        if (message == s_wm_taskbar_created) {
            Shell_NotifyIconW(NIM_ADD, &s_notification_data);
            Shell_NotifyIconW(NIM_SETVERSION, &s_notification_data);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

void menu_init(HINSTANCE instance)
{
    const WNDCLASSEXW wcex = {
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
        .lpfnWndProc = window_callback,
        .cbWndExtra = sizeof(void*),
        .hInstance = instance,
        .hIcon = LoadIconW(instance, MAKEINTRESOURCE(IDI_ICON1)),
        .hCursor = LoadCursorW(instance, IDC_ARROW),
        .lpszClassName = L"Dark Mode Switcher",
    };
    RegisterClassExW(&wcex);

    const HWND hwnd = CreateWindowExW(
        0,
        L"Dark Mode Switcher",
        L"Dark Mode Switcher",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        NULL,
        NULL,
        instance,
        NULL
    );

    s_menu = GetSubMenu(LoadMenuW(NULL, MAKEINTRESOURCEW(IDR_MENU1)), 0);
    s_wm_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");

    s_notification_data.hWnd = hwnd;
    s_notification_data.hIcon = wcex.hIcon;
    Shell_NotifyIconW(NIM_ADD, &s_notification_data);
    Shell_NotifyIconW(NIM_SETVERSION, &s_notification_data);
}

void menu_deinit()
{
    Shell_NotifyIconW(NIM_DELETE, &s_notification_data);
}

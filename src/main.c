#include "menu.h"
#include "settings.h"
#include "update.h"

#include <Uxtheme.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance, PWSTR cmd_line, int cmd_show)
{
    // As soon as you CreateWindow() an IME window is created, even if the IME is never needed.
    // Disabling this behavior saves ~10% of our startup cost. God knows why it's not lazy init.
    ImmDisableIME(-1);

    // Make sure the context menu supports dark mode (Windows 10, 1903 or later).
    // An official API does not exist, owing to the fantastic shell team.
    // Leaked here: https://github.com/ysc3839/win32-darkmode/blob/master/win32-darkmode/DarkMode.h
    const HANDLE uxtheme = LoadLibraryExW(L"uxtheme.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    bool(WINAPI* const SetPreferredAppMode)(int) = (bool(WINAPI*)(int))GetProcAddress(uxtheme, MAKEINTRESOURCEA(135));
    SetPreferredAppMode(2); // PreferredAppMode::AllowDark

    settings_init();
    update_init();
    menu_init(instance);

    if (s_settings.switching_type != SettingsSwitchingType_Disabled) {
        menu_apply_override(UpdateOverride_None);
    }

    MSG msg;
    for (;;) {
        MsgWaitForMultipleObjectsEx(0, NULL, INFINITE, QS_ALLINPUT, MWMO_ALERTABLE);

        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (settings_dialog_dispatch(&msg)) {
                continue;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) {
                goto cleanup;
            }
        }
    }

cleanup:
    menu_deinit();
    return 0;
}

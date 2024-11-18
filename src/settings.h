#pragma once
#include "common.h"

typedef enum SettingsSwitchingType {
    SettingsSwitchingType_Disabled,
    SettingsSwitchingType_Custom,
    SettingsSwitchingType_Geographic,
} SettingsSwitchingType;

typedef struct Settings {
    SettingsSwitchingType switching_type;
    DWORD sunrise;
    DWORD sunset;
    float latitude;
    float longitude;
} Settings;

extern Settings s_settings;

void settings_init();
void show_settings(HWND hwnd);

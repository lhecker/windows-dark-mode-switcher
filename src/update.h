#pragma once
#include "resource.h"

typedef enum Override {
    UpdateOverride_None = ID_CONTEXTMENU_SWITCHAUTOMATICALLY,
    UpdateOverride_Light = ID_CONTEXTMENU_FORCELIGHTMODE,
    UpdateOverride_Dark = ID_CONTEXTMENU_FORCEDARKMODE,
} UpdateOverride;

void update_init();
void update_run(UpdateOverride override);

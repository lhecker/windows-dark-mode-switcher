#pragma once

typedef enum Override {
    UpdateOverride_None,
    UpdateOverride_Light,
    UpdateOverride_Dark,
} UpdateOverride;

void update_init();
void update_run(UpdateOverride override);

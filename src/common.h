#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _CRT_DECLARE_NONSTDC_NAMES 0
#include <Windows.h>

#include <stdbool.h>
#include <stddef.h>

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define clamp(x, a, b) (x) < (a) ? (a) : ((x) > (b) ? (b) : (x))
#define static_strlen(str) (ARRAYSIZE(str) - 1)

// If Failed (goto) Cleanup
#define IFC(x)            \
    do {                  \
        hr = (x);         \
        if (FAILED(hr)) { \
            goto cleanup; \
        }                 \
    } while (0)

#define SAFE_RELEASE(x)          \
    if (x) {                     \
        (x)->lpVtbl->Release(x); \
        (x) = NULL;              \
    }

typedef union FILETIME_QUAD {
    ULONGLONG QuadPart;
    FILETIME FtPart;
} FILETIME_QUAD;

enum UserMessages {
    WM_NOTIFICATION_ICON_CALLBACK = WM_USER,
    WM_GEOLOCATION_UPDATED,
};

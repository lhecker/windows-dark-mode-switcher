#include "dark_mode_switcher.h"

#include <shellapi.h>
#include <taskschd.h>

#include <string.h>
#include <wchar.h>

// If Failed Return
#define IFR(x)            \
    do {                  \
        HRESULT hr = (x); \
        if (FAILED(hr)) { \
            return hr;    \
        }                 \
    } while (0)

// If Failed Return, Win32 APIs that return error codes
#define IFR_WIN32(x)                        \
    do {                                    \
        DWORD err = (x);                    \
        if (err != 0) {                     \
            return HRESULT_FROM_WIN32(err); \
        }                                   \
    } while (0)

// If Failed Return, Win32 APIs that return BOOL
#define IFR_WIN32_BOOL(x)                              \
    do {                                               \
        BOOL ok = (x);                                 \
        if (!ok) {                                     \
            return HRESULT_FROM_WIN32(GetLastError()); \
        }                                              \
    } while (0)

// If Failed Return, BSTR allocations and interface put methods that require a single BSTR
#define IFR_PUT_BSTR(p_interface_, put_method_, str_)                       \
    do {                                                                    \
        BSTR bstr = SysAllocString(str_);                                   \
        if (!bstr) {                                                        \
            return E_OUTOFMEMORY;                                           \
        }                                                                   \
        HRESULT hr = p_interface_->lpVtbl->put_method_(p_interface_, bstr); \
        SysFreeString(bstr);                                                \
        if (FAILED(hr)) {                                                   \
            return hr;                                                      \
        }                                                                   \
    } while (0)

static void print_stdout(const char* str, size_t len)
{
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), str, (DWORD)len, NULL, NULL);
}

#define print_stdout_lit(str) print_stdout(str, sizeof(str) - 1)

// strcmp() for people without stdlib.
static bool string_equal(const wchar_t* a, const wchar_t* b)
{
    int result = CompareStringOrdinal(a, -1, b, -1, TRUE);
    return result == CSTR_EQUAL;
}

// Just a tiny helper to do argc/argv with bounds checking.
static const wchar_t* get_arg(int argc, wchar_t** argv, int index, const wchar_t* fallback)
{
    if (index >= argc) {
        return fallback;
    }
    return argv[index];
}

static HRESULT update_light_mode(DWORD light)
{
    IFR_WIN32(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", REG_DWORD, &light, sizeof(light)));
    IFR_WIN32(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"SystemUsesLightTheme", REG_DWORD, &light, sizeof(light)));
    IFR_WIN32_BOOL(SendNotifyMessageW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet"));
    return 0;
}

static HRESULT add_daily_trigger(ITriggerCollection* pTriggerCollection, const SYSTEMTIME* p_time)
{
    ITrigger* pTrigger = NULL;
    IFR(pTriggerCollection->lpVtbl->Create(pTriggerCollection, TASK_TRIGGER_DAILY, &pTrigger));
    IDailyTrigger* pDailyTrigger = NULL;
    IFR(pTrigger->lpVtbl->QueryInterface(pTrigger, &IID_IDailyTrigger, (void**)&pDailyTrigger));
    IFR_PUT_BSTR(pDailyTrigger, put_ExecutionTimeLimit, L"PT1M");
    wchar_t time_buffer[20];
    time_to_string(time_buffer, ARRAYSIZE(time_buffer), p_time);
    IFR_PUT_BSTR(pDailyTrigger, put_StartBoundary, time_buffer);
    IFR(pDailyTrigger->lpVtbl->put_DaysInterval(pDailyTrigger, 1));

    return 0;
}

static wchar_t* get_exe_path(wchar_t* buf, DWORD cap)
{
    DWORD len = GetModuleFileNameW(NULL, buf, cap);
    if (len == 0 || len >= cap) {
        return NULL;
    }
    return buf;
}

static HRESULT register_schtask(const SYSTEMTIME* p_sunrise, const SYSTEMTIME* p_sunset, const wchar_t* action_args)
{
    wchar_t user_id[256];
    DWORD user_id_size = ARRAYSIZE(user_id);
    IFR_WIN32_BOOL(GetUserNameW(user_id, &user_id_size));

    ITaskService* pService = NULL;
    IFR(CoCreateInstance(&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskService, (void**)&pService));
    IFR(pService->lpVtbl->Connect(pService, (VARIANT){0}, (VARIANT){0}, (VARIANT){0}, (VARIANT){0}));

    ITaskDefinition* pTask = NULL;
    IFR(pService->lpVtbl->NewTask(pService, 0, &pTask));

    ITaskSettings* pSettings = NULL;
    IFR(pTask->lpVtbl->get_Settings(pTask, &pSettings));
    IFR(pSettings->lpVtbl->put_AllowDemandStart(pSettings, VARIANT_FALSE));
    IFR(pSettings->lpVtbl->put_DisallowStartIfOnBatteries(pSettings, VARIANT_FALSE));
    IFR(pSettings->lpVtbl->put_Hidden(pSettings, VARIANT_TRUE));
    IFR(pSettings->lpVtbl->put_StartWhenAvailable(pSettings, VARIANT_TRUE));

    // Get the current user ID.
    IPrincipal* pPrincipal = NULL;
    IFR(pTask->lpVtbl->get_Principal(pTask, &pPrincipal));
    IFR_PUT_BSTR(pPrincipal, put_UserId, user_id);
    IFR(pPrincipal->lpVtbl->put_LogonType(pPrincipal, TASK_LOGON_INTERACTIVE_TOKEN));

    ITriggerCollection* pTriggerCollection = NULL;
    IFR(pTask->lpVtbl->get_Triggers(pTask, &pTriggerCollection));

    //  Add the sunrise trigger to the task.
    IFR(add_daily_trigger(pTriggerCollection, p_sunrise));

    //  Add the sunset trigger to the task.
    IFR(add_daily_trigger(pTriggerCollection, p_sunset));

    // Add an action to the task.
    IActionCollection* pActionCollection = NULL;
    IFR(pTask->lpVtbl->get_Actions(pTask, &pActionCollection));
    IAction* pAction = NULL;
    IFR(pActionCollection->lpVtbl->Create(pActionCollection, TASK_ACTION_EXEC, &pAction));
    IExecAction* pExecAction = NULL;
    IFR(pAction->lpVtbl->QueryInterface(pAction, &IID_IExecAction, (void**)&pExecAction));
    wchar_t exe_path_buf[64 * 1024];
    wchar_t* exe_path = get_exe_path(exe_path_buf, ARRAYSIZE(exe_path_buf));
    if (!exe_path) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    IFR_PUT_BSTR(pExecAction, put_Path, exe_path);
    IFR_PUT_BSTR(pExecAction, put_Arguments, action_args);

    // Save the task in the root folder.
    BSTR bstr_root_folder = SysAllocString(L"\\");
    if (!bstr_root_folder) {
        return E_OUTOFMEMORY;
    }
    ITaskFolder* pRootFolder = NULL;
    HRESULT hr_get_folder = pService->lpVtbl->GetFolder(pService, bstr_root_folder, &pRootFolder);
    SysFreeString(bstr_root_folder);
    if (FAILED(hr_get_folder)) {
        return hr_get_folder;
    }
    BSTR bstr_task_name = SysAllocString(L"Dark Mode Switcher");
    if (!bstr_task_name) {
        return E_OUTOFMEMORY;
    }
    IRegisteredTask* pRegisteredTask = NULL;
    HRESULT hr_register = pRootFolder->lpVtbl->RegisterTaskDefinition(pRootFolder, bstr_task_name, pTask, TASK_CREATE_OR_UPDATE, (VARIANT){0}, (VARIANT){0}, TASK_LOGON_INTERACTIVE_TOKEN, (VARIANT){0}, &pRegisteredTask);
    SysFreeString(bstr_task_name);
    if (FAILED(hr_register)) {
        return hr_register;
    }

    return 0;
}

static HRESULT do_switch(int argc, wchar_t** argv)
{
    SYSTEMTIME sunrise, sunset;
    if (!string_to_time(&sunrise, get_arg(argc, argv, 2, L"")) || !string_to_time(&sunset, get_arg(argc, argv, 3, L""))) {
        print_stdout_lit("Invalid time format. Use HH:mm.\r\n");
        return 1;
    }

    const int sunrise_minutes = sunrise.wHour * 60 + sunrise.wMinute;
    const int sunset_minutes = sunset.wHour * 60 + sunset.wMinute;
    if (sunrise_minutes >= sunset_minutes) {
        print_stdout_lit("Sunrise must be before sunset.\r\n");
        return 1;
    }

    SYSTEMTIME time;
    GetLocalTime(&time);

    const DWORD light = get_use_light(&time, &sunrise, &sunset);
    return update_light_mode(light);
}

static HRESULT do_register_time(int argc, wchar_t** argv)
{
    SYSTEMTIME time;
    GetLocalTime(&time);

    SYSTEMTIME sunrise = time, sunset = time;
    sunrise.wSecond = sunset.wSecond = 0;
    if (!string_to_time(&sunrise, get_arg(argc, argv, 2, L"")) || !string_to_time(&sunset, get_arg(argc, argv, 3, L""))) {
        print_stdout_lit("Invalid time format. Use HH:mm.\r\n");
        return 1;
    }

    const int sunrise_minutes = sunrise.wHour * 60 + sunrise.wMinute;
    const int sunset_minutes = sunset.wHour * 60 + sunset.wMinute;
    if (sunrise_minutes >= sunset_minutes) {
        print_stdout_lit("Sunrise must be before sunset.\r\n");
        return 1;
    }

    const DWORD light = get_use_light(&time, &sunrise, &sunset);
    IFR(update_light_mode(light));

    // prepare task arguments
    wchar_t args[20] = L"switch            "; // keep the number of spaces! the task only switches the mode, so the command to run is `switch`
    memcpy(&args[7], argv[2], 10);            // insert sunrise
    memcpy(&args[13], argv[3], 10);           // insert sunset

    return register_schtask(&sunrise, &sunset, args);
}

static HRESULT do_register_position(int argc, wchar_t** argv)
{
    double lat = 0.0, lon = 0.0;
    if (!string_to_coordinate(&lat, get_arg(argc, argv, 2, L"_"), 90.0, -90.0) || !string_to_coordinate(&lon, get_arg(argc, argv, 3, L"_"), 180.0, -180.0)) {
        return 1;
    }

    SYSTEMTIME rise = {0}, set = {0}, nextrise = {0}, nextset = {0};

    const int rc = get_sun_rise_set(&rise, &set, &nextrise, &nextset, lat, lon);
    if (rc == sun_error) {
        return 1;
    }

    // the current mode is based on today's sunrise and sunset
    DWORD light;
    if (rc & sun_normal) {
        SYSTEMTIME now;
        GetLocalTime(&now);
        light = get_use_light(&now, &rise, &set);
    } else if (rc & sun_above_horizon) {
        light = 1;
    } else {
        light = 0;
    }

    IFR(update_light_mode(light));

    // prepare task arguments
    wchar_t args[512] = L"registerposition "; // the task has to update itself, so the command to run is `registerposition`
    const size_t arglen2 = wcslen(argv[2]);
    const size_t arglen3 = wcslen(argv[3]);
    if (arglen2 + arglen3 > ARRAYSIZE(args) - 20) {
        return 1;
    }
    memcpy(&args[17], argv[2], arglen2 * sizeof(wchar_t)); // insert latitude
    args[17 + arglen2] = L' ';
    memcpy(&args[18 + arglen2], argv[3], (arglen3 + 1) * sizeof(wchar_t)); // insert longitude and terminating null

    // the scheduled task is based on the next sunrise and sunset that can still be one or even both of today's values
    return register_schtask(&nextrise, &nextset, args);
}

static int do_help(int status)
{
    print_stdout_lit(
        "Usage: dark-mode-switcher.exe <COMMAND>\r\n"
        "\r\n"
        "Commands:\r\n"
        "  help, -h, --help\r\n"
        "    Print help\r\n"
        "  switch <SUNRISE> <SUNSET>\r\n"
        "    Switch between light/dark mode. SUNRISE and SUNSET must be in the HH:mm 24-hour format. Prefer using the registertime command.\r\n"
        "  registertime <SUNRISE> <SUNSET>\r\n"
        "    Sets a scheduled task for the light/dark switch mode. SUNRISE and SUNSET must be in the HH:mm 24-hour format.\r\n"
        "  registerposition <LATITUDE> <LONGITUDE>\r\n"
        "    Dynamically sets a scheduled task switching the light/dark mode based on the geographic coordinates.\r\n"
        "    LATITUDE and LONGITUDE must be in the Decimal Degrees format.\r\n"
    );
    return status;
}

int wmain(int argc, wchar_t* argv[])
{
    const wchar_t* argv1 = get_arg(argc, argv, 1, L"help");

    if (string_equal(argv1, L"switch")) {
        return do_switch(argc, argv);
    }
    if (string_equal(argv1, L"registertime")) {
        IFR(CoInitializeEx(NULL, COINIT_MULTITHREADED));
        const int status = do_register_time(argc, argv);
        CoUninitialize();
        return status;
    }
    if (string_equal(argv1, L"registerposition")) {
        IFR(CoInitializeEx(NULL, COINIT_MULTITHREADED));
        const int status = do_register_position(argc, argv);
        CoUninitialize();
        return status;
    }
    if (string_equal(argv1, L"help") || string_equal(argv1, L"-h") || string_equal(argv1, L"--help")) {
        return do_help(0);
    }
    return do_help(1);
}

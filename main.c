#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define INITGUID
#include <Windows.h>
#include <shellapi.h>
#include <taskschd.h>

#include <stdbool.h>

// Curiously, the Windows SDK doesn't define these.
DEFINE_GUID(IID_ITaskService, 0x2faba4c7, 0x4da9, 0x4013, 0x96, 0x97, 0x20, 0xcc, 0x3f, 0xd4, 0x0f, 0x85);
DEFINE_GUID(IID_IDailyTrigger, 0x126c5cd8, 0xb288, 0x41d5, 0x8d, 0xbf, 0xe4, 0x91, 0x44, 0x6a, 0xdc, 0x5c);
DEFINE_GUID(IID_IExecAction, 0x4c3d624d, 0xfd6b, 0x49a3, 0xb9, 0xb7, 0x09, 0xcb, 0x3c, 0xd3, 0xf0, 0x47);
DEFINE_GUID(CLSID_TaskScheduler, 0x0f87369f, 0xa4e5, 0x4cfc, 0xbd, 0x3e, 0x73, 0xe6, 0x15, 0x45, 0x72, 0xdd);

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

// strlen() for people without stdlib.
static size_t string_length(const wchar_t* str)
{
    size_t len = 0;
    while (str[len] != L'\0') {
        len++;
    }
    return len;
}

// Just a tiny helper to do argc/argv with bounds checking.
static const wchar_t* get_arg(int argc, wchar_t** argv, int index, const wchar_t* fallback)
{
    if (index >= argc) {
        return fallback;
    }
    return argv[index];
}

// Validates that the input is HH:mm and returns 12:34 as 1234. Returns -1 on error.
static int parse_time(const wchar_t* arg)
{
    if (string_length(arg) != 5) {
        return -1;
    }

    if (arg[2] != L':') {
        return -1;
    }

    int hour = (arg[0] - L'0') * 10 + (arg[1] - L'0');
    int minute = (arg[3] - L'0') * 10 + (arg[4] - L'0');
    if (hour < 0 || hour >= 24 || minute < 0 || minute >= 60) {
        return -1;
    }

    return hour * 100 + minute;
}

static DWORD do_switch(int argc, wchar_t** argv)
{
    int sunrise = parse_time(get_arg(argc, argv, 2, L""));
    int sunset = parse_time(get_arg(argc, argv, 3, L""));

    if (sunrise < 0 || sunset < 0) {
        print_stdout_lit("Invalid time format. Use HH:mm.\r\n");
        return 1;
    }

    if (sunrise >= sunset) {
        print_stdout_lit("Sunrise must be before sunset.\r\n");
        return 1;
    }

    SYSTEMTIME time;
    GetLocalTime(&time);

    int current_time = time.wHour * 100 + time.wMinute;
    DWORD light = current_time >= sunrise && current_time < sunset;
    IFR_WIN32(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", REG_DWORD, &light, sizeof(light)));
    IFR_WIN32(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"SystemUsesLightTheme", REG_DWORD, &light, sizeof(light)));

    IFR_WIN32_BOOL(SendNotifyMessageW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet"));
    return 0;
}

static BSTR get_exe_path()
{
    wchar_t exe_path_buf[64 * 1024];
    DWORD cap = ARRAYSIZE(exe_path_buf);
    DWORD len = GetModuleFileNameW(NULL, exe_path_buf, cap);
    if (len == 0 || len >= cap) {
        return NULL;
    }
    return SysAllocString(exe_path_buf);
}

static DWORD do_register(int argc, wchar_t** argv)
{
    int sunrise = parse_time(get_arg(argc, argv, 2, L""));
    int sunset = parse_time(get_arg(argc, argv, 3, L""));

    if (sunrise < 0 || sunset < 0) {
        print_stdout_lit("Invalid time format. Use HH:mm.\r\n");
        return 1;
    }

    if (sunrise >= sunset) {
        print_stdout_lit("Sunrise must be before sunset.\r\n");
        return 1;
    }

    BSTR exe_path = get_exe_path();
    if (!exe_path) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    
    wchar_t user_id[256];
    DWORD user_id_size = ARRAYSIZE(user_id);
    IFR_WIN32_BOOL(GetUserNameW(user_id, &user_id_size));

    IFR(CoInitializeEx(NULL, COINIT_MULTITHREADED));

    ITaskService* pService = NULL;
    IFR(CoCreateInstance(&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskService, (void**)&pService));
    IFR(pService->lpVtbl->Connect(pService, (VARIANT){}, (VARIANT){}, (VARIANT){}, (VARIANT){}));

    ITaskDefinition* pTask = NULL;
    IFR(pService->lpVtbl->NewTask(pService, 0, &pTask));

    ITaskSettings* pSettings = NULL;
    IFR(pTask->lpVtbl->get_Settings(pTask, &pSettings));
    IFR(pSettings->lpVtbl->put_AllowDemandStart(pSettings, VARIANT_FALSE));
    IFR(pSettings->lpVtbl->put_StartWhenAvailable(pSettings, VARIANT_TRUE));

    // Get the current user ID.
    IPrincipal* pPrincipal = NULL;
    IFR(pTask->lpVtbl->get_Principal(pTask, &pPrincipal));
    IFR(pPrincipal->lpVtbl->put_UserId(pPrincipal, SysAllocString(user_id)));
    IFR(pPrincipal->lpVtbl->put_LogonType(pPrincipal, TASK_LOGON_INTERACTIVE_TOKEN));

    ITriggerCollection* pTriggerCollection = NULL;
    IFR(pTask->lpVtbl->get_Triggers(pTask, &pTriggerCollection));

    //  Add the daily trigger to the task.
    ITrigger* pTriggerSunrise = NULL;
    IFR(pTriggerCollection->lpVtbl->Create(pTriggerCollection, TASK_TRIGGER_DAILY, &pTriggerSunrise));
    IDailyTrigger* pDailyTriggerSunrise = NULL;
    IFR(pTriggerSunrise->lpVtbl->QueryInterface(pTriggerSunrise, &IID_IDailyTrigger, (void**)&pDailyTriggerSunrise));
    IFR(pDailyTriggerSunrise->lpVtbl->put_ExecutionTimeLimit(pDailyTriggerSunrise, SysAllocString(L"PT1M")));
    IFR(pDailyTriggerSunrise->lpVtbl->put_StartBoundary(pDailyTriggerSunrise, SysAllocString(L"2005-01-01T12:05:00")));
    IFR(pDailyTriggerSunrise->lpVtbl->put_DaysInterval(pDailyTriggerSunrise, 1));

    //  Add the daily trigger to the task.
    ITrigger* pTriggerSunset = NULL;
    IFR(pTriggerCollection->lpVtbl->Create(pTriggerCollection, TASK_TRIGGER_DAILY, &pTriggerSunset));
    IDailyTrigger* pDailyTriggerSunset = NULL;
    IFR(pTriggerSunset->lpVtbl->QueryInterface(pTriggerSunset, &IID_IDailyTrigger, (void**)&pDailyTriggerSunset));
    IFR(pDailyTriggerSunset->lpVtbl->put_ExecutionTimeLimit(pDailyTriggerSunset, SysAllocString(L"PT1M")));
    IFR(pDailyTriggerSunset->lpVtbl->put_StartBoundary(pDailyTriggerSunset, SysAllocString(L"2005-01-01T12:05:00")));
    IFR(pDailyTriggerSunset->lpVtbl->put_DaysInterval(pDailyTriggerSunset, 1));

    // Add an action to the task.
    IActionCollection* pActionCollection = NULL;
    IFR(pTask->lpVtbl->get_Actions(pTask, &pActionCollection));
    IAction* pAction = NULL;
    IFR(pActionCollection->lpVtbl->Create(pActionCollection, TASK_ACTION_EXEC, &pAction));
    IExecAction* pExecAction = NULL;
    IFR(pAction->lpVtbl->QueryInterface(pAction, &IID_IExecAction, (void**)&pExecAction));
    IFR(pExecAction->lpVtbl->put_Path(pExecAction, exe_path));

    // Save the task in the root folder.
    ITaskFolder* pRootFolder = NULL;
    IFR(pService->lpVtbl->GetFolder(pService, SysAllocString(L"\\"), &pRootFolder));
    IRegisteredTask* pRegisteredTask = NULL;
    IFR(pRootFolder->lpVtbl->RegisterTaskDefinition(pRootFolder, SysAllocString(L"Dark Mode Switcher"), pTask, TASK_CREATE_OR_UPDATE, (VARIANT){}, (VARIANT){}, TASK_LOGON_INTERACTIVE_TOKEN, (VARIANT){}, &pRegisteredTask));

    return 0;
}

static DWORD do_help(DWORD status)
{
    print_stdout_lit(
        "Usage: dark-mode-switcher.exe <COMMAND>\r\n"
        "\r\n"
        "Commands:\r\n"
        "  help, -h, --help\r\n"
        "    Print help\r\n"
        "  switch <SUNRISE> <SUNSET>\r\n"
        "    Switch between light/dark mode. SUNRISE and SUNSET must be in the HH:mm 24-hour format. Prefer using the register command.\r\n"
        "  register <SUNRISE> <SUNSET>\r\n"
        "    Sets a scheduled task for the light/dark switch mode. SUNRISE and SUNSET must be in the HH:mm 24-hour format.\r\n"
    );
    return status;
}

DWORD mainCRTStartup()
{
    int argc;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    const wchar_t* argv1 = get_arg(argc, argv, 1, L"help");

    if (string_equal(argv1, L"switch")) {
        return do_switch(argc, argv);
    }
    if (string_equal(argv1, L"register")) {
        return do_register(argc, argv);
    }
    if (string_equal(argv1, L"help") || string_equal(argv1, L"-h") || string_equal(argv1, L"--help")) {
        return do_help(0);
    }
    return do_help(1);
}

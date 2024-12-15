#pragma once

#include <asyncinfo.h>
#include <winstring.h>

static inline HSTRING hstring_reference(HSTRING_HEADER* header, const wchar_t* str)
{
    HSTRING hstr;
    WindowsCreateStringReference(str, (UINT32)wcslen(str), header, &hstr);
    return hstr;
}

void* create_wrapper_for_IAsyncOperationCompletedHandler(GUID iid, HRESULT (*callback)(void* context, void* asyncInfo, AsyncStatus asyncStatus), void* context);

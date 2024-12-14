#pragma once

#include <asyncinfo.h>

HSTRING hstring_reference(HSTRING_HEADER* header, const wchar_t* str, UINT32 len);
void* create_wrapper_for_IAsyncOperationCompletedHandler(GUID iid, HRESULT (*callback)(void* context, void* asyncInfo, AsyncStatus asyncStatus), void* context);

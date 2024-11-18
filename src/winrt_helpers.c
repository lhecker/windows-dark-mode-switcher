#include "winrt_helpers.h"

#include <winstring.h>

HSTRING hstring_reference(HSTRING_HEADER* header, const wchar_t* str)
{
    HSTRING hstr;
    WindowsCreateStringReference(str, (UINT32)wcsnlen(str, 0xffffffff), header, &hstr);
    return hstr;
}

typedef struct IAsyncOperationCompletedHandlerWrapper IAsyncOperationCompletedHandlerWrapper;

typedef struct IAsyncOperationCompletedHandlerWrapperVtable {
    // clang-format off
    HRESULT(STDMETHODCALLTYPE* QueryInterface)(IAsyncOperationCompletedHandlerWrapper* self, REFIID riid, void** ppvObject);
    ULONG(STDMETHODCALLTYPE* AddRef)(IAsyncOperationCompletedHandlerWrapper* self);
    ULONG(STDMETHODCALLTYPE* Release)(IAsyncOperationCompletedHandlerWrapper* self);
    HRESULT(STDMETHODCALLTYPE* Invoke)(IAsyncOperationCompletedHandlerWrapper* self, void* asyncInfo, AsyncStatus asyncStatus);
    // clang-format on
} IAsyncOperationCompletedHandlerWrapperVtable;

struct IAsyncOperationCompletedHandlerWrapper {
    const IAsyncOperationCompletedHandlerWrapperVtable* vtable;

    // clang-format off
    GUID iid;
    HRESULT(*callback)(void* context, void* asyncInfo, AsyncStatus asyncStatus);
    void* context;
    LONG ref_count;
    // clang-format on
};

static HRESULT STDMETHODCALLTYPE IAsyncOperationCompletedHandlerWrapper_QueryInterface(IAsyncOperationCompletedHandlerWrapper* self, REFIID riid, void** ppvObject)
{
    static const GUID IID_IAgileObject = {0x94EA2B94, 0xE9CC, 0x49E0, {0xC0, 0xFF, 0xEE, 0x64, 0xCA, 0x8F, 0x5B, 0x90}};

    if (ppvObject == NULL) {
        return E_POINTER;
    }
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IAgileObject) || IsEqualGUID(riid, &self->iid)) {
        *ppvObject = self;
        InterlockedIncrement(&self->ref_count);
        return S_OK;
    }
    *ppvObject = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE IAsyncOperationCompletedHandlerWrapper_AddRef(IAsyncOperationCompletedHandlerWrapper* self)
{
    return InterlockedIncrement(&self->ref_count);
}

static ULONG STDMETHODCALLTYPE IAsyncOperationCompletedHandlerWrapper_Release(IAsyncOperationCompletedHandlerWrapper* self)
{
    LONG ref_count = InterlockedDecrement(&self->ref_count);
    if (ref_count <= 0) {
        CoTaskMemFree(self);
    }
    return ref_count;
}

static HRESULT STDMETHODCALLTYPE IAsyncOperationCompletedHandlerWrapper_Invoke(IAsyncOperationCompletedHandlerWrapper* self, void* asyncInfo, AsyncStatus asyncStatus)
{
    return self->callback(self->context, asyncInfo, asyncStatus);
}

void* create_wrapper_for_IAsyncOperationCompletedHandler(GUID iid, HRESULT (*callback)(void* context, void* asyncInfo, AsyncStatus asyncStatus), void* context)
{
    static const IAsyncOperationCompletedHandlerWrapperVtable vtable = {
        .QueryInterface = IAsyncOperationCompletedHandlerWrapper_QueryInterface,
        .AddRef = IAsyncOperationCompletedHandlerWrapper_AddRef,
        .Release = IAsyncOperationCompletedHandlerWrapper_Release,
        .Invoke = IAsyncOperationCompletedHandlerWrapper_Invoke,
    };

    IAsyncOperationCompletedHandlerWrapper* wrapper = CoTaskMemAlloc(sizeof(IAsyncOperationCompletedHandlerWrapper));
    wrapper->vtable = &vtable;
    wrapper->iid = iid;
    wrapper->callback = callback;
    wrapper->context = context;
    wrapper->ref_count = 0;
    return wrapper;
}

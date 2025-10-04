/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.ModernExperience.Com.cpp
 * PURPOSE:   Entry point and self registration code
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: dinhngtu (contact@tudinh.xyz)
 */

#define WIN32_LEAN_AND_MEAN

#include <string>
#include <windows.h>
#include <unknwn.h>

#include "CNanaZipCopyHook.hpp"

static constexpr size_t GUID_STRING_LENGTH =
    ARRAYSIZE(L"{12345678-90AB-CDEF-1234-567890ABCDEF}");

static const std::wstring CLSID_REGISTRY_PATH =
    L"Software\\Classes\\CLSID\\";
static const std::wstring COPY_HOOK_REGISTRY_PATH =
    L"Software\\Classes\\Directory\\shellex\\CopyHookHandlers\\";

static HINSTANCE g_hInstance = NULL;

BOOL WINAPI DllMain(
    HMODULE hModule,
    DWORD ul_reason_for_call,
    LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(lpReserved);

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hInstance = hModule;
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

struct DECLSPEC_UUID("{542CE69A-6EA7-4D77-9B8F-8F56CEA2BF16}")
    CNanaZipCopyHookFactory : winrt::implements<
    CNanaZipCopyHookFactory,
    IClassFactory>
{
    IFACEMETHODIMP CreateInstance(
        IUnknown * pUnkOuter,
        REFIID riid,
        void** ppvObject) WIN_NOEXCEPT
    {
        if (pUnkOuter)
        {
            return CLASS_E_NOAGGREGATION;
        }

        try
        {
            return winrt::make<CNanaZipCopyHook>()
                ->QueryInterface(riid, ppvObject);
        }
        catch (...)
        {
            return winrt::to_hresult();
        }
    }

    IFACEMETHODIMP LockServer(BOOL fLock) WIN_NOEXCEPT
    {
        if (fLock)
        {
            ++winrt::get_module_lock();
        }
        else
        {
            --winrt::get_module_lock();
        }
        return S_OK;
    }
};

STDAPI DllCanUnloadNow()
{
    if (winrt::get_module_lock())
    {
        return S_FALSE;
    }
    winrt::clear_factory_cache();
    return S_OK;
}

STDAPI DllGetClassObject(
    _In_ REFCLSID rclsid,
    _In_ REFIID riid,
    _Outptr_ LPVOID* ppv)
{
    try
    {
        *ppv = NULL;
        if (rclsid == __uuidof(CNanaZipCopyHookFactory))
        {
            return winrt::make<CNanaZipCopyHookFactory>()
                ->QueryInterface(riid, ppv);
        }
        return E_INVALIDARG;
    }
    catch (...)
    {
        return winrt::to_hresult();
    }
}

STDAPI DllRegisterServer()
{
    try
    {
        wchar_t clsid[GUID_STRING_LENGTH];

        auto clsidLen = ::StringFromGUID2(
            __uuidof(CNanaZipCopyHookFactory),
            clsid,
            ARRAYSIZE(clsid));
        if (!clsidLen)
        {
            winrt::throw_hresult(E_OUTOFMEMORY);
        }

        auto serverKeyName = CLSID_REGISTRY_PATH + clsid + L"\\InprocServer32";

        wchar_t dllPath[MAX_PATH];
        ::SetLastError(ERROR_SUCCESS);
        auto dllPathLen = ::GetModuleFileNameW(
            g_hInstance,
            dllPath,
            ARRAYSIZE(dllPath));
        winrt::check_win32(::GetLastError());

        winrt::check_win32(::RegSetKeyValueW(
            HKEY_CURRENT_USER,
            serverKeyName.c_str(),
            NULL,
            REG_SZ,
            dllPath,
            sizeof(wchar_t) * (dllPathLen + 1)));
        winrt::check_win32(::RegSetKeyValueW(
            HKEY_CURRENT_USER,
            serverKeyName.c_str(),
            L"ThreadingModel",
            REG_SZ,
            L"Apartment",
            sizeof(L"Apartment")));

        auto handlerKeyName = COPY_HOOK_REGISTRY_PATH + clsid;
        winrt::check_win32(::RegSetKeyValueW(
            HKEY_CURRENT_USER,
            handlerKeyName.c_str(),
            NULL,
            REG_SZ,
            clsid,
            sizeof(wchar_t) * (clsidLen + 1)));

        return S_OK;
    }
    catch (...)
    {
        return winrt::to_hresult();
    }
}

STDAPI DllUnregisterServer()
{
    try
    {
        wchar_t clsid[GUID_STRING_LENGTH];

        if (!::StringFromGUID2(
            __uuidof(CNanaZipCopyHookFactory),
            clsid,
            ARRAYSIZE(clsid)))
            winrt::throw_hresult(E_OUTOFMEMORY);

        auto handlerKeyName = COPY_HOOK_REGISTRY_PATH + clsid;
        ::RegDeleteKeyExW(HKEY_CURRENT_USER, handlerKeyName.c_str(), 0, 0);

        auto serverKeyName = CLSID_REGISTRY_PATH + clsid + L"\\InprocServer32";
        ::RegDeleteKeyExW(HKEY_CURRENT_USER, serverKeyName.c_str(), 0, 0);

        auto clsidKeyName = CLSID_REGISTRY_PATH + clsid;
        ::RegDeleteKeyExW(HKEY_CURRENT_USER, clsidKeyName.c_str(), 0, 0);

        return S_OK;
    }
    catch (...)
    {
        return winrt::to_hresult();
    }
}

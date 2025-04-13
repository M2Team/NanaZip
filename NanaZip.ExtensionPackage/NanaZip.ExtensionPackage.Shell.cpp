/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.ExtensionPackage.Shell.cpp
 * PURPOSE:   Entry point and self registration code
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: dinhngtu (contact@tudinh.xyz)
 */

#define WIN32_LEAN_AND_MEAN

#include <string>
#include <windows.h>

#include <wil/registry.h>
#include <wil/win32_helpers.h>
#include <wil/stl.h>

#include "CNanaZipCopyHook.hpp"

using namespace std::string_literals;

static HINSTANCE g_hInstance = NULL;

BOOL WINAPI DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    UNREFERENCED_PARAMETER(lpReserved);

    switch (ul_reason_for_call) {
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

struct DECLSPEC_UUID("{542CE69A-6EA7-4D77-9B8F-8F56CEA2BF16}") CNanaZipLegacyContextMenuFactory
    : winrt::implements<CNanaZipLegacyContextMenuFactory, IClassFactory> {
    IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppvObject) WIN_NOEXCEPT {
        if (pUnkOuter)
            return CLASS_E_NOAGGREGATION;

        try {
            return winrt::make<CNanaZipCopyHook>()->QueryInterface(riid, ppvObject);
        } catch (...) {
            return winrt::to_hresult();
        }
    }

    IFACEMETHODIMP LockServer(BOOL fLock) WIN_NOEXCEPT {
        if (fLock)
            ++winrt::get_module_lock();
        else
            --winrt::get_module_lock();
        return S_OK;
    }
};

_Use_decl_annotations_ STDAPI DllCanUnloadNow() {
    if (winrt::get_module_lock())
        return S_FALSE;
    winrt::clear_factory_cache();
    return S_OK;
}

_Use_decl_annotations_ STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID *ppv) {
    try {
        *ppv = NULL;
        if (rclsid == __uuidof(CNanaZipLegacyContextMenuFactory))
            return winrt::make<CNanaZipLegacyContextMenuFactory>()->QueryInterface(riid, ppv);
        return E_INVALIDARG;
    } catch (...) {
        return winrt::to_hresult();
    }
}

_Use_decl_annotations_ STDAPI DllRegisterServer() {
    try {
        wchar_t clsid[wil::guid_string_buffer_length];

        if (!StringFromGUID2(__uuidof(CNanaZipLegacyContextMenuFactory), clsid, ARRAYSIZE(clsid)))
            winrt::throw_hresult(E_OUTOFMEMORY);

        auto clsidKeyName = L"Software\\Classes\\CLSID\\"s + clsid;
        auto clsidKey =
            wil::reg::create_unique_key(HKEY_CURRENT_USER, clsidKeyName.c_str(), wil::reg::key_access::readwrite);

        auto serverKey =
            wil::reg::create_unique_key(clsidKey.get(), L"InprocServer32", wil::reg::key_access::readwrite);
        auto dllPath = wil::GetModuleFileNameW<std::wstring>(g_hInstance);
        wil::reg::set_value_string(serverKey.get(), NULL, dllPath.c_str());
        wil::reg::set_value_string(serverKey.get(), L"ThreadingModel", L"Apartment");

        auto handlerKeyName = L"Software\\Classes\\Directory\\shellex\\CopyHookHandlers\\"s + clsid;
        auto handlerKey =
            wil::reg::create_unique_key(HKEY_CURRENT_USER, handlerKeyName.c_str(), wil::reg::key_access::readwrite);
        wil::reg::set_value_string(handlerKey.get(), NULL, clsid);

        return S_OK;
    } catch (...) {
        return winrt::to_hresult();
    }
}

_Use_decl_annotations_ STDAPI DllUnregisterServer() {
    try {
        wchar_t clsid[wil::guid_string_buffer_length];

        if (!StringFromGUID2(__uuidof(CNanaZipLegacyContextMenuFactory), clsid, ARRAYSIZE(clsid)))
            winrt::throw_hresult(E_OUTOFMEMORY);

        auto handlerKeyName = L"Software\\Classes\\Directory\\shellex\\CopyHookHandlers\\"s + clsid;
        RegDeleteKeyExW(HKEY_CURRENT_USER, handlerKeyName.c_str(), 0, 0);

        auto serverKeyName = L"Software\\Classes\\CLSID\\"s + clsid + L"\\InprocServer32";
        RegDeleteKeyExW(HKEY_CURRENT_USER, serverKeyName.c_str(), 0, 0);

        auto clsidKeyName = L"Software\\Classes\\CLSID\\"s + clsid;
        RegDeleteKeyExW(HKEY_CURRENT_USER, clsidKeyName.c_str(), 0, 0);

        return S_OK;
    } catch (...) {
        return winrt::to_hresult();
    }
}

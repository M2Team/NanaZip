/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Frieren.RuntimeObjectWrapper.cpp
 * PURPOSE:   Implementation for Windows Runtime C API Wrapper
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include <Windows.h>

#define _ROAPI_
#include <roapi.h>

#include <roerrorapi.h>

#include <winrt/Windows.Foundation.h>

namespace
{
    static HMODULE GetComBaseModuleHandle()
    {
        static HMODULE CachedResult = ::LoadLibraryExW(
            L"combase.dll",
            nullptr,
            LOAD_LIBRARY_SEARCH_SYSTEM32);
        return CachedResult;
    }

    static FARPROC GetRoGetActivationFactoryProcAddress()
    {
        static FARPROC CachedResult = ([]() -> FARPROC
        {
            HMODULE ModuleHandle = ::GetComBaseModuleHandle();
            if (ModuleHandle)
            {
                return ::GetProcAddress(
                    ModuleHandle,
                    "RoGetActivationFactory");
            }
            return nullptr;
        }());

        return CachedResult;
    }

    static FARPROC GetRoOriginateLanguageExceptionProcAddress()
    {
        static FARPROC CachedResult = ([]() -> FARPROC
        {
            HMODULE ModuleHandle = ::GetComBaseModuleHandle();
            if (ModuleHandle)
            {
                return ::GetProcAddress(
                    ModuleHandle,
                    "RoOriginateLanguageException");
            }
            return nullptr;
        }());

        return CachedResult;
    }

    struct ErrorInfoFallback : public winrt::implements<
        ErrorInfoFallback,
        IErrorInfo,
        IRestrictedErrorInfo>
    {
    private:

        HRESULT m_Code;
        winrt::hstring m_Message;

    public:

        ErrorInfoFallback(
            _In_ HRESULT Code,
            _In_ HSTRING Message) :
            m_Code(Code),
            m_Message(*reinterpret_cast<winrt::hstring*>(&Message))
        {

        }

        HRESULT STDMETHODCALLTYPE GetGUID(
            _Out_ GUID* pGUID)
        {
            std::memset(pGUID, 0, sizeof(GUID));
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetSource(
            _Out_ BSTR* pBstrSource)
        {
            *pBstrSource = nullptr;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetDescription(
            _Out_ BSTR* pBstrDescription)
        {
            *pBstrDescription = ::SysAllocString(this->m_Message.c_str());
            return *pBstrDescription ? S_OK : E_OUTOFMEMORY;
        }

        HRESULT STDMETHODCALLTYPE GetHelpFile(
            _Out_ BSTR* pBstrHelpFile)
        {
            *pBstrHelpFile = nullptr;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetHelpContext(
            _Out_ DWORD* pdwHelpContext)
        {
            *pdwHelpContext = 0;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE GetErrorDetails(
            _Out_ BSTR* description,
            _Out_ HRESULT* error,
            _Out_ BSTR* restrictedDescription,
            _Out_ BSTR* capabilitySid)
        {
            *description = nullptr;
            *error = this->m_Code;
            *capabilitySid = nullptr;
            *restrictedDescription = ::SysAllocString(this->m_Message.c_str());
            return *restrictedDescription ? S_OK : E_OUTOFMEMORY;
        }

        virtual HRESULT STDMETHODCALLTYPE GetReference(
            _Out_ BSTR* reference)
        {
            *reference = nullptr;
            return S_OK;
        }
    };
}

EXTERN_C HRESULT WINAPI RoGetActivationFactory(
    _In_ HSTRING activatableClassId,
    _In_ REFIID iid,
    _Out_ LPVOID* factory)
{
    using ProcType = decltype(::RoGetActivationFactory)*;

    ProcType ProcAddress = reinterpret_cast<ProcType>(
        ::GetRoGetActivationFactoryProcAddress());

    if (ProcAddress)
    {
        return ProcAddress(activatableClassId, iid, factory);
    }

    *factory = nullptr;
    return CLASS_E_CLASSNOTAVAILABLE;
}

EXTERN_C BOOL WINAPI RoOriginateLanguageException(
    _In_ HRESULT error,
    _In_opt_ HSTRING message,
    _In_opt_ IUnknown* languageException)
{
    using ProcType = decltype(::RoOriginateLanguageException)*;

    ProcType ProcAddress = reinterpret_cast<ProcType>(
        ::GetRoOriginateLanguageExceptionProcAddress());

    if (ProcAddress)
    {
        return ProcAddress(error, message, languageException);
    }

    winrt::com_ptr<IErrorInfo> ErrorInfo =
        winrt::make<ErrorInfoFallback>(error, message);
    ::SetErrorInfo(0, ErrorInfo.get());
    return TRUE;
}

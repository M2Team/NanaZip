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

#include <hstring.h>
#include <winstring.h>

// WINDOWS_RUNTIME_HSTRING_FLAGS

#define WRHF_NONE 0x00000000
#define WRHF_STRING_REFERENCE 0x00000001
#define WRHF_VALID_UNICODE_FORMAT_INFO 0x00000002
#define WRHF_WELL_FORMED_UNICODE 0x00000004
#define WRHF_HAS_EMBEDDED_NULLS 0x00000008
#define WRHF_EMBEDDED_NULLS_COMPUTED 0x00000010
#define WRHF_RESERVED_FOR_PREALLOCATED_STRING_BUFFER 0x80000000

typedef struct _HSTRING_HEADER_INTERNAL
{
    UINT32 Flags;
    UINT32 Length;
    UINT32 Padding1;
    UINT32 Padding2;
    PCWSTR StringRef;
} HSTRING_HEADER_INTERNAL;

#include <Mile.Helpers.CppBase.h>

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

    struct ErrorInfoFallback : public Mile::ComObject<
        ErrorInfoFallback,
        IErrorInfo,
        IRestrictedErrorInfo>
    {
    private:

        HRESULT m_Code;
        BSTR m_Message;

    public:

        ErrorInfoFallback(
            _In_ HRESULT Code,
            _In_opt_ HSTRING Message) :
            m_Code(Code),
            m_Message(::SysAllocString(Message
                ? reinterpret_cast<HSTRING_HEADER_INTERNAL*>(
                    this->m_Message)->StringRef
                : nullptr))
        {

        }

        ~ErrorInfoFallback()
        {
            ::SysFreeString(this->m_Message);
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
            *pBstrDescription = ::SysAllocString(this->m_Message);
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
            *restrictedDescription = ::SysAllocString(this->m_Message);
            return *restrictedDescription ? S_OK : E_OUTOFMEMORY;
        }

        virtual HRESULT STDMETHODCALLTYPE GetReference(
            _Out_ BSTR* reference)
        {
            *reference = nullptr;
            return S_OK;
        }
    };

    static FARPROC GetRoFailFastWithErrorContextProcAddress()
    {
        static FARPROC CachedResult = ([]() -> FARPROC
        {
            HMODULE ModuleHandle = ::GetComBaseModuleHandle();
            if (ModuleHandle)
            {
                return ::GetProcAddress(
                    ModuleHandle,
                    "RoFailFastWithErrorContext");
            }
            return nullptr;
        }());

        return CachedResult;
    }
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

    IErrorInfo* ErrorInfo = new ErrorInfoFallback(error, message);
    ::SetErrorInfo(0, ErrorInfo);
    return TRUE;
}

EXTERN_C VOID WINAPI RoFailFastWithErrorContext(
    _In_ HRESULT hrError)
{
    using ProcType = decltype(::RoFailFastWithErrorContext)*;

    ProcType ProcAddress = reinterpret_cast<ProcType>(
        ::GetRoFailFastWithErrorContextProcAddress());

    if (ProcAddress)
    {
        ProcAddress(hrError);
    }

    std::abort();
}

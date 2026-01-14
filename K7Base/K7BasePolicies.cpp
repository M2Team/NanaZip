/*
 * PROJECT:    NanaZip Platform Base Library (K7Base)
 * FILE:       K7BasePolicies.cpp
 * PURPOSE:    Implementation for NanaZip Platform Base Policies Interfaces
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: dinhngtu (contact@tudinh.xyz)
 *             MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "K7BasePrivate.h"

#include <Mile.Helpers.CppBase.h>

#include <set>
#include <string>

namespace
{
    // Note: These static variables are initialized with default policy
    // settings. Also, no thread-safety mechanism is needed here because the
    // only modification of these variables happens in the
    // K7BaseInitializePolicies function should be called only once during the
    // K7Base library initialization phase as early as possible.

    static MO_BOOL g_AllowDynamicCodeGeneration = MO_FALSE;
    static MO_BOOL g_AllowChildProcessCreation = MO_FALSE;
    static std::set<std::string> g_AllowedHandlers;
    static std::set<std::string> g_BlockedHandlers;
    static std::set<std::string> g_AllowedCodecs;
    static std::set<std::string> g_BlockedCodecs;

    static bool QueryDwordValue(
        _Out_ PDWORD Value,
        _In_ HKEY KeyHandle,
        _In_ LPCWSTR ValueName)
    {
        DWORD Type = REG_NONE;
        DWORD Length = sizeof(DWORD);
        if (ERROR_SUCCESS != ::RegQueryValueExW(
            KeyHandle,
            ValueName,
            nullptr,
            &Type,
            reinterpret_cast<LPBYTE>(Value),
            &Length))
        {
            return false;
        }
        if (REG_DWORD != Type || sizeof(DWORD) != Length)
        {
            return false;
        }
        return true;
    }

    static std::set<std::string> QueryMultiStringValue(
        _In_ HKEY KeyHandle,
        _In_ LPCWSTR ValueName)
    {
        std::set<std::string> Result;

        PMO_WIDE_CHAR Buffer = nullptr;

        do
        {
            DWORD Type = REG_NONE;
            DWORD Length = sizeof(DWORD);
            if (ERROR_SUCCESS != ::RegQueryValueExW(
                KeyHandle,
                ValueName,
                nullptr,
                &Type,
                nullptr,
                &Length))
            {
                break;
            }
            if (REG_MULTI_SZ != Type)
            {
                break;
            }

            Buffer = static_cast<PMO_WIDE_CHAR>(::MileAllocateMemory(Length));
            if (!Buffer)
            {
                break;
            }

            if (ERROR_SUCCESS != ::RegQueryValueExW(
                KeyHandle,
                ValueName,
                nullptr,
                nullptr,
                reinterpret_cast<LPBYTE>(Buffer),
                &Length))
            {
                break;
            }

            for (PMO_WIDE_CHAR Current = Buffer;
                *Current;
                Current += std::wcslen(Current) + 1)
            {
                Result.insert(Mile::ToString(CP_UTF8, std::wstring(Current)));
            }
        }
        while (false);

        if (Buffer)
        {
            ::MileFreeMemory(Buffer);
        }

        return Result;
    }
}

EXTERN_C MO_RESULT MOAPI K7BaseInitializePolicies()
{
    HKEY PoliciesKeyHandle = nullptr;
    if (ERROR_SUCCESS != ::RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"Software\\Policies\\M2Team\\NanaZip",
        0,
        KEY_READ | KEY_WOW64_64KEY,
        &PoliciesKeyHandle))
    {
        // NanaZip should use the default policy settings if the relevant
        // policies registry key cannot be opened successfully.
        return MO_RESULT_SUCCESS_OK;
    }

    {
        DWORD Value = 0;
        if (::QueryDwordValue(
            &Value,
            PoliciesKeyHandle,
            L"AllowDynamicCodeGeneration"))
        {
            if (1 == Value)
            {
                // Only when the registry value is 1, NanaZip will allow dynamic
                // code generation.
                g_AllowDynamicCodeGeneration = MO_TRUE;
            }
        }
    }

    {
        DWORD Value = 0;
        if (::QueryDwordValue(
            &Value,
            PoliciesKeyHandle,
            L"AllowChildProcessCreation"))
        {
            if (1 == Value)
            {
                // Only when the registry value is 1, NanaZip will allow child
                // process creation.
                g_AllowChildProcessCreation = MO_TRUE;
            }
        }
    }

    g_AllowedHandlers = ::QueryMultiStringValue(
        PoliciesKeyHandle,
        L"AllowedHandlers");

    g_BlockedHandlers = ::QueryMultiStringValue(
        PoliciesKeyHandle,
        L"BlockedHandlers");

    g_AllowedCodecs = ::QueryMultiStringValue(
        PoliciesKeyHandle,
        L"AllowedCodecs");

    g_BlockedCodecs = ::QueryMultiStringValue(
        PoliciesKeyHandle,
        L"BlockedCodecs");

    ::RegCloseKey(PoliciesKeyHandle);

    return MO_RESULT_SUCCESS_OK;
}

EXTERN_C MO_BOOL MOAPI K7BaseGetAllowDynamicCodeGenerationPolicy()
{
    return g_AllowDynamicCodeGeneration;
}

EXTERN_C MO_BOOL MOAPI K7BaseGetAllowChildProcessCreationPolicy()
{
    return g_AllowChildProcessCreation;
}

EXTERN_C MO_BOOL MOAPI K7BaseGetAllowedHandlerPolicy(
    _In_ MO_CONSTANT_STRING Name)
{
    std::string HandlerName = std::string(Name);
    if (!g_AllowedHandlers.empty())
    {
        return g_AllowedHandlers.contains(HandlerName) ? MO_TRUE : MO_FALSE;
    }
    if (!g_BlockedHandlers.empty())
    {
        return g_BlockedHandlers.contains(HandlerName) ? MO_FALSE : MO_TRUE;
    }
    return MO_TRUE;
}

EXTERN_C MO_BOOL MOAPI K7BaseGetAllowedCodecPolicy(
    _In_ MO_CONSTANT_STRING Name)
{
    std::string CodecName = std::string(Name);
    if (!g_AllowedCodecs.empty())
    {
        return g_AllowedCodecs.contains(CodecName) ? MO_TRUE : MO_FALSE;
    }
    if (!g_BlockedCodecs.empty())
    {
        return g_BlockedCodecs.contains(CodecName) ? MO_FALSE : MO_TRUE;
    }
    return MO_TRUE;
}

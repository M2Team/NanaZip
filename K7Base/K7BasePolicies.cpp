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

#include "K7BasePolicies.h"

#include <Windows.h>

namespace
{
    static MO_BOOL g_AllowDynamicCodeGeneration = MO_FALSE;
    static MO_BOOL g_AllowChildProcessCreation = MO_FALSE;

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
}

EXTERN_C MO_RESULT MOAPI K7BasePoliciesInitialize()
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

    ::RegCloseKey(PoliciesKeyHandle);

    return MO_RESULT_SUCCESS_OK;
}

EXTERN_C MO_BOOL MOAPI K7BasePoliciesGetAllowDynamicCodeGeneration()
{
    return g_AllowDynamicCodeGeneration;
}

EXTERN_C MO_BOOL MOAPI K7BasePoliciesGetAllowChildProcessCreation()
{
    return g_AllowChildProcessCreation;
}

EXTERN_C MO_BOOL MOAPI K7BaseIsSecurityMitigationPoliciesDisabled()
{
    static MO_BOOL CachedResult = ([]() -> MO_BOOL
    {
        DWORD Result = 0;
        DWORD Length = sizeof(DWORD);
        if (ERROR_SUCCESS != ::RegGetValueW(
            HKEY_LOCAL_MACHINE,
            L"Software\\NanaZip\\Policies",
            L"DisableMitigations",
            RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6464KEY,
            nullptr,
            &Result,
            &Length))
        {
            // The security mitigation policies should not be disabled if the
            // registry value cannot be acquired successfully to reduce the
            // security risk caused by intentional or unintentional registry
            // tampering.
            return MO_FALSE;
        }

        // According NanaZip Policies documentation, we will know:
        // - 0: Should not disable the security mitigation policies.
        // - 1: Should disable the security mitigation policies.
        // - Other values: Reserved for future use.
        // In Kenji Mouri's opinion, any reserved values should be treated as
        // not disabling the security mitigation policies to reduce the security
        // risk caused by intentional or unintentional registry tampering.
        return (1 == Result) ? MO_TRUE : MO_FALSE;
    }());
    return CachedResult;
}

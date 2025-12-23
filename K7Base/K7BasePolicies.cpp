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

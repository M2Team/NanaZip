/*
 * PROJECT:   NanaZip
 * FILE:      Mitigations.cpp
 * PURPOSE:   Implementation for applied mitigation policy
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: dinhngtu (contact@tudinh.xyz)
 *            Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#include "Mitigations.h"

#include <VersionHelpers.h>

EXTERN_C BOOL WINAPI NanaZipEnableMitigations()
{
    if (!IsWindows8OrGreater())
        return TRUE;

    HMODULE ModuleHandle = ::GetModuleHandleW(L"kernel32.dll");
    if (!ModuleHandle)
    {
        return FALSE;
    }

    decltype(::SetProcessMitigationPolicy)* my_SetProcessMitigationPolicy =
        reinterpret_cast<decltype(::SetProcessMitigationPolicy)*>(
            ::GetProcAddress(ModuleHandle, "SetProcessMitigationPolicy"));
    if (!my_SetProcessMitigationPolicy)
    {
        return FALSE;
    }

    {
        PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY Policy = { 0 };
        Policy.RaiseExceptionOnInvalidHandleReference = 1;
        Policy.HandleExceptionsPermanentlyEnabled = 1;
        if (!my_SetProcessMitigationPolicy(
            ProcessStrictHandleCheckPolicy,
            &Policy,
            sizeof(PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY)))
        {
            return FALSE;
        }
    }

#ifdef NDEBUG
    if (IsWindows8Point1OrGreater())
    {
        PROCESS_MITIGATION_DYNAMIC_CODE_POLICY Policy = { 0 };
        Policy.ProhibitDynamicCode = 1;
        if (!my_SetProcessMitigationPolicy(
            ProcessDynamicCodePolicy,
            &Policy,
            sizeof(PROCESS_MITIGATION_DYNAMIC_CODE_POLICY)))
        {
            return FALSE;
        }
    }
#endif // NDEBUG

    if (IsWindows10OrGreater())
    {
        PROCESS_MITIGATION_IMAGE_LOAD_POLICY Policy = { 0 };
        Policy.NoRemoteImages = 1;
        Policy.NoLowMandatoryLabelImages = 1;
        if (!my_SetProcessMitigationPolicy(
            ProcessImageLoadPolicy,
            &Policy,
            sizeof(PROCESS_MITIGATION_IMAGE_LOAD_POLICY)))
        {
            return FALSE;
        }
    }

    return TRUE;
}

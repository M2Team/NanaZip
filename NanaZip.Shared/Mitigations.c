#include <Windows.h>
#include <VersionHelpers.h>

#include "Mitigations.h"

typedef BOOL(WINAPI* Func_SetProcessMitigationPolicy)(
    _In_ PROCESS_MITIGATION_POLICY MitigationPolicy,
    _In_reads_bytes_(dwLength) PVOID lpBuffer,
    _In_ SIZE_T dwLength);

BOOL EnableMitigations()
{
    if (!IsWindows8OrGreater())
        return TRUE;

    HMODULE mod = GetModuleHandleW(L"kernel32.dll");
    if (!mod)
        return FALSE;

    Func_SetProcessMitigationPolicy my_SetProcessMitigationPolicy =
        (Func_SetProcessMitigationPolicy)GetProcAddress(mod, "SetProcessMitigationPolicy");
    if (!my_SetProcessMitigationPolicy)
        return FALSE;

    PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY shcp = { 0 };
    shcp.RaiseExceptionOnInvalidHandleReference = 1;
    shcp.HandleExceptionsPermanentlyEnabled = 1;
    if (!my_SetProcessMitigationPolicy(ProcessStrictHandleCheckPolicy, &shcp, sizeof(shcp)))
        return FALSE;

    if (!IsWindows8Point1OrGreater())
        return TRUE;

    PROCESS_MITIGATION_DYNAMIC_CODE_POLICY dcp = { 0 };
    dcp.ProhibitDynamicCode = 1;
    if (!my_SetProcessMitigationPolicy(ProcessDynamicCodePolicy, &dcp, sizeof(dcp)))
        return FALSE;

    if (!IsWindows10OrGreater())
        return TRUE;

    PROCESS_MITIGATION_IMAGE_LOAD_POLICY ilp = { 0 };
    ilp.NoRemoteImages = 1;
    ilp.NoLowMandatoryLabelImages = 1;
    if (!my_SetProcessMitigationPolicy(ProcessImageLoadPolicy, &ilp, sizeof(ilp)))
        return FALSE;

    return TRUE;
}

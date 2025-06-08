/*
 * PROJECT:   NanaZip
 * FILE:      Mitigations.h
 * PURPOSE:   Definition for applied mitigation policy
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: dinhngtu (contact@tudinh.xyz)
 *            MouriNaruto (KurikoMouri@outlook.jp)
 */

#ifndef NANAZIP_SHARED_MITIGATIONS
#define NANAZIP_SHARED_MITIGATIONS

#include <Windows.h>

EXTERN_C DWORD WINAPI NanaZipGetMitigationDisable();

// All of the functions below are affected by NanaZipGetMitigationDisable().
EXTERN_C BOOL WINAPI NanaZipEnableMitigations();
EXTERN_C BOOL WINAPI NanaZipDisableChildProcesses();
EXTERN_C BOOL WINAPI NanaZipSetThreadDynamicCodeOptout(
    _In_ BOOL OptOut);

#endif // !NANAZIP_SHARED_MITIGATIONS

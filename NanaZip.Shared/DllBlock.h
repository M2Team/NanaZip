/*
 * PROJECT:    NanaZip
 * FILE:       DllBlock.h
 * PURPOSE:    Definition for DLL blocker
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: dinhngtu (contact@tudinh.xyz)
 *             MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef NANAZIP_SHARED_DLLBLOCK
#define NANAZIP_SHARED_DLLBLOCK

#include <Windows.h>

// All these functions mentioned below are affected by the
// K7BaseIsSecurityMitigationPoliciesDisabled function in K7Base.

EXTERN_C BOOL WINAPI NanaZipBlockDlls();

#endif // !NANAZIP_SHARED_DLLBLOCK

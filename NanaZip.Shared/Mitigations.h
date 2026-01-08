/*
 * PROJECT:    NanaZip
 * FILE:       Mitigations.h
 * PURPOSE:    Definition for applied mitigation policy
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: dinhngtu (contact@tudinh.xyz)
 *             MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef NANAZIP_SHARED_MITIGATIONS
#define NANAZIP_SHARED_MITIGATIONS

#include <Windows.h>

EXTERN_C BOOL WINAPI NanaZipEnableMitigations();

#endif // !NANAZIP_SHARED_MITIGATIONS

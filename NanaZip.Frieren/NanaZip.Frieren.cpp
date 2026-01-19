/*
 * PROJECT:    NanaZip
 * FILE:       NanaZip.Frieren.cpp
 * PURPOSE:    Implementation for NanaZip.Frieren
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Frieren.h"

EXTERN_C VOID WINAPI NanaZipFrierenDarkModeGlobalInitialize();

EXTERN_C HRESULT WINAPI NanaZipFrierenGlobalInitialize()
{
    ::NanaZipFrierenDarkModeGlobalInitialize();
    return S_OK;
}

/*
 * PROJECT:   NanaZip
 * FILE:      NanaZip.Frieren.cpp
 * PURPOSE:   Implementation for NanaZip.Frieren
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "NanaZip.Frieren.h"

EXTERN_C VOID WINAPI NanaZipFrierenDarkModeThreadInitialize();
EXTERN_C VOID WINAPI NanaZipFrierenDarkModeThreadUninitialize();
EXTERN_C VOID WINAPI NanaZipFrierenDarkModeGlobalInitialize();
EXTERN_C VOID WINAPI NanaZipFrierenDarkModeGlobalUninitialize();

EXTERN_C HRESULT WINAPI NanaZipFrierenThreadInitialize()
{
    ::NanaZipFrierenDarkModeThreadInitialize();
    return S_OK;
}

EXTERN_C HRESULT WINAPI NanaZipFrierenThreadUninitialize()
{
    ::NanaZipFrierenDarkModeThreadUninitialize();
    return S_OK;
}

EXTERN_C HRESULT WINAPI NanaZipFrierenGlobalInitialize()
{
    ::NanaZipFrierenDarkModeGlobalInitialize();
    return S_OK;
}

EXTERN_C HRESULT WINAPI NanaZipFrierenGlobalUninitialize()
{
    ::NanaZipFrierenDarkModeGlobalUninitialize();
    return S_OK;
}

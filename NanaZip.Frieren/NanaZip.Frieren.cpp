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

EXTERN_C HRESULT WINAPI NanaZipFrierenThreadInitialize()
{
    if (g_ThreadInitialized)
    {
        return S_OK;
    }

    g_ThreadInitialized = true;

    return S_OK;
}

EXTERN_C HRESULT WINAPI NanaZipFrierenThreadUninitialize()
{
    if (!g_ThreadInitialized)
    {
        return S_OK;
    }

    g_ThreadInitialized = false;

    return S_OK;
}

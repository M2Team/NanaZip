/*
 * PROJECT:    NanaZip
 * FILE:       Restrictions.h
 * PURPOSE:    Definition for handler restriction policies
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: dinhngtu (contact@tudinh.xyz)
 */

#pragma once

#ifdef Z7_SFX
#error "This file cannot be used in SFX"
#endif

#include <Windows.h>

EXTERN_C bool NanaZipIsHandlerAllowedA(PCSTR HandlerName);
EXTERN_C bool NanaZipIsCodecAllowedA(PCSTR CodecName);

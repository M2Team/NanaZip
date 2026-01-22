/*
 * PROJECT:    NanaZip Platform User Library (K7User)
 * FILE:       K7UserRedirector.cpp
 * PURPOSE:    Implementation for NanaZip Platform User API Redirector
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "K7UserPrivate.h"

// Here are the linker-time redirections to replace the original APIs.
// Implementations only for x64 and ARM64, if you want to learn how to achieve
// that in x86, please refer to NanaZip.Shared.ModernExperienceShims project in
// the historical NanaZip versions source code.

#ifndef K7_REDIRECT
#define K7_REDIRECT(Source, Target) \
    extern "C" __declspec(selectany) void const* const __imp_##Source = \
        reinterpret_cast<void const*>(::Target); \
    __pragma(comment(linker, "/include:__imp_" #Source))
#endif // !K7_REDIRECT

K7_REDIRECT(MessageBoxW, K7UserModernMessageBoxW);
K7_REDIRECT(SHBrowseForFolderW, K7UserModernSHBrowseForFolderW);

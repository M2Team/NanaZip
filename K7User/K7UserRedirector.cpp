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

extern "C" __declspec(selectany) void const* const __imp_MessageBoxW =
    reinterpret_cast<void const*>(::K7UserModernMessageBoxW);
#pragma comment(linker, "/include:__imp_MessageBoxW")

extern "C" __declspec(selectany) void const* const __imp_SHBrowseForFolderW =
    reinterpret_cast<void const*>(::K7UserModernSHBrowseForFolderW);
#pragma comment(linker, "/include:__imp_SHBrowseForFolderW")

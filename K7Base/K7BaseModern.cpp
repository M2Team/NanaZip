/*
 * PROJECT:    NanaZip Platform Base Library (K7Base)
 * FILE:       K7BaseModern.cpp
 * PURPOSE:    Implementation for NanaZip Platform Base Modern Experiences
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "K7BasePrivate.h"

EXTERN_C BOOL WINAPI K7BaseModernFileTimeToLocalFileTime(
    _In_ CONST FILETIME* lpFileTime,
    _Out_ LPFILETIME lpLocalFileTime)
{
    if (!lpFileTime || !lpLocalFileTime)
    {
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    SYSTEMTIME UtcSystemTime = {};
    if (!::FileTimeToSystemTime(lpFileTime, &UtcSystemTime))
    {
        return FALSE;
    }

    SYSTEMTIME LocalSystemTime = {};
    if (!::SystemTimeToTzSpecificLocalTimeEx(
        nullptr,
        &UtcSystemTime,
        &LocalSystemTime))
    {
        return FALSE;
    }

    return ::SystemTimeToFileTime(&LocalSystemTime, lpLocalFileTime);
}

EXTERN_C BOOL WINAPI K7BaseModernLocalFileTimeToFileTime(
    _In_ CONST FILETIME* lpLocalFileTime,
    _Out_ LPFILETIME lpFileTime)
{
    if (!lpLocalFileTime || !lpFileTime)
    {
        ::SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    SYSTEMTIME LocalSystemTime = {};
    if (!::FileTimeToSystemTime(lpLocalFileTime, &LocalSystemTime))
    {
        return FALSE;
    }

    SYSTEMTIME UtcSystemTime = {};
    if (!::TzSpecificLocalTimeToSystemTimeEx(
        nullptr,
        &LocalSystemTime,
        &UtcSystemTime))
    {
        return FALSE;
    }

    return ::SystemTimeToFileTime(&UtcSystemTime, lpFileTime);
}

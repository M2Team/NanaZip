/*
 * PROJECT:   Mouri Internal Library Essentials
 * FILE:      Mile.Helpers.Portable.Base.Unstaged.cpp
 * PURPOSE:   Implementation for unstaged portable essential helper functions
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "Mile.Helpers.Portable.Base.Unstaged.h"

EXTERN_C MO_UINT8 MileReadUInt8(
    _In_ const void* BaseAddress)
{
    const MO_UINT8* Base = reinterpret_cast<const MO_UINT8*>(BaseAddress);
    return Base[0];
}

EXTERN_C MO_UINT16 MileReadUInt16BigEndian(
    _In_ const void* BaseAddress)
{
    const MO_UINT8* Base = reinterpret_cast<const MO_UINT8*>(BaseAddress);
    return
        (static_cast<MO_UINT16>(Base[0]) << 8) |
        (static_cast<MO_UINT16>(Base[1]));
}

EXTERN_C MO_UINT16 MileReadUInt16LittleEndian(
    _In_ const void* BaseAddress)
{
    const MO_UINT8* Base = reinterpret_cast<const MO_UINT8*>(BaseAddress);
    return
        (static_cast<MO_UINT16>(Base[0])) |
        (static_cast<MO_UINT16>(Base[1]) << 8);
}

EXTERN_C MO_UINT32 MileReadUInt32BigEndian(
    _In_ const void* BaseAddress)
{
    const MO_UINT8* Base = reinterpret_cast<const MO_UINT8*>(BaseAddress);
    return
        (static_cast<MO_UINT32>(Base[0]) << 24) |
        (static_cast<MO_UINT32>(Base[1]) << 16) |
        (static_cast<MO_UINT32>(Base[2]) << 8) |
        (static_cast<MO_UINT32>(Base[3]));
}

EXTERN_C MO_UINT32 MileReadUInt32LittleEndian(
    _In_ const void* BaseAddress)
{
    const MO_UINT8* Base = reinterpret_cast<const MO_UINT8*>(BaseAddress);
    return
        (static_cast<MO_UINT32>(Base[0])) |
        (static_cast<MO_UINT32>(Base[1]) << 8) |
        (static_cast<MO_UINT32>(Base[2]) << 16) |
        (static_cast<MO_UINT32>(Base[3]) << 24);
}


EXTERN_C MO_UINT64 MileReadUInt64BigEndian(
    _In_ const void* BaseAddress)
{
    const MO_UINT8* Base = reinterpret_cast<const MO_UINT8*>(BaseAddress);
    return
        (static_cast<MO_UINT64>(Base[0]) << 56) |
        (static_cast<MO_UINT64>(Base[1]) << 48) |
        (static_cast<MO_UINT64>(Base[2]) << 40) |
        (static_cast<MO_UINT64>(Base[3]) << 32) |
        (static_cast<MO_UINT64>(Base[4]) << 24) |
        (static_cast<MO_UINT64>(Base[5]) << 16) |
        (static_cast<MO_UINT64>(Base[6]) << 8) |
        (static_cast<MO_UINT64>(Base[7]));
}

EXTERN_C MO_UINT64 MileReadUInt64LittleEndian(
    _In_ const void* BaseAddress)
{
    const MO_UINT8* Base = reinterpret_cast<const MO_UINT8*>(BaseAddress);
    return
        (static_cast<MO_UINT64>(Base[0])) |
        (static_cast<MO_UINT64>(Base[1]) << 8) |
        (static_cast<MO_UINT64>(Base[2]) << 16) |
        (static_cast<MO_UINT64>(Base[3]) << 24) |
        (static_cast<MO_UINT64>(Base[4]) << 32) |
        (static_cast<MO_UINT64>(Base[5]) << 40) |
        (static_cast<MO_UINT64>(Base[6]) << 48) |
        (static_cast<MO_UINT64>(Base[7]) << 56);
}

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

EXTERN_C uint8_t MileReadUInt8(
    _In_ const void* BaseAddress)
{
    const uint8_t* Base = reinterpret_cast<const uint8_t*>(BaseAddress);
    return Base[0];
}

EXTERN_C uint16_t MileReadUInt16BigEndian(
    _In_ const void* BaseAddress)
{
    const uint8_t* Base = reinterpret_cast<const uint8_t*>(BaseAddress);
    return
        (static_cast<uint16_t>(Base[0]) << 8) |
        (static_cast<uint16_t>(Base[1]));
}

EXTERN_C uint16_t MileReadUInt16LittleEndian(
    _In_ const void* BaseAddress)
{
    const uint8_t* Base = reinterpret_cast<const uint8_t*>(BaseAddress);
    return
        (static_cast<uint16_t>(Base[0])) |
        (static_cast<uint16_t>(Base[1]) << 8);
}

EXTERN_C uint32_t MileReadUInt32BigEndian(
    _In_ const void* BaseAddress)
{
    const uint8_t* Base = reinterpret_cast<const uint8_t*>(BaseAddress);
    return
        (static_cast<uint32_t>(Base[0]) << 24) |
        (static_cast<uint32_t>(Base[1]) << 16) |
        (static_cast<uint32_t>(Base[2]) << 8) |
        (static_cast<uint32_t>(Base[3]));
}

EXTERN_C uint32_t MileReadUInt32LittleEndian(
    _In_ const void* BaseAddress)
{
    const uint8_t* Base = reinterpret_cast<const uint8_t*>(BaseAddress);
    return
        (static_cast<uint32_t>(Base[0])) |
        (static_cast<uint32_t>(Base[1]) << 8) |
        (static_cast<uint32_t>(Base[2]) << 16) |
        (static_cast<uint32_t>(Base[3]) << 24);
}


EXTERN_C uint64_t MileReadUInt64BigEndian(
    _In_ const void* BaseAddress)
{
    const uint8_t* Base = reinterpret_cast<const uint8_t*>(BaseAddress);
    return
        (static_cast<uint64_t>(Base[0]) << 56) |
        (static_cast<uint64_t>(Base[1]) << 48) |
        (static_cast<uint64_t>(Base[2]) << 40) |
        (static_cast<uint64_t>(Base[3]) << 32) |
        (static_cast<uint64_t>(Base[4]) << 24) |
        (static_cast<uint64_t>(Base[5]) << 16) |
        (static_cast<uint64_t>(Base[6]) << 8) |
        (static_cast<uint64_t>(Base[7]));
}

EXTERN_C uint64_t MileReadUInt64LittleEndian(
    _In_ const void* BaseAddress)
{
    const uint8_t* Base = reinterpret_cast<const uint8_t*>(BaseAddress);
    return
        (static_cast<uint64_t>(Base[0])) |
        (static_cast<uint64_t>(Base[1]) << 8) |
        (static_cast<uint64_t>(Base[2]) << 16) |
        (static_cast<uint64_t>(Base[3]) << 24) |
        (static_cast<uint64_t>(Base[4]) << 32) |
        (static_cast<uint64_t>(Base[5]) << 40) |
        (static_cast<uint64_t>(Base[6]) << 48) |
        (static_cast<uint64_t>(Base[7]) << 56);
}

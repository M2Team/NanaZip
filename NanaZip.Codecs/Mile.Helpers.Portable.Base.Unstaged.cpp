/*
 * PROJECT:    Mouri Internal Library Essentials
 * FILE:       Mile.Helpers.Portable.Base.Unstaged.cpp
 * PURPOSE:    Implementation for unstaged portable essential helper functions
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#include "Mile.Helpers.Portable.Base.Unstaged.h"

EXTERN_C MO_UINT8 MileReadUInt8(
    _In_ MO_CONSTANT_POINTER BaseAddress)
{
    CONST MO_UINT8* Base = reinterpret_cast<CONST MO_UINT8*>(BaseAddress);
    return Base[0];
}

EXTERN_C MO_UINT16 MileReadUInt16BigEndian(
    _In_ MO_CONSTANT_POINTER BaseAddress)
{
    CONST MO_UINT8* Base = reinterpret_cast<CONST MO_UINT8*>(BaseAddress);
    return
        (static_cast<MO_UINT16>(Base[0]) << 8) |
        (static_cast<MO_UINT16>(Base[1]));
}

EXTERN_C MO_UINT16 MileReadUInt16LittleEndian(
    _In_ MO_CONSTANT_POINTER BaseAddress)
{
    CONST MO_UINT8* Base = reinterpret_cast<CONST MO_UINT8*>(BaseAddress);
    return
        (static_cast<MO_UINT16>(Base[0])) |
        (static_cast<MO_UINT16>(Base[1]) << 8);
}

EXTERN_C MO_UINT32 MileReadUInt32BigEndian(
    _In_ MO_CONSTANT_POINTER BaseAddress)
{
    CONST MO_UINT8* Base = reinterpret_cast<CONST MO_UINT8*>(BaseAddress);
    return
        (static_cast<MO_UINT32>(Base[0]) << 24) |
        (static_cast<MO_UINT32>(Base[1]) << 16) |
        (static_cast<MO_UINT32>(Base[2]) << 8) |
        (static_cast<MO_UINT32>(Base[3]));
}

EXTERN_C MO_UINT32 MileReadUInt32LittleEndian(
    _In_ MO_CONSTANT_POINTER BaseAddress)
{
    CONST MO_UINT8* Base = reinterpret_cast<CONST MO_UINT8*>(BaseAddress);
    return
        (static_cast<MO_UINT32>(Base[0])) |
        (static_cast<MO_UINT32>(Base[1]) << 8) |
        (static_cast<MO_UINT32>(Base[2]) << 16) |
        (static_cast<MO_UINT32>(Base[3]) << 24);
}


EXTERN_C MO_UINT64 MileReadUInt64BigEndian(
    _In_ MO_CONSTANT_POINTER BaseAddress)
{
    CONST MO_UINT8* Base = reinterpret_cast<CONST MO_UINT8*>(BaseAddress);
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
    _In_ MO_CONSTANT_POINTER BaseAddress)
{
    CONST MO_UINT8* Base = reinterpret_cast<CONST MO_UINT8*>(BaseAddress);
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

EXTERN_C MO_INT8 MileSequenceCompare8(
    _In_ MO_UINT8 Left,
    _In_ MO_UINT8 Right)
{
    return static_cast<MO_INT8>(static_cast<MO_UINT8>(Left - Right));
}

EXTERN_C MO_INT16 MileSequenceCompare16(
    _In_ MO_UINT16 Left,
    _In_ MO_UINT16 Right)
{
    return static_cast<MO_INT16>(static_cast<MO_UINT16>(Left - Right));
}

EXTERN_C MO_INT32 MileSequenceCompare32(
    _In_ MO_UINT32 Left,
    _In_ MO_UINT32 Right)
{
    return static_cast<MO_INT32>(static_cast<MO_UINT32>(Left - Right));
}

EXTERN_C MO_INT64 MileSequenceCompare64(
    _In_ MO_UINT64 Left,
    _In_ MO_UINT64 Right)
{
    return static_cast<MO_INT64>(static_cast<MO_UINT64>(Left - Right));
}

EXTERN_C MO_UINT8 MileDecodeLeb128(
    _In_ MO_CONSTANT_POINTER BaseAddress,
    _In_ MO_UINT8 MaximumBits,
    _In_ MO_BOOL SignedMode,
    _Out_ PMO_UINT64 OutputValue)
{
    if (!BaseAddress || MaximumBits < 1 || MaximumBits > 64 || !OutputValue)
    {
        // Invalid parameters.
        return 0;
    }
    *OutputValue = 0;

    if (1 == MaximumBits && SignedMode)
    {
        // Signed mode with 1 bit is not valid.
        return 0;
    }

    MO_UINT8 MaximumByteCount = (MaximumBits + 6) / 7;

    MO_UINT8 MaximumByteValidBits = MaximumBits % 7;
    if (MaximumByteValidBits == 0)
    {
        // If the maximum bits is a multiple of 7, we can use all 7 bits of the
        // last byte.
        MaximumByteValidBits = 7;
    }

    MO_UINT8 MaximumByteMask = (1 << MaximumByteValidBits) - 1;

    CONST MO_UINT8* Base = reinterpret_cast<CONST MO_UINT8*>(BaseAddress);

    MO_UINT64 Value = 0;
    MO_UINT8 Index = 0;
    MO_UINT8 Shift = 0;
    MO_UINT8 Byte = 0;
    for (;;)
    {
        if (!(Index < MaximumByteCount))
        {
            // Exceeded maximum bytes.
            return 0;
        }

        Byte = Base[Index++];
        Value |= static_cast<MO_UINT64>(Byte & 0x7F) << Shift;
        Shift += 7;
        if (!(Byte & 0x80))
        {
            // No more bytes.
            break;
        }
    }

    if (SignedMode)
    {
        if (Shift < MaximumBits)
        {
            // Sign extend, second-highest bit is the sign bit
            if (Byte & 0x40)
            {
                Value |= static_cast<MO_UINT64>(-1) << Shift;
            }
        }
        else
        {
            // Clear the top bits beyond MaximumBits.
            Value &= (static_cast<MO_UINT64>(1) << MaximumBits) - 1;

            MO_UINT8 SignBitMask = (1 << (MaximumByteValidBits - 1));
            MO_UINT8 TopBitsMask = ~MaximumByteMask;
            MO_UINT8 TopBitsValue = 0x7F - MaximumByteMask;

            MO_BOOL IsSignBitSet = (Byte & SignBitMask);
            MO_UINT8 TopBits = (Byte & TopBitsMask);
            if ((IsSignBitSet && TopBitsValue != TopBits) ||
                (!IsSignBitSet && 0 != TopBits))
            {
                // Overflow.
                return 0;
            }

            if (IsSignBitSet)
            {
                // Sign extend the value if the sign bit is set.
                if (Value & (static_cast<MO_UINT64>(1) << (MaximumBits - 1)))
                {
                    Value |= static_cast<MO_UINT64>(-1) << MaximumBits;
                }
            }
        }
    }
    else
    {
        if (!(Shift < MaximumBits))
        {
            if (Byte & ~MaximumByteMask)
            {
                // Overflow.
                return 0;
            }
        }
    }

    *OutputValue = Value;
    return Index;
}

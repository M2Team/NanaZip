/*
 * PROJECT:    Mouri Internal Library Essentials
 * FILE:       Mile.Helpers.Portable.Base.Unstaged.h
 * PURPOSE:    Definition for unstaged portable essential helper functions
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef MILE_PORTABLE_HELPERS_BASE_UNSTAGED
#define MILE_PORTABLE_HELPERS_BASE_UNSTAGED

#include <Mile.Mobility.Portable.Types.h>

/**
 * @brief Decodes a LEB128-encoded value from the specified memory address.
 * @param BaseAddress The memory address to read. The caller must ensure that
 *                    the memory address is valid with at least
 *                    ((MaximumBits + 6) / 7) bytes of readable memory.
 * @param MaximumBits The maximum number of bits for the decoded value. This
 *                    must be between 1 and 64,
 * @param SignedMode If true, the value is treated as a signed integer; otherwise,
 *                   the value is treated as an unsigned integer.
 * @param OutputValue The memory address to store the decoded value. The caller
 *                    must ensure that the memory address is valid with at least
 *                    8 bytes of writable memory.
 * @return The number of processed bytes, or 0 if the decoding failed due to
 *         invalid parameters, overflow, or exceeding the maximum bits.
 */
MO_EXTERN_C MO_UINT8 MileDecodeLeb128(
    _Mo_In_ MO_CONSTANT_POINTER BaseAddress,
    _Mo_In_ MO_UINT8 MaximumBits,
    _Mo_In_ MO_BOOL SignedMode,
    _Mo_Out_ PMO_UINT64 OutputValue);

#endif // !MILE_PORTABLE_HELPERS_BASE_UNSTAGED

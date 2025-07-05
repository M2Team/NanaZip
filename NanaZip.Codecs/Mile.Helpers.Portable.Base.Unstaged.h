/*
 * PROJECT:   Mouri Internal Library Essentials
 * FILE:      Mile.Helpers.Portable.Base.Unstaged.h
 * PURPOSE:   Definition for unstaged portable essential helper functions
 *
 * LICENSE:   The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef MILE_PORTABLE_HELPERS_BASE_UNSTAGED
#define MILE_PORTABLE_HELPERS_BASE_UNSTAGED

#include <Mile.Mobility.Portable.Types.h>
#ifndef MILE_MOBILITY_ENABLE_MINIMUM_SAL
#include <sal.h>
#endif // !MILE_MOBILITY_ENABLE_MINIMUM_SAL

/**
 * @brief Reads an unsigned 8-bit integer from the specified memory address.
 * @param BaseAddress The memory address to read. The caller must ensure that
 *                    the memory address is valid with at least 1 byte of
*                     readable memory.
 * @return The unsigned 8-bit integer value.
 */
EXTERN_C MO_UINT8 MileReadUInt8(
    _In_ MO_CONSTANT_POINTER BaseAddress);

/**
 * @brief Reads an unsigned 16-bit integer from the specified memory address in
 *        big-endian byte order.
 * @param BaseAddress The memory address to read. The caller must ensure that
 *                    the memory address is valid with at least 2 bytes of
*                     readable memory.
 * @return The unsigned 16-bit integer value.
 */
EXTERN_C MO_UINT16 MileReadUInt16BigEndian(
    _In_ MO_CONSTANT_POINTER BaseAddress);

/**
 * @brief Reads an unsigned 16-bit integer from the specified memory address in
 *        little-endian byte order.
 * @param BaseAddress The memory address to read. The caller must ensure that
 *                    the memory address is valid with at least 2 bytes of
*                     readable memory.
 * @return The unsigned 16-bit integer value.
 */
EXTERN_C MO_UINT16 MileReadUInt16LittleEndian(
    _In_ MO_CONSTANT_POINTER BaseAddress);

/**
 * @brief Reads an unsigned 32-bit integer from the specified memory address in
 *        big-endian byte order.
 * @param BaseAddress The memory address to read. The caller must ensure that
 *                    the memory address is valid with at least 4 bytes of
*                     readable memory.
 * @return The unsigned 32-bit integer value.
 */
EXTERN_C MO_UINT32 MileReadUInt32BigEndian(
    _In_ MO_CONSTANT_POINTER BaseAddress);

/**
 * @brief Reads an unsigned 32-bit integer from the specified memory address in
 *        little-endian byte order.
 * @param BaseAddress The memory address to read. The caller must ensure that
 *                    the memory address is valid with at least 4 bytes of
*                     readable memory.
 * @return The unsigned 32-bit integer value.
 */
EXTERN_C MO_UINT32 MileReadUInt32LittleEndian(
    _In_ MO_CONSTANT_POINTER BaseAddress);

/**
 * @brief Reads an unsigned 64-bit integer from the specified memory address in
 *        big-endian byte order.
 * @param BaseAddress The memory address to read. The caller must ensure that
 *                    the memory address is valid with at least 8 bytes of
*                     readable memory.
 * @return The unsigned 64-bit integer value.
 */
EXTERN_C MO_UINT64 MileReadUInt64BigEndian(
    _In_ MO_CONSTANT_POINTER BaseAddress);

/**
 * @brief Reads an unsigned 64-bit integer from the specified memory address in
 *        little-endian byte order.
 * @param BaseAddress The memory address to read. The caller must ensure that
 *                    the memory address is valid with at least 8 bytes of
*                     readable memory.
 * @return The unsigned 64-bit integer value.
 */
EXTERN_C MO_UINT64 MileReadUInt64LittleEndian(
    _In_ MO_CONSTANT_POINTER BaseAddress);

/**
 * @brief Calculates the distance between two 8-bit unsigned integers Left and
 *        Right with sequence comparison which ignores overflow.
 * @param Left The left operand you want to calculate the distance.
 * @param Right The right operand you want to calculate the distance.
 * @return The distance between Left and Right as an 8-bit signed integer, where
 *         a positive value indicates Left is greater than Right, a negative
 *         value indicates Left is less than Right, and zero indicates they are
 *         equal.
 */
EXTERN_C MO_INT8 MileSequenceCompare8(
    _In_ MO_UINT8 Left,
    _In_ MO_UINT8 Right);

/**
 * @brief Calculates the distance between two 16-bit unsigned integers Left and
 *        Right with sequence comparison which ignores overflow.
 * @param Left The left operand you want to calculate the distance.
 * @param Right The right operand you want to calculate the distance.
 * @return The distance between Left and Right as a 16-bit signed integer, where
 *         a positive value indicates Left is greater than Right, a negative
 *         value indicates Left is less than Right, and zero indicates they are
 *         equal.
 */
EXTERN_C MO_INT16 MileSequenceCompare16(
    _In_ MO_UINT16 Left,
    _In_ MO_UINT16 Right);

/**
 * @brief Calculates the distance between two 32-bit unsigned integers Left and
 *        Right with sequence comparison which ignores overflow.
 * @param Left The left operand you want to calculate the distance.
 * @param Right The right operand you want to calculate the distance.
 * @return The distance between Left and Right as a 32-bit signed integer, where
 *         a positive value indicates Left is greater than Right, a negative
 *         value indicates Left is less than Right, and zero indicates they are
 *         equal.
 */
EXTERN_C MO_INT32 MileSequenceCompare32(
    _In_ MO_UINT32 Left,
    _In_ MO_UINT32 Right);

/**
 * @brief Calculates the distance between two 64-bit unsigned integers Left and
 *        Right with sequence comparison which ignores overflow.
 * @param Left The left operand you want to calculate the distance.
 * @param Right The right operand you want to calculate the distance.
 * @return The distance between Left and Right as a 64-bit signed integer, where
 *         a positive value indicates Left is greater than Right, a negative
 *         value indicates Left is less than Right, and zero indicates they are
 *         equal.
 */
EXTERN_C MO_INT64 MileSequenceCompare64(
    _In_ MO_UINT64 Left,
    _In_ MO_UINT64 Right);

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
EXTERN_C MO_UINT8 MileDecodeLeb128(
    _In_ MO_CONSTANT_POINTER BaseAddress,
    _In_ MO_UINT8 MaximumBits,
    _In_ MO_BOOL SignedMode,
    _Out_ PMO_UINT64 OutputValue);

#endif // !MILE_PORTABLE_HELPERS_BASE_UNSTAGED

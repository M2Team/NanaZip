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

#endif // !MILE_PORTABLE_HELPERS_BASE_UNSTAGED

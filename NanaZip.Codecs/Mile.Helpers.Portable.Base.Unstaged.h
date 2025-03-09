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

#include <stdint.h>

#ifndef EXTERN_C
#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C extern
#endif
#endif // !EXTERN_C

#ifndef _In_
#define _In_
#endif // !_In_

/**
 * @brief Reads an unsigned 8-bit integer from the specified memory address.
 * @param BaseAddress The memory address to read. The caller must ensure that
 *                    the memory address is valid with at least 1 byte of
*                     readable memory.
 * @return The unsigned 8-bit integer value.
 */
EXTERN_C uint8_t MileReadUInt8(
    _In_ const void* BaseAddress);

/**
 * @brief Reads an unsigned 16-bit integer from the specified memory address in
 *        big-endian byte order.
 * @param BaseAddress The memory address to read. The caller must ensure that
 *                    the memory address is valid with at least 2 bytes of
*                     readable memory.
 * @return The unsigned 16-bit integer value.
 */
EXTERN_C uint16_t MileReadUInt16BigEndian(
    _In_ const void* BaseAddress);

/**
 * @brief Reads an unsigned 16-bit integer from the specified memory address in
 *        little-endian byte order.
 * @param BaseAddress The memory address to read. The caller must ensure that
 *                    the memory address is valid with at least 2 bytes of
*                     readable memory.
 * @return The unsigned 16-bit integer value.
 */
EXTERN_C uint16_t MileReadUInt16LittleEndian(
    _In_ const void* BaseAddress);

/**
 * @brief Reads an unsigned 32-bit integer from the specified memory address in
 *        big-endian byte order.
 * @param BaseAddress The memory address to read. The caller must ensure that
 *                    the memory address is valid with at least 4 bytes of
*                     readable memory.
 * @return The unsigned 32-bit integer value.
 */
EXTERN_C uint32_t MileReadUInt32BigEndian(
    _In_ const void* BaseAddress);

/**
 * @brief Reads an unsigned 32-bit integer from the specified memory address in
 *        little-endian byte order.
 * @param BaseAddress The memory address to read. The caller must ensure that
 *                    the memory address is valid with at least 4 bytes of
*                     readable memory.
 * @return The unsigned 32-bit integer value.
 */
EXTERN_C uint32_t MileReadUInt32LittleEndian(
    _In_ const void* BaseAddress);

/**
 * @brief Reads an unsigned 64-bit integer from the specified memory address in
 *        big-endian byte order.
 * @param BaseAddress The memory address to read. The caller must ensure that
 *                    the memory address is valid with at least 8 bytes of
*                     readable memory.
 * @return The unsigned 64-bit integer value.
 */
EXTERN_C uint64_t MileReadUInt64BigEndian(
    _In_ const void* BaseAddress);

/**
 * @brief Reads an unsigned 64-bit integer from the specified memory address in
 *        little-endian byte order.
 * @param BaseAddress The memory address to read. The caller must ensure that
 *                    the memory address is valid with at least 8 bytes of
*                     readable memory.
 * @return The unsigned 64-bit integer value.
 */
EXTERN_C uint64_t MileReadUInt64LittleEndian(
    _In_ const void* BaseAddress);

#endif // !MILE_PORTABLE_HELPERS_BASE_UNSTAGED

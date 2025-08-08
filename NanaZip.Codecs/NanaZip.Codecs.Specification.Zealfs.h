/*
 * PROJECT:    NanaZip
 * FILE:       NanaZip.Codecs.Specification.Zealfs.h
 * PURPOSE:    Definition for the ZealFS series file system on-disk structure
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

// References
// - https://github.com/Zeal8bit/Zeal-8-bit-OS
//   - include/time_h.asm
//   - kernel_headers/sdcc/include/zos_time.h
// - https://github.com/Zeal8bit/ZealFS
//   - include/zealfs_v1.h
//   - include/zealfs_v2.h

#ifndef NANAZIP_CODECS_SPECIFICATION_ZEALFS
#define NANAZIP_CODECS_SPECIFICATION_ZEALFS

#include <Mile.Mobility.Portable.Types.h>
#ifndef MILE_MOBILITY_ENABLE_MINIMUM_SAL
#include <sal.h>
#endif // !MILE_MOBILITY_ENABLE_MINIMUM_SAL

/**
 * @brief Specifies a date and time, using individual members for the month,
 *        day, year, weekday, hour, minute, and second, with the format used
 *        in the Zeal 8-bit OS, which is using BCD encoding for all values.
 */
typedef struct _ZEALOS_TIME
{
    /**
     * @brief The year. The valid values for this member are 1970 through 2999.
     *        The Year[0] member contains the high part (hundreds) of the year,
     *        and the Year[1] member contains the low part of the year.
     */
    MO_UINT8 Year[2];

    /**
     * @brief The month. The valid values for this member are 1 through 12. The
     *        1 is January, 2 is February, and so on.
     */
    MO_UINT8 Month;

    /**
     * @brief The day of the month. The valid values for this member are 1
     *        through 31.
     */
    MO_UINT8 Day;

    /**
     * @brief The day of the week. The valid values for this member are 0
     *        through 6. The 0 is Sunday, 1 is Monday, and so on.
     */
    MO_UINT8 Date;

    /**
     * @brief The hour. The valid values for this member are 0 through 23.
     */
    MO_UINT8 Hours;

    /**
     * @brief The minute. The valid values for this member are 0 through 59.
     */
    MO_UINT8 Minutes;

    /**
     * @brief The second. The valid values for this member are 0 through 59.
     */
    MO_UINT8 Seconds;
} ZEALOS_TIME, *PZEALOS_TIME;

/**
 * @brief The signature for all ZealFS versions, must be 'Z' ascii code.
 */
#define ZEALFS_MAGIC 'Z'

/**
 * @brief The structure of the common partition header for all ZealFS versions.
 */
typedef struct _ZEALFS_COMMON_HEADER
{
    /**
     * @brief Must be ZEALFS_MAGIC.
     */
    MO_UINT8 Magic;

    /**
     * @brief Version of the file system.
     */
    MO_UINT8 Version;
} ZEALFS_COMMON_HEADER, *PZEALFS_COMMON_HEADER;

/**
 * @brief The page size for ZealFS v1.
 */
#define ZEALFS_V1_PAGE_SIZE 256

/**
 * @brief The maximum number of pages for ZealFS v1.
 */
#define ZEALFS_V1_MAXIMUM_PAGE_COUNT 256

/**
 * @brief The bitmap size for ZealFS v1.
 */
#define ZEALFS_V1_BITMAP_SIZE (ZEALFS_V1_MAXIMUM_PAGE_COUNT / 8)

/**
 * @brief The reserved size in partition header for ZealFS v1.
 */
#define ZEALFS_V1_RESERVED_SIZE 28

/**
 * @brief The minimum partition size for ZealFS v1. According to related FUSE
 *        implementation, the minimum bitmap size seems to be 1 a.k.a. 8 bits
 *        which for 8 pages.
 */
#define ZEALFS_V1_MINIMUM_PARTITION_SIZE (8 * ZEALFS_V1_PAGE_SIZE)

/**
 * @brief The partition header for ZealFS v1.
 */
typedef struct _ZEALFS_V1_HEADER
{
    /**
     * @brief The common partition header for all ZealFS versions.
     */
    ZEALFS_COMMON_HEADER Common;

    /**
     * @brief Number of bytes composing the bitmap. No matter how big the disk
     *        actually is, the bitmap is always ZEALFS_V1_BITMAP_SIZE bytes big,
     *        thus we need field to mark the actual number of bytes we will be
     *        using.
     */
    MO_UINT8 BitmapSize;

    /**
     * @brief Number of free pages.
     */
    MO_UINT8 FreePages;

    /**
     * @brief Bitmap for the free pages. A used page is marked as 1, else 0.
     *        (256 pages/8-bit = 32)
     */
    MO_UINT8 PagesBitmap[ZEALFS_V1_BITMAP_SIZE];

    /**
     * @brief Reserved bytes, to align the entries and for future use, such as
     *        extended root directory, volume name, extra bitmap, etc...
     */
    MO_UINT8 Reserved[ZEALFS_V1_RESERVED_SIZE];
} ZEALFS_V1_HEADER, *PZEALFS_V1_HEADER;

#endif // !NANAZIP_CODECS_SPECIFICATION_ZEALFS

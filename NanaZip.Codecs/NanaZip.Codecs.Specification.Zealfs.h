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
     * @brief The day of the week. The valid values for this member are 1
     *        through 7. The 1 is Sunday, 2 is Monday, and so on.
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

#endif // !NANAZIP_CODECS_SPECIFICATION_ZEALFS

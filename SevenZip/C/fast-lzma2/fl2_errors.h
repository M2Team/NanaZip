/*
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * All rights reserved.
 * Modified for FL2 by Conor McCarthy
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef FL2_ERRORS_H_398273423
#define FL2_ERRORS_H_398273423

#if defined (__cplusplus)
extern "C" {
#endif

/*===== dependency =====*/
#include <stddef.h>   /* size_t */

#include "fast-lzma2.h"

/*-****************************************
 *  error codes list
 *  note : this API is still considered unstable
 *         and shall not be used with a dynamic library.
 *         only static linking is allowed
 ******************************************/
typedef enum {
  FL2_error_no_error                = 0,
  FL2_error_GENERIC                 = 1,
  FL2_error_internal                = 2,
  FL2_error_corruption_detected     = 3,
  FL2_error_checksum_wrong          = 4,
  FL2_error_parameter_unsupported   = 5,
  FL2_error_parameter_outOfBound    = 6,
  FL2_error_lclpMax_exceeded        = 7,
  FL2_error_stage_wrong             = 8,
  FL2_error_init_missing            = 9,
  FL2_error_memory_allocation       = 10,
  FL2_error_dstSize_tooSmall        = 11,
  FL2_error_srcSize_wrong           = 12,
  FL2_error_canceled                = 13,
  FL2_error_buffer                  = 14,
  FL2_error_timedOut                = 15,
  FL2_error_maxCode                 = 20  /* never EVER use this value directly, it can change in future versions! Use FL2_isError() instead */
} FL2_ErrorCode;

/*! FL2_getErrorCode() :
    convert a `size_t` function result into a `FL2_ErrorCode` enum type,
    which can be used to compare with enum list published above */
FL2LIB_API FL2_ErrorCode FL2LIB_CALL FL2_getErrorCode(size_t functionResult);
FL2LIB_API const char* FL2LIB_CALL FL2_getErrorString(FL2_ErrorCode code);   /**< Same as FL2_getErrorName, but using a `FL2_ErrorCode` enum argument */


#if defined (__cplusplus)
}
#endif

#endif /* FL2_ERRORS_H_398273423 */

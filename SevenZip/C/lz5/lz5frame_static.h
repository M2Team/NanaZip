/*
   LZ5 auto-framing library
   Header File for static linking only
   Copyright (C) 2011-2015, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - LZ5 source repository : https://github.com/inikep/lz5
   - LZ5 public forum : https://groups.google.com/forum/#!forum/lz5c
*/

#pragma once

#if defined (__cplusplus)
extern "C" {
#endif

/* lz5frame_static.h should be used solely in the context of static linking.
 * It contains definitions which may still change overtime.
 * Never use it in the context of DLL linking.
 * */


/**************************************
*  Includes
**************************************/
#include "lz5frame.h"


/**************************************
 * Error management
 * ************************************/
#define LZ5F_LIST_ERRORS(ITEM) \
        ITEM(OK_NoError) ITEM(ERROR_GENERIC) \
        ITEM(ERROR_maxBlockSize_invalid) ITEM(ERROR_blockMode_invalid) ITEM(ERROR_contentChecksumFlag_invalid) \
        ITEM(ERROR_compressionLevel_invalid) \
        ITEM(ERROR_headerVersion_wrong) ITEM(ERROR_blockChecksum_unsupported) ITEM(ERROR_reservedFlag_set) \
        ITEM(ERROR_allocation_failed) \
        ITEM(ERROR_srcSize_tooLarge) ITEM(ERROR_dstMaxSize_tooSmall) \
        ITEM(ERROR_frameHeader_incomplete) ITEM(ERROR_frameType_unknown) ITEM(ERROR_frameSize_wrong) \
        ITEM(ERROR_srcPtr_wrong) \
        ITEM(ERROR_decompressionFailed) \
        ITEM(ERROR_headerChecksum_invalid) ITEM(ERROR_contentChecksum_invalid) \
        ITEM(ERROR_maxCode)

//#define LZ5F_DISABLE_OLD_ENUMS
#ifndef LZ5F_DISABLE_OLD_ENUMS
#define LZ5F_GENERATE_ENUM(ENUM) LZ5F_##ENUM, ENUM = LZ5F_##ENUM,
#else
#define LZ5F_GENERATE_ENUM(ENUM) LZ5F_##ENUM,
#endif
typedef enum { LZ5F_LIST_ERRORS(LZ5F_GENERATE_ENUM) } LZ5F_errorCodes;  /* enum is exposed, to handle specific errors; compare function result to -enum value */


#if defined (__cplusplus)
}
#endif

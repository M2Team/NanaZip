/* NanaZip Header File Redirection */
/*
 * Because of code deduplication we'll use zstd/bitstream.h here.
 * Thereby Lizard's own error enum must be established before zstd/bitstream.h
 * pulls in zstd/error_private.h: both share the ERROR_H_MODULE include guard,
 * so the first one included wins for the whole translation unit.
 * This will be now protected with COMPILE_TIME_ASSERT below.
 */
#ifndef LIZ_BITSTREAM_H_MODULE
#define LIZ_BITSTREAM_H_MODULE

#include "error_private.h"
#include <Zstandard/common/zstd_deps.h> /* ZSTD_memset */
#include <Zstandard/common/bitstream.h>

#define BIT_highbit32 ZSTD_highbit32

/* This would cause "negative subscript" error if condition is not valid: */
#define COMPILE_TIME_ASSERT(name, condition) \
    typedef char name[(condition) ? 1 : -1]

COMPILE_TIME_ASSERT(liz_error_enum_leaked_from_zstd, ERROR(maxCode) == (size_t)-FSE_error_maxCode);

#endif

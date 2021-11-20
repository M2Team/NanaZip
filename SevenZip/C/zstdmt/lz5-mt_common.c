
/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * Copyright (c) 2016 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "lz5frame.h"
#include "lz5-mt.h"

/* will be used for lib errors */
size_t lz5mt_errcode;

/* ****************************************
 * LZ5MT Error Management
 ******************************************/

/**
 * LZ5MT_isError() - tells if a return value is an error code
 */
unsigned LZ5MT_isError(size_t code)
{
	return (code > ERROR(maxCode));
}

/**
 * LZ5MT_getErrorString() - give error code string from function result
 */
const char *LZ5MT_getErrorString(size_t code)
{
	static const char *noErrorCode = "Unspecified lz5mt error code";

	if (LZ5F_isError(lz5mt_errcode))
		return LZ5F_getErrorName(lz5mt_errcode);

	switch ((LZ5MT_ErrorCode) (0 - code)) {
	case PREFIX(no_error):
		return "No error detected";
	case PREFIX(memory_allocation):
		return "Allocation error : not enough memory";
	case PREFIX(read_fail):
		return "Read failure";
	case PREFIX(write_fail):
		return "Write failure";
	case PREFIX(data_error):
		return "Malformed input";
	case PREFIX(frame_compress):
		return "Could not compress frame at once";
	case PREFIX(frame_decompress):
		return "Could not decompress frame at once";
	case PREFIX(compressionParameter_unsupported):
		return "Compression parameter is out of bound";
	case PREFIX(compression_library):
		return "Compression library reports failure";
	case PREFIX(maxCode):
	default:
		return noErrorCode;
	}
}

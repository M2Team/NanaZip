
/**
 * Copyright (c) 2016-present, Yann Collet, Facebook, Inc.
 * Copyright (c) 2016 - 2017 Tino Reichardt
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "encode.h"
#include "decode.h"

#include "brotli-mt.h"

/* ****************************************
 * BROMT Error Management
 ******************************************/

/**
 * BROTLIMT_isError() - tells if a return value is an error code
 */
unsigned BROTLIMT_isError(size_t code)
{
	return (code > MT_ERROR(maxCode));
}

/**
 * BROTLIMT_getErrorString() - give our error code string of result
 */
const char *BROTLIMT_getErrorString(size_t code)
{
	static const char *noErrorCode = "Unspecified brotli error code";

	switch ((BROTLIMT_ErrorCode) (0 - code)) {
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
	case PREFIX(maxCode):
	default:
		return noErrorCode;
	}
}

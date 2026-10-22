/*
Copyright 2011-2026 Frederic Langlet
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
you may obtain a copy of the License at

                http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once
#ifndef knz_Error
#define knz_Error

namespace kanzi
{

   struct Error
   {
   public:
       enum ErrorCode {
           ERR_MISSING_PARAM = 1,
           ERR_BLOCK_SIZE = 2,
           ERR_INVALID_CODEC = 3,
           ERR_CREATE_COMPRESSOR = 4,
           ERR_CREATE_DECOMPRESSOR = 5,
           ERR_OUTPUT_IS_DIR = 6,
           ERR_OVERWRITE_FILE = 7,
           ERR_CREATE_FILE = 8,
           ERR_CREATE_BITSTREAM = 9,
           ERR_OPEN_FILE = 10,
           ERR_READ_FILE = 11,
           ERR_WRITE_FILE = 12,
           ERR_PROCESS_BLOCK = 13,
           ERR_CREATE_CODEC = 14,
           ERR_INVALID_FILE = 15,
           ERR_STREAM_VERSION = 16,
           ERR_CREATE_STREAM = 17,
           ERR_INVALID_PARAM = 18,
           ERR_CRC_CHECK = 19,
           ERR_RESERVED_NAME = 20,
           ERR_UNKNOWN = 127
       };
   };

}
#endif


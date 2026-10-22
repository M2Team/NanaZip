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
#ifndef knz_InputBitStream
#define knz_InputBitStream

#include "types.hpp"

namespace kanzi
{

   class InputBitStream
   {
   public:
       // Returns 1 or 0
       virtual int readBit() = 0;

       // Length is the number of bits in [1..64]. Return the bits read as a long
       // Throws if the stream is closed.
       virtual uint64 readBits(uint length) = 0;

       // Read bits and put them in the byte array. Length is the number of bits
       // Return the number of bits read.
       // Throws if the stream is closed.
       virtual uint readBits(byte bits[], uint length) = 0;

       virtual void close() = 0;

       // Number of bits read
       virtual uint64 read() const = 0;

       // Return false when the bitstream is closed or the End-Of-Stream has been reached
       virtual bool hasMoreToRead() = 0;

       InputBitStream(){}

       virtual ~InputBitStream(){}
   };

}
#endif


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
#ifndef knz_OutputBitStream
#define knz_OutputBitStream

#include "types.hpp"

namespace kanzi
{

   class OutputBitStream
   {
   public:
       // Write the least significant bit of the input integer
       // Throws if the stream is closed.
       virtual void writeBit(int bit) = 0;

       // Length is the number of bits in [0..64]. A zero length is a successful
       // no-op regardless of the stream status. Return the number of bits written.
       // Throws if the stream is closed and length is not zero.
       virtual uint writeBits(uint64 bits, uint length) = 0;

       // Write bits ouf of the byte array. Length is the number of bits.
       // Return the number of bits written.
       // Throws if the stream is closed.
       virtual uint writeBits(const byte bits[], uint length) = 0;

       virtual void close() = 0;

       // Number of bits written
       virtual uint64 written() const = 0;

       OutputBitStream(){}

       virtual ~OutputBitStream(){}
   };

}
#endif


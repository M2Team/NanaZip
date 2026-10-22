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
#ifndef knz_EntropyDecoder
#define knz_EntropyDecoder

#include "InputBitStream.hpp"

namespace kanzi
{
   // EntropyDecoder entropy decodes data from a bitstream
   class EntropyDecoder
   {
   public:
       // Decode the array provided from the bitstream. Return the number of bytes
       // read from the bitstream
       virtual int decode(byte block[], uint blkptr, uint len) = 0;

       // Return the underlying bitstream
       virtual InputBitStream& getBitStream() const = 0;

       // Must be called before getting rid of the entropy decoder.
       // Trying to decode after a call to dispose gives undefined behavior
       virtual void dispose() = 0;

       virtual ~EntropyDecoder(){}
   };

}
#endif


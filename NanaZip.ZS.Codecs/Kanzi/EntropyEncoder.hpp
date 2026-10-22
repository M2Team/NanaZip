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
#ifndef knz_EntropyEncoder
#define knz_EntropyEncoder

#include "OutputBitStream.hpp"

namespace kanzi
{
   // EntropyEncoder entropy encodes data to a bitstream
   class EntropyEncoder
   {
   public:
       // Encode the array provided into the bitstream. Return the number of bytes
       // written to the bitstream
       virtual int encode(const byte block[], uint blkptr, uint len) = 0;

       // Return the underlying bitstream
       virtual OutputBitStream& getBitStream() const = 0;

       // Must be called before getting rid of the entropy encoder.
       // Trying to encode after a call to dispose gives undefined behavior
       virtual void dispose() = 0;

       virtual ~EntropyEncoder(){}
   };

}
#endif


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
#ifndef knz_EntropyUtils
#define knz_EntropyUtils

#include "../InputBitStream.hpp"
#include "../OutputBitStream.hpp"

namespace kanzi
{

   class EntropyUtils
   {
   private:
       static const int FULL_ALPHABET;
       static const int PARTIAL_ALPHABET;
       static const int ALPHABET_256;
       static const int ALPHABET_0;

   public:
       static const int INCOMPRESSIBLE_THRESHOLD;

       EntropyUtils() {}

       ~EntropyUtils() {}

       static int encodeAlphabet(OutputBitStream& obs, const uint alphabet[], int length, int count);

       static int decodeAlphabet(InputBitStream& ibs, uint alphabet[]);

       static int normalizeFrequencies(uint freqs[], uint alphabet[], int length, uint totalFreq, uint scale);

       static int writeVarInt(OutputBitStream& obs, uint32 val);

       static uint32 readVarInt(InputBitStream& ibs);
   };

}
#endif


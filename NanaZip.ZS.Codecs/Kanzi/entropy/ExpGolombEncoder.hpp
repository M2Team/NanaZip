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
#ifndef knz_ExpGolombEncoder
#define knz_ExpGolombEncoder

#include "../EntropyEncoder.hpp"

namespace kanzi
{

   class ExpGolombEncoder : public EntropyEncoder
   {
   private:
       static const int CACHE[2][256];
       OutputBitStream& _bitstream;
       const int _signed;

       void _dispose() const {}

   public:
       ExpGolombEncoder(OutputBitStream& bitstream, bool sign=true);

       ~ExpGolombEncoder() { _dispose(); }

       int encode(const byte block[], uint blkptr, uint len);

       OutputBitStream& getBitStream() const { return _bitstream; }

       void encodeByte(byte val);

       void dispose() { _dispose(); }

       bool isSigned() const { return _signed == 1; }
   };


   inline void ExpGolombEncoder::encodeByte(byte val)
   {
       if (val == byte(0)) {
           // shortcut when input is 0
           _bitstream.writeBit(1);
           return;
       }

       const int emit = CACHE[_signed][uint8(val)];
       _bitstream.writeBits(emit & 0x1FF, emit >> 9);
   }
}
#endif


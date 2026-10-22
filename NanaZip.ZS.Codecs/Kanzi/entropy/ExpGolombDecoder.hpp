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
#ifndef knz_ExpGolombDecoder
#define knz_ExpGolombDecoder

#include "../EntropyDecoder.hpp"

namespace kanzi
{

   class ExpGolombDecoder : public EntropyDecoder
   {
   private:
       InputBitStream& _bitstream;
       const bool _signed;

       void flush();

       void _dispose() const {}

   public:
       ExpGolombDecoder(InputBitStream& bitstream, bool sign=true);

       ~ExpGolombDecoder() { _dispose(); }

       int decode(byte arr[], uint blkptr, uint len);

       InputBitStream& getBitStream() const { return _bitstream; }

       byte decodeByte();

       void dispose() { _dispose(); }

       bool isSigned() const { return _signed; }
   };


   inline byte ExpGolombDecoder::decodeByte()
   {
       if (_bitstream.readBit() != 0)
           return byte(0);

       uint log2 = 1;

       while (_bitstream.readBit() == 0)
           log2++;

       // Clamp. Do not attempt to detect a corrupted bitstream.
       // Unsigned byte 255 requires an 8-bit suffix; signed values require at most 7.
       log2 = log2 <= 8u - uint(_signed) ? log2 : 8u - uint(_signed);
       const uint base = (1u << log2) - 1;

       if (_signed == true) {
           // Decode signed: read value + sign
           const uint res = uint(_bitstream.readBits(log2 + 1));
           const uint value = (res >> 1) + base;
           const uint sgn = res & 1;
           return byte((value ^ (0u - sgn)) + sgn); // value or -value
       }

       // Decode unsigned
       return byte(base + uint(_bitstream.readBits(log2)));
   }
}
#endif

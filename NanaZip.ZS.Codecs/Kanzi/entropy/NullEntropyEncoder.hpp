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
#ifndef knz_NullEntropyEncoder
#define knz_NullEntropyEncoder

#include "../EntropyEncoder.hpp"
#include "../OutputBitStream.hpp"

namespace kanzi {

   // Null entropy encoder
   // Pass through that writes the data directly to the bitstream
   class NullEntropyEncoder FINAL : public EntropyEncoder {
   private:
       OutputBitStream& _bitstream;


   public:
       NullEntropyEncoder(OutputBitStream& bitstream);

       ~NullEntropyEncoder() {}

       int encode(const byte block[], uint blkptr, uint len);

       void encodeByte(byte val);

       OutputBitStream& getBitStream() const { return _bitstream; }

       void dispose() {}
   };

   inline NullEntropyEncoder::NullEntropyEncoder(OutputBitStream& bitstream)
       : _bitstream(bitstream)
   {
   }

   inline int NullEntropyEncoder::encode(const byte block[], uint blkptr, uint count)
   {
      uint res = 0;

      while (count != 0) {
          const uint ckSize = (count < 1<<23) ? count : 1<<23;
          const uint w = uint(_bitstream.writeBits(&block[blkptr], 8 * ckSize) >> 3);

          if (w == 0)
             break;

          res += w;
          blkptr += w;
          count -= w;
      }

      return res;
   }

   inline void NullEntropyEncoder::encodeByte(byte val)
   {
      _bitstream.writeBits(uint64(val), 8);
   }
}
#endif


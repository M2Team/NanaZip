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
#ifndef knz_FPAQEncoder
#define knz_FPAQEncoder

#include <vector>

#include "../EntropyEncoder.hpp"
#include "../Memory.hpp"
#include "../SliceArray.hpp"

namespace kanzi
{

   // Derived from fpaq0r by Matt Mahoney & Alexander Ratushnyak.
   // See http://mattmahoney.net/dc/#fpaq0.
   // Simple (and fast) adaptive entropy bit coder
   class FPAQEncoder : public EntropyEncoder
   {
   private:
       static const uint64 TOP;
       static const uint64 MASK_0_24;
       static const uint64 MASK_0_32;
       static const uint DEFAULT_CHUNK_SIZE;
       static const uint MAX_BLOCK_SIZE;
       static const int PSCALE;

       uint64 _low;
       uint64 _high;
       bool _disposed;
       OutputBitStream& _bitstream;
       std::vector<byte> _buf;
       uint _index;
       uint16 _probs[4][256]; // probability of bit=1


       void encodeBit(int bit, uint16& prob);

       bool reset();

       void _dispose();

   public:
       FPAQEncoder(OutputBitStream& bitstream);

       ~FPAQEncoder();

       int encode(const byte block[], uint blkptr, uint count);

       OutputBitStream& getBitStream() const { return _bitstream; }

       void dispose() { _dispose(); }

       void flush();
   };


   inline void FPAQEncoder::encodeBit(int bit, uint16& prob)
   {
       // Update probabilities
       if (bit == 0) {
          _low = _low + ((((_high - _low) >> 8) * uint64(prob)) >> 8) + 1;
          prob -= uint16(prob >> 6);
       } else  {
          _high = _low + ((((_high - _low) >> 8) * uint64(prob)) >> 8);
          prob -= uint16((prob - PSCALE + 64) >> 6);
       }

       // Write unchanged first 32 bits to bitstream
       if (((_low ^ _high) >> 24) == 0)
           flush();
   }

   inline void FPAQEncoder::flush()
   {
       // Reserve space for the four-byte range coder output.
       if (size_t(_index) + 4 > _buf.size())
           _buf.resize(size_t(_index) + 4);

       BigEndian::writeInt32(&_buf[_index], int32(_high >> 24));
       _index += 4;
       _low <<= 32;
       _high = (_high << 32) | MASK_0_32;
   }
}
#endif


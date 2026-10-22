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
#ifndef knz_BinaryEntropyDecoder
#define knz_BinaryEntropyDecoder

#include "../EntropyDecoder.hpp"
#include "../Predictor.hpp"
#include "../SliceArray.hpp"

namespace kanzi
{

   // This class is a generic implementation of a bool entropy decoder
   class BinaryEntropyDecoder FINAL : public EntropyDecoder
   {
   private:
       static const uint64 TOP;
       static const uint64 MASK_0_56;
       static const uint64 MASK_0_32;
       static const int MAX_BLOCK_SIZE;
       static const int MAX_CHUNK_SIZE;

       Predictor* _predictor;
       uint64 _low;
       uint64 _high;
       uint64 _current;
       InputBitStream& _bitstream;
       bool _deallocate;
       SliceArray<byte> _sba;
       uint _payloadEnd;

       void read();

       void ensureCapacity(int required);

       void _dispose() const {}

   public:
       BinaryEntropyDecoder(InputBitStream& bitstream, Predictor* predictor, bool deallocate=true);

       ~BinaryEntropyDecoder();

       int decode(byte block[], uint blkptr, uint count);

       InputBitStream& getBitStream() const { return _bitstream; }

       void dispose() { _dispose(); }

       byte decodeByte();

       int decodeBit(int pred = 2048);
   };


   inline int BinaryEntropyDecoder::decodeBit(int pred)
   {
       // Calculate interval split
       const uint64 split = ((((_high - _low) >> 4) * uint64(pred)) >> 8) + _low;
       int bit;

       // Update predictor
       if (split >= _current) {
           bit = 1;
           _high = split;
           _predictor->update(1);
       }
       else {
           bit = 0;
           _low = split + 1;
           _predictor->update(0);
       }

       // Read 32 bits from bitstream
       if (((_low ^ _high) >> 24) == 0)
           read();

       return bit;
   }

}
#endif

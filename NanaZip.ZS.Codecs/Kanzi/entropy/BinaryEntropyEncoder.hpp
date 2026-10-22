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
#ifndef knz_BinaryEntropyEncoder
#define knz_BinaryEntropyEncoder

#include "../EntropyEncoder.hpp"
#include "../Predictor.hpp"
#include "../SliceArray.hpp"

namespace kanzi
{

   // This class is a generic implementation of a bool entropy encoder
   class BinaryEntropyEncoder FINAL : public EntropyEncoder
   {
   private:
       static const uint64 TOP;
       static const uint64 MASK_0_24;
       static const uint64 MASK_0_32;
       static const int MAX_BLOCK_SIZE;
       static const int MAX_CHUNK_SIZE;

       Predictor* _predictor;
       uint64 _low;
       uint64 _high;
       OutputBitStream& _bitstream;
       bool _disposed;
       bool _deallocate;
       SliceArray<byte> _sba;

       void _dispose();

       void ensureCapacity(int required);

       void flush();

   public:
       BinaryEntropyEncoder(OutputBitStream& bitstream, Predictor* predictor, bool deallocate=true);

       ~BinaryEntropyEncoder();

       int encode(const byte block[], uint blkptr, uint count);

       OutputBitStream& getBitStream() const { return _bitstream; }

       void dispose() { _dispose(); }

       void encodeByte(byte val);

       void encodeBit(int bit, int pred = 2048);
   };


   inline void BinaryEntropyEncoder::encodeBit(int bit, int pred)
   {
       // Update fields with new interval bounds and predictor
       const uint64 mid = _low + ((((_high - _low) >> 4) * uint64(pred)) >> 8);
       (bit != 0) ? _high = mid : _low = mid + 1;
       _predictor->update(bit != 0);

       // Write unchanged first 32 bits to bitstream
       if (((_low ^ _high) >> 24) == 0)
           flush();
   }
}
#endif

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
#ifndef knz_RangeDecoder
#define knz_RangeDecoder

#include "../EntropyDecoder.hpp"


namespace kanzi
{

   // Based on Order 0 range coder by Dmitry Subbotin itself derived from the algorithm
   // described by G.N.N Martin in his seminal article in 1979.
   // [G.N.N. Martin on the Data Recording Conference, Southampton, 1979]
   // Optimized for speed.

   class RangeDecoder : public EntropyDecoder {
   public:
       static const int DECODING_BATCH_SIZE;
       static const int DECODING_MASK;

       RangeDecoder(InputBitStream& bitstream, int chunkSize = DEFAULT_CHUNK_SIZE);

       ~RangeDecoder() { _dispose(); if (_f2s != nullptr) delete[] _f2s; }

       int decode(byte block[], uint blkptr, uint len);

       InputBitStream& getBitStream() const { return _bitstream; }

       void dispose() { _dispose(); }

   private:
       static const uint64 TOP_RANGE;
       static const uint64 BOTTOM_RANGE;
       static const uint64 RANGE_MASK;
       static const int DEFAULT_CHUNK_SIZE;
       static const int DEFAULT_LOG_RANGE;
       static const int MAX_CHUNK_SIZE;

       uint64 _code;
       uint64 _low;
       uint64 _range;
       uint _alphabet[256];
       uint _freqs[256];
       uint64 _cumFreqs[257];
       short* _f2s;
       int _lenF2S;
       InputBitStream& _bitstream;
       uint _chunkSize;
       uint _shift;

       int decodeHeader(uint frequencies[]);

       byte decodeByte();

       bool reset();

       void _dispose() const {}
   };

}
#endif


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
#ifndef knz_RangeEncoder
#define knz_RangeEncoder

#include "../EntropyEncoder.hpp"


namespace kanzi
{

   // Based on Order 0 range coder by Dmitry Subbotin itself derived from the algorithm
   // described by G.N.N Martin in his seminal article in 1979.
   // [G.N.N. Martin on the Data Recording Conference, Southampton, 1979]
   // Optimized for speed.

   class RangeEncoder : public EntropyEncoder
   {
   public:
       RangeEncoder(OutputBitStream& bitstream, int chunkSize = DEFAULT_CHUNK_SIZE, int logRange=DEFAULT_LOG_RANGE);

       ~RangeEncoder() { _dispose(); }

       int encode(const byte block[], uint blkptr, uint len);

       OutputBitStream& getBitStream() const { return _bitstream; }

       void dispose() { _dispose(); }

   private:
       static const uint64 TOP_RANGE;
       static const uint64 BOTTOM_RANGE;
       static const uint64 RANGE_MASK;
       static const int DEFAULT_CHUNK_SIZE;
       static const int DEFAULT_LOG_RANGE;
       static const int MAX_CHUNK_SIZE;

       uint64 _low;
       uint64 _range;
       uint _alphabet[256];
       uint _freqs[256];
       uint64 _cumFreqs[257];
       OutputBitStream& _bitstream;
       uint _chunkSize;
       uint _logRange;
       uint _shift;

       int rebuildStatistics(const byte block[], int start, int end, int lr);

       int updateFrequencies(uint frequencies[], int size, int lr);

       void encodeByte(byte b);

       bool encodeHeader(int alphabetSize, const uint alphabet[], const uint frequencies[], int lr) const;

       bool reset();

       void _dispose() const {}
   };

}
#endif


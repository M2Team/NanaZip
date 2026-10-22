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
#ifndef knz_ANSRangeDecoder
#define knz_ANSRangeDecoder

#include "../EntropyDecoder.hpp"
#include "../types.hpp"


// Implementation of an Asymmetric Numeral System decoder.
// See "Asymmetric Numeral System" by Jarek Duda at http://arxiv.org/abs/0902.0271
// Some code has been ported from https://github.com/rygorous/ryg_rans
// For an alternate C implementation example, see https://github.com/Cyan4973/FiniteStateEntropy

namespace kanzi
{

   struct ANSDecSymbol
   {
      void reset(int cumFreq, int freq, int logRange);

      uint16 _cumFreq;
      uint16 _freq;
   };


   class ANSRangeDecoder : public EntropyDecoder {
   public:
      static const uint ANS_TOP;

      ANSRangeDecoder(InputBitStream& bitstream,
                      int order = 0,
                      int chunkSize = DEFAULT_ANS0_CHUNK_SIZE);

      ~ANSRangeDecoder();

      int decode(byte block[], uint blkptr, uint len);

      InputBitStream& getBitStream() const { return _bitstream; }

      void dispose() { _dispose(); }


   private:
      static const int DEFAULT_ANS0_CHUNK_SIZE;
      static const int DEFAULT_LOG_RANGE;
      static const int MIN_CHUNK_SIZE;
      static const int MAX_CHUNK_SIZE;

      InputBitStream& _bitstream;
      uint* _freqs;
      uint8* _f2s;
      int _f2sSize;
      ANSDecSymbol* _symbols;
      byte* _buffer;
      uint _bufferSize;
      uint _chunkSize;
      uint _order;
      uint _logRange;

      bool decodeChunk(byte block[], uint count);

      uint decodeSymbol(byte*& p, uint& st, const ANSDecSymbol& sym, const int mask) const;

      int decodeHeader(uint frequencies[], uint alphabet[]);

      void _dispose() const {}
   };


   inline void ANSDecSymbol::reset(int cumFreq, int freq, int logRange)
   {
       _cumFreq = uint16(cumFreq);
       _freq = (freq >= (1 << logRange)) ? uint16((1 << logRange) - 1) : uint16(freq); // Mirror encoder
   }


   inline uint ANSRangeDecoder::decodeSymbol(byte*& p, uint& st, const ANSDecSymbol& sym, const int mask) const
   {
      // Compute next ANS state
      // D(x) = (s, q_s (x/M) + mod(x,M) - b_s) where s is such b_s <= x mod M < b_{s+1}
      st = uint(sym._freq) * (st >> _logRange) + (st & mask) - uint(sym._cumFreq);

      // Normalize
      const int x = (st < ANS_TOP) ? -1 : 0;
      st = (st << (x & 16)) | (x & ((uint(p[0]) << 8) | uint(p[1])));
      p -= (x + x);
      return st;
   }

}
#endif


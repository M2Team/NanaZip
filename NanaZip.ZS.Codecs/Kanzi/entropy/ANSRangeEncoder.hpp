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
#ifndef knz_ANSRangeEncoder
#define knz_ANSRangeEncoder

#include "../EntropyEncoder.hpp"


// Implementation of an Asymmetric Numeral System encoder.
// See "Asymmetric Numeral System" by Jarek Duda at http://arxiv.org/abs/0902.0271
// Some code has been ported from https://github.com/rygorous/ryg_rans
// For an alternate C implementation example, see https://github.com/Cyan4973/FiniteStateEntropy

namespace kanzi
{

   struct ANSEncSymbol
   {
      void reset(int cumFreq, int freq, uint logRange);

      int _xMax; // (Exclusive) upper bound of pre-normalization interval
      int _bias; // Bias
      int _cmplFreq; // Complement of frequency: (1 << scale_bits) - freq
      int _invShift; // Reciprocal shift
      uint64 _invFreq; // Fixed-point reciprocal frequency
   };


   class ANSRangeEncoder : public EntropyEncoder
   {
   public:
       static const int ANS_TOP;

       ANSRangeEncoder(OutputBitStream& bitstream,
                      int order = 0,
                      int chunkSize = DEFAULT_ANS0_CHUNK_SIZE,
                      int logRange = DEFAULT_LOG_RANGE);

       ~ANSRangeEncoder();

       int updateFrequencies(uint frequencies[], uint lr);

       int encode(const byte block[], uint blkptr, uint len);

       OutputBitStream& getBitStream() const { return _bitstream; }

       void dispose() { _dispose(); }


   private:
       static const int DEFAULT_ANS0_CHUNK_SIZE;
       static const int DEFAULT_LOG_RANGE;
       static const int MIN_CHUNK_SIZE;
       static const int MAX_CHUNK_SIZE;

       ANSEncSymbol* _symbols;
       uint* _freqs;
       byte* _buffer;
       uint _bufferSize;
       OutputBitStream& _bitstream;
       uint _chunkSize;
       uint _logRange;
       uint _order;


       int rebuildStatistics(const byte block[], int end, uint lr);

       void encodeChunk(const byte block[], int end);

       int encodeSymbol(byte*& p, int& st, const ANSEncSymbol& sym) const;

       bool encodeHeader(int alphabetSize, const uint alphabet[], const uint frequencies[], uint lr) const;

       void _dispose() const {}
   };


   inline void ANSEncSymbol::reset(int cumFreq, int freq, uint logRange)
   {
      // Make sure xMax is a positive int32. Compatibility with Java implementation
      if (freq >= 1 << logRange)
         freq = (1 << logRange) - 1;

      _xMax = ((ANSRangeEncoder::ANS_TOP >> logRange) << 16) * freq;
      _cmplFreq = (1 << logRange) - freq;

      if (freq < 2) {
         _invFreq = uint64(0xFFFFFFFF);
         _invShift = 32;
         _bias = cumFreq + (1 << logRange) - 1;
      }
      else {
         int shift = 0;

         while (freq > (1 << shift))
               shift++;

         // Alverson, "Integer Division using reciprocals"
         _invFreq = (((uint64(1) << (shift + 31)) + freq - 1) / freq) & uint64(0xFFFFFFFF);
         _invShift = 32 + shift - 1;
         _bias = cumFreq;
      }
   }

   inline int ANSRangeEncoder::encodeSymbol(byte*& p, int& st, const ANSEncSymbol& sym) const
   {
      const int x = (st >= sym._xMax) ? 1 : 0;
      *p = byte(st);
      p -= x;
      *p = byte(st >> 8);
      p -= x;
      st >>= (-x & 16);

      // Compute next ANS state
      // C(s,x) = M floor(x/q_s) + mod(x,q_s) + b_s where b_s = q_0 + ... + q_{s-1}
      // st = ((st / freq) << lr) + (st % freq) + cumFreq;
      return st + sym._bias + int((st * sym._invFreq) >> sym._invShift) * sym._cmplFreq;
   }
}
#endif


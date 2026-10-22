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
#ifndef knz_HuffmanEncoder
#define knz_HuffmanEncoder

#include "HuffmanCommon.hpp"
#include "../EntropyEncoder.hpp"


namespace kanzi
{

   // Implementation of a static Huffman encoder.
   // Uses in place generation of canonical codes instead of a tree
   class HuffmanEncoder : public EntropyEncoder
   {
   public:
       HuffmanEncoder(OutputBitStream& bitstream, int chunkSize = HuffmanCommon::MAX_CHUNK_SIZE);

       ~HuffmanEncoder() { _dispose(); if (_buffer != nullptr) delete[] _buffer; }

       int updateFrequencies(uint frequencies[]);

       int encode(const byte block[], uint blkptr, uint len);

       OutputBitStream& getBitStream() const { return _bitstream; }

       void dispose() { _dispose(); }


   private:
       OutputBitStream& _bitstream;
       uint16 _codes[256];
       int _chunkSize;
       byte* _buffer;
       uint _bufferSize;

       void encodeChunk(const byte block[], uint count);

       int computeCodeLengths(uint16 sizes[], uint sranks[], int count) const;

       int limitCodeLengths(const uint alphabet[], uint freqs[], uint16 sizes[], uint sranks[], int count) const;

       void _dispose() const {}

       bool reset();

       static void computeInPlaceSizesPhase1(uint data[], int n);

       static uint computeInPlaceSizesPhase2(uint data[], int n);
   };

}
#endif


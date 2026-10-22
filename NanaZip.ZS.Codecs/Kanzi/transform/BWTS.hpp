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
#ifndef knz_BWTS
#define knz_BWTS

#include "../Context.hpp"
#include "../Transform.hpp"
#include "DivSufSort.hpp"


namespace kanzi
{

   // Bijective version of the Burrows-Wheeler Transform
   // The main advantage over the regular BWT is that there is no need for a primary
   // index (hence the bijectivity). BWTS is about 10% slower than BWT.
   // Forward transform based on the code at https://code.google.com/p/mk-bwts/
   // by Neal Burns and DivSufSort (port of libDivSufSort by Yuta Mori)

   class BWTS FINAL : public Transform<byte> {

   private:
       static const int MAX_BLOCK_SIZE;

       int* _buffer1;
       int* _buffer2;
       int _bufferSize;
       DivSufSort _saAlgo;

       int moveLyndonWordHead(int sa[], int isa[], const byte data[],
                              int count, int start, int size, int rank) const;

   public:
       BWTS()
       {
           _buffer1 = nullptr;
           _buffer2 = nullptr;
           _bufferSize = 0;
       }

       BWTS(Context&)
       {
           _buffer1 = nullptr;
           _buffer2 = nullptr;
           _bufferSize = 0;
       }

       ~BWTS()
       {
          if (_buffer1 != nullptr) delete[] _buffer1;
          if (_buffer2 != nullptr) delete[] _buffer2;
       }

       bool forward(SliceArray<byte>& input, SliceArray<byte>& output, int length);

       bool inverse(SliceArray<byte>& input, SliceArray<byte>& output, int length);

       int getMaxEncodedLength(int srcLen) const { return srcLen; }
   };

}
#endif


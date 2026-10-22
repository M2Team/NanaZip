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
#ifndef knz_ZRLT
#define knz_ZRLT

#include "../Context.hpp"
#include "../Transform.hpp"

namespace kanzi
{
   // Zero Run Length Encoding is a simple encoding algorithm by Wheeler
   // closely related to Run Length Encoding. The main difference is
   // that only runs of 0 values are processed. Also, the length is
   // encoded in a different way (each digit in a different byte)
   // This algorithm is well adapted to process post BWT/MTFT data.

   class ZRLT FINAL : public Transform<byte>
   {
   public:
       ZRLT() {}
       ZRLT(Context&) {}
       ~ZRLT() {}

       bool forward(SliceArray<byte>& pSrc, SliceArray<byte>& pDst, int length);

       bool inverse(SliceArray<byte>& pSrc, SliceArray<byte>& pDst, int length);

       // Required encoding output buffer size unknown => guess
       int getMaxEncodedLength(int srcLen) const { return srcLen; }
   };

}
#endif


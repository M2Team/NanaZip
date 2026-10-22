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
#ifndef knz_SBRT
#define knz_SBRT

#include "../Context.hpp"
#include "../Transform.hpp"


namespace kanzi
{
   // Sort by Rank Transform is a family of transforms typically used after
   // a BWT to reduce the variance of the data prior to entropy coding.
   // SBR(alpha) is defined by sbr(x, alpha) = (1-alpha)*(t-w1(x,t)) + alpha*(t-w2(x,t))
   // where x is an item in the data list, t is the current access time and wk(x,t) is
   // the k-th access time to x at time t (with 0 <= alpha <= 1).
   // See [Two new families of list update algorithms] by Frank Schulz for details.
   // SBR(0)= Move to Front Transform
   // SBR(1)= Time Stamp Transform
   // This code implements SBR(0), SBR(1/2) and SBR(1). Code derived from openBWT
   class SBRT FINAL : public Transform<byte>
   {
   public:
       static const int MODE_MTF;
       static const int MODE_RANK;
       static const int MODE_TIMESTAMP;

       SBRT(int mode);
       SBRT(int mode, Context&);
       ~SBRT() {}

       bool forward(SliceArray<byte>& input, SliceArray<byte>& output, int length);

       bool inverse(SliceArray<byte>& input, SliceArray<byte>& output, int length);

       int getMaxEncodedLength(int srcLen) const { return srcLen; }

   private:

       const int _mask1;
       const int _mask2;
       const int _shift;
   };

}
#endif


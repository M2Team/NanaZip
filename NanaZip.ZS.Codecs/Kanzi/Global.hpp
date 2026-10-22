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
#ifndef knz_Global
#define knz_Global

#include <set>
#include <string>

#include "types.hpp"

namespace kanzi {

   class Global {
   public:
       enum DataType { UNDEFINED, TEXT, MULTIMEDIA, EXE, NUMERIC, BASE64, DNA, BIN, UTF8, SMALL_ALPHABET };

       static int stretch(int d); // ln(x / (1 - x))

       static int squash(int d); // 1 / (1 + e-x)  (inverse of stretch)

       static int log2(uint32 x); // fast, integer rounded

       static int log2(uint64 x); // fast, integer rounded

       static int _log2(uint32 x); // same as log2 minus check on input value

       static int _log2(uint64 x); // same as log2 minus check on input value

       static int trailingZeros(uint32 x);

       static int trailingZeros(uint64 x);

       static int log2_1024(uint32 x); // slow, accurate to 1/1024th

       static void computeJobsPerTask(int jobsPerTask[], int jobs, int tasks);

       static int computeOrder0Entropy1024(int blockLen, const uint histo[]);

       static int computeOrder1Entropy1024(int blockLen, const uint histo[]);

       static void computeHistogram(const byte block[], int end, uint freqs[], bool isOrder0=true, bool withTotal=false);

       static DataType detectSimpleType(int count, const uint histo[]);

       static bool isReservedName(std::string fileName);

   private:
       Global();
       ~Global() {}

       static const Global _singleton;
       static const int LOG2_4096[257]; // 4096*Math.log2(x)
       static const int LOG2[256]; // int(Math.log2(x-1))
       static int STRETCH[4096];
       static int SQUASH[4096];
       static char BASE64_SYMBOLS[];
       static char DNA_SYMBOLS[];
       static char NUMERIC_SYMBOLS[];

       std::set<std::string> _reservedNames;
   };


   // return p = 1/(1 + exp(-d)), d scaled by 8 bits, p scaled by 12 bits
   inline int Global::squash(int d)
   {
       if (d >= 2048)
           return 4095;

       return (d <= -2048) ? 0 : SQUASH[d + 2047];
   }

   inline int Global::stretch(int d)
   {
       return STRETCH[d];
   }

   // x cannot be 0
   inline int Global::_log2(uint32 x)
   {
       #if defined(_MSC_VER)
           unsigned long res;
           _BitScanReverse(&res, x);
           return int(res);
       #elif defined(__GNUG__) || defined(__clang__)
           return 31 ^ __builtin_clz(x);
       #else
           int res = 0;

           if (x >= 1 << 16) {
              x >>= 16;
              res = 16;
           }

           if (x >= 1 << 8) {
              x >>= 8;
              res += 8;
           }

           return res + Global::LOG2[x - 1];
       #endif
   }


   // x cannot be 0
   inline int Global::_log2(uint64 x)
   {
       #if defined(_MSC_VER) && defined(_M_AMD64)
           unsigned long res;
           _BitScanReverse64(&res, x);
           return int(res);
       #elif defined(__GNUG__) || defined(__clang__)
           return 63 ^ __builtin_clzll(x);
       #else
           int res = 0;

           if (x >= uint64(1) << 32) {
              x >>= 32;
              res = 32;
           }

           if (x >= uint64(1) << 16) {
              x >>= 16;
              res += 16;
           }

           if (x >= uint64(1) << 8) {
              x >>= 8;
              res += 8;
           }

           return res + Global::LOG2[x - 1];
       #endif
   }


   // x cannot be 0
   inline int Global::trailingZeros(uint32 x)
   {
       #if defined(_MSC_VER)
           unsigned long res;
           _BitScanForward(&res, x);
           return int(res);
       #elif defined(__GNUG__) || defined(__clang__)
           return __builtin_ctz(x);
       #else
           return _log2(x & (~x + 1));
       #endif
   }


   // x cannot be 0
   inline int Global::trailingZeros(uint64 x)
   {
       #if defined(_MSC_VER) && defined(_M_AMD64)
           unsigned long res;
           _BitScanForward64(&res, x);
           return int(res);
       #elif defined(__GNUG__) || defined(__clang__)
           return __builtin_ctzll(x);
       #else
           return _log2(x & (~x + 1));
       #endif
   }
}
#endif

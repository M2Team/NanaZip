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
#ifndef knz_XXHash32
#define knz_XXHash32

#include <ctime>

#include "../Memory.hpp"


namespace kanzi
{

   // XXHash is an extremely fast hash algorithm. It was written by Yann Collet.
   // Original source code: https://github.com/Cyan4973/xxHash

   class XXHash32 
   {
   private:
#if __cplusplus >= 201103L
          static constexpr uint32 PRIME32_1 = uint32(-1640531535);
          static constexpr uint32 PRIME32_2 = uint32(-2048144777);
          static constexpr uint32 PRIME32_3 = uint32(-1028477379);
          static constexpr uint32 PRIME32_4 = uint32(668265263);
          static constexpr uint32 PRIME32_5 = uint32(374761393);
#else
          static const uint32 PRIME32_1 = uint32(-1640531535);
          static const uint32 PRIME32_2 = uint32(-2048144777);
          static const uint32 PRIME32_3 = uint32(-1028477379);
          static const uint32 PRIME32_4 = uint32(668265263);
          static const uint32 PRIME32_5 = uint32(374761393);
#endif 

       uint32 _seed;

       uint32 round(uint32 acc, int32 val) const;

   public:
       XXHash32() { _seed = uint32(time(nullptr)); }
       XXHash32(uint32 seed) : _seed(seed) {}
       ~XXHash32(){}

       void setSeed(uint32 seed) { _seed = seed; }
       uint32 hash(const byte data[], int length) const;
   };

   inline uint32 XXHash32::hash(const byte data[], int length) const
   {
       uint32 h32;
       int idx = 0;

       if (length >= 16) {
           const int end16 = length - 16;
           uint32 v1 = _seed + PRIME32_1 + PRIME32_2;
           uint32 v2 = _seed + PRIME32_2;
           uint32 v3 = _seed;
           uint32 v4 = _seed - PRIME32_1;

           do {
               v1 = round(v1, LittleEndian::readInt32(&data[idx]));
               v2 = round(v2, LittleEndian::readInt32(&data[idx + 4]));
               v3 = round(v3, LittleEndian::readInt32(&data[idx + 8]));
               v4 = round(v4, LittleEndian::readInt32(&data[idx + 12]));
               idx += 16;
           } while (idx <= end16);

           h32 = ((v1 << 1) | (v1 >> 31));
           h32 += ((v2 << 7) | (v2 >> 25));
           h32 += ((v3 << 12) | (v3 >> 20));
           h32 += ((v4 << 18) | (v4 >> 14));
       }
       else {
           h32 = _seed + PRIME32_5;
       }

       h32 += uint32(length);

       while (idx <= length - 4) {
           h32 += (uint32(LittleEndian::readInt32(&data[idx])) * PRIME32_3);
           h32 = ((h32 << 17) | (h32 >> 15)) * PRIME32_4;
           idx += 4;
       }

       while (idx < length) {
           h32 += ((uint32(data[idx]) & 0xFF) * PRIME32_5);
           h32 = ((h32 << 11) | (h32 >> 21)) * PRIME32_1;
           idx++;
       }

       h32 ^= (h32 >> 15);
       h32 *= PRIME32_2;
       h32 ^= (h32 >> 13);
       h32 *= PRIME32_3;
       return h32 ^ (h32 >> 16);
   }

   inline uint32 XXHash32::round(uint32 acc, int32 val) const
   {
       acc += (uint32(val) * PRIME32_2);
       return ((acc << 13) | (acc >> 19)) * PRIME32_1;
   }




   class XXHash64
   {
   private:
#if __cplusplus >= 201103L
         static constexpr uint64 PRIME64_1 = uint64(0x9E3779B185EBCA87);
         static constexpr uint64 PRIME64_2 = uint64(0xC2B2AE3D27D4EB4F);
         static constexpr uint64 PRIME64_3 = uint64(0x165667B19E3779F9);
         static constexpr uint64 PRIME64_4 = uint64(0x85EBCA77C2b2AE63);
         static constexpr uint64 PRIME64_5 = uint64(0x27D4EB2F165667C5);
#else
         static const uint64 PRIME64_1 = uint64(0x9E3779B185EBCA87);
         static const uint64 PRIME64_2 = uint64(0xC2B2AE3D27D4EB4F);
         static const uint64 PRIME64_3 = uint64(0x165667B19E3779F9);
         static const uint64 PRIME64_4 = uint64(0x85EBCA77C2b2AE63);
         static const uint64 PRIME64_5 = uint64(0x27D4EB2F165667C5);
#endif

       int64 _seed;

       uint64 round(uint64 acc, uint64 val) const;
       uint64 mergeRound(uint64 acc, uint64 val) const;


   public:
       XXHash64() { _seed = int64(time(nullptr)); }
       XXHash64(int64 seed) : _seed(seed) {}
       ~XXHash64(){}

       void setSeed(int64 seed) { _seed = seed; }
       uint64 hash(const byte data[], int length) const;
     };


     inline uint64 XXHash64::hash(const byte data[], int length) const
     {
        uint64 h64;
        int idx = 0;

        if (length >= 32) {
           const int length32 = length - 32;
           uint64 v1 = _seed + PRIME64_1 + PRIME64_2;
           uint64 v2 = _seed + PRIME64_2;
           uint64 v3 = _seed;
           uint64 v4 = _seed - PRIME64_1;

           do {
              v1 = round(v1, uint64(LittleEndian::readLong64(&data[idx])));
              v2 = round(v2, uint64(LittleEndian::readLong64(&data[idx + 8])));
              v3 = round(v3, uint64(LittleEndian::readLong64(&data[idx + 16])));
              v4 = round(v4, uint64(LittleEndian::readLong64(&data[idx + 24])));
              idx += 32;
           }
           while (idx <= length32);

           h64  = ((v1 << 1)  | (v1 >> 31)) + ((v2 << 7)  | (v2 >> 25)) +
                  ((v3 << 12) | (v3 >> 20)) + ((v4 << 18) | (v4 >> 14));

           h64 = mergeRound(h64, v1);
           h64 = mergeRound(h64, v2);
           h64 = mergeRound(h64, v3);
           h64 = mergeRound(h64, v4);
         }
         else {
            h64 = _seed + PRIME64_5;
         }

         h64 += length;

         while (idx+8 <= length) {
            h64 ^= round(0, uint64(LittleEndian::readLong64(&data[idx])));
            h64 = ((h64 << 27) | (h64 >> 37)) * PRIME64_1 + PRIME64_4;
            idx += 8;
         }

         while (idx+4 <= length) {
            h64 ^= (uint32(LittleEndian::readInt32(&data[idx])) * PRIME64_1);
            h64 = ((h64 << 23) | (h64 >> 41)) * PRIME64_2 + PRIME64_3;
            idx += 4;
         }

         while (idx < length) {
            h64 ^= (uint64(data[idx] & byte(0xFF)) * PRIME64_5);
            h64 = ((h64 << 11) | (h64 >> 53)) * PRIME64_1;
            idx++;
         }

         // Finalize
         h64 ^= (h64 >> 33);
         h64 *= PRIME64_2;
         h64 ^= (h64 >> 29);
         h64 *= PRIME64_3;
         return h64 ^ (h64 >> 32);
      }


      inline uint64 XXHash64::round(uint64 acc, uint64 val) const
      {
         acc += (val*PRIME64_2);
         return ((acc << 31) | (acc >> 33)) * PRIME64_1;
      }


      inline uint64 XXHash64::mergeRound(uint64 acc, uint64 val) const
      {
         acc ^= round(0, val);
         return acc*PRIME64_1 + PRIME64_4;
      }

}
#endif


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
#ifndef knz_TPAQPredictor
#define knz_TPAQPredictor

#include <cstring>
#include "../Context.hpp"
#include "../Global.hpp"
#include "../Predictor.hpp"
#include "../Memory.hpp"
#include "AdaptiveProbMap.hpp"


namespace kanzi
{

   // TPAQ predictor
   // Initially based on Tangelo 2.4 (by Jan Ondrus).
   // PAQ8 is written by Matt Mahoney.
   // See http://encode.su/threads/1738-TANGELO-new-compressor-(derived-from-PAQ8-FP8)

   // Mixer combines models using neural networks with 8 inputs.
   class TPAQMixer
   {
   public:
      TPAQMixer();

      ~TPAQMixer() { }

       void update(int bit);

       int get(int p0, int p1, int p2, int p3, int p4, int p5, int p6, int p7);

   private:
       static const int BEGIN_LEARN_RATE;
       static const int END_LEARN_RATE;

       int _w0, _w1, _w2, _w3, _w4, _w5, _w6, _w7;
       int _p0, _p1, _p2, _p3, _p4, _p5, _p6, _p7;
       int _pr;
       int _skew;
       int _learnRate;
   };


   template <bool T>
   class TPAQPredictor FINAL : public Predictor
   {
   public:
       TPAQPredictor(Context* ctx = nullptr);

       ~TPAQPredictor();

       void update(int bit);

       // Return the split value representing the probability of 1 in the [0..4095] range.
       int get() { return _pr; }

   private:
       static const int MAX_LENGTH;
       static const int BUFFER_SIZE;
       static const int HASH_SIZE;
       static const int HASH;
       static const uint MASK_80808080;
       static const uint MASK_F0F0F000;
       static const uint MASK_4F4FFFFF;

       #define SSE0_RATE(T) ((T == true) ? 6 : 7)

       int _pr; // next predicted value (0-4095)
       uint _c0; // bitwise context: last 0-7 bits with a leading 1 (1-255)
       uint _c4; // last 4 whole bytes, last is in low 8 bits
       uint _c8; // last 8 to 4 whole bytes, last is in low 8 bits
       int _bpos; // number of bits in c0 (0-7)
       int _pos;
       int _binCount;
       int _matchLen;
       int _matchPos;
       int _matchVal;
       uint _hash;
       LogisticAdaptiveProbMap<false, SSE0_RATE(T)> _sse0;
       LogisticAdaptiveProbMap<false, 7> _sse1;
       TPAQMixer* _mixers;
       TPAQMixer* _mixer; // current mixer
       byte* _buffer;
       int* _hashes; // hash table(context, buffer position)
       uint8* _bigStatesMap;// hash table(context, prediction)
       uint8* _smallStatesMap0; // hash table(context, prediction)
       uint8* _smallStatesMap1; // hash table(context, prediction)
       uint _statesMask;
       uint _mixersMask;
       uint _hashMask;
       uint _bufferMask;
       uint8* _cp0; // context pointers
       uint8* _cp1;
       uint8* _cp2;
       uint8* _cp3;
       uint8* _cp4;
       uint8* _cp5;
       uint8* _cp6;
       int _ctx0; // contexts
       int _ctx1;
       int _ctx2;
       int _ctx3;
       int _ctx4;
       int _ctx5;
       int _ctx6;

       int hash(uint x, uint y) const;

       int createContext(uint ctxId, uint cx) const;

       int getMatchContextPred();

       void findMatch();

       bool reset();
  };


   // Adjust weights to minimize coding cost of last prediction
   inline void TPAQMixer::update(int bit)
   {
       const int err = (((bit << 12) - _pr) * _learnRate) >> 10;

       if (err == 0)
           return;

       // Quickly decaying learn rate
       _learnRate -= (uint32(END_LEARN_RATE - _learnRate) >> 31);
       _skew += err;

       // Train Neural Network: update weights
       _w0 += ((_p0 * err + 0) >> 12);
       _w1 += ((_p1 * err + 0) >> 12);
       _w2 += ((_p2 * err + 0) >> 12);
       _w3 += ((_p3 * err + 0) >> 12);
       _w4 += ((_p4 * err + 0) >> 12);
       _w5 += ((_p5 * err + 0) >> 12);
       _w6 += ((_p6 * err + 0) >> 12);
       _w7 += ((_p7 * err + 0) >> 12);
   }

   inline int TPAQMixer::get(int p0, int p1, int p2, int p3, int p4, int p5, int p6, int p7)
   {
       _p0 = p0;
       _p1 = p1;
       _p2 = p2;
       _p3 = p3;
       _p4 = p4;
       _p5 = p5;
       _p6 = p6;
       _p7 = p7;

       // Neural Network dot product (sum weights*inputs)
       _pr = Global::squash(((p0 * _w0) + (p1 * _w1) + (p2 * _w2) + (p3 * _w3) +
                             (p4 * _w4) + (p5 * _w5) + (p6 * _w6) + (p7 * _w7) +
                             _skew + 65536) >> 17);
       return _pr;
   }



   ///////////////////////// state table ////////////////////////
   // States represent a bit history within some context.
   // State 0 is the starting state (no bits seen).
   // States 1-30 represent all possible sequences of 1-4 bits.
   // States 31-252 represent a pair of counts, (n0,n1), the number
   //   of 0 and 1 bits respectively.  If n0+n1 < 16 then there are
   //   two states for each pair, depending on if a 0 or 1 was the last
   //   bit seen.
   // If n0 and n1 are too large, then there is no state to represent this
   // pair, so another state with about the same ratio of n0/n1 is substituted.
   // Also, when a bit is observed and the count of the opposite bit is large,
   // then part of this count is discarded to favor newer data over old.
   const uint8 STATE_TRANSITIONS[2][256] = {
       // Bit 0
       { 1, 3, 143, 4, 5, 6, 7, 8, 9, 10,
           11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
           21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
           31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
           41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
           51, 52, 47, 54, 55, 56, 57, 58, 59, 60,
           61, 62, 63, 64, 65, 66, 67, 68, 69, 6,
           71, 71, 71, 61, 75, 56, 77, 78, 77, 80,
           81, 82, 83, 84, 85, 86, 87, 88, 77, 90,
           91, 92, 80, 94, 95, 96, 97, 98, 99, 90,
           101, 94, 103, 101, 102, 104, 107, 104, 105, 108,
           111, 112, 113, 114, 115, 116, 92, 118, 94, 103,
           119, 122, 123, 94, 113, 126, 113, 128, 129, 114,
           131, 132, 112, 134, 111, 134, 110, 134, 134, 128,
           128, 142, 143, 115, 113, 142, 128, 148, 149, 79,
           148, 142, 148, 150, 155, 149, 157, 149, 159, 149,
           131, 101, 98, 115, 114, 91, 79, 58, 1, 170,
           129, 128, 110, 174, 128, 176, 129, 174, 179, 174,
           176, 141, 157, 179, 185, 157, 187, 188, 168, 151,
           191, 192, 188, 187, 172, 175, 170, 152, 185, 170,
           176, 170, 203, 148, 185, 203, 185, 192, 209, 188,
           211, 192, 213, 214, 188, 216, 168, 84, 54, 54,
           221, 54, 55, 85, 69, 63, 56, 86, 58, 230,
           231, 57, 229, 56, 224, 54, 54, 66, 58, 54,
           61, 57, 222, 78, 85, 82, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0 },
       // Bit 1
       { 2, 163, 169, 163, 165, 89, 245, 217, 245, 245,
           233, 244, 227, 74, 221, 221, 218, 226, 243, 218,
           238, 242, 74, 238, 241, 240, 239, 224, 225, 221,
           232, 72, 224, 228, 223, 225, 238, 73, 167, 76,
           237, 234, 231, 72, 31, 63, 225, 237, 236, 235,
           53, 234, 53, 234, 229, 219, 229, 233, 232, 228,
           226, 72, 74, 222, 75, 220, 167, 57, 218, 70,
           168, 72, 73, 74, 217, 76, 167, 79, 79, 166,
           162, 162, 162, 162, 165, 89, 89, 165, 89, 162,
           93, 93, 93, 161, 100, 93, 93, 93, 93, 93,
           161, 102, 120, 104, 105, 106, 108, 106, 109, 110,
           160, 134, 108, 108, 126, 117, 117, 121, 119, 120,
           107, 124, 117, 117, 125, 127, 124, 139, 130, 124,
           133, 109, 110, 135, 110, 136, 137, 138, 127, 140,
           141, 145, 144, 124, 125, 146, 147, 151, 125, 150,
           127, 152, 153, 154, 156, 139, 158, 139, 156, 139,
           130, 117, 163, 164, 141, 163, 147, 2, 2, 199,
           171, 172, 173, 177, 175, 171, 171, 178, 180, 172,
           181, 182, 183, 184, 186, 178, 189, 181, 181, 190,
           193, 182, 182, 194, 195, 196, 197, 198, 169, 200,
           201, 202, 204, 180, 205, 206, 207, 208, 210, 194,
           212, 184, 215, 193, 184, 208, 193, 163, 219, 168,
           94, 217, 223, 224, 225, 76, 227, 217, 229, 219,
           79, 86, 165, 217, 214, 225, 216, 216, 234, 75,
           214, 237, 74, 74, 163, 217, 0, 0, 0, 0,
           0, 0, 0, 0, 0, 0 }
   };

   const int STATE_MAP[] = {
      -31,  -400,   406,  -547,  -642,  -743,  -827,  -901,
     -901,  -974,  -945,  -955, -1060, -1031, -1044,  -956,
     -994, -1035, -1147, -1069, -1111, -1145, -1096, -1084,
    -1171, -1199, -1062, -1498, -1199, -1199, -1328, -1405,
    -1275, -1248, -1167, -1448, -1441, -1199, -1357, -1160,
    -1437, -1428, -1238, -1343, -1526, -1331, -1443, -2047,
    -2047, -2044, -2047, -2047, -2047,  -232,  -414,  -573,
     -517,  -768,  -627,  -666,  -644,  -740,  -721,  -829,
     -770,  -963,  -863, -1099,  -811,  -830,  -277, -1036,
     -286,  -218,   -42,  -411,   141, -1014, -1028,  -226,
     -469,  -540,  -573,  -581,  -594,  -610,  -628,  -711,
     -670,  -144,  -408,  -485,  -464,  -173,  -221,  -310,
     -335,  -375,  -324,  -413,   -99,  -179,  -105,  -150,
      -63,    -9,    56,    83,   119,   144,   198,   118,
      -42,   -96,  -188,  -285,  -376,   107,  -138,    38,
      -82,   186,  -114,  -190,   200,   327,    65,   406,
      108,   -95,   308,   171,   -18,   343,   135,   398,
      415,   464,   514,   494,   508,   519,    92,  -123,
      343,   575,   585,   516,    -7,  -156,   209,   574,
      613,   621,   670,   107,   989,   210,   961,   246,
      254,   -12,  -108,    97,   281,  -143,    41,   173,
     -209,   583,   -55,   250,   354,   558,    43,   274,
       14,   488,   545,    84,   528,   519,   587,   634,
      663,    95,   700,    94,  -184,   730,   742,   162,
      -10,   708,   692,   773,   707,   855,   811,   703,
      790,   871,   806,     9,   867,   840,   990,  1023,
     1409,   194,  1397,   183,  1462,   178,   -23,  1403,
      247,   172,     1,   -32,  -170,    72,  -508,   -46,
     -365,   -26,  -146,   101,   -18,  -163,  -422,  -461,
     -146,   -69,   -78,  -319,  -334,  -232,   -99,     0,
       47,   -74,     0,  -452,    14,   -57,     1,     1,
        1,     1,     1,     1,     1,     1,     1,     1,
   };

   const int MATCH_PRED[] = {
        0,    64,   128,   192,   256,   320,   384,   448,
      512,   576,   640,   704,   768,   832,   896,   960,
     1024,  1038,  1053,  1067,  1082,  1096,  1111,  1125,
     1139,  1154,  1168,  1183,  1197,  1211,  1226,  1240,
     1255,  1269,  1284,  1298,  1312,  1327,  1341,  1356,
     1370,  1385,  1399,  1413,  1428,  1442,  1457,  1471,
     1486,  1500,  1514,  1529,  1543,  1558,  1572,  1586,
     1601,  1615,  1630,  1644,  1659,  1673,  1687,  1702,
     1716,  1731,  1745,  1760,  1774,  1788,  1803,  1817,
     1832,  1846,  1861,  1875,  1889,  1904,  1918,  1933,
     1947,  1961,  1976,  1990,  2005,  2019,  2034,  2047,
   };


   template <bool T>
   TPAQPredictor<T>::TPAQPredictor(Context* ctx)
       : _sse0(256)
       , _sse1((T == true) ? 65536 : 256)
   {
       uint statesSize = 1 << 28;
       uint mixersSize = 1 << 12;
       uint hashSize = HASH_SIZE;
       uint extraMem = (T == true) ? 1 : 0;
       uint bufferSize = BUFFER_SIZE;
       uint bsVersion = 7;

       if (ctx != nullptr) {
           // Block size requested by the user
           // The user can request a big block size to force more states
           const int rbsz = ctx->getInt("blockSize", 32768);

           if (rbsz >= 64 * 1024 * 1024)
               statesSize = 1 << 28;
           else if (rbsz >= 16 * 1024 * 1024)
               statesSize = 1 << 27;
           else if (rbsz >= 4 * 1024 * 1024)
               statesSize = 1 << 26;
           else
               statesSize = (rbsz >= 1024 * 1024) ? 1 << 24 : 1 << 22;

           // Actual size of the current block
           // Too many mixers hurts compression for small blocks.
           // Too few mixers hurts compression for big blocks.
           const int absz = ctx->getInt("size", rbsz);

           if (absz >= 32 * 1024 * 1024)
               mixersSize = 1 << 16;
           else if (absz >= 16 * 1024 * 1024)
               mixersSize = 1 << 15;
           else if (absz >= 8 * 1024 * 1024)
               mixersSize = 1 << 14;
           else if (absz >= 4 * 1024 * 1024)
               mixersSize = 1 << 13;
           else
               mixersSize = (absz >= 1 * 1024 * 1024) ? 1 << 11 : 1 << 8;

           bufferSize = rbsz < BUFFER_SIZE ? rbsz : BUFFER_SIZE;
           const uint mxsz = absz < (1 << 26) ? absz * 16 : 1 << 30;
           hashSize = hashSize < mxsz ? hashSize : mxsz;
           bsVersion = ctx->getInt("bsVersion", bsVersion);
       }

       if (bsVersion > 6) {
          // The ring buffer and hash table use bit masks for indexing.
          // Normalize their sizes to powers of two before creating the masks.
          bufferSize = 1u << Global::_log2(bufferSize);
          hashSize = 1u << Global::_log2(hashSize);
       }

       mixersSize <<= (2 * extraMem);
       statesSize <<= (2 * extraMem);
       hashSize <<= (2 * extraMem);

       // Cap hash size for java compatibility
       if ((bsVersion > 5) && (hashSize > 1024 * 1024 * 1024))
           hashSize = 1024 * 1024 * 1024;

       _statesMask = statesSize - 1;
       _mixersMask = (mixersSize - 1) & ~1;
       _hashMask = hashSize - 1;
       _bufferMask = bufferSize - 1;
       _mixers = new TPAQMixer[mixersSize];
       _bigStatesMap = new uint8[statesSize];
       _smallStatesMap0 = new uint8[1 << 16];
       _smallStatesMap1 = new uint8[1 << 24];
       _hashes = new int[hashSize];
       _buffer = new byte[bufferSize];

       reset();
   }

   template <bool T>
   bool TPAQPredictor<T>::reset() {
       _pr = 2048;
       _c0 = 1;
       _c4 = 0;
       _c8 = 0;
       _pos = 0;
       _bpos = 8;
       _binCount = 0;
       _matchLen = 0;
       _matchPos = 0;
       _matchVal = 0;
       _hash = 0;
       _mixer = &_mixers[0];
       memset(_bigStatesMap, 0, size_t(_statesMask + 1));
       memset(_smallStatesMap0, 0, 1 << 16);
       memset(_smallStatesMap1, 0, 1 << 24);
       memset(_hashes, 0, sizeof(int) * size_t(_hashMask + 1));
       memset(_buffer, 0, size_t(_bufferMask + 1));
       _cp0 = &_smallStatesMap0[0];
       _cp1 = &_smallStatesMap1[0];
       _cp2 = &_bigStatesMap[0];
       _cp3 = &_bigStatesMap[0];
       _cp4 = &_bigStatesMap[0];
       _cp5 = &_bigStatesMap[0];
       _cp6 = &_bigStatesMap[0];
       _ctx0 = _ctx1 = _ctx2 = _ctx3 = 0;
       _ctx4 = _ctx5 = _ctx6 = 0;
       return true;
   }

   template <bool T>
   TPAQPredictor<T>::~TPAQPredictor()
   {
       delete[] _bigStatesMap;
       delete[] _smallStatesMap0;
       delete[] _smallStatesMap1;
       delete[] _hashes;
       delete[] _buffer;
       delete[] _mixers;
   }

   // Update the probability model
   template <bool T>
   void TPAQPredictor<T>::update(int bit)
   {
       _mixer->update(bit);
       _c0 += (_c0 + bit);
       _bpos--;

       if (_bpos == 0) {
           _buffer[_pos & _bufferMask] = byte(_c0);
           _pos++;
           _c8 = (_c8 << 8) | ((_c4 >> 24) & 0xFF);
           _c4 = (_c4 << 8) | (_c0 & 0xFF);
           _hash = (((_hash * HASH) << 4) + _c4) & _hashMask;
           _c0 = 1;
           _bpos = 8;
           _binCount += ((_c4 >> 7) & 1);

           // Select Neural Net
           _mixer = &_mixers[(_c4 & _mixersMask) + (_matchLen != 0 ? 1 : 0)];

           // Add contexts to NN
           _ctx0 = (_c4 & 0xFF) << 8;
           _ctx1 = (_c4 & 0xFFFF) << 8;
           _ctx2 = createContext(2, _c4 & 0x00FFFFFF);
           _ctx3 = createContext(3, _c4);

           if (_binCount < (_pos >> 2)) {
               // Mostly text or mixed
               _ctx4 = createContext(_ctx1, _c4 ^ (_c8 & 0xFFFF));
               _ctx5 = (_c8 & MASK_F0F0F000) | ((_c4 & MASK_F0F0F000) >> 4);

               if (T == true) {
                  const uint h1 = ((_c4 & MASK_80808080) == 0) ?
                      _c4 & MASK_4F4FFFFF : _c4 & MASK_80808080;
                  const uint h2 = ((_c8 & MASK_80808080) == 0) ?
                      _c8 & MASK_4F4FFFFF : _c8 & MASK_80808080;
                  _ctx6 = hash(h1 << 2, h2 >> 2);
               }
           }
           else {
               // Mostly binary
               _ctx4 = createContext(HASH + _matchLen, _c4 ^ (_c4 & 0x000FFFFF));
               _ctx5 = _ctx0 | (_c8 << 16);

               if (T == true) {
                  _ctx6 = hash(_c4 & 0xFFFF0000, _c8 >> 16);
               }
           }

           findMatch();
           _matchVal = int(_buffer[_matchPos & _bufferMask]) | 0x100;

           // Keep track current position
           _hashes[_hash] = _pos;
       }

       // Get initial predictions
       // It has been observed that accessing memory via [ctx ^ c] is significantly faster
       // on SandyBridge/Windows and slower on SkyLake/Linux except when [ctx & 255 == 0]
       // (with c < 256). Hence, use XOR for _ctx5 which is the only context that fulfills
       // the condition.
       const int idx2 = (uint(_ctx2) + _c0) & _statesMask;
       const int idx3 = (uint(_ctx3) + _c0) & _statesMask;
       const int idx4 = (uint(_ctx4) + _c0) & _statesMask;
       const int idx5 = (uint(_ctx5) ^ _c0) & _statesMask;
       prefetchRead(&_bigStatesMap[idx2]);
       prefetchRead(&_bigStatesMap[idx3]);
       prefetchRead(&_bigStatesMap[idx4]);
       prefetchRead(&_bigStatesMap[idx5]);

       const uint8* table = STATE_TRANSITIONS[bit];
       *_cp0 = table[*_cp0];
       *_cp1 = table[*_cp1];
       *_cp2 = table[*_cp2];
       *_cp3 = table[*_cp3];
       *_cp4 = table[*_cp4];
       *_cp5 = table[*_cp5];
       _cp0 = &_smallStatesMap0[_ctx0 + _c0];
       const int p0 = STATE_MAP[*_cp0];
       _cp1 = &_smallStatesMap1[_ctx1 + _c0];
       const int p1 = STATE_MAP[*_cp1];
       _cp2 = &_bigStatesMap[idx2];
       const int p2 = STATE_MAP[*_cp2];
       _cp3 = &_bigStatesMap[idx3];
       const int p3 = STATE_MAP[*_cp3];
       _cp4 = &_bigStatesMap[idx4];
       const int p4 = STATE_MAP[*_cp4];
       _cp5 = &_bigStatesMap[idx5];
       const int p5 = STATE_MAP[*_cp5];

       const int p7 = (_matchLen == 0) ? 0 : getMatchContextPred();
       int p;

       if (T == false) {
          // Mix predictions using NN
          p = _mixer->get(p0, p1, p2, p3, p4, p5, p7, p7);

          // SSE (Secondary Symbol Estimation)
          if (_binCount < (_pos >> 3)) {
              p = (3 * _sse0.get(bit, p, _c0) + p) >> 2;
          }
       } else {
          // One more prediction
          const int idx6 = (uint(_ctx6) + _c0) & _statesMask;
          prefetchRead(&_bigStatesMap[idx6]);
          *_cp6 = table[*_cp6];
          _cp6 = &_bigStatesMap[idx6];
          const int p6 = STATE_MAP[*_cp6];

          // Mix predictions using NN
          p = _mixer->get(p0, p1, p2, p3, p4, p5, p6, p7);

          // SSE (Secondary Symbol Estimation)
          if (_binCount < (_pos >> 3)) {
              p = _sse1.get(bit, p, _ctx0 + _c0);
          }
          else {
              if (_binCount >= (_pos >> 2))
                 p = (3 * _sse0.get(bit, p, _c0) + p) >> 2;

              p = (3 * _sse1.get(bit, p, _ctx0 + _c0) + p) >> 2;
          }
       }

       _pr = p + ((p < 2048) ? 1 : 0);
   }

   template <bool T>
   void TPAQPredictor<T>::findMatch()
   {
       // Update ongoing sequence match or detect match in the buffer (LZ like)
       if (_matchLen > 0) {
           if (_matchLen < MAX_LENGTH)
               _matchLen++;

           _matchPos++;
           return;
       }

       // Retrieve match position
       _matchPos = _hashes[_hash];

       // Detect match
       if ((_matchPos != 0) && (uint(_pos - _matchPos) <= _bufferMask)) {
           int r = _matchLen + 2;

           // Compare four pairs at once when both logical ranges are physically
           // contiguous. Wrapping ranges use the scalar path below.
           while (r + 6 <= MAX_LENGTH) {
               const uint p0 = uint(_pos - r - 7) & _bufferMask;
               const uint p1 = uint(_matchPos - r - 7) & _bufferMask;

               if ((p0 > _bufferMask - 7) || (p1 > _bufferMask - 7))
                   break;

               // Read as big endian so the newest byte is in the low bits.
               // This makes trailingZeros() identify the first mismatch in
               // the same direction as the original pair-by-pair scan.
               const uint64 diff = uint64(BigEndian::readLong64(&_buffer[p0])) ^
                                   uint64(BigEndian::readLong64(&_buffer[p1]));

               if (diff != 0) {
                   // Round down to a complete pair. The scalar loop below
                   // locates the exact mismatching pair.
                   r += (Global::trailingZeros(diff) >> 4) << 1;
                   break;
               }

               r += 8;
           }

           // Locate the first mismatching pair and preserve the original
           // even-length match semantics.
           while (r <= MAX_LENGTH) {
               if ((_buffer[(_pos - r - 1) & _bufferMask]) != (_buffer[(_matchPos - r - 1) & _bufferMask]))
                   break;

               if ((_buffer[(_pos - r) & _bufferMask]) != (_buffer[(_matchPos - r) & _bufferMask]))
                   break;

               r += 2;
           }

           _matchLen = r - 2;
       }
   }

   template <bool T>
   inline int TPAQPredictor<T>::hash(uint x, uint y) const
   {
       const int h = x * HASH ^ y * HASH;
       return (h >> 1) ^ (h >> 9) ^ (x >> 2) ^ (y >> 3) ^ HASH;
   }

   template <bool T>
   inline int TPAQPredictor<T>::createContext(uint ctxId, uint cx) const
   {
       cx = cx * 987654323 + ctxId;
       cx = (cx << 16) | (cx >> 16);
       return cx * 123456791 + ctxId;
   }

   // Get a prediction from the match model in [-2047..2048]
   template <bool T>
   inline int TPAQPredictor<T>::getMatchContextPred()
   {
       const uint matchPrefix = uint(_matchVal) >> _bpos;

       if (_c0 == matchPrefix) {
           return (((_matchVal >> (_bpos - 1)) & 1) != 0) ?
               MATCH_PRED[_matchLen - 1] : -MATCH_PRED[_matchLen - 1];
       }

       _matchLen = 0;
       return 0;
   }
}
#endif

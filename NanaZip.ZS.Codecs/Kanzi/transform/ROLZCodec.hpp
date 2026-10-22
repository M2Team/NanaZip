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
#ifndef knz_ROLZCodec
#define knz_ROLZCodec

#include "../Context.hpp"
#include "../Memory.hpp"
#include "../Transform.hpp"

// Implementation of a Reduced Offset Lempel Ziv transform
// More information about ROLZ at http://ezcodesample.com/rolz/rolz_article.html

namespace kanzi {

   class ROLZEncoder {
   private:
       static const uint64 TOP;
       static const uint64 MASK_0_32;
       static const int MATCH_FLAG;
       static const int LITERAL_FLAG;
       static const int PSCALE;

       uint16* _probs[2];
       uint _logSizes[2];
       int& _idx;
       uint64 _low;
       uint64 _high;
       byte* _buf;
       int32 _c1;
       int32 _ctx;
       int _pIdx;

       void encodeBit(int bit);

   public:
       ROLZEncoder(uint litLogSize, uint mLogSize, byte buf[], int& idx);

       ~ROLZEncoder()
       {
           delete[] _probs[LITERAL_FLAG];
           delete[] _probs[MATCH_FLAG];
       }

       void encodeBits(int val, int n);

       void encode9Bits(int val);

       void dispose();

       void reset();

       void setContext(int n, byte ctx) { _pIdx = n; _ctx = int32(ctx) << _logSizes[_pIdx]; }
   };

   class ROLZDecoder {
   private:
       static const uint64 TOP;
       static const uint64 MASK_0_56;
       static const uint64 MASK_0_32;
       static const int MATCH_FLAG;
       static const int LITERAL_FLAG;
       static const int PSCALE;

       uint16* _probs[2];
       uint _logSizes[2];
       int& _idx;
       uint64 _low;
       uint64 _high;
       uint64 _current;
       byte* _buf;
       int _end;
       bool _error;
       int32 _c1;
       int32 _ctx;
       int _pIdx;

       int decodeBit();

   public:
       ROLZDecoder(uint litLogSize, uint mLogSize, byte buf[], int& idx, int end);

       ~ROLZDecoder()
       {
           delete[] _probs[LITERAL_FLAG];
           delete[] _probs[MATCH_FLAG];
       }

       int decodeBits(int n);

       int decode9Bits();

       void dispose() const {}

       void reset();

       void setContext(int n, byte ctx) { _pIdx = n; _ctx = int32(ctx) << _logSizes[_pIdx]; }

       bool isValid() const { return _error == false; }
   };

   // Use ANS to encode/decode literals and matches
   class ROLZCodec1 FINAL : public Transform<byte> {
   public:
       ROLZCodec1(uint logPosChecks);

       ROLZCodec1(Context& ctx);

       ~ROLZCodec1() { if (_matches != nullptr) delete[] _matches; }

       bool forward(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

       bool inverse(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

       // Required encoding output buffer size
       int getMaxEncodedLength(int srcLen) const
       {
           return (srcLen <= 512) ? srcLen + 64 : srcLen;
       }

   private:
       static const int MIN_MATCH3;
       static const int MIN_MATCH4;
       static const int MIN_MATCH7;
       static const int MAX_MATCH;
       static const int LOG_POS_CHECKS;

       uint32* _matches;
       size_t _mSize;
       uint8 _counters[65536];
       int _logPosChecks;
       int _posChecks;
       Context* _pCtx;
       int _minMatch;
       uint8 _maskChecks;	   

       int findMatch(const byte buf[], int pos, int end, uint32 hash32, const uint32* matches, const uint8* counter) const;

       int emitLength(byte block[], int length) const;

       int readLength(const byte block[], int& idx) const;
   };

   // Use CM (ROLZEncoder/ROLZDecoder) to encode/decode literals and matches
   // Code loosely based on 'balz' by Ilya Muravyov
   class ROLZCodec2 FINAL : public Transform<byte> {
   public:
       ROLZCodec2(uint logPosChecks);

       ROLZCodec2(Context& ctx);

       ~ROLZCodec2() { if (_matches != nullptr) delete[] _matches; }

       bool forward(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

       bool inverse(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

       // Required encoding output buffer size
       int getMaxEncodedLength(int srcLen) const
       {
           // Since we do not check the dst index for each byte (for speed purpose)
           // allocate some extra buffer for incompressible data.
           return srcLen + ((srcLen < 32768) ? 1024 : srcLen >> 5);
       }

   private:
       static const int MATCH_FLAG;
       static const int LITERAL_FLAG;
       static const int MATCH_CTX;
       static const int LITERAL_CTX;
       static const int MIN_MATCH3;
       static const int MIN_MATCH7;
       static const int MAX_MATCH;
       static const int LOG_POS_CHECKS;

       uint32* _matches;
       uint8 _counters[65536];
       int _logPosChecks;
       uint8 _maskChecks;
       Context* _pCtx;
       int _minMatch;
       int _posChecks;

       int findMatch(const byte buf[], int pos, int end, uint32 key);
   };

   class ROLZCodec FINAL : public Transform<byte> {
       friend class ROLZCodec1;
       friend class ROLZCodec2;

   public:
       ROLZCodec(uint logPosChecks = 4);

       ROLZCodec(Context& ctx);

       ~ROLZCodec() { delete _delegate; }

       bool forward(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

       bool inverse(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

       // Required encoding output buffer size
       int getMaxEncodedLength(int srcLen) const
       {
           return _delegate->getMaxEncodedLength(srcLen);
       }

   private:
       static const int HASH_SIZE;
       static const int CHUNK_SIZE;
       static const int32 HASH;
       static const int32 HASH_MASK;
       static const int MAX_BLOCK_SIZE;
       static const int MIN_BLOCK_SIZE;

       Transform<byte>* _delegate;

       static uint32 getKey1(const byte* p)
       {
           return uint32(LittleEndian::readInt16(p)) & (HASH_SIZE - 1);
       }

       static uint32 getKey2(const byte* p)
       {
           return uint32((uint64(LittleEndian::readLong64(p)) * HASH) >> 40) & (HASH_SIZE - 1);
       }

       static uint32 hash(const byte* p)
       {
           return ((uint32(LittleEndian::readInt32(p)) << 8) * HASH) & HASH_MASK;
       }

       static int emitCopy(byte dst[], int dstIdx, int ref, int matchLen);
   };

   inline int ROLZCodec1::emitLength(byte block[], int length) const
   {
       if (length < 1 << 7) {
           block[0] = byte(length);
           return 1;
       }

       int idx = 0;

       if (length >= 1 << 14) {       
            block[idx] = byte(0x80 | (length >> 21));
            idx += ((length >= 1 << 21) ? 1 : 0);
            block[idx++] = byte(0x80 | (length >> 14));
        }

       block[idx++] = byte(0x80 | (length >> 7));
       block[idx++] = byte(length & 0x7F);
       return idx;
   }

   inline int ROLZCodec1::readLength(const byte block[], int& pos) const
   {
       int next = int(block[pos++]);

       if (next < 128)
          return next;

       int length = next & 0x7F;
       next = int(block[pos++]);
       length = (length << 7) | (next & 0x7F);

       if (next >= 128) {
           next = int(block[pos++]);
           length = (length << 7) | (next & 0x7F);

           if (next >= 128) {
                next = int(block[pos++]);
                length = (length << 7) | (next & 0x7F);
           }
       }
 
       return length;
   }

   inline int ROLZCodec::emitCopy(byte buf[], int dstIdx, int ref, int matchLen)
   {
       const int res = dstIdx + matchLen;

       if (dstIdx - ref >= 8) {
           while (matchLen > 0) {
               KANZI_MEM_CP8(&buf[dstIdx], &buf[ref]);
               ref += 8;
               dstIdx += 8;
               matchLen -= 8;
           }
       }
       else {
           while (matchLen != 0) {
              buf[dstIdx++] = buf[ref++];
              matchLen--;
           }
       }

       return res;
   }

   inline void ROLZEncoder::encodeBit(int bit)
   {
       const uint64 split = ((_high - _low) >> 4) * uint64(_probs[_pIdx][_ctx + _c1] >> 4) >> 8;

       // Update fields with new interval bounds
       if (bit == 0) {
           _low += (split + 1);
           _probs[_pIdx][_ctx + _c1] -= (_probs[_pIdx][_ctx + _c1] >> 5);
           _c1 += _c1;
       }
       else {
           _high = _low + split;
           _probs[_pIdx][_ctx + _c1] -= ((_probs[_pIdx][_ctx + _c1] - PSCALE + 32) >> 5);
           _c1 += (_c1 + 1);
       }

       // Emit unchanged first 32 bits
       while (((_low ^ _high) >> 24) == 0) {
           BigEndian::writeInt32(&_buf[_idx], int32(_high >> 32));
           _idx += 4;
           _low <<= 32;
           _high = (_high << 32) | MASK_0_32;
       }
   }

   inline int ROLZDecoder::decodeBit()
   {
       if (_error)
           return 0;

       const uint64 mid = _low + (((_high - _low) >> 4) * uint64(_probs[_pIdx][_ctx + _c1] >> 4) >> 8);
       int bit;

       // Update bounds and predictor
       if (mid >= _current) {
           bit = 1;
           _high = mid;
           _probs[_pIdx][_ctx + _c1] -= ((_probs[_pIdx][_ctx + _c1] - PSCALE + 32) >> 5);
           _c1 += (_c1 + 1);
       }
       else {
           bit = 0;
           _low = mid + 1;
           _probs[_pIdx][_ctx + _c1] -= (_probs[_pIdx][_ctx + _c1] >> 5);
           _c1 += _c1;
       }

       // Read 32 bits
       while (((_low ^ _high) >> 24) == 0) {
           if ((_idx < 0) || (_idx > _end) || (_end - _idx < 4)) {
               _error = true;
               return bit;
           }

           _low = (_low << 32) & MASK_0_56;
           _high = ((_high << 32) | MASK_0_32) & MASK_0_56;
           const uint64 val = uint64(BigEndian::readInt32(&_buf[_idx])) & MASK_0_32;
           _current = ((_current << 32) | val) & MASK_0_56;
           _idx += 4;
       }

       return bit;
   }
}
#endif

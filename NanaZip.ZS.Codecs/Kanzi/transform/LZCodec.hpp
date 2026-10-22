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
#ifndef knz_LZCodec
#define knz_LZCodec

#include "../Context.hpp"
#include "../Global.hpp"
#include "../Transform.hpp"
#include "../Memory.hpp"

namespace kanzi {

    class LZCodec FINAL : public Transform<byte> {

    public:
        LZCodec();

        LZCodec(Context& ctx);

        ~LZCodec() { delete _delegate; }

        bool forward(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

        bool inverse(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

        // Required encoding output buffer size
        int getMaxEncodedLength(int srcLen) const
        {
            return _delegate->getMaxEncodedLength(srcLen);
        }

    private:
        Transform<byte>* _delegate;
    };

    // Simple byte oriented LZ77 implementation.
    template <bool T>
    class LZXCodec FINAL : public Transform<byte> {
    public:
        LZXCodec()
        {
            _hashes = nullptr;
            _hashSize = 0;
            _tkBuf = nullptr;
            _mLenBuf = nullptr;
            _mBuf = nullptr;
            _bufferSize = 0;
            _pCtx = nullptr;
        }

        LZXCodec(Context& ctx) :
            _pCtx(&ctx)
        {
            _hashes = nullptr;
            _hashSize = 0;
            _tkBuf = nullptr;
            _mLenBuf = nullptr;
            _mBuf = nullptr;
            _bufferSize = 0;
        }

        ~LZXCodec()
        {
            _bufferSize = 0;
            _hashSize = 0;
            if (_hashes != nullptr) delete[] _hashes;
            if (_mLenBuf != nullptr) delete[] _mLenBuf;
            if (_mBuf != nullptr) delete[] _mBuf;
            if (_tkBuf != nullptr) delete[] _tkBuf;
        }

        bool forward(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

        bool inverse(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

        // Required encoding output buffer size
        int getMaxEncodedLength(int srcLen) const
        {
            // readLength() may read two bytes past the encoded block
            return ((srcLen <= 1024) ? srcLen + 16 : srcLen + (srcLen / 64)) + 4;
        }

    private:
        static const uint HASH_SEED;
        static const uint HASH_LOG;
        static const uint HASH_LSHIFT;
        static const uint HASH_RSHIFT;
        static const int MAX_DISTANCE1;
        static const int MAX_DISTANCE2;
        static const int MIN_MATCH4;
        static const int MIN_MATCH6;
        static const int MIN_MATCH9;
        static const int MAX_MATCH;
        static const int MIN_BLOCK_LENGTH;
        static const int READ_LENGTH_GUARD;

        int32* _hashes;
        int _hashSize;
        byte* _mLenBuf;
        byte* _mBuf;
        byte* _tkBuf;
        int _bufferSize;
        Context* _pCtx;

        bool inverseV7(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

        bool inverseV6(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

        bool inverseV5(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

        static int emitLength(byte block[], int len);

        static void emitLiterals(const byte src[], byte dst[], int len);

        static int findMatch(const byte block[], const int pos, const int ref, const int maxMatch);

        static uint readLength(const byte block[], int& pos);

        static int32 hash(const byte* p);
    };

    class LZPCodec FINAL : public Transform<byte> {
    public:
        LZPCodec()
        {
            _hashes = nullptr;
            _hashSize = 0;
        }

        LZPCodec(Context&)
        {
            _hashes = nullptr;
            _hashSize = 0;
        }

        ~LZPCodec()
        {
            delete[] _hashes;
        }

        bool forward(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

        bool inverse(SliceArray<byte>& src, SliceArray<byte>& dst, int length);

        // Required encoding output buffer size
        int getMaxEncodedLength(int srcLen) const
        {
            return (srcLen <= 1024) ? srcLen + 16 : srcLen + (srcLen / 64);
        }

    private:
        static const uint HASH_SEED;
        static const uint HASH_LOG;
        static const uint HASH_SHIFT;
        static const int MIN_MATCH;
        static const int MIN_BLOCK_LENGTH;
        static const int MATCH_FLAG;

        int32* _hashes;
        int _hashSize;

        static int findMatch(const byte block[], const int pos, const int ref, const int maxMatch);
    };

    template <bool T>
    inline void LZXCodec<T>::emitLiterals(const byte src[], byte dst[], int len)
    {
        // Callers provide trailing padding for this 16-byte copy, which may
        // read and write up to 15 bytes past the requested literal length.
        for (int i = 0; i < len; i += 16)
            KANZI_MEM_CP16(&dst[i], &src[i]);
    }

    template <bool T>
    inline int32 LZXCodec<T>::hash(const byte* p)
    {
        return ((uint64(LittleEndian::readLong64(p)) << HASH_LSHIFT) * HASH_SEED) >> HASH_RSHIFT;
    }

    template <bool T>
    inline int LZXCodec<T>::emitLength(byte block[], int length)
    {
        if (length < 254) {
            block[0] = byte(length);
            return 1;
        }

        if (length < 65536 + 254) {
            const uint32 l = (length - 254) | 0x00FE0000;
            kanzi::BigEndian::writeInt32(&block[0], l << 8);
            return 3;
        }

        const uint32 l = (length - 255) | 0xFF000000;
        kanzi::BigEndian::writeInt32(&block[0], l);
        return 4;
    }

    template <bool T>
    inline uint LZXCodec<T>::readLength(const byte block[], int& pos)
    {
        uint res = uint(block[pos++]);

        if (res < 254)
            return res;

        if (res == 254) {
            res += uint((kanzi::BigEndian::readInt16(&block[pos])) & 0xFFFF);
            pos += 2;
            return res;
        }

        res += (uint32(kanzi::BigEndian::readInt32(&block[pos])) >> 8);
        pos += 3;
        return res;
    }


    template <bool T>
    inline int LZXCodec<T>::findMatch(const byte src[], const int srcIdx, const int ref, const int maxMatch)
    {
        int n = 0;

        while (n + 8 <= maxMatch) {
            const int64 diff = LittleEndian::readLong64(&src[srcIdx + n]) ^ LittleEndian::readLong64(&src[ref + n]);

            if (diff != 0) {
                n += (Global::trailingZeros(uint64(diff)) >> 3);
                break;
            }

            n += 8;
        }

        return n;
    }

    inline int LZPCodec::findMatch(const byte src[], const int srcIdx, const int ref, const int maxMatch)
    {
        int n = 0;

        while (n + 8 <= maxMatch) {
            const int64 diff = LittleEndian::readLong64(&src[srcIdx + n]) ^ LittleEndian::readLong64(&src[ref + n]);

            if (diff != 0) {
                n += (Global::trailingZeros(uint64(diff)) >> 3);
                break;
            }

            n += 8;
        }

        return n;
    }

}
#endif

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

#include "LZCodec.hpp"
#include "../Memory.hpp"
#include "TransformFactory.hpp"

using namespace kanzi;
using namespace std;

LZCodec::LZCodec()
{
    _delegate = new LZXCodec<false>();
}

LZCodec::LZCodec(Context& ctx)
{
    const int lzType = ctx.getInt("lz", TransformFactory<kanzi::byte>::LZ_TYPE);

    if (lzType == TransformFactory<kanzi::byte>::LZP_TYPE) {
        _delegate = new LZPCodec(ctx);
    }
    else if (lzType == TransformFactory<kanzi::byte>::LZX_TYPE) {
        _delegate = new LZXCodec<true>(ctx);
    }
    else {
        _delegate = new LZXCodec<false>(ctx);
    }
}

bool LZCodec::forward(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if (input._array == output._array)
        return false;

    return _delegate->forward(input, output, count);
}

bool LZCodec::inverse(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if (input._array == output._array)
        return false;

    return _delegate->inverse(input, output, count);
}


template<>
const uint LZXCodec<false>::HASH_SEED = 0x1E35A7BD;
template<>
const uint LZXCodec<false>::HASH_LOG = 16;
template<>
const uint LZXCodec<false>::HASH_RSHIFT = 64 - HASH_LOG;
template<>
const uint LZXCodec<false>::HASH_LSHIFT = 24;
template<>
const int LZXCodec<false>::MAX_DISTANCE1 = (1 << 16) - 2;
template<>
const int LZXCodec<false>::MAX_DISTANCE2 = (1 << 24) - 2;
template<>
const int LZXCodec<false>::MIN_MATCH4 = 4;
template<>
const int LZXCodec<false>::MIN_MATCH6 = 6;
template<>
const int LZXCodec<false>::MIN_MATCH9 = 9;
template<>
const int LZXCodec<false>::MAX_MATCH = 65535 + 254 + MIN_MATCH4;
template<>
const int LZXCodec<false>::MIN_BLOCK_LENGTH = 24;
template<>
const int LZXCodec<false>::READ_LENGTH_GUARD = 4;
template<>
const uint LZXCodec<true>::HASH_SEED = 0x1E35A7BD;
template<>
const uint LZXCodec<true>::HASH_LOG = 19;
template<>
const uint LZXCodec<true>::HASH_RSHIFT = 64 - HASH_LOG;
template<>
const uint LZXCodec<true>::HASH_LSHIFT = 24;
template<>
const int LZXCodec<true>::MAX_DISTANCE1 = (1 << 16) - 2;
template<>
const int LZXCodec<true>::MAX_DISTANCE2 = (1 << 24) - 2;
template<>
const int LZXCodec<true>::MIN_MATCH4 = 4;
template<>
const int LZXCodec<true>::MIN_MATCH6 = 6;
template<>
const int LZXCodec<true>::MIN_MATCH9 = 9;
template<>
const int LZXCodec<true>::MAX_MATCH = 65535 + 254 + MIN_MATCH4;
template<>
const int LZXCodec<true>::MIN_BLOCK_LENGTH = 24;
template<>
const int LZXCodec<true>::READ_LENGTH_GUARD = 4;



template <bool T>
bool LZXCodec<T>::forward(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    // Forward LZ processing:
    // 1. Allocate/reset the hash table and staging buffers.
    // 2. Reserve the last 18 input bytes as a safe literal tail.
    // 3. At each position:
    //    - hash the current position;
    //    - test the two repeat distances at srcIdx + 1;
    //    - if no repeat match is found, test the latest hash candidate at srcIdx;
    //    - optionally test matches at srcIdx + 1 and srcIdx + 2;
    //    - extend the selected match backwards to reduce the literal run.
    // 4. Emit the literal length, repeat or explicit distance, match length,
    //    and literals.
    // 5. Insert all positions covered by the match into the hash table.
    // 6. Append the final literals and concatenate the staging buffers.
    if (count == 0)
        return true;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw invalid_argument("LZ codec: Invalid input block");

    if (count > input._length - input._index)
        return false;

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw invalid_argument("LZ codec: Invalid output block");

    if (output._length - output._index < getMaxEncodedLength(count))
        return false;

    // If too small, skip
    if (count < MIN_BLOCK_LENGTH)
        return false;

    if (_hashSize == 0) {
        const int newSize = 1 << HASH_LOG;
        int32* hashes = new int32[newSize];
        delete[] _hashes;
        _hashes = hashes;
        _hashSize = newSize;
    }

    if (_bufferSize < max(count / 5, 256)) {
        const int newSize = max(count / 5, 256);
        kanzi::byte* mLenBuf = new kanzi::byte[newSize];
        kanzi::byte* mBuf = nullptr;
        kanzi::byte* tkBuf = nullptr;

        try {
            mBuf = new kanzi::byte[newSize];
            tkBuf = new kanzi::byte[newSize];
        }
        catch (...) {
            delete[] mLenBuf;
            delete[] mBuf;
            delete[] tkBuf;
            throw;
        }

        delete[] _mLenBuf;
        delete[] _mBuf;
        delete[] _tkBuf;
        _mLenBuf = mLenBuf;
        _mBuf = mBuf;
        _tkBuf = tkBuf;
        _bufferSize = newSize;
    }

    memset(_hashes, 0, sizeof(int32) * _hashSize);
    const int srcEnd = count - 16 - 2;
    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];
    const int maxDist = (srcEnd < 4 * MAX_DISTANCE1) ? MAX_DISTANCE1 : MAX_DISTANCE2;
    dst[12] = (maxDist == MAX_DISTANCE1) ? kanzi::byte(0) : kanzi::byte(1);
    int mm = MIN_MATCH4;

    if (_pCtx != nullptr) {
        Global::DataType dt = (Global::DataType)_pCtx->getInt("dataType", Global::UNDEFINED);

        if (dt == Global::DNA) {
            // Longer min match for DNA input
            mm = MIN_MATCH6;
        }
        else if (dt == Global::SMALL_ALPHABET) {
            return false;
        }
    }

    // dst[12] = 0000MMMD (4 bits + 3 bits minMatch + 1 bit max distance)
    dst[12] |= kanzi::byte(((mm - 2) & 0x07) << 1); // minMatch in [2..9]
    const int minMatch = mm;
    int srcIdx = 0;
    int dstIdx = 13;
    int anchor = 0;
    int mIdx = 0;
    int mLenIdx = 0;
    int tkIdx = 0;
    int repd[] = { count, count };
    int repIdx = 0;
    int srcInc = 0;

    while (srcIdx < srcEnd) {
        int bestLen = 0;
        const int32 h0 = hash(&src[srcIdx]);
        const int ref0 = _hashes[h0];
        _hashes[h0] = srcIdx;
        const int srcIdx1 = srcIdx + 1;
        int ref = srcIdx1 - repd[repIdx];
        const int minRef = max(srcIdx - maxDist, 0);

        if ((ref > minRef) && KANZI_MEM_EQ4(&src[srcIdx1], &src[ref])) {
            // Check repd0 first
            bestLen = findMatch(src, srcIdx1, ref, min(srcEnd - srcIdx1, MAX_MATCH));
        }
        else {
            ref = srcIdx1 - repd[repIdx ^ 1];

            if ((ref > minRef) && KANZI_MEM_EQ4(&src[srcIdx1], &src[ref])) {
                // Check repd1 first
                bestLen = findMatch(src, srcIdx1, ref, min(srcEnd - srcIdx1, MAX_MATCH));
            }
        }

        if (bestLen < minMatch) {
            // Check match at position in hash table
            ref = ref0;

            if ((ref > minRef) && KANZI_MEM_EQ4(&src[srcIdx], &src[ref])) {
                bestLen = findMatch(src, srcIdx, ref, min(srcEnd - srcIdx, MAX_MATCH));
            }

            // No good match ?
            if (bestLen < minMatch) {
                srcIdx = srcIdx1 + (srcInc >> 6);
                srcInc++;
                repIdx = 0;
                continue;
            }

            if ((srcIdx - ref != repd[0]) && (srcIdx - ref != repd[1])) {
                // Check if better match at next position
                const int32 h1 = hash(&src[srcIdx1]);
                const int ref1 = _hashes[h1];
                _hashes[h1] = srcIdx1;

                if ((ref1 > minRef + 1) && KANZI_MEM_EQ4(&src[srcIdx1 + bestLen - 3], &src[ref1 + bestLen - 3])) {
                    const int bestLen1 = findMatch(src, srcIdx1, ref1, min(srcEnd - srcIdx1, MAX_MATCH));

                    // Select best match
                    if (bestLen1 >= bestLen) {
                        ref = ref1;
                        bestLen = bestLen1;
                        srcIdx = srcIdx1;
                    }
                }

                if (T == true) {
                   const int srcIdx2 = srcIdx1 + 1;
                   const int32 h2 = hash(&src[srcIdx2]);
                   const int ref2 = _hashes[h2];
                   _hashes[h2] = srcIdx2;

                   if ((ref2 > minRef + 2) && KANZI_MEM_EQ4(&src[srcIdx2 + bestLen - 3], &src[ref2 + bestLen - 3])) {
                       const int bestLen2 = findMatch(src, srcIdx2, ref2, min(srcEnd - srcIdx2, MAX_MATCH));

                       // Select best match
                       if (bestLen2 >= bestLen) {
                           ref = ref2;
                           bestLen = bestLen2;
                           srcIdx = srcIdx2;
                       }
                    }
                }
            }

            // Extend backwards
            while ((srcIdx > anchor) && (ref > minRef) && (src[srcIdx - 1] == src[ref - 1])) {
                bestLen++;
                ref--;
                srcIdx--;
            }

            if (bestLen > MAX_MATCH) {
                ref += (bestLen - MAX_MATCH);
                srcIdx += (bestLen - MAX_MATCH);
                bestLen = MAX_MATCH;
            }
        }
        else {
            if ((bestLen >= MAX_MATCH) || (src[srcIdx] != src[ref - 1])) {
                srcIdx++;
                const int32 h1 = hash(&src[srcIdx]);
                _hashes[h1] = srcIdx;
            }
            else {
                bestLen++;
                ref--;
            }
        }

        // Emit match
        srcInc = 0;

        // Token: 3 bits litLen + 2 bits flag + 3 bits mLen (LLLFFMMM)
        //    or  3 bits litLen + 3 bits flag + 2 bits mLen (LLLFFFMM)
        // LLL : <= 7 --> LLL == literal length (if 7, remainder encoded outside of token)
        // MMM : <= 7 --> MMM == match length (if 7, remainder encoded outside of token)
        // MM  : <= 3 --> MM  == match length (if 3, remainder encoded outside of token)
        // FF = 01    --> 1 byte dist
        // FF = 10    --> 2 byte dist
        // FF = 11    --> 3 byte dist
        // FFF = 000  --> dist == repd0
        // FFF = 001  --> dist == repd1
        const int dist = srcIdx - ref;
        int token, mLenTh;

        if (dist == repd[0]) {
            token = 0x00;
            mLenTh = 3;
        }
        else if (dist == repd[1]) {
            token = 0x04;
            mLenTh = 3;
        }
        else {
            // Emit distance (since not repeat)
            //   1-byte distance: dist - 1
            //   2-byte distance: dist - 257
            //   3-byte distance: dist - 65793
            const int encodedDist = dist - 1;

            if (encodedDist < 256) {
                _mBuf[mIdx++] = kanzi::byte(encodedDist);
                token = 0x08;
            }
            else if (encodedDist < 65792) {
                const uint32 value = uint32(encodedDist - 256) << 16;
                BigEndian::writeInt32(&_mBuf[mIdx], int32(value));
                mIdx += 2;
                token = 0x10;
            }
            else {
                const uint32 value = uint32(encodedDist - 65792) << 8;
                BigEndian::writeInt32(&_mBuf[mIdx], int32(value));
                mIdx += 3;
                token = 0x18;
            }
            mLenTh = 7;
        }

        const int mLen = bestLen - minMatch;

        // Emit match length
        if (mLen >= mLenTh) {
            token += mLenTh;
            mLenIdx += emitLength(&_mLenBuf[mLenIdx], mLen - mLenTh);
        }
        else {
            token += mLen;
        }

        repd[1] = repd[0];
        repd[0] = dist;
        repIdx = 1;
        const int litLen = srcIdx - anchor;

        // Emit token
        // Literals to process ?
        if (litLen == 0) {
            _tkBuf[tkIdx++] = kanzi::byte(token);
        }
        else {
            // Emit literal length
            if (litLen >= 7) {
                if (litLen >= (1 << 24))
                    return false;

                _tkBuf[tkIdx++] = kanzi::byte((7 << 5) | token);
                dstIdx += emitLength(&dst[dstIdx], litLen - 7);
            }
            else {
                _tkBuf[tkIdx++] = kanzi::byte((litLen << 5) | token);
            }

            // Emit literals
            emitLiterals(&src[anchor], &dst[dstIdx], litLen);
            dstIdx += litLen;
        }

        if (mIdx >= _bufferSize - 8) {
            // Expand match buffer
            kanzi::byte* mBuf = new kanzi::byte[(_bufferSize * 3) / 2];
            memcpy(&mBuf[0], &_mBuf[0], _bufferSize);

            if ( _mBuf != nullptr)
                delete[] _mBuf;

            _mBuf = mBuf;

            if (mLenIdx >= _bufferSize - 8) {
                kanzi::byte* mLenBuf = new kanzi::byte[(_bufferSize * 3) / 2];
                memcpy(&mLenBuf[0], &_mLenBuf[0], _bufferSize);

                if (_mLenBuf != nullptr)
                   delete[] _mLenBuf;

                _mLenBuf = mLenBuf;
            }

            _bufferSize = (_bufferSize * 3) / 2;
        }

        // Fill _hashes and update positions. The current position was already
        // inserted above; sample four positions out of every eight (offsets
        // 0, 3, 5, and 6; two even and two odd) to reduce hash-table work
        // while retaining coverage.
        anchor = srcIdx + bestLen;

        int hashIdx = srcIdx + 1;

        while (hashIdx + 6 < anchor) {
            const int32 hh0 = hash(&src[hashIdx]);
            const int32 hh1 = hash(&src[hashIdx + 3]);
            const int32 hh2 = hash(&src[hashIdx + 5]);
            const int32 hh3 = hash(&src[hashIdx + 6]);
            _hashes[hh0] = hashIdx;
            _hashes[hh1] = hashIdx + 3;
            _hashes[hh2] = hashIdx + 5;
            _hashes[hh3] = hashIdx + 6;
            hashIdx += 8;
        }

        while (hashIdx < anchor) {
            const int32 h = hash(&src[hashIdx]);
            _hashes[h] = hashIdx;
            hashIdx++;
        }

        srcIdx = anchor;
    }

    // Emit last literals
    const int litLen = count - anchor;

    if (dstIdx + litLen + tkIdx + mIdx + mLenIdx >= count)
        return false;

    if (litLen >= 7) {
        _tkBuf[tkIdx++] = kanzi::byte(7 << 5);
        dstIdx += emitLength(&dst[dstIdx], litLen - 7);
    }
    else {
        _tkBuf[tkIdx++] = kanzi::byte(litLen << 5);
    }

    memcpy(&dst[dstIdx], &src[anchor], litLen);
    dstIdx += litLen;

    // Emit buffers: literals + tokens + matches
    LittleEndian::writeInt32(&dst[0], dstIdx);
    LittleEndian::writeInt32(&dst[4], tkIdx);
    LittleEndian::writeInt32(&dst[8], mIdx);
    memcpy(&dst[dstIdx], &_tkBuf[0], tkIdx);
    dstIdx += tkIdx;
    memcpy(&dst[dstIdx], &_mBuf[0], mIdx);
    dstIdx += mIdx;
    memcpy(&dst[dstIdx], &_mLenBuf[0], mLenIdx);
    dstIdx += mLenIdx;
    input._index += count;
    output._index += dstIdx;
    return dstIdx <= count - (count / 100);
}

template <bool T>
bool LZXCodec<T>::inverse(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    // CompressedInputStream supplies trailing input padding. The versioned
    // decoders rely on READ_LENGTH_GUARD bytes for readLength() lookahead.
    int bsVersion = _pCtx == nullptr ? 6 : _pCtx->getInt("bsVersion", 6);

    if (bsVersion < 6)
       return inverseV5(input, output, count);

    if (bsVersion < 7)
        return inverseV6(input, output, count);

    return inverseV7(input, output, count);
}


template <bool T>
bool LZXCodec<T>::inverseV6(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if (count < 13)
        return false;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw invalid_argument("LZ codec: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw invalid_argument("LZ codec: Invalid output block");

    const int inputSize = input._length - input._index;

    // readLength() reads four bytes for a three-byte length encoding.
    if ((inputSize < READ_LENGTH_GUARD) || (count > inputSize - READ_LENGTH_GUARD))
       return false;

    const int dstCapacity = output._length - output._index;

    // A 16-byte match copy may write up to 15 bytes past the logical match end.
    if (dstCapacity < 15)
        return false;

    const int dstEnd = dstCapacity - 15;
    kanzi::byte* dst = &output._array[output._index];
    const kanzi::byte* src = &input._array[input._index];

    int tkIdx = LittleEndian::readInt32(&src[0]);
    int mIdx = LittleEndian::readInt32(&src[4]);
    int mLenIdx = LittleEndian::readInt32(&src[8]);

    // Sanity checks
    if ((tkIdx < 0) || (mIdx < 0) || (mLenIdx < 0))
        return false;

    if ((tkIdx <= 13) || (tkIdx > count) || (mIdx > count - tkIdx) || (mLenIdx > count - tkIdx - mIdx))
        return false;

    mIdx += tkIdx;
    mLenIdx += mIdx;

    const int tokenEnd = mIdx;
    const int matchEnd = mLenIdx;
    const int srcEnd = tkIdx - 13;
    const int litEnd = tkIdx;
    const int maxDist = ((int(src[12]) & 1) == 0) ? MAX_DISTANCE1 : MAX_DISTANCE2;
    const int minMatch = ((int(src[12]) >> 1) & 0x07) + 2;
    bool res = true;
    int srcIdx = 13;
    int dstIdx = 0;
    int repd0 = count;
    int repd1 = count;

    while (true) {
        const int token = int(src[tkIdx++]);

        // Get match length and distance
        int mLen, dist;

        if ((token & 0x18) == 0) {
            // Repetition distance, read mLen remainder (if any) outside of token
            mLen = token & 0x03;

            if (KANZI_UNLIKELY((mLen == 3) && (mLenIdx >= count))) {
                res = false;
                goto exit;
            }

            mLen += (mLen == 3 ? minMatch + int(readLength(src, mLenIdx)) : minMatch);
            dist = (token & 0x04) == 0 ? repd0 : repd1;
        }
        else {
            // Read mLen remainder (if any) outside of token
            mLen = token & 0x07;

            if (KANZI_UNLIKELY((mLen == 7) && (mLenIdx >= count))) {
                res = false;
                goto exit;
            }

            mLen += (mLen == 7 ? minMatch + int(readLength(src, mLenIdx)) : minMatch);
            if (KANZI_UNLIKELY(mIdx >= count)) {
                res = false;
                goto exit;
            }

            dist = int(src[mIdx++]);
            const int f1 = (token >> 4) & 1;
            const int f2 = (token >> 3) & f1;
            dist = (dist << (8 & -f1)) | (-f1 & int(src[mIdx]));
            mIdx += f1;
            dist = (dist << (8 & -f2)) | (-f2 & int(src[mIdx]));
            mIdx += f2;
        }

        if (token >= 32) {
            // Get literal length
            const uint litLen = (token >= 0xE0) ? uint(7) + readLength(src, srcIdx) : uint(token >> 5);

            if ((litLen > uint(dstEnd - dstIdx)) || (litLen > uint(litEnd - srcIdx))) {
                res = false;
                goto exit;
            }

            // Emit literals
            const kanzi::byte* s = &src[srcIdx];
            kanzi::byte* d = &dst[dstIdx];
            srcIdx += int(litLen);
            dstIdx += int(litLen);

            if (srcIdx >= srcEnd) {
                memcpy(d, s, litLen);
                break;
            }

            emitLiterals(s, d, litLen);
        }

        repd1 = repd0;
        repd0 = dist;
        const int mEnd = dstIdx + mLen;
        int ref = dstIdx - dist;

        // Sanity check
        if ((dist == 0) || (ref < 0) || (dist > maxDist) || (mEnd > dstEnd)) {
            res = false;
            goto exit;
        }

        if ((dist >= 64) && (mLen >= 64))
            prefetchRead(&dst[dstIdx + 64]);

        // Copy match
        if (dist >= 16) {
            do {
                // The stream decoder supplies trailing padding for this
                // 16-byte copy, which may write up to 15 bytes past mEnd.
                KANZI_MEM_CP16(&dst[dstIdx], &dst[ref]);
                ref += 16;
                dstIdx += 16;
            } while (dstIdx < mEnd);
        }
        else if (dist != 1) {
            const kanzi::byte* s = &dst[ref];
            kanzi::byte* p = &dst[dstIdx];
            const kanzi::byte* pend = &p[mLen];

            while (p < pend)
               *p++ = *s++;
        }
        else {
            // dist = 1
            memset(&dst[dstIdx], int(dst[ref]), mLen);
        }

        dstIdx = mEnd;
    }

exit:
    output._index += dstIdx;
    input._index += count;
    return res && (srcIdx == srcEnd + 13) && (tkIdx == tokenEnd) &&
        (mIdx == matchEnd) && (mLenIdx == count);
}


template <bool T>
bool LZXCodec<T>::inverseV7(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if (count < 13)
        return false;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw invalid_argument("LZ codec: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw invalid_argument("LZ codec: Invalid output block");

    const int inputSize = input._length - input._index;

    // readLength() reads four bytes for a three-byte length encoding.
    if ((inputSize < READ_LENGTH_GUARD) || (count > inputSize - READ_LENGTH_GUARD))
       return false;

    const int dstCapacity = output._length - output._index;

    // A 16-byte match copy may write up to 15 bytes past the logical match end.
    if (dstCapacity < 15)
        return false;

    const int dstEnd = dstCapacity - 15;
    kanzi::byte* dst = &output._array[output._index];
    const kanzi::byte* src = &input._array[input._index];

    int tkIdx = LittleEndian::readInt32(&src[0]);
    int mIdx = LittleEndian::readInt32(&src[4]);
    int mLenIdx = LittleEndian::readInt32(&src[8]);

    // Sanity checks
    if ((tkIdx < 0) || (mIdx < 0) || (mLenIdx < 0))
        return false;

    if ((tkIdx <= 13) || (tkIdx > count) || (mIdx > count - tkIdx) || (mLenIdx > count - tkIdx - mIdx))
        return false;

    mIdx += tkIdx;
    mLenIdx += mIdx;

    const int tokenEnd = mIdx;
    const int matchEnd = mLenIdx;
    const int srcEnd = tkIdx - 13;
    const int litEnd = tkIdx;
    const int maxDist = ((int(src[12]) & 1) == 0) ? MAX_DISTANCE1 : MAX_DISTANCE2;
    const int minMatch = ((int(src[12]) >> 1) & 0x07) + 2;
    bool res = true;
    int srcIdx = 13;
    int dstIdx = 0;
    int repd0 = count;
    int repd1 = count;

    while (true) {
        const int token = int(src[tkIdx++]);

        // Get match length and distance
        int mLen, dist;

        if ((token & 0x18) == 0) {
            // Repetition distance, read mLen remainder (if any) outside of token
            mLen = token & 0x03;

            if (KANZI_UNLIKELY((mLen == 3) && (mLenIdx >= count))) {
                res = false;
                goto exit;
            }

            mLen += (mLen == 3 ? minMatch + int(readLength(src, mLenIdx)) : minMatch);
            dist = (token & 0x04) == 0 ? repd0 : repd1;
        }
        else {
            // Read mLen remainder (if any) outside of token
            mLen = token & 0x07;

            if (KANZI_UNLIKELY((mLen == 7) && (mLenIdx >= count))) {
                res = false;
                goto exit;
            }

            mLen += (mLen == 7 ? minMatch + int(readLength(src, mLenIdx)) : minMatch);
            if (KANZI_UNLIKELY(mIdx >= count)) {
                res = false;
                goto exit;
            }

            static const int DIST_SHIFT[4] = { 0, 24, 16, 8 };
            static const uint32 DIST_BIAS[4] = { 0, 1, 257, 65793 };
            const int width = (token >> 3) & 3;

            const uint32 value = uint32(BigEndian::readInt32(&src[mIdx]));
            dist = int((value >> DIST_SHIFT[width]) + DIST_BIAS[width]);
            mIdx += width;
        }

        if (token >= 32) {
            // Get literal length
            const uint litLen = (token >= 0xE0) ? uint(7) + readLength(src, srcIdx) : uint(token >> 5);

            if (KANZI_UNLIKELY((uint(srcIdx) + litLen > uint(litEnd)) || (litLen > uint(dstEnd - dstIdx)))) {
                res = false;
                goto exit;
            }

            // Emit literals
            const kanzi::byte* s = &src[srcIdx];
            kanzi::byte* d = &dst[dstIdx];
            srcIdx += int(litLen);
            dstIdx += int(litLen);

            if (KANZI_UNLIKELY(srcIdx >= srcEnd)) {
                memcpy(d, s, litLen);
                break;
            }

            emitLiterals(s, d, litLen);
        }

        repd1 = repd0;
        repd0 = dist;
        const int mEnd = dstIdx + mLen;
        int ref = dstIdx - dist;

        // Sanity check
        if (KANZI_UNLIKELY((ref < 0) || (dist > maxDist) || (mEnd > dstEnd))) {
            res = false;
            goto exit;
        }

        // Copy match
        if (dist >= 16) {
            if (mLen >= 64)
                prefetchRead(&dst[dstIdx + 64]);

            do {
                // The stream decoder supplies trailing padding for this
                // 16-byte copy, which may write up to 15 bytes past mEnd.
                KANZI_MEM_CP16(&dst[dstIdx], &dst[ref]);
                ref += 16;
                dstIdx += 16;
            } while (dstIdx < mEnd);
        }
        else if (dist != 1) {
            const kanzi::byte* s = &dst[ref];
            kanzi::byte* p = &dst[dstIdx];
            const kanzi::byte* pend = &p[mLen];

            while (p < pend)
               *p++ = *s++;
        }
        else {
            // dist = 1
            memset(&dst[dstIdx], int(dst[ref]), mLen);
        }

        dstIdx = mEnd;
    }

exit:
    output._index += dstIdx;
    input._index += count;
    return res && (srcIdx == srcEnd + 13) && (tkIdx == tokenEnd) &&
        (mIdx == matchEnd) && (mLenIdx == count);
}


template <bool T>
bool LZXCodec<T>::inverseV5(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if (count < 13)
        return false;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw invalid_argument("LZ codec: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw invalid_argument("LZ codec: Invalid output block");

    const int inputSize = input._length - input._index;

    // readLength() reads four bytes for a three-byte length encoding.
    if ((inputSize < READ_LENGTH_GUARD) || (count > inputSize - READ_LENGTH_GUARD))
        return false;

    const int dstEnd = output._length - output._index;
    kanzi::byte* dst = &output._array[output._index];
    const kanzi::byte* src = &input._array[input._index];

    int tkIdx = LittleEndian::readInt32(&src[0]);
    int mIdx = LittleEndian::readInt32(&src[4]);
    int mLenIdx = LittleEndian::readInt32(&src[8]);

    // Sanity checks
    if ((tkIdx < 0) || (mIdx < 0) || (mLenIdx < 0))
        return false;

    if ((tkIdx <= 13) || (tkIdx > count) || (mIdx > count - tkIdx) || (mLenIdx > count - tkIdx - mIdx))
        return false;

    mIdx += tkIdx;
    mLenIdx += mIdx;

    const int tokenEnd = mIdx;
    const int matchEnd = mLenIdx;
    const int srcEnd = tkIdx - 13;
    const int litEnd = tkIdx;
    const int mFlag = int(src[12]) & 1;
    const int maxDist = (mFlag == 0) ? MAX_DISTANCE1 : MAX_DISTANCE2;
    const int mmIdx = (int(src[12]) >> 1) & 0x03;
    const int MIN_MATCHES[4] = { MIN_MATCH4, MIN_MATCH9, MIN_MATCH6, MIN_MATCH6 };
    const int minMatch = MIN_MATCHES[mmIdx];
    bool res = true;
    int srcIdx = 13;
    int dstIdx = 0;
    int repd0 = 0;
    int repd1 = 0;

    while (true) {
        const int token = int(src[tkIdx++]);

        if (token >= 32) {
            // Get literal length
            const uint litLen = (token >= 0xE0) ? uint(7) + readLength(src, srcIdx) : uint(token >> 5);

            if ((litLen > uint(dstEnd - dstIdx)) || (litLen > uint(litEnd - srcIdx))) {
                res = false;
                goto exit;
            }

            // Emit literals
            const kanzi::byte* s = &src[srcIdx];
            kanzi::byte* d = &dst[dstIdx];
            srcIdx += int(litLen);
            dstIdx += int(litLen);

            if (srcIdx >= srcEnd) {
                memcpy(d, s, litLen);
                break;
            }

            emitLiterals(s, d, litLen);
        }

        // Get match length and distance
        int mLen = token & 0x0F;
        int dist;

        if (mLen == 15) {
            // Repetition distance, read mLen fully outside of token

            if (KANZI_UNLIKELY(mLenIdx >= count)) {
                res = false;
                goto exit;
            }

            mLen = minMatch + int(readLength(src, mLenIdx));
            dist = ((token & 0x10) == 0) ? repd0 : repd1;
        }
        else {
            // Read mLen remainder (if any) outside of token

            if (KANZI_UNLIKELY((mLen == 14) && (mLenIdx >= count))) {
                res = false;
                goto exit;
            }

            mLen = (mLen == 14) ? 14 + minMatch + int(readLength(src, mLenIdx)) : mLen + minMatch;

            if (KANZI_UNLIKELY(mIdx >= count)) {
                res = false;
                goto exit;
            }

            dist = int(src[mIdx++]);

            if (mFlag != 0)
                dist = (dist << 8) | int(src[mIdx++]);

            const int t = (token >> 4) & 1;
            dist = (dist << (8 * t)) | (-t & int(src[mIdx]));
            mIdx += t;
        }

        prefetchRead(&src[mLenIdx]);
        repd1 = repd0;
        repd0 = dist;
        const int mEnd = dstIdx + mLen;
        int ref = dstIdx - dist;

        // Sanity check
        if ((dist == 0) || (ref < 0) || (dist > maxDist) || (mEnd > dstEnd)) {
            res = false;
            goto exit;
        }

        prefetchWrite(&dst[dstIdx]);

        // Copy match
        if (dist >= 16) {
            do {
                // The stream decoder supplies trailing padding for this
                // 16-byte copy, which may write up to 15 bytes past mEnd.
                KANZI_MEM_CP16(&dst[dstIdx], &dst[ref]);
                ref += 16;
                dstIdx += 16;
            } while (dstIdx < mEnd);
        }
        else if (dist != 1) {
            const kanzi::byte* s = &dst[ref];
            kanzi::byte* p = &dst[dstIdx];
            const kanzi::byte* pend = &p[mLen];

            while (p < pend)
               *p++ = *s++;
        }
        else {
            // dist = 1
            memset(&dst[dstIdx], int(dst[ref]), mLen);
        }

        dstIdx = mEnd;
    }

exit:
    output._index += dstIdx;
    input._index += count;
    return res && (srcIdx == srcEnd + 13) && (tkIdx == tokenEnd) &&
        (mIdx == matchEnd) && (mLenIdx == count);
}


const uint LZPCodec::HASH_SEED = 0x7FEB352D;
const uint LZPCodec::HASH_LOG = 16;
const uint LZPCodec::HASH_SHIFT = 32 - HASH_LOG;
const int LZPCodec::MIN_MATCH = 64;
const int LZPCodec::MIN_BLOCK_LENGTH = 128;
const int LZPCodec::MATCH_FLAG = 0xFC;


bool LZPCodec::forward(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if (count < 4)
        return false;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw invalid_argument("LZP codec: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw invalid_argument("LZP codec: Invalid output block");

    if (count > input._length - input._index)
        return false;

    if (output._length - output._index < getMaxEncodedLength(count))
        return false;

    // If too small, skip
    if (count < MIN_BLOCK_LENGTH)
        return false;

    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];
    const int srcEnd = count;
    const int dstEnd = count - (count >> 6);

    if (_hashSize == 0) {
        const int newSize = 1 << HASH_LOG;
        int32* hashes = new int32[newSize];
        delete[] _hashes;
        _hashes = hashes;
        _hashSize = newSize;
    }

    memset(_hashes, 0, sizeof(int32) * _hashSize);
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
    uint ctx = LittleEndian::readInt32(&src[0]);
    int srcIdx = 4;
    int dstIdx = 4;

    while ((srcIdx < srcEnd - MIN_MATCH) && (dstIdx < dstEnd)) {
        const uint32 h = (HASH_SEED * ctx) >> HASH_SHIFT;
        const int32 ref = _hashes[h];
        _hashes[h] = srcIdx;
        int bestLen = 0;

        // Find a match
        if ((ref != 0) && KANZI_MEM_EQ8(&src[ref + MIN_MATCH - 8], &src[srcIdx + MIN_MATCH - 8]))
            bestLen = findMatch(src, srcIdx, ref, srcEnd - srcIdx);

        // No good match ?
        if (bestLen < MIN_MATCH) {
            const uint val = uint(src[srcIdx]);
            ctx = (ctx << 8) | val;
            dst[dstIdx++] = src[srcIdx++];

            if ((ref != 0) && (val == MATCH_FLAG)) {
                if (dstIdx >= dstEnd)
                    return false;

                dst[dstIdx++] = kanzi::byte(0xFF);
            }

            continue;
        }

        srcIdx += bestLen;
        ctx = LittleEndian::readInt32(&src[srcIdx - 4]);
        dst[dstIdx++] = kanzi::byte(MATCH_FLAG);
        bestLen -= MIN_MATCH;

        // Emit match length
        while ((bestLen >= 254) && (dstIdx < dstEnd)) {
            bestLen -= 254;
            dst[dstIdx++] = kanzi::byte(0xFE);
        }

        if (dstIdx >= dstEnd)
            return false;

        dst[dstIdx++] = kanzi::byte(bestLen);
    }

    while ((srcIdx < srcEnd) && (dstIdx < dstEnd)) {
        const uint32 h = (HASH_SEED * ctx) >> HASH_SHIFT;
        const int ref = _hashes[h];
        _hashes[h] = srcIdx;
        const uint val = uint(src[srcIdx]);
        ctx = (ctx << 8) | val;
        dst[dstIdx++] = src[srcIdx++];

        if ((ref != 0) && (val == MATCH_FLAG)) {
            if (dstIdx >= dstEnd)
                return false;

            dst[dstIdx++] = kanzi::byte(0xFF);
        }
    }

    input._index += srcIdx;
    output._index += dstIdx;
    return (srcIdx == count) && (dstIdx < dstEnd);
}

bool LZPCodec::inverse(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw invalid_argument("LZP codec: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw invalid_argument("LZP codec: Invalid output block");

    if (count > input._length - input._index)
        return false;

    if (count < 4)
        return false;

    const int srcEnd = count;
    const int dstEnd = output._length - output._index;
    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];

    if (dstEnd < count)
        return false;

    if (_hashSize == 0) {
        const int newSize = 1 << HASH_LOG;
        int32* hashes = new int32[newSize];
        delete[] _hashes;
        _hashes = hashes;
        _hashSize = newSize;
    }

    memset(_hashes, 0, sizeof(int32) * _hashSize);
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
    uint32 ctx = LittleEndian::readInt32(&dst[0]);
    int srcIdx = 4;
    int dstIdx = 4;

    while (srcIdx < srcEnd) {
        const int32 h = (HASH_SEED * ctx) >> HASH_SHIFT;
        int ref = _hashes[h];
        _hashes[h] = dstIdx;

        if ((src[srcIdx] != kanzi::byte(MATCH_FLAG)) || (ref == 0)) {
            if (dstIdx >= dstEnd)
                return false;

            ctx = (ctx << 8) | uint32(src[srcIdx]);
            dst[dstIdx++] = src[srcIdx++];
            continue;
        }

        srcIdx++;

        if (srcIdx >= srcEnd)
            return false;

        if (src[srcIdx] == kanzi::byte(0xFF)) {
            if (dstIdx >= dstEnd)
                return false;

            ctx = (ctx << 8) | uint32(MATCH_FLAG);
            dst[dstIdx++] = kanzi::byte(MATCH_FLAG);
            srcIdx++;
            continue;
        }

        uint64 mLen = MIN_MATCH;

        if (src[srcIdx] == kanzi::byte(0xFE)) {
            while ((srcIdx < srcEnd) && (src[srcIdx] == kanzi::byte(0xFE))) {
                srcIdx++;
                mLen += 254;
            }

            if (srcIdx >= srcEnd)
                return false;
        }

        mLen += uint64(src[srcIdx++]);

        if (mLen > uint64(dstEnd - dstIdx))
            return false;

        const int matchLen = int(mLen);
        const int mEnd = dstIdx + matchLen;


        if (dstIdx >= ref + 16) {
            do {
                // The stream decoder supplies trailing padding for this
                // 16-byte copy, which may write up to 15 bytes past mEnd.
                KANZI_MEM_CP16(&dst[dstIdx], &dst[ref]);
                ref += 16;
                dstIdx += 16;
            } while (dstIdx < mEnd);
        }
        else {
            for (int i = 0; i < matchLen; i++)
                dst[dstIdx + i] = dst[ref + i];
        }

        dstIdx = mEnd;
        ctx = LittleEndian::readInt32(&dst[dstIdx - 4]);
    }

    input._index += srcIdx;
    output._index += dstIdx;
    return srcIdx == srcEnd;
}

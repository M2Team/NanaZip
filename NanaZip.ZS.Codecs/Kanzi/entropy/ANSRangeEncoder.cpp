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

#include <algorithm>
#include <cstring>
#include <sstream>
#include "ANSRangeEncoder.hpp"
#include "EntropyUtils.hpp"
#include "../Global.hpp"
#include "../Memory.hpp"

using namespace kanzi;
using namespace std;

const int ANSRangeEncoder::ANS_TOP = 1 << 15; // max possible for ANS_TOP=1<<23
const int ANSRangeEncoder::DEFAULT_ANS0_CHUNK_SIZE = 16384;
const int ANSRangeEncoder::DEFAULT_LOG_RANGE = 12;
const int ANSRangeEncoder::MIN_CHUNK_SIZE = 1024;
const int ANSRangeEncoder::MAX_CHUNK_SIZE = 1 << 27; // 8*MAX_CHUNK_SIZE must not overflow


// The chunk size indicates how many bytes are encoded (per block) before
// resetting the frequency stats.
ANSRangeEncoder::ANSRangeEncoder(OutputBitStream& bitstream, int order, int chunkSize, int logRange) : _bitstream(bitstream)
{
    if ((order != 0) && (order != 1))
        throw invalid_argument("ANS Codec: The order must be 0 or 1");

    if (chunkSize < MIN_CHUNK_SIZE) {
        stringstream ss;
        ss << "ANS Codec: The chunk size must be at least " << MIN_CHUNK_SIZE;
        throw invalid_argument(ss.str());
    }

    if (chunkSize > MAX_CHUNK_SIZE) {
        stringstream ss;
        ss << "ANS Codec: The chunk size must be at most " << MAX_CHUNK_SIZE;
        throw invalid_argument(ss.str());
    }

    if ((logRange < 8) || (logRange > 15)) {
        stringstream ss;
        ss << "ANS Codec: Invalid range: " << logRange << " (must be in [8..15])";
        throw invalid_argument(ss.str());
    }

    const uint64 scaledChunkSize = uint64(chunkSize) << (8 * order);
    _chunkSize = uint(min(scaledChunkSize, uint64(MAX_CHUNK_SIZE)));
    _order = order;
    const int dim = 255 * order + 1;
    _symbols = new ANSEncSymbol[dim * 256];
    _freqs = new uint[dim * 257]; // freqs[x][256] = total(freqs[x][0..255])
    _buffer = nullptr;
    _bufferSize = 0;
    _logRange = (order == 0) ? logRange : max(logRange - 1, 8);
}

ANSRangeEncoder::~ANSRangeEncoder()
{
    _dispose();

    if (_buffer != nullptr)
       delete[] _buffer;

    delete[] _symbols;
    delete[] _freqs;
}


// Compute cumulated frequencies and encode header
int ANSRangeEncoder::updateFrequencies(uint frequencies[], uint lr)
{
    int res = 0;
    const int endk = 255 * _order + 1;
    _bitstream.writeBits(lr - 8, 3); // logRange
    uint curAlphabet[256];

    for (int k = 0; k < endk; k++) {
        uint* f = &frequencies[k * 257];
        const int alphabetSize = EntropyUtils::normalizeFrequencies(f, curAlphabet, 256, f[256], 1 << lr);

        if (alphabetSize > 0) {
            ANSEncSymbol* symb = &_symbols[k << 8];
            int sum = 0;

            for (int i = 0, count = 0; i < 256; i++) {
                if (f[i] == 0)
                    continue;

                symb[i].reset(sum, f[i], lr);
                sum += f[i];
                count++;

                if (count >= alphabetSize)
                    break;
            }
        }

        if ((_order == 1) && (alphabetSize == 0)) {
            // The empty alphabet marker implicitly models the singleton {0}.
            _symbols[k << 8].reset(0, 1 << lr, lr);
        }

        encodeHeader(alphabetSize, curAlphabet, f, lr);
        res += alphabetSize;
    }

    return res;
}

// Encode alphabet and frequencies
bool ANSRangeEncoder::encodeHeader(int alphabetSize, const uint alphabet[], const uint frequencies[], uint lr) const
{
    const int encoded = EntropyUtils::encodeAlphabet(_bitstream, alphabet, 256, alphabetSize);

    if (encoded < 0)
        return false;

    if (encoded <= 1)
        return true;

    const int chkSize = (alphabetSize >= 64) ? 8 : 6;
    const int llr = Global::_log2(lr) + 1;

    // Encode all frequencies (but the first one) by chunks
    for (int i = 1; i < alphabetSize; i += chkSize) {
        uint max = frequencies[alphabet[i]] - 1;
        const int endj = min(i + chkSize, alphabetSize);

        // Search for max frequency log size in next chunk
        for (int j = i + 1; j < endj; j++) {
            if (frequencies[alphabet[j]] - 1 > max)
                max = frequencies[alphabet[j]] - 1;
        }

        const uint logMax = (max == 0) ? 0 : Global::_log2(max) + 1;
        _bitstream.writeBits(logMax, llr);

        if (logMax == 0) // all frequencies equal one in this chunk
            continue;

        // Write frequencies
        for (int j = i; j < endj; j++)
            _bitstream.writeBits(frequencies[alphabet[j]] - 1, logMax);
    }

    return true;
}

// Dynamically compute the frequencies for every chunk of data in the block
int ANSRangeEncoder::encode(const kanzi::byte block[], uint blkptr, uint count)
{
    if (count <= 32) {
        _bitstream.writeBits(&block[blkptr], 8 * count);
        return count;
    }

    const uint end = blkptr + count;
    uint startChunk = blkptr;
    uint sz = uint(_chunkSize);
    const uint size = min(sz, count);
    const uint extra = max(size >> 3, min(size, uint(1 << 16)));
    const uint bufSize = size + extra;

    if (_bufferSize < bufSize) {
        kanzi::byte* buffer = new kanzi::byte[bufSize];
        delete[] _buffer;
        _buffer = buffer;
        _bufferSize = bufSize;
    }

    while (startChunk < end) {
        const uint sizeChunk = min(sz, end - startChunk);
        const int alphabetSize = rebuildStatistics(&block[startChunk], sizeChunk, _logRange);

        // Skip chunk if only one symbol
        if ((alphabetSize <= 1) && (_order == 0)) {
            startChunk += sizeChunk;
            continue;
        }

        encodeChunk(&block[startChunk], sizeChunk);
        startChunk += sizeChunk;
    }

    return count;
}

void ANSRangeEncoder::encodeChunk(const kanzi::byte block[], int end)
{
    int st0 = ANS_TOP;
    int st1 = ANS_TOP;
    int st2 = ANS_TOP;
    int st3 = ANS_TOP;
    kanzi::byte* p = &_buffer[_bufferSize - 1];
    const kanzi::byte* p0 = p;
    const int end4 = end & -4;

    for (int i = end - 1; i >= end4; i--)
        *p-- = block[i];

    if (_order == 0) {
        for (int i = end4 - 1; i > 0; i -= 4) {
            st0 = encodeSymbol(p, st0, _symbols[int(block[i])]);
            st1 = encodeSymbol(p, st1, _symbols[int(block[i - 1])]);
            st2 = encodeSymbol(p, st2, _symbols[int(block[i - 2])]);
            st3 = encodeSymbol(p, st3, _symbols[int(block[i - 3])]);
        }
    }
    else { // order 1
        const int quarter = end4 >> 2;
        int i0 = 1 * quarter - 2;
        int i1 = 2 * quarter - 2;
        int i2 = 3 * quarter - 2;
        int i3 = end4 - 2;
        int prv0 = int(block[i0 + 1]);
        int prv1 = int(block[i1 + 1]);
        int prv2 = int(block[i2 + 1]);
        int prv3 = int(block[i3 + 1]);

        for ( ; i0 >= 0; i0--, i1--, i2--, i3--) {
            const int cur0 = int(block[i0]);
            st0 = encodeSymbol(p, st0, _symbols[(cur0 << 8) | prv0]);
            const int cur1 = int(block[i1]);
            st1 = encodeSymbol(p, st1, _symbols[(cur1 << 8) | prv1]);
            const int cur2 = int(block[i2]);
            st2 = encodeSymbol(p, st2, _symbols[(cur2 << 8) | prv2]);
            const int cur3 = int(block[i3]);
            st3 = encodeSymbol(p, st3, _symbols[(cur3 << 8) | prv3]);
            prv0 = cur0;
            prv1 = cur1;
            prv2 = cur2;
            prv3 = cur3;
        }

        // Last symbols
        st0 = encodeSymbol(p, st0, _symbols[prv0]);
        st1 = encodeSymbol(p, st1, _symbols[prv1]);
        st2 = encodeSymbol(p, st2, _symbols[prv2]);
        st3 = encodeSymbol(p, st3, _symbols[prv3]);
    }

    // Write chunk size
    EntropyUtils::writeVarInt(_bitstream, uint32(p0 - p));

    // Write final ANS states
    _bitstream.writeBits(st0, 32);
    _bitstream.writeBits(st1, 32);
    _bitstream.writeBits(st2, 32);
    _bitstream.writeBits(st3, 32);

    if (p != p0) {
        // Write encoded data to bitstream
        _bitstream.writeBits(&p[1], 8 * uint(p0 - p));
    }
}

// Compute chunk frequencies, cumulated frequencies and encode chunk header
int ANSRangeEncoder::rebuildStatistics(const kanzi::byte block[], int end, uint lr)
{
    const int dim = 255 * _order + 1;
    memset(_freqs, 0, size_t(257 * dim) * sizeof(uint));

    if (_order == 0){
       Global::computeHistogram(block, end, _freqs, true, true);
    }
    else {
       const int quarter = end >> 2;

       if (quarter == 0) {
          Global::computeHistogram(block, end, _freqs, false, true);
       }
       else {
          Global::computeHistogram(&block[0 * quarter], quarter, _freqs, false, true);
          Global::computeHistogram(&block[1 * quarter], quarter, _freqs, false, true);
          Global::computeHistogram(&block[2 * quarter], quarter, _freqs, false, true);
          Global::computeHistogram(&block[3 * quarter], quarter, _freqs, false, true);
       }
    }

    return updateFrequencies(_freqs, lr);
}

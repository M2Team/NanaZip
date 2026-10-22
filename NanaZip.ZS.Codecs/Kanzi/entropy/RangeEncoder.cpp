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
#include "RangeEncoder.hpp"
#include "EntropyUtils.hpp"
#include "../Global.hpp"

using namespace kanzi;
using namespace std;


const uint64 RangeEncoder::TOP_RANGE    = 0x0FFFFFFFFFFFFFFF;
const uint64 RangeEncoder::BOTTOM_RANGE = 0x000000000000FFFF;
const uint64 RangeEncoder::RANGE_MASK   = 0x0FFFFFFF00000000;
const int RangeEncoder::DEFAULT_CHUNK_SIZE = 1 << 15; // 32 KB by default
const int RangeEncoder::DEFAULT_LOG_RANGE = 12;
const int RangeEncoder::MAX_CHUNK_SIZE = 1 << 30;


// The chunk size indicates how many bytes are encoded (per block) before
// resetting the frequency stats.
RangeEncoder::RangeEncoder(OutputBitStream& bitstream, int chunkSize, int logRange) : _bitstream(bitstream)
{
    if (chunkSize < 1024)
        throw invalid_argument("The chunk size must be at least 1024");

    if (chunkSize > MAX_CHUNK_SIZE)
        throw invalid_argument("The chunk size must be at most 2^30");

    if ((logRange < 8) || (logRange > 15)) {
        stringstream ss;
        ss << "Invalid range parameter: " << logRange << " (must be in [8..15])";
        throw invalid_argument(ss.str());
    }

    _logRange = logRange;
    _chunkSize = chunkSize;
    reset();
}

bool RangeEncoder::reset()
{
    _low = 0;
    _range = TOP_RANGE;
    _shift = 0;
    memset(_alphabet, 0, 256 * sizeof(uint));
    memset(_freqs, 0, 256 * sizeof(uint));
    memset(_cumFreqs, 0, 257 * sizeof(uint64));
    return true;
}

int RangeEncoder::updateFrequencies(uint frequencies[], int size, int lr)
{
    int alphabetSize = EntropyUtils::normalizeFrequencies(frequencies, _alphabet, 256, size, 1 << lr);

    if (alphabetSize > 0) {
        _cumFreqs[0] = 0;

        // Create histogram of frequencies scaled to 'range'
        for (int i = 0; i < 256; i++)
            _cumFreqs[i + 1] = _cumFreqs[i] + frequencies[i];
    }

    encodeHeader(alphabetSize, _alphabet, frequencies, lr);
    return alphabetSize;
}

bool RangeEncoder::encodeHeader(int alphabetSize, const uint alphabet[], const uint frequencies[], int lr) const
{
    const int encoded = EntropyUtils::encodeAlphabet(_bitstream, alphabet, 256, alphabetSize);

    if (encoded < 0)
        return false;

    if (encoded == 0)
        return true;

    _bitstream.writeBits(lr - 8, 3); // logRange

    if (encoded == 1)
        return true;

    int chkSize = (alphabetSize >= 64) ? 8 : 6;
    int llr = 3;

    while (1 << llr <= lr)
        llr++;

    // Encode all frequencies (but the first one) by chunks
    for (int i = 1; i < alphabetSize; i += chkSize) {
        uint max = frequencies[alphabet[i]] - 1;
        int endj = min(i + chkSize, alphabetSize);

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

// Reset frequency stats for each chunk of data in the block
int RangeEncoder::encode(const kanzi::byte block[], uint blkptr, uint count)
{
    if (count == 0)
        return 0;

    const uint end = blkptr + count;
    const uint sz = _chunkSize;
    uint startChunk = blkptr;

    while (startChunk < end) {
        const uint endChunk = min(startChunk + sz, end);
        _range = TOP_RANGE;
        _low = 0;
        int lr = _logRange;

        // Lower log range if the size of the data chunk is small
        while ((lr > 8) && (uint(1 << lr) > endChunk - startChunk))
            lr--;

        if (rebuildStatistics(block, startChunk, endChunk, lr) <= 1) {
            // Skip chunk if only one symbol
            startChunk = endChunk;
            continue;
        }

        _shift = lr;

        for (uint i = startChunk; i < endChunk; i++)
            encodeByte(block[i]);

        // Flush 'low'
        _bitstream.writeBits(_low, 60);
        startChunk = endChunk;
    }

    return count;
}

void RangeEncoder::encodeByte(kanzi::byte b)
{
    // Compute next low and range
    const int symbol = int(b);
    const uint64 cumFreq = _cumFreqs[symbol];
    const uint64 freq = _cumFreqs[symbol + 1] - cumFreq;
    _range >>= _shift;
    _low += (cumFreq * _range);
    _range *= freq;

    // If the left-most digits are the same throughout the range, write bits to bitstream
    while (true) {
        if (((_low ^ (_low + _range)) & RANGE_MASK) != 0) {
            if (_range > BOTTOM_RANGE)
                  break;

            // Normalize
            _range = ~(_low - 1) & BOTTOM_RANGE;
        }

        _bitstream.writeBits(_low >> 32, 28);
        _range <<= 28;
        _low <<= 28;
    }
}

// Compute chunk frequencies, cumulated frequencies and encode chunk header
int RangeEncoder::rebuildStatistics(const kanzi::byte block[], int start, int end, int lr)
{
    memset(_freqs, 0, sizeof(_freqs));
    Global::computeHistogram(&block[start], end - start, _freqs);
    return updateFrequencies(_freqs, end - start, lr);
}

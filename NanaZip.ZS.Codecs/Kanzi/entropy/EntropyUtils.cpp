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
#include <deque>
#include <sstream>
#include "EntropyUtils.hpp"
#include "../BitStreamException.hpp"

using namespace kanzi;
using namespace std;


const int EntropyUtils::FULL_ALPHABET = 0;
const int EntropyUtils::PARTIAL_ALPHABET = 1;
const int EntropyUtils::ALPHABET_256 = 0;
const int EntropyUtils::ALPHABET_0 = 1;
const int EntropyUtils::INCOMPRESSIBLE_THRESHOLD = 973; // 0.95*1024



class FreqSortData {
public:
    uint* _freq;
    uint8 _symbol;

    FreqSortData(uint* freq, uint8 symbol) :
         _freq(freq)
       , _symbol(symbol)
    {
    }
};

struct FreqDataComparator {
    bool operator()(FreqSortData const& fd1, FreqSortData const& fd2) const
    {
        // Decreasing frequency then decreasing symbol
        int r;
        return ((r = int(*fd1._freq - *fd2._freq)) == 0) ? fd1._symbol > fd2._symbol: r > 0;
    }
};

// alphabet must be sorted in increasing order
// length = alphabet array length up to 256
int EntropyUtils::encodeAlphabet(OutputBitStream& obs, const uint alphabet[], int length, int count)
{
    // Alphabet length must be a power of 2
    if ((length & (length - 1)) != 0)
        return -1;

    if ((length > 256) || (count > length))
        return -1;

    if (count == 0) {
        obs.writeBit(FULL_ALPHABET);
        obs.writeBit(ALPHABET_0);
    }
    else if (count == 256) {
        obs.writeBit(FULL_ALPHABET);
        obs.writeBit(ALPHABET_256);
    }
    else {
        // Partial alphabet
        obs.writeBit(PARTIAL_ALPHABET);
        kanzi::byte masks[32] = { kanzi::byte(0) };

        // Encode presence flags
        for (int i = 0; i < count; i++)
            masks[alphabet[i] >> 3] |= kanzi::byte(1 << (alphabet[i] & 7));

        const int lastMask = alphabet[count - 1] >> 3;
        obs.writeBits(lastMask, 5);
        obs.writeBits(masks, 8 * (lastMask + 1));
    }

    return count;
}

int EntropyUtils::decodeAlphabet(InputBitStream& ibs, uint alphabet[])
{
    // Read encoding mode from bitstream
    if (ibs.readBit() == FULL_ALPHABET) {
        const int alphabetSize = (ibs.readBit() == ALPHABET_256) ? 256 : 0;

        // Full alphabet
        for (int i = 0; i < alphabetSize; i++)
            alphabet[i] = i;

        return alphabetSize;
    }

    // Partial alphabet
    const int lastMask = int(ibs.readBits(5));
    kanzi::byte masks[32] = { kanzi::byte(0) };
    int count = 0;

    // Decode presence flags
    ibs.readBits(masks, 8 * (lastMask + 1));

    for (int i = 0; i <= lastMask; i++) {
        const int n = 8 * i;

        for (uint j = 0; j < 8; j++) {
            const int bit = int(masks[i] >> j) & 1;
            alphabet[count] = n + j;
            count += bit;
        }
    }

    return count;
}


// Returns the size of the alphabet
// length is the length of the alphabet array
// 'totalFreq' is the sum of frequencies.
// 'scale' is the target new total of frequencies
// The alphabet and freqs parameters are updated
int EntropyUtils::normalizeFrequencies(uint freqs[], uint alphabet[], int length, uint totalFreq, uint scale)
{
    if (length > 256) {
        stringstream ss;
        ss << "Invalid alphabet size parameter: " << scale << " (must be less than or equal to 256)";
        throw invalid_argument(ss.str());
    }

    if ((scale < 256) || (scale > 65536)) {
        stringstream ss;
        ss << "Invalid scale parameter: " << scale << " (must be in [256..65536])";
        throw invalid_argument(ss.str());
    }

    if ((length == 0) || (totalFreq == 0))
        return 0;

    // Number of present symbols
    int alphabetSize = 0;

    // shortcut
    if (totalFreq == scale) {
        for (int i = 0; i < length; i++) {
            if (freqs[i] != 0)
                alphabet[alphabetSize++] = i;
        }

        return alphabetSize;
    }

    uint sumScaledFreq = 0;
    uint sumFreq = 0;
    int idxMax = 0;

    // Scale frequencies by squeezing/stretching distribution over complete range
    for (int i = 0; i < length; i++) {
        alphabet[i] = 0;
        const uint f = freqs[i];

        if (f == 0)
            continue;

        alphabet[alphabetSize++] = i;
        const int64 sf = int64(f) * int64(scale);
        const uint scaledFreq = sf <= int64(totalFreq) ? 1 : uint((sf + (int64(totalFreq) >> 1)) / int64(totalFreq));
        sumScaledFreq += scaledFreq;
        freqs[i] = scaledFreq;
        sumFreq += f;
        idxMax = (scaledFreq > freqs[idxMax]) ? i : idxMax;

        if (sumFreq >= totalFreq)
           break;
    }

    if (alphabetSize == 0)
        return 0;

    if (alphabetSize == 1) {
        freqs[alphabet[0]] = scale;
        return 1;
    }

    if (sumScaledFreq == scale)
        return alphabetSize;

    int delta = int(sumScaledFreq - scale);
    const int errThr = int(freqs[idxMax]) >> 4;

    if (abs(delta) <= errThr) {
        // Fast path (small error): just adjust the max frequency
        freqs[idxMax] -= delta;
        return alphabetSize;
    }

    if (delta < 0) {
        delta += errThr;
        freqs[idxMax] += uint(errThr);
    }
    else {
        delta -= errThr;
        freqs[idxMax] -= uint(errThr);
    }

    // Slow path: spread error across frequencies
    const int inc = (delta < 0) ? 1 : -1;
    delta = abs(delta);
    int round = 0;

    while ((++round < 6) && (delta > 0)) {
        int adjustments = 0;

        for (int i = 0; i < alphabetSize; i++) {
            const int idx = alphabet[i];

            // Skip small frequencies to avoid big distortion
            // Do not zero out frequencies
            if (freqs[idx] <= 2)
                continue;

            // Adjust frequency
            freqs[idx] += inc;
            adjustments++;
            delta--;

            if (delta == 0)
                break;
        }

        if (adjustments == 0)
           break;
    }

    freqs[idxMax] = max(freqs[idxMax] - delta, uint(1));
    return alphabetSize;
}

int EntropyUtils::writeVarInt(OutputBitStream& obs, uint32 value)
{
    uint32 res = 0;

    while (value >= 128) {
        obs.writeBits(0x80 | (value & 0x7F), 8);
        value >>= 7;
        res++;
    }

    obs.writeBits(value, 8);
    return res;
}

uint32 EntropyUtils::readVarInt(InputBitStream& ibs)
{
    uint32 value = uint32(ibs.readBits(8));
    uint32 res = value & 0x7F;

    for (int shift = 7; value >= 128; shift += 7) {
        value = uint32(ibs.readBits(8));

        if (shift == 28) {
            // uint32 varint: last byte may only carry 4 payload bits and
            // must terminate the sequence.
            if ((value >= 128) || ((value & 0x70) != 0)) {
                throw BitStreamException("Invalid variable-length integer in bitstream",
                    BitStreamException::INVALID_STREAM);
            }

            res |= ((value & 0x0F) << shift);
            return res;
        }

        res |= ((value & 0x7F) << shift);
    }

    return res;
}

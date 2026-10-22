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
#include "../BitStreamException.hpp"
#include "RangeDecoder.hpp"
#include "EntropyUtils.hpp"

using namespace kanzi;
using namespace std;


const int RangeDecoder::DECODING_BATCH_SIZE = 12; // in bits
const int RangeDecoder::DECODING_MASK = (1 << DECODING_BATCH_SIZE) - 1;
const uint64 RangeDecoder::TOP_RANGE    = 0x0FFFFFFFFFFFFFFF;
const uint64 RangeDecoder::BOTTOM_RANGE = 0x000000000000FFFF;
const uint64 RangeDecoder::RANGE_MASK   = 0x0FFFFFFF00000000;
const int RangeDecoder::DEFAULT_CHUNK_SIZE = 1 << 15; // 32 KB by default
const int RangeDecoder::DEFAULT_LOG_RANGE = 12;
const int RangeDecoder::MAX_CHUNK_SIZE = 1 << 30;



// The chunk size indicates how many bytes are encoded (per block) before
// resetting the frequency stats.
RangeDecoder::RangeDecoder(InputBitStream& bitstream, int chunkSize) : _bitstream(bitstream)
{
    if (chunkSize < 1024)
        throw invalid_argument("The chunk size must be at least 1024");

    if (chunkSize > MAX_CHUNK_SIZE)
        throw invalid_argument("The chunk size must be at most 2^30");

    _f2s = nullptr;
    _chunkSize = chunkSize;
    reset();
}

bool RangeDecoder::reset()
{
    _low = 0;
    _range = TOP_RANGE;
    _code = 0;
    _lenF2S = 0;
    _shift = 0;
    memset(_alphabet, 0, sizeof(uint) * 256);
    memset(_freqs, 0, sizeof(uint) * 256);
    memset(_cumFreqs, 0, sizeof(uint64) * 257);
    return true;
}

int RangeDecoder::decodeHeader(uint frequencies[])
{
    int alphabetSize = EntropyUtils::decodeAlphabet(_bitstream, _alphabet);

    if (alphabetSize == 0)
        return 0;

    if (alphabetSize != 256) {
        memset(frequencies, 0, sizeof(uint) * 256);
    }

    const uint logRange = uint(8 + _bitstream.readBits(3));
    const int scale = 1 << logRange;
    _shift = logRange;
    int sum = 0;
    const int chkSize = (alphabetSize >= 64) ? 8 : 6;
    int llr = 3;

    while (uint(1 << llr) <= logRange)
        llr++;

    // Decode all frequencies (but the first one) by chunks of size 'inc'
    for (int i = 1; i < alphabetSize; i += chkSize) {
        const int logMax = int(_bitstream.readBits(llr));

        if ((1 << logMax) > scale) {
            stringstream ss;
            ss << "Invalid bitstream: incorrect frequency size ";
            ss << logMax << " in ANS range decoder";
            throw BitStreamException(ss.str(), BitStreamException::INVALID_STREAM);
        }

        const int endj = min(i + chkSize, alphabetSize);

        // Read frequencies
        for (int j = i; j < endj; j++) {
            const int freq = (logMax == 0) ? 1 : int(_bitstream.readBits(logMax) + 1);

            if ((freq <= 0) || (freq >= scale)) {
                stringstream ss;
                ss << "Invalid bitstream: incorrect frequency " << freq;
                ss << " for symbol '" << _alphabet[j] << "' in range decoder";
                throw BitStreamException(ss.str(),
                    BitStreamException::INVALID_STREAM);
            }

            frequencies[_alphabet[j]] = uint(freq);
            sum += freq;
        }
    }

    // Infer first frequency
    if (scale <= sum) {
        stringstream ss;
        ss << "Invalid bitstream: incorrect frequency " << frequencies[_alphabet[0]];
        ss << " for symbol '" << _alphabet[0] << "' in range decoder";
        throw BitStreamException(ss.str(),
            BitStreamException::INVALID_STREAM);
    }

    frequencies[_alphabet[0]] = uint(scale - sum);
    _cumFreqs[0] = 0;

    if (_lenF2S < scale) {
        short* f2s = new short[scale];
        delete[] _f2s;
        _f2s = f2s;
        _lenF2S = scale;
    }

    // Create histogram of frequencies scaled to 'range' and reverse mapping
    for (int i = 0; i < 256; i++) {
        _cumFreqs[i + 1] = _cumFreqs[i] + frequencies[i];
        const int base = int(_cumFreqs[i]);

        for (int j = frequencies[i] - 1; j >= 0; j--)
            _f2s[base + j] = short(i);
    }

    return alphabetSize;
}

// Initialize once (if necessary) at the beginning, the use the faster decodeByte_()
// Reset frequency stats for each chunk of data in the block
int RangeDecoder::decode(kanzi::byte block[], uint blkptr, uint count)
{
    if (count == 0)
        return 0;

    const uint end = blkptr + count;
    const uint sz = _chunkSize;
    uint startChunk = blkptr;

    while (startChunk < end) {
        const uint endChunk = min(startChunk + sz, end);
        const int alphabetSize = decodeHeader(_freqs);

        if (alphabetSize == 0)
            return startChunk - blkptr;

        if (alphabetSize == 1) {
            // Shortcut for chunks with only one symbol
            memset(&block[startChunk], _alphabet[0], size_t(endChunk - startChunk));
            startChunk = endChunk;
            continue;
        }

        _range = TOP_RANGE;
        _low = 0;
        _code = _bitstream.readBits(60);

        for (uint i = startChunk; i < endChunk; i++)
            block[i] = decodeByte();

        startChunk = endChunk;
    }

    return count;
}

kanzi::byte RangeDecoder::decodeByte()
{
    // Compute next low and range
    _range >>= _shift;
    if (_range == 0) {
        throw BitStreamException("Invalid bitstream: incorrect range in range decoder",
            BitStreamException::INVALID_STREAM);
    }

    const uint64 cum = (_code - _low) / _range;

    if (cum >= (uint64(1) << _shift)) {
        throw BitStreamException("Invalid bitstream: incorrect cumulative frequency in range decoder",
            BitStreamException::INVALID_STREAM);
    }

    const int symbol = _f2s[int(cum)];
    const uint64 cumFreq = _cumFreqs[symbol];
    const uint64 freq = _cumFreqs[symbol + 1] - cumFreq;
    _low += (cumFreq * _range);
    _range *= freq;

    // If the left-most digits are the same throughout the range, read bits from bitstream
    while (true) {
        if (((_low ^ (_low + _range)) & RANGE_MASK) != 0) {
            if (_range > BOTTOM_RANGE)
                  break;

            // Normalize
            _range = ~(_low-1) & BOTTOM_RANGE;
        }

        _code = (_code << 28) | _bitstream.readBits(28);
        _range <<= 28;
        _low <<= 28;
    }

    return kanzi::byte(symbol);
}

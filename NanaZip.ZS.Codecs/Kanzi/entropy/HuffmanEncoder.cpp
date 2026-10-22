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
#include <sstream>

#include "HuffmanEncoder.hpp"
#include "EntropyUtils.hpp"
#include "ExpGolombEncoder.hpp"
#include "../Global.hpp"
#include "../Memory.hpp"

using namespace kanzi;
using namespace std;

// The chunk size indicates how many bytes are encoded (per block) before
// resetting the frequency stats. 0 means that frequencies calculated at the
// beginning of the block apply to the whole block.
HuffmanEncoder::HuffmanEncoder(OutputBitStream& bitstream, int chunkSize) : _bitstream(bitstream)
{
    if (chunkSize < 1024)
        throw invalid_argument("Huffman codec: The chunk size must be at least 1024");

    if (chunkSize > HuffmanCommon::MAX_CHUNK_SIZE) {
        stringstream ss;
        ss << "Huffman codec: The chunk size must be at most " << HuffmanCommon::MAX_CHUNK_SIZE;
        throw invalid_argument(ss.str());
    }

    _chunkSize = chunkSize;
    _buffer = nullptr;
    _bufferSize = 0;
    reset();
}

bool HuffmanEncoder::reset()
{
    for (uint16 i = 0; i < 256; i++)
        _codes[i] = i;

    return true;
}

// Rebuild Huffman codes
int HuffmanEncoder::updateFrequencies(uint freqs[])
{
    int count = 0;
    uint16 sizes[256] = { 0 };
    uint alphabet[256] = { 0 };

    for (int i = 0; i < 256; i++) {
        _codes[i] = 0;

        if (freqs[i] > 0)
            alphabet[count++] = i;
    }

    EntropyUtils::encodeAlphabet(_bitstream, alphabet, 256, count);

    if (count == 0)
        return 0;

    if (count == 1) {
        _codes[alphabet[0]] = 1 << 12;
        sizes[alphabet[0]] = 1;
    }
    else {
        uint ranks[256]; // sorted ranks

        for (int i = 0; i < count; i++)
            ranks[i] = (freqs[alphabet[i]] << 8) | alphabet[i];

        int maxCodeLen = computeCodeLengths(sizes, ranks, count);

        if (maxCodeLen == 0)
            throw invalid_argument("Could not generate Huffman codes: invalid code length 0");

        if (maxCodeLen > HuffmanCommon::MAX_SYMBOL_SIZE) {
            maxCodeLen = limitCodeLengths(alphabet, freqs, sizes, ranks, count);

            if (maxCodeLen == 0)
                throw invalid_argument("Could not generate Huffman codes: invalid code length 0");
        }

        if (maxCodeLen > HuffmanCommon::MAX_SYMBOL_SIZE) {
            uint16 n = 0;

            for (int i = 0; i < count; i++) {
                _codes[alphabet[i]] = n;
                sizes[alphabet[i]] = 8;
                n++;
            }
        }
        else {
            HuffmanCommon::generateCanonicalCodes(sizes, _codes, ranks, count);
        }
    }

    // Transmit code lengths only, freqs and codes do not matter
    ExpGolombEncoder egenc(_bitstream, true);
    uint16 prevSize = 2;

    // Pack size and code (size <= MAX_SYMBOL_SIZE bits)
    // Unary encode the code length differences
    for (int i = 0; i < count; i++) {
        const int s = alphabet[i];
        _codes[s] |= uint16(sizes[s] << 12);
        egenc.encodeByte(kanzi::byte(sizes[s] - prevSize));
        prevSize = sizes[s];
    }

    return count;
}


int HuffmanEncoder::limitCodeLengths(const uint alphabet[], uint freqs[], uint16 sizes[], uint ranks[], int count) const
{
    int n = 0;
    int debt = 0;

    // Fold over-the-limit sizes, skip at-the-limit sizes => incur bit debt
    while (sizes[ranks[n]] >= HuffmanCommon::MAX_SYMBOL_SIZE) {
        debt += (sizes[ranks[n]] - HuffmanCommon::MAX_SYMBOL_SIZE);
        sizes[ranks[n]] = HuffmanCommon::MAX_SYMBOL_SIZE;
        n++;
    }

    if (debt == 0)
        return HuffmanCommon::MAX_SYMBOL_SIZE;

    // Check (up to) 6 levels; one fixed-size bucket per size delta.
    // The alphabet is capped at 256 symbols, so no allocation is needed here.
    uint8 v[6][256] = { { 0 } };
    uint16 vSize[6] = { 0 };
    uint16 vHead[6] = { 0 };

    while (n < count) {
        const int idx = HuffmanCommon::MAX_SYMBOL_SIZE - 1 - sizes[ranks[n]];

        if ((idx > 5) || (debt < (1 << idx)))
            break;

        v[idx][vSize[idx]++] = uint8(n);
        n++;
    }

    int idx = 5;

    // Repay bit debt in a "semi optimized" way
    while ((debt > 0) && (idx >= 0)) {
        if ((vHead[idx] >= vSize[idx]) || (debt < (1 << idx))) {
            idx--;
            continue;
        }

        // Access element at current head
        sizes[ranks[v[idx][vHead[idx]]]]++;
        debt -= (1 << idx);

        // Advance head
        vHead[idx]++;
    }

    idx = 0;

    // Adjust if necessary
    while ((debt > 0) && (idx < 6)) {
        if (vHead[idx] >= vSize[idx]) {
            idx++;
            continue;
        }

        sizes[ranks[v[idx][vHead[idx]]]]++;
        debt -= (1 << idx);
        vHead[idx]++;
    }

    if (debt > 0) {
        // Fallback to slow (more accurate) path if fast path failed to repay the debt
        uint alpha[256] = { 0 };
        uint f[256];
        uint totalFreq = 0;

        for (int i = 0; i < count; i++) {
            f[i] = freqs[alphabet[i]];
            totalFreq += f[i];
        }

        // Renormalize to a smaller scale
        EntropyUtils::normalizeFrequencies(f, alpha, count, totalFreq, HuffmanCommon::MAX_CHUNK_SIZE >> 3);

        for (int i = 0; i < count; i++) {
            freqs[alphabet[i]] = f[i];
            ranks[i] = (f[i] << 8) | alphabet[i];
        }

        return computeCodeLengths(sizes, ranks, count);
    }

    return HuffmanCommon::MAX_SYMBOL_SIZE;
}


// Called only when more than 1 symbol
int HuffmanEncoder::computeCodeLengths(uint16 sizes[], uint ranks[], int count) const
{
    // Sort ranks by increasing freqs (first key) and increasing value (second key)
    sort(ranks, ranks + count);
    uint freqs[256] = { 0 };
    bool valid = true;

    for (int i = 0; i < count; i++) {
        freqs[i] = ranks[i] >> 8;
        ranks[i] = ranks[i] & 0xFF;
        valid &= (freqs[i] != 0);
    }

    if (valid == false)
        return 0;

    // See [In-Place Calculation of Minimum-Redundancy Codes]
    // by Alistair Moffat & Jyrki Katajainen
    computeInPlaceSizesPhase1(freqs, count);
    const int maxCodeLen = computeInPlaceSizesPhase2(freqs, count);

    for (int i = 0; i < count; i++)
        sizes[ranks[i]] = uint16(freqs[i]);

    return maxCodeLen;
}

void HuffmanEncoder::computeInPlaceSizesPhase1(uint data[], int n)
{
    for (int s = 0, r = 0, t = 0; t < n - 1; t++) {
        uint sum = 0;

        for (int i = 0; i < 2; i++) {
            if ((s >= n) || ((r < t) && (data[r] < data[s]))) {
                sum += data[r];
                data[r] = t;
                r++;
                continue;
            }

            sum += data[s];

            if (s > t)
                data[s] = 0;

            s++;
        }

        data[t] = sum;
    }
}

// n must be at least 2
// return max symbol length
uint HuffmanEncoder::computeInPlaceSizesPhase2(uint data[], int n)
{
    if (n < 2)
        return 0;

    uint topLevel = n - 2; //root
    uint depth = 1;
    uint totalNodesAtLevel = 2;

    while (n > 0) {
        uint k = topLevel;

        while ((k != 0) && (data[k - 1] >= topLevel))
            k--;

        const int internalNodesAtLevel = topLevel - k;
        const int leavesAtLevel = totalNodesAtLevel - internalNodesAtLevel;

        for (int j = 0; j < leavesAtLevel; j++)
            data[--n] = depth;

        totalNodesAtLevel = internalNodesAtLevel << 1;
        topLevel = k;
        depth++;
    }

    return depth - 1;
}


// Dynamically compute the frequencies for every chunk of data in the block
int HuffmanEncoder::encode(const kanzi::byte block[], uint blkptr, uint count)
{
    if (count == 0)
        return 0;

    const uint sz = uint(_chunkSize);
    const uint minLenBuf = max(min(sz + (sz >> 3), 2 * count), uint(65536));

    if (_bufferSize < minLenBuf) {
        kanzi::byte* buffer = new kanzi::byte[minLenBuf];
        delete[] _buffer;
        _buffer = buffer;
        _bufferSize = minLenBuf;
    }

    uint startChunk = blkptr;
    const uint end = startChunk + count;

    while (startChunk < end) {
        // Update frequencies and rebuild Huffman codes
        const uint sizeChunk = min(uint(_chunkSize), end - startChunk);

        if (sizeChunk < 32) {
            // Special case for small chunks
            _bitstream.writeBits(&block[startChunk], 8 * sizeChunk);
        }
        else {
            uint freqs[256] = { 0 };
            Global::computeHistogram(&block[startChunk], sizeChunk, freqs);

            // Skip chunk if only one symbol
            if (updateFrequencies(freqs) > 1) {
                encodeChunk(&block[startChunk], sizeChunk);
            }
        }

        startChunk += sizeChunk;
    }

    return count;
}


// count is at least 32
void HuffmanEncoder::encodeChunk(const kanzi::byte block[], uint count)
{
    uint nbBits[4] = { 0 };
    const uint szFrag = count / 4;
    const uint szFrag4 = szFrag & ~3;
    const uint szBuf = _bufferSize / 4;

    // Encode chunk
    for (int j = 0; j < 4; j++) {
        const kanzi::byte* src = &block[j * szFrag];
        kanzi::byte* buf = &_buffer[j * szBuf];
        int idx = 0;
        int bits = 0; // number of accumulated bits
        uint64 state = 0;

        // Encode fragments sequentially
        for (uint i = 0; i < szFrag4; i += 4) {
            const uint16 code0 = _codes[int(src[i])];
            const uint16 codeLen0 = code0 >> 12;
            const uint16 code1 = _codes[int(src[i + 1])];
            const uint16 codeLen1 = code1 >> 12;
            const uint16 code2 = _codes[int(src[i + 2])];
            const uint16 codeLen2 = code2 >> 12;
            const uint16 code3 = _codes[int(src[i + 3])];
            const uint16 codeLen3 = code3 >> 12;

            const uint32 pack01 = (uint32(code0 & 0x0FFF) << codeLen1) | uint32(code1 & 0x0FFF);
            const uint32 pack23 = (uint32(code2 & 0x0FFF) << codeLen3) | uint32(code3 & 0x0FFF);
            state = (state << (codeLen0 + codeLen1)) | uint64(pack01);
            state = (state << (codeLen2 + codeLen3)) | uint64(pack23);

            bits += (codeLen0 + codeLen1 + codeLen2 + codeLen3);
            BigEndian::writeLong64(&buf[idx], state << (64 - bits)); // bits cannot be 0
            idx += (bits >> 3);
            bits &= 7;
        }

        // Fragment last bytes
        for (uint i = szFrag4; i < szFrag; i++) {
            const uint16 code = _codes[int(src[i])];
            const uint16 codeLen = code >> 12;
            state = (state << codeLen) | uint64(code & 0x0FFF);
            bits += codeLen;
        }

        nbBits[j] = (idx * 8) + bits;

        while (bits >= 8) {
            bits -= 8;
            buf[idx++] = kanzi::byte(state >> bits);
        }

        if (bits > 0)
            buf[idx++] = kanzi::byte(state << (8 - bits));
    }

    // Write chunk size in bits
    EntropyUtils::writeVarInt(_bitstream, nbBits[0]);
    EntropyUtils::writeVarInt(_bitstream, nbBits[1]);
    EntropyUtils::writeVarInt(_bitstream, nbBits[2]);
    EntropyUtils::writeVarInt(_bitstream, nbBits[3]);

    // Write compressed data to bitstream
    _bitstream.writeBits(&_buffer[0 * szBuf], nbBits[0]);
    _bitstream.writeBits(&_buffer[1 * szBuf], nbBits[1]);
    _bitstream.writeBits(&_buffer[2 * szBuf], nbBits[2]);
    _bitstream.writeBits(&_buffer[3 * szBuf], nbBits[3]);

    // Chunk last bytes
    const uint count4 = 4 * szFrag;

    for (uint i = count4; i < count; i++)
        _bitstream.writeBits(uint64(block[i]), 8);
}

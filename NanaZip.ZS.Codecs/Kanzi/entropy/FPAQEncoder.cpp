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
#include <stdexcept>
#include "FPAQEncoder.hpp"
#include "EntropyUtils.hpp"

using namespace kanzi;
using namespace std;

const uint64 FPAQEncoder::TOP = 0x00FFFFFFFFFFFFFF;
const uint64 FPAQEncoder::MASK_0_24 = 0x0000000000FFFFFF;
const uint64 FPAQEncoder::MASK_0_32 = 0x00000000FFFFFFFF;
const uint FPAQEncoder::DEFAULT_CHUNK_SIZE = 4 * 1024 * 1024;
const uint FPAQEncoder::MAX_BLOCK_SIZE = 1 << 30;
const int FPAQEncoder::PSCALE = 65536;


FPAQEncoder::FPAQEncoder(OutputBitStream& bitstream)
    : _bitstream(bitstream)
{
    reset();
}

FPAQEncoder::~FPAQEncoder()
{
    // Destructors must not replace an encoding error or terminate the process
    // while unwinding after a bitstream failure.
    try {
        _dispose();
    }
    catch (...) {
    }
}

bool FPAQEncoder::reset()
{
    _index = 0;
    _low = 0;
    _high = TOP;
    _disposed = false;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 256; j++)
            _probs[i][j] = PSCALE >> 1;
    }

    return true;
}

int FPAQEncoder::encode(const kanzi::byte block[], uint blkptr, uint count)
{
    if (count >= MAX_BLOCK_SIZE)
        throw invalid_argument("Invalid block size parameter (max is 1<<30)");

    uint startChunk = blkptr;
    const uint end = blkptr + count;
    const uint size = min(DEFAULT_CHUNK_SIZE, count);
    const uint extra = max(size >> 3, min(size, uint(1 << 16)));
    const uint bufSize = max(size + extra, uint(1024));

    if (_buf.size() < bufSize)
        _buf.resize(bufSize);

    // Split block into chunks, encode chunk and write bit array to bitstream
    while (startChunk < end) {
        const uint chunkSize = min(DEFAULT_CHUNK_SIZE, end - startChunk);

        _index = 0;
        const uint endChunk = startChunk + chunkSize;
        uint16* p = _probs[0];

        for (uint i = startChunk; i < endChunk; i++) {
            const int val = int(block[i]);
            const int bits = val + 256;
            encodeBit(val & 0x80, p[1]);
            encodeBit(val & 0x40, p[bits >> 7]);
            encodeBit(val & 0x20, p[bits >> 6]);
            encodeBit(val & 0x10, p[bits >> 5]);
            encodeBit(val & 0x08, p[bits >> 4]);
            encodeBit(val & 0x04, p[bits >> 3]);
            encodeBit(val & 0x02, p[bits >> 2]);
            encodeBit(val & 0x01, p[bits >> 1]);
            p = _probs[val >> 6];
        }

        EntropyUtils::writeVarInt(_bitstream, uint32(_index));
        _bitstream.writeBits(&_buf[0], 8 * _index);
        startChunk += chunkSize;

        if (startChunk < end)
            _bitstream.writeBits(_low | MASK_0_24, 56);
    }

    return count;
}

void FPAQEncoder::_dispose()
{
    if (_disposed == true)
        return;

    _disposed = true;
    _bitstream.writeBits(_low | MASK_0_24, 56);
}

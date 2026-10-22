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
#include "BinaryEntropyEncoder.hpp"
#include "../Memory.hpp"
#include "EntropyUtils.hpp"

using namespace kanzi;
using namespace std;

const uint64 BinaryEntropyEncoder::TOP = 0x00FFFFFFFFFFFFFF;
const uint64 BinaryEntropyEncoder::MASK_0_24 = 0x0000000000FFFFFF;
const uint64 BinaryEntropyEncoder::MASK_0_32 = 0x00000000FFFFFFFF;
const int BinaryEntropyEncoder::MAX_BLOCK_SIZE = 1 << 30;
const int BinaryEntropyEncoder::MAX_CHUNK_SIZE = 1 << 26;


BinaryEntropyEncoder::BinaryEntropyEncoder(OutputBitStream& bitstream, Predictor* predictor, bool deallocate)
    : _predictor(predictor)
    , _bitstream(bitstream)
    , _deallocate(deallocate)
    , _sba(nullptr, 0)
{
    if (predictor == nullptr)
        throw invalid_argument("Invalid null predictor parameter");

    _low = 0;
    _high = TOP;
    _disposed = false;
}

BinaryEntropyEncoder::~BinaryEntropyEncoder()
{
    // Destructors must not replace an encoding error or terminate the process
    // while unwinding after a bitstream failure.
    try {
        _dispose();
    }
    catch (...) {
    }

    if (_sba._array != nullptr)
        delete[] _sba._array;

    if (_deallocate)
        delete _predictor;
}

void BinaryEntropyEncoder::ensureCapacity(int required)
{
    if (required <= _sba._length)
        return;

    const int grownSize = _sba._length + max(_sba._length >> 2, 1 << 10);
    int newSize = max(required, max(grownSize, 1024));
    kanzi::byte* buf = new kanzi::byte[newSize];

    if ((_sba._array != nullptr) && (_sba._index > 0))
        memcpy(buf, _sba._array, size_t(_sba._index));

    delete[] _sba._array;
    _sba._array = buf;
    _sba._length = newSize;
}

int BinaryEntropyEncoder::encode(const kanzi::byte block[], uint blkptr, uint count)
{
    if (count >= MAX_BLOCK_SIZE)
        throw invalid_argument("Invalid block size parameter (max is 1<<30)");

    uint startChunk = blkptr;
    const uint end = blkptr + count;
    uint length = max(count, 64u);

    if (length >= MAX_CHUNK_SIZE) {
        // If the block is big (>=64MB), split the encoding to avoid allocating
        // too much memory.
        length = (length / 8 < MAX_CHUNK_SIZE) ? count >> 3 : count >> 4;
    }

    const uint extra = max(length >> 3, min(length, uint(1 << 16)));
    const uint64 bufSize = uint64(length) + uint64(extra);
    ensureCapacity(int(min(bufSize, uint64(0x7FFFFFFF))));

    // Split block into chunks, encode chunk and write bit array to bitstream
    while (startChunk < end) {
        const uint chunkSize = min(length, end - startChunk);
        const uint endChunk = startChunk + chunkSize;
        _sba._index = 0;

        for (uint i = startChunk; i < endChunk; i++) {
            encodeBit(int(block[i]) & 0x80, _predictor->get());
            encodeBit(int(block[i]) & 0x40, _predictor->get());
            encodeBit(int(block[i]) & 0x20, _predictor->get());
            encodeBit(int(block[i]) & 0x10, _predictor->get());
            encodeBit(int(block[i]) & 0x08, _predictor->get());
            encodeBit(int(block[i]) & 0x04, _predictor->get());
            encodeBit(int(block[i]) & 0x02, _predictor->get());
            encodeBit(int(block[i]) & 0x01, _predictor->get());
        }

        EntropyUtils::writeVarInt(_bitstream, uint32(_sba._index));
        _bitstream.writeBits(&_sba._array[0], 8 * _sba._index);
        startChunk = endChunk;

        if (startChunk < end)
            _bitstream.writeBits(_low | MASK_0_24, 56);
    }

    return count;
}

void BinaryEntropyEncoder::_dispose()
{
    if (_disposed == true)
        return;

    _disposed = true;
    _bitstream.writeBits(_low | MASK_0_24, 56);
}

void BinaryEntropyEncoder::flush()
{
    if (_sba._length - _sba._index < 4)
        ensureCapacity(_sba._index + 4);

    BigEndian::writeInt32(&_sba._array[_sba._index], int32(_high >> 24));
    _sba._index += 4;
    _low <<= 32;
    _high = (_high << 32) | MASK_0_32;
}

void BinaryEntropyEncoder::encodeByte(kanzi::byte val)
{
    encodeBit(int(val) & 0x80, _predictor->get());
    encodeBit(int(val) & 0x40, _predictor->get());
    encodeBit(int(val) & 0x20, _predictor->get());
    encodeBit(int(val) & 0x10, _predictor->get());
    encodeBit(int(val) & 0x08, _predictor->get());
    encodeBit(int(val) & 0x04, _predictor->get());
    encodeBit(int(val) & 0x02, _predictor->get());
    encodeBit(int(val) & 0x01, _predictor->get());
}

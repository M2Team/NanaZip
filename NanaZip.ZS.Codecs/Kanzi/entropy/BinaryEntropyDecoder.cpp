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
#include "BinaryEntropyDecoder.hpp"
#include "../BitStreamException.hpp"
#include "../Memory.hpp"
#include "EntropyUtils.hpp"

using namespace kanzi;
using namespace std;

const uint64 BinaryEntropyDecoder::TOP = 0x00FFFFFFFFFFFFFF;
const uint64 BinaryEntropyDecoder::MASK_0_56 = 0x00FFFFFFFFFFFFFF;
const uint64 BinaryEntropyDecoder::MASK_0_32 = 0x00000000FFFFFFFF;
const int BinaryEntropyDecoder::MAX_BLOCK_SIZE = 1 << 30;
const int BinaryEntropyDecoder::MAX_CHUNK_SIZE = 1 << 26;


BinaryEntropyDecoder::BinaryEntropyDecoder(InputBitStream& bitstream, Predictor* predictor, bool deallocate)
    : _predictor(predictor)
    , _bitstream(bitstream)
    , _deallocate(deallocate)
    , _sba(nullptr, 0)
{
    if (predictor == nullptr)
        throw invalid_argument("Invalid null predictor parameter");

    _low = 0;
    _high = TOP;
    _current = 0;
    _payloadEnd = 0;
}

BinaryEntropyDecoder::~BinaryEntropyDecoder()
{
    _dispose();

    if (_sba._array != nullptr)
        delete[] _sba._array;

    if (_deallocate)
        delete _predictor;
}

void BinaryEntropyDecoder::ensureCapacity(int required)
{
    if (required <= _sba._length)
        return;

    const int grownSize = _sba._length + max(_sba._length >> 2, 1 << 20);
    int newSize = max(required, max(grownSize, 1024));
    kanzi::byte* buf = new kanzi::byte[newSize];

    if ((_sba._array != nullptr) && (_sba._index > 0))
        memcpy(buf, _sba._array, size_t(_sba._index));

    delete[] _sba._array;
    _sba._array = buf;
    _sba._length = newSize;
}

int BinaryEntropyDecoder::decode(kanzi::byte block[], uint blkptr, uint count)
{
    if (count >= MAX_BLOCK_SIZE)
        throw invalid_argument("Invalid block size parameter (max is 1<<30)");

    uint startChunk = blkptr;
    const uint end = blkptr + count;
    uint length = max(count, 64u);

    if (length >= MAX_CHUNK_SIZE) {
        // If the block is big (>=64MB), split the decoding to avoid allocating
        // too much memory.
        length = (length / 8 < MAX_CHUNK_SIZE) ? count >> 3 : count >> 4;
    }

    const uint bufSize = length + (length >> 3);

    ensureCapacity(int(bufSize));

    // Split block into chunks, read bit array from bitstream and decode chunk
    while (startChunk < end) {
        const uint chunkSize = min(length, end - startChunk);
        const uint szBytes = uint(EntropyUtils::readVarInt(_bitstream));

        const uint64 maxEncodedSize = min(uint64(chunkSize) << 5, uint64(uint(-1) >> 3));

        if (uint64(szBytes) > maxEncodedSize)
           return 0;

        ensureCapacity(int(szBytes));

        _payloadEnd = szBytes;
        _current = _bitstream.readBits(56);

        if (szBytes != 0)
            _bitstream.readBits(&_sba._array[0], 8 * szBytes);

        _sba._index = 0;
        const uint endChunk = startChunk + chunkSize;

        for (uint i = startChunk; i < endChunk; i++) {
            block[i] = kanzi::byte((decodeBit(_predictor->get()) << 7)
                          | (decodeBit(_predictor->get()) << 6)
                          | (decodeBit(_predictor->get()) << 5)
                          | (decodeBit(_predictor->get()) << 4)
                          | (decodeBit(_predictor->get()) << 3)
                          | (decodeBit(_predictor->get()) << 2)
                          | (decodeBit(_predictor->get()) << 1)
                          |  decodeBit(_predictor->get()));
        }

        if (uint(_sba._index) != _payloadEnd)
            return 0;

        startChunk = endChunk;
    }

    return count;
}


// no inline
void BinaryEntropyDecoder::read()
{
    if (KANZI_UNLIKELY(uint(_sba._index) + 4 > _payloadEnd))
        throw BitStreamException("Invalid bitstream: binary entropy payload underrun",
            BitStreamException::INVALID_STREAM);

    _low = (_low << 32) & MASK_0_56;
    _high = ((_high << 32) | MASK_0_32) & MASK_0_56;
    const uint64 val = BigEndian::readInt32(&_sba._array[_sba._index]) & MASK_0_32;
    _current = ((_current << 32) | val) & MASK_0_56;
    _sba._index += 4;
}

// no inline
kanzi::byte BinaryEntropyDecoder::decodeByte()
{
    return kanzi::byte((decodeBit(_predictor->get()) << 7)
        | (decodeBit(_predictor->get()) << 6)
        | (decodeBit(_predictor->get()) << 5)
        | (decodeBit(_predictor->get()) << 4)
        | (decodeBit(_predictor->get()) << 3)
        | (decodeBit(_predictor->get()) << 2)
        | (decodeBit(_predictor->get()) << 1)
        |  decodeBit(_predictor->get()));
}

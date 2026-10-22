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

#include <cstring>
#include <stdexcept>
#include "BWTBlockCodec.hpp"
#include "../Global.hpp"

using namespace kanzi;


BWTBlockCodec::BWTBlockCodec(Context& ctx)
{
   _pBWT = new BWT(ctx);
   _bsVersion = ctx.getInt("bsVersion");
}

// Return true if the compression chain succeeded. In this case, the input data
// may be modified. If the compression failed, the input data is returned unmodified.
bool BWTBlockCodec::forward(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int blockSize)
{
    if (blockSize == 0)
        return true;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw std::invalid_argument("BWTBlockCodec: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw std::invalid_argument("BWTBlockCodec: Invalid output block");

    if (input._array == output._array)
        return false;

    if (output._length - output._index < getMaxEncodedLength(blockSize))
        return false;

    int logBlockSize = Global::_log2(uint32(blockSize));

    if ((blockSize & (blockSize - 1)) != 0)
       logBlockSize++;

    const int pIndexSize = (logBlockSize + 7) >> 3;

    if ((pIndexSize <= 0) || (pIndexSize >= 5))
       return false;

    const int chunks = BWT::getBWTChunks(blockSize);
    const int logNbChunks = Global::_log2(uint32(chunks));

    if (logNbChunks > 7)
        return false;

    kanzi::byte* dst = &output._array[output._index];
    output._index += (1 + chunks * pIndexSize);

    // Apply forward transform
    if (_pBWT->forward(input, output, blockSize) == false)
        return false;

    const kanzi::byte mode = kanzi::byte((logNbChunks << 2) | (pIndexSize - 1));

    // Emit header
    for (int i = 0, idx = 1; i < chunks; i++) {
        const int primaryIndex = _pBWT->getPrimaryIndex(i) - 1;
        int shift = (pIndexSize - 1) << 3;

        while (shift >= 0) {
            dst[idx++] = kanzi::byte(primaryIndex >> shift);
            shift -= 8;
        }
    }

    dst[0] = mode;
    return true;
}

bool BWTBlockCodec::inverse(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int blockSize)
{
    if (blockSize <= 1)
        return blockSize == 0;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw std::invalid_argument("BWTBlockCodec: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw std::invalid_argument("BWTBlockCodec: Invalid output block");

    if (input._array == output._array)
        return false;

    if ((blockSize < 1) || (input._length - input._index < 1))
        return false;

    if (_bsVersion > 5) {
       // Number of chunks and primary index size in bitstream since bsVersion 6
       kanzi::byte mode = input._array[input._index++];
       const uint logNbChunks = uint(mode >> 2) & 0x07;
       const int pIndexSize = (int(mode) & 0x03) + 1;
       const int chunks = 1 << logNbChunks;
       const int headerSize = 1 + chunks * pIndexSize;

       if ((input._length - input._index < headerSize) || (blockSize < headerSize))
           return false;

       if (chunks != BWT::getBWTChunks(blockSize - headerSize))
           return false;

       // Read header
       for (int i = 0; i < chunks; i++) {
           int shift = (pIndexSize - 1) << 3;
           uint primaryIndex = 0;

           // Extract BWT primary index
           while (shift >= 0) {
               primaryIndex = (primaryIndex << 8) | int(input._array[input._index++]);
               shift -= 8;
           }

           if (primaryIndex >= 0x7FFFFFFFU)
               return false;

           if (_pBWT->setPrimaryIndex(i, int(primaryIndex) + 1) == false)
               return false;
       }

       blockSize -= headerSize;
    }
    else {
       const int chunks = BWT::getBWTChunks(blockSize);

       for (int i = 0; i < chunks; i++) {
           // Read block header (mode + primary index)
           if (input._index >= input._length)
               return false;

           const int blockMode = int(input._array[input._index++]);
           const int pIndexSizeBytes = 1 + ((blockMode >> 6) & 0x03);

           if ((blockSize < pIndexSizeBytes) || (input._index + (pIndexSizeBytes - 1) > input._length))
               return false;

           blockSize -= pIndexSizeBytes;
           int shift = (pIndexSizeBytes - 1) << 3;
           int primaryIndex = (blockMode & 0x3F) << shift;

           // Extract BWT primary index
           for (int n = 1; n < pIndexSizeBytes; n++) {
               shift -= 8;
               primaryIndex |= (int(input._array[input._index++]) << shift);
           }

           if (_pBWT->setPrimaryIndex(i, primaryIndex) == false)
               return false;
       }
    }

    // Apply inverse Transform
    return _pBWT->inverse(input, output, blockSize);
}

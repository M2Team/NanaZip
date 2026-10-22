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
#include <stddef.h>
#include <stdexcept>

#include "../Global.hpp"
#include "../Memory.hpp"
#include "ZRLT.hpp"

using namespace kanzi;
using namespace std;

bool ZRLT::forward(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int length)
{
    if (length == 0)
        return true;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw invalid_argument("ZRLT: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw invalid_argument("ZRLT: Invalid output block");

    if (output._length - output._index < getMaxEncodedLength(length))
        return false;

    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];
    uint srcIdx = 0;
    uint dstIdx = 0;
    const uint srcEnd = uint(length);
    const uint dstEnd = uint(output._length - output._index);
    const uint srcEnd4 = (length >= 4) ? uint(length - 4) : uint(0);
    bool res = true;
    kanzi::byte zeros[4] = { kanzi::byte(0) };

    while (srcIdx < srcEnd) {
        if (src[srcIdx] == kanzi::byte(0)) {
            uint runLength = 1;

            while ((srcIdx + runLength < srcEnd4) && (KANZI_MEM_EQ4(&src[srcIdx + runLength], &zeros[0])))
                runLength += 4;

            while ((srcIdx + runLength < srcEnd) && src[srcIdx + runLength] == kanzi::byte(0))
                runLength++;

            srcIdx += runLength;

            // Encode length
            runLength++;
            uint log = uint(Global::_log2(uint32(runLength)));

            if (KANZI_UNLIKELY(log > dstEnd - dstIdx)) {
                res = false;
                break;
            }

            // Write every bit as a kanzi::byte except the most significant one
            while (log >= 4) {
                const uint32 w = (uint32(((runLength >> (log - 1)) & 1) << 24) |
                                  uint32(((runLength >> (log - 2)) & 1) << 16) |
                                  uint32(((runLength >> (log - 3)) & 1) << 8)  |
                                  uint32( (runLength >> (log - 4)) & 1));
                BigEndian::writeInt32(&dst[dstIdx], int32(w));
                dstIdx += 4;
                log -= 4;
            }

            while (log > 0) {
                log--;
                dst[dstIdx++] = kanzi::byte((runLength >> log) & 1);
            }

            continue;
        }

        const int val = int(src[srcIdx]);
        const uint needed = (val >= 0xFE) ? uint(2) : uint(1);

        if (KANZI_UNLIKELY(needed > dstEnd - dstIdx)) {
            res = false;
            break;
        }

        if (val >= 0xFE) {
            dst[dstIdx] = kanzi::byte(0xFF);
            dst[dstIdx + 1] = kanzi::byte(val - 0xFE);
            dstIdx++;
        }
        else {
            dst[dstIdx] = kanzi::byte(val + 1);
        }

        srcIdx++;
        dstIdx++;
    }

    input._index += srcIdx;
    output._index += dstIdx;
    return res && (srcIdx == srcEnd);
}

bool ZRLT::inverse(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int length)
{
    if (length < 0)
       return false;

    if (length == 0)
        return true;

    if (length > input._length - input._index)
        return false;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw invalid_argument("ZRLT: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw invalid_argument("ZRLT: Invalid output block");

    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];
    uint srcIdx = 0;
    uint dstIdx = 0;
    const uint srcEnd = uint(length);
    const uint dstEnd = uint(output._length - output._index);
    uint runLength = 0;

    while (true) {
        uint val = uint(src[srcIdx]);

        if (val <= 1) {
            // Generate the run length bit by bit (but force MSB)
            runLength = 1;

            do {
                runLength += (runLength + val);
                srcIdx++;

                if (srcIdx >= srcEnd)
                    goto End;

                val = uint(src[srcIdx]);
            }
            while (val <= 1);

            runLength--;

            if (runLength > 0) {
                if (KANZI_UNLIKELY(runLength >= dstEnd - dstIdx))
                    goto End;

                memset(&dst[dstIdx], 0, size_t(runLength));
                dstIdx += runLength;
                runLength = 0;
                continue;
            }
        }

        // Regular data processing
        if (KANZI_UNLIKELY(dstIdx >= dstEnd))
            return false;

        if (val != 0xFF) {
            const uint startIdx = srcIdx;

            while ((srcIdx + 4 <= srcEnd) && (dstIdx + 4 <= dstEnd)) {
                const uint32 word = uint32(LittleEndian::readInt32(&src[srcIdx]));
                // Detect bytes below 2 or equal to 0xFF before lane-wise subtraction.
                const uint32 invalid = (((word - 0x02020202U) & ~word) |
                                        ((~word - 0x01010101U) & word)) & 0x80808080U;

                if (invalid != 0)
                    break;

                LittleEndian::writeInt32(&dst[dstIdx], int32(word - 0x01010101U));
                srcIdx += 4;
                dstIdx += 4;
            }

            if (srcIdx != startIdx) {
                if (KANZI_UNLIKELY((srcIdx >= srcEnd) || (dstIdx >= dstEnd)))
                    break;

                continue;
            }
        }

        if (val == 0xFF) {
            srcIdx++;

            if (KANZI_UNLIKELY(srcIdx >= srcEnd))
                return false;

            dst[dstIdx] = kanzi::byte(0xFE + int(src[srcIdx]));
        }
        else {
            dst[dstIdx] = kanzi::byte(val - 1);
        }

        srcIdx++;
        dstIdx++;

        if (KANZI_UNLIKELY((srcIdx >= srcEnd) || (dstIdx >= dstEnd)))
            break;
    }

End:
    if (runLength > 0) {
        runLength--;

        // If runLength is not 1, add trailing 0s
        if (runLength > dstEnd - dstIdx)
            return false;

        if (runLength > 0) {
            memset(&dst[dstIdx], 0, size_t(runLength));
            dstIdx += runLength;
        }
    }

    input._index += srcIdx;
    output._index += dstIdx;
    return srcIdx == srcEnd;
}

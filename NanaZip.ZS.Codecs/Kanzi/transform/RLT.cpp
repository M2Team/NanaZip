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
#include <stdexcept>

#include "RLT.hpp"
#include "../Global.hpp"
#include "../Memory.hpp"
#include "../util/strings.hpp"


using namespace kanzi;
using namespace std;


const int RLT::RUN_LEN_ENCODE1 = 224; // used to encode run length
const int RLT::RUN_LEN_ENCODE2 = (255 - RUN_LEN_ENCODE1) << 8; // used to encode run length
const int RLT::RUN_THRESHOLD = 3;
const int RLT::MAX_RUN = 0xFFFF + RUN_LEN_ENCODE2 + RUN_THRESHOLD - 1;
const int RLT::MAX_RUN4 = MAX_RUN - 4;
const int RLT::MIN_BLOCK_LENGTH = 16;
const kanzi::byte RLT::DEFAULT_ESCAPE = kanzi::byte(0xFB);


bool RLT::forward(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if (count < MIN_BLOCK_LENGTH)
        return false;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw invalid_argument("RLT: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw invalid_argument("RLT: Invalid output block");

    if (output._length - output._index < getMaxEncodedLength(count))
        return false;

    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];
    Global::DataType dt = Global::UNDEFINED;
    bool findBestEscape = true;

    if (_pCtx != nullptr) {
        dt = (Global::DataType) _pCtx->getInt("dataType", Global::UNDEFINED);

        if ((dt == Global::DNA) || (dt == Global::BASE64) || (dt == Global::UTF8))
            return false;

        std::string entropyType = _pCtx->getString("entropy");
        transform(entropyType.begin(), entropyType.end(), entropyType.begin(), safeToUpper);

        // Fast track if fast entropy coder is used
        if ((entropyType == "NONE") || (entropyType == "ANS0") ||
            (entropyType == "HUFFMAN") || (entropyType == "RANGE"))
            findBestEscape = false;
    }

    kanzi::byte escape = DEFAULT_ESCAPE;

    if (findBestEscape == true) {
        uint freqs[256] = { 0 };
        Global::computeHistogram(&src[0], count, freqs);

        if (dt == Global::UNDEFINED) {
            dt = Global::detectSimpleType(count, freqs);

            if ((_pCtx != nullptr) && (dt != Global::UNDEFINED))
                _pCtx->putInt("dataType", dt);

            if ((dt == Global::DNA) || (dt == Global::BASE64) || (dt == Global::UTF8))
                return false;
        }

        int minIdx = 0;

        if (freqs[minIdx] > 0) {
            for (int i = 1; i < 256; i++) {
                if (freqs[i] < freqs[minIdx]) {
                    minIdx = i;

                    if (freqs[i] == 0)
                        break;
                }
            }
        }

        escape = kanzi::byte(minIdx);
    }

    int srcIdx = 0;
    int dstIdx = 0;
    const int srcEnd = count;
    const int srcEnd4 = srcEnd - 4;
    const int dstEnd = output._length;
    bool res = true;
    int run = 0;
    kanzi::byte prev = src[srcIdx++];
    dst[dstIdx++] = escape;
    dst[dstIdx++] = prev;

    if (prev == escape)
        dst[dstIdx++] = kanzi::byte(0);

    // Main loop
    while (true) {
        if (prev == src[srcIdx]) {
            const uint32 v = 0x01010101u * uint32(prev);
            const uint32 diff = uint32(LittleEndian::readInt32(&src[srcIdx])) ^ v;

            if (diff == 0) {
                srcIdx += 4; run += 4;

                if ((run < MAX_RUN4) && (srcIdx < srcEnd4))
                    continue;
            }
            else {
                const int n = Global::trailingZeros(diff) >> 3;
                srcIdx += n;
                run += n;
            }
        }

        if (run > RUN_THRESHOLD) {
            if (dstIdx + 6 >= dstEnd) {
                res = false;
                break;
            }

            dstIdx += emitRunLength(&dst[dstIdx], run, escape, prev);
        }
        else if (prev != escape) {
            if (dstIdx + run >= dstEnd) {
               res = false;
               break;
            }

            if (run-- > 0)
               dst[dstIdx++] = prev;

            while (run-- > 0)
               dst[dstIdx++] = prev;
        }
        else { // escape literal
            if (dstIdx + (2 * run) >= dstEnd) {
               res = false;
               break;
            }

            while (run-- > 0) {
               dst[dstIdx++] = escape;
               dst[dstIdx++] = kanzi::byte(0);
            }
        }

        prev = src[srcIdx];
        srcIdx++;
        run = 1;

        if (srcIdx >= srcEnd4)
            break;
    }

    if (res == true) {
        // run == 1
        if (prev != escape) {
            if (dstIdx + run < dstEnd) {
               while (run-- > 0)
                  dst[dstIdx++] = prev;
            }
        }
        else { // escape literal
            if (dstIdx + (2 * run) < dstEnd) {
               while (run-- > 0) {
                  dst[dstIdx++] = escape;
                  dst[dstIdx++] = kanzi::byte(0);
               }
            }
        }

        // Emit the last few bytes
        while ((srcIdx < srcEnd) && (dstIdx < dstEnd)) {
            if (src[srcIdx] == escape) {
               if (dstIdx + 2 >= dstEnd) {
                  res = false;
                  break;
               }

               dst[dstIdx++] = escape;
               dst[dstIdx++] = kanzi::byte(0);
               srcIdx++;
               continue;
            }

            dst[dstIdx++] = src[srcIdx++];
        }

        res &= (srcIdx == srcEnd);
    }

    input._index += srcIdx;
    output._index += dstIdx;
    return res && (dstIdx < srcIdx);
}

int RLT::emitRunLength(kanzi::byte dst[], int run, kanzi::byte escape, kanzi::byte val) {
    dst[0] = val;
    dst[1] = kanzi::byte(0);
    int dstIdx = (val == escape) ? 2 : 1;
    dst[dstIdx++] = escape;
    run -= RUN_THRESHOLD;

    // Encode run length
    if (run >= RUN_LEN_ENCODE1) {
        if (run < RUN_LEN_ENCODE2) {
            run -= RUN_LEN_ENCODE1;
            dst[dstIdx++] = kanzi::byte(RUN_LEN_ENCODE1 + (run >> 8));
        }
        else {
            run -= RUN_LEN_ENCODE2;
            dst[dstIdx++] = kanzi::byte(0xFF);
            dst[dstIdx++] = kanzi::byte(run >> 8);
        }
    }

    dst[dstIdx] = kanzi::byte(run);
    return dstIdx + 1;
}

bool RLT::inverse(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw invalid_argument("RLT: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw invalid_argument("RLT: Invalid output block");

    if (input._index + count > input._length)
        return false;

    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];
    int srcIdx = 0;
    int dstIdx = 0;
    const int srcEnd = srcIdx + count;
    const int dstEnd = output._length - output._index;
    bool res = true;
    const kanzi::byte escape = src[srcIdx++];

    if ((srcIdx < srcEnd) && (src[srcIdx] == escape)) {
        srcIdx++;

        // The data cannot start with a run but may start with an escape literal
        if ((srcIdx < srcEnd) && (src[srcIdx] != kanzi::byte(0)))
            return false;

        if (dstIdx >= dstEnd)
            return false;

        dst[dstIdx++] = escape;
        srcIdx++;
    }

    // Main loop
    while (srcIdx < srcEnd) {
        // Copy a span of literals in one operation. Escaped literals are
        // encoded as escape, 0, so the first escape always terminates the
        // current literal span.
        const byte* const esc = static_cast<const byte*>(
            std::memchr(&src[srcIdx], int(uint8(escape)), size_t(srcEnd - srcIdx)));
        const int literalLen = (esc == nullptr) ? (srcEnd - srcIdx) : int(esc - &src[srcIdx]);

        if (literalLen > 0) {
            if (literalLen > dstEnd - dstIdx) {
                res = false;
                break;
            }

            std::memcpy(&dst[dstIdx], &src[srcIdx], size_t(literalLen));
            srcIdx += literalLen;
            dstIdx += literalLen;
        }

        if (srcIdx >= srcEnd)
            break;

        // An escape marker was found
        srcIdx++;

        if (srcIdx >= srcEnd) {
            res = false;
            break;
        }

        int run = int(src[srcIdx++]);

        if (run == 0) {
            // Just an escape symbol, not a run
            if (dstIdx >= dstEnd) {
                  res = false;
                  break;
            }

            dst[dstIdx++] = escape;
            continue;
        }

        // Decode run length
        if (run == 0xFF) {
            if (srcIdx + 1 >= srcEnd) {
                  res = false;
                  break;
            }

            run = (int(src[srcIdx]) << 8) | int(src[srcIdx + 1]);
            srcIdx += 2;
            run += RUN_LEN_ENCODE2;
        }
        else if (run >= RUN_LEN_ENCODE1) {
            if (srcIdx >= srcEnd) {
                  res = false;
                  break;
            }

            run = ((run - RUN_LEN_ENCODE1) << 8) | int(src[srcIdx]);
            srcIdx++;
            run += RUN_LEN_ENCODE1;
        }

        run += (RUN_THRESHOLD - 1);

        if ((dstIdx + run > dstEnd) || (run > MAX_RUN)) {
            res = false;
            break;
        }

        if (dstIdx == 0) {
            res = false;
            break;
        }

        memset(&dst[dstIdx], int(dst[dstIdx - 1]), size_t(run));
        dstIdx += run;
    }

    input._index += srcIdx;
    output._index += dstIdx;
    return res && (srcIdx == srcEnd);
}

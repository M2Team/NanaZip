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

#include <stdexcept>

#include "FSDCodec.hpp"
#include "../Global.hpp"
#include "../Magic.hpp"
#include "../Memory.hpp"

using namespace kanzi;
using namespace std;


const int FSDCodec::MIN_LENGTH = 1024;
const kanzi::byte FSDCodec::ESCAPE_TOKEN = kanzi::byte(255);
const kanzi::byte FSDCodec::DELTA_CODING = kanzi::byte(0);
const kanzi::byte FSDCodec::XOR_CODING = kanzi::byte(1);

const int8 FSDCodec::ZIGZAG2[256] = {
             0,    -1,     1,    -2,     2,     -3,    3,    -4,
             4,    -5,     5,    -6,     6,     -7,    7,    -8,
             8,    -9,     9,   -10,    10,    -11,   11,   -12,
            12,   -13,    13,   -14,    14,    -15,   15,   -16,
            16,   -17,    17,   -18,    18,    -19,   19,   -20,
            20,   -21,    21,   -22,    22,    -23,   23,   -24,
            24,   -25,    25,   -26,    26,    -27,   27,   -28,
            28,   -29,    29,   -30,    30,    -31,   31,   -32,
            32,   -33,    33,   -34,    34,    -35,   35,   -36,
            36,   -37,    37,   -38,    38,    -39,   39,   -40,
            40,   -41,    41,   -42,    42,    -43,   43,   -44,
            44,   -45,    45,   -46,    46,    -47,   47,   -48,
            48,   -49,    49,   -50,    50,    -51,   51,   -52,
            52,   -53,    53,   -54,    54,    -55,   55,   -56,
            56,   -57,    57,   -58,    58,    -59,   59,   -60,
            60,   -61,    61,   -62,    62,    -63,   63,   -64,
            64,   -65,    65,   -66,    66,    -67,   67,   -68,
            68,   -69,    69,   -70,    70,    -71,   71,   -72,
            72,   -73,    73,   -74,    74,    -75,   75,   -76,
            76,   -77,    77,   -78,    78,    -79,   79,   -80,
            80,   -81,    81,   -82,    82,    -83,   83,   -84,
            84,   -85,    85,   -86,    86,    -87,   87,   -88,
            88,   -89,    89,   -90,    90,    -91,   91,   -92,
            92,   -93,    93,   -94,    94,    -95,   95,   -96,
            96,   -97,    97,   -98,    98,    -99,   99,  -100,
           100,  -101,   101,  -102,   102,   -103,  103,  -104,
           104,  -105,   105,  -106,   106,   -107,  107,  -108,
           108,  -109,   109,  -110,   110,   -111,  111,  -112,
           112,  -113,   113,  -114,   114,   -115,  115,  -116,
           116,  -117,   117,  -118,   118,   -119,  119,  -120,
           120,  -121,   121,  -122,   122,   -123,  123,  -124,
           124,  -125,   125,  -126,   126,   -127,  127,  -128,
};

bool FSDCodec::forward(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw invalid_argument("FSD codec: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw invalid_argument("FSD codec: Invalid output block");

    if (input._array == output._array)
        return false;

    if (output._length - output._index < getMaxEncodedLength(count))
        return false;

    // If too small, skip
    if (count < MIN_LENGTH)
        return false;

    if (_pCtx != nullptr) {
        Global::DataType dt = (Global::DataType) _pCtx->getInt("dataType", Global::UNDEFINED);

        if ((dt != Global::UNDEFINED) && (dt != Global::MULTIMEDIA) && (dt != Global::BIN))
            return false;
    }

    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];
    uint magic = Magic::getType(src);

    // Skip detection except for a few candidate types
    switch (magic) {
        case Magic::BMP_MAGIC:
        case Magic::RIFF_MAGIC:
        case Magic::PBM_MAGIC:
        case Magic::PGM_MAGIC:
        case Magic::PPM_MAGIC:
        case Magic::NO_MAGIC:
           break;
        default:
           return false;
    }

    const int srcEnd = count;
    const int count10 = count / 10;
    const int count5 = 2 * count10; // count5=count/5 does not guarantee count5=2*count10 !
    uint histo[7][256];
    memset(&histo[0][0], 0, sizeof(histo));

    // Check several step values on a few sub-blocks (no memory allocation)
    const kanzi::byte* in0 = &src[count5 * 0];
    const kanzi::byte* in1 = &src[count5 * 2];
    const kanzi::byte* in2 = &src[count5 * 4];

    for (int i = count10; i < count5; i++) {
        const kanzi::byte b0 = in0[i];
        histo[0][int(b0)]++;
        histo[1][int(b0 ^ in0[i - 1])]++;
        histo[2][int(b0 ^ in0[i - 2])]++;
        histo[3][int(b0 ^ in0[i - 3])]++;
        histo[4][int(b0 ^ in0[i - 4])]++;
        histo[5][int(b0 ^ in0[i - 8])]++;
        histo[6][int(b0 ^ in0[i - 16])]++;
        const kanzi::byte b1 = in1[i];
        histo[0][int(b1)]++;
        histo[1][int(b1 ^ in1[i - 1])]++;
        histo[2][int(b1 ^ in1[i - 2])]++;
        histo[3][int(b1 ^ in1[i - 3])]++;
        histo[4][int(b1 ^ in1[i - 4])]++;
        histo[5][int(b1 ^ in1[i - 8])]++;
        histo[6][int(b1 ^ in1[i - 16])]++;
        const kanzi::byte b2 = in2[i];
        histo[0][int(b2)]++;
        histo[1][int(b2 ^ in2[i - 1])]++;
        histo[2][int(b2 ^ in2[i - 2])]++;
        histo[3][int(b2 ^ in2[i - 3])]++;
        histo[4][int(b2 ^ in2[i - 4])]++;
        histo[5][int(b2 ^ in2[i - 8])]++;
        histo[6][int(b2 ^ in2[i - 16])]++;
    }

    // Find if entropy is lower post transform
    int minIdx = 0;
    int ent[7];

    for (int i = 0; i < 7; i++) {
        ent[i] = Global::computeOrder0Entropy1024(3 * count10, histo[i]);

        if (ent[i] < ent[minIdx])
            minIdx = i;
    }

    // If not better, quick exit
    if (ent[minIdx] >= ent[0]) {
        if (_pCtx != nullptr)
            _pCtx->putInt("dataType", Global::detectSimpleType(3 * count10, histo[0]));

        return false;
    }

    if (_pCtx != nullptr)
       _pCtx->putInt("dataType", Global::MULTIMEDIA);

    const int distances[7] = { 0, 1, 2, 3, 4, 8, 16 };
    const int dist = distances[minIdx];
    const kanzi::byte coding = DELTA_CODING;

    // Keep triplet-correlated data interleaved since phase bucketing can
    // disrupt downstream matches for this layout.
    const bool bucketed = (dist > 1) && (dist != 3) && (dist != 16);
    const kanzi::byte mode = kanzi::byte(int(coding) | (bucketed == true ? 2 : 0));
    dst[0] = mode;
    dst[1] = kanzi::byte(dist);
    int srcIdx = 0;
    int dstIdx = 2;

    // Emit first bytes
    for (int i = 0; i < dist; i++)
        dst[dstIdx++] = src[srcIdx++];

    // Emit modified bytes. The bucketed layout keeps each phase together.
    if (bucketed == true) {
        const int bucketLength = 1 << 15;
        const int tileLength = dist * bucketLength;

        for (int tileStart = 0; tileStart < srcEnd; tileStart += tileLength) {
            const int tileEnd = min(tileStart + tileLength, srcEnd);

            for (int lane = 0; lane < dist; lane++) {
                const int firstPos = tileStart + lane + ((tileStart == 0) ? dist : 0);

                for (int pos = firstPos; pos < tileEnd; pos += dist) {
                    const uint residual = uint(uint8(int(src[pos]) - int(src[pos - dist])));
                    const uint zigzag = (residual & 0x80) ? ((256 - residual) << 1) - 1 : residual << 1;
                    dst[dstIdx++] = kanzi::byte(zigzag);
                }
            }
        }

        srcIdx = srcEnd;
    }
    else {
        while (srcIdx < srcEnd) {
            // Encode the delta modulo 256. The signed difference is not
            // needed to reconstruct a byte, and all 256 residuals fit in
            // one byte. Values in [-127..127] retain the previous zigzag
            // mapping; -128 and +128 share the same modular residual.
            const uint residual = uint(uint8(int(src[srcIdx]) - int(src[srcIdx - dist])));
            const uint zigzag = (residual & 0x80) ? ((256 - residual) << 1) - 1 : residual << 1;
            dst[dstIdx++] = kanzi::byte(zigzag);
            srcIdx++;
        }
    }

    if (srcIdx != srcEnd)
        return false;

    // Extra check that the transform makes sense
    memset(&histo[0][0], 0, sizeof(uint) * 256);
    const kanzi::byte* out1 = &dst[count5 * 1];
    const kanzi::byte* out2 = &dst[count5 * 3];

    for (int i = 0; i < count10; i++) {
        histo[0][int(out1[i])]++;
        histo[0][int(out2[i])]++;
    }

    const int entropy = Global::computeOrder0Entropy1024(count5, histo[0]);

    if (entropy >= ent[0])
        return false;

    input._index += srcIdx;
    output._index += dstIdx;
    return true; // Allowed to expand
}

bool FSDCodec::inverse(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    const int bsVersion = (_pCtx == nullptr) ? 7 : _pCtx->getInt("bsVersion", 7);

    if (count == 0)
        return true;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw invalid_argument("FSD codec: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw invalid_argument("FSD codec: Invalid output block");

    if (input._array == output._array)
        return false;

    if (count < 4)
        return false;

    if (input._index + count > input._length)
        return false;

    const int srcEnd = count;
    const int dstEnd = output._length - output._index;
    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];

    // Retrieve mode & step value
    const kanzi::byte mode = src[0];
    const int modeValue = int(mode);
    const kanzi::byte coding = kanzi::byte(modeValue & 1);
    const bool bucketed = ((modeValue & 2) != 0);
    const int dist = int(src[1]);

    // Sanity check
    if ((dist < 1) || ((dist > 4) && (dist != 8) && (dist != 16)))
        return false;

    if ((bsVersion >= 7) && ((modeValue & ~3) != 0))
        return false;

    const int dataLength = count - 2;

    if ((count < dist + 2) || (dist > dstEnd) ||
        (bucketed && (dataLength > dstEnd)))
        return false;

    // Emit first bytes
    memcpy(&dst[0], &src[2], size_t(dist));
    int srcIdx = dist + 2;
    int dstIdx = dist;

    // Recover original bytes
    if (bsVersion < 7) {
        if (mode == DELTA_CODING) {
            while ((srcIdx < srcEnd) && (dstIdx < dstEnd)) {
                if (src[srcIdx] != ESCAPE_TOKEN) {
                    const int value = int(src[srcIdx]);
                    const int delta = (value >> 1) ^ -(value & 1);
                    dst[dstIdx] = kanzi::byte(int(dst[dstIdx - dist]) + delta);
                    srcIdx++;
                    dstIdx++;
                    continue;
                }

                srcIdx++;

                if (srcIdx == srcEnd)
                    return false;

                dst[dstIdx] = src[srcIdx] ^ dst[dstIdx - dist];
                srcIdx++;
                dstIdx++;
            }
        }
        else if (mode == XOR_CODING) {
            if (dist == 16) {
                while ((srcIdx + 16 <= srcEnd) && (dstIdx + 16 <= dstEnd)) {
                    KANZI_MEM_XOR16(&dst[dstIdx], &src[srcIdx], &dst[dstIdx - 16]);
                    srcIdx += 16;
                    dstIdx += 16;
                }
            }
            else if (dist == 8) {
                while ((srcIdx + 8 <= srcEnd) && (dstIdx + 8 <= dstEnd)) {
                    KANZI_MEM_XOR8(&dst[dstIdx], &src[srcIdx], &dst[dstIdx - 8]);
                    srcIdx += 8;
                    dstIdx += 8;
                }
            }

            while ((srcIdx < srcEnd) && (dstIdx < dstEnd)) {
                dst[dstIdx] = src[srcIdx] ^ dst[dstIdx - dist];
                srcIdx++;
                dstIdx++;
            }
        }
        else {
            // Invalid mode
            return false;
        }
    }
    else if (bucketed) {
        const int bucketLength = 1 << 15;
        const int tileLength = dist * bucketLength;

        for (int tileStart = 0; tileStart < dataLength; tileStart += tileLength) {
            const int tileEnd = min(tileStart + tileLength, dataLength);

            for (int lane = 0; lane < dist; lane++) {
                const int firstPos = tileStart + lane + ((tileStart == 0) ? dist : 0);

                for (int pos = firstPos; pos < tileEnd; pos += dist) {
                    if (srcIdx >= srcEnd)
                        return false;

                    if (coding == DELTA_CODING) {
                        const uint value = uint(uint8(src[srcIdx++]));
                        dst[pos] = kanzi::byte(int(dst[pos - dist]) + int(ZIGZAG2[value]));
                    }
                    else {
                        dst[pos] = src[srcIdx++] ^ dst[pos - dist];
                    }
                }
            }
        }

        dstIdx = dataLength;
    }
    else if (coding == DELTA_CODING) {
        while ((srcIdx < srcEnd) && (dstIdx < dstEnd)) {
            const uint value = uint(uint8(src[srcIdx++]));
            const int delta = int(ZIGZAG2[value]);
            dst[dstIdx] = kanzi::byte(int(dst[dstIdx - dist]) + delta);
            dstIdx++;
        }
    }
    else if (coding == XOR_CODING) {
        if (dist == 16) {
            while ((srcIdx + 16 <= srcEnd) && (dstIdx + 16 <= dstEnd)) {
                KANZI_MEM_XOR16(&dst[dstIdx], &src[srcIdx], &dst[dstIdx - 16]);
                srcIdx += 16;
                dstIdx += 16;
            }
        }
        else if (dist == 8) {
            while ((srcIdx + 8 <= srcEnd) && (dstIdx + 8 <= dstEnd)) {
                KANZI_MEM_XOR8(&dst[dstIdx], &src[srcIdx], &dst[dstIdx - 8]);
                srcIdx += 8;
                dstIdx += 8;
            }
        }

        while ((srcIdx < srcEnd) && (dstIdx < dstEnd)) {
            dst[dstIdx] = src[srcIdx] ^ dst[dstIdx - dist];
            srcIdx++;
            dstIdx++;
        }
    }
    else {
        // Invalid mode
        return false;
    }

    input._index += srcIdx;
    output._index += dstIdx;
    return srcIdx == srcEnd;
}

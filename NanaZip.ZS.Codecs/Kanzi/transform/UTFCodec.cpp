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
#include <vector>

#include "UTFCodec.hpp"
#include "../Global.hpp"
#include "../types.hpp"

using namespace kanzi;
using namespace std;

const int UTFCodec::MIN_BLOCK_SIZE = 1024;
const int UTFCodec::LEN_SEQ[256] = {
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
   3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
   4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

bool UTFCodec::forward(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if (count < MIN_BLOCK_SIZE)
        return false;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw invalid_argument("UTFCodec: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw invalid_argument("UTFCodec: Invalid output block");

    if (output._length - output._index < getMaxEncodedLength(count))
        return false;

    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];
    bool mustValidate = true;

    if (_pCtx != nullptr) {
        Global::DataType dt = (Global::DataType)_pCtx->getInt("dataType", Global::UNDEFINED);

        if ((dt != Global::UNDEFINED) && (dt != Global::UTF8))
            return false;

        mustValidate = dt != Global::UTF8;
    }

    int start = 0;

    if ((count >= 3) && (src[0] == kanzi::byte(0xEF)) && (src[1] == kanzi::byte(0xBB)) && (src[2] == kanzi::byte(0xBF))) {
        // Byte Order Mark (BOM)
        start = 3;
    }
    else {
        // First (possibly) invalid symbols (due to block truncation).
        while ((start < 4) && (LEN_SEQ[uint8(src[start])] == 0))
            start++;
    }

    if ((mustValidate == true) && (validate(&src[start], count - start - 4)) == false)
        return false;

    if (_pCtx != nullptr)
        _pCtx->putInt("dataType", Global::UTF8);

    // 1-3 bit size + (7 or 11 or 16 or 21) bit payload
    // 3 MSBs indicate symbol size (limit map size to 22 bits)
    // 000 -> 7 bits
    // 001 -> 11 bits
    // 010 -> 16 bits
    // 1xx -> 21 bits
    if (_aliasMap == nullptr)
        _aliasMap = new uint32[1 << 22];

    memset(_aliasMap, 0, size_t(1 << 22) * sizeof(uint32));
    vector<sdUTF> v;
    v.reserve(max(count >> 9, 256));
    int n = 0;
    bool res = true;

    for (int i = start; i < (count - 4); ) {
        uint32 val;
        const int s = pack(&src[i], val);
        res = s != 0;

        // Validation of longer sequences
        // Third kanzi::byte in [0x80..0xBF]
        res &= ((s != 3) || ((src[i + 2] & kanzi::byte(0xC0)) == kanzi::byte(0x80)));
        // Third and fourth bytes in [0x80..0xBF]
        res &= ((s != 4) || ((((uint16(src[i + 2]) << 8) | uint16(src[i + 3])) & 0xC0C0) == 0x8080));

        // Add to map ?
        if (_aliasMap[val] == 0) {
            n++;
            res &= (n < 32768);

#if __cplusplus >= 201103L
            v.emplace_back(val, 0);
#else
            sdUTF u(val, 0);
            v.push_back(u);
#endif
        }

        if (res == false)
           break;

        _aliasMap[val]++;
        i += s;
    }

    const int maxTarget = count - (count / 10);

    if ((res == false) || (n == 0) || ((3 * n + 6) >= maxTarget)) {
        return false;
    }

    for (int i = 0; i < n; i++)
        v[i].freq = _aliasMap[v[i].val];

    // Sort ranks by decreasing frequencies;
    sort(v.begin(), v.end());

    int dstIdx = 2;

    // Emit map length then map data
    dst[dstIdx++] = kanzi::byte(n >> 8);
    dst[dstIdx++] = kanzi::byte(n);

    int estimate = dstIdx + 6;

    for (int i = 0; i < n; i++) {
        estimate += int((i < 128) ? v[i].freq : 2 * v[i].freq);
        const uint32 s = v[i].val;
        _aliasMap[s] = (i < 128) ? i : 0x10080 | ((i << 1) & 0xFF00) | (i & 0x7F);
        dst[dstIdx] = kanzi::byte(s >> 16);
        dst[dstIdx + 1] = kanzi::byte(s >> 8);
        dst[dstIdx + 2] = kanzi::byte(s);
        dstIdx += 3;
    }

    if (estimate >= maxTarget) {
        // Not worth it
        return false;
    }

    // Emit first (possibly invalid) symbols (due to block truncation)
    for (int i = 0; i < start; i++)
        dst[dstIdx++] = src[i];

    v.clear();
    int srcIdx = start;

    // Emit aliases
    while (srcIdx < count - 4) {
        uint32 val;
        srcIdx += pack(&src[srcIdx], val);
        const uint32 alias = _aliasMap[val];
        dst[dstIdx++] = kanzi::byte(alias);
        dst[dstIdx] = kanzi::byte(alias >> 8);
        dstIdx += (alias >> 16);
    }

    dst[0] = kanzi::byte(start);
    dst[1] = kanzi::byte(srcIdx - (count - 4));

    // Emit last (possibly invalid) symbols (due to block truncation)
    while (srcIdx < count)
        dst[dstIdx++] = src[srcIdx++];

    input._index += srcIdx;
    output._index += dstIdx;
    return dstIdx < maxTarget;
}

bool UTFCodec::inverse(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if (count < 4)
        return false;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw invalid_argument("UTFCodec: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw invalid_argument("UTFCodec: Invalid output block");

    if (input._index + count > input._length)
        return false;

    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];
    const int start = int(src[0]) & 0x03;
    const int adjust = int(src[1]) & 0x03; // adjust end of regular processing
    const int n = (int(src[2]) << 8) + int(src[3]);

    // Protect against invalid map size value
    if ((n == 0) || (n >= 32768) || (3 * n > count - 4))
        return false;

    struct symb {
        uint32 val;
        uint8 len;
    };

    symb m[32768];
    int srcIdx = 4;

    // Build inverse mapping
    for (int i = 0; i < n; i++) {
        if (srcIdx + 3 > count)
            return false;

        int s = (uint32(src[srcIdx]) << 16) | (uint32(src[srcIdx + 1]) << 8) | uint32(src[srcIdx + 2]);
        const int sl = unpack(s, reinterpret_cast<kanzi::byte*>(&m[i].val));

        if (sl == 0)
            return false;

        m[i].len = uint8(sl);
        srcIdx += 3;
    }

    int dstIdx = 0;
    const int srcEnd = count - 4 + adjust;
    const int dstCap = output._length - output._index;
    const int dstEnd = dstCap - 4;

    if (dstEnd < 0)
        return false;

    if ((srcEnd > count) || (srcIdx + start > srcEnd) || (dstIdx + start > dstCap))
        return false;

    for (int i = 0; i < start; i++)
        dst[dstIdx++] = src[srcIdx++];

    // Emit data
    if (n <= 128) {
        // All valid aliases fit in one byte.
        while (srcIdx < srcEnd) {
            const uint alias = uint(src[srcIdx++]);

            if (KANZI_UNLIKELY(alias >= uint(n)))
                return false;

            const symb& s = m[alias];

            // The symbol length controls the logical output advance, but the
            // decoder always copies four bytes from the packed symbol value.
            if (KANZI_UNLIKELY(dstIdx + 4 > dstCap))
                return false;

            memcpy(&dst[dstIdx], &s.val, 4);
            dstIdx += s.len;
        }
    }
    else {
        // Decode the next alias and load its value before storing the current
        // one. This allows independent dictionary lookups to overlap.
        while (srcIdx < srcEnd) {
            uint alias = uint(src[srcIdx++]);

            if (alias >= 128) {
                if (KANZI_UNLIKELY(srcIdx >= srcEnd))
                    return false;

                alias = (uint(src[srcIdx++]) << 7) + (alias & 0x7F);
            }

            if (KANZI_UNLIKELY(alias >= uint(n)))
                return false;

            const uint32 val0 = m[alias].val;
            const uint8 len0 = m[alias].len;

            if (srcIdx >= srcEnd) {
                if (KANZI_UNLIKELY(dstIdx + 4 > dstCap))
                    return false;

                memcpy(&dst[dstIdx], &val0, 4);
                dstIdx += len0;
                break;
            }

            alias = uint(src[srcIdx++]);

            if (alias >= 128) {
                if (KANZI_UNLIKELY(srcIdx >= srcEnd))
                    return false;

                alias = (uint(src[srcIdx++]) << 7) + (alias & 0x7F);
            }

            if (KANZI_UNLIKELY(alias >= uint(n)))
                return false;

            const uint32 val1 = m[alias].val;
            const uint8 len1 = m[alias].len;

            const int needed = int(len0) + int(len1) + 4;

            if (KANZI_UNLIKELY(dstCap - dstIdx < needed))
                return false;

            memcpy(&dst[dstIdx], &val0, 4);
            dstIdx += len0;
            memcpy(&dst[dstIdx], &val1, 4);
            dstIdx += len1;
        }
    }

    if ((srcIdx == srcEnd) && (dstIdx < dstEnd + adjust)) {
        if ((srcIdx + 4 - adjust > count) || (dstIdx + 4 - adjust > dstCap))
            return false;

        for (int i = 0; i < 4 - adjust; i++)
            dst[dstIdx++] = src[srcIdx++];
    }

    input._index += srcIdx;
    output._index += dstIdx;
    return srcIdx == count;
}

// A quick partial validation
// A more complete validation is done during processing for the remaining cases
// (rules for 3 and 4 kanzi::byte sequences)
bool UTFCodec::validate(const kanzi::byte block[], int count)
{
    uint freqs0[256] = { 0 };
    uint* freqs1 = new uint[65536];
    memset(&freqs1[0], 0, 65536 * sizeof(uint));
    uint f0[256] = { 0 };
    uint f1[256] = { 0 };
    uint f3[256] = { 0 };
    uint f2[256] = { 0 };
    uint8 prv = 0;
    const uint8* data = reinterpret_cast<const uint8*>(&block[0]);
    const int count4 = count & -4;
    bool res = true;

    // Unroll loop
    for (int i = 0; i < count4; i += 4) {
        const uint8 cur0 = data[i];
        const uint8 cur1 = data[i + 1];
        const uint8 cur2 = data[i + 2];
        const uint8 cur3 = data[i + 3];
        f0[cur0]++;
        f1[cur1]++;
        f2[cur2]++;
        f3[cur3]++;
        freqs1[(prv * 256) + cur0]++;
        freqs1[(cur0 * 256) + cur1]++;
        freqs1[(cur1 * 256) + cur2]++;
        freqs1[(cur2 * 256) + cur3]++;
        prv = cur3;
    }

    for (int i = count4; i < count; i++) {
        freqs0[data[i]]++;
        freqs1[(prv * 256) + data[i]]++;
        prv = data[i];
    }

    for (int i = 0; i < 256; i++) {
        freqs0[i] += (f0[i] + f1[i] + f2[i] + f3[i]);
    }

    // Valid UTF-8 sequences
    // See Unicode 16 Standard - UTF-8 Table 3.7
    // U+0000..U+007F          00..7F
    // U+0080..U+07FF          C2..DF 80..BF
    // U+0800..U+0FFF          E0 A0..BF 80..BF
    // U+1000..U+CFFF          E1..EC 80..BF 80..BF
    // U+D000..U+D7FF          ED 80..9F 80..BF 80..BF
    // U+E000..U+FFFF          EE..EF 80..BF 80..BF
    // U+10000..U+3FFFF        F0 90..BF 80..BF 80..BF
    // U+40000..U+FFFFF        F1..F3 80..BF 80..BF 80..BF
    // U+100000..U+10FFFF      F4 80..8F 80..BF 80..BF

    // Check rules for 1 kanzi::byte
    uint sum = freqs0[0xC0] + freqs0[0xC1];
    uint sum2 = 0;

    for (int i = 0xF5; i <= 0xFF; i++)
        sum += freqs0[i];

    if (sum != 0) {
        res = false;
        goto end;
    }

    // Check rules for first 2 bytes
    for (int i = 0; i < 256; i++) {
        // Exclude < 0xE0A0 || > 0xE0BF
        if ((i < 0xA0) || (i > 0xBF))
            sum += freqs1[0xE0 * 256 + i];

        // Exclude < 0xED80 || > 0xEDE9F
        if ((i < 0x80) || (i > 0x9F))
            sum += freqs1[0xED * 256 + i];

        // Exclude < 0xF090 || > 0xF0BF
        if ((i < 0x90) || (i > 0xBF))
            sum += freqs1[0xF0 * 256 + i];

        // Exclude < 0xF480 || > 0xF48F
        if ((i < 0x80) || (i > 0x8F))
            sum += freqs1[0xF4 * 256 + i];

        if ((i < 0x80) || (i > 0xBF)) {
            // Exclude < 0x??80 || > 0x??BF with ?? in [C2..DF]
            for (int j = 0xC2; j <= 0xDF; j++)
                sum += freqs1[j * 256 + i];

            // Exclude < 0x??80 || > 0x??BF with ?? in [E1..EC]
            for (int j = 0xE1; j <= 0xEC; j++)
                sum += freqs1[j * 256 + i];

            // Exclude < 0x??80 || > 0x??BF with ?? in [F1..F3]
            sum += freqs1[0xF1 * 256 + i];
            sum += freqs1[0xF2 * 256 + i];
            sum += freqs1[0xF3 * 256 + i];

            // Exclude < 0xEE80 || > 0xEEBF
            sum += freqs1[0xEE * 256 + i];

            // Exclude < 0xEF80 || > 0xEFBF
            sum += freqs1[0xEF * 256 + i];
        }
        else {
            // Count non-primary bytes
            sum2 += freqs0[i];
        }

        if (sum != 0) {
            res = false;
            break;
        }
    }

end:
    delete[] freqs1;

    // Ad-hoc threshold
    return (res == true) && (sum2 >= uint(count / 8));
}

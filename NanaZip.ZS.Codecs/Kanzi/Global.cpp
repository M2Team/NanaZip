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

#include <algorithm>
#include <stdexcept>
#include "Global.hpp"
#include "util/strings.hpp"

using namespace kanzi;
using namespace std;

// int(Math.log2(x-1))
const int Global::LOG2[256] = {
    0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8,
};

// 4096*Math.log2(x)
const int Global::LOG2_4096[257] = {
    0, 0, 4096, 6492, 8192, 9511, 10588, 11499, 12288, 12984,
    13607, 14170, 14684, 15157, 15595, 16003, 16384, 16742, 17080, 17400,
    17703, 17991, 18266, 18529, 18780, 19021, 19253, 19476, 19691, 19898,
    20099, 20292, 20480, 20662, 20838, 21010, 21176, 21338, 21496, 21649,
    21799, 21945, 22087, 22226, 22362, 22495, 22625, 22752, 22876, 22998,
    23117, 23234, 23349, 23462, 23572, 23680, 23787, 23892, 23994, 24095,
    24195, 24292, 24388, 24483, 24576, 24668, 24758, 24847, 24934, 25021,
    25106, 25189, 25272, 25354, 25434, 25513, 25592, 25669, 25745, 25820,
    25895, 25968, 26041, 26112, 26183, 26253, 26322, 26390, 26458, 26525,
    26591, 26656, 26721, 26784, 26848, 26910, 26972, 27033, 27094, 27154,
    27213, 27272, 27330, 27388, 27445, 27502, 27558, 27613, 27668, 27722,
    27776, 27830, 27883, 27935, 27988, 28039, 28090, 28141, 28191, 28241,
    28291, 28340, 28388, 28437, 28484, 28532, 28579, 28626, 28672, 28718,
    28764, 28809, 28854, 28898, 28943, 28987, 29030, 29074, 29117, 29159,
    29202, 29244, 29285, 29327, 29368, 29409, 29450, 29490, 29530, 29570,
    29609, 29649, 29688, 29726, 29765, 29803, 29841, 29879, 29916, 29954,
    29991, 30027, 30064, 30100, 30137, 30172, 30208, 30244, 30279, 30314,
    30349, 30384, 30418, 30452, 30486, 30520, 30554, 30587, 30621, 30654,
    30687, 30719, 30752, 30784, 30817, 30849, 30880, 30912, 30944, 30975,
    31006, 31037, 31068, 31099, 31129, 31160, 31190, 31220, 31250, 31280,
    31309, 31339, 31368, 31397, 31426, 31455, 31484, 31513, 31541, 31569,
    31598, 31626, 31654, 31681, 31709, 31737, 31764, 31791, 31818, 31846,
    31872, 31899, 31926, 31952, 31979, 32005, 32031, 32058, 32084, 32109,
    32135, 32161, 32186, 32212, 32237, 32262, 32287, 32312, 32337, 32362,
    32387, 32411, 32436, 32460, 32484, 32508, 32533, 32557, 32580, 32604,
    32628, 32651, 32675, 32698, 32722, 32745, 32768
};

char Global::BASE64_SYMBOLS[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char Global::NUMERIC_SYMBOLS[] = "0123456789+-*/=,.:; ";

char Global::DNA_SYMBOLS[] = "acgntuACGNTU"; // either T or U and N for unknown

const Global Global::_singleton;

int Global::SQUASH[4096];

int Global::STRETCH[4096];


Global::Global()
{
    //  65536 /(1 + exp(-alpha*x)) with alpha ~= 0.54
    const int INV_EXP[33] = {
        0, 8, 22, 47, 88, 160, 283, 492,
        848, 1451, 2459, 4117, 6766, 10819, 16608, 24127,
        32768, 41409, 48928, 54717, 58770, 61419, 63077, 64085,
        64688, 65044, 65253, 65376, 65448, 65489, 65514, 65528,
        65536
    };

    for (int x = 1; x < 4096; x++) {
        const int w = x & 127;
        const int y = x >> 7;
        SQUASH[x - 1] = (INV_EXP[y] * (128 - w) + INV_EXP[y + 1] * w) >> 11;
    }

    SQUASH[4095] = 4095;
    int n = 0;

    for (int x = -2047; x <= 2047; x++) {
        const int sq = squash(x);

        while (n <= sq)
            STRETCH[n++] = x;

	if (n >= 4096)
           break;
    }

    STRETCH[4095] = 2047;

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
    static const string reserved[27] = {
       "AUX", "COM0", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6",
       "COM7", "COM8", "COM9", "COM¹", "COM²", "COM³", "CON", "LPT0",
       "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8",
       "LPT9", "NUL", "PRN"
    };

    for (int i = 0; i < 27; i++)
       _reservedNames.insert(reserved[i]);
#endif
}

// Return 1024 * log2(x). Max error is around 0.1%
int Global::log2_1024(uint32 x)
{
    if (x == 0)
        throw std::invalid_argument("Cannot calculate log of a negative or null value");

    if (x < 256)
        return (Global::LOG2_4096[x] + 2) >> 2;

    const int log = _log2(x);

    if ((x & (x - 1)) == 0)
        return log << 10;

    return ((log - 7) * 1024) + ((LOG2_4096[x >> (log - 7)] + 2) >> 2);
}

int Global::log2(uint32 x)
{
    if (x == 0)
        throw std::invalid_argument("Cannot calculate log of a negative or null value");

    return _log2(x);
}


int Global::log2(uint64 x)
{
    if (x == 0)
        throw std::invalid_argument("Cannot calculate log of a negative or null value");

    return _log2(x);
}

// If withTotal is true, the last spot in each frequencies order 0 array is for the total
void Global::computeHistogram(const kanzi::byte block[], int length, uint freqs[], bool isOrder0, bool withTotal)
{
    if (length <= 0) {
        if ((isOrder0 == true) && (withTotal == true))
            freqs[256] = 0;

        return;
    }

    const uint8* const base = reinterpret_cast<const uint8*>(block);
    const uint8* p = base;

    if (isOrder0 == true) {
        if (withTotal == true)
            freqs[256] = uint(length);

        uint f0[256] = { 0 };
        uint f1[256] = { 0 };
        uint f2[256] = { 0 };
        uint f3[256] = { 0 };
        const uint8* end16 = &base[length & -16];
        uint64 q;

        while (p < end16) {
            memcpy(&q, &p[0], 8);
            f0[uint8(q>>56)]++;
            f1[uint8(q>>48)]++;
            f2[uint8(q>>40)]++;
            f3[uint8(q>>32)]++;
            f0[uint8(q>>24)]++;
            f1[uint8(q>>16)]++;
            f2[uint8(q>>8)]++;
            f3[uint8(q)]++;
            memcpy(&q, &p[8], 8);
            f0[uint8(q>>56)]++;
            f1[uint8(q>>48)]++;
            f2[uint8(q>>40)]++;
            f3[uint8(q>>32)]++;
            f0[uint8(q>>24)]++;
            f1[uint8(q>>16)]++;
            f2[uint8(q>>8)]++;
            f3[uint8(q)]++;
            p += 16;
        }

        const uint8* end = &base[length];

        while (p < end)
            freqs[*p++]++;

        for (int i = 0; i < 256; i++)
            freqs[i] += (f0[i] + f1[i] + f2[i] + f3[i]);
    }
    else { // Order 1
        const int quarter = length >> 2;
        int n0 = 0 * quarter;
        int n1 = 1 * quarter;
        int n2 = 2 * quarter;
        int n3 = 3 * quarter;

        if (withTotal == true) {
            if (length < 32) {
                uint prv = 0;

                for (int i = 0; i < length; i++) {
                    freqs[prv + uint(p[i])]++;
                    freqs[prv + 256]++;
                    prv = 257 * uint(p[i]);
                }
            }
            else {
                uint prv0 = 0;
                uint prv1 = 257 * uint(p[n1 - 1]);
                uint prv2 = 257 * uint(p[n2 - 1]);
                uint prv3 = 257 * uint(p[n3 - 1]);

                for (; n0 < quarter; n0++, n1++, n2++, n3++) {
                    const uint cur0 = uint(p[n0]);
                    const uint cur1 = uint(p[n1]);
                    const uint cur2 = uint(p[n2]);
                    const uint cur3 = uint(p[n3]);
                    freqs[prv0 + cur0]++;
                    freqs[prv0 + 256]++;
                    freqs[prv1 + cur1]++;
                    freqs[prv1 + 256]++;
                    freqs[prv2 + cur2]++;
                    freqs[prv2 + 256]++;
                    freqs[prv3 + cur3]++;
                    freqs[prv3 + 256]++;
                    prv0 = 257 * cur0;
                    prv1 = 257 * cur1;
                    prv2 = 257 * cur2;
                    prv3 = 257 * cur3;
                }

                for (; n3 < length; n3++) {
                    freqs[prv3 + uint(p[n3])]++;
                    freqs[prv3 + 256]++;
                    prv3 = 257 * uint(p[n3]);
                }
            }
        }
        else { // order 1, no total
            if (length < 32) {
                uint prv = 0;

                for (int i = 0; i < length; i++) {
                    freqs[prv + uint(p[i])]++;
                    prv = 256 * uint(p[i]);
                }
            }
            else {
                uint prv0 = 0;
                uint prv1 = 256 * uint(p[n1 - 1]);
                uint prv2 = 256 * uint(p[n2 - 1]);
                uint prv3 = 256 * uint(p[n3 - 1]);

                for (; n0 < quarter; n0++, n1++, n2++, n3++) {
                    const uint cur0 = uint(p[n0]);
                    const uint cur1 = uint(p[n1]);
                    const uint cur2 = uint(p[n2]);
                    const uint cur3 = uint(p[n3]);
                    freqs[prv0 + cur0]++;
                    freqs[prv1 + cur1]++;
                    freqs[prv2 + cur2]++;
                    freqs[prv3 + cur3]++;
                    prv0 = cur0 << 8;
                    prv1 = cur1 << 8;
                    prv2 = cur2 << 8;
                    prv3 = cur3 << 8;
                }

                for (; n3 < length; n3++) {
                    freqs[prv3 + uint(p[n3])]++;
                    prv3 = uint(p[n3]) << 8;
                }
            }
        }
    }
}

// Return the zero order entropy scaled to the [0..1024] range
// Incoming array size must be 256
int Global::computeOrder0Entropy1024(int blockLen, const uint histo[])
{
    if (blockLen == 0)
        return 0;

    uint64 sum = 0;
    const int logLength1024 = Global::log2_1024(uint32(blockLen));

    for (int i = 0; i < 256; i++) {
        if (histo[i] == 0)
            continue;

        sum += ((uint64(histo[i]) * uint64(logLength1024 - Global::log2_1024(histo[i]))) >> 3);
    }

    return int(sum / uint64(blockLen));
}


// Return the order 1 entropy scaled to the [0..1024] range
// Incoming array size must be 65536
int Global::computeOrder1Entropy1024(int blockLen, const uint histo[])
{
    if (blockLen == 0)
        return 0;

    uint64 sum = 0;

    for (int prev = 0; prev < 256; prev++) {
        const int base = 256 * prev;
        uint total = 0;

        for (int cur = 0; cur < 256; cur++)
            total += histo[base + cur];

        if (total == 0)
            continue;

        const int logTotal = Global::log2_1024(total);

        for (int cur = 0; cur < 256; cur++) {
            const uint freq = histo[base + cur];

            if (freq != 0)
                sum += (uint64(freq) * uint64(logTotal - Global::log2_1024(freq))) >> 3;
        }
    }

    return int(sum / uint64(blockLen));
}

void Global::computeJobsPerTask(int jobsPerTask[], int jobs, int tasks)
{
    if (jobs <= 0)
        throw std::invalid_argument("Invalid number of jobs provided");

    if (tasks <= 0)
        throw std::invalid_argument("Invalid number of tasks provided");

    int q = (jobs <= tasks) ? 1 : jobs / tasks;
    int r = (jobs <= tasks) ? 0 : jobs - q * tasks;

    for (int i = 0; i < tasks; i++)
        jobsPerTask[i] = q;

    int n = 0;

    while (r != 0) {
        jobsPerTask[n]++;
        r--;
        n++;
    }
}

Global::DataType Global::detectSimpleType(int count, const uint freqs0[]) {
    int sum = 0;

    for (int i = 0; i < 12; i++)
        sum += freqs0[int(DNA_SYMBOLS[i])];

    if (sum > (count - count / 12))
        return DNA;

    sum = 0;

    for (int i = 0; i < 20; i++)
        sum += freqs0[int(NUMERIC_SYMBOLS[i])];

    if (sum == count)
        return NUMERIC;

    // Last symbol with padding '='
    sum = (freqs0[0x3D] == 1) ? 1 : 0;

    for (int i = 0; i < 64; i++)
        sum += freqs0[int(BASE64_SYMBOLS[i])];

    if (sum == count)
        return BASE64;

    sum = 0;

    for (int i = 0; i < 256; i += 8) {
        sum += (freqs0[i+0] > 0) ? 1 : 0;
        sum += (freqs0[i+1] > 0) ? 1 : 0;
        sum += (freqs0[i+2] > 0) ? 1 : 0;
        sum += (freqs0[i+3] > 0) ? 1 : 0;
        sum += (freqs0[i+4] > 0) ? 1 : 0;
        sum += (freqs0[i+5] > 0) ? 1 : 0;
        sum += (freqs0[i+6] > 0) ? 1 : 0;
        sum += (freqs0[i+7] > 0) ? 1 : 0;
    }

    if (sum == 256)
        return BIN;

    return (sum <= 4) ? SMALL_ALPHABET : UNDEFINED;
}

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
bool Global::isReservedName(string fileName)
{
    transform(fileName.begin(), fileName.end(), fileName.begin(), safeToUpper);
    return _singleton._reservedNames.find(fileName) != _singleton._reservedNames.end();
}
#else
bool Global::isReservedName(string)
{
    return false;
}
#endif

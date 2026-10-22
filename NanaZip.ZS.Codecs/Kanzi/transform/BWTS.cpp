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

#include <sstream>

#include "BWTS.hpp"
#include "../Global.hpp"

using namespace kanzi;
using namespace std;


const int BWTS::MAX_BLOCK_SIZE = 1024 * 1024 * 1024; // 1024 MB


bool BWTS::forward(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw invalid_argument("BWTS: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw invalid_argument("BWTS: Invalid output block");

    if ((count < 0) ||
        (count > input._length - input._index) ||
        (count > output._length - output._index))
        return false;

    if (count > MAX_BLOCK_SIZE) {
        // Not a recoverable error: instead of silently fail the transform,
        // issue a fatal error.
        stringstream ss;
        ss << "The max BWTS block size is " << MAX_BLOCK_SIZE << ", got " << count;
        throw invalid_argument(ss.str());
    }

    if (count < 2) {
        if (count == 1) {
            if ((input._index >= input._length) || (output._index >= output._length))
                return false;

            output._array[output._index++] = input._array[input._index++];
        }

        return true;
    }

    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];

    // Lazy dynamic memory allocation
    if (_bufferSize < count) {
        int* buffer1 = new int[count];
        int* buffer2 = nullptr;

        try {
            buffer2 = new int[count];
        }
        catch (...) {
            delete[] buffer1;
            throw;
        }

        delete[] _buffer1;
        delete[] _buffer2;
        _buffer1 = buffer1;
        _buffer2 = buffer2;
        _bufferSize = count;
    }

    // Aliasing
    int* sa = _buffer1;
    int* isa = _buffer2;

    if (_saAlgo.computeSuffixArray(src, sa, count) == false)
        return false;

    for (int i = 0; i < count; i++)
        isa[sa[i]] = i;

    int min = isa[0];
    int idxMin = 0;

    for (int i = 1; ((i < count) && (min > 0)); i++) {
        if (isa[i] >= min)
            continue;

        int refRank = moveLyndonWordHead(sa, isa, src, count, idxMin, i - idxMin, min);

        for (int j = i - 1; j > idxMin; j--) {
            // Iterate through the new Lyndon word from end to start
            int testRank = isa[j];
            int startRank = testRank;

            while (testRank < count - 1) {
                int nextRankStart = sa[testRank + 1];

                if ((j > nextRankStart) || (src[j] != src[nextRankStart])
                    || (refRank < isa[nextRankStart + 1]))
                    break;

                sa[testRank] = nextRankStart;
                isa[nextRankStart] = testRank;
                testRank++;
            }

            sa[testRank] = j;
            isa[j] = testRank;
            refRank = testRank;

            if (startRank == testRank)
                break;
        }

        min = isa[i];
        idxMin = i;
    }

    min = count;

    for (int i = 0; i < count; i++) {
        if (isa[i] >= min) {
            dst[isa[i]] = src[i - 1];
            continue;
        }

        if (min < count)
            dst[min] = src[i - 1];

        min = isa[i];
    }

    dst[0] = src[count - 1];
    input._index += count;
    output._index += count;
    return true;
}

int BWTS::moveLyndonWordHead(int sa[], int isa[], const kanzi::byte data[], int count,
                             int start, int size, int rank) const
{
    const int end = start + size;

    while (rank + 1 < count) {
        const int nextStart0 = sa[rank + 1];

        if (nextStart0 <= end)
            break;

        int nextStart = nextStart0;
        int k = 0;

        while ((k < size) && (nextStart < count) && (data[start + k] == data[nextStart])) {
            k++;
            nextStart++;
        }

        if ((k == size) && (rank < isa[nextStart]))
            break;

        if ((k < size) && (nextStart < count) && (data[start + k] < data[nextStart]))
            break;

        sa[rank] = nextStart0;
        isa[nextStart0] = rank;
        rank++;
    }

    sa[rank] = start;
    isa[start] = rank;
    return rank;
}

bool BWTS::inverse(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw invalid_argument("BWTS: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw invalid_argument("BWTS: Invalid output block");

    if ((count < 0) ||
        (count > input._length - input._index) ||
        (count > output._length - output._index))
        return false;

    if (count > MAX_BLOCK_SIZE) {
        stringstream ss;
        ss << "The max BWTS block size is " << MAX_BLOCK_SIZE << ", got " << count;
        throw invalid_argument(ss.str());
    }

    if (count < 2) {
        if (count == 1) {
            if ((input._index >= input._length) || (output._index >= output._length))
                return false;

            output._array[output._index++] = input._array[input._index++];
        }

        return true;
    }

    // Lazy dynamic memory allocation
    if (_bufferSize < count) {
        int* buffer1 = new int[count];
        delete[] _buffer1;
        _buffer1 = buffer1;
        _bufferSize = count;
    }

    // Initialize histogram
    uint buckets[256] = { 0 };
    Global::computeHistogram(&input._array[input._index], count, buckets, true);

    // Histogram
    for (int i = 0, sum = 0; i < 256; i++) {
        sum += buckets[i];
        buckets[i] = sum - buckets[i];
    }

    // Aliasing
    int* lf = _buffer1;
    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];

    for (int i = 0; i < count; i++)
        lf[i] = buckets[int(src[i])]++;

    // Build inverse
    for (int i = 0, j = count - 1; j >= 0; i++) {
        if (lf[i] < 0)
            continue;

        int p = i;

        do {
            dst[j] = src[p];
            j--;
            const int t = lf[p];
            lf[p] = -1;
            p = t;
        } while (lf[p] >= 0);
    }

    input._index += count;
    output._index += count;
    return true;
}

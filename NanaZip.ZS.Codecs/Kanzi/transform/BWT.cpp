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
#include <vector>

#include "BWT.hpp"
#include "../Global.hpp"
#include "../Memory.hpp"

#ifdef CONCURRENCY_ENABLED
#include <future>
#endif

using namespace kanzi;
using namespace std;


const int BWT::MAX_BLOCK_SIZE = 1024 * 1024 * 1024; // 1024 MB
const int BWT::NB_FASTBITS = 17;
const int BWT::MASK_FASTBITS = (1 << NB_FASTBITS) - 1;
const int BWT::BLOCK_SIZE_THRESHOLD1 = 256;
const int BWT::BLOCK_SIZE_THRESHOLD2 = 2 * 1024 * 1024;

#define DECODE_BWT(P, S) \
    do { \
        (S) = _fastBits[(P) >> shift]; \
        if (_buckets[(S)] <= (P)) { \
            do { \
                (S)++; \
            } while (_buckets[(S)] <= (P)); \
        } \
    } while (0)


BWT::BWT(int jobs)
{
    _buffer = nullptr;
    _sa = nullptr;
    _bufferSize = 0;
    _saSize = 0;

#ifdef CONCURRENCY_ENABLED
    _pool = nullptr;

    if (jobs < 1)
        throw invalid_argument("The number of jobs must be at least 1");
#else
    if (jobs != 1)
        throw invalid_argument("The number of jobs is limited to 1 in this version");
#endif

    _jobs = jobs;
    memset(_primaryIndexes, 0, sizeof(int) * 8);
}


BWT::BWT(Context& ctx)
{
    _buffer = nullptr;
    _sa = nullptr;
    _bufferSize = 0;
    _saSize = 0;
    int jobs = ctx.getInt("jobs", 1);

#ifdef CONCURRENCY_ENABLED
    _pool = ctx.getPool(); // can be null

    if (jobs < 1)
        throw invalid_argument("The number of jobs must be at least 1");
#else
    if (jobs != 1)
        throw invalid_argument("The number of jobs is limited to 1 in this version");
#endif

    _jobs = jobs;
    memset(_primaryIndexes, 0, sizeof(int) * 8);
}

bool BWT::setPrimaryIndex(int n, int primaryIndex)
{
    if ((primaryIndex < 0) || (n < 0) || (n >= 8))
        return false;

    _primaryIndexes[n] = primaryIndex;
    return true;
}

bool BWT::forward(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if (!SliceArray<kanzi::byte>::isValid(input))
       throw invalid_argument("BWT: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw invalid_argument("BWT: Invalid output block");

    if ((count < 0) ||
        (count > input._length - input._index) ||
        (count > output._length - output._index) ||
        (count > MAX_BLOCK_SIZE))
        return false;

    if (count == 1) {
        if ((input._index >= input._length) || (output._index >= output._length))
            return false;

        output._array[output._index++] = input._array[input._index++];
        return true;
    }

    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];

    // Lazy dynamic memory allocation
    if (_saSize < count) {
         int* sa = new int[count];
         delete[] _sa;
         _sa = sa;
         _saSize = count;
    }

    if (_saAlgo.computeBWT(src, dst, _sa, count, _primaryIndexes, getBWTChunks(count)) == false)
        return false;

    input._index += count;
    output._index += count;
    return true;
}

bool BWT::inverse(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw invalid_argument("BWT: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw invalid_argument("BWT: Invalid output block");

    if ((count < 0) ||
        (count > input._length - input._index) ||
        (count > output._length - output._index) ||
        (count > MAX_BLOCK_SIZE))
        return false;

    if (count == 1) {
        if ((input._index >= input._length) || (output._index >= output._length))
            return false;

        output._array[output._index++] = input._array[input._index++];
        return true;
    }

    // Find the fastest way to implement inverse based on block size
    if (count <= BLOCK_SIZE_THRESHOLD2)
        return inverseMergeTPSI(input, output, count);

    return inverseBiPSIv2(input, output, count);
}

// When count <= BLOCK_SIZE_THRESHOLD2, mergeTPSI algo
bool BWT::inverseMergeTPSI(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
       return true;

    const int pIdx = getPrimaryIndex(0);

    if ((pIdx <= 0) || (pIdx > count))
        return false;

    // Lazy dynamic memory allocation
    if (_bufferSize < count) {
        const int newSize = max(count, 256);
        uint* buffer = new uint[newSize];
        delete[] _buffer;
        _buffer = buffer;
        _bufferSize = newSize;
    }

    // Build array of packed index + value (assumes block size < 1<<24)
    uint buckets[256] = { 0 };
    Global::computeHistogram(&input._array[input._index], count, buckets);

    for (int i = 0, sum = 0; i < 256; i++) {
        const int tmp = buckets[i];
        buckets[i] = sum;
        sum += tmp;
    }

    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];
    const uint end1 = uint(pIdx);
    const uint end2 = uint(count);

    _buffer[buckets[uint8(src[0])]] = uint(src[0]);
    buckets[uint8(src[0])]++;

    for (uint i = 1; i < end1; i++) {
        const uint8 val = uint8(src[i]);
        _buffer[buckets[val]] = ((i - 1) << 8) | val;
        buckets[val]++;
    }

    for (uint i = end1; i < end2; i++) {
        const uint8 val = uint8(src[i]);
        _buffer[buckets[val]] = (i << 8) | val;
        buckets[val]++;
    }

    if (getBWTChunks(count) != 8) {
        int t = pIdx - 1;
        int n = 0;

        while (n < count) {
            const int ptr = _buffer[t];
            dst[n++] = kanzi::byte(ptr);
            t = ptr >> 8;
        }
    }
    else {
        const int ckSize = ((count & 7) == 0) ? count >> 3 : (count >> 3) + 1;
        int t0 = getPrimaryIndex(0) - 1;
        if ((t0 < 0) || (t0 >= count)) return false;
        int t1 = getPrimaryIndex(1) - 1;
        if ((t1 < 0) || (t1 >= count)) return false;
        int t2 = getPrimaryIndex(2) - 1;
        if ((t2 < 0) || (t2 >= count)) return false;
        int t3 = getPrimaryIndex(3) - 1;
        if ((t3 < 0) || (t3 >= count)) return false;
        int t4 = getPrimaryIndex(4) - 1;
        if ((t4 < 0) || (t4 >= count)) return false;
        int t5 = getPrimaryIndex(5) - 1;
        if ((t5 < 0) || (t5 >= count)) return false;
        int t6 = getPrimaryIndex(6) - 1;
        if ((t6 < 0) || (t6 >= count)) return false;
        int t7 = getPrimaryIndex(7) - 1;
        if ((t7 < 0) || (t7 >= count)) return false;

        // Last interval [7*chunk:count] smaller when 8*ckSize != count
        const int end = count - ckSize * 7;
        kanzi::byte* d0 = &dst[end + ckSize * 0];
        kanzi::byte* d1 = &dst[end + ckSize * 1];
        kanzi::byte* d2 = &dst[end + ckSize * 2];
        kanzi::byte* d3 = &dst[end + ckSize * 3];
        kanzi::byte* d4 = &dst[end + ckSize * 4];
        kanzi::byte* d5 = &dst[end + ckSize * 5];
        kanzi::byte* d6 = &dst[end + ckSize * 6];
        kanzi::byte* d7 = &dst[end + ckSize * 7];
        int n = -end;
        int ptr;

        #define S(t, d) ptr = _buffer[t]; \
           d[n] = kanzi::byte(ptr); \
           t = ptr >> 8

        while (n < 0) {
            S(t0, d0);
            S(t1, d1);
            S(t2, d2);
            S(t3, d3);
            S(t4, d4);
            S(t5, d5);
            S(t6, d6);
            S(t7, d7);
            n++;
        }

        while (n < ckSize - end) {
            S(t0, d0);
            S(t1, d1);
            S(t2, d2);
            S(t3, d3);
            S(t4, d4);
            S(t5, d5);
            S(t6, d6);
            n++;
        }
    }

    input._index += count;
    output._index += count;
    return true;
}

// When count > BLOCK_SIZE_THRESHOLD2, biPSIv2 algo
bool BWT::inverseBiPSIv2(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    // Lazy dynamic memory allocations
    if (_bufferSize < count + 1) {
        const int newSize = max(count + 1, 256);
        uint* buffer = new uint[newSize];
        delete[] _buffer;
        _buffer = buffer;
        _bufferSize = newSize;
    }

    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];
    const int pIdx = getPrimaryIndex(0);

    if ((pIdx <= 0) || (pIdx > count))
        return false;

    for (int i = 1; i < 8; i++) {
        const int p = getPrimaryIndex(i);

        if ((p <= 0) || (p > count))
            return false;
    }

    uint* buckets = nullptr;
    uint16* fastBits = nullptr;
#ifdef CONCURRENCY_ENABLED
    vector<InverseBiPSIv2Task<int>*> tasks;
#endif

    try {
        buckets = new uint[65536];
        memset(&buckets[0], 0, 65536 * sizeof(uint));
        uint freqs[256] = { 0 };
        Global::computeHistogram(&input._array[input._index], count, freqs);

        for (int sum = 1, c = 0; c < 256; c++) {
            const int f = sum;
            sum += int(freqs[c]);
            freqs[c] = f;

            if (f != sum) {
                uint* ptr = &buckets[c << 8];
                const int hi = min(sum, pIdx);

                for (int i = f; i < hi; i++)
                    ptr[int(src[i])]++;

                const int lo = max(f - 1, pIdx);

                for (int i = lo; i < sum - 1; i++)
                    ptr[int(src[i])]++;
            }
        }

        const int lastc = int(src[0]);
        fastBits = new uint16[MASK_FASTBITS + 1];
        memset(&fastBits[0], 0, size_t(MASK_FASTBITS + 1) * sizeof(uint16));
        int shift = 0;

        while ((count >> shift) > MASK_FASTBITS)
            shift++;

        for (int v = 0, sum = 1, c = 0; c < 256; c++) {
            if (c == lastc)
                sum++;

            uint* ptr = &buckets[c];

            for (int d = 0; d < 256; d++) {
                const int s = sum;
                sum += ptr[d << 8];
                ptr[d << 8] = s;

                if (s == sum)
                    continue;

                for (; v <= ((sum - 1) >> shift); v++)
                    fastBits[v] = uint16((c << 8) | d);
            }
        }

        memset(&_buffer[0], 0, size_t(count) * sizeof(uint));
        int n = 0;

        while (n < pIdx) {
            const int c = int(src[n]);
            const int p = freqs[c];

            if (p < pIdx)
                _buffer[buckets[(c << 8) | int(src[p])]++] = n;
            else if (p > pIdx)
                _buffer[buckets[(c << 8) | int(src[p - 1])]++] = n;

            freqs[c]++;
            n++;
        }

        while (n < count) {
            const int c = int(src[n]);
            const int p = freqs[c];
            freqs[c]++;
            n++;

            if (p < pIdx)
                _buffer[buckets[(c << 8) | int(src[p])]++] = n;
            else if (p > pIdx)
                _buffer[buckets[(c << 8) | int(src[p - 1])]++] = n;
        }

        for (int c = 0; c < 256; c++) {
            for (int d = 0; d < c; d++) {
                swap(buckets[(d << 8) | c], buckets[(c << 8) | d]);
            }
        }

        const int chunks = getBWTChunks(count);

        // Build inverse
        const int st = count / chunks;
        const int ckSize = (chunks * st == count) ? st : st + 1;
        const int nbTasks = (_jobs < chunks) ? _jobs : chunks;

        if (nbTasks == 1) {
            InverseBiPSIv2Task<int> task(_buffer, buckets, fastBits, dst, _primaryIndexes,
                count, 0, ckSize, 0, chunks);
            task.run();
        }
        else {
#ifdef CONCURRENCY_ENABLED
            // Several chunks may be decoded concurrently (depending on the availability
            // of jobs per block).
            int jobsPerTask[64];
            Global::computeJobsPerTask(jobsPerTask, chunks, nbTasks);
            vector<future<int> > futures;
            tasks.reserve(nbTasks);
            futures.reserve(nbTasks);

            try {
                // Create one task per job
                for (int j = 0, c = 0; j < nbTasks; j++) {
                    // Each task decodes jobsPerTask[j] chunks
                    tasks.push_back(new InverseBiPSIv2Task<int>(_buffer, buckets, fastBits, dst, _primaryIndexes,
                        count, c * ckSize, ckSize, c, c + jobsPerTask[j]));

                    if (_pool == nullptr)
                       futures.push_back(std::async(launch::async, &InverseBiPSIv2Task<int>::run, tasks[j]));
                    else
                       futures.push_back(_pool->schedule(&InverseBiPSIv2Task<int>::run, tasks[j]));

                    c += jobsPerTask[j];
                }

                // Wait for completion of all concurrent tasks
                for (int j = 0; j < nbTasks; j++)
                    futures[j].get();
            }
            catch (...) {
                for (uint i = 0; i < futures.size(); i++) {
                    try {
                        if (futures[i].valid())
                            futures[i].wait();
                    }
                    catch (const exception&) {
                    }
                }

                throw;
            }
#else
            // nbTasks > 1 but concurrency is not enabled (should never happen)
            throw invalid_argument("Error during BWT inverse: concurrency not supported");
#endif
        }

        dst[count - 1] = kanzi::byte(lastc);
        input._index += count;
        output._index += count;
    }
    catch (...) {
#ifdef CONCURRENCY_ENABLED
        for (uint i = 0; i < tasks.size(); i++)
            delete tasks[i];
#endif
        delete[] fastBits;
        delete[] buckets;
        throw;
    }

#ifdef CONCURRENCY_ENABLED
    for (uint i = 0; i < tasks.size(); i++)
        delete tasks[i];
#endif
    delete[] fastBits;
    delete[] buckets;
    return true;
}

template <class T>
InverseBiPSIv2Task<T>::InverseBiPSIv2Task(uint* buf, uint* buckets, uint16* fastBits, kanzi::byte* output,
                                          int* primaryIndexes, int total, int start, int ckSize, int firstChunk, int lastChunk)
                                          : _data(buf)
                                          , _buckets(buckets)
                                          , _fastBits(fastBits)
                                          , _primaryIndexes(primaryIndexes)
                                          , _dst(output)
                                          , _total(total)
                                          , _start(start)
                                          , _ckSize(ckSize)
                                          , _firstChunk(firstChunk)
                                          , _lastChunk(lastChunk)
{
}

template <class T>
T InverseBiPSIv2Task<T>::run()
{
    uint sh = 0;

    while ((_total >> sh) > BWT::MASK_FASTBITS)
        sh++;

    const uint shift = sh;
    int c = _firstChunk;
    kanzi::byte* d0 = &_dst[0 * _ckSize];
    kanzi::byte* d1 = &_dst[1 * _ckSize];
    kanzi::byte* d2 = &_dst[2 * _ckSize];
    kanzi::byte* d3 = &_dst[3 * _ckSize];
    kanzi::byte* d4 = &_dst[4 * _ckSize];
    kanzi::byte* d5 = &_dst[5 * _ckSize];
    kanzi::byte* d6 = &_dst[6 * _ckSize];
    kanzi::byte* d7 = &_dst[7 * _ckSize];

    if (_start + 7 * _ckSize <= _total) {
        for (; c + 8 <= _lastChunk; c += 8) {
            const int end = _start + _ckSize - 1;
            const int end8 = min(end, _total - 7 * _ckSize - 1);
            uint p0 = _primaryIndexes[c + 0];
            uint p1 = _primaryIndexes[c + 1];
            uint p2 = _primaryIndexes[c + 2];
            uint p3 = _primaryIndexes[c + 3];
            uint p4 = _primaryIndexes[c + 4];
            uint p5 = _primaryIndexes[c + 5];
            uint p6 = _primaryIndexes[c + 6];
            uint p7 = _primaryIndexes[c + 7];

            for (int i = _start + 1; i <= end8; i += 2) {
                prefetchRead(&_data[p0]);
                prefetchRead(&_data[p1]);
                prefetchRead(&_data[p2]);
                prefetchRead(&_data[p3]);
                prefetchRead(&_data[p4]);
                prefetchRead(&_data[p5]);
                prefetchRead(&_data[p6]);
                prefetchRead(&_data[p7]);
                uint16 s0 = _fastBits[p0 >> shift];
                uint16 s1 = _fastBits[p1 >> shift];
                uint16 s2 = _fastBits[p2 >> shift];
                uint16 s3 = _fastBits[p3 >> shift];
                uint16 s4 = _fastBits[p4 >> shift];
                uint16 s5 = _fastBits[p5 >> shift];
                uint16 s6 = _fastBits[p6 >> shift];
                uint16 s7 = _fastBits[p7 >> shift];

                if (_buckets[s0] <= p0) {
                   do {
                      s0++;
                   } while (_buckets[s0] <= p0);
                }

                if (_buckets[s1] <= p1) {
                   do {
                      s1++;
                   } while (_buckets[s1] <= p1);
                }

                if (_buckets[s2] <= p2) {
                   do {
                      s2++;
                    } while (_buckets[s2] <= p2);
                }

                if (_buckets[s3] <= p3) {
                   do {
                      s3++;
                    } while (_buckets[s3] <= p3);
                }

                if (_buckets[s4] <= p4) {
                   do {
                      s4++;
                    } while (_buckets[s4] <= p4);
                }

                if (_buckets[s5] <= p5) {
                   do {
                      s5++;
                    } while (_buckets[s5] <= p5);
                }

                if (_buckets[s6] <= p6) {
                   do {
                      s6++;
                    } while (_buckets[s6] <= p6);
                }

                if (_buckets[s7] <= p7) {
                   do {
                      s7++;
                    } while (_buckets[s7] <= p7);
                }

                d0[i - 1] = kanzi::byte(s0 >> 8);
                d0[i] = kanzi::byte(s0);
                d1[i - 1] = kanzi::byte(s1 >> 8);
                d1[i] = kanzi::byte(s1);
                d2[i - 1] = kanzi::byte(s2 >> 8);
                d2[i] = kanzi::byte(s2);
                d3[i - 1] = kanzi::byte(s3 >> 8);
                d3[i] = kanzi::byte(s3);
                d4[i - 1] = kanzi::byte(s4 >> 8);
                d4[i] = kanzi::byte(s4);
                d5[i - 1] = kanzi::byte(s5 >> 8);
                d5[i] = kanzi::byte(s5);
                d6[i - 1] = kanzi::byte(s6 >> 8);
                d6[i] = kanzi::byte(s6);
                d7[i - 1] = kanzi::byte(s7 >> 8);
                d7[i] = kanzi::byte(s7);

                p0 = _data[p0];
                p1 = _data[p1];
                p2 = _data[p2];
                p3 = _data[p3];
                p4 = _data[p4];
                p5 = _data[p5];
                p6 = _data[p6];
                p7 = _data[p7];
            }

            // Keep the eighth chain within its logical end. If the common
            // extent is odd, retain the low byte for the first seven chains.
            const bool oddCommon = ((end8 - _start + 1) & 1) != 0;

            if (oddCommon) {
                // Keep the low byte for the first seven chains; the eighth
                // chain ends at end8 and only needs the high byte.
                uint16 s0, s1, s2, s3, s4, s5, s6, s7;
                DECODE_BWT(p0, s0);
                DECODE_BWT(p1, s1);
                DECODE_BWT(p2, s2);
                DECODE_BWT(p3, s3);
                DECODE_BWT(p4, s4);
                DECODE_BWT(p5, s5);
                DECODE_BWT(p6, s6);
                DECODE_BWT(p7, s7);
                d0[end8] = kanzi::byte(s0 >> 8);
                d1[end8] = kanzi::byte(s1 >> 8);
                d2[end8] = kanzi::byte(s2 >> 8);
                d3[end8] = kanzi::byte(s3 >> 8);
                d4[end8] = kanzi::byte(s4 >> 8);
                d5[end8] = kanzi::byte(s5 >> 8);
                d6[end8] = kanzi::byte(s6 >> 8);
                d7[end8] = kanzi::byte(s7 >> 8);

                if (end8 < end) {
                    d0[end8 + 1] = kanzi::byte(s0);
                    d1[end8 + 1] = kanzi::byte(s1);
                    d2[end8 + 1] = kanzi::byte(s2);
                    d3[end8 + 1] = kanzi::byte(s3);
                    d4[end8 + 1] = kanzi::byte(s4);
                    d5[end8 + 1] = kanzi::byte(s5);
                    d6[end8 + 1] = kanzi::byte(s6);
                }

                p0 = _data[p0];
                p1 = _data[p1];
                p2 = _data[p2];
                p3 = _data[p3];
                p4 = _data[p4];
                p5 = _data[p5];
                p6 = _data[p6];
                p7 = _data[p7];
            }

            // The last chunk can be shorter than the other seven. Finish the
            // common extent of the first seven chains without a per-iteration
            // boundary check for the eighth chain.
            if (end8 < end) {
                const int nextPos = end8 + (oddCommon ? 2 : 1);
                const int tailStart = nextPos + 1;

                for (int i = tailStart; i <= end; i += 2) {
                    prefetchRead(&_data[p0]);
                    prefetchRead(&_data[p1]);
                    prefetchRead(&_data[p2]);
                    prefetchRead(&_data[p3]);
                    prefetchRead(&_data[p4]);
                    prefetchRead(&_data[p5]);
                    prefetchRead(&_data[p6]);
                    uint16 s0, s1, s2, s3, s4, s5, s6;
                    DECODE_BWT(p0, s0);
                    DECODE_BWT(p1, s1);
                    DECODE_BWT(p2, s2);
                    DECODE_BWT(p3, s3);
                    DECODE_BWT(p4, s4);
                    DECODE_BWT(p5, s5);
                    DECODE_BWT(p6, s6);
                    d0[i - 1] = kanzi::byte(s0 >> 8);
                    d0[i] = kanzi::byte(s0);
                    d1[i - 1] = kanzi::byte(s1 >> 8);
                    d1[i] = kanzi::byte(s1);
                    d2[i - 1] = kanzi::byte(s2 >> 8);
                    d2[i] = kanzi::byte(s2);
                    d3[i - 1] = kanzi::byte(s3 >> 8);
                    d3[i] = kanzi::byte(s3);
                    d4[i - 1] = kanzi::byte(s4 >> 8);
                    d4[i] = kanzi::byte(s4);
                    d5[i - 1] = kanzi::byte(s5 >> 8);
                    d5[i] = kanzi::byte(s5);
                    d6[i - 1] = kanzi::byte(s6 >> 8);
                    d6[i] = kanzi::byte(s6);
                    p0 = _data[p0];
                    p1 = _data[p1];
                    p2 = _data[p2];
                    p3 = _data[p3];
                    p4 = _data[p4];
                    p5 = _data[p5];
                    p6 = _data[p6];
                }

                if ((nextPos <= end) && (((end - nextPos + 1) & 1) != 0)) {
                    prefetchRead(&_data[p0]);
                    prefetchRead(&_data[p1]);
                    prefetchRead(&_data[p2]);
                    prefetchRead(&_data[p3]);
                    prefetchRead(&_data[p4]);
                    prefetchRead(&_data[p5]);
                    prefetchRead(&_data[p6]);
                    uint16 s0, s1, s2, s3, s4, s5, s6;
                    DECODE_BWT(p0, s0);
                    DECODE_BWT(p1, s1);
                    DECODE_BWT(p2, s2);
                    DECODE_BWT(p3, s3);
                    DECODE_BWT(p4, s4);
                    DECODE_BWT(p5, s5);
                    DECODE_BWT(p6, s6);
                    d0[end] = kanzi::byte(s0 >> 8);
                    d1[end] = kanzi::byte(s1 >> 8);
                    d2[end] = kanzi::byte(s2 >> 8);
                    d3[end] = kanzi::byte(s3 >> 8);
                    d4[end] = kanzi::byte(s4 >> 8);
                    d5[end] = kanzi::byte(s5 >> 8);
                    d6[end] = kanzi::byte(s6 >> 8);
                }
            }

            _start += (8 * _ckSize);
        }
    }

    // Preserve interleaving when a worker owns four or two chunks.
    if ((_start + 3 * _ckSize <= _total) && ((_ckSize & 1) == 0)) {
        for (; c + 4 <= _lastChunk; c += 4) {
            const int end = _start + _ckSize - 1;
            const int end4 = min(end, _total - 3 * _ckSize - 1);
            uint p0 = _primaryIndexes[c + 0];
            uint p1 = _primaryIndexes[c + 1];
            uint p2 = _primaryIndexes[c + 2];
            uint p3 = _primaryIndexes[c + 3];

            for (int i = _start + 1; i <= end4; i += 2) {
                prefetchRead(&_data[p0]);
                prefetchRead(&_data[p1]);
                prefetchRead(&_data[p2]);
                prefetchRead(&_data[p3]);
                uint16 s0, s1, s2, s3;
                DECODE_BWT(p0, s0);
                DECODE_BWT(p1, s1);
                DECODE_BWT(p2, s2);
                DECODE_BWT(p3, s3);
                d0[i - 1] = kanzi::byte(s0 >> 8);
                d0[i] = kanzi::byte(s0);
                d1[i - 1] = kanzi::byte(s1 >> 8);
                d1[i] = kanzi::byte(s1);
                d2[i - 1] = kanzi::byte(s2 >> 8);
                d2[i] = kanzi::byte(s2);
                d3[i - 1] = kanzi::byte(s3 >> 8);
                d3[i] = kanzi::byte(s3);
                p0 = _data[p0];
                p1 = _data[p1];
                p2 = _data[p2];
                p3 = _data[p3];
            }

            if (end4 < end) {
                const int tailStart = end4 + 1 + (end4 & 1);

                for (int i = tailStart; i <= end; i += 2) {
                    prefetchRead(&_data[p0]);
                    prefetchRead(&_data[p1]);
                    prefetchRead(&_data[p2]);
                    uint16 s0, s1, s2;
                    DECODE_BWT(p0, s0);
                    DECODE_BWT(p1, s1);
                    DECODE_BWT(p2, s2);
                    d0[i - 1] = kanzi::byte(s0 >> 8);
                    d0[i] = kanzi::byte(s0);
                    d1[i - 1] = kanzi::byte(s1 >> 8);
                    d1[i] = kanzi::byte(s1);
                    d2[i - 1] = kanzi::byte(s2 >> 8);
                    d2[i] = kanzi::byte(s2);
                    p0 = _data[p0];
                    p1 = _data[p1];
                    p2 = _data[p2];
                }
            }

            _start += (4 * _ckSize);
        }
    }

    if ((_start + _ckSize <= _total) && ((_ckSize & 1) == 0)) {
        for (; c + 2 <= _lastChunk; c += 2) {
            const int end = _start + _ckSize - 1;
            const int end2 = min(end, _total - _ckSize - 1);
            uint p0 = _primaryIndexes[c + 0];
            uint p1 = _primaryIndexes[c + 1];

            for (int i = _start + 1; i <= end2; i += 2) {
                prefetchRead(&_data[p0]);
                prefetchRead(&_data[p1]);
                uint16 s0, s1;
                DECODE_BWT(p0, s0);
                DECODE_BWT(p1, s1);
                d0[i - 1] = kanzi::byte(s0 >> 8);
                d0[i] = kanzi::byte(s0);
                d1[i - 1] = kanzi::byte(s1 >> 8);
                d1[i] = kanzi::byte(s1);
                p0 = _data[p0];
                p1 = _data[p1];
            }

            if (end2 < end) {
                const int tailStart = end2 + 1 + (end2 & 1);

                for (int i = tailStart; i <= end; i += 2) {
                    prefetchRead(&_data[p0]);
                    uint16 s0;
                    DECODE_BWT(p0, s0);
                    d0[i - 1] = kanzi::byte(s0 >> 8);
                    d0[i] = kanzi::byte(s0);
                    p0 = _data[p0];
                }
            }

            _start += (2 * _ckSize);
        }
    }

    for (; c < _lastChunk; c++) {
        const int end = min(_start + _ckSize, _total - 1);
        uint p = _primaryIndexes[c];

        for (int i = _start + 1; i <= end; i += 2) {
            uint16 s = _fastBits[p >> shift];

            while (_buckets[s] <= p)
                s++;

            _dst[i - 1] = kanzi::byte(s >> 8);
            _dst[i] = kanzi::byte(s);
            p = _data[p];
        }

        _start = end;
    }

    return T(0);
}

#undef DECODE_BWT

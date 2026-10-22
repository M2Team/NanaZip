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
#include "SBRT.hpp"

using namespace kanzi;


const int SBRT::MODE_MTF = 1; // alpha = 0
const int SBRT::MODE_RANK = 2; // alpha = 1/2
const int SBRT::MODE_TIMESTAMP = 3; // alpha = 1



SBRT::SBRT(int mode) :
	  _mask1((mode == MODE_TIMESTAMP) ? 0 : -1)
	, _mask2((mode == MODE_MTF) ? 0 : -1)
	, _shift((mode == MODE_RANK) ? 1 : 0)
{
    if ((mode != MODE_MTF) && (mode != MODE_RANK) && (mode != MODE_TIMESTAMP))
        throw std::invalid_argument("Invalid mode parameter");
}

SBRT::SBRT(int mode, Context&) :
	  _mask1((mode == MODE_TIMESTAMP) ? 0 : -1)
	, _mask2((mode == MODE_MTF) ? 0 : -1)
	, _shift((mode == MODE_RANK) ? 1 : 0)
{
    if ((mode != MODE_MTF) && (mode != MODE_RANK) && (mode != MODE_TIMESTAMP))
        throw std::invalid_argument("Invalid mode parameter");
}

bool SBRT::forward(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw std::invalid_argument("SBRT: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw std::invalid_argument("SBRT: Invalid output block");

    if ((count < 0) ||
        (count > input._length - input._index) ||
        (count > output._length - output._index))
        return false;

    // Aliasing
    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];
    int p[256] = { 0 };
    int q[256] = { 0 };
    uint8 s2r[256];
    uint8 r2s[256];

    for (int i = 0; i < 256; i++) {
        s2r[i] = uint8(i);
        r2s[i] = uint8(i);
    }

    for (int i = 0; i < count; i++) {
        const uint8 c = uint8(src[i]);
        int r = int(s2r[c]);
        dst[i] = kanzi::byte(r);
        const int qc = ((i & _mask1) + (p[c] & _mask2)) >> _shift;
        p[c] = i;
        q[c] = qc;

        // Move up symbol to correct rank
        while ((r > 0) && (q[r2s[r - 1]] <= qc)) {
            r2s[r] = r2s[r - 1];
            s2r[r2s[r]] = uint8(r);
            r--;
        }

        r2s[r] = c;
        s2r[c] = uint8(r);
    }

    input._index += count;
    output._index += count;
    return true;
}

bool SBRT::inverse(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw std::invalid_argument("SBRT: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw std::invalid_argument("SBRT: Invalid output block");

    int p[256] = { 0 };
    int q[256] = { 0 };
    uint8 r2s[256];

    for (int i = 0; i < 256; i++)
        r2s[i] = uint8(i);

    // Check before forming slice pointers. This placement also keeps the inverse loop fast.
    if ((count < 0) ||
        (count > input._length - input._index) ||
        (count > output._length - output._index))
        return false;

    // Aliasing
    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];

    for (int i = 0; i < count; i++) {
        int r = int(src[i]);
        const int c = int(r2s[r]);
        dst[i] = kanzi::byte(r2s[r]);
        const int qc = ((i & _mask1) + (p[c] & _mask2)) >> _shift;
        p[c] = i;
        q[c] = qc;

        // Move up symbol to correct rank
        while ((r > 0) && (q[r2s[r - 1]] <= qc)) {
            r2s[r] = r2s[r - 1];
            r--;
        }

        r2s[r] = uint8(c);
    }

    input._index += count;
    output._index += count;
    return true;
}

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

#include "HuffmanCommon.hpp"

using namespace kanzi;


const int HuffmanCommon::LOG_MAX_CHUNK_SIZE = 14;
const int HuffmanCommon::MAX_CHUNK_SIZE = 1 << LOG_MAX_CHUNK_SIZE;
const int HuffmanCommon::MAX_SYMBOL_SIZE = 12;


// Return the number of codes generated
// codes and symbols are updated
int HuffmanCommon::generateCanonicalCodes(const uint16 sizes[], uint16 codes[], uint symbols[], int count)
{
    if (count == 0)
        return 0;

    if ((count < 0) || (count > 256))
        return -1;

    if (count > 1) {
        uint8 present[256] = { 0 };
        uint16 offsets[MAX_SYMBOL_SIZE + 1] = { 0 };

        for (int i = 0; i < count; i++) {
            const uint s = symbols[i];

            if (s > 255)
                return -1;

            const uint16 len = sizes[s];

            if ((len == 0) || (len > MAX_SYMBOL_SIZE) || (present[s] != 0))
                return -1;

            present[s] = 1;
            offsets[len]++;
        }

        uint16 offset = 0;

        for (int len = 1; len <= MAX_SYMBOL_SIZE; len++) {
            const uint16 n = offsets[len];
            offsets[len] = offset;
            offset += n;
        }

        // Scanning symbols in value order preserves the previous tie break.
        for (uint s = 0; s < 256; s++) {
            if (present[s] != 0)
                symbols[offsets[sizes[s]]++] = s;
        }
    }

    int curLen = sizes[symbols[0]];

    for (int i = 0, code = 0; i < count; i++) {
        const int s = symbols[i];
        code <<= (sizes[s] - curLen);
        curLen = sizes[s];
        codes[s] = uint16(code);
        code++;
    }

    return count;
}

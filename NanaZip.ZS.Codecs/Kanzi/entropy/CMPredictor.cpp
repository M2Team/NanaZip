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


#include "CMPredictor.hpp"

using namespace kanzi;

const int CMPredictor::FAST_RATE = 2;
const int CMPredictor::MEDIUM_RATE = 4;
const int CMPredictor::SLOW_RATE = 6;
const int CMPredictor::PSCALE = 65536;


CMPredictor::CMPredictor(Context* pCtx)
{
    _ctx = 1;
    _runMask = 0;
    _c1 = 0;
    _c2 = 0;
    const int bsVersion = (pCtx == nullptr) ? 7 : pCtx->getInt("bsVersion", 7);

    for (int i = 0; i < 256; i++) {
        for (int j = 0; j <= 256; j++)
            _counter1[i][j] = 32768;

        for (int j = 0; j <= 16; j++) {
            _counter2[2 * i][j] = j << 12;
            _counter2[2 * i + 1][j] = j << 12;
        }

        // Keep backward compatibility with old streams
        if (bsVersion >= 7) {
            _counter2[2 * i][16] = 65535;
            _counter2[2 * i + 1][16] = 65535;
        }
    }

    _pc1 = _counter1[_ctx];
    _pc2 = &_counter2[_ctx][8];
}

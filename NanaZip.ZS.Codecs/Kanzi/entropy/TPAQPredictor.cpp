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

#include "TPAQPredictor.hpp"

using namespace kanzi;

const int TPAQMixer::BEGIN_LEARN_RATE = 60 << 7;
const int TPAQMixer::END_LEARN_RATE = 11 << 7;

template<>
const int TPAQPredictor<true>::MAX_LENGTH = 88;
template<>
const int TPAQPredictor<true>::BUFFER_SIZE = 64 * 1024 * 1024;
template<>
const int TPAQPredictor<true>::HASH_SIZE = 16 * 1024 * 1024;
template<>
const int TPAQPredictor<true>::HASH = 0x7FEB352D;
template<>
const uint TPAQPredictor<true>::MASK_80808080 = 0x80808080u;
template<>
const uint TPAQPredictor<true>::MASK_F0F0F000 = 0xF0F0F000u;
template<>
const uint TPAQPredictor<true>::MASK_4F4FFFFF = 0x4F4FFFFFu;
template<>
const int TPAQPredictor<false>::MAX_LENGTH = 88;
template<>
const int TPAQPredictor<false>::BUFFER_SIZE = 64 * 1024 * 1024;
template<>
const int TPAQPredictor<false>::HASH_SIZE = 16 * 1024 * 1024;
template<>
const int TPAQPredictor<false>::HASH = 0x7FEB352D;
template<>
const uint TPAQPredictor<false>::MASK_80808080 = 0x80808080u;
template<>
const uint TPAQPredictor<false>::MASK_F0F0F000 = 0xF0F0F000u;
template<>
const uint TPAQPredictor<false>::MASK_4F4FFFFF = 0x4F4FFFFFu;


TPAQMixer::TPAQMixer()
{
    _pr = 2048;
    _skew = 0;
    _w0 = _w1 = _w2 = _w3 = _w4 = _w5 = _w6 = _w7 = 32768;
    _p0 = _p1 = _p2 = _p3 = _p4 = _p5 = _p6 = _p7 = 0;
    _learnRate = BEGIN_LEARN_RATE;
}

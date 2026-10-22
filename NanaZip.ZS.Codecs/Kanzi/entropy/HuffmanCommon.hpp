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

#pragma once
#ifndef knz_HuffmanCommon
#define knz_HuffmanCommon

#include "../types.hpp"


namespace kanzi
{

   class HuffmanCommon
   {
   public:
       static const int LOG_MAX_CHUNK_SIZE;
       static const int MAX_CHUNK_SIZE;
       static const int MAX_SYMBOL_SIZE;

       static int generateCanonicalCodes(const uint16 sizes[], uint16 codes[], uint ranks[], int count);

   };

}
#endif

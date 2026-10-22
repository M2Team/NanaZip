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
#ifndef knz_EntropyEncoderFactory
#define knz_EntropyEncoderFactory

#include <algorithm>
#include "../util/strings.hpp"
#include "../Context.hpp"
#include "ANSRangeEncoder.hpp"
#include "BinaryEntropyEncoder.hpp"
#include "HuffmanEncoder.hpp"
#include "NullEntropyEncoder.hpp"
#include "RangeEncoder.hpp"
#include "CMPredictor.hpp"
#include "FPAQEncoder.hpp"
#include "TPAQPredictor.hpp"


namespace kanzi {

   class EntropyEncoderFactory {
   public:
       static const short NONE_TYPE = 0; // No compression
       static const short HUFFMAN_TYPE = 1; // Huffman
       static const short FPAQ_TYPE = 2; // Fast PAQ (order 0)
       static const short PAQ_TYPE = 3; // Obsolete
       static const short RANGE_TYPE = 4; // Range
       static const short ANS0_TYPE = 5; // Asymmetric Numerical System order 0
       static const short CM_TYPE = 6; // Context Model
       static const short TPAQ_TYPE = 7; // Tangelo PAQ
       static const short ANS1_TYPE = 8; // Asymmetric Numerical System order 1
       static const short TPAQX_TYPE = 9; // Tangelo PAQ Extra
       static const short RESERVED1 = 10; //Reserved
       static const short RESERVED2 = 11; //Reserved
       static const short RESERVED3 = 12; //Reserved
       static const short RESERVED4 = 13; //Reserved
       static const short RESERVED5 = 14; //Reserved
       static const short RESERVED6 = 15; //Reserved

       static EntropyEncoder* newEncoder(OutputBitStream& obs, Context& ctx, short entropyType);

       static const char* getName(short entropyType);

       static short getType(const char* name);
   };


   inline EntropyEncoder* EntropyEncoderFactory::newEncoder(OutputBitStream& obs, Context& ctx, short entropyType)
   {
       switch (entropyType) {
       case HUFFMAN_TYPE:
           return new HuffmanEncoder(obs);

       case ANS0_TYPE:
           return new ANSRangeEncoder(obs, 0);

       case ANS1_TYPE:
           return new ANSRangeEncoder(obs, 1);

       case RANGE_TYPE:
           return new RangeEncoder(obs);

       case FPAQ_TYPE:
           return new FPAQEncoder(obs);

       case CM_TYPE:
           return new BinaryEntropyEncoder(obs, new CMPredictor(&ctx));

       case TPAQ_TYPE:
           return new BinaryEntropyEncoder(obs, new TPAQPredictor<false>(&ctx));

       case TPAQX_TYPE:
           return new BinaryEntropyEncoder(obs, new TPAQPredictor<true>(&ctx));

       case NONE_TYPE:
           return new NullEntropyEncoder(obs);

       default:
           std::string msg = "Unknown entropy codec type: '";
           msg += char(entropyType);
           msg += '\'';
           throw std::invalid_argument(msg);
       }
   }


   inline const char* EntropyEncoderFactory::getName(short entropyType)
   {
       switch (entropyType) {
       case HUFFMAN_TYPE:
           return "HUFFMAN";

       case ANS0_TYPE:
           return "ANS0";

       case ANS1_TYPE:
           return "ANS1";

       case RANGE_TYPE:
           return "RANGE";

       case FPAQ_TYPE:
           return "FPAQ";

       case CM_TYPE:
           return "CM";

       case TPAQ_TYPE:
           return "TPAQ";

       case TPAQX_TYPE:
           return "TPAQX";

       case NONE_TYPE:
           return "NONE";

       default:
           std::string msg = "Unknown entropy codec type: '";
           msg += char(entropyType);
           msg += '\'';
           throw std::invalid_argument(msg);
       }
   }


   inline short EntropyEncoderFactory::getType(const char* str)
   {
       std::string name = str;
       transform(name.begin(), name.end(), name.begin(), safeToUpper);

       if (name == "HUFFMAN")
           return HUFFMAN_TYPE;

       if (name == "ANS0")
           return ANS0_TYPE;

       if (name == "ANS1")
           return ANS1_TYPE;

       if (name == "FPAQ")
           return FPAQ_TYPE;

       if (name == "RANGE")
           return RANGE_TYPE;

       if (name == "CM")
           return CM_TYPE;

       if (name == "TPAQ")
           return TPAQ_TYPE;

       if (name == "TPAQX")
           return TPAQX_TYPE;

       if (name == "NONE")
           return NONE_TYPE;

       throw std::invalid_argument("Unsupported entropy codec type: '" + name + "'");
   }
}
#endif

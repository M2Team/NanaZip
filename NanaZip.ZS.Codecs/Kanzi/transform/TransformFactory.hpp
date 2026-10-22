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
#ifndef knz_TransformFactory
#define knz_TransformFactory

#include <iostream>
#include <algorithm>
#include <sstream>
#include "../types.hpp"
#include "../Context.hpp"
#include "../util/strings.hpp"
#include "AliasCodec.hpp"
#include "BWTBlockCodec.hpp"
#include "BWTS.hpp"
#include "EXECodec.hpp"
#include "FSDCodec.hpp"
#include "LZCodec.hpp"
#include "NullTransform.hpp"
#include "ROLZCodec.hpp"
#include "RLT.hpp"
#include "SBRT.hpp"
#include "SRT.hpp"
#include "TextCodec.hpp"
#include "TransformSequence.hpp"
#include "UTFCodec.hpp"
#include "ZRLT.hpp"


namespace kanzi {

    template <class T>
    class TransformFactory {
    public:
        // Up to 64 transforms can be declared (6 bit index)
        enum TransformType {
            NONE_TYPE = 0, // Copy
            BWT_TYPE = 1, // Burrows Wheeler
            BWTS_TYPE = 2, // Burrows Wheeler Scott
            LZ_TYPE = 3, // Lempel Ziv
            SNAPPY_TYPE = 4, // Snappy (obsolete)
            RLT_TYPE = 5, // Run Length
            ZRLT_TYPE = 6, // Zero Run Length
            MTFT_TYPE = 7, // Move To Front
            RANK_TYPE = 8, // Rank
            EXE_TYPE = 9, // EXE codec
            DICT_TYPE = 10, // Text codec
            ROLZ_TYPE = 11, // ROLZ codec
            ROLZX_TYPE = 12, // ROLZ Extra codec
            SRT_TYPE = 13, // Sorted Rank
            LZP_TYPE = 14, // Lempel Ziv Predict
            MM_TYPE = 15, // Multimedia (FSD) codec
            LZX_TYPE = 16, // Lempel Ziv Extra
            UTF_TYPE = 17, // UTF Codec
            PACK_TYPE = 18, // Alias Codec
            DNA_TYPE = 19, // DNA Alias Codec
            RESERVED3 = 20, // Reserved
            RESERVED4 = 21, // Reserved
            RESERVED5 = 22 // Reserved
        };


        static uint64 getType(const char* tName);

        static uint64 getTypeToken(const char* tName);

        static std::string getName(uint64 functionType);

        static int getTransformCount(uint64 functionType);

        static TransformSequence<T>* newTransform(Context& ctx, uint64 functionType);

    private:
        TransformFactory() {}

        ~TransformFactory() {}

        static const int ONE_SHIFT = 6; // bits per transform
        static const int MAX_SHIFT = (8 - 1) * ONE_SHIFT; // 8 transforms
        static const int MASK = (1 << ONE_SHIFT) - 1;

        static Transform<T>* newToken(Context& ctx, uint64 functionType);

        static const char* getNameToken(uint64 functionType);
    };

    // The returned type contains 8 transform values
    template <class T>
    uint64 TransformFactory<T>::getType(const char* tName)
    {
        std::string name(tName);
        size_t pos = name.find('+');

        if (pos == std::string::npos)
            return getTypeToken(name.c_str()) << MAX_SHIFT;

        size_t prv = 0;
        int n = 0;
        uint64 res = 0;
        int shift = MAX_SHIFT;
        name += '+';

        while (pos != std::string::npos) {
            n++;

            if (n > 8) {
                std::stringstream ss;
                ss << "Only 8 transforms allowed: " << name;
                throw std::invalid_argument(ss.str());
            }

            std::string token = name.substr(prv, pos - prv);
            uint64 typeTk = getTypeToken(token.c_str());

            // Skip null transform
            if (typeTk != NONE_TYPE) {
                res |= (typeTk << shift);
                shift -= ONE_SHIFT;
            }

            prv = pos + 1;
            pos = name.find('+', prv);
        }

        return res;
    }

    template <class T>
    uint64 TransformFactory<T>::getTypeToken(const char* tName)
    {
        std::string name(tName);
        transform(name.begin(), name.end(), name.begin(), safeToUpper);

        if (name == "TEXT")
            return DICT_TYPE;

        if (name == "BWT")
            return BWT_TYPE;

        if (name == "BWTS")
            return BWTS_TYPE;

        if (name == "ROLZ")
            return ROLZ_TYPE;

        if (name == "ROLZX")
            return ROLZX_TYPE;

        if (name == "MTFT")
            return MTFT_TYPE;

        if (name == "ZRLT")
            return ZRLT_TYPE;

        if (name == "RLT")
            return RLT_TYPE;

        if (name == "SRT")
            return SRT_TYPE;

        if (name == "RANK")
            return RANK_TYPE;

        if (name == "LZ")
            return LZ_TYPE;

        if (name == "LZX")
            return LZX_TYPE;

        if (name == "LZP")
            return LZP_TYPE;

        if (name == "EXE")
            return EXE_TYPE;

        if (name == "UTF")
            return UTF_TYPE;

        if (name == "PACK")
            return PACK_TYPE;

        if (name == "DNA")
            return DNA_TYPE;

        if (name == "MM")
            return MM_TYPE;

        if (name == "NONE")
            return NONE_TYPE;

        std::stringstream ss;
        ss << "Unknown transform type: '" << name << "'";
        throw std::invalid_argument(ss.str());
    }

    template <class T>
    int TransformFactory<T>::getTransformCount(uint64 functionType)
    {
        int count = 1;

        for (int i = 1; i < 8; i++) {
            if (((functionType >> (MAX_SHIFT - ONE_SHIFT * i)) & MASK) == NONE_TYPE)
                break;

            count++;
        }

        return count;
    }

    template <class T>
    TransformSequence<T>* TransformFactory<T>::newTransform(Context& ctx, uint64 functionType)
    {
        Transform<T>* transforms[8];
        int nbtr = 0;

        for (int i = 0; i < 8; i++) {
            transforms[i] = nullptr;
            const uint64 t = (functionType >> (MAX_SHIFT - ONE_SHIFT * i)) & MASK;

            if ((t != NONE_TYPE) || (i == 0))
                transforms[nbtr++] = newToken(ctx, t);
        }

        return new TransformSequence<T>(transforms, true);
    }

    template <class T>
    Transform<T>* TransformFactory<T>::newToken(Context& ctx, uint64 functionType)
    {
        switch (functionType) {
        case DICT_TYPE: {
            int textCodecType = 1;

            if (ctx.has("entropy")) {
                std::string entropyType = ctx.getString("entropy");
                transform(entropyType.begin(), entropyType.end(), entropyType.begin(), safeToUpper);

                // Select text encoding based on entropy codec.
                if ((entropyType == "NONE") || (entropyType == "ANS0") ||
                   (entropyType == "HUFFMAN") || (entropyType == "RANGE"))
                    textCodecType = 2;
            }

            ctx.putInt("textcodec", textCodecType);
            return new TextCodec(ctx);
        }

        case ROLZ_TYPE:
            return new ROLZCodec(ctx);

        case ROLZX_TYPE:
            return new ROLZCodec(ctx);

        case BWT_TYPE:
            return new BWTBlockCodec(ctx);

        case BWTS_TYPE:
            return new BWTS(ctx);

        case LZX_TYPE:
            ctx.putInt("lz", LZX_TYPE);
            return new LZCodec(ctx);

        case LZ_TYPE:
            ctx.putInt("lz", LZ_TYPE);
            return new LZCodec(ctx);

        case LZP_TYPE:
            ctx.putInt("lz", LZP_TYPE);
            return new LZCodec(ctx);

        case RANK_TYPE:
            return new SBRT(SBRT::MODE_RANK, ctx);

        case SRT_TYPE:
            return new SRT(ctx);

        case MTFT_TYPE:
            return new SBRT(SBRT::MODE_MTF, ctx);

        case ZRLT_TYPE:
            return new ZRLT(ctx);

        case RLT_TYPE:
            return new RLT(ctx);

        case EXE_TYPE:
            return new EXECodec(ctx);

        case UTF_TYPE:
            return new UTFCodec(ctx);

        case PACK_TYPE:
            return new AliasCodec(ctx);

        case DNA_TYPE:
            ctx.putInt("packOnlyDNA", 1);
            return new AliasCodec(ctx);

        case MM_TYPE:
            return new FSDCodec(ctx);

        case NONE_TYPE:
            return new NullTransform(ctx);

        default:
            std::stringstream ss;
            ss << "Unknown transform type: '" << functionType << "'";
            throw std::invalid_argument(ss.str());
        }
    }

    template <class T>
    std::string TransformFactory<T>::getName(uint64 functionType)
    {
        std::stringstream res;
        bool first = true;

        for (int i = 0; i < 8; i++) {
            const uint64 t = (functionType >> (MAX_SHIFT - ONE_SHIFT * i)) & MASK;

            if (t == NONE_TYPE)
                continue;

            if (first == false)
                res << '+';

            res << getNameToken(t);
            first = false;
        }

        return (first == true) ? getNameToken(NONE_TYPE) : res.str();
    }

    template <class T>
    const char* TransformFactory<T>::getNameToken(uint64 functionType)
    {
        switch (functionType) {
        case DICT_TYPE:
            return "TEXT";

        case BWT_TYPE:
            return "BWT";

        case BWTS_TYPE:
            return "BWTS";

        case ROLZ_TYPE:
            return "ROLZ";

        case ROLZX_TYPE:
            return "ROLZX";

        case LZ_TYPE:
            return "LZ";

        case LZX_TYPE:
            return "LZX";

        case LZP_TYPE:
            return "LZP";

        case ZRLT_TYPE:
            return "ZRLT";

        case RLT_TYPE:
            return "RLT";

        case SRT_TYPE:
            return "SRT";

        case RANK_TYPE:
            return "RANK";

        case MTFT_TYPE:
            return "MTFT";

        case EXE_TYPE:
            return "EXE";

        case PACK_TYPE:
            return "PACK";

        case DNA_TYPE:
            return "DNA";

        case UTF_TYPE:
            return "UTF";

        case MM_TYPE:
            return "MM";

        case NONE_TYPE:
            return "NONE";

        default:
            std::stringstream ss;
            ss << "Unknown transform type: '" << functionType << "'";
            throw std::invalid_argument(ss.str());
        }
    }
}

#endif

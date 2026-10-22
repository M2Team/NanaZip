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
#ifndef knz_UTFCodec
#define knz_UTFCodec

#include "../Context.hpp"
#include "../Transform.hpp"


namespace kanzi
{
    typedef struct ssUTF
    {
        uint32 val;
        uint32 freq;

        ssUTF(uint32 v, uint32 f) : val(v), freq(f) {}

        friend bool operator< (ssUTF const& lhs, ssUTF const& rhs) {
            int r;
            return ((r = int(lhs.freq - rhs.freq)) != 0) ? r > 0 : lhs.val > rhs.val;
        }
    } sdUTF;

    
    // UTF8 encoder/decoder
    class UTFCodec FINAL : public Transform<byte> {
    public:
        UTFCodec() : _pCtx(nullptr), _aliasMap(nullptr) {}

        UTFCodec(Context& ctx) : _pCtx(&ctx), _aliasMap(nullptr) {}

        ~UTFCodec() { delete[] _aliasMap; }

        bool forward(SliceArray<byte>& source, SliceArray<byte>& destination, int length);

        bool inverse(SliceArray<byte>& source, SliceArray<byte>& destination, int length);

        int getMaxEncodedLength(int srcLen) const { return srcLen + 8192; }

    private:

        static const int MIN_BLOCK_SIZE;
        static const int LEN_SEQ[256];

        Context* _pCtx;
        uint32* _aliasMap;
       
        static bool validate(const byte block[], int count);

        static int pack(const byte in[], uint32& out);

        static int unpack(uint32 in, byte out[]);
   };


    inline int UTFCodec::pack(const byte in[], uint32& out)
    {   
       int s;

       switch (int(in[0]) >> 4) {
       case 0:
       case 1:
       case 2:
       case 3:
       case 4:
       case 5:
       case 6:
       case 7:
           out = uint32(in[0]);
           s = 1;
           break;

       case 12:
       case 13:
           out = (1 << 19) | (uint32(in[0]) << 8) | uint32(in[1]);
           s = 2;
           break; 

       case 14:
           out = (2 << 19) | ((uint32(in[0]) & 0x0F) << 12) | ((uint32(in[1]) & 0x3F) << 6) | (uint32(in[2]) & 0x3F);
           s = 3;
           break;

       case 15:
           out = (4 << 19) | ((uint32(in[0]) & 0x07) << 18) | ((uint32(in[1]) & 0x3F) << 12) | ((uint32(in[2]) & 0x3F) << 6) | (uint32(in[3]) & 0x3F);
           s = 4;
           break;

       default:
           out = 0;
           s = 0; // signal invalid value
           break;
       }

       return s; 
    }


    inline int UTFCodec::unpack(uint32 in, byte out[]) 
    { 
       int s;
       
       switch (in >> 19) {
       case 0:
           out[0] = byte(in);
           s = 1;
           break;

       case 1:
           out[0] = byte(in >> 8);
           out[1] = byte(in);
           s = 2;
           break;

       case 2:
           out[0] = byte(((in >> 12) & 0x0F) | 0xE0);
           out[1] = byte(((in >> 6) & 0x3F) | 0x80);
           out[2] = byte((in & 0x3F) | 0x80);
           s = 3;
           break;

       case 4:
       case 5:
       case 6:
       case 7:
           out[0] = byte(((in >> 18) & 0x07) | 0xF0);
           out[1] = byte(((in >> 12) & 0x3F) | 0x80);
           out[2] = byte(((in >> 6) & 0x3F) | 0x80);
           out[3] = byte((in & 0x3F) | 0x80);
           s = 4;
           break;

       default:
           s = 0; // signal invalid value
           break;
       }

       return s; 
    }
}
#endif

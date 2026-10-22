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
#ifndef knz_EXECodec
#define knz_EXECodec

#include "../Context.hpp"
#include "../Transform.hpp"

namespace kanzi
{
   class EXECodec FINAL : public Transform<byte> {
   public:
       EXECodec() { _pCtx = nullptr; }

       EXECodec(Context& ctx) : _pCtx(&ctx) {}

       ~EXECodec() {}

       bool forward(SliceArray<byte>& source, SliceArray<byte>& destination, int length);

       bool inverse(SliceArray<byte>& source, SliceArray<byte>& destination, int length);

       int getMaxEncodedLength(int inputLen) const;

   private:

       static const byte X86_MASK_JUMP;
       static const byte X86_INSTRUCTION_JUMP;
       static const byte X86_INSTRUCTION_JCC;
       static const byte X86_TWO_BYTE_PREFIX;
       static const byte X86_MASK_JCC;
       static const byte X86_ESCAPE;
       static const byte NOT_EXE;
       static const byte X86;
       static const byte ARM64;
       static const byte MASK_DT;
       static const int X86_ADDR_MASK;
       static const int MASK_ADDRESS;
       static const int ARM_B_ADDR_MASK;
       static const int ARM_B_OPCODE_MASK;
       static const int ARM_B_ADDR_SGN_MASK;
       static const int ARM_OPCODE_B;
       static const int ARM_OPCODE_BL;
       static const int ARM_CB_REG_BITS;
       static const int ARM_CB_ADDR_MASK;
       static const int ARM_CB_ADDR_SGN_MASK;
       static const int ARM_CB_OPCODE_MASK;
       static const int ARM_OPCODE_CBZ;
       static const int ARM_OPCODE_CBNZ;
       static const int WIN_PE;
       static const uint16 WIN_X86_ARCH;
       static const uint16 WIN_AMD64_ARCH;
       static const uint16 WIN_ARM64_ARCH;
       static const int ELF_X86_ARCH;
       static const int ELF_AMD64_ARCH;
       static const int ELF_ARM64_ARCH;
       static const int MAC_AMD64_ARCH;
       static const int MAC_ARM64_ARCH;
       static const int MAC_MH_EXECUTE;
       static const int MAC_LC_SEGMENT;
       static const int MAC_LC_SEGMENT64;
       static const int MIN_BLOCK_SIZE;
       static const int MAX_BLOCK_SIZE;


       bool forwardARM(SliceArray<byte>& source, SliceArray<byte>& destination, int length, int codeStart, int codeEnd);

       bool forwardX86(SliceArray<byte>& source, SliceArray<byte>& destination, int length, int codeStart, int codeEnd);

       bool inverseARM(SliceArray<byte>& source, SliceArray<byte>& destination, int length);

       bool inverseX86(SliceArray<byte>& source, SliceArray<byte>& destination, int length);

       static byte detectType(const byte src[], int count, int& codeStart, int& codeEnd);
       
       static bool parseHeader(const byte src[], int count, uint magic, int& arch, int& codeStart, int& codeEnd);

       Context* _pCtx;
   };
   
   
    inline int EXECodec::getMaxEncodedLength(int srcLen) const
    {
        // Allocate some extra buffer for incompressible data.
        return (srcLen <= 256) ? srcLen + 32 : srcLen + srcLen / 8;
    }   

}
#endif


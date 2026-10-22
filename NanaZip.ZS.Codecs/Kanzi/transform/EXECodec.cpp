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

#include "../Global.hpp"
#include "../Magic.hpp"
#include "EXECodec.hpp"

using namespace kanzi;
using namespace std;

const kanzi::byte EXECodec::X86_MASK_JUMP = kanzi::byte(0xFE);
const kanzi::byte EXECodec::X86_INSTRUCTION_JUMP = kanzi::byte(0xE8);
const kanzi::byte EXECodec::X86_INSTRUCTION_JCC = kanzi::byte(0x80);
const kanzi::byte EXECodec::X86_TWO_BYTE_PREFIX = kanzi::byte(0x0F);
const kanzi::byte EXECodec::X86_MASK_JCC = kanzi::byte(0xF0);
const kanzi::byte EXECodec::X86_ESCAPE = kanzi::byte(0x9B);
const kanzi::byte EXECodec::NOT_EXE = kanzi::byte(0x80);
const kanzi::byte EXECodec::X86 = kanzi::byte(0x40);
const kanzi::byte EXECodec::ARM64 = kanzi::byte(0x20);
const kanzi::byte EXECodec::MASK_DT = kanzi::byte(0x0F);
const int EXECodec::X86_ADDR_MASK = (1 << 24) - 1;
const int EXECodec::MASK_ADDRESS = 0xF0F0F0F0;
const int EXECodec::ARM_B_ADDR_MASK = (1 << 26) - 1;
const int EXECodec::ARM_B_OPCODE_MASK = 0xFFFFFFFF ^ ARM_B_ADDR_MASK;
const int EXECodec::ARM_B_ADDR_SGN_MASK = 1 << 25;
const int EXECodec::ARM_OPCODE_B = 0x14000000;  // 6 bit opcode
const int EXECodec::ARM_OPCODE_BL = 0x94000000; // 6 bit opcode
const int EXECodec::ARM_CB_REG_BITS = 5; // lowest bits for register
const int EXECodec::ARM_CB_ADDR_MASK = 0x00FFFFE0; // 18 bit addr mask
const int EXECodec::ARM_CB_ADDR_SGN_MASK = 1 << 18;
const int EXECodec::ARM_CB_OPCODE_MASK = 0x7F000000;
const int EXECodec::ARM_OPCODE_CBZ = 0x34000000;  // 8 bit opcode
const int EXECodec::ARM_OPCODE_CBNZ = 0x35000000; // 8 bit opcode
const int EXECodec::WIN_PE = 0x00004550;
const uint16 EXECodec::WIN_X86_ARCH = 0x014C;
const uint16 EXECodec::WIN_AMD64_ARCH = 0x8664;
const uint16 EXECodec::WIN_ARM64_ARCH = 0xAA64;
const int EXECodec::ELF_X86_ARCH = 0x03;
const int EXECodec::ELF_AMD64_ARCH = 0x3E;
const int EXECodec::ELF_ARM64_ARCH = 0xB7;
const int EXECodec::MAC_AMD64_ARCH = 0x01000007;
const int EXECodec::MAC_ARM64_ARCH = 0x0100000C;
const int EXECodec::MAC_MH_EXECUTE = 0x02;
const int EXECodec::MAC_LC_SEGMENT = 0x01;
const int EXECodec::MAC_LC_SEGMENT64 = 0x19;
const int EXECodec::MIN_BLOCK_SIZE = 4096;
const int EXECodec::MAX_BLOCK_SIZE = (1 << (26 + 2)) - 1; // max offset << 2

bool EXECodec::forward(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if ((count < MIN_BLOCK_SIZE) || (count > MAX_BLOCK_SIZE))
        return false;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw std::invalid_argument("EXECodec: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw std::invalid_argument("EXECodec: Invalid output block");

    if (output._length - output._index < getMaxEncodedLength(count))
        return false;

    if (_pCtx != nullptr) {
        Global::DataType dt = (Global::DataType)_pCtx->getInt("dataType", Global::UNDEFINED);

        if ((dt != Global::UNDEFINED) && (dt != Global::EXE) && (dt != Global::BIN))
            return false;
    }

    int codeStart = 0;
    int codeEnd = count;
    kanzi::byte mode = detectType(&input._array[input._index], count, codeStart, codeEnd);

    if ((mode & NOT_EXE) != kanzi::byte(0)) {
        if (_pCtx != nullptr)
            _pCtx->putInt("dataType", Global::DataType(mode & MASK_DT));

        return false;
    }

    mode &= ~MASK_DT;
    bool res = false;

    if (mode == X86)
        res = forwardX86(input, output, count, codeStart, codeEnd);
    else if (mode == ARM64)
        res = forwardARM(input, output, count, codeStart, codeEnd);
    else
        return false;

    if ((_pCtx != nullptr) && (res == true))
        _pCtx->putInt("dataType", Global::EXE);

    return res;
}

bool EXECodec::forwardX86(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count, int codeStart, int codeEnd)
{
    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];
    const int dstCapacity = output._length - output._index;
    dst[0] = X86;
    int srcIdx = codeStart;
    int dstIdx = 9;
    int matches = 0;
    const int dstEnd = dstCapacity - 5;
    bool boundaryReached = false;

    if ((codeStart < 0) || (codeStart > count) || (dstIdx + codeStart > dstCapacity))
        return false;

    if ((codeEnd < codeStart) || (codeEnd > count))
        return false;

    if (codeStart > 0) {
        memcpy(&dst[dstIdx], &src[0], size_t(codeStart));
        dstIdx += codeStart;
    }

    while ((srcIdx < codeEnd) && (dstIdx < dstEnd)) {
        if (src[srcIdx] == X86_TWO_BYTE_PREFIX) {
            if (srcIdx + 1 >= codeEnd) {
                boundaryReached = true;
                break;
            }

            if ((src[srcIdx + 1] & X86_MASK_JCC) == X86_INSTRUCTION_JCC) {
                if (srcIdx + 5 >= codeEnd) {
                    boundaryReached = true;
                    break;
                }
            }

            dst[dstIdx++] = src[srcIdx++];

            if ((src[srcIdx] & X86_MASK_JCC) != X86_INSTRUCTION_JCC) {
                // Not a relative jump
                if (src[srcIdx] == X86_ESCAPE)
                    dst[dstIdx++] = X86_ESCAPE;

                dst[dstIdx++] = src[srcIdx++];
                continue;
            }

            if (srcIdx + 4 >= codeEnd) {
                boundaryReached = true;
                break;
            }
        } else if ((src[srcIdx] & X86_MASK_JUMP) != X86_INSTRUCTION_JUMP) {
            // Not a relative call
            if (src[srcIdx] == X86_ESCAPE)
                dst[dstIdx++] = X86_ESCAPE;

            dst[dstIdx++] = src[srcIdx++];
            continue;
        } else if (srcIdx + 4 >= codeEnd) {
            boundaryReached = true;
            break;
        }

        // Current instruction is a jump/call.
        const int sgn = int(src[srcIdx + 4]);
        const int offset = LittleEndian::readInt32(&src[srcIdx + 1]);

        if (((sgn != 0) && (sgn != 0xFF)) || (offset == int(0xFF000000))) {
            dst[dstIdx++] = X86_ESCAPE;
            dst[dstIdx++] = src[srcIdx++];
            continue;
        }

        // Absolute target address = srcIdx + 5 + offset. Let us ignore the +5
        const int addr = srcIdx + ((sgn == 0) ? offset : -(-offset & X86_ADDR_MASK));
        dst[dstIdx++] = src[srcIdx++];
        BigEndian::writeInt32(&dst[dstIdx], addr ^ MASK_ADDRESS);
        srcIdx += 4;
        dstIdx += 4;
        matches++;
    }

    if ((matches < 16) || ((srcIdx < codeEnd) && (boundaryReached == false)))
        return false;

    if (dstIdx + (count - srcIdx) > dstEnd)
        return false;

    LittleEndian::writeInt32(&dst[1], codeStart);
    LittleEndian::writeInt32(&dst[5], dstIdx);
    memcpy(&dst[dstIdx], &src[srcIdx], size_t(count - srcIdx));
    dstIdx += (count - srcIdx);

    // Cap expansion due to false positives
    if (dstIdx > count + (count / 50))
        return false;

    input._index += count;
    output._index += dstIdx;
    return true;
}

bool EXECodec::forwardARM(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count, int codeStart, int codeEnd)
{
    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];
    const int dstCapacity = output._length - output._index;
    dst[0] = ARM64;
    int srcIdx = codeStart;
    int dstIdx = 9;
    int matches = 0;
    const int dstEnd = dstCapacity - 8;

    if ((codeStart < 0) || (codeStart > count) || (dstIdx + codeStart > dstCapacity))
        return false;

    if ((codeEnd < codeStart) || (codeEnd > count))
        return false;

    if (codeStart > 0) {
        memcpy(&dst[dstIdx], &src[0], size_t(codeStart));
        dstIdx += codeStart;
    }

    while ((srcIdx + 4 <= codeEnd) && (dstIdx < dstEnd)) {
        const int instr = LittleEndian::readInt32(&src[srcIdx]);
        const int opcode1 = instr & ARM_B_OPCODE_MASK;
        //const int opcode2 = instr & ARM_CB_OPCODE_MASK;
        bool isBL = (opcode1 == ARM_OPCODE_B) || (opcode1 == ARM_OPCODE_BL); // unconditional jump
        bool isCB = false; // disable for now ... isCB = (opcode2 == ARM_OPCODE_CBZ) || (opcode2 == ARM_OPCODE_CBNZ); // conditional jump

        if ((isBL == false) && (isCB == false)) {
            // Not a relative jump
            memcpy(&dst[dstIdx], &src[srcIdx], 4);
            srcIdx += 4;
            dstIdx += 4;
            continue;
        }

        int addr, val;

        if (isBL == true) {
            // opcode(6) + sgn(1) + offset(25)
            // Absolute target address = srcIdx +/- (offset*4)
            const int offset = instr & ARM_B_ADDR_MASK;
            const int sgn = instr & ARM_B_ADDR_SGN_MASK;
            addr = srcIdx + 4 * ((sgn == 0) ? offset : -(-offset & ARM_B_ADDR_MASK));

            if (addr < 0)
                addr = 0;

            val = opcode1 | (addr >> 2);
        } else { // isCB == true
            // opcode(8) + sgn(1) + offset(18) + register(5)
            // Absolute target address = srcIdx +/- (offset*4)
            const int offset = (instr & ARM_CB_ADDR_MASK) >> ARM_CB_REG_BITS;
            const int sgn = instr & ARM_CB_ADDR_SGN_MASK;
            addr = srcIdx + 4 * ((sgn == 0) ? offset : -(-offset & ARM_B_ADDR_MASK));

            if (addr < 0)
                addr = 0;

            val = (instr & ~ARM_CB_ADDR_MASK) | ((addr >> 2) << ARM_CB_REG_BITS);
        }

        if (addr == 0) {
            LittleEndian::writeInt32(&dst[dstIdx], val); // 0 address as escape
            memcpy(&dst[dstIdx + 4], &src[srcIdx], 4);
            srcIdx += 4;
            dstIdx += 8;
            continue;
        }

        LittleEndian::writeInt32(&dst[dstIdx], val);
        srcIdx += 4;
        dstIdx += 4;
        matches++;
    }

    if ((matches < 16) || ((srcIdx + 4 <= codeEnd) && (dstIdx >= dstEnd)))
        return false;

    if (dstIdx + (count - srcIdx) > dstEnd)
        return false;

    LittleEndian::writeInt32(&dst[1], codeStart);
    LittleEndian::writeInt32(&dst[5], dstIdx);
    memcpy(&dst[dstIdx], &src[srcIdx], size_t(count - srcIdx));
    dstIdx += (count - srcIdx);

    // Cap expansion due to false positives
    if (dstIdx > count + (count / 50))
        return false;

    input._index += count;
    output._index += dstIdx;
    return true;
}

bool EXECodec::inverse(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    if (count == 0)
        return true;

    if ((count < 9) || (count > input._length - input._index))
        return false;

    if (!SliceArray<kanzi::byte>::isValid(input))
        throw std::invalid_argument("EXECodec: Invalid input block");

    if (!SliceArray<kanzi::byte>::isValid(output))
        throw std::invalid_argument("EXECodec: Invalid output block");

    kanzi::byte mode = input._array[input._index];

    if (mode == X86)
        return inverseX86(input, output, count);

    if (mode == ARM64)
        return inverseARM(input, output, count);

    return false;
}

bool EXECodec::inverseX86(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];
    int srcIdx = 9;
    int dstIdx = 0;
    const int dstEnd = output._length - output._index;
    const int codeStart = LittleEndian::readInt32(&src[1]);
    const int codeEnd = LittleEndian::readInt32(&src[5]);

    // Sanity check
    if ((codeStart < 0) || (codeEnd < srcIdx) || (codeEnd > count) ||
        (codeStart > codeEnd - srcIdx) || (codeStart > dstEnd - dstIdx))
        return false;

    if (codeStart > 0) {
        memcpy(&dst[dstIdx], &src[srcIdx], size_t(codeStart));
        dstIdx += codeStart;
        srcIdx += codeStart;
    }

    while (srcIdx < codeEnd) {
        if (src[srcIdx] == X86_TWO_BYTE_PREFIX) {
            if (srcIdx + 1 >= codeEnd) {
                // Accept legacy streams where a trailing 0x0F was emitted in
                // the code section and the remaining bytes were copied as tail.
                if (dstIdx >= dstEnd)
                    return false;

                dst[dstIdx++] = src[srcIdx++];
                break;
            }

            if (dstIdx >= dstEnd)
                return false;

            dst[dstIdx++] = src[srcIdx++];

            if ((src[srcIdx] & X86_MASK_JCC) != X86_INSTRUCTION_JCC) {
                // Not a relative jump
                if (src[srcIdx] == X86_ESCAPE) {
                    srcIdx++;

                    if (srcIdx >= codeEnd)
                        return false;
                }

                if (dstIdx >= dstEnd)
                    return false;

                dst[dstIdx++] = src[srcIdx++];
                continue;
            }
        } else if ((src[srcIdx] & X86_MASK_JUMP) != X86_INSTRUCTION_JUMP) {
            // Not a relative call
            if (src[srcIdx] == X86_ESCAPE) {
                srcIdx++;

                if (srcIdx >= codeEnd)
                    return false;
            }

            if (dstIdx >= dstEnd)
                return false;

            dst[dstIdx++] = src[srcIdx++];
            continue;
        }

        if (srcIdx + 4 >= codeEnd)
            return false;

        if (dstIdx + 5 > dstEnd)
            return false;

        // Current instruction is a jump/call. Decode absolute address
        const int addr = BigEndian::readInt32(&src[srcIdx + 1]) ^ MASK_ADDRESS;
        const int64 offset = int64(addr) - int64(dstIdx);
        const int32 encodedOffset = (offset >= 0) ? int32(offset) : -int32((-offset) & X86_ADDR_MASK);
        dst[dstIdx++] = src[srcIdx++];
        LittleEndian::writeInt32(&dst[dstIdx], encodedOffset);
        srcIdx += 4;
        dstIdx += 4;
    }

    if (dstIdx + (count - srcIdx) > dstEnd)
        return false;

    if (srcIdx < count) {
       memcpy(&dst[dstIdx], &src[srcIdx], size_t(count - srcIdx));
       dstIdx += (count - srcIdx);
    }

    input._index += count;
    output._index += dstIdx;
    return true;
}

bool EXECodec::inverseARM(SliceArray<kanzi::byte>& input, SliceArray<kanzi::byte>& output, int count)
{
    const kanzi::byte* src = &input._array[input._index];
    kanzi::byte* dst = &output._array[output._index];
    int srcIdx = 9;
    int dstIdx = 0;
    const int dstEnd = output._length - output._index;
    const int codeStart = LittleEndian::readInt32(&src[1]);
    const int codeEnd = LittleEndian::readInt32(&src[5]);

    // Sanity check
    if ((codeStart < 0) || (codeEnd < srcIdx) || (codeEnd > count) ||
        (codeStart > codeEnd - srcIdx) || (codeStart > dstEnd - dstIdx))
        return false;

    if (codeStart > 0) {
        memcpy(&dst[dstIdx], &src[srcIdx], size_t(codeStart));
        dstIdx += codeStart;
        srcIdx += codeStart;
    }

    while (srcIdx < codeEnd) {
        if (srcIdx + 4 > codeEnd)
            return false;

        if (dstIdx + 4 > dstEnd)
            return false;

        const int instr = LittleEndian::readInt32(&src[srcIdx]);
        const int opcode1 = instr & ARM_B_OPCODE_MASK;
        //const int opcode2 = instr & ARM_CB_OPCODE_MASK;
        bool isBL = (opcode1 == ARM_OPCODE_B) || (opcode1 == ARM_OPCODE_BL); // unconditional jump
        bool isCB = false; // disable for now ... isCB = (opcode2 == ARM_OPCODE_CBZ) || (opcode2 == ARM_OPCODE_CBNZ); // conditional jump

        if ((isBL == false) && (isCB == false)) {
            // Not a relative jump
            memcpy(&dst[dstIdx], &src[srcIdx], 4);
            srcIdx += 4;
            dstIdx += 4;
            continue;
        }

        // Decode absolute address
        int val, addr;

        if (isBL == true) {
            addr = (instr & ARM_B_ADDR_MASK) << 2;
            const int offset = (addr - dstIdx) >> 2;
            val = opcode1 | (offset & ARM_B_ADDR_MASK);
        } else {
            addr = ((instr & ARM_CB_ADDR_MASK) >> ARM_CB_REG_BITS) << 2;
            const int offset = (addr - dstIdx) >> 2;
            val = (instr & ~ARM_CB_ADDR_MASK) | (offset << ARM_CB_REG_BITS);
        }

        if (addr == 0) {
            if (srcIdx + 8 > codeEnd)
                return false;

            memcpy(&dst[dstIdx], &src[srcIdx + 4], 4);
            srcIdx += 8;
            dstIdx += 4;
            continue;
        }

        LittleEndian::writeInt32(&dst[dstIdx], val);
        srcIdx += 4;
        dstIdx += 4;
    }

    if (dstIdx + (count - srcIdx) > dstEnd)
        return false;

    if (srcIdx < count) {
       memcpy(&dst[dstIdx], &src[srcIdx], size_t(count - srcIdx));
       dstIdx += (count - srcIdx);
    }

    input._index += count;
    output._index += dstIdx;
    return true;
}

kanzi::byte EXECodec::detectType(const kanzi::byte src[], int count, int& codeStart, int& codeEnd)
{
    // Let us check the first bytes ... but this may not be the first block
    // Best effort
    const uint magic = Magic::getType(src);
    int arch = 0;
    const int blockSize = count;

    if (parseHeader(src, count, magic, arch, codeStart, codeEnd) == true) {
        if ((codeStart < 0) || (codeStart > blockSize) || (codeEnd < codeStart) || (codeEnd > blockSize))
            return NOT_EXE | kanzi::byte(Global::UNDEFINED);

        switch(arch) {
            case ELF_X86_ARCH:
            case ELF_AMD64_ARCH:
            case WIN_X86_ARCH:
            case WIN_AMD64_ARCH:
            case MAC_AMD64_ARCH:
               return X86;

            case ELF_ARM64_ARCH:
            case WIN_ARM64_ARCH:
            case MAC_ARM64_ARCH:
               return ARM64;

            default:
               count = codeEnd - codeStart;
        }
    }

    if ((codeStart < 0) || (codeStart > blockSize) || (codeEnd < codeStart) || (codeEnd > blockSize))
        return NOT_EXE | kanzi::byte(Global::UNDEFINED);

    if (count <= 0)
        return NOT_EXE | kanzi::byte(Global::UNDEFINED);

    int jumpsX86 = 0;
    int jumpsARM64 = 0;
    uint histo[256] = { 0 };

    for (int i = codeStart; i < codeEnd; i++) {
        histo[int(src[i])]++;

        // X86
        if (((i + 4) < codeEnd) && ((src[i] & X86_MASK_JUMP) == X86_INSTRUCTION_JUMP)) {
            if ((src[i + 4] == kanzi::byte(0)) || (src[i + 4] == kanzi::byte(0xFF))) {
                // Count relative jumps (CALL = E8/ JUMP = E9 .. .. .. 00/FF)
                jumpsX86++;
                continue;
            }
        } else if ((src[i] == X86_TWO_BYTE_PREFIX) && (i + 1 < codeEnd)) {
            int j = i + 1;

            if (((src[j] == kanzi::byte(0x38)) || (src[j] == kanzi::byte(0x3A))) && (j + 1 < codeEnd))
                j++;

            // Count relative conditional jumps (0x0F 0x8?) with 16/32 offsets
            if ((src[j] & X86_MASK_JCC) == X86_INSTRUCTION_JCC) {
                jumpsX86++;
                i = j;
                continue;
            }

            i = j;
        }

        // ARM
        if (((i & 3) != 0) || (i + 4 > codeEnd))
            continue;

        const int instr = LittleEndian::readInt32(&src[i]);
        const int opcode1 = instr & ARM_B_OPCODE_MASK;
        const int opcode2 = instr & ARM_CB_OPCODE_MASK;

        if ((opcode1 == ARM_OPCODE_B) || (opcode1 == ARM_OPCODE_BL) ||
             (opcode2 == ARM_OPCODE_CBZ) || (opcode2 == ARM_OPCODE_CBNZ))
            jumpsARM64++;
    }

    Global::DataType dt = Global::detectSimpleType(count, histo);

    if (dt != Global::BIN)
        return NOT_EXE | kanzi::byte(dt);

    // Filter out (some/many) multimedia files
    if ((histo[0] < uint(count / 10)) || (histo[255] < uint(count / 100)))
        return NOT_EXE | kanzi::byte(dt);

    int smallVals = 0;

    for (int i = 0; i < 16; i++)
        smallVals += histo[i];

    if (smallVals > (count / 2))
        return NOT_EXE | kanzi::byte(dt);

    // Ad-hoc thresholds
    if (jumpsX86 >= (count / 200))
        return X86;

    if (jumpsARM64 >= (count / 200))
        return ARM64;

    // Number of jump instructions too small => either not an exe or not worth the change, skip.
    return NOT_EXE | kanzi::byte(dt);
}

static bool setCodeRange(int count, int& codeStart, int& codeEnd, int64 start, int64 length)
{
    if ((start < 0) || (length < 0) || (start > int64(count)) || (length > int64(count) - start))
        return false;

    if (codeStart == 0)
        codeStart = int(start);

    codeEnd = int(start + length);
    return true;
}

// Return true if known header
bool EXECodec::parseHeader(const kanzi::byte src[], int count, uint magic, int& arch, int& codeStart, int& codeEnd)
{

    if (magic == Magic::WIN_MAGIC) {
        if (count >= 64) {
            const int posPE = LittleEndian::readInt32(&src[60]);

            if ((posPE > 0) && (posPE <= count - 48) && (LittleEndian::readInt32(&src[posPE]) == WIN_PE)) {
                const kanzi::byte* pe = &src[posPE];

                if (setCodeRange(count, codeStart, codeEnd,
                                 int64(LittleEndian::readInt32(&pe[44])),
                                 int64(LittleEndian::readInt32(&pe[28]))) == false)
                    return false;

                arch = LittleEndian::readInt16(&pe[4]);
            }

            return true;
        }
    } else if (magic == Magic::ELF_MAGIC) {
        bool isLittleEndian = src[5] == kanzi::byte(1);

        if (count >= 64) {
            codeStart = 0;

            if (isLittleEndian == true) {
                if (src[4] == kanzi::byte(2)) {
                    // 64 bits
                    const int nbEntries = int(LittleEndian::readInt16(&src[0x3C]));
                    const int szEntry = int(LittleEndian::readInt16(&src[0x3A]));
                    const int64 posSection = LittleEndian::readLong64(&src[0x28]);

                    if ((szEntry <= 0) || (posSection < 0) || (posSection > int64(count) - 0x28))
                        return false;

                    for (int i = 0; i < nbEntries; i++) {
                        const int64 startEntry = posSection + int64(i) * int64(szEntry);

                        if (startEntry > int64(count) - 0x28)
                            return false;

                        int typeSection = int(LittleEndian::readInt32(&src[startEntry + 4]));
                        const int64 offSection = LittleEndian::readLong64(&src[startEntry + 0x18]);
                        const int64 lenSection = LittleEndian::readLong64(&src[startEntry + 0x20]);

                        if ((typeSection == 1) && (lenSection >= 64)) {
                            if (setCodeRange(count, codeStart, codeEnd, offSection, lenSection) == false)
                                return false;
                        }
                    }
                } else {
                    // 32 bits
                    const int nbEntries = int(LittleEndian::readInt16(&src[0x30]));
                    const int szEntry = int(LittleEndian::readInt16(&src[0x2E]));
                    const int64 posSection = int64(LittleEndian::readInt32(&src[0x20]));

                    if ((szEntry <= 0) || (posSection < 0) || (posSection > int64(count) - 0x18))
                        return false;

                    for (int i = 0; i < nbEntries; i++) {
                        const int64 startEntry = posSection + int64(i) * int64(szEntry);

                        if (startEntry > int64(count) - 0x18)
                            return false;

                        int typeSection = int(LittleEndian::readInt32(&src[startEntry + 4]));
                        const int64 offSection = int64(LittleEndian::readInt32(&src[startEntry + 0x10]));
                        const int64 lenSection = int64(LittleEndian::readInt32(&src[startEntry + 0x14]));

                        if ((typeSection == 1) && (lenSection >= 64)) {
                            if (setCodeRange(count, codeStart, codeEnd, offSection, lenSection) == false)
                                return false;
                        }
                    }
                }

                arch = LittleEndian::readInt16(&src[18]);
            } else {
                if (src[4] == kanzi::byte(2)) {
                    // 64 bits
                    const int nbEntries = int(BigEndian::readInt16(&src[0x3C]));
                    const int szEntry = int(BigEndian::readInt16(&src[0x3A]));
                    const int64 posSection = BigEndian::readLong64(&src[0x28]);

                    if ((szEntry <= 0) || (posSection < 0) || (posSection > int64(count) - 0x28))
                        return false;

                    for (int i = 0; i < nbEntries; i++) {
                        const int64 startEntry = posSection + int64(i) * int64(szEntry);

                        if (startEntry > int64(count) - 0x28)
                            return false;

                        int typeSection = int(BigEndian::readInt32(&src[startEntry + 4]));
                        const int64 offSection = BigEndian::readLong64(&src[startEntry + 0x18]);
                        const int64 lenSection = BigEndian::readLong64(&src[startEntry + 0x20]);

                        if ((typeSection == 1) && (lenSection >= 64)) {
                            if (setCodeRange(count, codeStart, codeEnd, offSection, lenSection) == false)
                                return false;
                        }
                    }
                } else {
                    // 32 bits
                    const int nbEntries = int(BigEndian::readInt16(&src[0x30]));
                    const int szEntry = int(BigEndian::readInt16(&src[0x2E]));
                    const int64 posSection = int64(BigEndian::readInt32(&src[0x20]));

                    if ((szEntry <= 0) || (posSection < 0) || (posSection > int64(count) - 0x18))
                        return false;

                    for (int i = 0; i < nbEntries; i++) {
                        const int64 startEntry = posSection + int64(i) * int64(szEntry);

                        if (startEntry > int64(count) - 0x18)
                            return false;

                        int typeSection = int(BigEndian::readInt32(&src[startEntry + 4]));
                        const int64 offSection = int64(BigEndian::readInt32(&src[startEntry + 0x10]));
                        const int64 lenSection = int64(BigEndian::readInt32(&src[startEntry + 0x14]));

                        if ((typeSection == 1) && (lenSection >= 64)) {
                            if (setCodeRange(count, codeStart, codeEnd, offSection, lenSection) == false)
                                return false;
                        }
                    }
                }

                arch = BigEndian::readInt16(&src[18]);
            }

            codeStart = min(codeStart, count);
            codeEnd = min(codeEnd, count);
            return true;
        }
    } else if ((magic == Magic::MAC_MAGIC32) || (magic == Magic::MAC_CIGAM32) || (magic == Magic::MAC_MAGIC64) || (magic == Magic::MAC_CIGAM64)) {

        bool is64Bits = (magic == Magic::MAC_MAGIC64) || (magic == Magic::MAC_CIGAM64);
        codeStart = 0;
        static char MAC_TEXT_SEGMENT[] = "__TEXT";
        static char MAC_TEXT_SECTION[] = "__text";

        if (count >= 64) {
            int type = LittleEndian::readInt32(&src[12]);

            if (type != MAC_MH_EXECUTE)
                return false;

            arch = LittleEndian::readInt32(&src[4]);
            int nbCmds = LittleEndian::readInt32(&src[0x10]);
            int pos = (is64Bits == true) ? 0x20 : 0x1C;
            int cmd = 0;

            while (cmd < nbCmds) {
                if ((pos < 0) || (pos > count - 8))
                    return false;

                int ldCmd = LittleEndian::readInt32(&src[pos]);
                int szCmd = LittleEndian::readInt32(&src[pos + 4]);
                int szSegHdr = (is64Bits == true) ? 0x48 : 0x38;

                if ((szCmd < 8) || (szCmd > count - pos))
                    return false;

                if ((ldCmd == MAC_LC_SEGMENT) || (ldCmd == MAC_LC_SEGMENT64)) {
                    if ((pos > count - 14) || (pos > count - szSegHdr))
                        return false;

                    if (memcmp(&src[pos + 8], reinterpret_cast<kanzi::byte*>(MAC_TEXT_SEGMENT), 6) == 0) {
                        int posSection = pos + szSegHdr;

                        const int minSectionSize = (is64Bits == true) ? 0x38 : 0x30;

                        if (posSection > count - minSectionSize)
                            return false;

                        if (memcmp(&src[posSection], reinterpret_cast<kanzi::byte*>(MAC_TEXT_SECTION), 6) == 0) {
                            // Text section in TEXT segment
                            if (is64Bits == true) {
                                if (setCodeRange(count, codeStart, codeEnd,
                                                 LittleEndian::readLong64(&src[posSection + 0x30]),
                                                 int64(LittleEndian::readInt32(&src[posSection + 0x28]))) == false)
                                    return false;

                                break;
                            } else {
                                if (setCodeRange(count, codeStart, codeEnd,
                                                 int64(LittleEndian::readInt32(&src[posSection + 0x2C])),
                                                 int64(LittleEndian::readInt32(&src[posSection + 0x28]))) == false)
                                    return false;

                                break;
                            }
                        }
                    }
                }

                cmd++;
                pos += szCmd;
            }

            codeStart = min(codeStart, count);
            codeEnd = min(codeEnd, count);
            return true;
        }
    }

    return false;
}

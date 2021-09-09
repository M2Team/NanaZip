// Rar3Vm.cpp
// According to unRAR license, this code may not be used to develop
// a program that creates RAR archives

/*
Note:
  Due to performance considerations Rar VM may set Flags C incorrectly
  for some operands (SHL x, 0, ... ).
  Check implementation of concrete VM command
  to see if it sets flags right.
*/

#include "StdAfx.h"

#include <stdlib.h>

#include "../../../C/7zCrc.h"
#include "../../../C/Alloc.h"

#include "../../Common/Defs.h"

#include "Rar3Vm.h"

namespace NCompress {
namespace NRar3 {

UInt32 CMemBitDecoder::ReadBits(unsigned numBits)
{
  UInt32 res = 0;
  for (;;)
  {
    unsigned b = _bitPos < _bitSize ? (unsigned)_data[_bitPos >> 3] : 0;
    unsigned avail = (unsigned)(8 - (_bitPos & 7));
    if (numBits <= avail)
    {
      _bitPos += numBits;
      return res | ((b >> (avail - numBits)) & ((1 << numBits) - 1));
    }
    numBits -= avail;
    res |= (UInt32)(b & ((1 << avail) - 1)) << numBits;
    _bitPos += avail;
  }
}

UInt32 CMemBitDecoder::ReadBit() { return ReadBits(1); }

UInt32 CMemBitDecoder::ReadEncodedUInt32()
{
  unsigned v = (unsigned)ReadBits(2);
  UInt32 res = ReadBits(4 << v);
  if (v == 1 && res < 16)
    res = 0xFFFFFF00 | (res << 4) | ReadBits(4);
  return res;
}

namespace NVm {

static const UInt32 kStackRegIndex = kNumRegs - 1;

#ifdef RARVM_VM_ENABLE

static const UInt32 FLAG_C = 1;
static const UInt32 FLAG_Z = 2;
static const UInt32 FLAG_S = 0x80000000;

static const Byte CF_OP0 = 0;
static const Byte CF_OP1 = 1;
static const Byte CF_OP2 = 2;
static const Byte CF_OPMASK = 3;
static const Byte CF_BYTEMODE = 4;
static const Byte CF_JUMP = 8;
static const Byte CF_PROC = 16;
static const Byte CF_USEFLAGS = 32;
static const Byte CF_CHFLAGS = 64;

static const Byte kCmdFlags[]=
{
  /* CMD_MOV   */ CF_OP2 | CF_BYTEMODE,
  /* CMD_CMP   */ CF_OP2 | CF_BYTEMODE | CF_CHFLAGS,
  /* CMD_ADD   */ CF_OP2 | CF_BYTEMODE | CF_CHFLAGS,
  /* CMD_SUB   */ CF_OP2 | CF_BYTEMODE | CF_CHFLAGS,
  /* CMD_JZ    */ CF_OP1 | CF_JUMP | CF_USEFLAGS,
  /* CMD_JNZ   */ CF_OP1 | CF_JUMP | CF_USEFLAGS,
  /* CMD_INC   */ CF_OP1 | CF_BYTEMODE | CF_CHFLAGS,
  /* CMD_DEC   */ CF_OP1 | CF_BYTEMODE | CF_CHFLAGS,
  /* CMD_JMP   */ CF_OP1 | CF_JUMP,
  /* CMD_XOR   */ CF_OP2 | CF_BYTEMODE | CF_CHFLAGS,
  /* CMD_AND   */ CF_OP2 | CF_BYTEMODE | CF_CHFLAGS,
  /* CMD_OR    */ CF_OP2 | CF_BYTEMODE | CF_CHFLAGS,
  /* CMD_TEST  */ CF_OP2 | CF_BYTEMODE | CF_CHFLAGS,
  /* CMD_JS    */ CF_OP1 | CF_JUMP | CF_USEFLAGS,
  /* CMD_JNS   */ CF_OP1 | CF_JUMP | CF_USEFLAGS,
  /* CMD_JB    */ CF_OP1 | CF_JUMP | CF_USEFLAGS,
  /* CMD_JBE   */ CF_OP1 | CF_JUMP | CF_USEFLAGS,
  /* CMD_JA    */ CF_OP1 | CF_JUMP | CF_USEFLAGS,
  /* CMD_JAE   */ CF_OP1 | CF_JUMP | CF_USEFLAGS,
  /* CMD_PUSH  */ CF_OP1,
  /* CMD_POP   */ CF_OP1,
  /* CMD_CALL  */ CF_OP1 | CF_PROC,
  /* CMD_RET   */ CF_OP0 | CF_PROC,
  /* CMD_NOT   */ CF_OP1 | CF_BYTEMODE,
  /* CMD_SHL   */ CF_OP2 | CF_BYTEMODE | CF_CHFLAGS,
  /* CMD_SHR   */ CF_OP2 | CF_BYTEMODE | CF_CHFLAGS,
  /* CMD_SAR   */ CF_OP2 | CF_BYTEMODE | CF_CHFLAGS,
  /* CMD_NEG   */ CF_OP1 | CF_BYTEMODE | CF_CHFLAGS,
  /* CMD_PUSHA */ CF_OP0,
  /* CMD_POPA  */ CF_OP0,
  /* CMD_PUSHF */ CF_OP0 | CF_USEFLAGS,
  /* CMD_POPF  */ CF_OP0 | CF_CHFLAGS,
  /* CMD_MOVZX */ CF_OP2,
  /* CMD_MOVSX */ CF_OP2,
  /* CMD_XCHG  */ CF_OP2 | CF_BYTEMODE,
  /* CMD_MUL   */ CF_OP2 | CF_BYTEMODE,
  /* CMD_DIV   */ CF_OP2 | CF_BYTEMODE,
  /* CMD_ADC   */ CF_OP2 | CF_BYTEMODE | CF_USEFLAGS | CF_CHFLAGS ,
  /* CMD_SBB   */ CF_OP2 | CF_BYTEMODE | CF_USEFLAGS | CF_CHFLAGS ,
  /* CMD_PRINT */ CF_OP0
};

#endif


CVm::CVm(): Mem(NULL) {}

bool CVm::Create()
{
  if (!Mem)
    Mem = (Byte *)::MyAlloc(kSpaceSize + 4);
  return (Mem != NULL);
}

CVm::~CVm()
{
  ::MyFree(Mem);
}

// CVm::Execute can change CProgram object: it clears progarm if VM returns error.

bool CVm::Execute(CProgram *prg, const CProgramInitState *initState,
    CBlockRef &outBlockRef, CRecordVector<Byte> &outGlobalData)
{
  memcpy(R, initState->InitR, sizeof(initState->InitR));
  R[kStackRegIndex] = kSpaceSize;
  R[kNumRegs] = 0;
  Flags = 0;

  UInt32 globalSize = MyMin((UInt32)initState->GlobalData.Size(), kGlobalSize);
  if (globalSize != 0)
    memcpy(Mem + kGlobalOffset, &initState->GlobalData[0], globalSize);
  UInt32 staticSize = MyMin((UInt32)prg->StaticData.Size(), kGlobalSize - globalSize);
  if (staticSize != 0)
    memcpy(Mem + kGlobalOffset + globalSize, &prg->StaticData[0], staticSize);

  bool res = true;
  
  #ifdef RARVM_STANDARD_FILTERS
  if (prg->StandardFilterIndex >= 0)
    res = ExecuteStandardFilter(prg->StandardFilterIndex);
  else
  #endif
  {
    #ifdef RARVM_VM_ENABLE
    res = ExecuteCode(prg);
    if (!res)
    {
      prg->Commands.Clear();
      prg->Commands.Add(CCommand());
      prg->Commands.Back().OpCode = CMD_RET;
    }
    #else
    res = false;
    #endif
  }
  
  UInt32 newBlockPos = GetFixedGlobalValue32(NGlobalOffset::kBlockPos) & kSpaceMask;
  UInt32 newBlockSize = GetFixedGlobalValue32(NGlobalOffset::kBlockSize) & kSpaceMask;
  if (newBlockPos + newBlockSize >= kSpaceSize)
    newBlockPos = newBlockSize = 0;
  outBlockRef.Offset = newBlockPos;
  outBlockRef.Size = newBlockSize;

  outGlobalData.Clear();
  UInt32 dataSize = GetFixedGlobalValue32(NGlobalOffset::kGlobalMemOutSize);
  dataSize = MyMin(dataSize, kGlobalSize - kFixedGlobalSize);
  if (dataSize != 0)
  {
    dataSize += kFixedGlobalSize;
    outGlobalData.ClearAndSetSize(dataSize);
    memcpy(&outGlobalData[0], Mem + kGlobalOffset, dataSize);
  }

  return res;
}

#ifdef RARVM_VM_ENABLE

#define SET_IP(IP) \
  if ((IP) >= numCommands) return true; \
  if (--maxOpCount <= 0) return false; \
  cmd = commands + (IP);

#define GET_FLAG_S_B(res) (((res) & 0x80) ? FLAG_S : 0)
#define SET_IP_OP1 { UInt32 val = GetOperand32(&cmd->Op1); SET_IP(val); }
#define FLAGS_UPDATE_SZ Flags = res == 0 ? FLAG_Z : res & FLAG_S
#define FLAGS_UPDATE_SZ_B Flags = (res & 0xFF) == 0 ? FLAG_Z : GET_FLAG_S_B(res)

UInt32 CVm::GetOperand32(const COperand *op) const
{
  switch (op->Type)
  {
    case OP_TYPE_REG: return R[op->Data];
    case OP_TYPE_REGMEM: return GetValue32(&Mem[(op->Base + R[op->Data]) & kSpaceMask]);
    default: return op->Data;
  }
}

void CVm::SetOperand32(const COperand *op, UInt32 val)
{
  switch (op->Type)
  {
    case OP_TYPE_REG: R[op->Data] = val; return;
    case OP_TYPE_REGMEM: SetValue32(&Mem[(op->Base + R[op->Data]) & kSpaceMask], val); return;
  }
}

Byte CVm::GetOperand8(const COperand *op) const
{
  switch (op->Type)
  {
    case OP_TYPE_REG: return (Byte)R[op->Data];
    case OP_TYPE_REGMEM: return Mem[(op->Base + R[op->Data]) & kSpaceMask];;
    default: return (Byte)op->Data;
  }
}

void CVm::SetOperand8(const COperand *op, Byte val)
{
  switch (op->Type)
  {
    case OP_TYPE_REG: R[op->Data] = (R[op->Data] & 0xFFFFFF00) | val; return;
    case OP_TYPE_REGMEM: Mem[(op->Base + R[op->Data]) & kSpaceMask] = val; return;
  }
}

UInt32 CVm::GetOperand(bool byteMode, const COperand *op) const
{
  if (byteMode)
    return GetOperand8(op);
  return GetOperand32(op);
}

void CVm::SetOperand(bool byteMode, const COperand *op, UInt32 val)
{
  if (byteMode)
    SetOperand8(op, (Byte)(val & 0xFF));
  else
    SetOperand32(op, val);
}

bool CVm::ExecuteCode(const CProgram *prg)
{
  Int32 maxOpCount = 25000000;
  const CCommand *commands = &prg->Commands[0];
  const CCommand *cmd = commands;
  UInt32 numCommands = prg->Commands.Size();
  if (numCommands == 0)
    return false;

  for (;;)
  {
    switch (cmd->OpCode)
    {
      case CMD_MOV:
        SetOperand32(&cmd->Op1, GetOperand32(&cmd->Op2));
        break;
      case CMD_MOVB:
        SetOperand8(&cmd->Op1, GetOperand8(&cmd->Op2));
        break;
      case CMD_CMP:
        {
          UInt32 v1 = GetOperand32(&cmd->Op1);
          UInt32 res = v1 - GetOperand32(&cmd->Op2);
          Flags = res == 0 ? FLAG_Z : (res > v1) | (res & FLAG_S);
        }
        break;
      case CMD_CMPB:
        {
          Byte v1 = GetOperand8(&cmd->Op1);
          Byte res = (Byte)((v1 - GetOperand8(&cmd->Op2)) & 0xFF);
          Flags = res == 0 ? FLAG_Z : (res > v1) | GET_FLAG_S_B(res);
        }
        break;
      case CMD_ADD:
        {
          UInt32 v1 = GetOperand32(&cmd->Op1);
          UInt32 res = v1 + GetOperand32(&cmd->Op2);
          SetOperand32(&cmd->Op1, res);
          Flags = (res < v1) | (res == 0 ? FLAG_Z : (res & FLAG_S));
        }
        break;
      case CMD_ADDB:
        {
          Byte v1 = GetOperand8(&cmd->Op1);
          Byte res = (Byte)((v1 + GetOperand8(&cmd->Op2)) & 0xFF);
          SetOperand8(&cmd->Op1, (Byte)res);
          Flags = (res < v1) | (res == 0 ? FLAG_Z : GET_FLAG_S_B(res));
        }
        break;
      case CMD_ADC:
        {
          UInt32 v1 = GetOperand(cmd->ByteMode, &cmd->Op1);
          UInt32 FC = (Flags & FLAG_C);
          UInt32 res = v1 + GetOperand(cmd->ByteMode, &cmd->Op2) + FC;
          if (cmd->ByteMode)
            res &= 0xFF;
          SetOperand(cmd->ByteMode, &cmd->Op1, res);
          Flags = (res < v1 || res == v1 && FC) | (res == 0 ? FLAG_Z : (res & FLAG_S));
        }
        break;
      case CMD_SUB:
        {
          UInt32 v1 = GetOperand32(&cmd->Op1);
          UInt32 res = v1 - GetOperand32(&cmd->Op2);
          SetOperand32(&cmd->Op1, res);
          Flags = res == 0 ? FLAG_Z : (res > v1) | (res & FLAG_S);
        }
        break;
      case CMD_SUBB:
        {
          UInt32 v1 = GetOperand8(&cmd->Op1);
          UInt32 res = v1 - GetOperand8(&cmd->Op2);
          SetOperand8(&cmd->Op1, (Byte)res);
          Flags = res == 0 ? FLAG_Z : (res > v1) | (res & FLAG_S);
        }
        break;
      case CMD_SBB:
        {
          UInt32 v1 = GetOperand(cmd->ByteMode, &cmd->Op1);
          UInt32 FC = (Flags & FLAG_C);
          UInt32 res = v1 - GetOperand(cmd->ByteMode, &cmd->Op2) - FC;
          // Flags = res == 0 ? FLAG_Z : (res > v1 || res == v1 && FC) | (res & FLAG_S);
          if (cmd->ByteMode)
            res &= 0xFF;
          SetOperand(cmd->ByteMode, &cmd->Op1, res);
          Flags = (res > v1 || res == v1 && FC) | (res == 0 ? FLAG_Z : (res & FLAG_S));
        }
        break;
      case CMD_INC:
        {
          UInt32 res = GetOperand32(&cmd->Op1) + 1;
          SetOperand32(&cmd->Op1, res);
          FLAGS_UPDATE_SZ;
        }
        break;
      case CMD_INCB:
        {
          Byte res = (Byte)(GetOperand8(&cmd->Op1) + 1);
          SetOperand8(&cmd->Op1, res);;
          FLAGS_UPDATE_SZ_B;
        }
        break;
      case CMD_DEC:
        {
          UInt32 res = GetOperand32(&cmd->Op1) - 1;
          SetOperand32(&cmd->Op1, res);
          FLAGS_UPDATE_SZ;
        }
        break;
      case CMD_DECB:
        {
          Byte res = (Byte)(GetOperand8(&cmd->Op1) - 1);
          SetOperand8(&cmd->Op1, res);;
          FLAGS_UPDATE_SZ_B;
        }
        break;
      case CMD_XOR:
        {
          UInt32 res = GetOperand32(&cmd->Op1) ^ GetOperand32(&cmd->Op2);
          SetOperand32(&cmd->Op1, res);
          FLAGS_UPDATE_SZ;
        }
        break;
      case CMD_XORB:
        {
          Byte res = (Byte)(GetOperand8(&cmd->Op1) ^ GetOperand8(&cmd->Op2));
          SetOperand8(&cmd->Op1, res);
          FLAGS_UPDATE_SZ_B;
        }
        break;
      case CMD_AND:
        {
          UInt32 res = GetOperand32(&cmd->Op1) & GetOperand32(&cmd->Op2);
          SetOperand32(&cmd->Op1, res);
          FLAGS_UPDATE_SZ;
        }
        break;
      case CMD_ANDB:
        {
          Byte res = (Byte)(GetOperand8(&cmd->Op1) & GetOperand8(&cmd->Op2));
          SetOperand8(&cmd->Op1, res);
          FLAGS_UPDATE_SZ_B;
        }
        break;
      case CMD_OR:
        {
          UInt32 res = GetOperand32(&cmd->Op1) | GetOperand32(&cmd->Op2);
          SetOperand32(&cmd->Op1, res);
          FLAGS_UPDATE_SZ;
        }
        break;
      case CMD_ORB:
        {
          Byte res = (Byte)(GetOperand8(&cmd->Op1) | GetOperand8(&cmd->Op2));
          SetOperand8(&cmd->Op1, res);
          FLAGS_UPDATE_SZ_B;
        }
        break;
      case CMD_TEST:
        {
          UInt32 res = GetOperand32(&cmd->Op1) & GetOperand32(&cmd->Op2);
          FLAGS_UPDATE_SZ;
        }
        break;
      case CMD_TESTB:
        {
          Byte res = (Byte)(GetOperand8(&cmd->Op1) & GetOperand8(&cmd->Op2));
          FLAGS_UPDATE_SZ_B;
        }
        break;
      case CMD_NOT:
        SetOperand(cmd->ByteMode, &cmd->Op1, ~GetOperand(cmd->ByteMode, &cmd->Op1));
        break;
      case CMD_NEG:
        {
          UInt32 res = 0 - GetOperand32(&cmd->Op1);
          SetOperand32(&cmd->Op1, res);
          Flags = res == 0 ? FLAG_Z : FLAG_C | (res & FLAG_S);
        }
        break;
      case CMD_NEGB:
        {
          Byte res = (Byte)(0 - GetOperand8(&cmd->Op1));
          SetOperand8(&cmd->Op1, res);
          Flags = res == 0 ? FLAG_Z : FLAG_C | GET_FLAG_S_B(res);
        }
        break;

      case CMD_SHL:
        {
          UInt32 v1 = GetOperand32(&cmd->Op1);
          int v2 = (int)GetOperand32(&cmd->Op2);
          UInt32 res = v1 << v2;
          SetOperand32(&cmd->Op1, res);
          Flags = (res == 0 ? FLAG_Z : (res & FLAG_S)) | ((v1 << (v2 - 1)) & 0x80000000 ? FLAG_C : 0);
        }
        break;
      case CMD_SHLB:
        {
          Byte v1 = GetOperand8(&cmd->Op1);
          int v2 = (int)GetOperand8(&cmd->Op2);
          Byte res = (Byte)(v1 << v2);
          SetOperand8(&cmd->Op1, res);
          Flags = (res == 0 ? FLAG_Z : GET_FLAG_S_B(res)) | ((v1 << (v2 - 1)) & 0x80 ? FLAG_C : 0);
        }
        break;
      case CMD_SHR:
        {
          UInt32 v1 = GetOperand32(&cmd->Op1);
          int v2 = (int)GetOperand32(&cmd->Op2);
          UInt32 res = v1 >> v2;
          SetOperand32(&cmd->Op1, res);
          Flags = (res == 0 ? FLAG_Z : (res & FLAG_S)) | ((v1 >> (v2 - 1)) & FLAG_C);
        }
        break;
      case CMD_SHRB:
        {
          Byte v1 = GetOperand8(&cmd->Op1);
          int v2 = (int)GetOperand8(&cmd->Op2);
          Byte res = (Byte)(v1 >> v2);
          SetOperand8(&cmd->Op1, res);
          Flags = (res == 0 ? FLAG_Z : GET_FLAG_S_B(res)) | ((v1 >> (v2 - 1)) & FLAG_C);
        }
        break;
      case CMD_SAR:
        {
          UInt32 v1 = GetOperand32(&cmd->Op1);
          int v2 = (int)GetOperand32(&cmd->Op2);
          UInt32 res = UInt32(((Int32)v1) >> v2);
          SetOperand32(&cmd->Op1, res);
          Flags= (res == 0 ? FLAG_Z : (res & FLAG_S)) | ((v1 >> (v2 - 1)) & FLAG_C);
        }
        break;
      case CMD_SARB:
        {
          Byte v1 = GetOperand8(&cmd->Op1);
          int v2 = (int)GetOperand8(&cmd->Op2);
          Byte res = (Byte)(((signed char)v1) >> v2);
          SetOperand8(&cmd->Op1, res);
          Flags= (res == 0 ? FLAG_Z : GET_FLAG_S_B(res)) | ((v1 >> (v2 - 1)) & FLAG_C);
        }
        break;

      case CMD_JMP:
        SET_IP_OP1;
        continue;
      case CMD_JZ:
        if ((Flags & FLAG_Z) != 0)
        {
          SET_IP_OP1;
          continue;
        }
        break;
      case CMD_JNZ:
        if ((Flags & FLAG_Z) == 0)
        {
          SET_IP_OP1;
          continue;
        }
        break;
      case CMD_JS:
        if ((Flags & FLAG_S) != 0)
        {
          SET_IP_OP1;
          continue;
        }
        break;
      case CMD_JNS:
        if ((Flags & FLAG_S) == 0)
        {
          SET_IP_OP1;
          continue;
        }
        break;
      case CMD_JB:
        if ((Flags & FLAG_C) != 0)
        {
          SET_IP_OP1;
          continue;
        }
        break;
      case CMD_JBE:
        if ((Flags & (FLAG_C | FLAG_Z)) != 0)
        {
          SET_IP_OP1;
          continue;
        }
        break;
      case CMD_JA:
        if ((Flags & (FLAG_C | FLAG_Z)) == 0)
        {
          SET_IP_OP1;
          continue;
        }
        break;
      case CMD_JAE:
        if ((Flags & FLAG_C) == 0)
        {
          SET_IP_OP1;
          continue;
        }
        break;
      
      case CMD_PUSH:
        R[kStackRegIndex] -= 4;
        SetValue32(&Mem[R[kStackRegIndex] & kSpaceMask], GetOperand32(&cmd->Op1));
        break;
      case CMD_POP:
        SetOperand32(&cmd->Op1, GetValue32(&Mem[R[kStackRegIndex] & kSpaceMask]));
        R[kStackRegIndex] += 4;
        break;
      case CMD_CALL:
        R[kStackRegIndex] -= 4;
        SetValue32(&Mem[R[kStackRegIndex] & kSpaceMask], (UInt32)(cmd - commands + 1));
        SET_IP_OP1;
        continue;

      case CMD_PUSHA:
        {
          for (UInt32 i = 0, SP = R[kStackRegIndex] - 4; i < kNumRegs; i++, SP -= 4)
            SetValue32(&Mem[SP & kSpaceMask], R[i]);
          R[kStackRegIndex] -= kNumRegs * 4;
        }
        break;
      case CMD_POPA:
        {
          for (UInt32 i = 0, SP = R[kStackRegIndex]; i < kNumRegs; i++, SP += 4)
            R[kStackRegIndex - i] = GetValue32(&Mem[SP & kSpaceMask]);
        }
        break;
      case CMD_PUSHF:
        R[kStackRegIndex] -= 4;
        SetValue32(&Mem[R[kStackRegIndex]&kSpaceMask], Flags);
        break;
      case CMD_POPF:
        Flags = GetValue32(&Mem[R[kStackRegIndex] & kSpaceMask]);
        R[kStackRegIndex] += 4;
        break;
      
      case CMD_MOVZX:
        SetOperand32(&cmd->Op1, GetOperand8(&cmd->Op2));
        break;
      case CMD_MOVSX:
        SetOperand32(&cmd->Op1, (UInt32)(Int32)(signed char)GetOperand8(&cmd->Op2));
        break;
      case CMD_XCHG:
        {
          UInt32 v1 = GetOperand(cmd->ByteMode, &cmd->Op1);
          SetOperand(cmd->ByteMode, &cmd->Op1, GetOperand(cmd->ByteMode, &cmd->Op2));
          SetOperand(cmd->ByteMode, &cmd->Op2, v1);
        }
        break;
      case CMD_MUL:
        {
          UInt32 res = GetOperand32(&cmd->Op1) * GetOperand32(&cmd->Op2);
          SetOperand32(&cmd->Op1, res);
        }
        break;
      case CMD_MULB:
        {
          Byte res = (Byte)(GetOperand8(&cmd->Op1) * GetOperand8(&cmd->Op2));
          SetOperand8(&cmd->Op1, res);
        }
        break;
      case CMD_DIV:
        {
          UInt32 divider = GetOperand(cmd->ByteMode, &cmd->Op2);
          if (divider != 0)
          {
            UInt32 res = GetOperand(cmd->ByteMode, &cmd->Op1) / divider;
            SetOperand(cmd->ByteMode, &cmd->Op1, res);
          }
        }
        break;
      
      case CMD_RET:
        {
          if (R[kStackRegIndex] >= kSpaceSize)
            return true;
          UInt32 ip = GetValue32(&Mem[R[kStackRegIndex] & kSpaceMask]);
          SET_IP(ip);
          R[kStackRegIndex] += 4;
          continue;
        }
      case CMD_PRINT:
        break;
    }
    cmd++;
    --maxOpCount;
  }
}

//////////////////////////////////////////////////////
// Read program

static void DecodeArg(CMemBitDecoder &inp, COperand &op, bool byteMode)
{
  if (inp.ReadBit())
  {
    op.Type = OP_TYPE_REG;
    op.Data = inp.ReadBits(kNumRegBits);
  }
  else if (inp.ReadBit() == 0)
  {
    op.Type = OP_TYPE_INT;
    if (byteMode)
      op.Data = inp.ReadBits(8);
    else
      op.Data = inp.ReadEncodedUInt32();
  }
  else
  {
    op.Type = OP_TYPE_REGMEM;
    if (inp.ReadBit() == 0)
    {
      op.Data = inp.ReadBits(kNumRegBits);
      op.Base = 0;
    }
    else
    {
      if (inp.ReadBit() == 0)
        op.Data = inp.ReadBits(kNumRegBits);
      else
        op.Data = kNumRegs;
      op.Base = inp.ReadEncodedUInt32();
    }
  }
}

void CProgram::ReadProgram(const Byte *code, UInt32 codeSize)
{
  CMemBitDecoder inp;
  inp.Init(code, codeSize);

  StaticData.Clear();
  
  if (inp.ReadBit())
  {
    UInt32 dataSize = inp.ReadEncodedUInt32() + 1;
    for (UInt32 i = 0; inp.Avail() && i < dataSize; i++)
      StaticData.Add((Byte)inp.ReadBits(8));
  }
  
  while (inp.Avail())
  {
    Commands.Add(CCommand());
    CCommand *cmd = &Commands.Back();
    
    if (inp.ReadBit() == 0)
      cmd->OpCode = (ECommand)inp.ReadBits(3);
    else
      cmd->OpCode = (ECommand)(8 + inp.ReadBits(5));

    if (kCmdFlags[(unsigned)cmd->OpCode] & CF_BYTEMODE)
      cmd->ByteMode = (inp.ReadBit()) ? true : false;
    else
      cmd->ByteMode = 0;
    
    int opNum = (kCmdFlags[(unsigned)cmd->OpCode] & CF_OPMASK);
    
    if (opNum > 0)
    {
      DecodeArg(inp, cmd->Op1, cmd->ByteMode);
      if (opNum == 2)
        DecodeArg(inp, cmd->Op2, cmd->ByteMode);
      else
      {
        if (cmd->Op1.Type == OP_TYPE_INT && (kCmdFlags[(unsigned)cmd->OpCode] & (CF_JUMP | CF_PROC)))
        {
          int dist = cmd->Op1.Data;
          if (dist >= 256)
            dist -= 256;
          else
          {
            if (dist >= 136)
              dist -= 264;
            else if (dist >= 16)
              dist -= 8;
            else if (dist >= 8)
              dist -= 16;
            dist += Commands.Size() - 1;
          }
          cmd->Op1.Data = dist;
        }
      }
    }

    if (cmd->ByteMode)
    {
      switch (cmd->OpCode)
      {
        case CMD_MOV: cmd->OpCode = CMD_MOVB; break;
        case CMD_CMP: cmd->OpCode = CMD_CMPB; break;
        case CMD_ADD: cmd->OpCode = CMD_ADDB; break;
        case CMD_SUB: cmd->OpCode = CMD_SUBB; break;
        case CMD_INC: cmd->OpCode = CMD_INCB; break;
        case CMD_DEC: cmd->OpCode = CMD_DECB; break;
        case CMD_XOR: cmd->OpCode = CMD_XORB; break;
        case CMD_AND: cmd->OpCode = CMD_ANDB; break;
        case CMD_OR: cmd->OpCode = CMD_ORB; break;
        case CMD_TEST: cmd->OpCode = CMD_TESTB; break;
        case CMD_NEG: cmd->OpCode = CMD_NEGB; break;
        case CMD_SHL: cmd->OpCode = CMD_SHLB; break;
        case CMD_SHR: cmd->OpCode = CMD_SHRB; break;
        case CMD_SAR: cmd->OpCode = CMD_SARB; break;
        case CMD_MUL: cmd->OpCode = CMD_MULB; break;
      }
    }
  }
}

#endif


#ifdef RARVM_STANDARD_FILTERS

enum EStandardFilter
{
  SF_E8,
  SF_E8E9,
  SF_ITANIUM,
  SF_RGB,
  SF_AUDIO,
  SF_DELTA
  // SF_UPCASE
};

static const struct CStandardFilterSignature
{
  UInt32 Length;
  UInt32 CRC;
  EStandardFilter Type;
}
kStdFilters[]=
{
  {  53, 0xad576887, SF_E8 },
  {  57, 0x3cd7e57e, SF_E8E9 },
  { 120, 0x3769893f, SF_ITANIUM },
  {  29, 0x0e06077d, SF_DELTA },
  { 149, 0x1c2c5dc8, SF_RGB },
  { 216, 0xbc85e701, SF_AUDIO }
  // {  40, 0x46b9c560, SF_UPCASE }
};

static int FindStandardFilter(const Byte *code, UInt32 codeSize)
{
  UInt32 crc = CrcCalc(code, codeSize);
  for (unsigned i = 0; i < ARRAY_SIZE(kStdFilters); i++)
  {
    const CStandardFilterSignature &sfs = kStdFilters[i];
    if (sfs.CRC == crc && sfs.Length == codeSize)
      return i;
  }
  return -1;
}

#endif


bool CProgram::PrepareProgram(const Byte *code, UInt32 codeSize)
{
  IsSupported = false;

  #ifdef RARVM_VM_ENABLE
  Commands.Clear();
  #endif
  
  #ifdef RARVM_STANDARD_FILTERS
  StandardFilterIndex = -1;
  #endif

  bool isOK = false;

  Byte xorSum = 0;
  for (UInt32 i = 0; i < codeSize; i++)
    xorSum ^= code[i];

  if (xorSum == 0 && codeSize != 0)
  {
    IsSupported = true;
    isOK = true;
    #ifdef RARVM_STANDARD_FILTERS
    StandardFilterIndex = FindStandardFilter(code, codeSize);
    if (StandardFilterIndex >= 0)
      return true;
    #endif
  
    #ifdef RARVM_VM_ENABLE
    ReadProgram(code + 1, codeSize - 1);
    #else
    IsSupported = false;
    #endif
  }
  
  #ifdef RARVM_VM_ENABLE
  Commands.Add(CCommand());
  Commands.Back().OpCode = CMD_RET;
  #endif
  
  return isOK;
}

void CVm::SetMemory(UInt32 pos, const Byte *data, UInt32 dataSize)
{
  if (pos < kSpaceSize && data != Mem + pos)
    memmove(Mem + pos, data, MyMin(dataSize, kSpaceSize - pos));
}

#ifdef RARVM_STANDARD_FILTERS

static void E8E9Decode(Byte *data, UInt32 dataSize, UInt32 fileOffset, bool e9)
{
  if (dataSize <= 4)
    return;
  dataSize -= 4;
  const UInt32 kFileSize = 0x1000000;
  Byte cmpMask = (Byte)(e9 ? 0xFE : 0xFF);
  for (UInt32 curPos = 0; curPos < dataSize;)
  {
    curPos++;
    if (((*data++) & cmpMask) == 0xE8)
    {
      UInt32 offset = curPos + fileOffset;
      UInt32 addr = GetValue32(data);
      if (addr < kFileSize)
        SetValue32(data, addr - offset);
      else if ((addr & 0x80000000) != 0 && ((addr + offset) & 0x80000000) == 0)
        SetValue32(data, addr + kFileSize);
      data += 4;
      curPos += 4;
    }
  }
}


static void ItaniumDecode(Byte *data, UInt32 dataSize, UInt32 fileOffset)
{
  if (dataSize <= 21)
    return;
  fileOffset >>= 4;
  dataSize -= 21;
  dataSize += 15;
  dataSize >>= 4;
  dataSize += fileOffset;
  do
  {
    unsigned m = ((UInt32)0x334B0000 >> (data[0] & 0x1E)) & 3;
    if (m)
    {
      m++;
      do
      {
        Byte *p = data + ((size_t)m * 5 - 8);
        if (((p[3] >> m) & 15) == 5)
        {
          const UInt32 kMask = 0xFFFFF;
          // UInt32 raw = ((UInt32)p[0]) | ((UInt32)p[1] << 8) | ((UInt32)p[2] << 16);
          UInt32 raw = GetUi32(p);
          UInt32 v = raw >> m;
          v -= fileOffset;
          v &= kMask;
          raw &= ~(kMask << m);
          raw |= (v << m);
          // p[0] = (Byte)raw; p[1] = (Byte)(raw >> 8); p[2] = (Byte)(raw >> 16);
          SetUi32(p, raw);
        }
      }
      while (++m <= 4);
    }
    data += 16;
  }
  while (++fileOffset != dataSize);
}


static void DeltaDecode(Byte *data, UInt32 dataSize, UInt32 numChannels)
{
  UInt32 srcPos = 0;
  const UInt32 border = dataSize * 2;
  for (UInt32 curChannel = 0; curChannel < numChannels; curChannel++)
  {
    Byte prevByte = 0;
    for (UInt32 destPos = dataSize + curChannel; destPos < border; destPos += numChannels)
      data[destPos] = (prevByte = (Byte)(prevByte - data[srcPos++]));
  }
}

static void RgbDecode(Byte *srcData, UInt32 dataSize, UInt32 width, UInt32 posR)
{
  Byte *destData = srcData + dataSize;
  const UInt32 kNumChannels = 3;
  
  for (UInt32 curChannel = 0; curChannel < kNumChannels; curChannel++)
  {
    Byte prevByte = 0;
    
    for (UInt32 i = curChannel; i < dataSize; i += kNumChannels)
    {
      unsigned int predicted;
      if (i < width)
        predicted = prevByte;
      else
      {
        unsigned int upperLeftByte = destData[i - width];
        unsigned int upperByte = destData[i - width + 3];
        predicted = prevByte + upperByte - upperLeftByte;
        int pa = abs((int)(predicted - prevByte));
        int pb = abs((int)(predicted - upperByte));
        int pc = abs((int)(predicted - upperLeftByte));
        if (pa <= pb && pa <= pc)
          predicted = prevByte;
        else
          if (pb <= pc)
            predicted = upperByte;
          else
            predicted = upperLeftByte;
      }
      destData[i] = prevByte = (Byte)(predicted - *(srcData++));
    }
  }
  if (dataSize < 3)
    return;
  const UInt32 border = dataSize - 2;
  for (UInt32 i = posR; i < border; i += 3)
  {
    Byte g = destData[i + 1];
    destData[i    ] = (Byte)(destData[i    ] + g);
    destData[i + 2] = (Byte)(destData[i + 2] + g);
  }
}

static void AudioDecode(Byte *srcData, UInt32 dataSize, UInt32 numChannels)
{
  Byte *destData = srcData + dataSize;
  for (UInt32 curChannel = 0; curChannel < numChannels; curChannel++)
  {
    UInt32 prevByte = 0, prevDelta = 0, dif[7];
    Int32 D1 = 0, D2 = 0, D3;
    Int32 K1 = 0, K2 = 0, K3 = 0;
    memset(dif, 0, sizeof(dif));
    
    for (UInt32 i = curChannel, byteCount = 0; i < dataSize; i += numChannels, byteCount++)
    {
      D3 = D2;
      D2 = prevDelta - D1;
      D1 = prevDelta;
      
      UInt32 predicted = 8 * prevByte + K1 * D1 + K2 * D2 + K3 * D3;
      predicted = (predicted >> 3) & 0xFF;
      
      UInt32 curByte = *(srcData++);
      
      predicted -= curByte;
      destData[i] = (Byte)predicted;
      prevDelta = (UInt32)(Int32)(signed char)(predicted - prevByte);
      prevByte = predicted;
      
      Int32 D = ((Int32)(signed char)curByte) << 3;
      
      dif[0] += abs(D);
      dif[1] += abs(D - D1);
      dif[2] += abs(D + D1);
      dif[3] += abs(D - D2);
      dif[4] += abs(D + D2);
      dif[5] += abs(D - D3);
      dif[6] += abs(D + D3);
      
      if ((byteCount & 0x1F) == 0)
      {
        UInt32 minDif = dif[0], numMinDif = 0;
        dif[0] = 0;
        for (unsigned j = 1; j < ARRAY_SIZE(dif); j++)
        {
          if (dif[j] < minDif)
          {
            minDif = dif[j];
            numMinDif = j;
          }
          dif[j] = 0;
        }
        switch (numMinDif)
        {
          case 1: if (K1 >= -16) K1--; break;
          case 2: if (K1 <   16) K1++; break;
          case 3: if (K2 >= -16) K2--; break;
          case 4: if (K2 <   16) K2++; break;
          case 5: if (K3 >= -16) K3--; break;
          case 6: if (K3 <   16) K3++; break;
        }
      }
    }
  }
}

/*
static UInt32 UpCaseDecode(Byte *data, UInt32 dataSize)
{
  UInt32 srcPos = 0, destPos = dataSize;
  while (srcPos < dataSize)
  {
    Byte curByte = data[srcPos++];
    if (curByte == 2 && (curByte = data[srcPos++]) != 2)
      curByte -= 32;
    data[destPos++] = curByte;
  }
  return destPos - dataSize;
}
*/

bool CVm::ExecuteStandardFilter(unsigned filterIndex)
{
  UInt32 dataSize = R[4];
  if (dataSize >= kGlobalOffset)
    return false;
  EStandardFilter filterType = kStdFilters[filterIndex].Type;

  switch (filterType)
  {
    case SF_E8:
    case SF_E8E9:
      E8E9Decode(Mem, dataSize, R[6], (filterType == SF_E8E9));
      break;
    
    case SF_ITANIUM:
      ItaniumDecode(Mem, dataSize, R[6]);
      break;
    
    case SF_DELTA:
    {
      if (dataSize >= kGlobalOffset / 2)
        return false;
      UInt32 numChannels = R[0];
      if (numChannels == 0 || numChannels > 1024) // unrar 5.5.5
        return false;
      SetBlockPos(dataSize);
      DeltaDecode(Mem, dataSize, numChannels);
      break;
    }
    
    case SF_RGB:
    {
      if (dataSize >= kGlobalOffset / 2 || dataSize < 3) // unrar 5.5.5
        return false;
      UInt32 width = R[0];
      UInt32 posR = R[1];
      if (width < 3 || width - 3 > dataSize || posR > 2) // unrar 5.5.5
        return false;
      SetBlockPos(dataSize);
      RgbDecode(Mem, dataSize, width, posR);
      break;
    }
    
    case SF_AUDIO:
    {
      if (dataSize >= kGlobalOffset / 2)
        return false;
      UInt32 numChannels = R[0];
      if (numChannels == 0 || numChannels > 128) // unrar 5.5.5
        return false;
      SetBlockPos(dataSize);
      AudioDecode(Mem, dataSize, numChannels);
      break;
    }
    
    /*
    case SF_UPCASE:
      if (dataSize >= kGlobalOffset / 2)
        return false;
      UInt32 destSize = UpCaseDecode(Mem, dataSize);
      SetBlockSize(destSize);
      SetBlockPos(dataSize);
      break;
    */
  }
  return true;
}

#endif

}}}

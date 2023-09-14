// LzmsDecoder.cpp
// The code is based on LZMS description from wimlib code

#include "StdAfx.h"

#include "../../../C/Alloc.h"

#include "LzmsDecoder.h"

namespace NCompress {
namespace NLzms {

static UInt32 g_PosBases[k_NumPosSyms /* + 1 */];

static Byte g_PosDirectBits[k_NumPosSyms];

static const Byte k_PosRuns[31] =
{
  8, 0, 9, 7, 10, 15, 15, 20, 20, 30, 33, 40, 42, 45, 60, 73,
  80, 85, 95, 105, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
};

static UInt32 g_LenBases[k_NumLenSyms];

static const Byte k_LenDirectBits[k_NumLenSyms] =
{
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2,
  2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 6,
  7, 8, 9, 10, 16, 30,
};

static struct CInit
{
  CInit()
  {
    {
      unsigned sum = 0;
      for (unsigned i = 0; i < sizeof(k_PosRuns); i++)
      {
        unsigned t = k_PosRuns[i];
        for (unsigned y = 0; y < t; y++)
          g_PosDirectBits[sum + y] = (Byte)i;
        sum += t;
      }
    }
    {
      UInt32 sum = 1;
      for (unsigned i = 0; i < k_NumPosSyms; i++)
      {
        g_PosBases[i] = sum;
        sum += (UInt32)1 << g_PosDirectBits[i];
      }
      // g_PosBases[k_NumPosSyms] = sum;
    }
    {
      UInt32 sum = 1;
      for (unsigned i = 0; i < k_NumLenSyms; i++)
      {
        g_LenBases[i] = sum;
        sum += (UInt32)1 << k_LenDirectBits[i];
      }
    }
  }
} g_Init;

static unsigned GetNumPosSlots(size_t size)
{
  if (size < 2)
    return 0;
  
  size--;

  if (size >= g_PosBases[k_NumPosSyms - 1])
    return k_NumPosSyms;
  unsigned left = 0;
  unsigned right = k_NumPosSyms;
  for (;;)
  {
    const unsigned m = (left + right) / 2;
    if (left == m)
      return m + 1;
    if (size >= g_PosBases[m])
      left = m;
    else
      right = m;
  }
}


static const Int32 k_x86_WindowSize = 65535;
static const Int32 k_x86_TransOffset = 1023;

static const size_t k_x86_HistorySize = (1 << 16);

static void x86_Filter(Byte *data, UInt32 size, Int32 *history)
{
  if (size <= 17)
    return;

  Byte isCode[256];
  memset(isCode, 0, 256);
  isCode[0x48] = 1;
  isCode[0x4C] = 1;
  isCode[0xE8] = 1;
  isCode[0xE9] = 1;
  isCode[0xF0] = 1;
  isCode[0xFF] = 1;

  {
    for (size_t i = 0; i < k_x86_HistorySize; i++)
      history[i] = -(Int32)k_x86_WindowSize - 1;
  }

  size -= 16;
  const unsigned kSave = 6;
  const Byte savedByte = data[(size_t)size + kSave];
  data[(size_t)size + kSave] = 0xE8;
  Int32 last_x86_pos = -k_x86_TransOffset - 1;

  // first byte is ignored
  Int32 i = 0;
  
  for (;;)
  {
    Byte *p = data + (UInt32)i;

    for (;;)
    {
      if (isCode[*(++p)]) break;
      if (isCode[*(++p)]) break;
    }
    
    i = (Int32)(p - data);
    if ((UInt32)i >= size)
      break;

    UInt32 codeLen;

    Int32 maxTransOffset = k_x86_TransOffset;
    
    const Byte b = p[0];
    
    if (b == 0x48)
    {
      if (p[1] == 0x8B)
      {
        if ((p[2] & 0xF7) != 0x5)
          continue;
        // MOV RAX / RCX, [RIP + disp32]
      }
      else if (p[1] == 0x8D) // LEA
      {
        if ((p[2] & 0x7) != 0x5)
          continue;
        // LEA R**, []
      }
      else
        continue;
      codeLen = 3;
    }
    else if (b == 0x4C)
    {
      if (p[1] != 0x8D || (p[2] & 0x7) != 0x5)
        continue;
      // LEA R*, []
      codeLen = 3;
    }
    else if (b == 0xE8)
    {
      // CALL
      codeLen = 1;
      maxTransOffset /= 2;
    }
    else if (b == 0xE9)
    {
      // JUMP
      i += 4;
      continue;
    }
    else if (b == 0xF0)
    {
      if (p[1] != 0x83 || p[2] != 0x05)
        continue;
      // LOCK ADD [RIP + disp32], imm8
      // LOCK ADD [disp32], imm8
      codeLen = 3;
    }
    else
    // if (b == 0xFF)
    {
      if (p[1] != 0x15)
        continue;
      // CALL [RIP + disp32];
      // CALL [disp32];
      codeLen = 2;
    }

    Int32 *target;
    {
      Byte *p2 = p + codeLen;
      UInt32 n = GetUi32(p2);
      if (i - last_x86_pos <= maxTransOffset)
      {
        n = (UInt32)((Int32)n - i);
        SetUi32(p2, n)
      }
      target = history + (((UInt32)i + n) & 0xFFFF);
    }

    i += (Int32)(codeLen + sizeof(UInt32) - 1);

    if (i - *target <= k_x86_WindowSize)
      last_x86_pos = i;
    *target = i;
  }

  data[(size_t)size + kSave] = savedByte;
}



// static const int kLenIdNeedInit = -2;

CDecoder::CDecoder():
  _x86_history(NULL)
{
}

CDecoder::~CDecoder()
{
  ::MidFree(_x86_history);
}

// #define RIF(x) { if (!(x)) return false; }

#define LIMIT_CHECK if (_bs._buf < _rc.cur) return S_FALSE;
// #define LIMIT_CHECK

#define READ_BITS_CHECK(numDirectBits) \
  if (_bs._buf < _rc.cur) return S_FALSE; \
  if ((size_t)(_bs._buf - _rc.cur) < (numDirectBits >> 3)) return S_FALSE;


#define HUFF_DEC(sym, pp) \
    sym = pp.DecodeFull(&_bs); \
    pp.Freqs[sym]++; \
    if (--pp.RebuildRem == 0) pp.Rebuild();


HRESULT CDecoder::CodeReal(const Byte *in, size_t inSize, Byte *_win, size_t outSize)
{
  // size_t inSizeT = (size_t)(inSize);
  // Byte *_win;
  // size_t _pos;
  _pos = 0;

  CBitDecoder _bs;
  CRangeDecoder _rc;
 
  if (inSize < 8 || (inSize & 1) != 0)
    return S_FALSE;
  _rc.Init(in, inSize);
  if (_rc.code >= _rc.range)
    return S_FALSE;
  _bs.Init(in, inSize);

  {
    {
      {
        for (unsigned i = 0 ; i < k_NumReps + 1; i++)
          _reps[i] = i + 1;
      }

      {
        for (unsigned i = 0 ; i < k_NumReps + 1; i++)
          _deltaReps[i] = i + 1;
      }

      mainState = 0;
      matchState = 0;

      { for (size_t i = 0; i < k_NumMainProbs; i++) mainProbs[i].Init(); }
      { for (size_t i = 0; i < k_NumMatchProbs; i++) matchProbs[i].Init(); }

      {
        for (size_t k = 0; k < k_NumReps; k++)
        {
          lzRepStates[k] = 0;
          for (size_t i = 0; i < k_NumRepProbs; i++)
            lzRepProbs[k][i].Init();
        }
      }
      {
        for (size_t k = 0; k < k_NumReps; k++)
        {
          deltaRepStates[k] = 0;
          for (size_t i = 0; i < k_NumRepProbs; i++)
            deltaRepProbs[k][i].Init();
        }
      }

      m_LitDecoder.Init();
      m_LenDecoder.Init();
      m_PowerDecoder.Init();
      unsigned numPosSyms = GetNumPosSlots(outSize);
      if (numPosSyms < 2)
        numPosSyms = 2;
      m_PosDecoder.Init(numPosSyms);
      m_DeltaDecoder.Init(numPosSyms);
    }
  }

  {
    unsigned prevType = 0;
    
    while (_pos < outSize)
    {
      if (_rc.Decode(&mainState, k_NumMainProbs, mainProbs) == 0)
      {
        UInt32 number;
        HUFF_DEC(number, m_LitDecoder)
        LIMIT_CHECK
        _win[_pos++] = (Byte)number;
        prevType = 0;
      }
      else if (_rc.Decode(&matchState, k_NumMatchProbs, matchProbs) == 0)
      {
        UInt32 distance;
        
        if (_rc.Decode(&lzRepStates[0], k_NumRepProbs, lzRepProbs[0]) == 0)
        {
          UInt32 number;
          HUFF_DEC(number, m_PosDecoder)
          LIMIT_CHECK

          const unsigned numDirectBits = g_PosDirectBits[number];
          distance = g_PosBases[number];
          READ_BITS_CHECK(numDirectBits)
          distance += _bs.ReadBits32(numDirectBits);
          // LIMIT_CHECK
          _reps[3] = _reps[2];
          _reps[2] = _reps[1];
          _reps[1] = _reps[0];
          _reps[0] = distance;
        }
        else
        {
          if (_rc.Decode(&lzRepStates[1], k_NumRepProbs, lzRepProbs[1]) == 0)
          {
            if (prevType != 1)
              distance = _reps[0];
            else
            {
              distance = _reps[1];
              _reps[1] = _reps[0];
              _reps[0] = distance;
            }
          }
          else if (_rc.Decode(&lzRepStates[2], k_NumRepProbs, lzRepProbs[2]) == 0)
          {
            if (prevType != 1)
            {
              distance = _reps[1];
              _reps[1] = _reps[0];
              _reps[0] = distance;
            }
            else
            {
              distance = _reps[2];
              _reps[2] = _reps[1];
              _reps[1] = _reps[0];
              _reps[0] = distance;
            }
          }
          else
          {
            if (prevType != 1)
            {
              distance = _reps[2];
              _reps[2] = _reps[1];
              _reps[1] = _reps[0];
              _reps[0] = distance;
            }
            else
            {
              distance = _reps[3];
              _reps[3] = _reps[2];
              _reps[2] = _reps[1];
              _reps[1] = _reps[0];
              _reps[0] = distance;
            }
          }
        }

        UInt32 lenSlot;
        HUFF_DEC(lenSlot, m_LenDecoder)
        LIMIT_CHECK

        UInt32 len = g_LenBases[lenSlot];
        {
          const unsigned numDirectBits = k_LenDirectBits[lenSlot];
          READ_BITS_CHECK(numDirectBits)
          len += _bs.ReadBits32(numDirectBits);
        }
        // LIMIT_CHECK

        if (len > outSize - _pos)
          return S_FALSE;

        if (distance > _pos)
          return S_FALSE;

        Byte *dest = _win + _pos;
        const Byte *src = dest - distance;
        _pos += len;
        do
          *dest++ = *src++;
        while (--len);

        prevType = 1;
      }
      else
      {
        UInt64 distance;

        UInt32 power;
        UInt32 distance32;
        
        if (_rc.Decode(&deltaRepStates[0], k_NumRepProbs, deltaRepProbs[0]) == 0)
        {
          HUFF_DEC(power, m_PowerDecoder)
          LIMIT_CHECK

          UInt32 number;
          HUFF_DEC(number, m_DeltaDecoder)
          LIMIT_CHECK

          const unsigned numDirectBits = g_PosDirectBits[number];
          distance32 = g_PosBases[number];
          READ_BITS_CHECK(numDirectBits)
          distance32 += _bs.ReadBits32(numDirectBits);
          // LIMIT_CHECK

          distance = ((UInt64)power << 32) | distance32;

          _deltaReps[3] = _deltaReps[2];
          _deltaReps[2] = _deltaReps[1];
          _deltaReps[1] = _deltaReps[0];
          _deltaReps[0] = distance;
        }
        else
        {
          if (_rc.Decode(&deltaRepStates[1], k_NumRepProbs, deltaRepProbs[1]) == 0)
          {
            if (prevType != 2)
              distance = _deltaReps[0];
            else
            {
              distance = _deltaReps[1];
              _deltaReps[1] = _deltaReps[0];
              _deltaReps[0] = distance;
            }
          }
          else if (_rc.Decode(&deltaRepStates[2], k_NumRepProbs, deltaRepProbs[2]) == 0)
          {
            if (prevType != 2)
            {
              distance = _deltaReps[1];
              _deltaReps[1] = _deltaReps[0];
              _deltaReps[0] = distance;
            }
            else
            {
              distance = _deltaReps[2];
              _deltaReps[2] = _deltaReps[1];
              _deltaReps[1] = _deltaReps[0];
              _deltaReps[0] = distance;
            }
          }
          else
          {
            if (prevType != 2)
            {
              distance = _deltaReps[2];
              _deltaReps[2] = _deltaReps[1];
              _deltaReps[1] = _deltaReps[0];
              _deltaReps[0] = distance;
            }
            else
            {
              distance = _deltaReps[3];
              _deltaReps[3] = _deltaReps[2];
              _deltaReps[2] = _deltaReps[1];
              _deltaReps[1] = _deltaReps[0];
              _deltaReps[0] = distance;
            }
          }
          distance32 = (UInt32)_deltaReps[0] & 0xFFFFFFFF;
          power = (UInt32)(_deltaReps[0] >> 32);
        }

        const UInt32 dist = (distance32 << power);
        
        UInt32 lenSlot;
        HUFF_DEC(lenSlot, m_LenDecoder)
        LIMIT_CHECK

        UInt32 len = g_LenBases[lenSlot];
        {
          unsigned numDirectBits = k_LenDirectBits[lenSlot];
          READ_BITS_CHECK(numDirectBits)
          len += _bs.ReadBits32(numDirectBits);
        }
        // LIMIT_CHECK

        if (len > outSize - _pos)
          return S_FALSE;

        size_t span = (size_t)1 << power;
        if ((UInt64)dist + span > _pos)
          return S_FALSE;
        Byte *dest = _win + _pos - span;
        const Byte *src = dest - dist;
        _pos += len;
        do
        {
          *(dest + span) = (Byte)(*(dest) + *(src + span) - *(src));
          src++;
          dest++;
        }
        while (--len);

        prevType = 2;
      }
    }
  }

  _rc.Normalize();
  if (_rc.code != 0)
    return S_FALSE;
  if (_rc.cur > _bs._buf
      || (_rc.cur == _bs._buf && _bs._bitPos != 0))
    return S_FALSE;

  /*
  int delta = (int)(_bs._buf - _rc.cur);
  if (_bs._bitPos != 0)
    delta--;
  if ((delta & 1))
    delta--;
  printf("%d ", delta);
  */

  return S_OK;
}

HRESULT CDecoder::Code(const Byte *in, size_t inSize, Byte *out, size_t outSize)
{
  if (!_x86_history)
  {
    _x86_history = (Int32 *)::MidAlloc(sizeof(Int32) * k_x86_HistorySize);
    if (!_x86_history)
      return E_OUTOFMEMORY;
  }
  HRESULT res;
  // try
  {
    res = CodeReal(in, inSize, out, outSize);
  }
  // catch (...) { res = S_FALSE; }
  x86_Filter(out, (UInt32)_pos, _x86_history);
  return res;
}

}}

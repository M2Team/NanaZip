// LzxDecoder.cpp

#include "StdAfx.h"

#include <string.h>

// #define SHOW_DEBUG_INFO

#ifdef SHOW_DEBUG_INFO
#include <stdio.h>
#define PRF(x) x
#else
#define PRF(x)
#endif

#include "../../../C/Alloc.h"

#include "LzxDecoder.h"

namespace NCompress {
namespace NLzx {

static void x86_Filter(Byte *data, UInt32 size, UInt32 processedSize, UInt32 translationSize)
{
  const UInt32 kResidue = 10;
  if (size <= kResidue)
    return;
  size -= kResidue;
  
  Byte save = data[(size_t)size + 4];
  data[(size_t)size + 4] = 0xE8;
  
  for (UInt32 i = 0;;)
  {
    Byte *p = data + i;
    for (;;)
    {
      if (*p++ == 0xE8) break;
      if (*p++ == 0xE8) break;
      if (*p++ == 0xE8) break;
      if (*p++ == 0xE8) break;
    }

    i = (UInt32)(p - data);

    if (i > size)
      break;
    {
      Int32 v = (Int32)GetUi32(p);
      Int32 pos = (Int32)((Int32)1 - (Int32)(processedSize + i));
      i += 4;
      if (v >= pos && v < (Int32)translationSize)
      {
        v += (v >= 0 ? pos : (Int32)translationSize);
        SetUi32(p, (UInt32)v);
      }
    }
  }

  data[(size_t)size + 4] = save;
}


CDecoder::CDecoder(bool wimMode):
    _win(NULL),
    _skipByte(false),
    _unpackBlockSize(0),
    KeepHistoryForNext(true),
    NeedAlloc(true),
    _keepHistory(false),
    _wimMode(wimMode),
    _numDictBits(15),
    _x86_buf(NULL),
    _x86_translationSize(0),
    _unpackedData(NULL)
{
}

CDecoder::~CDecoder()
{
  if (NeedAlloc)
    ::MidFree(_win);
  ::MidFree(_x86_buf);
}

HRESULT CDecoder::Flush()
{
  if (_x86_translationSize != 0)
  {
    Byte *destData = _win + _writePos;
    UInt32 curSize = _pos - _writePos;
    if (KeepHistoryForNext)
    {
      if (!_x86_buf)
      {
        // we must change it to support another chunk sizes
        const size_t kChunkSize = (size_t)1 << 15;
        if (curSize > kChunkSize)
          return E_NOTIMPL;
        _x86_buf = (Byte *)::MidAlloc(kChunkSize);
        if (!_x86_buf)
          return E_OUTOFMEMORY;
      }
      memcpy(_x86_buf, destData, curSize);
      _unpackedData = _x86_buf;
      destData = _x86_buf;
    }
    x86_Filter(destData, (UInt32)curSize, _x86_processedSize, _x86_translationSize);
    _x86_processedSize += (UInt32)curSize;
    if (_x86_processedSize >= ((UInt32)1 << 30))
      _x86_translationSize = 0;
  }
 
  return S_OK;
}


UInt32 CDecoder::ReadBits(unsigned numBits) { return _bitStream.ReadBitsSmall(numBits); }

#define RIF(x) { if (!(x)) return false; }

bool CDecoder::ReadTable(Byte *levels, unsigned numSymbols)
{
  {
    Byte levels2[kLevelTableSize];
    for (unsigned i = 0; i < kLevelTableSize; i++)
      levels2[i] = (Byte)ReadBits(kNumLevelBits);
    RIF(_levelDecoder.Build(levels2));
  }
  
  unsigned i = 0;
  do
  {
    UInt32 sym = _levelDecoder.Decode(&_bitStream);
    if (sym <= kNumHuffmanBits)
    {
      int delta = (int)levels[i] - (int)sym;
      delta += (delta < 0) ? (kNumHuffmanBits + 1) : 0;
      levels[i++] = (Byte)delta;
      continue;
    }
    
    unsigned num;
    Byte symbol;

    if (sym < kLevelSym_Same)
    {
      sym -= kLevelSym_Zero1;
      num = kLevelSym_Zero1_Start + ((unsigned)sym << kLevelSym_Zero1_NumBits) +
          (unsigned)ReadBits(kLevelSym_Zero1_NumBits + sym);
      symbol = 0;
    }
    else if (sym == kLevelSym_Same)
    {
      num = kLevelSym_Same_Start + (unsigned)ReadBits(kLevelSym_Same_NumBits);
      sym = _levelDecoder.Decode(&_bitStream);
      if (sym > kNumHuffmanBits)
        return false;
      int delta = (int)levels[i] - (int)sym;
      delta += (delta < 0) ? (kNumHuffmanBits + 1) : 0;
      symbol = (Byte)delta;
    }
    else
      return false;

    unsigned limit = i + num;
    if (limit > numSymbols)
      return false;

    do
      levels[i++] = symbol;
    while (i < limit);
  }
  while (i < numSymbols);

  return true;
}


bool CDecoder::ReadTables(void)
{
  {
    if (_skipByte)
    {
      if (_bitStream.DirectReadByte() != 0)
        return false;
    }

    _bitStream.NormalizeBig();

    unsigned blockType = (unsigned)ReadBits(kBlockType_NumBits);
    if (blockType > kBlockType_Uncompressed)
      return false;
    
    _unpackBlockSize = (1 << 15);
    if (!_wimMode || ReadBits(1) == 0)
    {
      _unpackBlockSize = ReadBits(16);
      // wimlib supports chunks larger than 32KB (unsupported my MS wim).
      if (!_wimMode || _numDictBits >= 16)
      {
        _unpackBlockSize <<= 8;
        _unpackBlockSize |= ReadBits(8);
      }
    }

    PRF(printf("\nBlockSize = %6d   %s  ", _unpackBlockSize, (_pos & 1) ? "@@@" : "   "));

    _isUncompressedBlock = (blockType == kBlockType_Uncompressed);

    _skipByte = false;

    if (_isUncompressedBlock)
    {
      _skipByte = ((_unpackBlockSize & 1) != 0);

      PRF(printf(" UncompressedBlock "));
      if (_unpackBlockSize & 1)
      {
        PRF(printf(" ######### "));
      }

      if (!_bitStream.PrepareUncompressed())
        return false;
      if (_bitStream.GetRem() < kNumReps * 4)
        return false;

      for (unsigned i = 0; i < kNumReps; i++)
      {
        UInt32 rep = _bitStream.ReadUInt32();
        if (rep > _winSize)
          return false;
        _reps[i] = rep;
      }
      
      return true;
    }

    _numAlignBits = 64;

    if (blockType == kBlockType_Aligned)
    {
      Byte levels[kAlignTableSize];
      _numAlignBits = kNumAlignBits;
      for (unsigned i = 0; i < kAlignTableSize; i++)
        levels[i] = (Byte)ReadBits(kNumAlignLevelBits);
      RIF(_alignDecoder.Build(levels));
    }
  }

  RIF(ReadTable(_mainLevels, 256));
  RIF(ReadTable(_mainLevels + 256, _numPosLenSlots));
  unsigned end = 256 + _numPosLenSlots;
  memset(_mainLevels + end, 0, kMainTableSize - end);
  RIF(_mainDecoder.Build(_mainLevels));
  RIF(ReadTable(_lenLevels, kNumLenSymbols));
  return _lenDecoder.Build(_lenLevels);
}


HRESULT CDecoder::CodeSpec(UInt32 curSize)
{
  if (!_keepHistory || !_isUncompressedBlock)
    _bitStream.NormalizeBig();
 
  if (!_keepHistory)
  {
    _skipByte = false;
    _unpackBlockSize = 0;

    memset(_mainLevels, 0, kMainTableSize);
    memset(_lenLevels, 0, kNumLenSymbols);
    
    {
      _x86_translationSize = 12000000;
      if (!_wimMode)
      {
        _x86_translationSize = 0;
        if (ReadBits(1) != 0)
        {
          UInt32 v = ReadBits(16) << 16;
          v |= ReadBits(16);
          _x86_translationSize = v;
        }
      }
      
      _x86_processedSize = 0;
    }

    _reps[0] = 1;
    _reps[1] = 1;
    _reps[2] = 1;
  }

  while (curSize > 0)
  {
    if (_bitStream.WasExtraReadError_Fast())
      return S_FALSE;
    
    if (_unpackBlockSize == 0)
    {
      if (!ReadTables())
        return S_FALSE;
      continue;
    }

    UInt32 next = _unpackBlockSize;
    if (next > curSize)
      next = curSize;
    
    if (_isUncompressedBlock)
    {
      size_t rem = _bitStream.GetRem();
      if (rem == 0)
        return S_FALSE;
      if (next > rem)
        next = (UInt32)rem;
      _bitStream.CopyTo(_win + _pos, next);
      _pos += next;
      curSize -= next;
      _unpackBlockSize -= next;

      /* we don't know where skipByte can be placed, if it's end of chunk:
          1) in current chunk - there are such cab archives, if chunk is last
          2) in next chunk - are there such archives ? */

      if (_skipByte
          && _unpackBlockSize == 0
          && curSize == 0
          && _bitStream.IsOneDirectByteLeft())
      {
        _skipByte = false;
        if (_bitStream.DirectReadByte() != 0)
          return S_FALSE;
      }
      
      continue;
    }

    curSize -= next;
    _unpackBlockSize -= next;
    
    Byte *win = _win;

    while (next > 0)
    {
      if (_bitStream.WasExtraReadError_Fast())
        return S_FALSE;

      UInt32 sym = _mainDecoder.Decode(&_bitStream);
      
      if (sym < 256)
      {
        win[_pos++] = (Byte)sym;
        next--;
        continue;
      }
      {
        sym -= 256;
        if (sym >= _numPosLenSlots)
          return S_FALSE;
        UInt32 posSlot = sym / kNumLenSlots;
        UInt32 lenSlot = sym % kNumLenSlots;
        UInt32 len = kMatchMinLen + lenSlot;
        
        if (lenSlot == kNumLenSlots - 1)
        {
          UInt32 lenTemp = _lenDecoder.Decode(&_bitStream);
          if (lenTemp >= kNumLenSymbols)
            return S_FALSE;
          len = kMatchMinLen + kNumLenSlots - 1 + lenTemp;
        }
        
        UInt32 dist;
        
        if (posSlot < kNumReps)
        {
          dist = _reps[posSlot];
          _reps[posSlot] = _reps[0];
          _reps[0] = dist;
        }
        else
        {
          unsigned numDirectBits;
          
          if (posSlot < kNumPowerPosSlots)
          {
            numDirectBits = (unsigned)(posSlot >> 1) - 1;
            dist = ((2 | (posSlot & 1)) << numDirectBits);
          }
          else
          {
            numDirectBits = kNumLinearPosSlotBits;
            dist = ((posSlot - 0x22) << kNumLinearPosSlotBits);
          }

          if (numDirectBits >= _numAlignBits)
          {
            dist += (_bitStream.ReadBitsSmall(numDirectBits - kNumAlignBits) << kNumAlignBits);
            UInt32 alignTemp = _alignDecoder.Decode(&_bitStream);
            if (alignTemp >= kAlignTableSize)
              return S_FALSE;
            dist += alignTemp;
          }
          else
            dist += _bitStream.ReadBitsBig(numDirectBits);
          
          dist -= kNumReps - 1;
          _reps[2] = _reps[1];
          _reps[1] = _reps[0];
          _reps[0] = dist;
        }

        if (len > next)
          return S_FALSE;

        if (dist > _pos && !_overDict)
          return S_FALSE;

        Byte *dest = win + _pos;
        const UInt32 mask = (_winSize - 1);
        UInt32 srcPos = (_pos - dist) & mask;

        next -= len;
        
        if (len > _winSize - srcPos)
        {
          _pos += len;
          do
          {
            *dest++ = win[srcPos++];
            srcPos &= mask;
          }
          while (--len);
        }
        else
        {
          ptrdiff_t src = (ptrdiff_t)srcPos - (ptrdiff_t)_pos;
          _pos += len;
          const Byte *lim = dest + len;
          *(dest) = *(dest + src);
          dest++;
          do
            *(dest) = *(dest + src);
          while (++dest != lim);
        }
      }
    }
  }

  if (!_bitStream.WasFinishedOK())
    return S_FALSE;

  return S_OK;
}


HRESULT CDecoder::Code(const Byte *inData, size_t inSize, UInt32 outSize)
{
  if (!_keepHistory)
  {
    _pos = 0;
    _overDict = false;
  }
  else if (_pos == _winSize)
  {
    _pos = 0;
    _overDict = true;
  }

  _writePos = _pos;
  _unpackedData = _win + _pos;
  
  if (outSize > _winSize - _pos)
    return S_FALSE;

  PRF(printf("\ninSize = %d", inSize));
  if ((inSize & 1) != 0)
  {
    PRF(printf(" ---------"));
  }

  if (inSize < 1)
    return S_FALSE;

  _bitStream.Init(inData, inSize);

  HRESULT res = CodeSpec(outSize);
  HRESULT res2 = Flush();
  return (res == S_OK ? res2 : res);
}


HRESULT CDecoder::SetParams2(unsigned numDictBits)
{
  _numDictBits = numDictBits;
  if (numDictBits < kNumDictBits_Min || numDictBits > kNumDictBits_Max)
    return E_INVALIDARG;
  unsigned numPosSlots = (numDictBits < 20) ?
      numDictBits * 2 :
      34 + ((unsigned)1 << (numDictBits - 17));
  _numPosLenSlots = numPosSlots * kNumLenSlots;
  return S_OK;
}
  

HRESULT CDecoder::SetParams_and_Alloc(unsigned numDictBits)
{
  RINOK(SetParams2(numDictBits));
  
  UInt32 newWinSize = (UInt32)1 << numDictBits;
 
  if (NeedAlloc)
  {
    if (!_win || newWinSize != _winSize)
    {
      ::MidFree(_win);
      _winSize = 0;
      _win = (Byte *)::MidAlloc(newWinSize);
      if (!_win)
        return E_OUTOFMEMORY;
    }
  }

  _winSize = (UInt32)newWinSize;
  return S_OK;
}

}}

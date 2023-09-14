// LzxDecoder.h

#ifndef ZIP7_INC_LZX_DECODER_H
#define ZIP7_INC_LZX_DECODER_H

#include "../../../C/CpuArch.h"

#include "../../Common/MyCom.h"

#include "HuffmanDecoder.h"
#include "Lzx.h"

namespace NCompress {
namespace NLzx {

class CBitDecoder
{
  unsigned _bitPos;
  UInt32 _value;
  const Byte *_buf;
  const Byte *_bufLim;
  UInt32 _extraSize;
public:

  void Init(const Byte *data, size_t size)
  {
    _buf = data;
    _bufLim = data + size - 1;
    _bitPos = 0;
    _extraSize = 0;
  }

  size_t GetRem() const { return (size_t)(_bufLim + 1 - _buf); }
  bool WasExtraReadError_Fast() const { return _extraSize > 4; }

  bool WasFinishedOK() const
  {
    if (_buf != _bufLim + 1)
      return false;
    if ((_bitPos >> 4) * 2 != _extraSize)
      return false;
    unsigned numBits = _bitPos & 15;
    return (((_value >> (_bitPos - numBits)) & (((UInt32)1 << numBits) - 1)) == 0);
  }
  
  void NormalizeSmall()
  {
    if (_bitPos <= 16)
    {
      UInt32 val;
      if (_buf >= _bufLim)
      {
        val = 0xFFFF;
        _extraSize += 2;
      }
      else
      {
        val = GetUi16(_buf);
        _buf += 2;
      }
      _value = (_value << 16) | val;
      _bitPos += 16;
    }
  }

  void NormalizeBig()
  {
    if (_bitPos <= 16)
    {
      {
        UInt32 val;
        if (_buf >= _bufLim)
        {
          val = 0xFFFF;
          _extraSize += 2;
        }
        else
        {
          val = GetUi16(_buf);
          _buf += 2;
        }
        _value = (_value << 16) | val;
        _bitPos += 16;
      }
      if (_bitPos <= 16)
      {
        UInt32 val;
        if (_buf >= _bufLim)
        {
          val = 0xFFFF;
          _extraSize += 2;
        }
        else
        {
          val = GetUi16(_buf);
          _buf += 2;
        }
        _value = (_value << 16) | val;
        _bitPos += 16;
      }
    }
  }

  UInt32 GetValue(unsigned numBits) const
  {
    return (_value >> (_bitPos - numBits)) & (((UInt32)1 << numBits) - 1);
  }
  
  void MovePos(unsigned numBits)
  {
    _bitPos -= numBits;
    NormalizeSmall();
  }

  UInt32 ReadBitsSmall(unsigned numBits)
  {
    _bitPos -= numBits;
    UInt32 val = (_value >> _bitPos) & (((UInt32)1 << numBits) - 1);
    NormalizeSmall();
    return val;
  }

  UInt32 ReadBitsBig(unsigned numBits)
  {
    _bitPos -= numBits;
    UInt32 val = (_value >> _bitPos) & (((UInt32)1 << numBits) - 1);
    NormalizeBig();
    return val;
  }

  bool PrepareUncompressed()
  {
    if (_extraSize != 0)
      return false;
    unsigned numBits = _bitPos - 16;
    if (((_value >> 16) & (((UInt32)1 << numBits) - 1)) != 0)
      return false;
    _buf -= 2;
    _bitPos = 0;
    return true;
  }

  UInt32 ReadUInt32()
  {
    UInt32 v = GetUi32(_buf);
    _buf += 4;
    return v;
  }

  void CopyTo(Byte *dest, size_t size)
  {
    memcpy(dest, _buf, size);
    _buf += size;
  }

  bool IsOneDirectByteLeft() const { return _buf == _bufLim && _extraSize == 0; }

  Byte DirectReadByte()
  {
    if (_buf > _bufLim)
    {
      _extraSize++;
      return 0xFF;
    }
    return *_buf++;
  }
};


Z7_CLASS_IMP_COM_0(
  CDecoder
)
  CBitDecoder _bitStream;
  Byte *_win;
  UInt32 _pos;
  UInt32 _winSize;

  bool _overDict;
  bool _isUncompressedBlock;
  bool _skipByte;
  unsigned _numAlignBits;

  UInt32 _reps[kNumReps];
  UInt32 _numPosLenSlots;
  UInt32 _unpackBlockSize;

public:
  bool KeepHistoryForNext;
  bool NeedAlloc;
private:
  bool _keepHistory;
  bool _wimMode;
  unsigned _numDictBits;
  UInt32 _writePos;

  Byte *_x86_buf;
  UInt32 _x86_translationSize;
  UInt32 _x86_processedSize;

  Byte *_unpackedData;
  
  NHuffman::CDecoder<kNumHuffmanBits, kMainTableSize> _mainDecoder;
  NHuffman::CDecoder<kNumHuffmanBits, kNumLenSymbols> _lenDecoder;
  NHuffman::CDecoder7b<kAlignTableSize> _alignDecoder;
  NHuffman::CDecoder<kNumHuffmanBits, kLevelTableSize, 7> _levelDecoder;

  Byte _mainLevels[kMainTableSize];
  Byte _lenLevels[kNumLenSymbols];

  HRESULT Flush();

  UInt32 ReadBits(unsigned numBits);
  bool ReadTable(Byte *levels, unsigned numSymbols);
  bool ReadTables();

  HRESULT CodeSpec(UInt32 size);
  HRESULT SetParams2(unsigned numDictBits);
public:
  CDecoder(bool wimMode = false);
  ~CDecoder();

  HRESULT SetExternalWindow(Byte *win, unsigned numDictBits)
  {
    NeedAlloc = false;
    _win = win;
    _winSize = (UInt32)1 << numDictBits;
    return SetParams2(numDictBits);
  }

  void SetKeepHistory(bool keepHistory) { _keepHistory = keepHistory; }

  HRESULT SetParams_and_Alloc(unsigned numDictBits);

  HRESULT Code(const Byte *inData, size_t inSize, UInt32 outSize);
  
  bool WasBlockFinished() const { return _unpackBlockSize == 0; }
  const Byte *GetUnpackData() const { return _unpackedData; }
  UInt32 GetUnpackSize() const { return _pos - _writePos; }
};

}}

#endif

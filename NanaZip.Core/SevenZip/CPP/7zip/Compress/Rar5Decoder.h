// Rar5Decoder.h
// According to unRAR license, this code may not be used to develop
// a program that creates RAR archives

#ifndef ZIP7_INC_COMPRESS_RAR5_DECODER_H
#define ZIP7_INC_COMPRESS_RAR5_DECODER_H

#include "../../../C/CpuArch.h"

#include "../../Common/MyBuffer2.h"
#include "../../Common/MyCom.h"
#include "../../Common/MyException.h"
#include "../../Common/MyVector.h"

#include "../ICoder.h"

#include "HuffmanDecoder.h"

namespace NCompress {
namespace NRar5 {

/*
struct CInBufferException: public CSystemException
{
  CInBufferException(HRESULT errorCode): CSystemException(errorCode) {}
};
*/

class CBitDecoder
{
public:
  const Byte *_buf;
  unsigned _bitPos;
  bool _wasFinished;
  Byte _blockEndBits7;
  const Byte *_bufCheck2;
  const Byte *_bufCheck;
  Byte *_bufLim;
  Byte *_bufBase;

  UInt64 _processedSize;
  UInt64 _blockEnd;

  ISequentialInStream *_stream;
  HRESULT _hres;

  void SetCheck2()
  {
    _bufCheck2 = _bufCheck;
    if (_bufCheck > _buf)
    {
      UInt64 processed = GetProcessedSize_Round();
      if (_blockEnd < processed)
        _bufCheck2 = _buf;
      else
      {
        const UInt64 delta = _blockEnd - processed;
        if ((size_t)(_bufCheck - _buf) > delta)
          _bufCheck2 = _buf + (size_t)delta;
      }
    }
  }

  bool IsBlockOverRead() const
  {
    const UInt64 v = GetProcessedSize_Round();
    if (v < _blockEnd)
      return false;
    if (v > _blockEnd)
      return true;
    return _bitPos > _blockEndBits7;
  }

  /*
  CBitDecoder() throw():
      _buf(0),
      _bufLim(0),
      _bufBase(0),
      _stream(0),
      _processedSize(0),
      _wasFinished(false)
      {}
  */

  void Init() throw()
  {
    _blockEnd = 0;
    _blockEndBits7 = 0;

    _bitPos = 0;
    _processedSize = 0;
    _buf = _bufBase;
    _bufLim = _bufBase;
    _bufCheck = _buf;
    _bufCheck2 = _buf;
    _wasFinished = false;
  }

  void Prepare2() throw();

  void Prepare() throw()
  {
    if (_buf >= _bufCheck)
      Prepare2();
  }

  bool ExtraBitsWereRead() const
  {
    return _buf >= _bufLim && (_buf > _bufLim || _bitPos != 0);
  }

  bool InputEofError() const { return ExtraBitsWereRead(); }

  unsigned GetProcessedBits7() const { return _bitPos; }
  UInt64 GetProcessedSize_Round() const { return _processedSize + (size_t)(_buf - _bufBase); }
  UInt64 GetProcessedSize() const { return _processedSize + (size_t)(_buf - _bufBase) + ((_bitPos + 7) >> 3); }

  void AlignToByte()
  {
    _buf += (_bitPos + 7) >> 3;
    _bitPos = 0;
  }

  Byte ReadByteInAligned()
  {
    return *_buf++;
  }

  UInt32 GetValue(unsigned numBits) const
  {
    UInt32 v = ((UInt32)_buf[0] << 16) | ((UInt32)_buf[1] << 8) | (UInt32)_buf[2];
    v >>= (24 - numBits - _bitPos);
    return v & ((1 << numBits) - 1);
  }
  
  void MovePos(unsigned numBits)
  {
    _bitPos += numBits;
    _buf += (_bitPos >> 3);
    _bitPos &= 7;
  }

  UInt32 ReadBits9(unsigned numBits)
  {
    const Byte *buf = _buf;
    UInt32 v = ((UInt32)buf[0] << 8) | (UInt32)buf[1];
    v &= ((UInt32)0xFFFF >> _bitPos);
    numBits += _bitPos;
    v >>= (16 - numBits);
    _buf = buf + (numBits >> 3);
    _bitPos = numBits & 7;
    return v;
  }

  UInt32 ReadBits9fix(unsigned numBits)
  {
    const Byte *buf = _buf;
    UInt32 v = ((UInt32)buf[0] << 8) | (UInt32)buf[1];
    const UInt32 mask = ((1 << numBits) - 1);
    numBits += _bitPos;
    v >>= (16 - numBits);
    _buf = buf + (numBits >> 3);
    _bitPos = numBits & 7;
    return v & mask;
  }

  UInt32 ReadBits32(unsigned numBits)
  {
    const UInt32 mask = ((1 << numBits) - 1);
    numBits += _bitPos;
    const Byte *buf = _buf;
    UInt32 v = GetBe32(buf);
    if (numBits > 32)
    {
      v <<= (numBits - 32);
      v |= (UInt32)buf[4] >> (40 - numBits);
    }
    else
      v >>= (32 - numBits);
    _buf = buf + (numBits >> 3);
    _bitPos = numBits & 7;
    return v & mask;
  }
};


struct CFilter
{
  Byte Type;
  Byte Channels;
  UInt32 Size;
  UInt64 Start;
};


const unsigned kNumReps = 4;
const unsigned kLenTableSize = 11 * 4;
const unsigned kMainTableSize = 256 + 1 + 1 + kNumReps + kLenTableSize;
const unsigned kDistTableSize = 64;
const unsigned kNumAlignBits = 4;
const unsigned kAlignTableSize = (1 << kNumAlignBits);
const unsigned kLevelTableSize = 20;
const unsigned kTablesSizesSum = kMainTableSize + kDistTableSize + kAlignTableSize + kLenTableSize;

const unsigned kNumHuffmanBits = 15;

Z7_CLASS_IMP_NOQIB_2(
  CDecoder
  , ICompressCoder
  , ICompressSetDecoderProperties2
)
  bool _useAlignBits;
  bool _isLastBlock;
  bool _unpackSize_Defined;
  // bool _packSize_Defined;
  
  bool _unsupportedFilter;
  bool _lzError;
  bool _writeError;

  bool _isSolid;
  bool _solidAllowed;
  bool _tableWasFilled;
  bool _wasInit;

  Byte _dictSizeLog;
  
  // CBitDecoder _bitStream;
  Byte *_window;
  size_t _winPos;
  size_t _winSize;
  size_t _winMask;

  UInt64 _lzSize;

  unsigned _numCorrectDistSymbols;
  unsigned _numUnusedFilters;

  UInt64 _lzWritten;
  UInt64 _lzFileStart;
  UInt64 _unpackSize;
  // UInt64 _packSize;
  UInt64 _lzEnd;
  UInt64 _writtenFileSize;
  size_t _winSizeAllocated;

  UInt32 _reps[kNumReps];
  UInt32 _lastLen;
  
  UInt64 _filterEnd;
  CMidBuffer _filterSrc;
  CMidBuffer _filterDst;

  CRecordVector<CFilter> _filters;

  ISequentialInStream *_inStream;
  ISequentialOutStream *_outStream;
  ICompressProgressInfo *_progress;
  Byte *_inputBuf;

  NHuffman::CDecoder<kNumHuffmanBits, kMainTableSize> m_MainDecoder;
  NHuffman::CDecoder<kNumHuffmanBits, kDistTableSize> m_DistDecoder;
  NHuffman::CDecoder<kNumHuffmanBits, kAlignTableSize> m_AlignDecoder;
  NHuffman::CDecoder<kNumHuffmanBits, kLenTableSize> m_LenDecoder;
  NHuffman::CDecoder<kNumHuffmanBits, kLevelTableSize> m_LevelDecoder;


  void InitFilters()
  {
    _numUnusedFilters = 0;
    _filters.Clear();
  }

  void DeleteUnusedFilters()
  {
    if (_numUnusedFilters != 0)
    {
      _filters.DeleteFrontal(_numUnusedFilters);
      _numUnusedFilters = 0;
    }
  }

  HRESULT WriteData(const Byte *data, size_t size);
  HRESULT ExecuteFilter(const CFilter &f);
  HRESULT WriteBuf();
  HRESULT AddFilter(CBitDecoder &_bitStream);

  HRESULT ReadTables(CBitDecoder &_bitStream);
  HRESULT DecodeLZ();
  HRESULT CodeReal();
  
public:
  CDecoder();
  ~CDecoder();
};

}}

#endif

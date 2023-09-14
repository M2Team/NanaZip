// Rar5Decoder.cpp
// According to unRAR license, this code may not be used to develop
// a program that creates RAR archives

#include "StdAfx.h"

// #include <stdio.h>

#include "../Common/StreamUtils.h"

#include "Rar5Decoder.h"

namespace NCompress {
namespace NRar5 {

static const size_t kInputBufSize = 1 << 20;
 
void CBitDecoder::Prepare2() throw()
{
  const unsigned kSize = 16;
  if (_buf > _bufLim)
    return;

  size_t rem = (size_t)(_bufLim - _buf);
  if (rem != 0)
    memmove(_bufBase, _buf, rem);

  _bufLim = _bufBase + rem;
  _processedSize += (size_t)(_buf - _bufBase);
  _buf = _bufBase;

  if (!_wasFinished)
  {
    UInt32 processed = (UInt32)(kInputBufSize - rem);
    _hres = _stream->Read(_bufLim, (UInt32)processed, &processed);
    _bufLim += processed;
    _wasFinished = (processed == 0);
    if (_hres != S_OK)
    {
      _wasFinished = true;
      // throw CInBufferException(result);
    }
  }

  rem = (size_t)(_bufLim - _buf);
  _bufCheck = _buf;
  if (rem < kSize)
    memset(_bufLim, 0xFF, kSize - rem);
  else
    _bufCheck = _bufLim - kSize;

  SetCheck2();
}


enum FilterType
{
  FILTER_DELTA = 0,
  FILTER_E8,
  FILTER_E8E9,
  FILTER_ARM
};

static const size_t kWriteStep = (size_t)1 << 22;

CDecoder::CDecoder():
    _isSolid(false),
    _solidAllowed(false),
    _wasInit(false),
    _dictSizeLog(0),
    _window(NULL),
    _winPos(0),
    _lzSize(0),
    _lzEnd(0),
    _writtenFileSize(0),
    _winSizeAllocated(0),
    _inputBuf(NULL)
{
}

CDecoder::~CDecoder()
{
  ::MidFree(_window);
  ::MidFree(_inputBuf);
}

HRESULT CDecoder::WriteData(const Byte *data, size_t size)
{
  HRESULT res = S_OK;
  if (!_unpackSize_Defined || _writtenFileSize < _unpackSize)
  {
    size_t cur = size;
    if (_unpackSize_Defined)
    {
      const UInt64 rem = _unpackSize - _writtenFileSize;
      if (cur > rem)
        cur = (size_t)rem;
    }
    res = WriteStream(_outStream, data, cur);
    if (res != S_OK)
      _writeError = true;
  }
  _writtenFileSize += size;
  return res;
}

HRESULT CDecoder::ExecuteFilter(const CFilter &f)
{
  bool useDest = false;

  Byte *data = _filterSrc;
  UInt32 dataSize = f.Size;
  
  // printf("\nType = %d offset = %9d  size = %5d", f.Type, (unsigned)(f.Start - _lzFileStart), dataSize);
  
  switch (f.Type)
  {
    case FILTER_E8:
    case FILTER_E8E9:
    {
      // printf("  FILTER_E8");
      if (dataSize > 4)
      {
        dataSize -= 4;
        const UInt32 fileOffset = (UInt32)(f.Start - _lzFileStart);
        
        const UInt32 kFileSize = (UInt32)1 << 24;
        const Byte cmpMask = (Byte)(f.Type == FILTER_E8 ? 0xFF : 0xFE);
        
        for (UInt32 curPos = 0; curPos < dataSize;)
        {
          curPos++;
          if (((*data++) & cmpMask) == 0xE8)
          {
            const UInt32 offset = (curPos + fileOffset) & (kFileSize - 1);
            const UInt32 addr = GetUi32(data);
            
            if (addr < kFileSize)
            {
              SetUi32(data, addr - offset)
            }
            else if (addr > ((UInt32)0xFFFFFFFF - offset)) // (addr > ~(offset))
            {
              SetUi32(data, addr + kFileSize)
            }
              
            data += 4;
            curPos += 4;
          }
        }
      }
      break;
    }

    case FILTER_ARM:
    {
      if (dataSize >= 4)
      {
        dataSize -= 4;
        const UInt32 fileOffset = (UInt32)(f.Start - _lzFileStart);
        
        for (UInt32 curPos = 0; curPos <= dataSize; curPos += 4)
        {
          Byte *d = data + curPos;
          if (d[3] == 0xEB)
          {
            UInt32 offset = d[0] | ((UInt32)d[1] << 8) | ((UInt32)d[2] << 16);
            offset -= (fileOffset + curPos) >> 2;
            d[0] = (Byte)offset;
            d[1] = (Byte)(offset >> 8);
            d[2] = (Byte)(offset >> 16);
          }
        }
      }
      break;
    }
    
    case FILTER_DELTA:
    {
      // printf("  channels = %d", f.Channels);
      _filterDst.AllocAtLeast(dataSize);
      if (!_filterDst.IsAllocated())
        return E_OUTOFMEMORY;

      Byte *dest = _filterDst;
      const UInt32 numChannels = f.Channels;
      
      for (UInt32 curChannel = 0; curChannel < numChannels; curChannel++)
      {
        Byte prevByte = 0;
        for (UInt32 destPos = curChannel; destPos < dataSize; destPos += numChannels)
          dest[destPos] = (prevByte = (Byte)(prevByte - *data++));
      }
      useDest = true;
      break;
    }

    default:
      _unsupportedFilter = true;
      memset(_filterSrc, 0, f.Size);
      // return S_OK;  // unrar
  }

  return WriteData(useDest ?
      (const Byte *)_filterDst :
      (const Byte *)_filterSrc,
      f.Size);
}


HRESULT CDecoder::WriteBuf()
{
  DeleteUnusedFilters();

  for (unsigned i = 0; i < _filters.Size();)
  {
    const CFilter &f = _filters[i];
    
    const UInt64 blockStart = f.Start;

    const size_t lzAvail = (size_t)(_lzSize - _lzWritten);
    if (lzAvail == 0)
      break;
    
    if (blockStart > _lzWritten)
    {
      const UInt64 rem = blockStart - _lzWritten;
      size_t size = lzAvail;
      if (size > rem)
        size = (size_t)rem;
      if (size != 0)
      {
        RINOK(WriteData(_window + _winPos - lzAvail, size))
        _lzWritten += size;
      }
      continue;
    }
    
    const UInt32 blockSize = f.Size;
    size_t offset = (size_t)(_lzWritten - blockStart);
    if (offset == 0)
    {
      _filterSrc.AllocAtLeast(blockSize);
      if (!_filterSrc.IsAllocated())
        return E_OUTOFMEMORY;
    }
    
    const size_t blockRem = (size_t)blockSize - offset;
    size_t size = lzAvail;
    if (size > blockRem)
      size = blockRem;
    memcpy(_filterSrc + offset, _window + _winPos - lzAvail, size);
    _lzWritten += size;
    offset += size;
    if (offset != blockSize)
      return S_OK;

    _numUnusedFilters = ++i;
    RINOK(ExecuteFilter(f))
  }
      
  DeleteUnusedFilters();

  if (!_filters.IsEmpty())
    return S_OK;
  
  const size_t lzAvail = (size_t)(_lzSize - _lzWritten);
  RINOK(WriteData(_window + _winPos - lzAvail, lzAvail))
  _lzWritten += lzAvail;
  return S_OK;
}


static UInt32 ReadUInt32(CBitDecoder &bi)
{
  const unsigned numBytes = bi.ReadBits9fix(2) + 1;
  UInt32 v = 0;
  for (unsigned i = 0; i < numBytes; i++)
    v += ((UInt32)bi.ReadBits9fix(8) << (i * 8));
  return v;
}


static const unsigned MAX_UNPACK_FILTERS = 8192;

HRESULT CDecoder::AddFilter(CBitDecoder &_bitStream)
{
  DeleteUnusedFilters();

  if (_filters.Size() >= MAX_UNPACK_FILTERS)
  {
    RINOK(WriteBuf())
    DeleteUnusedFilters();
    if (_filters.Size() >= MAX_UNPACK_FILTERS)
    {
      _unsupportedFilter = true;
      InitFilters();
    }
  }

  _bitStream.Prepare();

  CFilter f;
  const UInt32 blockStart = ReadUInt32(_bitStream);
  f.Size = ReadUInt32(_bitStream);

  if (f.Size > ((UInt32)1 << 22))
  {
    _unsupportedFilter = true;
    f.Size = 0;  // unrar 5.5.5
  }

  f.Type = (Byte)_bitStream.ReadBits9fix(3);
  f.Channels = 0;
  if (f.Type == FILTER_DELTA)
    f.Channels = (Byte)(_bitStream.ReadBits9fix(5) + 1);
  f.Start = _lzSize + blockStart;

  if (f.Start < _filterEnd)
    _unsupportedFilter = true;
  else
  {
    _filterEnd = f.Start + f.Size;
    if (f.Size != 0)
      _filters.Add(f);
  }

  return S_OK;
}


#define RIF(x) { if (!(x)) return S_FALSE; }

HRESULT CDecoder::ReadTables(CBitDecoder &_bitStream)
{
  if (_progress)
  {
    const UInt64 packSize = _bitStream.GetProcessedSize();
    RINOK(_progress->SetRatioInfo(&packSize, &_writtenFileSize))
  }

  _bitStream.AlignToByte();
  _bitStream.Prepare();
  
  {
    const unsigned flags = _bitStream.ReadByteInAligned();
    unsigned checkSum = _bitStream.ReadByteInAligned();
    checkSum ^= flags;
    const unsigned num = (flags >> 3) & 3;
    if (num == 3)
      return S_FALSE;
    UInt32 blockSize = _bitStream.ReadByteInAligned();
    checkSum ^= blockSize;

    if (num != 0)
    {
      unsigned b = _bitStream.ReadByteInAligned();
      checkSum ^= b;
      blockSize += (UInt32)b << 8;
      if (num > 1)
      {
        b = _bitStream.ReadByteInAligned();
        checkSum ^= b;
        blockSize += (UInt32)b << 16;
      }
    }
    
    if (checkSum != 0x5A)
      return S_FALSE;

    unsigned blockSizeBits7 = (flags & 7) + 1;
    blockSize += (blockSizeBits7 >> 3);
    if (blockSize == 0)
      return S_FALSE;
    blockSize--;
    blockSizeBits7 &= 7;

    _bitStream._blockEndBits7 = (Byte)blockSizeBits7;
    _bitStream._blockEnd = _bitStream.GetProcessedSize_Round() + blockSize;
    
    _bitStream.SetCheck2();
    
    _isLastBlock = ((flags & 0x40) != 0);
    
    if ((flags & 0x80) == 0)
    {
      if (!_tableWasFilled)
        if (blockSize != 0 || blockSizeBits7 != 0)
          return S_FALSE;
      return S_OK;
    }
    
    _tableWasFilled = false;
  }

  {
    Byte lens2[kLevelTableSize];
    
    for (unsigned i = 0; i < kLevelTableSize;)
    {
      _bitStream.Prepare();
      const unsigned len = (unsigned)_bitStream.ReadBits9fix(4);
      if (len == 15)
      {
        unsigned num = (unsigned)_bitStream.ReadBits9fix(4);
        if (num != 0)
        {
          num += 2;
          num += i;
          if (num > kLevelTableSize)
            num = kLevelTableSize;
          do
            lens2[i++] = 0;
          while (i < num);
          continue;
        }
      }
      lens2[i++] = (Byte)len;
    }

    if (_bitStream.IsBlockOverRead())
      return S_FALSE;
    
    RIF(m_LevelDecoder.Build(lens2))
  }
  
  Byte lens[kTablesSizesSum];
  unsigned i = 0;
  
  do
  {
    if (_bitStream._buf >= _bitStream._bufCheck2)
    {
      if (_bitStream._buf >= _bitStream._bufCheck)
        _bitStream.Prepare();
      if (_bitStream.IsBlockOverRead())
        return S_FALSE;
    }
    
    const UInt32 sym = m_LevelDecoder.Decode(&_bitStream);
    
    if (sym < 16)
      lens[i++] = (Byte)sym;
    else if (sym > kLevelTableSize)
      return S_FALSE;
    else
    {
      unsigned num = ((sym - 16) & 1) * 4;
      num += num + 3 + (unsigned)_bitStream.ReadBits9(num + 3);
      num += i;
      if (num > kTablesSizesSum)
        num = kTablesSizesSum;
      Byte v = 0;
      if (sym < 16 + 2)
      {
        if (i == 0)
          return S_FALSE;
        v = lens[(size_t)i - 1];
      }
      do
        lens[i++] = v;
      while (i < num);
    }
  }
  while (i < kTablesSizesSum);

  if (_bitStream.IsBlockOverRead())
    return S_FALSE;
  if (_bitStream.InputEofError())
    return S_FALSE;

  RIF(m_MainDecoder.Build(&lens[0]))
  RIF(m_DistDecoder.Build(&lens[kMainTableSize]))
  RIF(m_AlignDecoder.Build(&lens[kMainTableSize + kDistTableSize]))
  RIF(m_LenDecoder.Build(&lens[kMainTableSize + kDistTableSize + kAlignTableSize]))

  _useAlignBits = false;
  // _useAlignBits = true;
  for (i = 0; i < kAlignTableSize; i++)
    if (lens[kMainTableSize + kDistTableSize + (size_t)i] != kNumAlignBits)
    {
      _useAlignBits = true;
      break;
    }

  _tableWasFilled = true;
  return S_OK;
}


static inline unsigned SlotToLen(CBitDecoder &_bitStream, unsigned slot)
{
  if (slot < 8)
    return slot + 2;
  const unsigned numBits = (slot >> 2) - 1;
  return 2 + ((4 | (slot & 3)) << numBits) + _bitStream.ReadBits9(numBits);
}


static const UInt32 kSymbolRep = 258;
// static const unsigned kMaxMatchLen = 0x1001 + 3;

HRESULT CDecoder::DecodeLZ()
{
  CBitDecoder _bitStream;
  _bitStream._stream = _inStream;
  _bitStream._bufBase = _inputBuf;
  _bitStream.Init();

  UInt32 rep0 = _reps[0];

  UInt32 remLen = 0;

  size_t limit;
  {
    size_t rem = _winSize - _winPos;
    if (rem > kWriteStep)
      rem = kWriteStep;
    limit = _winPos + rem;
  }
  
  for (;;)
  {
    if (_winPos >= limit)
    {
      RINOK(WriteBuf())
      if (_unpackSize_Defined && _writtenFileSize > _unpackSize)
        break; // return S_FALSE;
      
      {
        size_t rem = _winSize - _winPos;
        
        if (rem == 0)
        {
          _winPos = 0;
          rem = _winSize;
        }
        if (rem > kWriteStep)
          rem = kWriteStep;
        limit = _winPos + rem;
      }

      if (remLen != 0)
      {
        size_t winPos = _winPos;
        const size_t winMask = _winMask;
        size_t pos = (winPos - (size_t)rep0 - 1) & winMask;
        
        Byte *win = _window;
        do
        {
          if (winPos >= limit)
            break;
          win[winPos] = win[pos];
          winPos++;
          pos = (pos + 1) & winMask;
        }
        while (--remLen != 0);
        
        _lzSize += winPos - _winPos;
        _winPos = winPos;
        continue;
      }
    }

    if (_bitStream._buf >= _bitStream._bufCheck2)
    {
      if (_bitStream.InputEofError())
        break; // return S_FALSE;
      if (_bitStream._buf >= _bitStream._bufCheck)
        _bitStream.Prepare2();

      const UInt64 processed = _bitStream.GetProcessedSize_Round();
      if (processed >= _bitStream._blockEnd)
      {
        if (processed > _bitStream._blockEnd)
          break; // return S_FALSE;
        {
          const unsigned bits7 = _bitStream.GetProcessedBits7();
          if (bits7 > _bitStream._blockEndBits7)
            break; // return S_FALSE;
          if (bits7 == _bitStream._blockEndBits7)
          {
            if (_isLastBlock)
            {
              _reps[0] = rep0;
              
              if (_bitStream.InputEofError())
                break;
              
              /*
              // packSize can be 15 bytes larger for encrypted archive
              if (_packSize_Defined && _packSize < _bitStream.GetProcessedSize())
                break;
              */
              
              return _bitStream._hres;
              // break;
            }
            RINOK(ReadTables(_bitStream))
            continue;
          }
        }
      }

      // that check is not required, but it can help, if there is BUG in another code
      if (!_tableWasFilled)
        break; // return S_FALSE;
    }

    const UInt32 sym = m_MainDecoder.Decode(&_bitStream);
    
    if (sym < 256)
    {
      size_t winPos = _winPos;
      _window[winPos] = (Byte)sym;
      _winPos = winPos + 1;
      _lzSize++;
      continue;
    }
   
    UInt32 len;

    if (sym < kSymbolRep + kNumReps)
    {
      if (sym >= kSymbolRep)
      {
        if (sym != kSymbolRep)
        {
          UInt32 dist;
          if (sym == kSymbolRep + 1)
            dist = _reps[1];
          else
          {
            if (sym == kSymbolRep + 2)
              dist = _reps[2];
            else
            {
              dist = _reps[3];
              _reps[3] = _reps[2];
            }
            _reps[2] = _reps[1];
          }
          _reps[1] = rep0;
          rep0 = dist;
        }
        
        const UInt32 sym2 = m_LenDecoder.Decode(&_bitStream);
        if (sym2 >= kLenTableSize)
          break; // return S_FALSE;
        len = SlotToLen(_bitStream, sym2);
      }
      else
      {
        if (sym == 256)
        {
          RINOK(AddFilter(_bitStream))
          continue;
        }
        else // if (sym == 257)
        {
          len = _lastLen;
          // if (len = 0), we ignore that symbol, like original unRAR code, but it can mean error in stream.
          // if (len == 0) return S_FALSE;
          if (len == 0)
            continue;
        }
      }
    }
    else if (sym >= kMainTableSize)
      break; // return S_FALSE;
    else
    {
      _reps[3] = _reps[2];
      _reps[2] = _reps[1];
      _reps[1] = rep0;
      len = SlotToLen(_bitStream, sym - (kSymbolRep + kNumReps));
      
      rep0 = m_DistDecoder.Decode(&_bitStream);
      
      if (rep0 >= 4)
      {
        if (rep0 >= _numCorrectDistSymbols)
          break; // return S_FALSE;
        const unsigned numBits = (rep0 >> 1) - 1;
        rep0 = (2 | (rep0 & 1)) << numBits;
        
        if (numBits < kNumAlignBits)
          rep0 += _bitStream.ReadBits9(numBits);
        else
        {
          len += (numBits >= 7);
          len += (numBits >= 12);
          len += (numBits >= 17);
        
          if (_useAlignBits)
          {
            // if (numBits > kNumAlignBits)
              rep0 += (_bitStream.ReadBits32(numBits - kNumAlignBits) << kNumAlignBits);
            const UInt32 a = m_AlignDecoder.Decode(&_bitStream);
            if (a >= kAlignTableSize)
              break; // return S_FALSE;
            rep0 += a;
          }
          else
            rep0 += _bitStream.ReadBits32(numBits);
        }
      }
    }

    _lastLen = len;

    if (rep0 >= _lzSize)
      _lzError = true;
    
    {
      UInt32 lenCur = len;
      size_t winPos = _winPos;
      size_t pos = (winPos - (size_t)rep0 - 1) & _winMask;
      {
        const size_t rem = limit - winPos;
        // size_t rem = _winSize - winPos;

        if (lenCur > rem)
        {
          lenCur = (UInt32)rem;
          remLen = len - lenCur;
        }
      }
      
      Byte *win = _window;
      _lzSize += lenCur;
      _winPos = winPos + lenCur;
      if (_winSize - pos >= lenCur)
      {
        const Byte *src = win + pos;
        Byte *dest = win + winPos;
        do
          *dest++ = *src++;
        while (--lenCur != 0);
      }
      else
      {
        do
        {
          win[winPos] = win[pos];
          winPos++;
          pos = (pos + 1) & _winMask;
        }
        while (--lenCur != 0);
      }
    }
  }
  
  if (_bitStream._hres != S_OK)
    return _bitStream._hres;

  return S_FALSE;
}


HRESULT CDecoder::CodeReal()
{
  _unsupportedFilter = false;
  _lzError = false;
  _writeError = false;

  if (!_isSolid || !_wasInit)
  {
    size_t clearSize = _winSize;
    if (_lzSize < _winSize)
      clearSize = (size_t)_lzSize;
    memset(_window, 0, clearSize);

    _wasInit = true;
    _lzSize = 0;
    _lzWritten = 0;
    _winPos = 0;
    
    for (unsigned i = 0; i < kNumReps; i++)
      _reps[i] = (UInt32)0 - 1;

    _lastLen = 0;
    _tableWasFilled = false;
  }

  _isLastBlock = false;

  InitFilters();

  _filterEnd = 0;
  _writtenFileSize = 0;

  _lzFileStart = _lzSize;
  _lzWritten = _lzSize;
  
  HRESULT res = DecodeLZ();

  HRESULT res2 = S_OK;
  if (!_writeError && res != E_OUTOFMEMORY)
    res2 = WriteBuf();

  /*
  if (res == S_OK)
    if (InputEofError())
      res = S_FALSE;
  */

  if (res == S_OK)
  {
    _solidAllowed = true;
    res = res2;
  }
     
  if (res == S_OK && _unpackSize_Defined && _writtenFileSize != _unpackSize)
    return S_FALSE;
  return res;
}


// Original unRAR claims that maximum possible filter block size is (1 << 16) now,
// and (1 << 17) is minimum win size required to support filter.
// Original unRAR uses (1 << 18) for "extra safety and possible filter area size expansion"
// We can use any win size.

static const unsigned kWinSize_Log_Min = 17;

Z7_COM7F_IMF(CDecoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 *outSize, ICompressProgressInfo *progress))
{
  try
  {
    if (_isSolid && !_solidAllowed)
      return S_FALSE;
    _solidAllowed = false;

    if (_dictSizeLog >= sizeof(size_t) * 8)
      return E_NOTIMPL;

    if (!_isSolid)
      _lzEnd = 0;
    else
    {
      if (_lzSize < _lzEnd)
      {
        if (_window)
        {
          UInt64 rem = _lzEnd - _lzSize;
          if (rem >= _winSize)
            memset(_window, 0, _winSize);
          else
          {
            const size_t pos = (size_t)_lzSize & _winSize;
            size_t rem2 = _winSize - pos;
            if (rem2 > rem)
              rem2 = (size_t)rem;
            memset(_window + pos, 0, rem2);
            rem -= rem2;
            memset(_window, 0, (size_t)rem);
          }
        }
        _lzEnd &= ((((UInt64)1) << 33) - 1);
        _lzSize = _lzEnd;
        _winPos = (size_t)(_lzSize & _winSize);
      }
      _lzEnd = _lzSize;
    }

    size_t newSize;
    {
      unsigned newSizeLog = _dictSizeLog;
      if (newSizeLog < kWinSize_Log_Min)
        newSizeLog = kWinSize_Log_Min;
      newSize = (size_t)1 << newSizeLog;
      _numCorrectDistSymbols = newSizeLog * 2;
    }

    // If dictionary was reduced, we use allocated dictionary block
    // for compatibility with original unRAR decoder.

    if (_window && newSize < _winSizeAllocated)
      _winSize = _winSizeAllocated;
    else if (!_window || _winSize != newSize)
    {
      if (!_isSolid)
      {
        ::MidFree(_window);
        _window = NULL;
        _winSizeAllocated = 0;
      }

      Byte *win;

      {
        win = (Byte *)::MidAlloc(newSize);
        if (!win)
          return E_OUTOFMEMORY;
        memset(win, 0, newSize);
      }
      
      if (_isSolid && _window)
      {
        // original unRAR claims:
        // "Archiving code guarantees that win size does not grow in the same solid stream",
        // but the original unRAR decoder still supports such grow case.
        
        Byte *winOld = _window;
        const size_t oldSize = _winSize;
        const size_t newMask = newSize - 1;
        const size_t oldMask = _winSize - 1;
        const size_t winPos = _winPos;
        for (size_t i = 1; i <= oldSize; i++)
          win[(winPos - i) & newMask] = winOld[(winPos - i) & oldMask];
        ::MidFree(_window);
      }
      
      _window = win;
      _winSizeAllocated = newSize;
      _winSize = newSize;
    }

    _winMask = _winSize - 1;
    _winPos &= _winMask;

    if (!_inputBuf)
    {
      _inputBuf = (Byte *)::MidAlloc(kInputBufSize);
      if (!_inputBuf)
        return E_OUTOFMEMORY;
    }

    _inStream = inStream;
    _outStream = outStream;

    /*
    _packSize = 0;
    _packSize_Defined = (inSize != NULL);
    if (_packSize_Defined)
      _packSize = *inSize;
    */
    
    _unpackSize = 0;
    _unpackSize_Defined = (outSize != NULL);
    if (_unpackSize_Defined)
      _unpackSize = *outSize;

    if ((Int64)_unpackSize >= 0)
      _lzEnd += _unpackSize;
    else
      _lzEnd = 0;
    
    _progress = progress;
    
    const HRESULT res = CodeReal();
    
    if (res != S_OK)
      return res;
    if (_lzError)
      return S_FALSE;
    if (_unsupportedFilter)
      return E_NOTIMPL;
    return S_OK;
  }
  // catch(const CInBufferException &e)  { return e.ErrorCode; }
  // catch(...) { return S_FALSE; }
  catch(...) { return E_OUTOFMEMORY; }
  // CNewException is possible here. But probably CNewException is caused
  // by error in data stream.
}

Z7_COM7F_IMF(CDecoder::SetDecoderProperties2(const Byte *data, UInt32 size))
{
  if (size != 2)
    return E_NOTIMPL;
  _dictSizeLog = (Byte)((data[0] & 0xF) + 17);
  _isSolid = ((data[1] & 1) != 0);
  return S_OK;
}

}}

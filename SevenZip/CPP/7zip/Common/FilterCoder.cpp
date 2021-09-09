// FilterCoder.cpp

#include "StdAfx.h"

#include "../../Common/Defs.h"

#include "FilterCoder.h"
#include "StreamUtils.h"

#ifdef _WIN32
  #define alignedMidBuffer_Alloc g_MidAlloc
#else
  #define alignedMidBuffer_Alloc g_AlignedAlloc
#endif

CAlignedMidBuffer::~CAlignedMidBuffer()
{
  ISzAlloc_Free(&alignedMidBuffer_Alloc, _buf);
}

void CAlignedMidBuffer::AllocAligned(size_t size)
{
  ISzAlloc_Free(&alignedMidBuffer_Alloc, _buf);
  _buf = (Byte *)ISzAlloc_Alloc(&alignedMidBuffer_Alloc, size);
}

/*
  AES filters need 16-bytes alignment for HARDWARE-AES instructions.
  So we call IFilter::Filter(, size), where (size != 16 * N) only for last data block.

  AES-CBC filters need data size aligned for 16-bytes.
  So the encoder can add zeros to the end of original stream.

  Some filters (BCJ and others) don't process data at the end of stream in some cases.
  So the encoder and decoder write such last bytes without change.
*/


static const UInt32 kBufSize = 1 << 20;

STDMETHODIMP CFilterCoder::SetInBufSize(UInt32 , UInt32 size) { _inBufSize = size; return S_OK; }
STDMETHODIMP CFilterCoder::SetOutBufSize(UInt32 , UInt32 size) { _outBufSize = size; return S_OK; }

HRESULT CFilterCoder::Alloc()
{
  UInt32 size = MyMin(_inBufSize, _outBufSize);
  /* minimal bufSize is 16 bytes for AES and IA64 filter.
     bufSize for AES must be aligned for 16 bytes.
     We use (1 << 12) min size to support future aligned filters. */
  const UInt32 kMinSize = 1 << 12;
  size &= ~(UInt32)(kMinSize - 1);
  if (size < kMinSize)
    size = kMinSize;
  if (!_buf || _bufSize != size)
  {
    AllocAligned(size);
    if (!_buf)
      return E_OUTOFMEMORY;
    _bufSize = size;
  }
  return S_OK;
}

HRESULT CFilterCoder::Init_and_Alloc()
{
  RINOK(Filter->Init());
  return Alloc();
}

CFilterCoder::CFilterCoder(bool encodeMode):
    _bufSize(0),
    _inBufSize(kBufSize),
    _outBufSize(kBufSize),
    _encodeMode(encodeMode),
    _outSizeIsDefined(false),
    _outSize(0),
    _nowPos64(0)
  {}

CFilterCoder::~CFilterCoder()
{
}

STDMETHODIMP CFilterCoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 *outSize, ICompressProgressInfo *progress)
{
  RINOK(Init_and_Alloc());
  
  UInt64 prev = 0;
  UInt64 nowPos64 = 0;
  bool inputFinished = false;
  UInt32 pos = 0;

  while (!outSize || nowPos64 < *outSize)
  {
    if (!inputFinished)
    {
      size_t processedSize = _bufSize - pos;
      RINOK(ReadStream(inStream, _buf + pos, &processedSize));
      pos += (UInt32)processedSize;
      inputFinished = (pos != _bufSize);
    }

    if (pos == 0)
      return S_OK;

    UInt32 filtered = Filter->Filter(_buf, pos);
    
    if (filtered > pos)
    {
      // AES
      if (!inputFinished || filtered > _bufSize)
        return E_FAIL;
      if (!_encodeMode)
        return S_FALSE;
      
      Byte *buf = _buf;
      do
        buf[pos] = 0;
      while (++pos != filtered);
      
      if (filtered != Filter->Filter(buf, filtered))
        return E_FAIL;
    }

    UInt32 size = (filtered != 0 ? filtered : pos);
    if (outSize)
    {
      const UInt64 remSize = *outSize - nowPos64;
      if (size > remSize)
        size = (UInt32)remSize;
    }
    
    RINOK(WriteStream(outStream, _buf, size));
    nowPos64 += size;

    if (filtered == 0)
      return S_OK;
    pos -= filtered;
    for (UInt32 i = 0; i < pos; i++)
      _buf[i] = _buf[filtered++];

    if (progress && (nowPos64 - prev) >= (1 << 22))
    {
      prev = nowPos64;
      RINOK(progress->SetRatioInfo(&nowPos64, &nowPos64));
    }
  }

  return S_OK;
}



// ---------- Write to Filter ----------

STDMETHODIMP CFilterCoder::SetOutStream(ISequentialOutStream *outStream)
{
  _outStream = outStream;
  return S_OK;
}

STDMETHODIMP CFilterCoder::ReleaseOutStream()
{
  _outStream.Release();
  return S_OK;
}

HRESULT CFilterCoder::Flush2()
{
  while (_convSize != 0)
  {
    UInt32 num = _convSize;
    if (_outSizeIsDefined)
    {
      UInt64 rem = _outSize - _nowPos64;
      if (num > rem)
        num = (UInt32)rem;
      if (num == 0)
        return k_My_HRESULT_WritingWasCut;
    }
    
    UInt32 processed = 0;
    HRESULT res = _outStream->Write(_buf + _convPos, num, &processed);
    if (processed == 0)
      return res != S_OK ? res : E_FAIL;
    
    _convPos += processed;
    _convSize -= processed;
    _nowPos64 += processed;
    RINOK(res);
  }
    
  if (_convPos != 0)
  {
    UInt32 num = _bufPos - _convPos;
    for (UInt32 i = 0; i < num; i++)
      _buf[i] = _buf[_convPos + i];
    _bufPos = num;
    _convPos = 0;
  }
    
  return S_OK;
}

STDMETHODIMP CFilterCoder::Write(const void *data, UInt32 size, UInt32 *processedSize)
{
  if (processedSize)
    *processedSize = 0;
  
  while (size != 0)
  {
    RINOK(Flush2());

    // _convSize is 0
    // _convPos is 0
    // _bufPos is small

    if (_bufPos != _bufSize)
    {
      UInt32 num = MyMin(size, _bufSize - _bufPos);
      memcpy(_buf + _bufPos, data, num);
      size -= num;
      data = (const Byte *)data + num;
      if (processedSize)
        *processedSize += num;
      _bufPos += num;
      if (_bufPos != _bufSize)
        continue;
    }

    // _bufPos == _bufSize
    _convSize = Filter->Filter(_buf, _bufPos);
    
    if (_convSize == 0)
      break;
    if (_convSize > _bufPos)
    {
      // that case is not possible.
      _convSize = 0;
      return E_FAIL;
    }
  }

  return S_OK;
}

STDMETHODIMP CFilterCoder::OutStreamFinish()
{
  for (;;)
  {
    RINOK(Flush2());
    if (_bufPos == 0)
      break;
    _convSize = Filter->Filter(_buf, _bufPos);
    if (_convSize == 0)
      _convSize = _bufPos;
    else if (_convSize > _bufPos)
    {
      // AES
      if (_convSize > _bufSize)
      {
        _convSize = 0;
        return E_FAIL;
      }
      if (!_encodeMode)
      {
        _convSize = 0;
        return S_FALSE;
      }
      for (; _bufPos < _convSize; _bufPos++)
        _buf[_bufPos] = 0;
      _convSize = Filter->Filter(_buf, _bufPos);
      if (_convSize != _bufPos)
        return E_FAIL;
    }
  }
  
  CMyComPtr<IOutStreamFinish> finish;
  _outStream.QueryInterface(IID_IOutStreamFinish, &finish);
  if (finish)
    return finish->OutStreamFinish();
  return S_OK;
}

// ---------- Init functions ----------

STDMETHODIMP CFilterCoder::InitEncoder()
{
  InitSpecVars();
  return Init_and_Alloc();
}

HRESULT CFilterCoder::Init_NoSubFilterInit()
{
  InitSpecVars();
  return Alloc();
}

STDMETHODIMP CFilterCoder::SetOutStreamSize(const UInt64 *outSize)
{
  InitSpecVars();
  if (outSize)
  {
    _outSize = *outSize;
    _outSizeIsDefined = true;
  }
  return Init_and_Alloc();
}

// ---------- Read from Filter ----------

STDMETHODIMP CFilterCoder::SetInStream(ISequentialInStream *inStream)
{
  _inStream = inStream;
  return S_OK;
}

STDMETHODIMP CFilterCoder::ReleaseInStream()
{
  _inStream.Release();
  return S_OK;
}


STDMETHODIMP CFilterCoder::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  if (processedSize)
    *processedSize = 0;
  
  while (size != 0)
  {
    if (_convSize != 0)
    {
      if (size > _convSize)
        size = _convSize;
      if (_outSizeIsDefined)
      {
        UInt64 rem = _outSize - _nowPos64;
        if (size > rem)
          size = (UInt32)rem;
      }
      memcpy(data, _buf + _convPos, size);
      _convPos += size;
      _convSize -= size;
      _nowPos64 += size;
      if (processedSize)
        *processedSize = size;
      break;
    }
  
    if (_convPos != 0)
    {
      UInt32 num = _bufPos - _convPos;
      for (UInt32 i = 0; i < num; i++)
        _buf[i] = _buf[_convPos + i];
      _bufPos = num;
      _convPos = 0;
    }
    
    {
      size_t readSize = _bufSize - _bufPos;
      HRESULT res = ReadStream(_inStream, _buf + _bufPos, &readSize);
      _bufPos += (UInt32)readSize;
      RINOK(res);
    }
    
    _convSize = Filter->Filter(_buf, _bufPos);
    
    if (_convSize == 0)
    {
      if (_bufPos == 0)
        break;
      // BCJ
      _convSize = _bufPos;
      continue;
    }
    
    if (_convSize > _bufPos)
    {
      // AES
      if (_convSize > _bufSize)
        return E_FAIL;
      if (!_encodeMode)
        return S_FALSE;
      
      do
        _buf[_bufPos] = 0;
      while (++_bufPos != _convSize);
      
      _convSize = Filter->Filter(_buf, _convSize);
      if (_convSize != _bufPos)
        return E_FAIL;
    }
  }
 
  return S_OK;
}


#ifndef _NO_CRYPTO

STDMETHODIMP CFilterCoder::CryptoSetPassword(const Byte *data, UInt32 size)
  { return _SetPassword->CryptoSetPassword(data, size); }

STDMETHODIMP CFilterCoder::SetKey(const Byte *data, UInt32 size)
  { return _CryptoProperties->SetKey(data, size); }

STDMETHODIMP CFilterCoder::SetInitVector(const Byte *data, UInt32 size)
  { return _CryptoProperties->SetInitVector(data, size); }

#endif


#ifndef EXTRACT_ONLY

STDMETHODIMP CFilterCoder::SetCoderProperties(const PROPID *propIDs,
    const PROPVARIANT *properties, UInt32 numProperties)
  { return _SetCoderProperties->SetCoderProperties(propIDs, properties, numProperties); }

STDMETHODIMP CFilterCoder::WriteCoderProperties(ISequentialOutStream *outStream)
  { return _WriteCoderProperties->WriteCoderProperties(outStream); }

/*
STDMETHODIMP CFilterCoder::ResetSalt()
  { return _CryptoResetSalt->ResetSalt(); }
*/

STDMETHODIMP CFilterCoder::ResetInitVector()
  { return _CryptoResetInitVector->ResetInitVector(); }

#endif


STDMETHODIMP CFilterCoder::SetDecoderProperties2(const Byte *data, UInt32 size)
  { return _SetDecoderProperties2->SetDecoderProperties2(data, size); }

// NsisDecode.cpp

#include "StdAfx.h"

#include "../../../../C/CpuArch.h"

#include "NsisDecode.h"

#include "../../Common/CreateCoder.h"
#include "../../Common/LimitedStreams.h"
#include "../../Common/MethodId.h"

#include "../../Compress/BcjCoder.h"

#define Get32(p) GetUi32(p)

namespace NArchive {
namespace NNsis {

UInt64 CDecoder::GetInputProcessedSize() const
{
  if (_lzmaDecoder)
    return _lzmaDecoder->GetInputProcessedSize();
  if (_deflateDecoder)
    return _deflateDecoder->GetInputProcessedSize();
  if (_bzDecoder)
    return _bzDecoder->GetInputProcessedSize();
  return 0;
}


HRESULT CDecoder::Init(ISequentialInStream *inStream, bool &useFilter)
{
  useFilter = false;

  if (_decoderInStream)
    if (Method != _curMethod)
      Release();
  _curMethod = Method;
  
  if (!_codecInStream)
  {
    switch (Method)
    {
      // case NMethodType::kCopy: return E_NOTIMPL;
      case NMethodType::kDeflate:
        _deflateDecoder = new NCompress::NDeflate::NDecoder::CCOMCoder();
        _codecInStream = _deflateDecoder;
        break;
      case NMethodType::kBZip2:
        _bzDecoder = new NCompress::NBZip2::CNsisDecoder();
        _codecInStream = _bzDecoder;
        break;
      case NMethodType::kLZMA:
        _lzmaDecoder = new NCompress::NLzma::CDecoder();
        _codecInStream = _lzmaDecoder;
        break;
      default: return E_NOTIMPL;
    }
  }

  if (Method == NMethodType::kDeflate)
    _deflateDecoder->SetNsisMode(IsNsisDeflate);

  if (FilterFlag)
  {
    Byte flag;
    RINOK(ReadStream_FALSE(inStream, &flag, 1));
    if (flag > 1)
      return E_NOTIMPL;
    useFilter = (flag != 0);
  }
  
  if (!useFilter)
    _decoderInStream = _codecInStream;
  else
  {
    if (!_filterInStream)
    {
      _filter = new CFilterCoder(false);
      _filterInStream = _filter;
      _filter->Filter = new NCompress::NBcj::CCoder(false);
    }
    RINOK(_filter->SetInStream(_codecInStream));
    _decoderInStream = _filterInStream;
  }

  if (Method == NMethodType::kLZMA)
  {
    const unsigned kPropsSize = LZMA_PROPS_SIZE;
    Byte props[kPropsSize];
    RINOK(ReadStream_FALSE(inStream, props, kPropsSize));
    RINOK(_lzmaDecoder->SetDecoderProperties2((const Byte *)props, kPropsSize));
  }

  {
    CMyComPtr<ICompressSetInStream> setInStream;
    _codecInStream.QueryInterface(IID_ICompressSetInStream, &setInStream);
    if (!setInStream)
      return E_NOTIMPL;
    RINOK(setInStream->SetInStream(inStream));
  }

  {
    CMyComPtr<ICompressSetOutStreamSize> setOutStreamSize;
    _codecInStream.QueryInterface(IID_ICompressSetOutStreamSize, &setOutStreamSize);
    if (!setOutStreamSize)
      return E_NOTIMPL;
    RINOK(setOutStreamSize->SetOutStreamSize(NULL));
  }

  if (useFilter)
  {
    RINOK(_filter->SetOutStreamSize(NULL));
  }

  return S_OK;
}


static const UInt32 kMask_IsCompressed = (UInt32)1 << 31;


HRESULT CDecoder::SetToPos(UInt64 pos, ICompressProgressInfo *progress)
{
  if (StreamPos > pos)
    return E_FAIL;
  const UInt64 inSizeStart = GetInputProcessedSize();
  UInt64 offset = 0;
  while (StreamPos < pos)
  {
    size_t size = (size_t)MyMin(pos - StreamPos, (UInt64)Buffer.Size());
    RINOK(Read(Buffer, &size));
    if (size == 0)
      return S_FALSE;
    StreamPos += size;
    offset += size;

    const UInt64 inSize = GetInputProcessedSize() - inSizeStart;
    RINOK(progress->SetRatioInfo(&inSize, &offset));
  }
  return S_OK;
}


HRESULT CDecoder::Decode(CByteBuffer *outBuf, bool unpackSizeDefined, UInt32 unpackSize,
    ISequentialOutStream *realOutStream, ICompressProgressInfo *progress,
    UInt32 &packSizeRes, UInt32 &unpackSizeRes)
{
  CLimitedSequentialInStream *limitedStreamSpec = NULL;
  CMyComPtr<ISequentialInStream> limitedStream;
  packSizeRes = 0;
  unpackSizeRes = 0;

  if (Solid)
  {
    Byte temp[4];
    size_t processedSize = 4;
    RINOK(Read(temp, &processedSize));
    StreamPos += processedSize;
    if (processedSize != 4)
      return S_FALSE;
    UInt32 size = Get32(temp);
    if (unpackSizeDefined && size != unpackSize)
      return S_FALSE;
    unpackSize = size;
    unpackSizeDefined = true;
  }
  else
  {
    Byte temp[4];
    {
      size_t processedSize = 4;
      RINOK(ReadStream(InputStream, temp, &processedSize));
      StreamPos += processedSize;
      if (processedSize != 4)
        return S_FALSE;
    }
    UInt32 size = Get32(temp);

    if ((size & kMask_IsCompressed) == 0)
    {
      if (unpackSizeDefined && size != unpackSize)
        return S_FALSE;
      packSizeRes = size;
      if (outBuf)
        outBuf->Alloc(size);

      UInt64 offset = 0;
      
      while (size > 0)
      {
        UInt32 curSize = (UInt32)MyMin((size_t)size, Buffer.Size());
        UInt32 processedSize;
        RINOK(InputStream->Read(Buffer, curSize, &processedSize));
        if (processedSize == 0)
          return S_FALSE;
        if (outBuf)
          memcpy((Byte *)*outBuf + (size_t)offset, Buffer, processedSize);
        offset += processedSize;
        size -= processedSize;
        StreamPos += processedSize;
        unpackSizeRes += processedSize;
        if (realOutStream)
          RINOK(WriteStream(realOutStream, Buffer, processedSize));
        RINOK(progress->SetRatioInfo(&offset, &offset));
      }

      return S_OK;
    }
    
    size &= ~kMask_IsCompressed;
    packSizeRes = size;
    limitedStreamSpec = new CLimitedSequentialInStream;
    limitedStream = limitedStreamSpec;
    limitedStreamSpec->SetStream(InputStream);
    limitedStreamSpec->Init(size);
    {
      bool useFilter;
      RINOK(Init(limitedStream, useFilter));
    }
  }
  
  if (outBuf)
  {
    if (unpackSizeDefined)
      outBuf->Alloc(unpackSize);
  }

  const UInt64 inSizeStart = GetInputProcessedSize();

  // we don't allow files larger than 4 GB;
  if (!unpackSizeDefined)
    unpackSize = 0xFFFFFFFF;
  UInt32 offset = 0;

  HRESULT res = S_OK;

  for (;;)
  {
    size_t rem = unpackSize - offset;
    if (rem == 0)
      break;
    size_t size = Buffer.Size();
    if (size > rem)
      size = rem;
    RINOK(Read(Buffer, &size));
    if (size == 0)
    {
      if (unpackSizeDefined)
        res = S_FALSE;
      break;
    }
    
    if (outBuf)
    {
      size_t nextSize = offset + size;
      if (outBuf->Size() < nextSize)
      {
        {
          const size_t nextSize2 = outBuf->Size() * 2;
          if (nextSize < nextSize2)
            nextSize = nextSize2;
        }
        outBuf->ChangeSize_KeepData(nextSize, offset);
      }
      memcpy((Byte *)*outBuf + (size_t)offset, Buffer, size);
    }
    
    StreamPos += size;
    offset += (UInt32)size;

    const UInt64 inSize = GetInputProcessedSize() - inSizeStart;

    if (Solid)
      packSizeRes = (UInt32)inSize;
    unpackSizeRes += (UInt32)size;
    
    UInt64 outSize = offset;
    RINOK(progress->SetRatioInfo(&inSize, &outSize));
    if (realOutStream)
    {
      res = WriteStream(realOutStream, Buffer, size);
      if (res != S_OK)
        break;
    }
  }

  if (outBuf && offset != outBuf->Size())
    outBuf->ChangeSize_KeepData(offset, offset);
  
  return res;
}

}}

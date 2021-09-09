// Bcj2Coder.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"

#include "../Common/StreamUtils.h"

#include "Bcj2Coder.h"

namespace NCompress {
namespace NBcj2 {

CBaseCoder::CBaseCoder()
{
  for (int i = 0; i < BCJ2_NUM_STREAMS + 1; i++)
  {
    _bufs[i] = NULL;
    _bufsCurSizes[i] = 0;
    _bufsNewSizes[i] = (1 << 18);
  }
}

CBaseCoder::~CBaseCoder()
{
  for (int i = 0; i < BCJ2_NUM_STREAMS + 1; i++)
    ::MidFree(_bufs[i]);
}

HRESULT CBaseCoder::Alloc(bool allocForOrig)
{
  unsigned num = allocForOrig ? BCJ2_NUM_STREAMS + 1 : BCJ2_NUM_STREAMS;
  for (unsigned i = 0; i < num; i++)
  {
    UInt32 newSize = _bufsNewSizes[i];
    const UInt32 kMinBufSize = 1;
    if (newSize < kMinBufSize)
      newSize = kMinBufSize;
    if (!_bufs[i] || newSize != _bufsCurSizes[i])
    {
      if (_bufs[i])
      {
        ::MidFree(_bufs[i]);
        _bufs[i] = 0;
      }
      _bufsCurSizes[i] = 0;
      Byte *buf = (Byte *)::MidAlloc(newSize);
      _bufs[i] = buf;
      if (!buf)
        return E_OUTOFMEMORY;
      _bufsCurSizes[i] = newSize;
    }
  }
  return S_OK;
}



#ifndef EXTRACT_ONLY

CEncoder::CEncoder(): _relatLim(BCJ2_RELAT_LIMIT) {}
CEncoder::~CEncoder() {}

STDMETHODIMP CEncoder::SetInBufSize(UInt32, UInt32 size) { _bufsNewSizes[BCJ2_NUM_STREAMS] = size; return S_OK; }
STDMETHODIMP CEncoder::SetOutBufSize(UInt32 streamIndex, UInt32 size) { _bufsNewSizes[streamIndex] = size; return S_OK; }

STDMETHODIMP CEncoder::SetCoderProperties(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps)
{
  UInt32 relatLim = BCJ2_RELAT_LIMIT;
  
  for (UInt32 i = 0; i < numProps; i++)
  {
    const PROPVARIANT &prop = props[i];
    PROPID propID = propIDs[i];
    if (propID >= NCoderPropID::kReduceSize)
      continue;
    switch (propID)
    {
      /*
      case NCoderPropID::kDefaultProp:
      {
        if (prop.vt != VT_UI4)
          return E_INVALIDARG;
        UInt32 v = prop.ulVal;
        if (v > 31)
          return E_INVALIDARG;
        relatLim = (UInt32)1 << v;
        break;
      }
      */
      case NCoderPropID::kDictionarySize:
      {
        if (prop.vt != VT_UI4)
          return E_INVALIDARG;
        relatLim = prop.ulVal;
        if (relatLim > ((UInt32)1 << 31))
          return E_INVALIDARG;
        break;
      }

      case NCoderPropID::kNumThreads:
        continue;
      case NCoderPropID::kLevel:
        continue;
     
      default: return E_INVALIDARG;
    }
  }
  
  _relatLim = relatLim;
  
  return S_OK;
}


HRESULT CEncoder::CodeReal(ISequentialInStream * const *inStreams, const UInt64 * const *inSizes, UInt32 numInStreams,
    ISequentialOutStream * const *outStreams, const UInt64 * const * /* outSizes */, UInt32 numOutStreams,
    ICompressProgressInfo *progress)
{
  if (numInStreams != 1 || numOutStreams != BCJ2_NUM_STREAMS)
    return E_INVALIDARG;

  RINOK(Alloc());

  UInt32 fileSize_for_Conv = 0;
  if (inSizes && inSizes[0])
  {
    UInt64 inSize = *inSizes[0];
    if (inSize <= BCJ2_FileSize_MAX)
      fileSize_for_Conv = (UInt32)inSize;
  }

  CMyComPtr<ICompressGetSubStreamSize> getSubStreamSize;
  inStreams[0]->QueryInterface(IID_ICompressGetSubStreamSize, (void **)&getSubStreamSize);

  CBcj2Enc enc;
    
  enc.src = _bufs[BCJ2_NUM_STREAMS];
  enc.srcLim = enc.src;
    
  {
    for (int i = 0; i < BCJ2_NUM_STREAMS; i++)
    {
      enc.bufs[i] = _bufs[i];
      enc.lims[i] = _bufs[i] + _bufsCurSizes[i];
    }
  }

  size_t numBytes_in_ReadBuf = 0;
  UInt64 prevProgress = 0;
  UInt64 totalStreamRead = 0; // size read from InputStream
  UInt64 currentInPos = 0; // data that was processed, it doesn't include data in input buffer and data in enc.temp
  UInt64 outSizeRc = 0;

  Bcj2Enc_Init(&enc);

  enc.fileIp = 0;
  enc.fileSize = fileSize_for_Conv;

  enc.relatLimit = _relatLim;

  enc.finishMode = BCJ2_ENC_FINISH_MODE_CONTINUE;

  bool needSubSize = false;
  UInt64 subStreamIndex = 0;
  UInt64 subStreamStartPos = 0;
  bool readWasFinished = false;

  for (;;)
  {
    if (needSubSize && getSubStreamSize)
    {
      enc.fileIp = 0;
      enc.fileSize = fileSize_for_Conv;
      enc.finishMode = BCJ2_ENC_FINISH_MODE_CONTINUE;
      
      for (;;)
      {
        UInt64 subStreamSize = 0;
        HRESULT result = getSubStreamSize->GetSubStreamSize(subStreamIndex, &subStreamSize);
        needSubSize = false;
        
        if (result == S_OK)
        {
          UInt64 newEndPos = subStreamStartPos + subStreamSize;
          
          bool isAccurateEnd = (newEndPos < totalStreamRead ||
            (newEndPos <= totalStreamRead && readWasFinished));
          
          if (newEndPos <= currentInPos && isAccurateEnd)
          {
            subStreamStartPos = newEndPos;
            subStreamIndex++;
            continue;
          }
          
          enc.srcLim = _bufs[BCJ2_NUM_STREAMS] + numBytes_in_ReadBuf;
          
          if (isAccurateEnd)
          {
            // data in enc.temp is possible here
            size_t rem = (size_t)(totalStreamRead - newEndPos);
            
            /* Pos_of(enc.src) <= old newEndPos <= newEndPos
               in another case, it's fail in some code */
            if ((size_t)(enc.srcLim - enc.src) < rem)
              return E_FAIL;
            
            enc.srcLim -= rem;
            enc.finishMode = BCJ2_ENC_FINISH_MODE_END_BLOCK;
          }
          
          if (subStreamSize <= BCJ2_FileSize_MAX)
          {
            enc.fileIp = enc.ip + (UInt32)(subStreamStartPos - currentInPos);
            enc.fileSize = (UInt32)subStreamSize;
          }
          break;
        }
        
        if (result == S_FALSE)
          break;
        if (result == E_NOTIMPL)
        {
          getSubStreamSize.Release();
          break;
        }
        return result;
      }
    }

    if (readWasFinished && totalStreamRead - currentInPos == Bcj2Enc_Get_InputData_Size(&enc))
      enc.finishMode = BCJ2_ENC_FINISH_MODE_END_STREAM;

    Bcj2Enc_Encode(&enc);

    currentInPos = totalStreamRead - numBytes_in_ReadBuf + (size_t)(enc.src - _bufs[BCJ2_NUM_STREAMS]) - enc.tempPos;
    
    if (Bcj2Enc_IsFinished(&enc))
      break;

    if (enc.state < BCJ2_NUM_STREAMS)
    {
      const size_t curSize = (size_t)(enc.bufs[enc.state] - _bufs[enc.state]);
      // printf("Write stream = %2d %6d\n", enc.state, curSize);
      RINOK(WriteStream(outStreams[enc.state], _bufs[enc.state], curSize));
      if (enc.state == BCJ2_STREAM_RC)
        outSizeRc += curSize;

      enc.bufs[enc.state] = _bufs[enc.state];
      enc.lims[enc.state] = _bufs[enc.state] + _bufsCurSizes[enc.state];
    }
    else if (enc.state != BCJ2_ENC_STATE_ORIG)
      return E_FAIL;
    else
    {
      needSubSize = true;

      if (numBytes_in_ReadBuf != (size_t)(enc.src - _bufs[BCJ2_NUM_STREAMS]))
      {
        enc.srcLim = _bufs[BCJ2_NUM_STREAMS] + numBytes_in_ReadBuf;
        continue;
      }

      if (readWasFinished)
        continue;
      
      numBytes_in_ReadBuf = 0;
      enc.src    = _bufs[BCJ2_NUM_STREAMS];
      enc.srcLim = _bufs[BCJ2_NUM_STREAMS];
 
      UInt32 curSize = _bufsCurSizes[BCJ2_NUM_STREAMS];
      RINOK(inStreams[0]->Read(_bufs[BCJ2_NUM_STREAMS], curSize, &curSize));

      // printf("Read %6d bytes\n", curSize);
      if (curSize == 0)
      {
        readWasFinished = true;
        continue;
      }

      numBytes_in_ReadBuf = curSize;
      totalStreamRead += numBytes_in_ReadBuf;
      enc.srcLim = _bufs[BCJ2_NUM_STREAMS] + numBytes_in_ReadBuf;
    }

    if (progress && currentInPos - prevProgress >= (1 << 20))
    {
      const UInt64 outSize2 = currentInPos + outSizeRc + (size_t)(enc.bufs[BCJ2_STREAM_RC] - enc.bufs[BCJ2_STREAM_RC]);
      prevProgress = currentInPos;
      // printf("progress %8d, %8d\n", (int)inSize2, (int)outSize2);
      RINOK(progress->SetRatioInfo(&currentInPos, &outSize2));
    }
  }

  for (int i = 0; i < BCJ2_NUM_STREAMS; i++)
  {
    RINOK(WriteStream(outStreams[i], _bufs[i], (size_t)(enc.bufs[i] - _bufs[i])));
  }

  // if (currentInPos != subStreamStartPos + subStreamSize) return E_FAIL;

  return S_OK;
}

STDMETHODIMP CEncoder::Code(ISequentialInStream * const *inStreams, const UInt64 * const *inSizes, UInt32 numInStreams,
    ISequentialOutStream * const *outStreams, const UInt64 * const *outSizes, UInt32 numOutStreams,
    ICompressProgressInfo *progress)
{
  try
  {
    return CodeReal(inStreams, inSizes, numInStreams, outStreams, outSizes,numOutStreams, progress);
  }
  catch(...) { return E_FAIL; }
}

#endif






STDMETHODIMP CDecoder::SetInBufSize(UInt32 streamIndex, UInt32 size) { _bufsNewSizes[streamIndex] = size; return S_OK; }
STDMETHODIMP CDecoder::SetOutBufSize(UInt32 , UInt32 size) { _bufsNewSizes[BCJ2_NUM_STREAMS] = size; return S_OK; }

CDecoder::CDecoder(): _finishMode(false), _outSizeDefined(false), _outSize(0)
{}

STDMETHODIMP CDecoder::SetFinishMode(UInt32 finishMode)
{
  _finishMode = (finishMode != 0);
  return S_OK;
}

void CDecoder::InitCommon()
{
  {
    for (int i = 0; i < BCJ2_NUM_STREAMS; i++)
      dec.lims[i] = dec.bufs[i] = _bufs[i];
  }

  {
    for (int i = 0; i < BCJ2_NUM_STREAMS; i++)
    {
      _extraReadSizes[i] = 0;
      _inStreamsProcessed[i] = 0;
      _readRes[i] = S_OK;
    }
  }
    
  Bcj2Dec_Init(&dec);
}

HRESULT CDecoder::Code(ISequentialInStream * const *inStreams, const UInt64 * const *inSizes, UInt32 numInStreams,
    ISequentialOutStream * const *outStreams, const UInt64 * const *outSizes, UInt32 numOutStreams,
    ICompressProgressInfo *progress)
{
  if (numInStreams != BCJ2_NUM_STREAMS || numOutStreams != 1)
    return E_INVALIDARG;

  RINOK(Alloc());
    
  InitCommon();

  dec.destLim = dec.dest = _bufs[BCJ2_NUM_STREAMS];
  
  UInt64 outSizeProcessed = 0;
  UInt64 prevProgress = 0;

  HRESULT res = S_OK;

  for (;;)
  {
    if (Bcj2Dec_Decode(&dec) != SZ_OK)
      return S_FALSE;
    
    if (dec.state < BCJ2_NUM_STREAMS)
    {
      size_t totalRead = _extraReadSizes[dec.state];
      {
        Byte *buf = _bufs[dec.state];
        for (size_t i = 0; i < totalRead; i++)
          buf[i] = dec.bufs[dec.state][i];
        dec.lims[dec.state] =
        dec.bufs[dec.state] = buf;
      }

      if (_readRes[dec.state] != S_OK)
      {
        res = _readRes[dec.state];
        break;
      }

      do
      {
        UInt32 curSize = _bufsCurSizes[dec.state] - (UInt32)totalRead;
        /*
        we want to call Read even even if size is 0
        if (inSizes && inSizes[dec.state])
        {
          UInt64 rem = *inSizes[dec.state] - _inStreamsProcessed[dec.state];
          if (curSize > rem)
            curSize = (UInt32)rem;
        }
        */

        HRESULT res2 = inStreams[dec.state]->Read(_bufs[dec.state] + totalRead, curSize, &curSize);
        _readRes[dec.state] = res2;
        if (curSize == 0)
          break;
        _inStreamsProcessed[dec.state] += curSize;
        totalRead += curSize;
        if (res2 != S_OK)
          break;
      }
      while (totalRead < 4 && BCJ2_IS_32BIT_STREAM(dec.state));

      if (_readRes[dec.state] != S_OK)
        res = _readRes[dec.state];

      if (totalRead == 0)
        break;

      // res == S_OK;

      if (BCJ2_IS_32BIT_STREAM(dec.state))
      {
        unsigned extraSize = ((unsigned)totalRead & 3);
        _extraReadSizes[dec.state] = extraSize;
        if (totalRead < 4)
        {
          res = (_readRes[dec.state] != S_OK) ? _readRes[dec.state] : S_FALSE;
          break;
        }
        totalRead -= extraSize;
      }

      dec.lims[dec.state] = _bufs[dec.state] + totalRead;
    }
    else // if (dec.state <= BCJ2_STATE_ORIG)
    {
      const size_t curSize = (size_t)(dec.dest - _bufs[BCJ2_NUM_STREAMS]);
      if (curSize != 0)
      {
        outSizeProcessed += curSize;
        RINOK(WriteStream(outStreams[0], _bufs[BCJ2_NUM_STREAMS], curSize));
      }
      dec.dest = _bufs[BCJ2_NUM_STREAMS];
      {
        size_t rem = _bufsCurSizes[BCJ2_NUM_STREAMS];
        if (outSizes && outSizes[0])
        {
          UInt64 outSize = *outSizes[0] - outSizeProcessed;
          if (rem > outSize)
            rem = (size_t)outSize;
        }
        dec.destLim = dec.dest + rem;
        if (rem == 0)
          break;
      }
    }

    if (progress)
    {
      const UInt64 outSize2 = outSizeProcessed + (size_t)(dec.dest - _bufs[BCJ2_NUM_STREAMS]);
      if (outSize2 - prevProgress >= (1 << 22))
      {
        const UInt64 inSize2 = outSize2 + _inStreamsProcessed[BCJ2_STREAM_RC] - (size_t)(dec.lims[BCJ2_STREAM_RC] - dec.bufs[BCJ2_STREAM_RC]);
        RINOK(progress->SetRatioInfo(&inSize2, &outSize2));
        prevProgress = outSize2;
      }
    }
  }

  const size_t curSize = (size_t)(dec.dest - _bufs[BCJ2_NUM_STREAMS]);
  if (curSize != 0)
  {
    outSizeProcessed += curSize;
    RINOK(WriteStream(outStreams[0], _bufs[BCJ2_NUM_STREAMS], curSize));
  }

  if (res != S_OK)
    return res;

  if (_finishMode)
  {
    if (!Bcj2Dec_IsFinished(&dec))
      return S_FALSE;

    // we still allow the cases when input streams are larger than required for decoding.
    // so the case (dec.state == BCJ2_STATE_ORIG) is also allowed, if MAIN stream is larger than required.
    if (dec.state != BCJ2_STREAM_MAIN &&
        dec.state != BCJ2_DEC_STATE_ORIG)
      return S_FALSE;

    if (inSizes)
    {
      for (int i = 0; i < BCJ2_NUM_STREAMS; i++)
      {
        const size_t rem = (size_t)(dec.lims[i] - dec.bufs[i]) + _extraReadSizes[i];
        /*
        if (rem != 0)
          return S_FALSE;
        */
        if (inSizes[i] && *inSizes[i] != _inStreamsProcessed[i] - rem)
          return S_FALSE;
      }
    }
  }

  return S_OK;
}

STDMETHODIMP CDecoder::SetInStream2(UInt32 streamIndex, ISequentialInStream *inStream)
{
  _inStreams[streamIndex] = inStream;
  return S_OK;
}

STDMETHODIMP CDecoder::ReleaseInStream2(UInt32 streamIndex)
{
  _inStreams[streamIndex].Release();
  return S_OK;
}

STDMETHODIMP CDecoder::SetOutStreamSize(const UInt64 *outSize)
{
  _outSizeDefined = (outSize != NULL);
  _outSize = 0;
  if (_outSizeDefined)
    _outSize = *outSize;

  _outSize_Processed = 0;

  HRESULT res = Alloc(false);
  
  InitCommon();
  dec.destLim = dec.dest = NULL;

  return res;
}


STDMETHODIMP CDecoder::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  if (processedSize)
    *processedSize = 0;

  if (size == 0)
    return S_OK;

  UInt32 totalProcessed = 0;
 
  if (_outSizeDefined)
  {
    UInt64 rem = _outSize - _outSize_Processed;
    if (size > rem)
      size = (UInt32)rem;
  }
  dec.dest = (Byte *)data;
  dec.destLim = (const Byte *)data + size;

  HRESULT res = S_OK;

  for (;;)
  {
    SRes sres = Bcj2Dec_Decode(&dec);
    if (sres != SZ_OK)
      return S_FALSE;
    
    {
      UInt32 curSize = (UInt32)(dec.dest - (Byte *)data);
      if (curSize != 0)
      {
        totalProcessed += curSize;
        if (processedSize)
          *processedSize = totalProcessed;
        data = (void *)((Byte *)data + curSize);
        size -= curSize;
        _outSize_Processed += curSize;
      }
    }

    if (dec.state >= BCJ2_NUM_STREAMS)
      break;

    {
      size_t totalRead = _extraReadSizes[dec.state];
      {
        Byte *buf = _bufs[dec.state];
        for (size_t i = 0; i < totalRead; i++)
          buf[i] = dec.bufs[dec.state][i];
        dec.lims[dec.state] =
        dec.bufs[dec.state] = buf;
      }

      if (_readRes[dec.state] != S_OK)
        return _readRes[dec.state];

      do
      {
        UInt32 curSize = _bufsCurSizes[dec.state] - (UInt32)totalRead;
        HRESULT res2 = _inStreams[dec.state]->Read(_bufs[dec.state] + totalRead, curSize, &curSize);
        _readRes[dec.state] = res2;
        if (curSize == 0)
          break;
        _inStreamsProcessed[dec.state] += curSize;
        totalRead += curSize;
        if (res2 != S_OK)
          break;
      }
      while (totalRead < 4 && BCJ2_IS_32BIT_STREAM(dec.state));

      if (totalRead == 0)
      {
        if (totalProcessed == 0)
          res = _readRes[dec.state];
        break;
      }

      if (BCJ2_IS_32BIT_STREAM(dec.state))
      {
        unsigned extraSize = ((unsigned)totalRead & 3);
        _extraReadSizes[dec.state] = extraSize;
        if (totalRead < 4)
        {
          if (totalProcessed != 0)
            return S_OK;
          return (_readRes[dec.state] != S_OK) ? _readRes[dec.state] : S_FALSE;
        }
        totalRead -= extraSize;
      }

      dec.lims[dec.state] = _bufs[dec.state] + totalRead;
    }
  }

  if (_finishMode && _outSizeDefined && _outSize == _outSize_Processed)
  {
    if (!Bcj2Dec_IsFinished(&dec))
      return S_FALSE;

    if (dec.state != BCJ2_STREAM_MAIN &&
        dec.state != BCJ2_DEC_STATE_ORIG)
      return S_FALSE;
  
    /*
    for (int i = 0; i < BCJ2_NUM_STREAMS; i++)
      if (dec.bufs[i] != dec.lims[i] || _extraReadSizes[i] != 0)
        return S_FALSE;
    */
  }

  return res;
}


STDMETHODIMP CDecoder::GetInStreamProcessedSize2(UInt32 streamIndex, UInt64 *value)
{
  const size_t rem = (size_t)(dec.lims[streamIndex] - dec.bufs[streamIndex]) + _extraReadSizes[streamIndex];
  *value = _inStreamsProcessed[streamIndex] - rem;
  return S_OK;
}

}}

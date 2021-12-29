// HandlerCont.cpp

#include "StdAfx.h"

#include "../../Common/ComTry.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"

#include "HandlerCont.h"

namespace NArchive {

STDMETHODIMP CHandlerCont::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback)
{
  COM_TRY_BEGIN
  bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
  {
    RINOK(GetNumberOfItems(&numItems));
  }
  if (numItems == 0)
    return S_OK;
  UInt64 totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    UInt64 pos, size;
    GetItem_ExtractInfo(allFilesMode ? i : indices[i], pos, size);
    totalSize += size;
  }
  extractCallback->SetTotal(totalSize);

  totalSize = 0;
  
  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder();
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  CLimitedSequentialInStream *streamSpec = new CLimitedSequentialInStream;
  CMyComPtr<ISequentialInStream> inStream(streamSpec);
  streamSpec->SetStream(_stream);

  for (i = 0; i < numItems; i++)
  {
    lps->InSize = totalSize;
    lps->OutSize = totalSize;
    RINOK(lps->SetCur());
    CMyComPtr<ISequentialOutStream> outStream;
    Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    Int32 index = allFilesMode ? i : indices[i];
    
    RINOK(extractCallback->GetStream(index, &outStream, askMode));

    UInt64 pos, size;
    int opRes = GetItem_ExtractInfo(index, pos, size);
    totalSize += size;
    if (!testMode && !outStream)
      continue;
    
    RINOK(extractCallback->PrepareOperation(askMode));

    if (opRes == NExtract::NOperationResult::kOK)
    {
      RINOK(_stream->Seek(pos, STREAM_SEEK_SET, NULL));
      streamSpec->Init(size);
    
      RINOK(copyCoder->Code(inStream, outStream, NULL, NULL, progress));
      
      opRes = NExtract::NOperationResult::kDataError;
      
      if (copyCoderSpec->TotalSize == size)
        opRes = NExtract::NOperationResult::kOK;
      else if (copyCoderSpec->TotalSize < size)
        opRes = NExtract::NOperationResult::kUnexpectedEnd;
    }
    
    outStream.Release();
    RINOK(extractCallback->SetOperationResult(opRes));
  }
  
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandlerCont::GetStream(UInt32 index, ISequentialInStream **stream)
{
  COM_TRY_BEGIN
  *stream = NULL;
  UInt64 pos, size;
  if (GetItem_ExtractInfo(index, pos, size) != NExtract::NOperationResult::kOK)
    return S_FALSE;
  return CreateLimitedInStream(_stream, pos, size, stream);
  COM_TRY_END
}



CHandlerImg::CHandlerImg()
{
  Clear_HandlerImg_Vars();
}

STDMETHODIMP CHandlerImg::Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition)
{
  switch (seekOrigin)
  {
    case STREAM_SEEK_SET: break;
    case STREAM_SEEK_CUR: offset += _virtPos; break;
    case STREAM_SEEK_END: offset += _size; break;
    default: return STG_E_INVALIDFUNCTION;
  }
  if (offset < 0)
  {
    if (newPosition)
      *newPosition = _virtPos;
    return HRESULT_WIN32_ERROR_NEGATIVE_SEEK;
  }
  _virtPos = offset;
  if (newPosition)
    *newPosition = offset;
  return S_OK;
}

static const Byte k_GDP_Signature[] = { 'E', 'F', 'I', ' ', 'P', 'A', 'R', 'T' };


static const char *GetImgExt(ISequentialInStream *stream)
{
  const size_t kHeaderSize = 1 << 10;
  Byte buf[kHeaderSize];
  if (ReadStream_FAIL(stream, buf, kHeaderSize) == S_OK)
  {
    if (buf[0x1FE] == 0x55 && buf[0x1FF] == 0xAA)
    {
      if (memcmp(buf + 512, k_GDP_Signature, sizeof(k_GDP_Signature)) == 0)
        return "gpt";
      return "mbr";
    }
  }
  return NULL;
}

void CHandlerImg::CloseAtError()
{
  Stream.Release();
}

void CHandlerImg::Clear_HandlerImg_Vars()
{
  _imgExt = NULL;
  _size = 0;
  ClearStreamVars();
  Reset_VirtPos();
  Reset_PosInArc();
}

STDMETHODIMP CHandlerImg::Open(IInStream *stream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback * openCallback)
{
  COM_TRY_BEGIN
  {
    Close();
    HRESULT res;
    try
    {
      res = Open2(stream, openCallback);
      if (res == S_OK)
      {
        CMyComPtr<ISequentialInStream> inStream;
        const HRESULT res2 = GetStream(0, &inStream);
        if (res2 == S_OK && inStream)
          _imgExt = GetImgExt(inStream);
        // _imgExt = GetImgExt(this); // for debug
        /*  we reset (_virtPos) to support cases, if some code will
            call Read() from Handler object instead of GetStream() object. */
        Reset_VirtPos();
        // optional: we reset (_posInArc). if real seek position of stream will be changed in external code
        Reset_PosInArc();
        // optional: here we could also reset seek positions in parent streams..
        return S_OK;
      }
    }
    catch(...)
    {
      CloseAtError();
      throw;
    }
    CloseAtError();
    return res;
  }
  COM_TRY_END
}

STDMETHODIMP CHandlerImg::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = 1;
  return S_OK;
}

STDMETHODIMP CHandlerImg::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback)
{
  COM_TRY_BEGIN
  if (numItems == 0)
    return S_OK;
  if (numItems != (UInt32)(Int32)-1 && (numItems != 1 || indices[0] != 0))
    return E_INVALIDARG;

  RINOK(extractCallback->SetTotal(_size));
  CMyComPtr<ISequentialOutStream> outStream;
  Int32 askMode = testMode ?
      NExtract::NAskMode::kTest :
      NExtract::NAskMode::kExtract;
  RINOK(extractCallback->GetStream(0, &outStream, askMode));
  if (!testMode && !outStream)
    return S_OK;
  RINOK(extractCallback->PrepareOperation(askMode));

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  int opRes = NExtract::NOperationResult::kDataError;
  
  ClearStreamVars();
  
  CMyComPtr<ISequentialInStream> inStream;
  HRESULT hres = GetStream(0, &inStream);
  if (hres == S_FALSE)
    hres = E_NOTIMPL;

  if (hres == S_OK && inStream)
  {
    NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder();
    CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

    hres = copyCoder->Code(inStream, outStream, NULL, &_size, progress);
    if (hres == S_OK)
    {
      if (copyCoderSpec->TotalSize == _size)
        opRes = NExtract::NOperationResult::kOK;
      
      if (_stream_unavailData)
        opRes = NExtract::NOperationResult::kUnavailable;
      else if (_stream_unsupportedMethod)
        opRes = NExtract::NOperationResult::kUnsupportedMethod;
      else if (_stream_dataError)
        opRes = NExtract::NOperationResult::kDataError;
      else if (copyCoderSpec->TotalSize < _size)
        opRes = NExtract::NOperationResult::kUnexpectedEnd;
    }
  }

  inStream.Release();
  outStream.Release();
  
  if (hres != S_OK)
  {
    if (hres == S_FALSE)
      opRes = NExtract::NOperationResult::kDataError;
    else if (hres == E_NOTIMPL)
      opRes = NExtract::NOperationResult::kUnsupportedMethod;
    else
      return hres;
  }
 
  return extractCallback->SetOperationResult(opRes);
  COM_TRY_END
}


HRESULT ReadZeroTail(ISequentialInStream *stream, bool &areThereNonZeros, UInt64 &numZeros, UInt64 maxSize)
{
  areThereNonZeros = false;
  numZeros = 0;
  const size_t kBufSize = 1 << 11;
  Byte buf[kBufSize];
  for (;;)
  {
    UInt32 size = 0;
    RINOK(stream->Read(buf, kBufSize, &size));
    if (size == 0)
      return S_OK;
    for (UInt32 i = 0; i < size; i++)
      if (buf[i] != 0)
      {
        areThereNonZeros = true;
        numZeros += i;
        return S_OK;
      }
    numZeros += size;
    if (numZeros > maxSize)
      return S_OK;
  }
}

}

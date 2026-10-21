// ZHandler.cpp

#include "StdAfx.h"

#include "../../Common/ComTry.h"
// #include "../../Common/MyBuffer.h"

#include "../../Windows/PropVariant.h"

#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "../Compress/ZDecoder.h"

#include "Common/DummyOutStream.h"

namespace NArchive {
namespace NZ {

Z7_CLASS_IMP_CHandler_IInArchive_0

  CMyComPtr<IInStream> _stream;
  UInt64 _packSize;
  // UInt64 _unpackSize;
  // bool _unpackSize_Defined;
};

static const Byte kProps[] =
{
  kpidPackSize
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps_NO_Table

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = 1;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPhySizeCantBeDetected: prop = true; break;
  }
  prop.Detach(value);
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 /* index */, PROPID propID, PROPVARIANT *value))
{
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    // case kpidSize: if (_unpackSize_Defined) prop = _unpackSize; break;
    case kpidPackSize: prop = _packSize; break;
  }
  prop.Detach(value);
  return S_OK;
}

/*
Z7_CLASS_IMP_COM_1(
  CCompressProgressInfoImp
  , ICompressProgressInfo
)
public:
  CMyComPtr<IArchiveOpenCallback> Callback;
  void Init(IArchiveOpenCallback *callback) { Callback = callback; }
};

Z7_COM7F_IMF(CCompressProgressInfoImp::SetRatioInfo(const UInt64 *inSize, const UInt64 *))
{
  if (Callback)
    return Callback->SetCompleted(NULL, inSize);
  return S_OK;
}
*/

API_FUNC_static_IsArc IsArc_Z(const Byte *p, size_t size)
{
  if (size < 3)
    return k_IsArc_Res_NEED_MORE;
  if (size > NCompress::NZ::kRecommendedCheckSize)
      size = NCompress::NZ::kRecommendedCheckSize;
  if (!NCompress::NZ::CheckStream(p, size))
    return k_IsArc_Res_NO;
  return k_IsArc_Res_YES;
}
}

Z7_COM7F_IMF(CHandler::Open(IInStream *stream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback * /* openCallback */))
{
  COM_TRY_BEGIN
  {
#if 1 // 0 for debug
    Byte buffer[NCompress::NZ::kRecommendedCheckSize];
    size_t size = NCompress::NZ::kRecommendedCheckSize;
#else
    size_t size = 1 << 26; // for debug
    CByteBuffer buffer;
    buffer.Alloc(size);
#endif
    RINOK(ReadStream(stream, buffer, &size))
    // for (int y = 0; y < 10; y++)
    if (!NCompress::NZ::CheckStream(buffer, size))
      return S_FALSE;

    UInt64 endPos;
    RINOK(InStream_GetSize_SeekToEnd(stream, endPos))
    _packSize = endPos;
/*
    // bool fullCheck = false;
    // if (fullCheck)
    {
      CMyComPtr2_Create<ICompressProgressInfo, CCompressProgressInfoImp> compressProgress;
      compressProgress->Callback = openCallback;
      CMyComPtr2_Create<ISequentialOutStream, CDummyOutStream> outStream;
      outStream->SetStream(NULL);
      outStream->Init();
      // decoderSpec->SetProp(_prop);
      if (openCallback)
        RINOK(openCallback->SetTotal(NULL, &endPos));
      RINOK(InStream_SeekToBegin(stream))
      HRESULT res = NCompress::NZ::Decode(stream, outStream, openCallback ? compressProgress.Interface() : NULL);
      if (res != S_OK)
        return S_FALSE;
      // _packSize = decoderSpec->PackSize;
      _unpackSize = outStream->GetSize();
      _unpackSize_Defined = true;
    }
*/
    _stream = stream;
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _packSize = 0;
  // _unpackSize_Defined = false;
  _stream.Release();
  return S_OK;
}


Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  if (numItems == 0)
    return S_OK;
  if (numItems != (UInt32)(Int32)-1 && (numItems != 1 || indices[0] != 0))
    return E_INVALIDARG;
  RINOK(extractCallback->SetTotal(_packSize))
  
  int opRes;
  {
    CMyComPtr<ISequentialOutStream> realOutStream;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    RINOK(extractCallback->GetStream(0, &realOutStream, askMode))
    if (!testMode && !realOutStream)
      return S_OK;
    RINOK(extractCallback->PrepareOperation(askMode))
    /*
    // NZ::Decode() supports (outStream == NULL)
    CMyComPtr2_Create<ISequentialOutStream, CDummyOutStream> outStream;
    outStream->SetStream(realOutStream);
    outStream->Init();
    // realOutStream.Release();
    */
    CMyComPtr2_Create<ICompressProgressInfo, CLocalProgress> lps;
    lps->Init(extractCallback, true);
    RINOK(InStream_SeekToBegin(_stream))
    const HRESULT hres = NCompress::NZ::Decode(_stream, realOutStream, lps);
    if (hres == S_FALSE)
      opRes = NExtract::NOperationResult::kDataError;
    else
    {
      RINOK(hres)
      opRes = NExtract::NOperationResult::kOK;
    }
    // _unpackSize = outStream->GetSize();
    // _unpackSize_Defined = true;
  }
  return extractCallback->SetOperationResult(opRes);
  COM_TRY_END
}

static const Byte k_Signature[] = { 0x1F, 0x9D };

REGISTER_ARC_I(
  "Z", "z taz", "* .tar", 5,
  k_Signature,
  0,
  0,
  IsArc_Z)

}}

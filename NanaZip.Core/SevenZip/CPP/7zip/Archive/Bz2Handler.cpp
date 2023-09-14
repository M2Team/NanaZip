// Bz2Handler.cpp

#include "StdAfx.h"

#include "../../Common/ComTry.h"

#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "../Compress/BZip2Decoder.h"
#include "../Compress/BZip2Encoder.h"
#include "../Compress/CopyCoder.h"

#include "Common/DummyOutStream.h"
#include "Common/HandlerOut.h"

using namespace NWindows;

namespace NArchive {
namespace NBz2 {

Z7_CLASS_IMP_CHandler_IInArchive_3(
  IArchiveOpenSeq,
  IOutArchive,
  ISetProperties
)
  CMyComPtr<IInStream> _stream;
  CMyComPtr<ISequentialInStream> _seqStream;
  
  bool _isArc;
  bool _needSeekToStart;
  bool _dataAfterEnd;
  bool _needMoreInput;

  bool _packSize_Defined;
  bool _unpackSize_Defined;
  bool _numStreams_Defined;
  bool _numBlocks_Defined;

  UInt64 _packSize;
  UInt64 _unpackSize;
  UInt64 _numStreams;
  UInt64 _numBlocks;

  CSingleMethodProps _props;
};

static const Byte kProps[] =
{
  kpidSize,
  kpidPackSize
};

static const Byte kArcProps[] =
{
  kpidNumStreams,
  kpidNumBlocks
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPhySize: if (_packSize_Defined) prop = _packSize; break;
    case kpidUnpackSize: if (_unpackSize_Defined) prop = _unpackSize; break;
    case kpidNumStreams: if (_numStreams_Defined) prop = _numStreams; break;
    case kpidNumBlocks: if (_numBlocks_Defined) prop = _numBlocks; break;
    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc) v |= kpv_ErrorFlags_IsNotArc;
      if (_needMoreInput) v |= kpv_ErrorFlags_UnexpectedEnd;
      if (_dataAfterEnd) v |= kpv_ErrorFlags_DataAfterEnd;
      prop = v;
    }
  }
  prop.Detach(value);
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = 1;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 /* index */, PROPID propID, PROPVARIANT *value))
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPackSize: if (_packSize_Defined) prop = _packSize; break;
    case kpidSize: if (_unpackSize_Defined) prop = _unpackSize; break;
  }
  prop.Detach(value);
  return S_OK;
}

static const unsigned kSignatureCheckSize = 10;

API_FUNC_static_IsArc IsArc_BZip2(const Byte *p, size_t size)
{
  if (size < kSignatureCheckSize)
    return k_IsArc_Res_NEED_MORE;
  if (p[0] != 'B' || p[1] != 'Z' || p[2] != 'h' || p[3] < '1' || p[3] > '9')
    return k_IsArc_Res_NO;
  p += 4;
  if (NCompress::NBZip2::IsBlockSig(p))
    return k_IsArc_Res_YES;
  if (NCompress::NBZip2::IsEndSig(p))
    return k_IsArc_Res_YES;
  return k_IsArc_Res_NO;
}
}

Z7_COM7F_IMF(CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *))
{
  COM_TRY_BEGIN
  Close();
  {
    Byte buf[kSignatureCheckSize];
    RINOK(ReadStream_FALSE(stream, buf, kSignatureCheckSize))
    if (IsArc_BZip2(buf, kSignatureCheckSize) == k_IsArc_Res_NO)
      return S_FALSE;
    _isArc = true;
    _stream = stream;
    _seqStream = stream;
    _needSeekToStart = true;
  }
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::OpenSeq(ISequentialInStream *stream))
{
  Close();
  _isArc = true;
  _seqStream = stream;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Close())
{
  _isArc = false;
  _needSeekToStart = false;
  _dataAfterEnd = false;
  _needMoreInput = false;

  _packSize_Defined = false;
  _unpackSize_Defined = false;
  _numStreams_Defined = false;
  _numBlocks_Defined = false;

  _packSize = 0;

  _seqStream.Release();
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

  if (_packSize_Defined)
    extractCallback->SetTotal(_packSize);

  CMyComPtr<ISequentialOutStream> realOutStream;
  const Int32 askMode = testMode ?
      NExtract::NAskMode::kTest :
      NExtract::NAskMode::kExtract;
  RINOK(extractCallback->GetStream(0, &realOutStream, askMode))
  if (!testMode && !realOutStream)
    return S_OK;

  extractCallback->PrepareOperation(askMode);

  if (_needSeekToStart)
  {
    if (!_stream)
      return E_FAIL;
    RINOK(InStream_SeekToBegin(_stream))
  }
  else
    _needSeekToStart = true;

  // try {

  NCompress::NBZip2::CDecoder *decoderSpec = new NCompress::NBZip2::CDecoder;
  CMyComPtr<ICompressCoder> decoder = decoderSpec;

  #ifndef Z7_ST
  RINOK(decoderSpec->SetNumberOfThreads(_props._numThreads))
  #endif

  CDummyOutStream *outStreamSpec = new CDummyOutStream;
  CMyComPtr<ISequentialOutStream> outStream(outStreamSpec);
  outStreamSpec->SetStream(realOutStream);
  outStreamSpec->Init();
  
  realOutStream.Release();

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, true);

  decoderSpec->FinishMode = true;
  decoderSpec->Base.DecodeAllStreams = true;
  
  _dataAfterEnd = false;
  _needMoreInput = false;

  lps->InSize = 0;
  lps->OutSize = 0;
  
  HRESULT result = decoder->Code(_seqStream, outStream, NULL, NULL, progress);
  
  if (result != S_FALSE && result != S_OK)
    return result;
  
  if (decoderSpec->Base.NumStreams == 0)
  {
    _isArc = false;
    result = S_FALSE;
  }
  else
  {
    const UInt64 inProcessedSize = decoderSpec->GetInputProcessedSize();
    UInt64 packSize = inProcessedSize;

    if (decoderSpec->Base.NeedMoreInput)
      _needMoreInput = true;
    
    if (!decoderSpec->Base.IsBz)
    {
      packSize = decoderSpec->Base.FinishedPackSize;
      if (packSize != inProcessedSize)
        _dataAfterEnd = true;
    }

    _packSize = packSize;
    _unpackSize = decoderSpec->GetOutProcessedSize();
    _numStreams = decoderSpec->Base.NumStreams;
    _numBlocks = decoderSpec->GetNumBlocks();

    _packSize_Defined = true;
    _unpackSize_Defined = true;
    _numStreams_Defined = true;
    _numBlocks_Defined = true;
  }
  
  outStream.Release();

  Int32 opRes;

  if (!_isArc)
    opRes = NExtract::NOperationResult::kIsNotArc;
  else if (_needMoreInput)
    opRes = NExtract::NOperationResult::kUnexpectedEnd;
  else if (decoderSpec->GetCrcError())
    opRes = NExtract::NOperationResult::kCRCError;
  else if (_dataAfterEnd)
    opRes = NExtract::NOperationResult::kDataAfterEnd;
  else if (result == S_FALSE)
    opRes = NExtract::NOperationResult::kDataError;
  else if (decoderSpec->Base.MinorError)
    opRes = NExtract::NOperationResult::kDataError;
  else if (result == S_OK)
    opRes = NExtract::NOperationResult::kOK;
  else
    return result;

  return extractCallback->SetOperationResult(opRes);

  // } catch(...)  { return E_FAIL; }

  COM_TRY_END
}


/*
static HRESULT ReportItemProp(IArchiveUpdateCallbackArcProp *reportArcProp, PROPID propID, const PROPVARIANT *value)
{
  return reportArcProp->ReportProp(NEventIndexType::kOutArcIndex, 0, propID, value);
}

static HRESULT ReportArcProp(IArchiveUpdateCallbackArcProp *reportArcProp, PROPID propID, const PROPVARIANT *value)
{
  return reportArcProp->ReportProp(NEventIndexType::kArcProp, 0, propID, value);
}

static HRESULT ReportArcProps(IArchiveUpdateCallbackArcProp *reportArcProp,
    const UInt64 *unpackSize,
    const UInt64 *numBlocks)
{
  NCOM::CPropVariant sizeProp;
  if (unpackSize)
  {
    sizeProp = *unpackSize;
    RINOK(ReportItemProp(reportArcProp, kpidSize, &sizeProp));
    RINOK(reportArcProp->ReportFinished(NEventIndexType::kOutArcIndex, 0, NArchive::NUpdate::NOperationResult::kOK));
  }
 
  if (unpackSize)
  {
    RINOK(ReportArcProp(reportArcProp, kpidSize, &sizeProp));
  }
  if (numBlocks)
  {
    NCOM::CPropVariant prop;
    prop = *numBlocks;
    RINOK(ReportArcProp(reportArcProp, kpidNumBlocks, &prop));
  }
  return S_OK;
}
*/

static HRESULT UpdateArchive(
    UInt64 unpackSize,
    ISequentialOutStream *outStream,
    const CProps &props,
    IArchiveUpdateCallback *updateCallback
    // , ArchiveUpdateCallbackArcProp *reportArcProp
    )
{
  {
    CMyComPtr<ISequentialInStream> fileInStream;
    RINOK(updateCallback->GetStream(0, &fileInStream))
    if (!fileInStream)
      return S_FALSE;
    {
      Z7_DECL_CMyComPtr_QI_FROM(
          IStreamGetSize,
          streamGetSize, fileInStream)
      if (streamGetSize)
      {
        UInt64 size;
        if (streamGetSize->GetSize(&size) == S_OK)
          unpackSize = size;
      }
    }
    RINOK(updateCallback->SetTotal(unpackSize))
    CLocalProgress *localProgressSpec = new CLocalProgress;
    CMyComPtr<ICompressProgressInfo> localProgress = localProgressSpec;
    localProgressSpec->Init(updateCallback, true);
    {
      NCompress::NBZip2::CEncoder *encoderSpec = new NCompress::NBZip2::CEncoder;
      CMyComPtr<ICompressCoder> encoder = encoderSpec;
      RINOK(props.SetCoderProps(encoderSpec, NULL))
      RINOK(encoder->Code(fileInStream, outStream, NULL, NULL, localProgress))
      /*
      if (reportArcProp)
      {
        unpackSize = encoderSpec->GetInProcessedSize();
        RINOK(ReportArcProps(reportArcProp, &unpackSize, &encoderSpec->NumBlocks));
      }
      */
    }
  }
  return updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK);
}

Z7_COM7F_IMF(CHandler::GetFileTimeType(UInt32 *timeType))
{
  *timeType = GET_FileTimeType_NotDefined_for_GetFileTimeType;
  // *timeType = NFileTimeType::kUnix;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::UpdateItems(ISequentialOutStream *outStream, UInt32 numItems,
    IArchiveUpdateCallback *updateCallback))
{
  COM_TRY_BEGIN

  if (numItems != 1)
    return E_INVALIDARG;

  {
    Z7_DECL_CMyComPtr_QI_FROM(
        IStreamSetRestriction,
        setRestriction, outStream)
    if (setRestriction)
      RINOK(setRestriction->SetRestriction(0, 0))
  }

  Int32 newData, newProps;
  UInt32 indexInArchive;
  if (!updateCallback)
    return E_FAIL;
  RINOK(updateCallback->GetUpdateItemInfo(0, &newData, &newProps, &indexInArchive))
 
  // Z7_DECL_CMyComPtr_QI_FROM(IArchiveUpdateCallbackArcProp, reportArcProp, updateCallback)

  if (IntToBool(newProps))
  {
    {
      NCOM::CPropVariant prop;
      RINOK(updateCallback->GetProperty(0, kpidIsDir, &prop))
      if (prop.vt != VT_EMPTY)
        if (prop.vt != VT_BOOL || prop.boolVal != VARIANT_FALSE)
          return E_INVALIDARG;
    }
  }
  
  if (IntToBool(newData))
  {
    UInt64 size;
    {
      NCOM::CPropVariant prop;
      RINOK(updateCallback->GetProperty(0, kpidSize, &prop))
      if (prop.vt != VT_UI8)
        return E_INVALIDARG;
      size = prop.uhVal.QuadPart;
    }

    CMethodProps props2 = _props;
    #ifndef Z7_ST
    props2.AddProp_NumThreads(_props._numThreads);
    #endif

    return UpdateArchive(size, outStream, props2, updateCallback);
  }

  if (indexInArchive != 0)
    return E_INVALIDARG;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(updateCallback, true);

  Z7_DECL_CMyComPtr_QI_FROM(
      IArchiveUpdateCallbackFile,
      opCallback, updateCallback)
  if (opCallback)
  {
    RINOK(opCallback->ReportOperation(
        NEventIndexType::kInArcIndex, 0,
        NUpdateNotifyOp::kReplicate))
  }

  if (_stream)
    RINOK(InStream_SeekToBegin(_stream))

  return NCompress::CopyStream(_stream, outStream, progress);

  // return ReportArcProps(reportArcProp, NULL, NULL);

  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps))
{
  return _props.SetProperties(names, values, numProps);
}

static const Byte k_Signature[] = { 'B', 'Z', 'h' };

REGISTER_ARC_IO(
  "bzip2", "bz2 bzip2 tbz2 tbz", "* * .tar .tar", 2,
  k_Signature,
  0,
  NArcInfoFlags::kKeepName
  , 0
  , IsArc_BZip2)

}}

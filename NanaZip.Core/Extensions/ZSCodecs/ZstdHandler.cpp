// ZstdHandler.cpp

#include "../../SevenZip/CPP/7zip/Archive/StdAfx.h"

#include "../../SevenZip/C/CpuArch.h"
#include "../../SevenZip/CPP/Common/ComTry.h"
#include "../../SevenZip/CPP/Common/Defs.h"

#include "../../SevenZip/CPP/7zip/Common/ProgressUtils.h"
#include "../../SevenZip/CPP/7zip/Common/RegisterArc.h"
#include "../../SevenZip/CPP/7zip/Common/StreamUtils.h"

#include "../../SevenZip/CPP/7zip/Compress/ZstdDecoder.h"
#include "ZstdEncoder.h"
#include "../../SevenZip/CPP/7zip/Compress/CopyCoder.h"

#include "../../SevenZip/CPP/7zip/Archive/Common/DummyOutStream.h"
#include "../../SevenZip/CPP/7zip/Archive/Common/HandlerOut.h"

using namespace NWindows;

namespace NArchive {
namespace NZSTD {

class CHandler:
  public IInArchive,
  public IArchiveOpenSeq,
  public IOutArchive,
  public ISetProperties,
  public CMyUnknownImp
{
  CMyComPtr<IInStream> _stream;
  CMyComPtr<ISequentialInStream> _seqStream;

  bool _isArc;
  bool _dataAfterEnd;
  bool _needMoreInput;

  bool _packSize_Defined;
  bool _unpackSize_Defined;

  UInt64 _packSize;
  UInt64 _unpackSize;
  UInt64 _numStreams;
  UInt64 _numBlocks;

  CSingleMethodProps _props;

public:
  Z7_IFACES_IMP_UNK_4(
      IInArchive,
      IArchiveOpenSeq,
      IOutArchive,
      ISetProperties)
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

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID /*propID*/, PROPVARIANT * /*value*/))
{
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

static const unsigned kSignatureCheckSize = 4;

API_FUNC_static_IsArc IsArc_zstd(const Byte *p, size_t size)
{
  if (size < 4)
    return k_IsArc_Res_NEED_MORE;

  UInt32 magic = GetUi32(p);

  // skippable frames
  if (magic >= 0x184D2A50 && magic <= 0x184D2A5F) {
    if (size < 16)
      return k_IsArc_Res_NEED_MORE;
    magic = GetUi32(p+12);
  }

  /* only version 1.x */
  if (magic == 0xFD2FB528)
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
    RINOK(ReadStream_FALSE(stream, buf, kSignatureCheckSize));
    if (IsArc_zstd(buf, kSignatureCheckSize) == k_IsArc_Res_NO)
      return S_FALSE;
    _isArc = true;
    _stream = stream;
    _seqStream = stream;
    RINOK(_stream->Seek(0, STREAM_SEEK_SET, NULL));
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
  _dataAfterEnd = false;
  _needMoreInput = false;

  _packSize_Defined = false;
  _unpackSize_Defined = false;

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
  Int32 askMode = testMode ?
      NExtract::NAskMode::kTest :
      NExtract::NAskMode::kExtract;
  RINOK(extractCallback->GetStream(0, &realOutStream, askMode));
  if (!testMode && !realOutStream)
    return S_OK;

  extractCallback->PrepareOperation(askMode);

  Int32 opRes;

  CMyComPtr2_Create<ICompressProgressInfo, CLocalProgress> lps;
  lps->Init(extractCallback, true);

  CMyComPtr2_Create<ICompressCoder, NCompress::NZstd::CDecoder> decoder;

  CMyComPtr2_Create<ISequentialOutStream, CDummyOutStream> outStreamSpec;
  outStreamSpec->SetStream(realOutStream);
  outStreamSpec->Init();

  decoder->FinishMode = true;

  const HRESULT hres = decoder.Interface()->Code(
      _seqStream,
      outStreamSpec,
      nullptr,
      nullptr,
      lps);

  const UInt64 outSize = outStreamSpec->GetSize();

  opRes = NExtract::NOperationResult::kDataError;

  if (hres == E_OUTOFMEMORY)
  {
      return hres;
  }
  else if (hres == S_OK || hres == S_FALSE)
  {
      const UInt64 inSize = decoder->_inProcessed;

      _unpackSize_Defined = true;
      _unpackSize = outSize;

      // RINOK(
      lps.Interface()->SetRatioInfo(&inSize, &outSize);

      if (decoder->ResInfo.decode_SRes == SZ_ERROR_CRC)
      {
          opRes = NExtract::NOperationResult::kCRCError;
      }
      else if (decoder->ResInfo.decode_SRes == SZ_ERROR_NO_ARCHIVE)
      {
          _isArc = false;
          opRes = NExtract::NOperationResult::kIsNotArc;
      }
      else if (decoder->ResInfo.decode_SRes == SZ_ERROR_INPUT_EOF)
          opRes = NExtract::NOperationResult::kUnexpectedEnd;
      else
      {
          if (hres == S_OK && decoder->ResInfo.decode_SRes == SZ_OK)
              opRes = NExtract::NOperationResult::kOK;
          if (decoder->ResInfo.extraSize)
          {
              opRes = NExtract::NOperationResult::kDataAfterEnd;
          }
      }
  }
  else if (hres == E_NOTIMPL)
  {
      opRes = NExtract::NOperationResult::kUnsupportedMethod;
  }
  else
      return hres;

  return extractCallback->SetOperationResult(opRes);

  COM_TRY_END
}

static HRESULT UpdateArchive(
    UInt64 unpackSize,
    ISequentialOutStream *outStream,
    const CProps &props,
    IArchiveUpdateCallback *updateCallback)
{
  RINOK(updateCallback->SetTotal(unpackSize));
  CMyComPtr<ISequentialInStream> fileInStream;
  RINOK(updateCallback->GetStream(0, &fileInStream));
  CLocalProgress *localProgressSpec = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> localProgress = localProgressSpec;
  localProgressSpec->Init(updateCallback, true);
  NCompress::NZSTD::CEncoder *encoderSpec = new NCompress::NZSTD::CEncoder;
  // by zstd archive type store dictID and checksum (similar to zstd client)
  encoderSpec->dictIDFlag = 1;
  encoderSpec->checksumFlag = 1;
  encoderSpec->unpackSize = unpackSize;
  CMyComPtr<ICompressCoder> encoder = encoderSpec;
  RINOK(props.SetCoderProps(encoderSpec, NULL));
  RINOK(encoder->Code(fileInStream, outStream, NULL, NULL, localProgress));
  return updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK);
}

Z7_COM7F_IMF(CHandler::GetFileTimeType(UInt32 *type))
{
  *type = NFileTimeType::kUnix;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::UpdateItems(ISequentialOutStream *outStream, UInt32 numItems,
    IArchiveUpdateCallback *updateCallback))
{
  COM_TRY_BEGIN

  if (numItems != 1)
    return E_INVALIDARG;

  Int32 newData, newProps;
  UInt32 indexInArchive;
  if (!updateCallback)
    return E_FAIL;
  RINOK(updateCallback->GetUpdateItemInfo(0, &newData, &newProps, &indexInArchive));

  if ((newProps))
  {
    {
      NCOM::CPropVariant prop;
      RINOK(updateCallback->GetProperty(0, kpidIsDir, &prop));
      if (prop.vt != VT_EMPTY)
        if (prop.vt != VT_BOOL || prop.boolVal != VARIANT_FALSE)
          return E_INVALIDARG;
    }
  }

  if ((newData))
  {
    UInt64 size;
    {
      NCOM::CPropVariant prop;
      RINOK(updateCallback->GetProperty(0, kpidSize, &prop));
      if (prop.vt != VT_UI8)
        return E_INVALIDARG;
      size = prop.uhVal.QuadPart;
    }
    return UpdateArchive(size, outStream, _props, updateCallback);
  }

  if (indexInArchive != 0)
    return E_INVALIDARG;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(updateCallback, true);

  CMyComPtr<IArchiveUpdateCallbackFile> opCallback;
  updateCallback->QueryInterface(IID_IArchiveUpdateCallbackFile, (void **)&opCallback);
  if (opCallback)
  {
    RINOK(opCallback->ReportOperation(
        NEventIndexType::kInArcIndex, 0,
        NUpdateNotifyOp::kReplicate))
  }

  if (_stream)
    RINOK(_stream->Seek(0, STREAM_SEEK_SET, NULL));

  return NCompress::CopyStream(_stream, outStream, progress);

  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps))
{
  return _props.SetProperties(names, values, numProps);
}

static const unsigned kSignatureSize = 4;
static const Byte k_Signature[kSignatureSize] = { 0x28, 0xb5, 0x2f, 0xfd };

REGISTER_ARC_IO(
  "zstd", "zst zstd tzst tzstd", "* * .tar .tar", 0x0e,
  k_Signature,
  0,
  NArcInfoFlags::kKeepName,
  0,
  IsArc_zstd)

}}

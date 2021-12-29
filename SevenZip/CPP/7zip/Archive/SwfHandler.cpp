// SwfHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/MyBuffer.h"
#include "../../Common/MyString.h"

#include "../../Windows/PropVariant.h"
#include "../../Windows/PropVariantUtils.h"

#include "../Common/InBuffer.h"
#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"
#include "../Compress/LzmaDecoder.h"
#include "../Compress/ZlibDecoder.h"

#include "Common/DummyOutStream.h"

// #define SWF_UPDATE

#ifdef SWF_UPDATE

#include "../Compress/LzmaEncoder.h"
#include "../Compress/ZlibEncoder.h"

#include "Common/HandlerOut.h"
 
#endif

using namespace NWindows;

namespace NArchive {

static const UInt32 kFileSizeMax = (UInt32)1 << 29;

namespace NSwfc {

static const unsigned kHeaderBaseSize = 8;
static const unsigned kHeaderLzmaSize = 17;

static const Byte SWF_UNCOMPRESSED = 'F';
static const Byte SWF_COMPRESSED_ZLIB = 'C';
static const Byte SWF_COMPRESSED_LZMA = 'Z';

static const Byte SWF_MIN_COMPRESSED_ZLIB_VER = 6;
static const Byte SWF_MIN_COMPRESSED_LZMA_VER = 13;

static const Byte kVerLim = 64;

API_FUNC_static_IsArc IsArc_Swf(const Byte *p, size_t size)
{
  if (size < kHeaderBaseSize)
    return k_IsArc_Res_NEED_MORE;
  if (p[0] != SWF_UNCOMPRESSED ||
      p[1] != 'W' ||
      p[2] != 'S' ||
      p[3] >= kVerLim)
    return k_IsArc_Res_NO;
  UInt32 uncompressedSize = GetUi32(p + 4);
  if (uncompressedSize > kFileSizeMax)
    return k_IsArc_Res_NO;
  return k_IsArc_Res_YES;
}
}

API_FUNC_static_IsArc IsArc_Swfc(const Byte *p, size_t size)
{
  if (size < kHeaderBaseSize + 2 + 1) // 2 + 1 (for zlib check)
    return k_IsArc_Res_NEED_MORE;
  if ((p[0] != SWF_COMPRESSED_ZLIB &&
      p[0] != SWF_COMPRESSED_LZMA) ||
      p[1] != 'W' ||
      p[2] != 'S' ||
      p[3] >= kVerLim)
    return k_IsArc_Res_NO;
  UInt32 uncompressedSize = GetUi32(p + 4);
  if (uncompressedSize > kFileSizeMax)
    return k_IsArc_Res_NO;

  if (p[0] == SWF_COMPRESSED_ZLIB)
  {
    if (!NCompress::NZlib::IsZlib_3bytes(p + 8))
      return k_IsArc_Res_NO;
  }
  else
  {
    if (size < kHeaderLzmaSize + 2)
      return k_IsArc_Res_NEED_MORE;
    if (p[kHeaderLzmaSize] != 0 ||
        (p[kHeaderLzmaSize + 1] & 0x80) != 0)
      return k_IsArc_Res_NO;
    UInt32 lzmaPackSize = GetUi32(p + 8);
    UInt32 lzmaProp = p[12];
    UInt32 lzmaDicSize = GetUi32(p + 13);
    if (lzmaProp > 5 * 5 * 9 ||
        lzmaDicSize > ((UInt32)1 << 28) ||
        lzmaPackSize < 5 ||
        lzmaPackSize > ((UInt32)1 << 28))
      return k_IsArc_Res_NO;
  }

  return k_IsArc_Res_YES;
}
}

struct CItem
{
  Byte Buf[kHeaderLzmaSize];
  unsigned HeaderSize;

  UInt32 GetSize() const { return GetUi32(Buf + 4); }
  UInt32 GetLzmaPackSize() const { return GetUi32(Buf + 8); }
  UInt32 GetLzmaDicSize() const { return GetUi32(Buf + 13); }

  bool IsSwf() const { return (Buf[1] == 'W' && Buf[2] == 'S' && Buf[3] < kVerLim); }
  bool IsUncompressed() const { return Buf[0] == SWF_UNCOMPRESSED; }
  bool IsZlib() const { return Buf[0] == SWF_COMPRESSED_ZLIB; }
  bool IsLzma() const { return Buf[0] == SWF_COMPRESSED_LZMA; }

  void MakeUncompressed()
  {
    Buf[0] = SWF_UNCOMPRESSED;
    HeaderSize = kHeaderBaseSize;
  }
  void MakeZlib()
  {
    Buf[0] = SWF_COMPRESSED_ZLIB;
    if (Buf[3] < SWF_MIN_COMPRESSED_ZLIB_VER)
      Buf[3] = SWF_MIN_COMPRESSED_ZLIB_VER;
  }
  void MakeLzma(UInt32 packSize)
  {
    Buf[0] = SWF_COMPRESSED_LZMA;
    if (Buf[3] < SWF_MIN_COMPRESSED_LZMA_VER)
      Buf[3] = SWF_MIN_COMPRESSED_LZMA_VER;
    SetUi32(Buf + 8, packSize);
    HeaderSize = kHeaderLzmaSize;
  }

  HRESULT ReadHeader(ISequentialInStream *stream)
  {
    HeaderSize = kHeaderBaseSize;
    return ReadStream_FALSE(stream, Buf, kHeaderBaseSize);
  }
  HRESULT WriteHeader(ISequentialOutStream *stream)
  {
    return WriteStream(stream, Buf, HeaderSize);
  }
};

class CHandler:
  public IInArchive,
  public IArchiveOpenSeq,
 #ifdef SWF_UPDATE
  public IOutArchive,
  public ISetProperties,
 #endif
  public CMyUnknownImp
{
  CItem _item;
  UInt64 _packSize;
  bool _packSizeDefined;
  CMyComPtr<ISequentialInStream> _seqStream;
  CMyComPtr<IInStream> _stream;

 #ifdef SWF_UPDATE
  CSingleMethodProps _props;
  bool _lzmaMode;
  #endif

public:
 #ifdef SWF_UPDATE
  MY_UNKNOWN_IMP4(IInArchive, IArchiveOpenSeq, IOutArchive, ISetProperties)
  CHandler(): _lzmaMode(false) {}
  INTERFACE_IOutArchive(;)
  STDMETHOD(SetProperties)(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps);
 #else
  MY_UNKNOWN_IMP2(IInArchive, IArchiveOpenSeq)
 #endif
  INTERFACE_IInArchive(;)
  STDMETHOD(OpenSeq)(ISequentialInStream *stream);
};

static const Byte kProps[] =
{
  kpidSize,
  kpidPackSize,
  kpidMethod
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps_NO_Table

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPhySize: if (_packSizeDefined) prop = _item.HeaderSize + _packSize; break;
    case kpidIsNotArcType: prop = true; break;
  }
  prop.Detach(value);
  return S_OK;
}

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = 1;
  return S_OK;
}

static void DicSizeToString(char *s, UInt32 val)
{
  char c = 0;
  unsigned i;
  for (i = 0; i <= 31; i++)
    if (((UInt32)1 << i) == val)
    {
      val = i;
      break;
    }
  if (i == 32)
  {
    c = 'b';
    if      ((val & ((1 << 20) - 1)) == 0) { val >>= 20; c = 'm'; }
    else if ((val & ((1 << 10) - 1)) == 0) { val >>= 10; c = 'k'; }
  }
  ::ConvertUInt32ToString(val, s);
  unsigned pos = MyStringLen(s);
  s[pos++] = c;
  s[pos] = 0;
}

STDMETHODIMP CHandler::GetProperty(UInt32 /* index */, PROPID propID, PROPVARIANT *value)
{
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidSize: prop = (UInt64)_item.GetSize(); break;
    case kpidPackSize: if (_packSizeDefined) prop = _item.HeaderSize + _packSize; break;
    case kpidMethod:
    {
      char s[32];
      if (_item.IsZlib())
        MyStringCopy(s, "zlib");
      else
      {
        MyStringCopy(s, "LZMA:");
        DicSizeToString(s + 5, _item.GetLzmaDicSize());
      }
      prop = s;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
}

STDMETHODIMP CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *)
{
  RINOK(OpenSeq(stream));
  _stream = stream;
  return S_OK;
}

STDMETHODIMP CHandler::OpenSeq(ISequentialInStream *stream)
{
  Close();
  RINOK(_item.ReadHeader(stream));
  if (!_item.IsSwf())
    return S_FALSE;
  if (_item.IsLzma())
  {
    RINOK(ReadStream_FALSE(stream, _item.Buf + kHeaderBaseSize, kHeaderLzmaSize - kHeaderBaseSize));
    _item.HeaderSize = kHeaderLzmaSize;
    _packSize = _item.GetLzmaPackSize();
    _packSizeDefined = true;
  }
  else if (!_item.IsZlib())
    return S_FALSE;
  if (_item.GetSize() < _item.HeaderSize)
    return S_FALSE;
  _seqStream = stream;
  return S_OK;
}

STDMETHODIMP CHandler::Close()
{
  _packSize = 0;
  _packSizeDefined = false;
  _seqStream.Release();
  _stream.Release();
  return S_OK;
}

class CCompressProgressInfoImp:
  public ICompressProgressInfo,
  public CMyUnknownImp
{
  CMyComPtr<IArchiveOpenCallback> Callback;
public:
  UInt64 Offset;
  MY_UNKNOWN_IMP1(ICompressProgressInfo)
  STDMETHOD(SetRatioInfo)(const UInt64 *inSize, const UInt64 *outSize);
  void Init(IArchiveOpenCallback *callback) { Callback = callback; }
};

STDMETHODIMP CCompressProgressInfoImp::SetRatioInfo(const UInt64 *inSize, const UInt64 * /* outSize */)
{
  if (Callback)
  {
    UInt64 files = 0;
    UInt64 value = Offset + *inSize;
    return Callback->SetCompleted(&files, &value);
  }
  return S_OK;
}

STDMETHODIMP CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback)
{
  COM_TRY_BEGIN
  if (numItems == 0)
    return S_OK;
  if (numItems != (UInt32)(Int32)-1 && (numItems != 1 || indices[0] != 0))
    return E_INVALIDARG;

  extractCallback->SetTotal(_item.GetSize());
  CMyComPtr<ISequentialOutStream> realOutStream;
  Int32 askMode = testMode ?
      NExtract::NAskMode::kTest :
      NExtract::NAskMode::kExtract;
  RINOK(extractCallback->GetStream(0, &realOutStream, askMode));
  if (!testMode && !realOutStream)
    return S_OK;

  extractCallback->PrepareOperation(askMode);

  CDummyOutStream *outStreamSpec = new CDummyOutStream;
  CMyComPtr<ISequentialOutStream> outStream(outStreamSpec);
  outStreamSpec->SetStream(realOutStream);
  outStreamSpec->Init();
  realOutStream.Release();

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  lps->InSize = _item.HeaderSize;
  lps->OutSize = outStreamSpec->GetSize();
  RINOK(lps->SetCur());
  
  CItem item = _item;
  item.MakeUncompressed();
  if (_stream)
    RINOK(_stream->Seek(_item.HeaderSize, STREAM_SEEK_SET, NULL));
  NCompress::NZlib::CDecoder *_decoderZlibSpec = NULL;
  NCompress::NLzma::CDecoder *_decoderLzmaSpec = NULL;
  CMyComPtr<ICompressCoder> _decoder;

  CMyComPtr<ISequentialInStream> inStream2;

  UInt64 unpackSize = _item.GetSize() - (UInt32)8;
  if (_item.IsZlib())
  {
    _decoderZlibSpec = new NCompress::NZlib::CDecoder;
    _decoder = _decoderZlibSpec;
    inStream2 = _seqStream;
  }
  else
  {
    /* Some .swf files with lzma contain additional 8 bytes at the end
       in uncompressed stream.
       What does that data mean ???
       We don't decompress these additional 8 bytes */
    
    // unpackSize = _item.GetSize();
    // SetUi32(item.Buf + 4, (UInt32)(unpackSize + 8));
    CLimitedSequentialInStream *limitedStreamSpec = new CLimitedSequentialInStream;
    inStream2 = limitedStreamSpec;
    limitedStreamSpec->SetStream(_seqStream);
    limitedStreamSpec->Init(_item.GetLzmaPackSize());

    _decoderLzmaSpec = new NCompress::NLzma::CDecoder;
    _decoder = _decoderLzmaSpec;
    // _decoderLzmaSpec->FinishStream = true;

    Byte props[5];
    memcpy(props, _item.Buf + 12, 5);
    UInt32 dicSize = _item.GetLzmaDicSize();
    if (dicSize > (UInt32)unpackSize)
    {
      dicSize = (UInt32)unpackSize;
      SetUi32(props + 1, dicSize);
    }
    RINOK(_decoderLzmaSpec->SetDecoderProperties2(props, 5));
  }
  RINOK(item.WriteHeader(outStream));
  HRESULT result = _decoder->Code(inStream2, outStream, NULL, &unpackSize, progress);
  Int32 opRes = NExtract::NOperationResult::kDataError;
  if (result == S_OK)
  {
    if (item.GetSize() == outStreamSpec->GetSize())
    {
      if (_item.IsZlib())
      {
        _packSizeDefined = true;
        _packSize = _decoderZlibSpec->GetInputProcessedSize();
        opRes = NExtract::NOperationResult::kOK;
      }
      else
      {
        // if (_decoderLzmaSpec->GetInputProcessedSize() == _packSize)
          opRes = NExtract::NOperationResult::kOK;
      }
    }
  }
  else if (result != S_FALSE)
    return result;

  outStream.Release();
  return extractCallback->SetOperationResult(opRes);
  COM_TRY_END
}


#ifdef SWF_UPDATE

static HRESULT UpdateArchive(ISequentialOutStream *outStream, UInt64 size,
    bool lzmaMode, const CSingleMethodProps &props,
    IArchiveUpdateCallback *updateCallback)
{
  UInt64 complexity = 0;
  RINOK(updateCallback->SetTotal(size));
  RINOK(updateCallback->SetCompleted(&complexity));

  CMyComPtr<ISequentialInStream> fileInStream;
  RINOK(updateCallback->GetStream(0, &fileInStream));

  /*
  CDummyOutStream *outStreamSpec = new CDummyOutStream;
  CMyComPtr<ISequentialOutStream> outStream(outStreamSpec);
  outStreamSpec->SetStream(realOutStream);
  outStreamSpec->Init();
  realOutStream.Release();
  */

  CItem item;
  HRESULT res = item.ReadHeader(fileInStream);
  if (res == S_FALSE)
    return E_INVALIDARG;
  RINOK(res);
  if (!item.IsSwf() || !item.IsUncompressed() || size != item.GetSize())
    return E_INVALIDARG;

  NCompress::NZlib::CEncoder *encoderZlibSpec = NULL;
  NCompress::NLzma::CEncoder *encoderLzmaSpec = NULL;
  CMyComPtr<ICompressCoder> encoder;
  CMyComPtr<IOutStream> outSeekStream;
  if (lzmaMode)
  {
    outStream->QueryInterface(IID_IOutStream, (void **)&outSeekStream);
    if (!outSeekStream)
      return E_NOTIMPL;
    encoderLzmaSpec = new NCompress::NLzma::CEncoder;
    encoder = encoderLzmaSpec;
    RINOK(props.SetCoderProps(encoderLzmaSpec, &size));
    item.MakeLzma((UInt32)0xFFFFFFFF);
    CBufPtrSeqOutStream *propStreamSpec = new CBufPtrSeqOutStream;
    CMyComPtr<ISequentialOutStream> propStream = propStreamSpec;
    propStreamSpec->Init(item.Buf + 12, 5);
    RINOK(encoderLzmaSpec->WriteCoderProperties(propStream));
  }
  else
  {
    encoderZlibSpec = new NCompress::NZlib::CEncoder;
    encoder = encoderZlibSpec;
    encoderZlibSpec->Create();
    RINOK(props.SetCoderProps(encoderZlibSpec->DeflateEncoderSpec, NULL));
    item.MakeZlib();
  }
  RINOK(item.WriteHeader(outStream));

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(updateCallback, true);
  
  RINOK(encoder->Code(fileInStream, outStream, NULL, NULL, progress));
  UInt64 inputProcessed;
  if (lzmaMode)
  {
    UInt64 curPos = 0;
    RINOK(outSeekStream->Seek(0, STREAM_SEEK_CUR, &curPos));
    UInt64 packSize = curPos - kHeaderLzmaSize;
    if (packSize > (UInt32)0xFFFFFFFF)
      return E_INVALIDARG;
    item.MakeLzma((UInt32)packSize);
    RINOK(outSeekStream->Seek(0, STREAM_SEEK_SET, NULL));
    item.WriteHeader(outStream);
    inputProcessed = encoderLzmaSpec->GetInputProcessedSize();
  }
  else
  {
    inputProcessed = encoderZlibSpec->GetInputProcessedSize();
  }
  if (inputProcessed + kHeaderBaseSize != size)
    return E_INVALIDARG;
  return updateCallback->SetOperationResult(NUpdate::NOperationResult::kOK);
}

STDMETHODIMP CHandler::GetFileTimeType(UInt32 *timeType)
{
  *timeType = NFileTimeType::kUnix;
  return S_OK;
}

STDMETHODIMP CHandler::UpdateItems(ISequentialOutStream *outStream, UInt32 numItems,
    IArchiveUpdateCallback *updateCallback)
{
  if (numItems != 1)
    return E_INVALIDARG;

  Int32 newData, newProps;
  UInt32 indexInArchive;
  if (!updateCallback)
    return E_FAIL;
  RINOK(updateCallback->GetUpdateItemInfo(0, &newData, &newProps, &indexInArchive));

  if (IntToBool(newProps))
  {
    {
      NCOM::CPropVariant prop;
      RINOK(updateCallback->GetProperty(0, kpidIsDir, &prop));
      if (prop.vt == VT_BOOL)
      {
        if (prop.boolVal != VARIANT_FALSE)
          return E_INVALIDARG;
      }
      else if (prop.vt != VT_EMPTY)
        return E_INVALIDARG;
    }
  }

  if (IntToBool(newData))
  {
    UInt64 size;
    {
      NCOM::CPropVariant prop;
      RINOK(updateCallback->GetProperty(0, kpidSize, &prop));
      if (prop.vt != VT_UI8)
        return E_INVALIDARG;
      size = prop.uhVal.QuadPart;
    }
    return UpdateArchive(outStream, size, _lzmaMode, _props, updateCallback);
  }
    
  if (indexInArchive != 0)
    return E_INVALIDARG;

  if (!_seqStream)
    return E_NOTIMPL;

  if (_stream)
  {
    RINOK(_stream->Seek(0, STREAM_SEEK_SET, NULL));
  }
  else
    _item.WriteHeader(outStream);
  return NCompress::CopyStream(_seqStream, outStream, NULL);
}

STDMETHODIMP CHandler::SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps)
{
  _lzmaMode = false;
  RINOK(_props.SetProperties(names, values, numProps));
  const AString &m = _props.MethodName;
  if (m.IsEqualTo_Ascii_NoCase("lzma"))
  {
    return E_NOTIMPL;
    // _lzmaMode = true;
  }
  else if (m.IsEqualTo_Ascii_NoCase("Deflate") || m.IsEmpty())
    _lzmaMode = false;
  else
    return E_INVALIDARG;
  return S_OK;
}

#endif


static const Byte k_Signature[] = {
    3, 'C', 'W', 'S',
    3, 'Z', 'W', 'S' };

REGISTER_ARC_I(
  "SWFc", "swf", "~.swf", 0xD8,
  k_Signature,
  0,
  NArcInfoFlags::kMultiSignature,
  IsArc_Swfc)

}

namespace NSwf {

static const unsigned kNumTagsMax = 1 << 23;

struct CTag
{
  UInt32 Type;
  CByteBuffer Buf;
};

class CHandler:
  public IInArchive,
  public IArchiveOpenSeq,
  public CMyUnknownImp
{
  CObjectVector<CTag> _tags;
  NSwfc::CItem _item;
  UInt64 _phySize;

  HRESULT OpenSeq3(ISequentialInStream *stream, IArchiveOpenCallback *callback);
  HRESULT OpenSeq2(ISequentialInStream *stream, IArchiveOpenCallback *callback);
public:
  MY_UNKNOWN_IMP2(IInArchive, IArchiveOpenSeq)
  INTERFACE_IInArchive(;)

  STDMETHOD(OpenSeq)(ISequentialInStream *stream);
};

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidComment,
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps_NO_Table

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPhySize: prop = _phySize; break;
    case kpidIsNotArcType: prop = true; break;
  }
  prop.Detach(value);
  return S_OK;
}


STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = _tags.Size();
  return S_OK;
}

static const char * const g_TagDesc[92] =
{
    "End"
  , "ShowFrame"
  , "DefineShape"
  , NULL
  , "PlaceObject"
  , "RemoveObject"
  , "DefineBits"
  , "DefineButton"
  , "JPEGTables"
  , "SetBackgroundColor"
  , "DefineFont"
  , "DefineText"
  , "DoAction"
  , "DefineFontInfo"
  , "DefineSound"
  , "StartSound"
  , NULL
  , "DefineButtonSound"
  , "SoundStreamHead"
  , "SoundStreamBlock"
  , "DefineBitsLossless"
  , "DefineBitsJPEG2"
  , "DefineShape2"
  , "DefineButtonCxform"
  , "Protect"
  , NULL
  , "PlaceObject2"
  , NULL
  , "RemoveObject2"
  , NULL
  , NULL
  , NULL
  , "DefineShape3"
  , "DefineText2"
  , "DefineButton2"
  , "DefineBitsJPEG3"
  , "DefineBitsLossless2"
  , "DefineEditText"
  , NULL
  , "DefineSprite"
  , NULL
  , "41"
  , NULL
  , "FrameLabel"
  , NULL
  , "SoundStreamHead2"
  , "DefineMorphShape"
  , NULL
  , "DefineFont2"
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , "ExportAssets"
  , "ImportAssets"
  , "EnableDebugger"
  , "DoInitAction"
  , "DefineVideoStream"
  , "VideoFrame"
  , "DefineFontInfo2"
  , NULL
  , "EnableDebugger2"
  , "ScriptLimits"
  , "SetTabIndex"
  , NULL
  , NULL
  , "FileAttributes"
  , "PlaceObject3"
  , "ImportAssets2"
  , NULL
  , "DefineFontAlignZones"
  , "CSMTextSettings"
  , "DefineFont3"
  , "SymbolClass"
  , "Metadata"
  , "DefineScalingGrid"
  , NULL
  , NULL
  , NULL
  , "DoABC"
  , "DefineShape4"
  , "DefineMorphShape2"
  , NULL
  , "DefineSceneAndFrameLabelData"
  , "DefineBinaryData"
  , "DefineFontName"
  , "StartSound2"
  , "DefineBitsJPEG4"
  , "DefineFont4"
};

STDMETHODIMP CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
{
  NWindows::NCOM::CPropVariant prop;
  const CTag &tag = _tags[index];
  switch (propID)
  {
    case kpidPath:
    {
      char s[32];
      ConvertUInt32ToString(index, s);
      size_t i = strlen(s);
      s[i++] = '.';
      ConvertUInt32ToString(tag.Type, s + i);
      prop = s;
      break;
    }
    case kpidSize:
    case kpidPackSize:
      prop = (UInt64)tag.Buf.Size(); break;
    case kpidComment:
      TYPE_TO_PROP(g_TagDesc, tag.Type, prop);
      break;
  }
  prop.Detach(value);
  return S_OK;
}

STDMETHODIMP CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *callback)
{
  return OpenSeq2(stream, callback);
}

static UInt16 Read16(CInBuffer &stream)
{
  UInt32 res = 0;
  for (unsigned i = 0; i < 2; i++)
  {
    Byte b;
    if (!stream.ReadByte(b))
      throw 1;
    res |= (UInt32)b << (i * 8);
  }
  return (UInt16)res;
}

static UInt32 Read32(CInBuffer &stream)
{
  UInt32 res = 0;
  for (unsigned i = 0; i < 4; i++)
  {
    Byte b;
    if (!stream.ReadByte(b))
      throw 1;
    res |= (UInt32)b << (i * 8);
  }
  return res;
}

struct CBitReader
{
  CInBuffer *stream;
  unsigned NumBits;
  Byte Val;

  CBitReader(): NumBits(0), Val(0) {}

  UInt32 ReadBits(unsigned numBits);
};

UInt32 CBitReader::ReadBits(unsigned numBits)
{
  UInt32 res = 0;
  while (numBits > 0)
  {
    if (NumBits == 0)
    {
      Val = stream->ReadByte();
      NumBits = 8;
    }
    if (numBits <= NumBits)
    {
      res <<= numBits;
      NumBits -= numBits;
      res |= (Val >> NumBits);
      Val = (Byte)(Val & (((unsigned)1 << NumBits) - 1));
      break;
    }
    else
    {
      res <<= NumBits;
      res |= Val;
      numBits -= NumBits;
      NumBits = 0;
    }
  }
  return res;
}

HRESULT CHandler::OpenSeq3(ISequentialInStream *stream, IArchiveOpenCallback *callback)
{
  RINOK(_item.ReadHeader(stream))
  if (!_item.IsSwf() || !_item.IsUncompressed())
    return S_FALSE;
  UInt32 uncompressedSize = _item.GetSize();
  if (uncompressedSize > kFileSizeMax)
    return S_FALSE;

  
  CInBuffer s;
  if (!s.Create(1 << 20))
    return E_OUTOFMEMORY;
  s.SetStream(stream);
  s.Init();
  {
    CBitReader br;
    br.stream = &s;
    unsigned numBits = br.ReadBits(5);
    /* UInt32 xMin = */ br.ReadBits(numBits);
    /* UInt32 xMax = */ br.ReadBits(numBits);
    /* UInt32 yMin = */ br.ReadBits(numBits);
    /* UInt32 yMax = */ br.ReadBits(numBits);
  }
  /* UInt32 frameDelay = */ Read16(s);
  /* UInt32 numFrames =  */ Read16(s);

  _tags.Clear();
  UInt64 offsetPrev = 0;
  for (;;)
  {
    UInt32 pair = Read16(s);
    UInt32 type = pair >> 6;
    UInt32 length = pair & 0x3F;
    if (length == 0x3F)
      length = Read32(s);
    if (type == 0)
      break;
    UInt64 offset = s.GetProcessedSize() + NSwfc::kHeaderBaseSize + length;
    if (offset > uncompressedSize || _tags.Size() >= kNumTagsMax)
      return S_FALSE;
    CTag &tag = _tags.AddNew();
    tag.Type = type;
    tag.Buf.Alloc(length);
    if (s.ReadBytes(tag.Buf, length) != length)
      return S_FALSE;
    if (callback && offset >= offsetPrev + (1 << 20))
    {
      UInt64 numItems = _tags.Size();
      RINOK(callback->SetCompleted(&numItems, &offset));
      offsetPrev = offset;
    }
  }
  _phySize = s.GetProcessedSize() + NSwfc::kHeaderBaseSize;
  if (_phySize != uncompressedSize)
  {
    // do we need to support files extracted from SFW-LZMA with additional 8 bytes?
    return S_FALSE;
  }
  return S_OK;
}

HRESULT CHandler::OpenSeq2(ISequentialInStream *stream, IArchiveOpenCallback *callback)
{
  HRESULT res;
  try { res = OpenSeq3(stream, callback); }
  catch(...) { res = S_FALSE; }
  return res;
}

STDMETHODIMP CHandler::OpenSeq(ISequentialInStream *stream)
{
  return OpenSeq2(stream, NULL);
}

STDMETHODIMP CHandler::Close()
{
  _phySize = 0;
  return S_OK;
}

STDMETHODIMP CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback)
{
  COM_TRY_BEGIN
  bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _tags.Size();
  if (numItems == 0)
    return S_OK;
  UInt64 totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
    totalSize += _tags[allFilesMode ? i : indices[i]].Buf.Size();
  extractCallback->SetTotal(totalSize);

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  totalSize = 0;

  for (i = 0; i < numItems; i++)
  {
    lps->InSize = lps->OutSize = totalSize;
    RINOK(lps->SetCur());
    Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    UInt32 index = allFilesMode ? i : indices[i];
    const CByteBuffer &buf = _tags[index].Buf;
    totalSize += buf.Size();

    CMyComPtr<ISequentialOutStream> outStream;
    RINOK(extractCallback->GetStream(index, &outStream, askMode));
    if (!testMode && !outStream)
      continue;
      
    RINOK(extractCallback->PrepareOperation(askMode));
    if (outStream)
      RINOK(WriteStream(outStream, buf, buf.Size()));
    outStream.Release();
    RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK));
  }
  return S_OK;
  COM_TRY_END
}

static const Byte k_Signature[] = { 'F', 'W', 'S' };

REGISTER_ARC_I(
  "SWF", "swf", 0, 0xD7,
  k_Signature,
  0,
  NArcInfoFlags::kKeepName,
  NSwfc::IsArc_Swf)

}}

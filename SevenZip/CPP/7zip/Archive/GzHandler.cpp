// GzHandler.cpp

#include "StdAfx.h"

// #include  <stdio.h>

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/Defs.h"
#include "../../Common/StringConvert.h"

#include "../../Windows/PropVariant.h"
#include "../../Windows/PropVariantUtils.h"
#include "../../Windows/TimeUtils.h"

#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"
#include "../Compress/DeflateDecoder.h"
#include "../Compress/DeflateEncoder.h"

#include "Common/HandlerOut.h"
#include "Common/InStreamWithCRC.h"
#include "Common/OutStreamWithCRC.h"

#define Get32(p) GetUi32(p)

using namespace NWindows;

using namespace NCompress;
using namespace NDeflate;

namespace NArchive {
namespace NGz {

  static const Byte kSignature_0 = 0x1F;
  static const Byte kSignature_1 = 0x8B;
  static const Byte kSignature_2 = 8; //  NCompressionMethod::kDeflate

  // Latest versions of gzip program don't write comment field to gz archive.
  // We also don't write comment field to gz archive.

  namespace NFlags
  {
    // const Byte kIsText  = 1 << 0;
    const Byte kCrc     = 1 << 1;
    const Byte kExtra   = 1 << 2;
    const Byte kName    = 1 << 3;
    const Byte kComment = 1 << 4;
    const Byte kReserved = 0xE0;
  }
  
  namespace NExtraFlags
  {
    const Byte kMaximum = 2;
    const Byte kFastest = 4;
  }
  
  namespace NHostOS
  {
    enum EEnum
    {
      kFAT = 0,
      kAMIGA,
      kVMS,
      kUnix,
      kVM_CMS,
      kAtari,
      kHPFS,
      kMac,
      kZ_System,
      kCPM,
      kTOPS20,
      kNTFS,
      kQDOS,
      kAcorn,
      kVFAT,
      kMVS,
      kBeOS,
      kTandem,
      
      kUnknown = 255
    };
  }

static const char * const kHostOSes[] =
{
    "FAT"
  , "AMIGA"
  , "VMS"
  , "Unix"
  , "VM/CMS"
  , "Atari"
  , "HPFS"
  , "Macintosh"
  , "Z-System"
  , "CP/M"
  , "TOPS-20"
  , "NTFS"
  , "SMS/QDOS"
  , "Acorn"
  , "VFAT"
  , "MVS"
  , "BeOS"
  , "Tandem"
  , "OS/400"
  , "OS/X"
};


class CItem
{
  bool TestFlag(Byte flag) const { return (Flags & flag) != 0; }
public:
  Byte Flags;
  Byte ExtraFlags;
  Byte HostOS;
  UInt32 Time;
  UInt32 Crc;
  UInt32 Size32;

  AString Name;
  AString Comment;
  // CByteBuffer Extra;

  CItem():
    Flags(0),
    ExtraFlags(0),
    HostOS(0),
    Time(0),
    Crc(0),
    Size32(0) {}

  void Clear()
  {
    Name.Empty();
    Comment.Empty();
    // Extra.Free();
  }

  void CopyMetaPropsFrom(const CItem &a)
  {
    Flags = a.Flags;
    HostOS = a.HostOS;
    Time = a.Time;
    Name = a.Name;
    Comment = a.Comment;
    // Extra = a.Extra;
  }

  void CopyDataPropsFrom(const CItem &a)
  {
    ExtraFlags = a.ExtraFlags;
    Crc = a.Crc;
    Size32 = a.Size32;
  }
  
  // bool IsText() const { return TestFlag(NFlags::kIsText); }
  bool HeaderCrcIsPresent() const { return TestFlag(NFlags::kCrc); }
  bool ExtraFieldIsPresent() const { return TestFlag(NFlags::kExtra); }
  bool NameIsPresent() const { return TestFlag(NFlags::kName); }
  bool CommentIsPresent() const { return TestFlag(NFlags::kComment); }
  bool IsSupported() const { return (Flags & NFlags::kReserved) == 0; }

  HRESULT ReadHeader(NDecoder::CCOMCoder *stream);
  HRESULT ReadFooter1(NDecoder::CCOMCoder *stream);
  HRESULT ReadFooter2(ISequentialInStream *stream);

  HRESULT WriteHeader(ISequentialOutStream *stream);
  HRESULT WriteFooter(ISequentialOutStream *stream);
};

static HRESULT ReadBytes(NDecoder::CCOMCoder *stream, Byte *data, UInt32 size)
{
  for (UInt32 i = 0; i < size; i++)
    data[i] = stream->ReadAlignedByte();
  return stream->InputEofError() ? S_FALSE : S_OK;
}

static HRESULT SkipBytes(NDecoder::CCOMCoder *stream, UInt32 size)
{
  for (UInt32 i = 0; i < size; i++)
    stream->ReadAlignedByte();
  return stream->InputEofError() ? S_FALSE : S_OK;
}

static HRESULT ReadUInt16(NDecoder::CCOMCoder *stream, UInt32 &value /* , UInt32 &crc */)
{
  value = 0;
  for (int i = 0; i < 2; i++)
  {
    Byte b = stream->ReadAlignedByte();
    if (stream->InputEofError())
      return S_FALSE;
    // crc = CRC_UPDATE_BYTE(crc, b);
    value |= ((UInt32)(b) << (8 * i));
  }
  return S_OK;
}

static HRESULT ReadString(NDecoder::CCOMCoder *stream, AString &s, size_t limit /* , UInt32 &crc */)
{
  s.Empty();
  for (size_t i = 0; i < limit; i++)
  {
    Byte b = stream->ReadAlignedByte();
    if (stream->InputEofError())
      return S_FALSE;
    // crc = CRC_UPDATE_BYTE(crc, b);
    if (b == 0)
      return S_OK;
    s += (char)b;
  }
  return S_FALSE;
}

static UInt32 Is_Deflate(const Byte *p, size_t size)
{
  if (size < 1)
    return k_IsArc_Res_NEED_MORE;
  Byte b = *p;
  p++;
  size--;
  unsigned type = ((unsigned)b >> 1) & 3;
  if (type == 3)
    return k_IsArc_Res_NO;
  if (type == 0)
  {
     // Stored (uncompreessed data)
    if ((b >> 3) != 0)
      return k_IsArc_Res_NO;
    if (size < 4)
      return k_IsArc_Res_NEED_MORE;
    UInt16 r = (UInt16)~GetUi16(p + 2);
    if (GetUi16(p) != r)
      return k_IsArc_Res_NO;
  }
  else if (type == 2)
  {
    // Dynamic Huffman
    if (size < 1)
      return k_IsArc_Res_NEED_MORE;
    if ((*p & 0x1F) + 1 > 30) // numDistLevels
      return k_IsArc_Res_NO;
  }
  return k_IsArc_Res_YES;
}

static const unsigned kNameMaxLen = 1 << 12;
static const unsigned kCommentMaxLen = 1 << 16;

API_FUNC_static_IsArc IsArc_Gz(const Byte *p, size_t size)
{
  if (size < 10)
    return k_IsArc_Res_NEED_MORE;
  if (p[0] != kSignature_0 ||
      p[1] != kSignature_1 ||
      p[2] != kSignature_2)
    return k_IsArc_Res_NO;

  Byte flags = p[3];
  if ((flags & NFlags::kReserved) != 0)
    return k_IsArc_Res_NO;

  Byte extraFlags = p[8];
  // maybe that flag can have another values for some gz archives?
  if (extraFlags != 0 &&
      extraFlags != NExtraFlags::kMaximum &&
      extraFlags != NExtraFlags::kFastest)
    return k_IsArc_Res_NO;

  size -= 10;
  p += 10;

  if ((flags & NFlags::kExtra) != 0)
  {
    if (size < 2)
      return k_IsArc_Res_NEED_MORE;
    unsigned xlen = GetUi16(p);
    size -= 2;
    p += 2;
    while (xlen != 0)
    {
      if (xlen < 4)
        return k_IsArc_Res_NO;
      if (size < 4)
        return k_IsArc_Res_NEED_MORE;
      unsigned len = GetUi16(p + 2);
      size -= 4;
      xlen -= 4;
      p += 4;
      if (len > xlen)
        return k_IsArc_Res_NO;
      if (len > size)
        return k_IsArc_Res_NEED_MORE;
      size -= len;
      xlen -= len;
      p += len;
    }
  }

  if ((flags & NFlags::kName) != 0)
  {
    size_t limit = kNameMaxLen;
    if (limit > size)
      limit = size;
    size_t i;
    for (i = 0; i < limit && p[i] != 0; i++);
    if (i == size)
      return k_IsArc_Res_NEED_MORE;
    if (i == limit)
      return k_IsArc_Res_NO;
    i++;
    p += i;
    size -= i;
  }

  if ((flags & NFlags::kComment) != 0)
  {
    size_t limit = kCommentMaxLen;
    if (limit > size)
      limit = size;
    size_t i;
    for (i = 0; i < limit && p[i] != 0; i++);
    if (i == size)
      return k_IsArc_Res_NEED_MORE;
    if (i == limit)
      return k_IsArc_Res_NO;
    i++;
    p += i;
    size -= i;
  }
  
  if ((flags & NFlags::kCrc) != 0)
  {
    if (size < 2)
      return k_IsArc_Res_NEED_MORE;
    p += 2;
    size -= 2;
  }

  return Is_Deflate(p, size);
}
}

HRESULT CItem::ReadHeader(NDecoder::CCOMCoder *stream)
{
  Clear();

  // Header-CRC field had another meaning in old version of gzip!
  // UInt32 crc = CRC_INIT_VAL;
  Byte buf[10];

  RINOK(ReadBytes(stream, buf, 10));
  
  if (buf[0] != kSignature_0 ||
      buf[1] != kSignature_1 ||
      buf[2] != kSignature_2)
    return S_FALSE;

  Flags = buf[3];
  if (!IsSupported())
    return S_FALSE;

  Time = Get32(buf + 4);
  ExtraFlags = buf[8];
  HostOS = buf[9];

  // crc = CrcUpdate(crc, buf, 10);
  
  if (ExtraFieldIsPresent())
  {
    UInt32 xlen;
    RINOK(ReadUInt16(stream, xlen /* , crc */));
    RINOK(SkipBytes(stream, xlen));
    // Extra.SetCapacity(xlen);
    // RINOK(ReadStream_FALSE(stream, Extra, xlen));
    // crc = CrcUpdate(crc, Extra, xlen);
  }
  if (NameIsPresent())
    RINOK(ReadString(stream, Name, kNameMaxLen /* , crc */));
  if (CommentIsPresent())
    RINOK(ReadString(stream, Comment, kCommentMaxLen /* , crc */));

  if (HeaderCrcIsPresent())
  {
    UInt32 headerCRC;
    // UInt32 dummy = 0;
    RINOK(ReadUInt16(stream, headerCRC /* , dummy */));
    /*
    if ((UInt16)CRC_GET_DIGEST(crc) != headerCRC)
      return S_FALSE;
    */
  }
  return stream->InputEofError() ? S_FALSE : S_OK;
}

HRESULT CItem::ReadFooter1(NDecoder::CCOMCoder *stream)
{
  Byte buf[8];
  RINOK(ReadBytes(stream, buf, 8));
  Crc = Get32(buf);
  Size32 = Get32(buf + 4);
  return stream->InputEofError() ? S_FALSE : S_OK;
}

HRESULT CItem::ReadFooter2(ISequentialInStream *stream)
{
  Byte buf[8];
  RINOK(ReadStream_FALSE(stream, buf, 8));
  Crc = Get32(buf);
  Size32 = Get32(buf + 4);
  return S_OK;
}

HRESULT CItem::WriteHeader(ISequentialOutStream *stream)
{
  Byte buf[10];
  buf[0] = kSignature_0;
  buf[1] = kSignature_1;
  buf[2] = kSignature_2;
  buf[3] = (Byte)(Flags & NFlags::kName);
  // buf[3] |= NFlags::kCrc;
  SetUi32(buf + 4, Time);
  buf[8] = ExtraFlags;
  buf[9] = HostOS;
  RINOK(WriteStream(stream, buf, 10));
  // crc = CrcUpdate(CRC_INIT_VAL, buf, 10);
  if (NameIsPresent())
  {
    // crc = CrcUpdate(crc, (const char *)Name, Name.Len() + 1);
    RINOK(WriteStream(stream, (const char *)Name, Name.Len() + 1));
  }
  // SetUi16(buf, (UInt16)CRC_GET_DIGEST(crc));
  // RINOK(WriteStream(stream, buf, 2));
  return S_OK;
}

HRESULT CItem::WriteFooter(ISequentialOutStream *stream)
{
  Byte buf[8];
  SetUi32(buf, Crc);
  SetUi32(buf + 4, Size32);
  return WriteStream(stream, buf, 8);
}

class CHandler:
  public IInArchive,
  public IArchiveOpenSeq,
  public IOutArchive,
  public ISetProperties,
  public CMyUnknownImp
{
  CItem _item;

  bool _isArc;
  bool _needSeekToStart;
  bool _dataAfterEnd;
  bool _needMoreInput;

  bool _packSize_Defined;
  bool _unpackSize_Defined;
  bool _numStreams_Defined;

  UInt64 _packSize;
  UInt64 _unpackSize; // real unpack size (NOT from footer)
  UInt64 _numStreams;
  UInt64 _headerSize; // only start header (without footer)
  
  CMyComPtr<IInStream> _stream;
  CMyComPtr<ICompressCoder> _decoder;
  NDecoder::CCOMCoder *_decoderSpec;

  CSingleMethodProps _props;

public:
  MY_UNKNOWN_IMP4(
      IInArchive,
      IArchiveOpenSeq,
      IOutArchive,
      ISetProperties)
  INTERFACE_IInArchive(;)
  INTERFACE_IOutArchive(;)
  STDMETHOD(OpenSeq)(ISequentialInStream *stream);
  STDMETHOD(SetProperties)(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps);

  CHandler()
  {
    _decoderSpec = new NDecoder::CCOMCoder;
    _decoder = _decoderSpec;
  }
};

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidPackSize,
  kpidMTime,
  kpidHostOS,
  kpidCRC
  // kpidComment
};

static const Byte kArcProps[] =
{
  kpidHeadersSize,
  kpidNumStreams
};


IMP_IInArchive_Props
IMP_IInArchive_ArcProps

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPhySize: if (_packSize_Defined) prop = _packSize; break;
    case kpidUnpackSize: if (_unpackSize_Defined) prop = _unpackSize; break;
    case kpidNumStreams: if (_numStreams_Defined) prop = _numStreams; break;
    case kpidHeadersSize: if (_headerSize != 0) prop = _headerSize; break;
    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc) v |= kpv_ErrorFlags_IsNotArc;;
      if (_needMoreInput) v |= kpv_ErrorFlags_UnexpectedEnd;
      if (_dataAfterEnd) v |= kpv_ErrorFlags_DataAfterEnd;
      prop = v;
      break;
    }
    case kpidName:
      if (_item.NameIsPresent())
      {
        UString s = MultiByteToUnicodeString(_item.Name, CP_ACP);
        s += ".gz";
        prop = s;
      }
      break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = 1;
  return S_OK;
}

STDMETHODIMP CHandler::GetProperty(UInt32 /* index */, PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPath:
      if (_item.NameIsPresent())
        prop = MultiByteToUnicodeString(_item.Name, CP_ACP);
      break;
    // case kpidComment: if (_item.CommentIsPresent()) prop = MultiByteToUnicodeString(_item.Comment, CP_ACP); break;
    case kpidMTime:
      if (_item.Time != 0)
      {
        FILETIME utc;
        NTime::UnixTimeToFileTime(_item.Time, utc);
        prop = utc;
      }
      break;
    case kpidSize:
    {
      if (_unpackSize_Defined)
        prop = _unpackSize;
      else if (_stream)
        prop = (UInt64)_item.Size32;
      break;
    }
    case kpidPackSize:
    {
      if (_packSize_Defined || _stream)
        prop = _packSize;
      break;
    }
    case kpidHostOS: TYPE_TO_PROP(kHostOSes, _item.HostOS, prop); break;
    case kpidCRC: if (_stream) prop = _item.Crc; break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
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

/*
*/

STDMETHODIMP CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *)
{
  COM_TRY_BEGIN
  RINOK(OpenSeq(stream));
  _isArc = false;
  UInt64 endPos;
  RINOK(stream->Seek(-8, STREAM_SEEK_END, &endPos));
  _packSize = endPos + 8;
  RINOK(_item.ReadFooter2(stream));
  _stream = stream;
  _isArc = true;
  _needSeekToStart = true;
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::OpenSeq(ISequentialInStream *stream)
{
  COM_TRY_BEGIN
  try
  {
    Close();
    _decoderSpec->SetInStream(stream);
    _decoderSpec->InitInStream(true);
    RINOK(_item.ReadHeader(_decoderSpec));
    if (_decoderSpec->InputEofError())
      return S_FALSE;
    _headerSize = _decoderSpec->GetInputProcessedSize();
    _isArc = true;
    return S_OK;
  }
  catch(const CInBufferException &e) { return e.ErrorCode; }
  COM_TRY_END
}

STDMETHODIMP CHandler::Close()
{
  _isArc = false;
  _needSeekToStart = false;
  _dataAfterEnd = false;
  _needMoreInput = false;

  _packSize_Defined = false;
  _unpackSize_Defined = false;
  _numStreams_Defined = false;

  _packSize = 0;
  _headerSize = 0;
  
  _stream.Release();
  _decoderSpec->ReleaseInStream();
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

  if (_packSize_Defined)
    extractCallback->SetTotal(_packSize);
  // UInt64 currentTotalPacked = 0;
  // RINOK(extractCallback->SetCompleted(&currentTotalPacked));
  CMyComPtr<ISequentialOutStream> realOutStream;
  Int32 askMode = testMode ?
      NExtract::NAskMode::kTest :
      NExtract::NAskMode::kExtract;
  RINOK(extractCallback->GetStream(0, &realOutStream, askMode));
  if (!testMode && !realOutStream)
    return S_OK;

  extractCallback->PrepareOperation(askMode);

  COutStreamWithCRC *outStreamSpec = new COutStreamWithCRC;
  CMyComPtr<ISequentialOutStream> outStream(outStreamSpec);
  outStreamSpec->SetStream(realOutStream);
  outStreamSpec->Init();
  realOutStream.Release();

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, true);

  bool needReadFirstItem = _needSeekToStart;
  
  if (_needSeekToStart)
  {
    if (!_stream)
      return E_FAIL;
    RINOK(_stream->Seek(0, STREAM_SEEK_SET, NULL));
    _decoderSpec->InitInStream(true);
    // printf("\nSeek");
  }
  else
    _needSeekToStart = true;

  bool firstItem = true;

  UInt64 packSize = _decoderSpec->GetInputProcessedSize();
  // printf("\npackSize = %d", (unsigned)packSize);

  UInt64 unpackedSize = 0;
  UInt64 numStreams = 0;

  bool crcError = false;

  HRESULT result = S_OK;

  try {
  
  for (;;)
  {
    lps->InSize = packSize;
    lps->OutSize = unpackedSize;

    RINOK(lps->SetCur());

    CItem item;
    
    if (!firstItem || needReadFirstItem)
    {
      result = item.ReadHeader(_decoderSpec);

      if (result != S_OK && result != S_FALSE)
        return result;
    
      if (_decoderSpec->InputEofError())
        result = S_FALSE;

      if (result != S_OK && firstItem)
      {
        _isArc = false;
        break;
      }

      if (packSize == _decoderSpec->GetStreamSize())
      {
        result = S_OK;
        break;
      }

      if (result != S_OK)
      {
        _dataAfterEnd = true;
        break;
      }
    }
    
    numStreams++;
    firstItem = false;

    UInt64 startOffset = outStreamSpec->GetSize();
    outStreamSpec->InitCRC();

    result = _decoderSpec->CodeResume(outStream, NULL, progress);

    packSize = _decoderSpec->GetInputProcessedSize();
    unpackedSize = outStreamSpec->GetSize();

    if (result != S_OK && result != S_FALSE)
      return result;

    if (_decoderSpec->InputEofError())
    {
      packSize = _decoderSpec->GetStreamSize();
      _needMoreInput = true;
      result = S_FALSE;
    }

    if (result != S_OK)
      break;

    _decoderSpec->AlignToByte();
    
    result = item.ReadFooter1(_decoderSpec);

    packSize = _decoderSpec->GetInputProcessedSize();

    if (result != S_OK && result != S_FALSE)
      return result;

    if (result != S_OK)
    {
      if (_decoderSpec->InputEofError())
      {
        _needMoreInput = true;
        result = S_FALSE;
      }
      break;
    }

    if (item.Crc != outStreamSpec->GetCRC() ||
        item.Size32 != (UInt32)(unpackedSize - startOffset))
    {
      crcError = true;
      result = S_FALSE;
      break;
    }

    // break; // we can use break, if we need only first stream
  }

  } catch(const CInBufferException &e) { return e.ErrorCode; }

  if (!firstItem)
  {
    _packSize = packSize;
    _unpackSize = unpackedSize;
    _numStreams = numStreams;

    _packSize_Defined = true;
    _unpackSize_Defined = true;
    _numStreams_Defined = true;
  }

  outStream.Release();

  Int32 retResult = NExtract::NOperationResult::kDataError;

  if (!_isArc)
    retResult = NExtract::NOperationResult::kIsNotArc;
  else if (_needMoreInput)
    retResult = NExtract::NOperationResult::kUnexpectedEnd;
  else if (crcError)
    retResult = NExtract::NOperationResult::kCRCError;
  else if (_dataAfterEnd)
    retResult = NExtract::NOperationResult::kDataAfterEnd;
  else if (result == S_FALSE)
    retResult = NExtract::NOperationResult::kDataError;
  else if (result == S_OK)
    retResult = NExtract::NOperationResult::kOK;
  else
    return result;

  return extractCallback->SetOperationResult(retResult);


  COM_TRY_END
}

static const Byte kHostOS =
  #ifdef _WIN32
  NHostOS::kFAT;
  #else
  NHostOS::kUnix;
  #endif

static HRESULT UpdateArchive(
    ISequentialOutStream *outStream,
    UInt64 unpackSize,
    CItem &item,
    const CSingleMethodProps &props,
    IArchiveUpdateCallback *updateCallback)
{
  UInt64 complexity = 0;
  RINOK(updateCallback->SetTotal(unpackSize));
  RINOK(updateCallback->SetCompleted(&complexity));

  CMyComPtr<ISequentialInStream> fileInStream;

  RINOK(updateCallback->GetStream(0, &fileInStream));

  CSequentialInStreamWithCRC *inStreamSpec = new CSequentialInStreamWithCRC;
  CMyComPtr<ISequentialInStream> crcStream(inStreamSpec);
  inStreamSpec->SetStream(fileInStream);
  inStreamSpec->Init();

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(updateCallback, true);
  
  item.ExtraFlags = props.GetLevel() >= 7 ?
      NExtraFlags::kMaximum :
      NExtraFlags::kFastest;

  item.HostOS = kHostOS;

  RINOK(item.WriteHeader(outStream));

  NEncoder::CCOMCoder *deflateEncoderSpec = new NEncoder::CCOMCoder;
  CMyComPtr<ICompressCoder> deflateEncoder = deflateEncoderSpec;
  RINOK(props.SetCoderProps(deflateEncoderSpec, NULL));
  RINOK(deflateEncoder->Code(crcStream, outStream, NULL, NULL, progress));

  item.Crc = inStreamSpec->GetCRC();
  item.Size32 = (UInt32)inStreamSpec->GetSize();
  RINOK(item.WriteFooter(outStream));
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
  COM_TRY_BEGIN

  if (numItems != 1)
    return E_INVALIDARG;

  Int32 newData, newProps;
  UInt32 indexInArchive;
  if (!updateCallback)
    return E_FAIL;
  RINOK(updateCallback->GetUpdateItemInfo(0, &newData, &newProps, &indexInArchive));

  CItem newItem;
  
  if (!IntToBool(newProps))
  {
    newItem.CopyMetaPropsFrom(_item);
  }
  else
  {
    newItem.HostOS = kHostOS;
    {
      NCOM::CPropVariant prop;
      RINOK(updateCallback->GetProperty(0, kpidMTime, &prop));
      if (prop.vt == VT_FILETIME)
        NTime::FileTimeToUnixTime(prop.filetime, newItem.Time);
      else if (prop.vt == VT_EMPTY)
        newItem.Time = 0;
      else
        return E_INVALIDARG;
    }
    {
      NCOM::CPropVariant prop;
      RINOK(updateCallback->GetProperty(0, kpidPath, &prop));
      if (prop.vt == VT_BSTR)
      {
        UString name = prop.bstrVal;
        int slashPos = name.ReverseFind_PathSepar();
        if (slashPos >= 0)
          name.DeleteFrontal((unsigned)(slashPos + 1));
        newItem.Name = UnicodeStringToMultiByte(name, CP_ACP);
        if (!newItem.Name.IsEmpty())
          newItem.Flags |= NFlags::kName;
      }
      else if (prop.vt != VT_EMPTY)
        return E_INVALIDARG;
    }
    {
      NCOM::CPropVariant prop;
      RINOK(updateCallback->GetProperty(0, kpidIsDir, &prop));
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
      RINOK(updateCallback->GetProperty(0, kpidSize, &prop));
      if (prop.vt != VT_UI8)
        return E_INVALIDARG;
      size = prop.uhVal.QuadPart;
    }
    return UpdateArchive(outStream, size, newItem, _props, updateCallback);
  }

  if (indexInArchive != 0)
    return E_INVALIDARG;

  if (!_stream)
    return E_NOTIMPL;

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

  newItem.CopyDataPropsFrom(_item);

  UInt64 offset = 0;
  if (IntToBool(newProps))
  {
    newItem.WriteHeader(outStream);
    offset += _headerSize;
  }
  RINOK(_stream->Seek((Int64)offset, STREAM_SEEK_SET, NULL));

  return NCompress::CopyStream(_stream, outStream, progress);

  COM_TRY_END
}

STDMETHODIMP CHandler::SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps)
{
  return _props.SetProperties(names, values, numProps);
}

static const Byte k_Signature[] = { kSignature_0, kSignature_1, kSignature_2 };

REGISTER_ARC_IO(
  "gzip", "gz gzip tgz tpz apk", "* * .tar .tar .tar", 0xEF,
  k_Signature,
  0,
  NArcInfoFlags::kKeepName,
  IsArc_Gz)

}}

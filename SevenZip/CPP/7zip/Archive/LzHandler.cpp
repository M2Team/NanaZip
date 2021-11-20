// LzHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"

#include "../../Windows/PropVariant.h"

#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "../Compress/LzmaDecoder.h"

#include "Common/OutStreamWithCRC.h"

using namespace NWindows;

namespace NArchive {
namespace NLz {

static const Byte kProps[] =
{
  kpidSize,
  kpidPackSize,
};

static const Byte kArcProps[] =
{
  kpidNumStreams
};

static const Byte k_Signature[5] = { 'L', 'Z', 'I', 'P', 1 };
enum { k_SignatureSize = 5 };

struct CHeader
{
  Byte data[6];		// 0-3 magic bytes, 4 version, 5 coded_dict_size
  enum { size = 6 };
  enum { min_dictionary_size = 1 << 12, max_dictionary_size = 1 << 29 };
  unsigned DicSize;
  Byte LzmaProps[5];

  bool Parse();
};

struct CTrailer
{
  Byte data[20];	//  0-3  CRC32 of the uncompressed data
			//  4-11 size of the uncompressed data
			// 12-19 member size including header and trailer
  enum { size = 20 };

  unsigned data_crc() const
    {
    unsigned tmp = 0;
    for( int i = 3; i >= 0; --i ) { tmp <<= 8; tmp += data[i]; }
    return tmp;
    }

  UInt64 data_size() const
    {
    UInt64 tmp = 0;
    for( int i = 11; i >= 4; --i ) { tmp <<= 8; tmp += data[i]; }
    return tmp;
    }

  UInt64 member_size() const
    {
    UInt64 tmp = 0;
    for( int i = 19; i >= 12; --i ) { tmp <<= 8; tmp += data[i]; }
    return tmp;
    }
};

class CDecoder
{
  CMyComPtr<ICompressCoder> _lzmaDecoder;
public:
  NCompress::NLzma::CDecoder *_lzmaDecoderSpec;

  ~CDecoder();
  HRESULT Create(ISequentialInStream *inStream);

  HRESULT Code(const CHeader &header, ISequentialOutStream *outStream, ICompressProgressInfo *progress);

  UInt64 GetInputProcessedSize() const { return _lzmaDecoderSpec->GetInputProcessedSize(); }

  void ReleaseInStream() { if (_lzmaDecoder) _lzmaDecoderSpec->ReleaseInStream(); }

  HRESULT ReadInput(Byte *data, UInt32 size, UInt32 *processedSize)
    { return _lzmaDecoderSpec->ReadFromInputStream(data, size, processedSize); }
};

HRESULT CDecoder::Create(ISequentialInStream *inStream)
{
  if (!_lzmaDecoder)
  {
    _lzmaDecoderSpec = new NCompress::NLzma::CDecoder;
    _lzmaDecoderSpec->FinishStream = true;
    _lzmaDecoder = _lzmaDecoderSpec;
  }

  return _lzmaDecoderSpec->SetInStream(inStream);
}

CDecoder::~CDecoder()
{
  ReleaseInStream();
}

HRESULT CDecoder::Code(const CHeader &header, ISequentialOutStream *outStream,
    ICompressProgressInfo *progress)
{
  {
    CMyComPtr<ICompressSetDecoderProperties2> setDecoderProperties;
    _lzmaDecoder.QueryInterface(IID_ICompressSetDecoderProperties2, &setDecoderProperties);
    if (!setDecoderProperties)
      return E_NOTIMPL;
    RINOK(setDecoderProperties->SetDecoderProperties2(header.LzmaProps, 5));
  }

  RINOK(_lzmaDecoderSpec->CodeResume(outStream, NULL, progress));

  return S_OK;
}


class CHandler:
  public IInArchive,
  public IArchiveOpenSeq,
  public CMyUnknownImp
{
  CHeader _header;
  CMyComPtr<IInStream> _stream;
  CMyComPtr<ISequentialInStream> _seqStream;

  bool _isArc;
  bool _needSeekToStart;
  bool _dataAfterEnd;
  bool _needMoreInput;

  bool _packSize_Defined;
  bool _unpackSize_Defined;
  bool _numStreams_Defined;

  bool _dataError;

  UInt64 _packSize;
  UInt64 _unpackSize;
  UInt64 _numStreams;

public:
  MY_UNKNOWN_IMP2(IInArchive, IArchiveOpenSeq)

  INTERFACE_IInArchive(;)
  STDMETHOD(OpenSeq)(ISequentialInStream *stream);

  CHandler() { }
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPhySize: if (_packSize_Defined) prop = _packSize; break;
    case kpidUnpackSize: if (_unpackSize_Defined) prop = _unpackSize; break;
    case kpidNumStreams: if (_numStreams_Defined) prop = _numStreams; break;
    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc) v |= kpv_ErrorFlags_IsNotArc;;
      if (_needMoreInput) v |= kpv_ErrorFlags_UnexpectedEnd;
      if (_dataAfterEnd) v |= kpv_ErrorFlags_DataAfterEnd;
      if (_dataError) v |= kpv_ErrorFlags_DataError;
      prop = v;
    }
  }
  prop.Detach(value);
  return S_OK;
}

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = 1;
  return S_OK;
}

STDMETHODIMP CHandler::GetProperty(UInt32 /* index */, PROPID propID, PROPVARIANT *value)
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidSize: if (_unpackSize_Defined) prop = _unpackSize; break;
    case kpidPackSize: if (_packSize_Defined) prop = _packSize; break;
  }
  prop.Detach(value);
  return S_OK;
}

API_FUNC_static_IsArc IsArc_Lz(const Byte *p, size_t size)
{
  if (size < k_SignatureSize)
    return k_IsArc_Res_NEED_MORE;
  for( int i = 0; i < k_SignatureSize; ++i )
    if( p[i] != k_Signature[i] )
      return k_IsArc_Res_NO;
  return k_IsArc_Res_YES;
}
}

bool CHeader::Parse()
{
  if (IsArc_Lz(data, k_SignatureSize) == k_IsArc_Res_NO)
    return false;
  DicSize = ( 1 << ( data[5] & 0x1F ) );
  if( DicSize > min_dictionary_size )
    DicSize -= ( DicSize / 16 ) * ( ( data[5] >> 5 ) & 7 );
  LzmaProps[0] = 93;			/* (45 * 2) + (9 * 0) + 3 */
  unsigned ds = DicSize;
  for( int i = 1; i <= 4; ++i ) { LzmaProps[i] = ds & 0xFF; ds >>= 8; }
  return (DicSize >= min_dictionary_size && DicSize <= max_dictionary_size);
}

STDMETHODIMP CHandler::Open(IInStream *inStream, const UInt64 *, IArchiveOpenCallback *)
{
  Close();

  RINOK(ReadStream_FALSE(inStream, _header.data, CHeader::size));

  if (!_header.Parse())
    return S_FALSE;

  RINOK(inStream->Seek(0, STREAM_SEEK_END, &_packSize));
  if (_packSize < 36)
    return S_FALSE;
  _isArc = true;
  _stream = inStream;
  _seqStream = inStream;
  _needSeekToStart = true;
  return S_OK;
}

STDMETHODIMP CHandler::OpenSeq(ISequentialInStream *stream)
{
  Close();
  _isArc = true;
  _seqStream = stream;
  return S_OK;
}

STDMETHODIMP CHandler::Close()
{
  _isArc = false;
  _packSize_Defined = false;
  _unpackSize_Defined = false;
  _numStreams_Defined = false;

  _dataAfterEnd = false;
  _needMoreInput = false;
  _dataError = false;

  _packSize = 0;

  _needSeekToStart = false;

  _stream.Release();
  _seqStream.Release();
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

  COutStreamWithCRC *outStreamSpec = new COutStreamWithCRC;
  CMyComPtr<ISequentialOutStream> outStream(outStreamSpec);
  outStreamSpec->SetStream(realOutStream);
  outStreamSpec->Init();
  realOutStream.Release();

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, true);

  if (_needSeekToStart)
  {
    if (!_stream)
      return E_FAIL;
    RINOK(_stream->Seek(0, STREAM_SEEK_SET, NULL));
  }
  else
    _needSeekToStart = true;

  CDecoder decoder;
  HRESULT result = decoder.Create(_seqStream);
  RINOK(result);

  bool firstItem = true;

  UInt64 packSize = 0;
  UInt64 unpackSize = 0;
  UInt64 numStreams = 0;

  bool crcError = false;
  bool dataAfterEnd = false;

  for (;;)
  {
    lps->InSize = packSize;
    lps->OutSize = unpackSize;
    RINOK(lps->SetCur());

    CHeader st;
    UInt32 processed;
    RINOK(decoder.ReadInput(st.data, CHeader::size, &processed));
    if (processed != CHeader::size)
    {
      if (processed != 0)
        dataAfterEnd = true;
      break;
    }

    if (!st.Parse())
    {
      dataAfterEnd = true;
      break;
    }
    numStreams++;
    firstItem = false;
    outStreamSpec->InitCRC();

    result = decoder.Code(st, outStream, progress);

    UInt64 member_size = decoder.GetInputProcessedSize() - packSize;
    packSize += member_size;
    UInt64 data_size = outStreamSpec->GetSize() - unpackSize;
    unpackSize += data_size;

    if (result == S_OK)
    {
      CTrailer trailer;
      RINOK(decoder.ReadInput(trailer.data, CTrailer::size, &processed));
      packSize += processed; member_size += processed;
      if (processed != CTrailer::size ||
          trailer.data_crc() != outStreamSpec->GetCRC() ||
          trailer.data_size() != data_size ||
          trailer.member_size() != member_size)
      {
        crcError = true;
        result = S_FALSE;
        break;
      }
    }
    if (result == S_FALSE)
      break;
    RINOK(result);
  }

  if (firstItem)
  {
    _isArc = false;
    result = S_FALSE;
  }
  else if (result == S_OK || result == S_FALSE)
  {
    if (dataAfterEnd)
      _dataAfterEnd = true;
    else if (decoder._lzmaDecoderSpec->NeedsMoreInput())
      _needMoreInput = true;

    _packSize = packSize;
    _unpackSize = unpackSize;
    _numStreams = numStreams;

    _packSize_Defined = true;
    _unpackSize_Defined = true;
    _numStreams_Defined = true;
  }

  Int32 opResult = NExtract::NOperationResult::kOK;

  if (!_isArc)
    opResult = NExtract::NOperationResult::kIsNotArc;
  else if (_needMoreInput)
    opResult = NExtract::NOperationResult::kUnexpectedEnd;
  else if (crcError)
    opResult = NExtract::NOperationResult::kCRCError;
  else if (_dataAfterEnd)
    opResult = NExtract::NOperationResult::kDataAfterEnd;
  else if (result == S_FALSE)
    opResult = NExtract::NOperationResult::kDataError;
  else if (result == S_OK)
    opResult = NExtract::NOperationResult::kOK;
  else
    return result;

  outStream.Release();
  return extractCallback->SetOperationResult(opResult);

  COM_TRY_END
}

REGISTER_ARC_I(
  "lzip", "lz tlz", "* .tar", 0xC4,
  k_Signature,
  0,
  NArcInfoFlags::kKeepName,
  IsArc_Lz)

}}

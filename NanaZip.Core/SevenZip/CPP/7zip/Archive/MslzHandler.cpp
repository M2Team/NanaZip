// MslzHandler.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"
#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/MyString.h"

#include "../../Windows/PropVariant.h"

#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

namespace NArchive {
namespace NMslz {

// windows LZ expand functions have limit of (1u << 31) for maximum supported size
static const UInt32 kUnpackSizeMax = 1u << 31;

Z7_CLASS_IMP_CHandler_IInArchive_1(
  IArchiveOpenSeq
)
  bool _isArc;
  bool _needSeekToStart;
  bool _dataAfterEnd;
  bool _needMoreInput;

  CMyComPtr<IInStream> _inStream;
  CMyComPtr<ISequentialInStream> _seqStream;

  bool _packSize_Defined;
  bool _unpackSize_Defined;

  UInt32 _unpackSize;
  UInt64 _packSize;
  UInt64 _originalFileSize;
  UString _name;

  void ParseName(Byte replaceByte, IArchiveOpenCallback *callback);
};

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
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
  // COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    // case kpidExtension: prop = "mslz"; break;
    case kpidIsNotArcType: prop = true; break;
    case kpidPhySize: if (_packSize_Defined) prop = _packSize; break;
    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc) v |= kpv_ErrorFlags_IsNotArc;
      if (_needMoreInput) v |= kpv_ErrorFlags_UnexpectedEnd;
      if (_dataAfterEnd) v |= kpv_ErrorFlags_DataAfterEnd;
      if (v)
        prop = v;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
  // COM_TRY_END
}


Z7_COM7F_IMF(CHandler::GetProperty(UInt32 /* index */, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPath: if (!_name.IsEmpty()) prop = _name; break;
    case kpidSize: if (_unpackSize_Defined) prop = _unpackSize; break;
    case kpidPackSize: if (_packSize_Defined || _inStream) prop = _packSize; break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

static const unsigned kSignatureSize = 9;
static const unsigned kHeaderSize = kSignatureSize + 1 + 4;
static const Byte kSignature[kSignatureSize] =
  { 0x53, 0x5A, 0x44, 0x44, 0x88, 0xF0, 0x27, 0x33, 0x41 };

// we support only 3 chars strings here
static const char * const g_Exts =
  "bin"
  "dll"
  "dsp"
  "eng"
  "exe"
  "hlp"
  // "inf" / "ini" collision
  "kmd"
  "msg"
  "ocx"
  "olb"
  "pdf"
  "sys"
  "txt"
  "vxd"
  "\0\0";

void CHandler::ParseName(Byte replaceByte, IArchiveOpenCallback *callback)
{
  if (!callback)
    return;
  Z7_DECL_CMyComPtr_QI_FROM(IArchiveOpenVolumeCallback, volumeCallback, callback)
  if (!volumeCallback)
    return;
  NWindows::NCOM::CPropVariant prop;
  if (volumeCallback->GetProperty(kpidName, &prop) != S_OK || prop.vt != VT_BSTR)
    return;
  _name = prop.bstrVal;
  if (_name.IsEmpty() || _name.Back() != L'_')
  {
    _name.Empty();
    return;
  }
  _name.DeleteBack();
   
  if (replaceByte == 0)
  {
    const wchar_t * const ptr = _name.Ptr(_name.Len());
    if (_name.Len() < 3 || ptr[-3] != '.')
      return;
    const wchar_t low_0 = (wchar_t)(ptr[-2] | 0x20);
    const wchar_t low_1 = (wchar_t)(ptr[-1] | 0x20);
    const char *ext = g_Exts;
    for (;; ext += 3)
    {
      if (ext[0] == 0)
        break;
      if (low_0 == (Byte)ext[0] && low_1 == (Byte)ext[1])
      {
        // we use lower/upper case of previous character:
        replaceByte = (Byte)(ext[2] - (low_1 - ptr[-1]));
        break;
      }
    }
  }

  if (replaceByte)
  {
    if (replaceByte <= 0x20 || replaceByte >= 0x80
        || replaceByte == '/'
        || replaceByte == '\\'
        || replaceByte == ':'
        || replaceByte == '*'
        || replaceByte == '?'
        || replaceByte == '<'
        || replaceByte == '>'
        || replaceByte == '|'
        || replaceByte == '"'
        || replaceByte == '.'
        // || replaceByte == '_'
        )
    {
      // windows expand uses CP_ACP code page for >= 0x80.
      // const char temp[2] = { (char)replaceByte, 0 };
      // _name += MultiByteToUnicodeString(temp);
      // We use replacement scheme:
      _name.Add_Char('_');
      _name.Add_UInt32(replaceByte);
    }
    else
      _name.Add_Char((char)replaceByte);
  }
}


Z7_COM7F_IMF(CHandler::Open(IInStream *stream, const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback *callback))
{
  COM_TRY_BEGIN
  {
    Close();
    _needSeekToStart = true;
    Byte buffer[kHeaderSize];
    RINOK(ReadStream_FALSE(stream, buffer, kHeaderSize))
    if (memcmp(buffer, kSignature, kSignatureSize))
      return S_FALSE;
    _unpackSize = GetUi32(buffer + 10);
    if (_unpackSize > kUnpackSizeMax)
      return S_FALSE;
    RINOK(InStream_GetSize_SeekToEnd(stream, _originalFileSize))
    _packSize = _originalFileSize;
    ParseName(buffer[kSignatureSize], callback);
    _isArc = true;
    _unpackSize_Defined = true;
    _inStream = stream;
    _seqStream = stream;
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _unpackSize = 0;
  _packSize = 0;
  _originalFileSize = 0;

  _isArc = false;
  _needSeekToStart = false;
  _dataAfterEnd = false;
  _needMoreInput = false;
  _packSize_Defined = false;
  _unpackSize_Defined = false;

  _name.Empty();
  _seqStream.Release();
  _inStream.Release();
  return S_OK;
}


class CAlignedBuffer_NoSize_NoExceptions
{
  Byte *_data;
  Z7_CLASS_NO_COPY(CAlignedBuffer_NoSize_NoExceptions)
public:
  CAlignedBuffer_NoSize_NoExceptions(): _data(NULL) {}
  ~CAlignedBuffer_NoSize_NoExceptions() { z7_AlignedFree(_data); }
  bool IsAllocated() const { return _data != NULL; }
  operator       Byte *()       { return _data; }
  operator const Byte *() const { return _data; }
  void Alloc_after_Constructor(size_t size)
  {
   _data = (Byte *)z7_AlignedAlloc(size);
  }
};


struct CMslzDecoder
{
  bool _isArc;
  bool _needMoreData;
  UInt32 _unpackSize;
  UInt64 _inProcessed;
  ISequentialInStream *inStream;
  ISequentialOutStream *outStream;
  CMyComPtr2_Create<ICompressProgressInfo, CLocalProgress> lps;

  HRESULT Decode(IArchiveExtractCallback *extractCallback);

  CMslzDecoder():
      _isArc(false),
      _needMoreData(false),
      _unpackSize(0),
      _inProcessed(0)
      {}
};


#define CHECK_src \
  { if (src == src_end) { _needMoreData = true; goto data_error; }}


/*
Mslz is modified LZSS algorithm of Haruhiko Okumura
return:
   S_OK : OK
   S_FALSE : error in input stream data
     _isArc = false : Header error : bad signature or unsupported (unpackSize)
     _isArc = true  : Header is OK
        _needMoreData == true  : unexpected end of input stream
        _needMoreData == false : Data error:
            : output data exceeds (unpackSize)
            : non-zero unused bits in last byte of compressed stream
   another HRESULT error codes from Read / Write / Progress / Memory Allocation
*/
HRESULT CMslzDecoder::Decode(IArchiveExtractCallback * const extractCallback)
{
  const unsigned k_WinSize = 1 << 12;
  const unsigned k_InAlign_Size = 1 << 6; // ( >= 16) is required
  const unsigned k_InBuf_Size = 1 << 15;  // ( > 2 * k_InAlign_Size) is required
  const unsigned k_ChunkSize = 1 << 16;   // ( > 18 * 8 + 8) is required
  const unsigned k_Extra = 256;           // ( > 18 * 8 + 8) is required
  const unsigned k_OutBuf_Size = k_WinSize + k_ChunkSize + k_Extra;
  
  CAlignedBuffer_NoSize_NoExceptions buffer;
  buffer.Alloc_after_Constructor(k_InBuf_Size + k_OutBuf_Size);
  if (!buffer.IsAllocated())
    return E_OUTOFMEMORY;
  
  Byte * const inBuf = buffer; // + k_OutBuf_Size;
  Byte *src_end;
  {
    size_t num = k_InBuf_Size;
    RINOK(ReadStream(inStream, inBuf, &num))
    if (num < kHeaderSize)
      return S_FALSE;
    src_end = inBuf + num;
  }
  if (memcmp(inBuf, kSignature, kSignatureSize))
    return S_FALSE;
  UInt32 unpackSize = GetUi32(inBuf + 10);
  if (unpackSize > kUnpackSizeMax)
    return S_FALSE;
  _isArc = true;
  _unpackSize = unpackSize;
  RINOK(extractCallback->SetTotal(unpackSize))
  const unsigned kOffset = 16; // 16 for MS code, 18 in Okumura's code.
  Byte * const outBuf = buffer + k_InBuf_Size;
  memset(outBuf, ' ', k_WinSize);
  memset(outBuf, 0, kOffset);
  /*
    The original expand and another LZSS decoders likely contain bug,
    because they do not initialize 16 bytes of output array.
    If global array is used, that 16-bytes block is initialized with zeros.
    If they decompress just one file, then uninitialized data is
    decompressed as zeros. But if they decompress more than one file,
    it could extract some incorrect data.
    Note: LZSS encoders probably don't use these starting uninitialized 16 bytes of zeros.
    v26.06: we initialize that data with 16 bytes of zeros.
  */
  HRESULT res = S_OK;
  const unsigned kStartOffset = k_WinSize - kOffset;
  // (kUnpackSizeMax < (1 << 32) - kStartOffset) is required
  UInt32 unpackPos = kStartOffset;
  unpackSize += kStartOffset;
  Byte *src = inBuf + kHeaderSize;
  Byte *outPtr = outBuf + k_WinSize;
  unsigned mask = 0;

  while (unpackPos < unpackSize)
  {
    if (mask <= 1)
    {
      if (outPtr >= outBuf + k_WinSize + k_ChunkSize)
      {
        outPtr -= k_ChunkSize;
        const UInt32 unpackPos2 = unpackPos - kStartOffset;
        if ((unpackPos2 & ((1u << 24) - k_ChunkSize)) == 0)
        {
          const UInt64 outSize = unpackPos2;
          RINOK(lps.Interface()->SetRatioInfo(&_inProcessed, &outSize))
        }
        if (outStream)
        {
          RINOK(WriteStream(outStream, outBuf + k_WinSize, k_ChunkSize))
          memmove(outBuf, outBuf + k_ChunkSize, (size_t)(outPtr - outBuf));
        }
      }
      const size_t rem = (size_t)(src_end - src);
      if (rem < 1 + 8 * 2 // one control BYTE and 8 2-byte LZ_MATCH codes
          && inBuf + k_InBuf_Size == src_end)
      {
        _inProcessed += k_InBuf_Size - k_InAlign_Size;
        // memcpy(inBuf + k_InAlign_Size - 16, inBuf + k_InBuf_Size - 16, 16);
        memcpy(inBuf + k_InAlign_Size - rem, src, rem);
        size_t num = k_InBuf_Size - k_InAlign_Size;
        res = ReadStream(inStream, inBuf + k_InAlign_Size, &num);
        src_end = inBuf + k_InAlign_Size + num;
        src -= k_InBuf_Size - k_InAlign_Size;
        if (res != S_OK)
          goto end;
      }
      CHECK_src
      mask = (unsigned)(*src++) | 0x100;
    }
    {
      CHECK_src
      const unsigned b = *src++;
  
      if (mask & 1)
      {
        mask >>= 1;
        *outPtr++ = (Byte)b;
        unpackPos++;
      }
      else
      {
        mask >>= 1;
        CHECK_src
        const unsigned b1 = *src++;
        const unsigned len = (b1 & 0xf) + 3;
        /* real ms files are compressed using length limit (len <= 16),
           but orignal expand decoder ignores that limit and allows any length.
           We can use (len) check, if we want additional error checking.
        */
        // if (len > 16) goto data_error; // optional check following ms encoder scheme.
        const unsigned srcIndex = ((b1 & 0xf0) << 4) | b;
        Byte *dest = outPtr;
        Byte *from = dest - k_WinSize + ((srcIndex - unpackPos) & (k_WinSize - 1));
        unpackPos += len;
        if (unpackPos > unpackSize)
          goto data_error;
        outPtr += len;
        do
        {
          dest[0] = from[0];
          dest[1] = from[1];
          dest[2] = from[2];
          dest[3] = from[3];
          dest[4] = from[4];
          dest[5] = from[5];
          dest[6] = from[6];
          dest[7] = from[7];
          from += 8;
          dest += 8;
        }
        while (dest < outPtr);
      }
    }
  }
  
  if ((mask & (mask - 1)) == 0)
    goto end;

data_error:
  res = S_FALSE;
  
end:
  _inProcessed += (size_t)(src - inBuf);
  if (outStream)
  {
    const HRESULT res2 = WriteStream(outStream, outBuf + k_WinSize,
        (size_t)(outPtr - outBuf - k_WinSize));
    if (res == S_OK)
      res = res2;
  }
  if (res == S_OK)
  {
    const UInt64 outSize = unpackPos - kStartOffset;
    res = lps.Interface()->SetRatioInfo(&_inProcessed, &outSize);
  }
  return res;
}


Z7_COM7F_IMF(CHandler::OpenSeq(ISequentialInStream *stream))
{
  COM_TRY_BEGIN
  Close();
  _isArc = true;
  _seqStream = stream;
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Extract(const UInt32 * const indices, const UInt32 numItems,
    const Int32 testMode, IArchiveExtractCallback * const extractCallback))
{
  COM_TRY_BEGIN
  if (numItems == 0)
    return S_OK;
  if (numItems != (UInt32)(Int32)-1 && (numItems != 1 || indices[0] != 0))
    return E_INVALIDARG;

  Int32 opRes;
  {
    CMyComPtr<ISequentialOutStream> realOutStream;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    RINOK(extractCallback->GetStream(0, &realOutStream, askMode))
    if (!testMode && !realOutStream)
      return S_OK;
      
    RINOK(extractCallback->PrepareOperation(askMode))
        
    if (_needSeekToStart)
    {
      if (!_inStream)
        return E_FAIL;
      RINOK(InStream_SeekToBegin(_inStream))
    }
    _needSeekToStart = true;
    
    CMslzDecoder decoder;
    {
      decoder.inStream = _seqStream;
      decoder.outStream = realOutStream;
      decoder.lps->Init(extractCallback, false);
      const HRESULT hres = decoder.Decode(extractCallback);
      if (hres == S_OK)
      {
        opRes = NExtract::NOperationResult::kOK;
        _dataAfterEnd = false;
        if (_inStream)
        {
          if (decoder._inProcessed < _originalFileSize)
          {
            _dataAfterEnd = true;
            opRes = NExtract::NOperationResult::kDataAfterEnd;
          }
          if (decoder._inProcessed > _originalFileSize)
            opRes = NExtract::NOperationResult::kDataError; // unexpected or internal error
        }
        _packSize = decoder._inProcessed;
        _packSize_Defined = true;
      }
      else
      {
        if (hres != S_FALSE)
          return hres;
        _packSize_Defined = false;
        _dataAfterEnd = false;
        opRes = NExtract::NOperationResult::kDataError;
      }
    }
    
    _needMoreInput = decoder._needMoreData;
    if (_needMoreInput)
      opRes = NExtract::NOperationResult::kUnexpectedEnd;
    _isArc = decoder._isArc;
    if (_isArc)
    {
      _unpackSize = decoder._unpackSize;
      _unpackSize_Defined = true;
    }
    else
      opRes = NExtract::NOperationResult::kIsNotArc;
  }
  return extractCallback->SetOperationResult(opRes);
  COM_TRY_END
}

REGISTER_ARC_I(
  "MsLZ", "mslz", NULL, 0xD5,
  kSignature,
  0,
  0,
  NULL)

}}

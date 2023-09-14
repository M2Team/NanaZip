// MslzHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/MyString.h"

#include "../../Windows/PropVariant.h"

#include "../Common/InBuffer.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "Common/DummyOutStream.h"

namespace NArchive {
namespace NMslz {

static const UInt32 kUnpackSizeMax = 0xFFFFFFE0;

Z7_CLASS_IMP_CHandler_IInArchive_1(
  IArchiveOpenSeq
)
  CMyComPtr<IInStream> _inStream;
  CMyComPtr<ISequentialInStream> _seqStream;

  bool _isArc;
  bool _needSeekToStart;
  bool _dataAfterEnd;
  bool _needMoreInput;

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
  kpidPackSize,
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
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidExtension: prop = "mslz"; break;
    case kpidIsNotArcType: prop = true; break;
    case kpidPhySize: if (_packSize_Defined) prop = _packSize; break;
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
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::GetProperty(UInt32 /* index */, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPath: if (!_name.IsEmpty()) prop = _name; break;
    case kpidSize: if (_unpackSize_Defined || _inStream) prop = _unpackSize; break;
    case kpidPackSize: if (_packSize_Defined || _inStream) prop = _packSize; break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

static const unsigned kSignatureSize = 9;
static const unsigned kHeaderSize = kSignatureSize + 1 + 4;
#define MSLZ_SIGNATURE { 0x53, 0x5A, 0x44, 0x44, 0x88, 0xF0, 0x27, 0x33, 0x41 }
// old signature: 53 5A 20 88 F0 27 33
static const Byte kSignature[kSignatureSize] = MSLZ_SIGNATURE;

// we support only 3 chars strings here
static const char * const g_Exts[] =
{
    "bin"
  , "dll"
  , "exe"
  , "kmd"
  , "pdf"
  , "sys"
};

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

  UString s = prop.bstrVal;
  if (s.IsEmpty() ||
      s.Back() != L'_')
    return;
  
  s.DeleteBack();
  _name = s;
   
  if (replaceByte == 0)
  {
    if (s.Len() < 3 || s[s.Len() - 3] != '.')
      return;
    for (unsigned i = 0; i < Z7_ARRAY_SIZE(g_Exts); i++)
    {
      const char *ext = g_Exts[i];
      if (s[s.Len() - 2] == (Byte)ext[0] &&
          s[s.Len() - 1] == (Byte)ext[1])
      {
        replaceByte = (Byte)ext[2];
        break;
      }
    }
  }
  
  if (replaceByte >= 0x20 && replaceByte < 0x80)
    _name += (char)replaceByte;
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
    if (memcmp(buffer, kSignature, kSignatureSize) != 0)
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
  _originalFileSize = 0;
  _packSize = 0;
  _unpackSize = 0;

  _isArc = false;
  _needSeekToStart = false;
  _dataAfterEnd = false;
  _needMoreInput = false;

  _packSize_Defined = false;
  _unpackSize_Defined =  false;

  _seqStream.Release();
  _inStream.Release();
  _name.Empty();
  return S_OK;
}

// MslzDec is modified LZSS algorithm of Haruhiko Okumura:
//   maxLen = 18; Okumura
//   maxLen = 16; MS

#define PROGRESS_AND_WRITE \
  if ((dest & kMask) == 0) { if (outStream) RINOK(WriteStream(outStream, buf, kBufSize)); \
    if ((dest & ((1 << 20) - 1)) == 0) \
    if (progress) \
      { UInt64 inSize = inStream.GetProcessedSize(); UInt64 outSize = dest; \
        RINOK(progress->SetRatioInfo(&inSize, &outSize)); }}

static HRESULT MslzDec(CInBuffer &inStream, ISequentialOutStream *outStream, UInt32 unpackSize, bool &needMoreData, ICompressProgressInfo *progress)
{
  const unsigned kBufSize = (1 << 12);
  const unsigned kMask = kBufSize - 1;
  Byte buf[kBufSize];
  UInt32 dest = 0;
  memset(buf, ' ', kBufSize);
  
  while (dest < unpackSize)
  {
    Byte b;
    if (!inStream.ReadByte(b))
    {
      needMoreData = true;
      return S_FALSE;
    }
    
    for (unsigned mask = (unsigned)b | 0x100; mask > 1 && dest < unpackSize; mask >>= 1)
    {
      if (!inStream.ReadByte(b))
      {
        needMoreData = true;
        return S_FALSE;
      }
  
      if (mask & 1)
      {
        buf[dest++ & kMask] = b;
        PROGRESS_AND_WRITE
      }
      else
      {
        Byte b1;
        if (!inStream.ReadByte(b1))
        {
          needMoreData = true;
          return S_FALSE;
        }
        const unsigned kMaxLen = 16; // 18 in Okumura's code.
        unsigned src = (((((unsigned)b1 & 0xF0) << 4) | b) + kMaxLen) & kMask;
        unsigned len = (b1 & 0xF) + 3;
        if (len > kMaxLen || dest + len > unpackSize)
          return S_FALSE;
    
        do
        {
          buf[dest++ & kMask] = buf[src++ & kMask];
          PROGRESS_AND_WRITE
        }
        while (--len != 0);
      }
    }
  }
  
  if (outStream)
    RINOK(WriteStream(outStream, buf, dest & kMask))
  return S_OK;
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

Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  if (numItems == 0)
    return S_OK;
  if (numItems != (UInt32)(Int32)-1 && (numItems != 1 || indices[0] != 0))
    return E_INVALIDARG;

  // extractCallback->SetTotal(_unpackSize);

  CMyComPtr<ISequentialOutStream> realOutStream;
  const Int32 askMode = testMode ?
      NExtract::NAskMode::kTest :
      NExtract::NAskMode::kExtract;
  RINOK(extractCallback->GetStream(0, &realOutStream, askMode))
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

  if (_needSeekToStart)
  {
    if (!_inStream)
      return E_FAIL;
    RINOK(InStream_SeekToBegin(_inStream))
  }
  else
    _needSeekToStart = true;

  Int32 opRes = NExtract::NOperationResult::kDataError;

  bool isArc = false;
  bool needMoreInput = false;
  try
  {
    CInBuffer s;
    if (!s.Create(1 << 20))
      return E_OUTOFMEMORY;
    s.SetStream(_seqStream);
    s.Init();
    
    Byte buffer[kHeaderSize];
    if (s.ReadBytes(buffer, kHeaderSize) == kHeaderSize)
    {
      UInt32 unpackSize;
      if (memcmp(buffer, kSignature, kSignatureSize) == 0)
      {
        unpackSize = GetUi32(buffer + 10);
        if (unpackSize <= kUnpackSizeMax)
        {
          HRESULT result = MslzDec(s, outStream, unpackSize, needMoreInput, progress);
          if (result == S_OK)
            opRes = NExtract::NOperationResult::kOK;
          else if (result != S_FALSE)
            return result;
          _unpackSize = unpackSize;
          _unpackSize_Defined = true;

          _packSize = s.GetProcessedSize();
          _packSize_Defined = true;

          if (_inStream && _packSize < _originalFileSize)
            _dataAfterEnd = true;
          
          isArc = true;
        }
      }
    }
  }
  catch (CInBufferException &e) { return e.ErrorCode; }

  _isArc = isArc;
  if (isArc)
    _needMoreInput = needMoreInput;
  if (!_isArc)
    opRes = NExtract::NOperationResult::kIsNotArc;
  else if (_needMoreInput)
    opRes = NExtract::NOperationResult::kUnexpectedEnd;
  else if (_dataAfterEnd)
    opRes = NExtract::NOperationResult::kDataAfterEnd;

  outStream.Release();
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

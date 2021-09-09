// Base64Handler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/MyBuffer.h"
#include "../../Common/IntToString.h"
#include "../../Common/MyVector.h"

#include "../../Windows/PropVariant.h"

#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"
#include "../Common/InBuffer.h"

/*
spaces:
  9(TAB),10(LF),13(CR),32(SPACE)
  Non-breaking space:
    0xa0 : Unicode, Windows code pages 1250-1258
    0xff (unused): DOS code pages

end of stream markers: '=' (0x3d):
  "="  , if numBytes (% 3 == 2)
  "==" , if numBytes (% 3 == 1)
*/


static const Byte k_Base64Table[256] =
{
  66,77,77,77,77,77,77,77,77,65,65,77,77,65,77,77,
  77,77,77,77,77,77,77,77,77,77,77,77,77,77,77,77,
  65,77,77,77,77,77,77,77,77,77,77,62,77,77,77,63,
  52,53,54,55,56,57,58,59,60,61,77,77,77,64,77,77,
  77, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
  15,16,17,18,19,20,21,22,23,24,25,77,77,77,77,77,
  77,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
  41,42,43,44,45,46,47,48,49,50,51,77,77,77,77,77,
  77,77,77,77,77,77,77,77,77,77,77,77,77,77,77,77,
  77,77,77,77,77,77,77,77,77,77,77,77,77,77,77,77,
  65,77,77,77,77,77,77,77,77,77,77,77,77,77,77,77,
  77,77,77,77,77,77,77,77,77,77,77,77,77,77,77,77,
  77,77,77,77,77,77,77,77,77,77,77,77,77,77,77,77,
  77,77,77,77,77,77,77,77,77,77,77,77,77,77,77,77,
  77,77,77,77,77,77,77,77,77,77,77,77,77,77,77,77,
  77,77,77,77,77,77,77,77,77,77,77,77,77,77,77,77
};

static const unsigned k_Code_Equals = 64;
static const unsigned k_Code_Space = 65;
static const unsigned k_Code_Zero = 66;

API_FUNC_static_IsArc IsArc_Base64(const Byte *p, size_t size)
{
  size_t num = 0;
  size_t firstSpace = 0;

  for (;;)
  {
    if (size == 0)
      return k_IsArc_Res_NEED_MORE;
    UInt32 c = k_Base64Table[(Byte)(*p++)];
    size--;
    if (c < 64)
    {
      num++;
      continue;
    }

    if (c == k_Code_Space)
    {
      if (p[-1] == ' ' && firstSpace == 0)
        firstSpace = num;
      continue;
    }

    if (c != k_Code_Equals)
      return k_IsArc_Res_NO;
    break;
  }

  {
    // we try to redece false positive detection here.
    // we don't expect space character in starting base64 line
    const unsigned kNumExpectedNonSpaceSyms = 20;
    if (firstSpace != 0 && firstSpace < num && firstSpace < kNumExpectedNonSpaceSyms)
      return k_IsArc_Res_NO;
  }

  num &= 3;

  if (num <= 1)
    return k_IsArc_Res_NO;
  if (num != 3)
  {
    if (size == 0)
      return k_IsArc_Res_NEED_MORE;
    UInt32 c = k_Base64Table[(Byte)(*p++)];
    size--;
    if (c != k_Code_Equals)
      return k_IsArc_Res_NO;
  }

  for (;;)
  {
    if (size == 0)
      return k_IsArc_Res_YES;
    UInt32 c = k_Base64Table[(Byte)(*p++)];
    size--;
    if (c == k_Code_Space)
      continue;
    return k_IsArc_Res_NO;
  }
}
}


enum EBase64Res
{
  k_Base64_RES_MaybeFinished,
  k_Base64_RES_Finished,
  k_Base64_RES_NeedMoreInput,
  k_Base64_RES_UnexpectedChar
};


static EBase64Res Base64ToBin(Byte *p, size_t size, const Byte **srcEnd, Byte **destEnd)
{
  Byte *dest = p;
  UInt32 val = 1;
  EBase64Res res = k_Base64_RES_NeedMoreInput;
  
  for (;;)
  {
    if (size == 0)
    {
      if (val == 1)
        res = k_Base64_RES_MaybeFinished;
      break;
    }
    UInt32 c = k_Base64Table[(Byte)(*p++)];
    size--;
    if (c < 64)
    {
      val = (val << 6) | c;
      if ((val & ((UInt32)1 << 24)) == 0)
        continue;
      dest[0] = (Byte)(val >> 16);
      dest[1] = (Byte)(val >> 8);
      dest[2] = (Byte)(val);
      dest += 3;
      val = 1;
      continue;
    }

    if (c == k_Code_Space)
      continue;
    
    if (c == k_Code_Equals)
    {
      if (val >= (1 << 12))
      {
        if (val & (1 << 18))
        {
          res = k_Base64_RES_Finished;
          break;
        }
        if (size == 0)
          break;
        c = k_Base64Table[(Byte)(*p++)];
        size--;
        if (c == k_Code_Equals)
        {
          res = k_Base64_RES_Finished;
          break;
        }
      }
    }

    p--;
    res = k_Base64_RES_UnexpectedChar;
    break;
  }

  if (val >= ((UInt32)1 << 12))
  {
    if (val & (1 << 18))
    {
      *dest++ = (Byte)(val >> 10);
      val <<= 2;
    }
    *dest++ = (Byte)(val >> 4);
  }
  
  *srcEnd = p;
  *destEnd = dest;
  return res;
}


static const Byte *Base64_SkipSpaces(const Byte *p, size_t size)
{
  for (;;)
  {
    if (size == 0)
      return p;
    UInt32 c = k_Base64Table[(Byte)(*p++)];
    size--;
    if (c == k_Code_Space)
      continue;
    return p - 1;
  }
}


// the following function is used by DmgHandler.cpp

Byte *Base64ToBin(Byte *dest, const char *src);
Byte *Base64ToBin(Byte *dest, const char *src)
{
  UInt32 val = 1;
  
  for (;;)
  {
    UInt32 c = k_Base64Table[(Byte)(*src++)];

    if (c < 64)
    {
      val = (val << 6) | c;
      if ((val & ((UInt32)1 << 24)) == 0)
        continue;
      dest[0] = (Byte)(val >> 16);
      dest[1] = (Byte)(val >> 8);
      dest[2] = (Byte)(val);
      dest += 3;
      val = 1;
      continue;
    }
    
    if (c == k_Code_Space)
      continue;
    
    if (c == k_Code_Equals)
      break;
    
    if (c == k_Code_Zero && val == 1) // end of string
      return dest;
    
    return NULL;
  }

  if (val < (1 << 12))
    return NULL;

  if (val & (1 << 18))
  {
    *dest++ = (Byte)(val >> 10);
    val <<= 2;
  }
  else if (k_Base64Table[(Byte)(*src++)] != k_Code_Equals)
    return NULL;
  *dest++ = (Byte)(val >> 4);

  for (;;)
  {
    Byte c = k_Base64Table[(Byte)(*src++)];
    if (c == k_Code_Space)
      continue;
    if (c == k_Code_Zero)
      return dest;
    return NULL;
  }
}


namespace NArchive {
namespace NBase64 {

class CHandler:
  public IInArchive,
  public CMyUnknownImp
{
  bool _isArc;
  UInt64 _phySize;
  size_t _size;
  EBase64Res _sres;
  CByteBuffer _data;
public:
  MY_UNKNOWN_IMP1(IInArchive)
  INTERFACE_IInArchive(;)
};

static const Byte kProps[] =
{
  kpidSize,
  kpidPackSize,
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps_NO_Table

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = 1;
  return S_OK;
}

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPhySize: if (_phySize != 0) prop = _phySize; break;
    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc) v |= kpv_ErrorFlags_IsNotArc;
      if (_sres == k_Base64_RES_NeedMoreInput) v |= kpv_ErrorFlags_UnexpectedEnd;
      if (v != 0)
        prop = v;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
}

STDMETHODIMP CHandler::GetProperty(UInt32 /* index */, PROPID propID, PROPVARIANT *value)
{
  // COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidSize: prop = (UInt64)_size; break;
    case kpidPackSize: prop = _phySize; break;
  }
  prop.Detach(value);
  return S_OK;
  // COM_TRY_END
}


static HRESULT ReadStream_OpenProgress(ISequentialInStream *stream, void *data, size_t size, IArchiveOpenCallback *openCallback) throw()
{
  UInt64 bytes = 0;
  while (size != 0)
  {
    const UInt32 kBlockSize = ((UInt32)1 << 24);
    UInt32 curSize = (size < kBlockSize) ? (UInt32)size : kBlockSize;
    UInt32 processedSizeLoc;
    RINOK(stream->Read(data, curSize, &processedSizeLoc));
    if (processedSizeLoc == 0)
      return E_FAIL;
    data = (void *)((Byte *)data + processedSizeLoc);
    size -= processedSizeLoc;
    bytes += processedSizeLoc;
    const UInt64 files = 1;
    RINOK(openCallback->SetCompleted(&files, &bytes));
  }
  return S_OK;
}


STDMETHODIMP CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *openCallback)
{
  COM_TRY_BEGIN
  {
    Close();
    {
      const unsigned kStartSize = 1 << 12;
      _data.Alloc(kStartSize);
      size_t size = kStartSize;
      RINOK(ReadStream(stream, _data, &size));
      UInt32 isArcRes = IsArc_Base64(_data, size);
      if (isArcRes == k_IsArc_Res_NO)
        return S_FALSE;
    }
    _isArc = true;

    UInt64 packSize64;
    RINOK(stream->Seek(0, STREAM_SEEK_END, &packSize64));

    if (packSize64 == 0)
      return S_FALSE;

    size_t curSize = 1 << 16;
    if (curSize > packSize64)
      curSize = (size_t)packSize64;
    const unsigned kLogStep = 4;

    for (;;)
    {
      RINOK(stream->Seek(0, STREAM_SEEK_SET, NULL));
      
      _data.Alloc(curSize);
      RINOK(ReadStream_OpenProgress(stream, _data, curSize, openCallback));
      
      const Byte *srcEnd;
      Byte *dest;
      _sres = Base64ToBin(_data, curSize, &srcEnd, &dest);
      _size = dest - _data;
      size_t mainSize = srcEnd - _data;
      _phySize = mainSize;
      if (_sres == k_Base64_RES_UnexpectedChar)
        break;
      if (curSize != mainSize)
      {
        const Byte *end2 = Base64_SkipSpaces(srcEnd, curSize - mainSize);
        if ((size_t)(end2 - _data) != curSize)
          break;
        _phySize = curSize;
      }

      if (curSize == packSize64)
        break;
      
      UInt64 curSize64 = packSize64;
      if (curSize < (packSize64 >> kLogStep))
        curSize64 = (UInt64)curSize << kLogStep;
      curSize = (size_t)curSize64;
      if (curSize != curSize64)
        return E_OUTOFMEMORY;
    }
    if (_size == 0)
      return S_FALSE;
    return S_OK;
  }
  COM_TRY_END
}

STDMETHODIMP CHandler::Close()
{
  _phySize = 0;
  _size = 0;
  _isArc = false;
  _sres = k_Base64_RES_MaybeFinished;
  _data.Free();
  return S_OK;
}


STDMETHODIMP CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback)
{
  COM_TRY_BEGIN
  bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = 1;
  if (numItems == 0)
    return S_OK;
  if (numItems != 1 || *indices != 0)
    return E_INVALIDARG;

  RINOK(extractCallback->SetTotal(_size));

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  {
    lps->InSize = lps->OutSize = 0;
    RINOK(lps->SetCur());

    CMyComPtr<ISequentialOutStream> realOutStream;
    Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    
    RINOK(extractCallback->GetStream(0, &realOutStream, askMode));
    
    if (!testMode && !realOutStream)
      return S_OK;

    extractCallback->PrepareOperation(askMode);

    if (realOutStream)
    {
      RINOK(WriteStream(realOutStream, (const Byte *)_data, _size));
      realOutStream.Release();
    }
  
    Int32 opRes = NExtract::NOperationResult::kOK;

    if (_sres != k_Base64_RES_Finished)
    {
      if (_sres == k_Base64_RES_NeedMoreInput)
        opRes = NExtract::NOperationResult::kUnexpectedEnd;
      else if (_sres == k_Base64_RES_UnexpectedChar)
        opRes = NExtract::NOperationResult::kDataError;
    }

    RINOK(extractCallback->SetOperationResult(opRes));
  }
  
  lps->InSize = _phySize;
  lps->OutSize = _size;
  return lps->SetCur();

  COM_TRY_END
}

REGISTER_ARC_I_NO_SIG(
  "Base64", "b64", 0, 0xC5,
  0,
  NArcInfoFlags::kKeepName | NArcInfoFlags::kStartOpen | NArcInfoFlags::kByExtOnlyOpen,
  IsArc_Base64)

}}

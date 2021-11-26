// TarHandler.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/UTFConvert.h"

#include "../../../Windows/TimeUtils.h"

#include "../../Common/LimitedStreams.h"
#include "../../Common/MethodProps.h"
#include "../../Common/ProgressUtils.h"
#include "../../Common/StreamObjects.h"
#include "../../Common/StreamUtils.h"

#include "../Common/ItemNameUtils.h"

#include "TarHandler.h"

using namespace NWindows;

namespace NArchive {
namespace NTar {

// 21.02: we use UTF8 code page by default, even if some files show error
// before 21.02 : CP_OEMCP;
// static const UINT k_DefaultCodePage = CP_UTF8;


static const Byte kProps[] =
{
  kpidPath,
  kpidIsDir,
  kpidSize,
  kpidPackSize,
  kpidMTime,
  kpidPosixAttrib,
  kpidUser,
  kpidGroup,
  kpidSymLink,
  kpidHardLink,
  kpidCharacts
  // kpidLinkType
};

static const Byte kArcProps[] =
{
  kpidHeadersSize,
  kpidCodePage,
  kpidCharacts
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPhySize: if (_phySizeDefined) prop = _phySize; break;
    case kpidHeadersSize: if (_phySizeDefined) prop = _headersSize; break;
    case kpidErrorFlags:
    {
      UInt32 flags = 0;
      if (!_isArc)
        flags |= kpv_ErrorFlags_IsNotArc;
      else switch (_error)
      {
        case k_ErrorType_UnexpectedEnd: flags = kpv_ErrorFlags_UnexpectedEnd; break;
        case k_ErrorType_Corrupted: flags = kpv_ErrorFlags_HeadersError; break;
        // case k_ErrorType_OK: break;
        // case k_ErrorType_Warning: break;
        default: break;
      }
      if (flags != 0)
        prop = flags;
      break;
    }

    case kpidWarningFlags:
    {
      if (_warning)
        prop = kpv_ErrorFlags_HeadersError;
      break;
    }

    case kpidCodePage:
    {
      char sz[16];
      const char *name = NULL;
      switch (_openCodePage)
      {
        case CP_OEMCP: name = "OEM"; break;
        case CP_UTF8: name = "UTF-8"; break;
      }
      if (!name)
      {
        ConvertUInt32ToString(_openCodePage, sz);
        name = sz;
      }
      prop = name;
      break;
    }

    case kpidCharacts:
    {
      AString s = _encodingCharacts.GetCharactsString();
      prop = s;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
}

HRESULT CHandler::ReadItem2(ISequentialInStream *stream, bool &filled, CItemEx &item)
{
  item.HeaderPos = _phySize;
  EErrorType error;
  HRESULT res = ReadItem(stream, filled, item, error);
  if (error == k_ErrorType_Warning)
    _warning = true;
  else if (error != k_ErrorType_OK)
    _error = error;
  RINOK(res);
  if (filled)
  {
    /*
    if (item.IsSparse())
      _isSparse = true;
    */
    if (item.IsPaxExtendedHeader())
      _thereIsPaxExtendedHeader = true;
    if (item.IsThereWarning())
      _warning = true;
  }
  _phySize += item.HeaderSize;
  _headersSize += item.HeaderSize;
  return S_OK;
}


void CEncodingCharacts::Check(const AString &s)
{
  IsAscii = s.IsAscii();
  if (!IsAscii)
  {
    /*
    {
      Oem_Checked = true;
      UString u;
      MultiByteToUnicodeString2(u, s, CP_OEMCP);
      Oem_Ok = (u.Find((wchar_t)0xfffd) <= 0);
    }
    Utf_Checked = true;
    */
    UtfCheck.Check_AString(s);
  }
}


AString CEncodingCharacts::GetCharactsString() const
{
  AString s;
  if (IsAscii)
  {
    s += "ASCII";
  }
  /*
  if (Oem_Checked)
  {
    s.Add_Space_if_NotEmpty();
    s += (Oem_Ok ? "oem-ok" : "oem-error");
  }
  if (Utf_Checked)
  */
  else
  {
    s.Add_Space_if_NotEmpty();
    s += (UtfCheck.IsOK() ? "UTF8" : "UTF8-ERROR"); // "UTF8-error"
    {
      AString s2;
      UtfCheck.PrintStatus(s2);
      s.Add_Space_if_NotEmpty();
      s += s2;
    }
  }
  return s;
}


HRESULT CHandler::Open2(IInStream *stream, IArchiveOpenCallback *callback)
{
  UInt64 endPos = 0;
  {
    RINOK(stream->Seek(0, STREAM_SEEK_END, &endPos));
    RINOK(stream->Seek(0, STREAM_SEEK_SET, NULL));
  }
  
  _phySizeDefined = true;
  
  // bool utf8_OK = true;

  for (;;)
  {
    CItemEx item;
    bool filled;
    RINOK(ReadItem2(stream, filled, item));
    if (!filled)
      break;

    _isArc = true;

    /*
    if (!_forceCodePage)
    {
      if (utf8_OK) utf8_OK = CheckUTF8(item.Name, item.NameCouldBeReduced);
      if (utf8_OK) utf8_OK = CheckUTF8(item.LinkName, item.LinkNameCouldBeReduced);
      if (utf8_OK) utf8_OK = CheckUTF8(item.User);
      if (utf8_OK) utf8_OK = CheckUTF8(item.Group);
    }
    */
 
    item.EncodingCharacts.Check(item.Name);
    _encodingCharacts.Update(item.EncodingCharacts);

    _items.Add(item);

    RINOK(stream->Seek((Int64)item.GetPackSizeAligned(), STREAM_SEEK_CUR, &_phySize));
    if (_phySize > endPos)
    {
      _error = k_ErrorType_UnexpectedEnd;
      break;
    }
    /*
    if (_phySize == endPos)
    {
      _errorMessage = "There are no trailing zero-filled records";
      break;
    }
    */
    if (callback)
    {
      if (_items.Size() == 1)
      {
        RINOK(callback->SetTotal(NULL, &endPos));
      }
      if ((_items.Size() & 0x3FF) == 0)
      {
        UInt64 numFiles = _items.Size();
        RINOK(callback->SetCompleted(&numFiles, &_phySize));
      }
    }
  }

  /*
  if (!_forceCodePage)
  {
    if (!utf8_OK)
      _curCodePage = k_DefaultCodePage;
  }
  */
  _openCodePage = _curCodePage;

  if (_items.Size() == 0)
  {
    if (_error != k_ErrorType_OK)
    {
      _isArc = false;
      return S_FALSE;
    }
    CMyComPtr<IArchiveOpenVolumeCallback> openVolumeCallback;
    if (!callback)
      return S_FALSE;
    callback->QueryInterface(IID_IArchiveOpenVolumeCallback, (void **)&openVolumeCallback);
    if (!openVolumeCallback)
      return S_FALSE;
    NCOM::CPropVariant prop;
    if (openVolumeCallback->GetProperty(kpidName, &prop) != S_OK)
      return S_FALSE;
    if (prop.vt != VT_BSTR)
      return S_FALSE;
    unsigned len = MyStringLen(prop.bstrVal);
    if (len < 4 || MyStringCompareNoCase(prop.bstrVal + len - 4, L".tar") != 0)
      return S_FALSE;
  }

  _isArc = true;
  return S_OK;
}

STDMETHODIMP CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *openArchiveCallback)
{
  COM_TRY_BEGIN
  {
    Close();
    RINOK(Open2(stream, openArchiveCallback));
    _stream = stream;
  }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::OpenSeq(ISequentialInStream *stream)
{
  Close();
  _seqStream = stream;
  _isArc = true;
  return S_OK;
}

STDMETHODIMP CHandler::Close()
{
  _isArc = false;
  _warning = false;
  _error = k_ErrorType_OK;

  _phySizeDefined = false;
  _phySize = 0;
  _headersSize = 0;
  _curIndex = 0;
  _latestIsRead = false;
  // _isSparse = false;
  _thereIsPaxExtendedHeader = false;
  _encodingCharacts.Clear();
  _items.Clear();
  _seqStream.Release();
  _stream.Release();
  return S_OK;
}

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = (_stream ? _items.Size() : (UInt32)(Int32)-1);
  return S_OK;
}

CHandler::CHandler()
{
  copyCoderSpec = new NCompress::CCopyCoder();
  copyCoder = copyCoderSpec;
  _openCodePage = CP_UTF8;
  Init();
}

HRESULT CHandler::SkipTo(UInt32 index)
{
  while (_curIndex < index || !_latestIsRead)
  {
    if (_latestIsRead)
    {
      UInt64 packSize = _latestItem.GetPackSizeAligned();
      RINOK(copyCoderSpec->Code(_seqStream, NULL, &packSize, &packSize, NULL));
      _phySize += copyCoderSpec->TotalSize;
      if (copyCoderSpec->TotalSize != packSize)
      {
        _error = k_ErrorType_UnexpectedEnd;
        return S_FALSE;
      }
      _latestIsRead = false;
      _curIndex++;
    }
    else
    {
      bool filled;
      RINOK(ReadItem2(_seqStream, filled, _latestItem));
      if (!filled)
      {
        _phySizeDefined = true;
        return E_INVALIDARG;
      }
      _latestIsRead = true;
    }
  }
  return S_OK;
}

void CHandler::TarStringToUnicode(const AString &s, NWindows::NCOM::CPropVariant &prop, bool toOs) const
{
  UString dest;
  if (_curCodePage == CP_UTF8)
    ConvertUTF8ToUnicode(s, dest);
  else
    MultiByteToUnicodeString2(dest, s, _curCodePage);
  if (toOs)
    NItemName::ReplaceToOsSlashes_Remove_TailSlash(dest,
        true); // useBackslashReplacement
  prop = dest;
}

STDMETHODIMP CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;

  const CItemEx *item;
  if (_stream)
    item = &_items[index];
  else
  {
    if (index < _curIndex)
      return E_INVALIDARG;
    else
    {
      RINOK(SkipTo(index));
      item = &_latestItem;
    }
  }

  switch (propID)
  {
    case kpidPath: TarStringToUnicode(item->Name, prop, true); break;
    case kpidIsDir: prop = item->IsDir(); break;
    case kpidSize: prop = item->GetUnpackSize(); break;
    case kpidPackSize: prop = item->GetPackSizeAligned(); break;
    case kpidMTime:
      if (item->MTime != 0)
      {
        FILETIME ft;
        if (NTime::UnixTime64ToFileTime(item->MTime, ft))
          prop = ft;
      }
      break;
    case kpidPosixAttrib: prop = item->Get_Combined_Mode(); break;
    case kpidUser:  TarStringToUnicode(item->User, prop); break;
    case kpidGroup: TarStringToUnicode(item->Group, prop); break;
    case kpidSymLink:  if (item->LinkFlag == NFileHeader::NLinkFlag::kSymLink  && !item->LinkName.IsEmpty()) TarStringToUnicode(item->LinkName, prop); break;
    case kpidHardLink: if (item->LinkFlag == NFileHeader::NLinkFlag::kHardLink && !item->LinkName.IsEmpty()) TarStringToUnicode(item->LinkName, prop); break;
    // case kpidLinkType: prop = (int)item->LinkFlag; break;
    case kpidCharacts:
    {
      AString s = item->EncodingCharacts.GetCharactsString();
      if (item->IsThereWarning())
      {
        s.Add_Space_if_NotEmpty();
        s += "HEADER_ERROR";
      }
      prop = s;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

HRESULT CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback)
{
  COM_TRY_BEGIN
  ISequentialInStream *stream = _seqStream;
  bool seqMode = (_stream == NULL);
  if (!seqMode)
    stream = _stream;

  bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _items.Size();
  if (_stream && numItems == 0)
    return S_OK;
  UInt64 totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
    totalSize += _items[allFilesMode ? i : indices[i]].GetUnpackSize();
  extractCallback->SetTotal(totalSize);

  UInt64 totalPackSize;
  totalSize = totalPackSize = 0;
  
  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  CLimitedSequentialInStream *streamSpec = new CLimitedSequentialInStream;
  CMyComPtr<ISequentialInStream> inStream(streamSpec);
  streamSpec->SetStream(stream);

  CLimitedSequentialOutStream *outStreamSpec = new CLimitedSequentialOutStream;
  CMyComPtr<ISequentialOutStream> outStream(outStreamSpec);

  for (i = 0; i < numItems || seqMode; i++)
  {
    lps->InSize = totalPackSize;
    lps->OutSize = totalSize;
    RINOK(lps->SetCur());
    CMyComPtr<ISequentialOutStream> realOutStream;
    Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    const UInt32 index = allFilesMode ? i : indices[i];
    const CItemEx *item;
    if (seqMode)
    {
      HRESULT res = SkipTo(index);
      if (res == E_INVALIDARG)
        break;
      RINOK(res);
      item = &_latestItem;
    }
    else
      item = &_items[index];

    RINOK(extractCallback->GetStream(index, &realOutStream, askMode));
    UInt64 unpackSize = item->GetUnpackSize();
    totalSize += unpackSize;
    totalPackSize += item->GetPackSizeAligned();
    if (item->IsDir())
    {
      RINOK(extractCallback->PrepareOperation(askMode));
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK));
      continue;
    }
    bool skipMode = false;
    if (!testMode && !realOutStream)
    {
      if (!seqMode)
      {
        /*
        // probably we must show extracting info it callback handler instead
        if (item->IsHardLink() ||
            item->IsSymLink())
        {
          RINOK(extractCallback->PrepareOperation(askMode));
          RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK));
        }
        */
        continue;
      }
      skipMode = true;
      askMode = NExtract::NAskMode::kSkip;
    }
    RINOK(extractCallback->PrepareOperation(askMode));

    outStreamSpec->SetStream(realOutStream);
    realOutStream.Release();
    outStreamSpec->Init(skipMode ? 0 : unpackSize, true);

    Int32 opRes = NExtract::NOperationResult::kOK;
    CMyComPtr<ISequentialInStream> inStream2;
    if (!item->IsSparse())
      inStream2 = inStream;
    else
    {
      GetStream(index, &inStream2);
      if (!inStream2)
        return E_FAIL;
    }

    {
      if (item->IsSymLink())
      {
        RINOK(WriteStream(outStreamSpec, (const char *)item->LinkName, item->LinkName.Len()));
      }
      else
      {
        if (!seqMode)
        {
          RINOK(_stream->Seek((Int64)item->GetDataPosition(), STREAM_SEEK_SET, NULL));
        }
        streamSpec->Init(item->GetPackSizeAligned());
        RINOK(copyCoder->Code(inStream2, outStream, NULL, NULL, progress));
      }
      if (outStreamSpec->GetRem() != 0)
        opRes = NExtract::NOperationResult::kDataError;
    }
    if (seqMode)
    {
      _latestIsRead = false;
      _curIndex++;
    }
    outStreamSpec->ReleaseStream();
    RINOK(extractCallback->SetOperationResult(opRes));
  }
  return S_OK;
  COM_TRY_END
}

class CSparseStream:
  public IInStream,
  public CMyUnknownImp
{
  UInt64 _phyPos;
  UInt64 _virtPos;
  bool _needStartSeek;

public:
  CHandler *Handler;
  CMyComPtr<IUnknown> HandlerRef;
  unsigned ItemIndex;
  CRecordVector<UInt64> PhyOffsets;

  MY_UNKNOWN_IMP2(ISequentialInStream, IInStream)
  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);

  void Init()
  {
    _virtPos = 0;
    _phyPos = 0;
    _needStartSeek = true;
  }
};


STDMETHODIMP CSparseStream::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  if (processedSize)
    *processedSize = 0;
  if (size == 0)
    return S_OK;
  const CItemEx &item = Handler->_items[ItemIndex];
  if (_virtPos >= item.Size)
    return S_OK;
  {
    UInt64 rem = item.Size - _virtPos;
    if (size > rem)
      size = (UInt32)rem;
  }
  
  HRESULT res = S_OK;

  if (item.SparseBlocks.IsEmpty())
    memset(data, 0, size);
  else
  {
    unsigned left = 0, right = item.SparseBlocks.Size();
    for (;;)
    {
      unsigned mid = (left + right) / 2;
      if (mid == left)
        break;
      if (_virtPos < item.SparseBlocks[mid].Offset)
        right = mid;
      else
        left = mid;
    }
    
    const CSparseBlock &sb = item.SparseBlocks[left];
    UInt64 relat = _virtPos - sb.Offset;
    
    if (_virtPos >= sb.Offset && relat < sb.Size)
    {
      UInt64 rem = sb.Size - relat;
      if (size > rem)
        size = (UInt32)rem;
      UInt64 phyPos = PhyOffsets[left] + relat;
      if (_needStartSeek || _phyPos != phyPos)
      {
        RINOK(Handler->_stream->Seek((Int64)(item.GetDataPosition() + phyPos), STREAM_SEEK_SET, NULL));
        _needStartSeek = false;
        _phyPos = phyPos;
      }
      res = Handler->_stream->Read(data, size, &size);
      _phyPos += size;
    }
    else
    {
      UInt64 next = item.Size;
      if (_virtPos < sb.Offset)
        next = sb.Offset;
      else if (left + 1 < item.SparseBlocks.Size())
        next = item.SparseBlocks[left + 1].Offset;
      UInt64 rem = next - _virtPos;
      if (size > rem)
        size = (UInt32)rem;
      memset(data, 0, size);
    }
  }
  
  _virtPos += size;
  if (processedSize)
    *processedSize = size;
  return res;
}

STDMETHODIMP CSparseStream::Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition)
{
  switch (seekOrigin)
  {
    case STREAM_SEEK_SET: break;
    case STREAM_SEEK_CUR: offset += _virtPos; break;
    case STREAM_SEEK_END: offset += Handler->_items[ItemIndex].Size; break;
    default: return STG_E_INVALIDFUNCTION;
  }
  if (offset < 0)
    return HRESULT_WIN32_ERROR_NEGATIVE_SEEK;
  _virtPos = (UInt64)offset;
  if (newPosition)
    *newPosition = _virtPos;
  return S_OK;
}

STDMETHODIMP CHandler::GetStream(UInt32 index, ISequentialInStream **stream)
{
  COM_TRY_BEGIN
  
  const CItemEx &item = _items[index];

  if (item.IsSparse())
  {
    CSparseStream *streamSpec = new CSparseStream;
    CMyComPtr<IInStream> streamTemp = streamSpec;
    streamSpec->Init();
    streamSpec->Handler = this;
    streamSpec->HandlerRef = (IInArchive *)this;
    streamSpec->ItemIndex = index;
    streamSpec->PhyOffsets.Reserve(item.SparseBlocks.Size());
    UInt64 offs = 0;
    FOR_VECTOR(i, item.SparseBlocks)
    {
      const CSparseBlock &sb = item.SparseBlocks[i];
      streamSpec->PhyOffsets.AddInReserved(offs);
      offs += sb.Size;
    }
    *stream = streamTemp.Detach();
    return S_OK;
  }
  
  if (item.IsSymLink())
  {
    Create_BufInStream_WithReference((const Byte *)(const char *)item.LinkName, item.LinkName.Len(), (IInArchive *)this, stream);
    return S_OK;
  }
  
  return CreateLimitedInStream(_stream, item.GetDataPosition(), item.PackSize, stream);
  
  COM_TRY_END
}

void CHandler::Init()
{
  _forceCodePage = false;
  _curCodePage = _specifiedCodePage = CP_UTF8;  // CP_OEMCP;
  _thereIsPaxExtendedHeader = false;
}

STDMETHODIMP CHandler::SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps)
{
  Init();

  for (UInt32 i = 0; i < numProps; i++)
  {
    UString name = names[i];
    name.MakeLower_Ascii();
    if (name.IsEmpty())
      return E_INVALIDARG;

    const PROPVARIANT &prop = values[i];

    if (name[0] == L'x')
    {
      // some clients write 'x' property. So we support it
      UInt32 level = 0;
      RINOK(ParsePropToUInt32(name.Ptr(1), prop, level));
    }
    else if (name.IsEqualTo("cp"))
    {
      UInt32 cp = CP_OEMCP;
      RINOK(ParsePropToUInt32(L"", prop, cp));
      _forceCodePage = true;
      _curCodePage = _specifiedCodePage = cp;
    }
    else if (name.IsPrefixedBy_Ascii_NoCase("mt"))
    {
    }
    else if (name.IsPrefixedBy_Ascii_NoCase("memuse"))
    {
    }
    else
      return E_INVALIDARG;
  }
  return S_OK;
}

}}

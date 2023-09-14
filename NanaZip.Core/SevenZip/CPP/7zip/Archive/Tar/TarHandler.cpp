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
  kpidCTime,
  kpidATime,
  kpidPosixAttrib,
  kpidUser,
  kpidGroup,
  kpidUserId,
  kpidGroupId,
  kpidSymLink,
  kpidHardLink,
  kpidCharacts,
  kpidComment
  , kpidDeviceMajor
  , kpidDeviceMinor
  // , kpidDevice
  // , kpidHeadersSize // for debug
  // , kpidOffset // for debug
};

static const Byte kArcProps[] =
{
  kpidHeadersSize,
  kpidCodePage,
  kpidCharacts,
  kpidComment
};

static const char *k_Characts_Prefix = "PREFIX";

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPhySize:     if (_arc._phySize_Defined) prop = _arc._phySize; break;
    case kpidHeadersSize: if (_arc._phySize_Defined) prop = _arc._headersSize; break;
    case kpidErrorFlags:
    {
      UInt32 flags = 0;
      if (!_isArc)
        flags |= kpv_ErrorFlags_IsNotArc;
      else switch (_arc._error)
      {
        case k_ErrorType_UnexpectedEnd: flags = kpv_ErrorFlags_UnexpectedEnd; break;
        case k_ErrorType_Corrupted: flags = kpv_ErrorFlags_HeadersError; break;
        case k_ErrorType_OK: break;
        // case k_ErrorType_Warning: break;
        // default: break;
      }
      if (flags != 0)
        prop = flags;
      break;
    }

    case kpidWarningFlags:
    {
      if (_arc._is_Warning)
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
      AString s;
      if (_arc._are_Gnu) s.Add_OptSpaced("GNU");
      if (_arc._are_Posix) s.Add_OptSpaced("POSIX");
      if (_arc._are_Pax_Items) s.Add_OptSpaced("PAX_ITEM");
      if (_arc._pathPrefix_WasUsed) s.Add_OptSpaced(k_Characts_Prefix);
      if (_arc._are_LongName) s.Add_OptSpaced("LongName");
      if (_arc._are_LongLink) s.Add_OptSpaced("LongLink");
      if (_arc._are_Pax) s.Add_OptSpaced("PAX");
      if (_arc._are_pax_path) s.Add_OptSpaced("path");
      if (_arc._are_pax_link) s.Add_OptSpaced("linkpath");
      if (_arc._are_mtime) s.Add_OptSpaced("mtime");
      if (_arc._are_atime) s.Add_OptSpaced("atime");
      if (_arc._are_ctime) s.Add_OptSpaced("ctime");
      if (_arc._is_PaxGlobal_Error) s.Add_OptSpaced("PAX_GLOBAL_ERROR");
      s.Add_OptSpaced(_encodingCharacts.GetCharactsString());
      prop = s;
      break;
    }

    case kpidComment:
    {
      if (_arc.PaxGlobal_Defined)
      {
        AString s;
        _arc.PaxGlobal.Print_To_String(s);
        if (!s.IsEmpty())
          prop = s;
      }
      break;
    }
  }
  prop.Detach(value);
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
  UInt64 endPos;
  {
    RINOK(InStream_AtBegin_GetSize(stream, endPos))
  }
  
  _arc._phySize_Defined = true;
  
  // bool utf8_OK = true;

  _arc.SeqStream = stream;
  _arc.InStream = stream;
  _arc.OpenCallback = callback;

  CItemEx item;
  for (;;)
  {
    _arc.NumFiles = _items.Size();
    RINOK(_arc.ReadItem(item))
    if (!_arc.filled)
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

    RINOK(stream->Seek((Int64)item.Get_PackSize_Aligned(), STREAM_SEEK_CUR, &_arc._phySize))
    if (_arc._phySize > endPos)
    {
      _arc._error = k_ErrorType_UnexpectedEnd;
      break;
    }
    /*
    if (_phySize == endPos)
    {
      _errorMessage = "There are no trailing zero-filled records";
      break;
    }
    */
    /*
    if (callback)
    {
      if (_items.Size() == 1)
      {
        RINOK(callback->SetTotal(NULL, &endPos));
      }
      if ((_items.Size() & 0x3FF) == 0)
      {
        const UInt64 numFiles = _items.Size();
        RINOK(callback->SetCompleted(&numFiles, &_phySize));
      }
    }
    */
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
    if (_arc._error != k_ErrorType_OK)
    {
      _isArc = false;
      return S_FALSE;
    }
    if (!callback)
      return S_FALSE;
    Z7_DECL_CMyComPtr_QI_FROM(
        IArchiveOpenVolumeCallback,
        openVolumeCallback, callback)
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

Z7_COM7F_IMF(CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *openArchiveCallback))
{
  COM_TRY_BEGIN
  // for (int i = 0; i < 10; i++) // for debug
  {
    Close();
    RINOK(Open2(stream, openArchiveCallback))
    _stream = stream;
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::OpenSeq(ISequentialInStream *stream))
{
  Close();
  _seqStream = stream;
  _isArc = true;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Close())
{
  _isArc = false;

  _arc.Clear();

  _curIndex = 0;
  _latestIsRead = false;
  _encodingCharacts.Clear();
  _items.Clear();
  _seqStream.Release();
  _stream.Release();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
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
      const UInt64 packSize = _latestItem.Get_PackSize_Aligned();
      RINOK(copyCoder->Code(_seqStream, NULL, &packSize, &packSize, NULL))
      _arc._phySize += copyCoderSpec->TotalSize;
      if (copyCoderSpec->TotalSize != packSize)
      {
        _arc._error = k_ErrorType_UnexpectedEnd;
        return S_FALSE;
      }
      _latestIsRead = false;
      _curIndex++;
    }
    else
    {
      _arc.SeqStream = _seqStream;
      _arc.InStream = NULL;
      RINOK(_arc.ReadItem(_latestItem))
      if (!_arc.filled)
      {
        _arc._phySize_Defined = true;
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


// CPaxTime is defined (NumDigits >= 0)
static void PaxTimeToProp(const CPaxTime &pt, NWindows::NCOM::CPropVariant &prop)
{
  UInt64 v;
  if (!NTime::UnixTime64_To_FileTime64(pt.Sec, v))
    return;
  if (pt.Ns != 0)
    v += pt.Ns / 100;
  FILETIME ft;
  ft.dwLowDateTime = (DWORD)v;
  ft.dwHighDateTime = (DWORD)(v >> 32);
  prop.SetAsTimeFrom_FT_Prec_Ns100(ft,
      k_PropVar_TimePrec_Base + (unsigned)pt.NumDigits, pt.Ns % 100);
}


#define ValToHex(t) ((char)(((t) < 10) ? ('0' + (t)) : ('a' + ((t) - 10))))

static void AddByteToHex2(unsigned val, AString &s)
{
  unsigned t;
  t = val >> 4;
  s += ValToHex(t);
  t = val & 0xF;
  s += ValToHex(t);
}

static void AddSpecCharToString(const char c, AString &s)
{
  if ((Byte)c <= 0x20 || (Byte)c > 127)
  {
    s += '[';
    AddByteToHex2((Byte)(c), s);
    s += ']';
  }
  else
    s += c;
}

static void AddSpecUInt64(AString &s, const char *name, UInt64 v)
{
  if (v != 0)
  {
    s.Add_OptSpaced(name);
    if (v > 1)
    {
      s += ':';
      s.Add_UInt64(v);
    }
  }
}

static void AddSpecBools(AString &s, const char *name, bool b1, bool b2)
{
  if (b1)
  {
    s.Add_OptSpaced(name);
    if (b2)
      s += '*';
  }
}


Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
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
      RINOK(SkipTo(index))
      item = &_latestItem;
    }
  }

  switch (propID)
  {
    case kpidPath: TarStringToUnicode(item->Name, prop, true); break;
    case kpidIsDir: prop = item->IsDir(); break;
    case kpidSize: prop = item->Get_UnpackSize(); break;
    case kpidPackSize: prop = item->Get_PackSize_Aligned(); break;
    case kpidMTime:
    {
      /*
      // for debug:
      PropVariant_SetFrom_UnixTime(prop, 1 << 30);
      prop.wReserved1 = k_PropVar_TimePrec_Base + 1;
      prop.wReserved2 = 12;
      break;
      */

      if (item->PaxTimes.MTime.IsDefined())
        PaxTimeToProp(item->PaxTimes.MTime, prop);
      else
      // if (item->MTime != 0)
      {
        // we allow (item->MTime == 0)
        FILETIME ft;
        if (NTime::UnixTime64_To_FileTime(item->MTime, ft))
        {
          unsigned prec = k_PropVar_TimePrec_Unix;
          if (item->MTime_IsBin)
          {
            /* we report here that it's Int64-UnixTime
               instead of basic UInt32-UnixTime range */
            prec = k_PropVar_TimePrec_Base;
          }
          prop.SetAsTimeFrom_FT_Prec(ft, prec);
        }
      }
      break;
    }
    case kpidATime:
      if (item->PaxTimes.ATime.IsDefined())
        PaxTimeToProp(item->PaxTimes.ATime, prop);
      break;
    case kpidCTime:
      if (item->PaxTimes.CTime.IsDefined())
        PaxTimeToProp(item->PaxTimes.CTime, prop);
      break;
    case kpidPosixAttrib: prop = item->Get_Combined_Mode(); break;
    
    case kpidUser:
      if (!item->User.IsEmpty())
        TarStringToUnicode(item->User, prop);
      break;
    case kpidGroup:
      if (!item->Group.IsEmpty())
        TarStringToUnicode(item->Group, prop);
      break;

    case kpidUserId:
      // if (item->UID != 0)
        prop = (UInt32)item->UID;
      break;
    case kpidGroupId:
      // if (item->GID != 0)
        prop = (UInt32)item->GID;
      break;

    case kpidDeviceMajor:
      if (item->DeviceMajor_Defined)
      // if (item->DeviceMajor != 0)
        prop = (UInt32)item->DeviceMajor;
      break;

    case kpidDeviceMinor:
      if (item->DeviceMinor_Defined)
      // if (item->DeviceMinor != 0)
        prop = (UInt32)item->DeviceMinor;
      break;
    /*
    case kpidDevice:
      if (item->DeviceMajor_Defined)
      if (item->DeviceMinor_Defined)
        prop = (UInt64)MY_dev_makedev(item->DeviceMajor, item->DeviceMinor);
      break;
    */

    case kpidSymLink:
      if (item->Is_SymLink())
        if (!item->LinkName.IsEmpty())
          TarStringToUnicode(item->LinkName, prop);
      break;
    case kpidHardLink:
      if (item->Is_HardLink())
        if (!item->LinkName.IsEmpty())
          TarStringToUnicode(item->LinkName, prop);
      break;

    case kpidCharacts:
    {
      AString s;
      {
        s.Add_Space_if_NotEmpty();
        AddSpecCharToString(item->LinkFlag, s);
      }
      if (item->IsMagic_GNU())
        s.Add_OptSpaced("GNU");
      else if (item->IsMagic_Posix_ustar_00())
        s.Add_OptSpaced("POSIX");
      else
      {
        s.Add_Space_if_NotEmpty();
        for (unsigned i = 0; i < sizeof(item->Magic); i++)
          AddSpecCharToString(item->Magic[i], s);
      }

      if (item->IsSignedChecksum)
        s.Add_OptSpaced("SignedChecksum");

      if (item->Prefix_WasUsed)
        s.Add_OptSpaced(k_Characts_Prefix);

      s.Add_OptSpaced(item->EncodingCharacts.GetCharactsString());

      // AddSpecUInt64(s, "LongName", item->Num_LongName_Records);
      // AddSpecUInt64(s, "LongLink", item->Num_LongLink_Records);
      AddSpecBools(s, "LongName", item->LongName_WasUsed, item->LongName_WasUsed_2);
      AddSpecBools(s, "LongLink", item->LongLink_WasUsed, item->LongLink_WasUsed_2);

      if (item->MTime_IsBin)
        s.Add_OptSpaced("bin_mtime");
      if (item->PackSize_IsBin)
        s.Add_OptSpaced("bin_psize");
      if (item->Size_IsBin)
        s.Add_OptSpaced("bin_size");
      
      AddSpecUInt64(s, "PAX", item->Num_Pax_Records);

      if (item->PaxTimes.MTime.IsDefined()) s.Add_OptSpaced("mtime");
      if (item->PaxTimes.ATime.IsDefined()) s.Add_OptSpaced("atime");
      if (item->PaxTimes.CTime.IsDefined()) s.Add_OptSpaced("ctime");

      if (item->pax_path_WasUsed)
        s.Add_OptSpaced("pax_path");
      if (item->pax_link_WasUsed)
        s.Add_OptSpaced("pax_linkpath");
      if (item->pax_size_WasUsed)
        s.Add_OptSpaced("pax_size");

      if (item->IsThereWarning())
        s.Add_OptSpaced("WARNING");
      if (item->HeaderError)
        s.Add_OptSpaced("ERROR");
      if (item->Pax_Error)
        s.Add_OptSpaced("PAX_error");
      if (!item->PaxExtra.RawLines.IsEmpty())
        s.Add_OptSpaced("PAX_unsupported_line");
      if (item->Pax_Overflow)
        s.Add_OptSpaced("PAX_overflow");
      if (!s.IsEmpty())
        prop = s;
      break;
    }
    case kpidComment:
    {
      AString s;
      item->PaxExtra.Print_To_String(s);
      if (!s.IsEmpty())
        prop = s;
      break;
    }
    // case kpidHeadersSize: prop = item->HeaderSize; break; // for debug
    // case kpidOffset: prop = item->HeaderPos; break; // for debug
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  ISequentialInStream *stream = _seqStream;
  const bool seqMode = (_stream == NULL);
  if (!seqMode)
    stream = _stream;

  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _items.Size();
  if (_stream && numItems == 0)
    return S_OK;
  UInt64 totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
    totalSize += _items[allFilesMode ? i : indices[i]].Get_UnpackSize();
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
    RINOK(lps->SetCur())
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
      RINOK(res)
      item = &_latestItem;
    }
    else
      item = &_items[index];

    RINOK(extractCallback->GetStream(index, &realOutStream, askMode))
    const UInt64 unpackSize = item->Get_UnpackSize();
    totalSize += unpackSize;
    totalPackSize += item->Get_PackSize_Aligned();
    if (item->IsDir())
    {
      RINOK(extractCallback->PrepareOperation(askMode))
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
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
          RINOK(extractCallback->PrepareOperation(askMode))
          RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
        }
        */
        continue;
      }
      skipMode = true;
      askMode = NExtract::NAskMode::kSkip;
    }
    RINOK(extractCallback->PrepareOperation(askMode))

    outStreamSpec->SetStream(realOutStream);
    realOutStream.Release();
    outStreamSpec->Init(skipMode ? 0 : unpackSize, true);

    Int32 opRes = NExtract::NOperationResult::kOK;
    CMyComPtr<ISequentialInStream> inStream2;
    if (!item->Is_Sparse())
      inStream2 = inStream;
    else
    {
      GetStream(index, &inStream2);
      if (!inStream2)
        return E_FAIL;
    }

    {
      if (item->Is_SymLink())
      {
        RINOK(WriteStream(outStreamSpec, (const char *)item->LinkName, item->LinkName.Len()))
      }
      else
      {
        if (!seqMode)
        {
          RINOK(InStream_SeekSet(_stream, item->Get_DataPos()))
        }
        streamSpec->Init(item->Get_PackSize_Aligned());
        RINOK(copyCoder->Code(inStream2, outStream, NULL, NULL, progress))
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
    RINOK(extractCallback->SetOperationResult(opRes))
  }
  return S_OK;
  COM_TRY_END
}


Z7_CLASS_IMP_IInStream(
  CSparseStream
)
  UInt64 _phyPos;
  UInt64 _virtPos;
  bool _needStartSeek;

public:
  CHandler *Handler;
  CMyComPtr<IUnknown> HandlerRef;
  unsigned ItemIndex;
  CRecordVector<UInt64> PhyOffsets;

  void Init()
  {
    _virtPos = 0;
    _phyPos = 0;
    _needStartSeek = true;
  }
};


Z7_COM7F_IMF(CSparseStream::Read(void *data, UInt32 size, UInt32 *processedSize))
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
      const unsigned mid = (unsigned)(((size_t)left + (size_t)right) / 2);
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
        RINOK(InStream_SeekSet(Handler->_stream, (item.Get_DataPos() + phyPos)))
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

Z7_COM7F_IMF(CSparseStream::Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition))
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

Z7_COM7F_IMF(CHandler::GetStream(UInt32 index, ISequentialInStream **stream))
{
  COM_TRY_BEGIN
  
  const CItemEx &item = _items[index];

  if (item.Is_Sparse())
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
  
  if (item.Is_SymLink())
  {
    Create_BufInStream_WithReference((const Byte *)(const char *)item.LinkName, item.LinkName.Len(), (IInArchive *)this, stream);
    return S_OK;
  }
  
  return CreateLimitedInStream(_stream, item.Get_DataPos(), item.PackSize, stream);
  
  COM_TRY_END
}


void CHandler::Init()
{
  _forceCodePage = false;
  _curCodePage = _specifiedCodePage = CP_UTF8;  // CP_OEMCP;
  _posixMode = false;
  _posixMode_WasForced = false;
  // TimeOptions.Clear();
  _handlerTimeOptions.Init();
  // _handlerTimeOptions.Write_MTime.Val = true; // it's default already
}


Z7_COM7F_IMF(CHandler::SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps))
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
      RINOK(ParsePropToUInt32(name.Ptr(1), prop, level))
    }
    else if (name.IsEqualTo("cp"))
    {
      UInt32 cp = CP_OEMCP;
      RINOK(ParsePropToUInt32(L"", prop, cp))
      _forceCodePage = true;
      _curCodePage = _specifiedCodePage = cp;
    }
    else if (name.IsPrefixedBy_Ascii_NoCase("mt"))
    {
    }
    else if (name.IsPrefixedBy_Ascii_NoCase("memuse"))
    {
    }
    else if (name.IsEqualTo("m"))
    {
      if (prop.vt != VT_BSTR)
        return E_INVALIDARG;
      const UString s = prop.bstrVal;
      if (s.IsEqualTo_Ascii_NoCase("pax") ||
          s.IsEqualTo_Ascii_NoCase("posix"))
        _posixMode = true;
      else if (s.IsEqualTo_Ascii_NoCase("gnu"))
        _posixMode = false;
      else
        return E_INVALIDARG;
      _posixMode_WasForced = true;
    }
    else
    {
      /*
      if (name.IsPrefixedBy_Ascii_NoCase("td"))
      {
        name.Delete(0, 3);
        if (prop.vt == VT_EMPTY)
        {
          if (name.IsEqualTo_Ascii_NoCase("n"))
          {
            // TimeOptions.UseNativeDigits = true;
          }
          else if (name.IsEqualTo_Ascii_NoCase("r"))
          {
            // TimeOptions.RemoveZeroDigits = true;
          }
          else
            return E_INVALIDARG;
        }
        else
        {
          UInt32 numTimeDigits = 0;
          RINOK(ParsePropToUInt32(name, prop, numTimeDigits));
          TimeOptions.NumDigits_WasForced = true;
          TimeOptions.NumDigits = numTimeDigits;
        }
      }
      */
      bool processed = false;
      RINOK(_handlerTimeOptions.Parse(name, prop, processed))
      if (processed)
        continue;
      return E_INVALIDARG;
    }
  }
  return S_OK;
}

}}

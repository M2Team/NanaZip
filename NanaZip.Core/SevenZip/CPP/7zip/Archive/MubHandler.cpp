// MubHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"
#include "../../../C/SwapBytes.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"

#include "../../Windows/PropVariant.h"

#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "HandlerCont.h"

using namespace NWindows;
using namespace NCOM;

namespace NArchive {
namespace NMacho {
void GetCpuName(AString &s, UInt32 const cpuType, UInt32 const subType);
}
namespace NMub {

struct CItem
{
  UInt32 Type;
  UInt32 SubType;
  UInt32 Offset;
  UInt32 Size;
  UInt32 Align;

  void GetTypeString(AString &s) const
  {
    NMacho::GetCpuName(s, Type, SubType);
    s.Replace('.', '_');
  }
};

static const unsigned kNumFilesMax = 8; // we can change it to any value

Z7_class_CHandler_final: public CHandlerCont
{
  Z7_IFACE_COM7_IMP(IInArchive_Cont)

  UInt64 _phySize;
  UInt32 _numItems;
  bool _bigEndian;
  UString _arcName;
  CItem _items[kNumFilesMax];

  virtual int GetItem_ExtractInfo(UInt32 index, UInt64 &pos, UInt64 &size) const Z7_override
  {
    const CItem &item = _items[index];
    pos = item.Offset;
    size = item.Size;
    return NExtract::NOperationResult::kOK;
  }
};

static const Byte kArcProps[] =
{
  kpidBigEndian
};

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidOffset,
  kpidClusterSize // Align
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  PropVariant_Clear(value);
  switch (propID)
  {
    case kpidBigEndian: PropVarEm_Set_Bool(value, _bigEndian); break;
    case kpidPhySize: PropVarEm_Set_UInt64(value, _phySize); break;
  }
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  PropVariant_Clear(value);
  const CItem &item = _items[index];
  switch (propID)
  {
    case kpidPath:
    case kpidExtension:
    {
      AString s;
      item.GetTypeString(s);
      if (propID == kpidExtension)
      {
        if (_arcName.IsEmpty())
          return PropVarEm_Set_Str(value, s);
      }
      else if (!_arcName.IsEmpty())
      {
        CPropVariant prop;
#if 1
        // use dir extension postfix : "name.x64"
        UString res (_arcName);
        res.Add_Dot();
        res += s;
#else
        // use dir prefix : "x64/name"
        UString res (s);
        res.Add_PathSepar();
        res += _arcName;
#endif
        prop = res;
        prop.Detach(value);
        return S_OK;
      }
      break;
    }
    case kpidSize:
    case kpidPackSize:
      PropVarEm_Set_UInt64(value, item.Size);
      break;
    case kpidOffset:
      PropVarEm_Set_UInt64(value, item.Offset);
      break;
    case kpidClusterSize:
      PropVarEm_Set_UInt32(value, (UInt32)1 << item.Align);
      break;
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Open(IInStream *inStream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback *openArchiveCallback))
{
  COM_TRY_BEGIN
  const unsigned k_NumWords_in_PreHeader = 2;
  const unsigned k_NumWords_in_Record = 5;
#define GET_NUM_WORDS_BEFORE_RECORD_INDEX(numRecords) \
    (k_NumWords_in_PreHeader + (size_t)(numRecords) * k_NumWords_in_Record)

  Close();
  UInt32 buf[GET_NUM_WORDS_BEFORE_RECORD_INDEX(kNumFilesMax)];
  size_t processed2 = sizeof(buf);
  RINOK(ReadStream(inStream, buf, &processed2))
  const size_t processed = processed2 / sizeof(UInt32);
  if (processed < k_NumWords_in_PreHeader)
    return S_FALSE;
  
  bool be;
  switch (buf[0])
  {
    case Z7_CONV_BE_TO_NATIVE_CONST32(0xCAFEBABE): be = true; break;
    case Z7_CONV_BE_TO_NATIVE_CONST32(0xB9FAF10E): be = false; break;
    default: return S_FALSE;
  }
  _bigEndian = be;
  if (
      #if defined(MY_CPU_BE)
        !
      #endif
        be)
    z7_SwapBytes4(buf, processed);
  const UInt32 num = buf[1];
  if (num == 0 || num > kNumFilesMax || processed < GET_NUM_WORDS_BEFORE_RECORD_INDEX(num))
    return S_FALSE;
  UInt64 endPosMax = k_NumWords_in_PreHeader * 4;
  const UInt32 *p = buf + k_NumWords_in_PreHeader;
  for (size_t i = 0; i < num; i++, p += k_NumWords_in_Record)
  {
    CItem &sb = _items[i];
    sb.Type = p[0];
    sb.SubType = p[1];
    sb.Offset = p[2];
    sb.Size = p[3];
    const UInt32 align = p[4];
    sb.Align = align;
    if (align >= 32
        || sb.Offset < GET_NUM_WORDS_BEFORE_RECORD_INDEX(num) * 4
        || (sb.Type    & ((1 << 24) - (1 << 8)))   // high bits can be used for flags
        || (sb.SubType & ((1 << 16) - (1 << 10)))) // high bits can be used for flags
      return S_FALSE;
    const UInt64 endPos = (UInt64)sb.Offset + sb.Size;
    if (endPosMax < endPos)
      endPosMax = endPos;
  }
  _numItems = num;
  _phySize = endPosMax;
  _stream = inStream;

  if (openArchiveCallback)
  {
    CMyComPtr<IArchiveOpenVolumeCallback> openVolumeCallback;
    openArchiveCallback->QueryInterface(IID_IArchiveOpenVolumeCallback, (void **)&openVolumeCallback);
    if (openVolumeCallback)
    {
      NCOM::CPropVariant prop;
      RINOK(openVolumeCallback->GetProperty(kpidName, &prop))
      if (prop.vt == VT_BSTR)
      {
        _arcName = prop.bstrVal;
        if (_arcName.Len() > 4 && StringsAreEqualNoCase_Ascii(_arcName.RightPtr(4), ".mub"))
          _arcName.DeleteFrom(_arcName.Len() - 4);
      }
    }
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _stream.Release();
  _arcName.Empty();
  _numItems = 0;
  _phySize = 0;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _numItems;
  return S_OK;
}

static const Byte k_Signature[] = {
    7, 0xCA, 0xFE, 0xBA, 0xBE, 0, 0, 0,
    4, 0xB9, 0xFA, 0xF1, 0x0E };

REGISTER_ARC_I(
  "Mub", "mub", NULL, 0xE2,
  k_Signature,
  0,
  NArcInfoFlags::kMultiSignature |
  NArcInfoFlags::kPreArc,
  NULL)

}}

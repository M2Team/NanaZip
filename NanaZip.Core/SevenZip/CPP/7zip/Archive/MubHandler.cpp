// MubHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"
#include "../../../C/SwapBytes.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/MyString.h"

#include "../../Windows/PropVariant.h"

#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "HandlerCont.h"

using namespace NWindows;
using namespace NCOM;

namespace NArchive {
namespace NMub {

#define MACH_CPU_ARCH_ABI64 ((UInt32)1 << 24)
#define MACH_CPU_TYPE_386    7
#define MACH_CPU_TYPE_ARM   12
#define MACH_CPU_TYPE_SPARC 14
#define MACH_CPU_TYPE_PPC   18

#define MACH_CPU_TYPE_PPC64 (MACH_CPU_ARCH_ABI64 | MACH_CPU_TYPE_PPC)
#define MACH_CPU_TYPE_AMD64 (MACH_CPU_ARCH_ABI64 | MACH_CPU_TYPE_386)
#define MACH_CPU_TYPE_ARM64 (MACH_CPU_ARCH_ABI64 | MACH_CPU_TYPE_ARM)

#define MACH_CPU_SUBTYPE_LIB64 ((UInt32)1 << 31)

#define MACH_CPU_SUBTYPE_I386_ALL 3

struct CItem
{
  UInt32 Type;
  UInt32 SubType;
  UInt32 Offset;
  UInt32 Size;
  UInt32 Align;
};

static const UInt32 kNumFilesMax = 6;

Z7_class_CHandler_final: public CHandlerCont
{
  Z7_IFACE_COM7_IMP(IInArchive_Cont)

  // UInt64 _startPos;
  UInt64 _phySize;
  UInt32 _numItems;
  bool _bigEndian;
  CItem _items[kNumFilesMax];

  HRESULT Open2(IInStream *stream);

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
  PropVariant_Clear(value);
  const CItem &item = _items[index];
  switch (propID)
  {
    case kpidExtension:
    {
      char temp[32];
      const char *ext = NULL;
      switch (item.Type)
      {
        case MACH_CPU_TYPE_386:   ext = "x86";   break;
        case MACH_CPU_TYPE_ARM:   ext = "arm";   break;
        case MACH_CPU_TYPE_SPARC: ext = "sparc"; break;
        case MACH_CPU_TYPE_PPC:   ext = "ppc";   break;
        case MACH_CPU_TYPE_AMD64: ext = "x64";   break;
        case MACH_CPU_TYPE_ARM64: ext = "arm64"; break;
        case MACH_CPU_TYPE_PPC64: ext = "ppc64"; break;
        default:
          temp[0] = 'c';
          temp[1] = 'p';
          temp[2] = 'u';
          char *p = ConvertUInt32ToString(item.Type & ~MACH_CPU_ARCH_ABI64, temp + 3);
          if (item.Type & MACH_CPU_ARCH_ABI64)
            MyStringCopy(p, "_64");
          break;
      }
      if (ext)
        MyStringCopy(temp, ext);
      if (item.SubType != 0)
      if ((item.Type != MACH_CPU_TYPE_386 &&
           item.Type != MACH_CPU_TYPE_AMD64)
           || (item.SubType & ~(UInt32)MACH_CPU_SUBTYPE_LIB64) != MACH_CPU_SUBTYPE_I386_ALL
         )
      {
        unsigned pos = MyStringLen(temp);
        temp[pos++] = '-';
        ConvertUInt32ToString(item.SubType, temp + pos);
      }
      return PropVarEm_Set_Str(value, temp);
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
}

HRESULT CHandler::Open2(IInStream *stream)
{
  // RINOK(InStream_GetPos(stream, _startPos));

  const UInt32 kHeaderSize = 2;
  const UInt32 kRecordSize = 5;
  const UInt32 kBufSize = kHeaderSize + kNumFilesMax * kRecordSize;
  UInt32 buf[kBufSize];
  size_t processed = kBufSize * 4;
  RINOK(ReadStream(stream, buf, &processed))
  processed >>= 2;
  if (processed < kHeaderSize)
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
    z7_SwapBytes4(&buf[1], processed - 1);
  const UInt32 num = buf[1];
  if (num > kNumFilesMax || processed < kHeaderSize + num * kRecordSize)
    return S_FALSE;
  if (num == 0)
    return S_FALSE;
  UInt64 endPosMax = kHeaderSize;

  for (UInt32 i = 0; i < num; i++)
  {
    const UInt32 *p = buf + kHeaderSize + i * kRecordSize;
    CItem &sb = _items[i];
    sb.Type = p[0];
    sb.SubType = p[1];
    sb.Offset = p[2];
    sb.Size = p[3];
    const UInt32 align = p[4];
    sb.Align = align;
    if (align > 31)
      return S_FALSE;
    if (sb.Offset < kHeaderSize + num * kRecordSize)
      return S_FALSE;
    if ((sb.Type & ~MACH_CPU_ARCH_ABI64) >= 0x100 ||
        (sb.SubType & ~MACH_CPU_SUBTYPE_LIB64) >= 0x100)
      return S_FALSE;

    const UInt64 endPos = (UInt64)sb.Offset + sb.Size;
    if (endPosMax < endPos)
      endPosMax = endPos;
  }
  _numItems = num;
  _phySize = endPosMax;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Open(IInStream *inStream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback * /* openArchiveCallback */))
{
  COM_TRY_BEGIN
  Close();
  try
  {
    if (Open2(inStream) != S_OK)
      return S_FALSE;
    _stream = inStream;
  }
  catch(...) { return S_FALSE; }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _stream.Release();
  _numItems = 0;
  _phySize = 0;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _numItems;
  return S_OK;
}

namespace NBe {

static const Byte k_Signature[] = {
    7, 0xCA, 0xFE, 0xBA, 0xBE, 0, 0, 0,
    4, 0xB9, 0xFA, 0xF1, 0x0E };

REGISTER_ARC_I(
  "Mub", "mub", NULL, 0xE2,
  k_Signature,
  0,
  NArcInfoFlags::kMultiSignature,
  NULL)

}

}}

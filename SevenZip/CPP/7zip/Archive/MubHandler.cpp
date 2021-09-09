// MubHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/MyString.h"

#include "../../Windows/PropVariant.h"

#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "HandlerCont.h"

static UInt32 Get32(const Byte *p, bool be) { if (be) return GetBe32(p); return GetUi32(p); }

using namespace NWindows;
using namespace NCOM;

namespace NArchive {
namespace NMub {

#define MACH_CPU_ARCH_ABI64 (1 << 24)
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
  // UInt32 Align;
};

static const UInt32 kNumFilesMax = 10;

class CHandler: public CHandlerCont
{
  // UInt64 _startPos;
  UInt64 _phySize;
  UInt32 _numItems;
  bool _bigEndian;
  CItem _items[kNumFilesMax];

  HRESULT Open2(IInStream *stream);

  virtual int GetItem_ExtractInfo(UInt32 index, UInt64 &pos, UInt64 &size) const
  {
    const CItem &item = _items[index];
    pos = item.Offset;
    size = item.Size;
    return NExtract::NOperationResult::kOK;
  }

public:
  INTERFACE_IInArchive_Cont(;)
};

static const Byte kArcProps[] =
{
  kpidBigEndian
};

static const Byte kProps[] =
{
  kpidSize
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  PropVariant_Clear(value);
  switch (propID)
  {
    case kpidBigEndian: PropVarEm_Set_Bool(value, _bigEndian); break;
    case kpidPhySize: PropVarEm_Set_UInt64(value, _phySize); break;
  }
  return S_OK;
}

STDMETHODIMP CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
{
  PropVariant_Clear(value);
  const CItem &item = _items[index];
  switch (propID)
  {
    case kpidExtension:
    {
      char temp[32];
      const char *ext = 0;
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
          ConvertUInt32ToString(item.Type & ~MACH_CPU_ARCH_ABI64, temp + 3);
          if (item.Type & MACH_CPU_ARCH_ABI64)
            MyStringCopy(temp + MyStringLen(temp), "_64");
          break;
      }
      if (ext)
        strcpy(temp, ext);
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
  }
  return S_OK;
}

HRESULT CHandler::Open2(IInStream *stream)
{
  // RINOK(stream->Seek(0, STREAM_SEEK_CUR, &_startPos));

  const UInt32 kHeaderSize = 8;
  const UInt32 kRecordSize = 5 * 4;
  const UInt32 kBufSize = kHeaderSize + kNumFilesMax * kRecordSize;
  Byte buf[kBufSize];
  size_t processed = kBufSize;
  RINOK(ReadStream(stream, buf, &processed));
  if (processed < kHeaderSize)
    return S_FALSE;
  
  bool be;
  switch (GetBe32(buf))
  {
    case 0xCAFEBABE: be = true; break;
    case 0xB9FAF10E: be = false; break;
    default: return S_FALSE;
  }
  _bigEndian = be;
  UInt32 num = Get32(buf + 4, be);
  if (num > kNumFilesMax || processed < kHeaderSize + num * kRecordSize)
    return S_FALSE;
  if (num == 0)
    return S_FALSE;
  UInt64 endPosMax = kHeaderSize;

  for (UInt32 i = 0; i < num; i++)
  {
    const Byte *p = buf + kHeaderSize + i * kRecordSize;
    CItem &sb = _items[i];
    sb.Type = Get32(p, be);
    sb.SubType = Get32(p + 4, be);
    sb.Offset = Get32(p + 8, be);
    sb.Size = Get32(p + 12, be);
    UInt32 align = Get32(p + 16, be);
    if (align > 31)
      return S_FALSE;
    if (sb.Offset < kHeaderSize + num * kRecordSize)
      return S_FALSE;
    if ((sb.Type & ~MACH_CPU_ARCH_ABI64) >= 0x100 ||
        (sb.SubType & ~MACH_CPU_SUBTYPE_LIB64) >= 0x100)
      return S_FALSE;

    UInt64 endPos = (UInt64)sb.Offset + sb.Size;
    if (endPosMax < endPos)
      endPosMax = endPos;
  }
  _numItems = num;
  _phySize = endPosMax;
  return S_OK;
}

STDMETHODIMP CHandler::Open(IInStream *inStream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback * /* openArchiveCallback */)
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

STDMETHODIMP CHandler::Close()
{
  _stream.Release();
  _numItems = 0;
  _phySize = 0;
  return S_OK;
}

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = _numItems;
  return S_OK;
}

namespace NBe {

static const Byte k_Signature[] = {
    7, 0xCA, 0xFE, 0xBA, 0xBE, 0, 0, 0,
    4, 0xB9, 0xFA, 0xF1, 0x0E };

REGISTER_ARC_I(
  "Mub", "mub", 0, 0xE2,
  k_Signature,
  0,
  NArcInfoFlags::kMultiSignature,
  NULL)

}

}}

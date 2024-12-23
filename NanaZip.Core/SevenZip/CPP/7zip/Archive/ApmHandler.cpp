// ApmHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"

#include "../../Windows/PropVariantUtils.h"

#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "HandlerCont.h"

#define Get32(p) GetBe32a(p)

using namespace NWindows;

namespace NArchive {

namespace NDmg {
  const char *Find_Apple_FS_Ext(const AString &name);
  bool Is_Apple_FS_Or_Unknown(const AString &name);
}

namespace NApm {

static const Byte kSig0 = 'E';
static const Byte kSig1 = 'R';

static const CUInt32PCharPair k_Flags[] =
{
  { 0, "VALID" },
  { 1, "ALLOCATED" },
  { 2, "IN_USE" },
  { 3, "BOOTABLE" },
  { 4, "READABLE" },
  { 5, "WRITABLE" },
  { 6, "OS_PIC_CODE" },
  // { 7, "OS_SPECIFIC_2" }, // "Unused"
  { 8, "ChainCompatible" }, // "OS_SPECIFIC_1"
  { 9, "RealDeviceDriver" },
  // { 10, "CanChainToNext" },
  { 30, "MOUNTED_AT_STARTUP" },
  { 31, "STARTUP" }
};

#define DPME_FLAGS_VALID      (1u << 0)
#define DPME_FLAGS_ALLOCATED  (1u << 1)

static const unsigned k_Str_Size = 32;

struct CItem
{
  UInt32 StartBlock;
  UInt32 NumBlocks;
  UInt32 Flags; // pmPartStatus
  char Name[k_Str_Size];
  char Type[k_Str_Size];
  /*
  UInt32 DataStartBlock;
  UInt32 NumDataBlocks;
  UInt32 BootStartBlock;
  UInt32 BootSize;
  UInt32 BootAddr;
  UInt32 BootEntry;
  UInt32 BootChecksum;
  char Processor[16];
  */

  bool Is_Valid_and_Allocated() const
    { return (Flags & (DPME_FLAGS_VALID | DPME_FLAGS_ALLOCATED)) != 0; }

  bool Parse(const UInt32 *p32, UInt32 &numBlocksInMap)
  {
    if (GetUi32a(p32) != 0x4d50) // "PM"
      return false;
    numBlocksInMap = Get32(p32 + 4 / 4);
    StartBlock = Get32(p32 + 8 / 4);
    NumBlocks = Get32(p32 + 0xc / 4);
    Flags = Get32(p32 + 0x58 / 4);
    memcpy(Name, p32 + 0x10 / 4, k_Str_Size);
    memcpy(Type, p32 + 0x30 / 4, k_Str_Size);
    /*
    DataStartBlock = Get32(p + 0x50);
    NumDataBlocks = Get32(p + 0x54);
    BootStartBlock = Get32(p + 0x5c);
    BootSize = Get32(p + 0x60);
    BootAddr = Get32(p + 0x64);
    if (Get32(p + 0x68) != 0)
      return false;
    BootEntry = Get32(p + 0x6c);
    if (Get32(p + 0x70) != 0)
      return false;
    BootChecksum = Get32(p + 0x74);
    memcpy(Processor, p32 + 0x78 / 4, 16);
    */
    return true;
  }
};


Z7_class_CHandler_final: public CHandlerCont
{
  Z7_IFACE_COM7_IMP(IInArchive_Cont)

  CRecordVector<CItem> _items;
  unsigned _blockSizeLog;
  bool _isArc;
  // UInt32 _numBlocks;
  UInt64 _phySize;

  UInt64 BlocksToBytes(UInt32 i) const { return (UInt64)i << _blockSizeLog; }

  virtual int GetItem_ExtractInfo(UInt32 index, UInt64 &pos, UInt64 &size) const Z7_override
  {
    const CItem &item = _items[index];
    pos = BlocksToBytes(item.StartBlock);
    size = BlocksToBytes(item.NumBlocks);
    return NExtract::NOperationResult::kOK;
  }
};

static const UInt32 kSectorSize = 512;

// we support only 4 cluster sizes: 512, 1024, 2048, 4096 */

API_FUNC_static_IsArc IsArc_Apm(const Byte *p, size_t size)
{
  if (size < kSectorSize)
    return k_IsArc_Res_NEED_MORE;
  if (GetUi32(p + 12) != 0)
    return k_IsArc_Res_NO;
  UInt32 v = GetUi32(p); // we read as little-endian
  v ^= kSig0 | (unsigned)kSig1 << 8;
  if (v & ~((UInt32)0xf << 17))
    return k_IsArc_Res_NO;
  if ((0x116u >> (v >> 17)) & 1)
    return k_IsArc_Res_YES;
  return k_IsArc_Res_NO;
}
}

Z7_COM7F_IMF(CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback * /* callback */))
{
  COM_TRY_BEGIN
  Close();

  UInt32 buf32[kSectorSize / 4];
  unsigned numPadSectors, blockSizeLog_from_Header;
  {
    // Driver Descriptor Map (DDM)
    RINOK(ReadStream_FALSE(stream, buf32, kSectorSize))
    //  8: UInt16 sbDevType : =0 (usually), =1 in Apple Mac OS X 10.3.0 iso
    // 10: UInt16 sbDevId   : =0 (usually), =1 in Apple Mac OS X 10.3.0 iso
    // 12: UInt32 sbData    : =0
    if (buf32[3] != 0)
      return S_FALSE;
    UInt32 v = GetUi32a(buf32); // we read as little-endian
    v ^= kSig0 | (unsigned)kSig1 << 8;
    if (v & ~((UInt32)0xf << 17))
      return S_FALSE;
    v >>= 16;
    if (v == 0)
      return S_FALSE;
    if (v & (v - 1))
      return S_FALSE;
    // v == { 16,8,4,2 } : block size (x256 bytes)
    const unsigned a =
#if 1
        (0x30210u >> v) & 3;
#else
        0; // for debug : hardcoded switch to 512-bytes mode
#endif
    numPadSectors = (1u << a) - 1;
    _blockSizeLog = blockSizeLog_from_Header = 9 + a;
  }

/*
  some APMs (that are ".iso" macOS installation files) contain
    (blockSizeLog == 11) in DDM header,
  and contain 2 overlapping maps:
    1) map for  512-bytes-step
    2) map for 2048-bytes-step
   512-bytes-step map is correct.
  2048-bytes-step map can be incorrect in some cases.

  macos 8 / OSX DP2 iso:
    There is shared "hfs" item in both maps.
    And correct (offset/size) values for "hfs" partition
    can be calculated only in 512-bytes mode (ignoring blockSizeLog == 11).
    But some records (Macintosh.Apple_Driver*_)
    can be correct on both modes: 512-bytes mode / 2048-bytes-step.
  
  macos 921 ppc / Apple Mac OS X 10.3.0 iso:
    Both maps are correct.
    If we use 512-bytes-step, each 4th item is (Apple_Void) with zero size.
    And these zero size (Apple_Void) items will be first items in 2048-bytes-step map.
*/

// we define Z7_APM_SWITCH_TO_512_BYTES, because
// we want to support old MACOS APMs that contain correct value only
// for 512-bytes-step mode
#define Z7_APM_SWITCH_TO_512_BYTES

  const UInt32 numBlocks_from_Header = Get32(buf32 + 1);
  UInt32 numBlocks = 0;
  {
    for (unsigned k = 0; k < numPadSectors; k++)
    {
      RINOK(ReadStream_FALSE(stream, buf32, kSectorSize))
#ifdef Z7_APM_SWITCH_TO_512_BYTES
      if (k == 0)
      {
        if (GetUi32a(buf32) == 0x4d50        // "PM"
            // && (Get32(buf32 + 0x58 / 4) & 1) // Flags::VALID
            // some old APMs don't use VALID flag for Apple_partition_map item
            && Get32(buf32 + 8 / 4) == 1)    // StartBlock
        {
          // we switch the mode to 512-bytes-step map reading:
          numPadSectors = 0;
          _blockSizeLog = 9;
          break;
        }
      }
#endif
    }
  }

  for (unsigned i = 0;;)
  {
#ifdef Z7_APM_SWITCH_TO_512_BYTES
    if (i != 0 || _blockSizeLog == blockSizeLog_from_Header)
#endif
    {
      RINOK(ReadStream_FALSE(stream, buf32, kSectorSize))
    }
 
    CItem item;
    UInt32 numBlocksInMap = 0;
    if (!item.Parse(buf32, numBlocksInMap))
      return S_FALSE;
    // v24.09: we don't check that all entries have same (numBlocksInMap) values,
    // because some APMs have different (numBlocksInMap) values, if (Apple_Void) is used.
    if (numBlocksInMap > (1 << 8) || numBlocksInMap <= i)
      return S_FALSE;

    const UInt32 finish = item.StartBlock + item.NumBlocks;
    if (finish < item.StartBlock)
      return S_FALSE;
    if (numBlocks < finish)
        numBlocks = finish;
    
    _items.Add(item);
    if (numPadSectors != 0)
    {
      RINOK(stream->Seek(numPadSectors << 9, STREAM_SEEK_CUR, NULL))
    }
    if (++i == numBlocksInMap)
      break;
  }
  
  _phySize = BlocksToBytes(numBlocks);
  // _numBlocks = numBlocks;
  const UInt64 physSize = (UInt64)numBlocks_from_Header << blockSizeLog_from_Header;
  if (_phySize < physSize)
      _phySize = physSize;
  _isArc = true;
  _stream = stream;

  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::Close())
{
  _isArc = false;
  _phySize = 0;
  _items.Clear();
  _stream.Release();
  return S_OK;
}


static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidOffset,
  kpidCharacts
  // , kpidCpu
};

static const Byte kArcProps[] =
{
  kpidClusterSize
  // , kpidNumBlocks
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

static void GetString(AString &dest, const char *src)
{
  dest.SetFrom_CalcLen(src, k_Str_Size);
}

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidMainSubfile:
    {
      int mainIndex = -1;
      FOR_VECTOR (i, _items)
      {
        const CItem &item = _items[i];
        if (!item.Is_Valid_and_Allocated())
          continue;
        AString s;
        GetString(s, item.Type);
        if (NDmg::Is_Apple_FS_Or_Unknown(s))
        {
          if (mainIndex != -1)
          {
            mainIndex = -1;
            break;
          }
          mainIndex = (int)i;
        }
      }
      if (mainIndex != -1)
        prop = (UInt32)(Int32)mainIndex;
      break;
    }
    case kpidClusterSize: prop = (UInt32)1 << _blockSizeLog; break;
    case kpidPhySize: prop = _phySize; break;
    // case kpidNumBlocks: prop = _numBlocks; break;

    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc) v |= kpv_ErrorFlags_IsNotArc;
      prop = v;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _items.Size();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  const CItem &item = _items[index];
  switch (propID)
  {
    case kpidPath:
    {
      AString s;
      GetString(s, item.Name);
      if (s.IsEmpty())
        s.Add_UInt32(index);
      AString type;
      GetString(type, item.Type);
      {
        const char *ext = NDmg::Find_Apple_FS_Ext(type);
        if (ext)
          type = ext;
      }
      if (!type.IsEmpty())
      {
        s.Add_Dot();
        s += type;
      }
      prop = s;
      break;
    }
/*
    case kpidCpu:
    {
      AString s;
      s.SetFrom_CalcLen(item.Processor, sizeof(item.Processor));
      if (!s.IsEmpty())
        prop = s;
      break;
    }
*/
    case kpidSize:
    case kpidPackSize:
      prop = BlocksToBytes(item.NumBlocks);
      break;
    case kpidOffset: prop = BlocksToBytes(item.StartBlock); break;
    case kpidCharacts: FLAGS_TO_PROP(k_Flags, item.Flags, prop); break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

static const Byte k_Signature[] = { kSig0, kSig1 };

REGISTER_ARC_I(
  "APM", "apm", NULL, 0xD4,
  k_Signature,
  0,
  0,
  IsArc_Apm)

}}

// CpioHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/MyLinux.h"
#include "../../Common/StringConvert.h"
#include "../../Common/StringToInt.h"
#include "../../Common/UTFConvert.h"

#include "../../Windows/PropVariant.h"
#include "../../Windows/TimeUtils.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"

#include "Common/ItemNameUtils.h"

using namespace NWindows;

namespace NArchive {
namespace NCpio {

static const Byte kMagicBin0 = 0xC7;
static const Byte kMagicBin1 = 0x71;

// #define MAGIC_ASCII { '0', '7', '0', '7', '0' }

static const Byte kMagicHex    = '1'; // New ASCII Format
static const Byte kMagicHexCrc = '2'; // New CRC Format
static const Byte kMagicOct    = '7'; // Portable ASCII Format

static const char * const kName_TRAILER = "TRAILER!!!";

static const unsigned k_BinRecord_Size = 2 + 8 * 2 + 2 * 4;
static const unsigned k_OctRecord_Size = 6 + 8 * 6 + 2 * 11;
static const unsigned k_HexRecord_Size = 6 + 13 * 8;

static const unsigned k_RecordSize_Max = k_HexRecord_Size;

enum EType
{
  k_Type_BinLe,
  k_Type_BinBe,
  k_Type_Oct,
  k_Type_Hex,
  k_Type_HexCrc
};

static const char * const k_Types[] =
{
    "Binary LE"
  , "Binary BE"
  , "Portable ASCII"
  , "New ASCII"
  , "New CRC"
};

struct CItem
{
  UInt32 inode;
  unsigned MainIndex_ForInode;
  UInt32 Mode;
  UInt32 MTime;
  UInt32 DevMajor;
  UInt32 DevMinor;
  UInt64 Size;
  AString Name;
  UInt32 NumLinks;
  UInt32 UID;
  UInt32 GID;
  UInt32 RDevMajor;
  UInt32 RDevMinor;
  UInt32 ChkSum;

  UInt32 AlignMask;
  EType Type;

  UInt32 HeaderSize;
  UInt64 HeaderPos;

  CByteBuffer Data; // for symlink


  UInt32 GetAlignedSize(UInt32 size) const
  {
    return (size + AlignMask) & ~(UInt32)AlignMask;
  }

  UInt64 GetPackSize() const
  {
    const UInt64 alignMask64 = AlignMask;
    return (Size + alignMask64) & ~(UInt64)alignMask64;
  }

  bool IsSame_inode_Dev(const CItem &item) const
  {
    return inode == item.inode
        && DevMajor == item.DevMajor
        && DevMinor == item.DevMinor;
  }

  bool IsBin() const { return Type == k_Type_BinLe || Type == k_Type_BinBe; }
  bool IsCrcFormat() const { return Type == k_Type_HexCrc; }
  bool IsDir() const { return MY_LIN_S_ISDIR(Mode); }
  bool Is_SymLink() const { return MY_LIN_S_ISLNK(Mode); }
  bool IsTrailer() const { return strcmp(Name, kName_TRAILER) == 0; }
  UInt64 GetDataPosition() const { return HeaderPos + HeaderSize; }
};


enum EErrorType
{
  k_ErrorType_OK,
  k_ErrorType_BadSignature,
  k_ErrorType_Corrupted,
  k_ErrorType_UnexpectedEnd
};


struct CInArchive
{
  EErrorType errorType;
  ISequentialInStream *Stream;
  UInt64 Processed;
  CItem item;
  
  HRESULT Read(void *data, size_t *size);
  HRESULT GetNextItem();
};

HRESULT CInArchive::Read(void *data, size_t *size)
{
  const HRESULT res = ReadStream(Stream, data, size);
  Processed += *size;
  return res;
}


static bool CheckOctRecord(const Byte *p)
{
  for (unsigned i = 6; i < k_OctRecord_Size; i++)
  {
    const Byte c = p[i];
    if (c < '0' || c > '7')
      return false;
  }
  return true;
}

static bool CheckHexRecord(const Byte *p)
{
  for (unsigned i = 6; i < k_HexRecord_Size; i++)
  {
    const Byte c = p[i];
    if ((c < '0' || c > '9') &&
        (c < 'A' || c > 'F') &&
        (c < 'a' || c > 'f'))
      return false;
  }
  return true;
}

static UInt32 ReadHex(const Byte *p)
{
  char sz[16];
  memcpy(sz, p, 8);
  sz[8] = 0;
  const char *end;
  return ConvertHexStringToUInt32(sz, &end);
}

static UInt32 ReadOct6(const Byte *p)
{
  char sz[16];
  memcpy(sz, p, 6);
  sz[6] = 0;
  const char *end;
  return ConvertOctStringToUInt32(sz, &end);
}

static UInt64 ReadOct11(const Byte *p)
{
  char sz[16];
  memcpy(sz, p, 11);
  sz[11] = 0;
  const char *end;
  return ConvertOctStringToUInt64(sz, &end);
}


#define READ_HEX(    y, dest)  dest = ReadHex  (p + 6 + (y) * 8);
#define READ_OCT_6(  y, dest)  dest = ReadOct6 (p + 6 + (y));
#define READ_OCT_11( y, dest)  dest = ReadOct11(p + 6 + (y));

#define Get32spec(p) (((UInt32)GetUi16(p) << 16) + GetUi16(p + 2))
#define G16(offs, v) v = GetUi16(p + (offs))
#define G32(offs, v) v = Get32spec(p + (offs))

static const unsigned kNameSizeMax = 1 << 12;


API_FUNC_static_IsArc IsArc_Cpio(const Byte *p, size_t size)
{
  if (size < k_BinRecord_Size)
    return k_IsArc_Res_NEED_MORE;

  UInt32 namePos;
  UInt32 nameSize;
  UInt32 mode;
  UInt32 rDevMinor;
  UInt32 rDevMajor = 0;

  if (p[0] == '0')
  {
    if (p[1] != '7' ||
        p[2] != '0' ||
        p[3] != '7' ||
        p[4] != '0')
      return k_IsArc_Res_NO;
    if (p[5] == kMagicOct)
    {
      if (size < k_OctRecord_Size)
        return k_IsArc_Res_NEED_MORE;
      if (!CheckOctRecord(p))
        return k_IsArc_Res_NO;
      READ_OCT_6 (2 * 6, mode)
      READ_OCT_6 (6 * 6, rDevMinor)
      READ_OCT_6 (7 * 6 + 11, nameSize)
      namePos = k_OctRecord_Size;
    }
    else if (p[5] == kMagicHex || p[5] == kMagicHexCrc)
    {
      if (size < k_HexRecord_Size)
        return k_IsArc_Res_NEED_MORE;
      if (!CheckHexRecord(p))
        return k_IsArc_Res_NO;
      READ_HEX (1, mode)
      READ_HEX (9, rDevMajor)
      READ_HEX (10, rDevMinor)
      READ_HEX (11, nameSize)
      namePos = k_HexRecord_Size;
    }
    else
      return k_IsArc_Res_NO;
  }
  else
  {
    if (p[0] == kMagicBin0 && p[1] == kMagicBin1)
    {
      mode = GetUi16(p + 6);
      rDevMinor = GetUi16(p + 14);
      nameSize = GetUi16(p + 20);
    }
    else if (p[0] == kMagicBin1 && p[1] == kMagicBin0)
    {
      mode = GetBe16(p + 6);
      rDevMinor = GetBe16(p + 14);
      nameSize = GetBe16(p + 20);
    }
    else
      return k_IsArc_Res_NO;
    namePos = k_BinRecord_Size;
  }

  if (mode >= (1 << 16))
    return k_IsArc_Res_NO;

  if (rDevMajor != 0 ||
      rDevMinor != 0)
  {
    if (!MY_LIN_S_ISCHR(mode) &&
        !MY_LIN_S_ISBLK(mode))
      return k_IsArc_Res_NO;
  }

  // nameSize must include the null byte
  if (nameSize == 0 || nameSize > kNameSizeMax)
    return k_IsArc_Res_NO;
  {
    unsigned lim = namePos + nameSize - 1;
    if (lim >= size)
      lim = (unsigned)size;
    else if (p[lim] != 0)
      return k_IsArc_Res_NO;
    for (unsigned i = namePos; i < lim; i++)
      if (p[i] == 0)
        return k_IsArc_Res_NO;
  }

  return k_IsArc_Res_YES;
}
}


#define READ_STREAM(_dest_, _size_) \
  { size_t processed = (_size_); RINOK(Read(_dest_, &processed)); \
if (processed != (_size_)) { errorType = k_ErrorType_UnexpectedEnd; return S_OK; } }

HRESULT CInArchive::GetNextItem()
{
  errorType = k_ErrorType_BadSignature;

  Byte p[k_RecordSize_Max];

  READ_STREAM(p, k_BinRecord_Size)

  UInt32 nameSize;
  UInt32 namePos;

  /* we try to reduce probability of false detection,
     so we check some fields for unuxpected values */

  if (p[0] != '0')
  {
         if (p[0] == kMagicBin0 && p[1] == kMagicBin1) { item.Type = k_Type_BinLe; }
    else if (p[0] == kMagicBin1 && p[1] == kMagicBin0)
    {
      for (unsigned i = 2; i < k_BinRecord_Size; i += 2)
      {
        const Byte b = p[i];
        p[i] = p[i + 1];
        p[i + 1] = b;
      }
      item.Type = k_Type_BinBe;
    }
    else
      return S_OK;

    errorType = k_ErrorType_Corrupted;

    item.AlignMask = 2 - 1;
    item.DevMajor = 0;
    item.RDevMajor = 0;
    item.ChkSum = 0;

    G16(2, item.DevMinor);
    G16(4, item.inode);
    G16(6, item.Mode);
    G16(8, item.UID);
    G16(10, item.GID);
    G16(12, item.NumLinks);
    G16(14, item.RDevMinor);
    G32(16, item.MTime);
    G16(20, nameSize);
    G32(22, item.Size);

    namePos = k_BinRecord_Size;
  }
  else
  {
    if (p[1] != '7' ||
        p[2] != '0' ||
        p[3] != '7' ||
        p[4] != '0')
      return S_OK;
    if (p[5] == kMagicOct)
    {
      errorType = k_ErrorType_Corrupted;

      item.Type = k_Type_Oct;
      READ_STREAM(p + k_BinRecord_Size, k_OctRecord_Size - k_BinRecord_Size)
      item.AlignMask = 1 - 1;
      item.DevMajor = 0;
      item.RDevMajor = 0;
      item.ChkSum = 0;

      if (!CheckOctRecord(p))
        return S_OK;

      READ_OCT_6 (0, item.DevMinor)
      READ_OCT_6 (1 * 6, item.inode)
      READ_OCT_6 (2 * 6, item.Mode)
      READ_OCT_6 (3 * 6, item.UID)
      READ_OCT_6 (4 * 6, item.GID)
      READ_OCT_6 (5 * 6, item.NumLinks)
      READ_OCT_6 (6 * 6, item.RDevMinor)
      {
        UInt64 mTime64;
        READ_OCT_11 (7 * 6, mTime64)
        item.MTime = 0;
        if (mTime64 <= (UInt32)(Int32)-1)
          item.MTime = (UInt32)mTime64;
      }
      READ_OCT_6 (7 * 6 + 11, nameSize)
      READ_OCT_11 (8 * 6 + 11, item.Size)  // ?????

      namePos = k_OctRecord_Size;
    }
    else
    {
           if (p[5] == kMagicHex)    item.Type = k_Type_Hex;
      else if (p[5] == kMagicHexCrc) item.Type = k_Type_HexCrc;
      else return S_OK;

      errorType = k_ErrorType_Corrupted;

      READ_STREAM(p + k_BinRecord_Size, k_HexRecord_Size - k_BinRecord_Size)

      if (!CheckHexRecord(p))
        return S_OK;

      item.AlignMask = 4 - 1;
      READ_HEX (0, item.inode)
      READ_HEX (1, item.Mode)
      READ_HEX (2, item.UID)
      READ_HEX (3, item.GID)
      READ_HEX (4, item.NumLinks)
      READ_HEX (5, item.MTime)
      READ_HEX (6, item.Size)
      READ_HEX (7, item.DevMajor)
      READ_HEX (8, item.DevMinor)
      READ_HEX (9, item.RDevMajor)
      READ_HEX (10, item.RDevMinor)
      READ_HEX (11, nameSize)
      READ_HEX (12, item.ChkSum)

      if (item.Type == k_Type_Hex && item.ChkSum != 0)
        return S_OK;

      namePos = k_HexRecord_Size;
    }
  }

  if (item.Mode >= (1 << 16))
    return S_OK;

  if (item.RDevMinor != 0 ||
      item.RDevMajor != 0)
  {
    if (!MY_LIN_S_ISCHR(item.Mode) &&
        !MY_LIN_S_ISBLK(item.Mode))
      return S_OK;
  }

  // Size must be 0 for FIFOs and directories
  if (item.IsDir() || MY_LIN_S_ISFIFO(item.Mode))
    if (item.Size != 0)
      return S_OK;

  // nameSize must include the null byte
  if (nameSize == 0 || nameSize > kNameSizeMax)
    return S_OK;
  item.HeaderSize = item.GetAlignedSize(namePos + nameSize);
  const UInt32 rem = item.HeaderSize - namePos;
  char *s = item.Name.GetBuf(rem);
  size_t processedSize = rem;
  RINOK(Read(s, &processedSize))
  if (processedSize != rem)
  {
    item.Name.ReleaseBuf_SetEnd(0);
    errorType = k_ErrorType_UnexpectedEnd;
    return S_OK;
  }
  bool pad_error = false;
  for (size_t i = nameSize; i < processedSize; i++)
    if (s[i] != 0)
      pad_error = true;
  item.Name.ReleaseBuf_CalcLen(nameSize);
  if (item.Name.Len() + 1 != nameSize || pad_error)
    return S_OK;
  errorType = k_ErrorType_OK;
  return S_OK;
}



Z7_CLASS_IMP_CHandler_IInArchive_1(
  IInArchiveGetStream
)
  CObjectVector<CItem> _items;
  CMyComPtr<IInStream> _stream;
  UInt64 _phySize;
  EType _type;
  EErrorType _error;
  bool _isArc;
  bool _moreThanOneHardLinks_Error;
  bool _numLinks_Error;
  bool _pad_Error;
  bool _symLink_Error;
};

static const Byte kArcProps[] =
{
  kpidSubType
};

static const Byte kProps[] =
{
  kpidPath,
  kpidIsDir,
  kpidSize,
  kpidPackSize,
  kpidMTime,
  kpidPosixAttrib,
  kpidLinks,
  kpidINode,
  kpidUserId,
  kpidGroupId,
  kpidDevMajor,
  kpidDevMinor,
  kpidDeviceMajor,
  kpidDeviceMinor,
  kpidChecksum,
  kpidSymLink,
  kpidStreamId, // for debug
  kpidOffset
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidSubType: prop = k_Types[(unsigned)_type]; break;
    case kpidPhySize: prop = _phySize; break;
    case kpidINode: prop = true; break;
    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc)
        v |= kpv_ErrorFlags_IsNotArc;
      switch (_error)
      {
        case k_ErrorType_UnexpectedEnd: v |= kpv_ErrorFlags_UnexpectedEnd; break;
        case k_ErrorType_Corrupted:     v |= kpv_ErrorFlags_HeadersError; break;
        case k_ErrorType_OK:
        case k_ErrorType_BadSignature:
        // default:
          break;
      }
      prop = v;
      break;
    }
    case kpidWarningFlags:
    {
      UInt32 v = 0;
      if (_moreThanOneHardLinks_Error)
        v |= kpv_ErrorFlags_UnsupportedFeature; // kpv_ErrorFlags_HeadersError
      if (_numLinks_Error
          || _pad_Error
          || _symLink_Error)
        v |= kpv_ErrorFlags_HeadersError;
      if (v != 0)
        prop = v;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


static int CompareItems(const unsigned *p1, const unsigned *p2, void *param)
{
  const CObjectVector<CItem> &items = *(const CObjectVector<CItem> *)param;
  const unsigned index1 = *p1;
  const unsigned index2 = *p2;
  const CItem &i1 = items[index1];
  const CItem &i2 = items[index2];
  if (i1.DevMajor < i2.DevMajor) return -1;
  if (i1.DevMajor > i2.DevMajor) return 1;
  if (i1.DevMinor < i2.DevMinor) return -1;
  if (i1.DevMinor > i2.DevMinor) return 1;
  if (i1.inode < i2.inode) return -1;
  if (i1.inode > i2.inode) return 1;
  if (i1.IsDir())
  {
    if (!i2.IsDir())
      return -1;
  }
  else if (i2.IsDir())
    return 1;
  return MyCompare(index1, index2);
}


Z7_COM7F_IMF(CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *callback))
{
  COM_TRY_BEGIN
  {
    Close();
    
    UInt64 endPos;
    RINOK(InStream_AtBegin_GetSize(stream, endPos))
    if (callback)
    {
      RINOK(callback->SetTotal(NULL, &endPos))
    }

    CInArchive arc;

    arc.Stream = stream;
    arc.Processed = 0;

    for (;;)
    {
      CItem &item = arc.item;
      item.HeaderPos = arc.Processed;
      
      RINOK(arc.GetNextItem())

      _error = arc.errorType;
      
      if (_error != k_ErrorType_OK)
      {
        if (_error == k_ErrorType_BadSignature ||
            _error == k_ErrorType_Corrupted)
          arc.Processed = item.HeaderPos;
        break;
      }
      
      if (_items.IsEmpty())
        _type = item.Type;
      else if (_items.Back().Type != item.Type)
      {
        _error = k_ErrorType_Corrupted;
        arc.Processed = item.HeaderPos;
        break;
      }
      
      if (item.IsTrailer())
        break;
      
      item.MainIndex_ForInode = _items.Size();
      _items.Add(item);

      const UInt64 dataSize = item.GetPackSize();
      arc.Processed += dataSize;
      if (arc.Processed > endPos)
      {
        _error = k_ErrorType_UnexpectedEnd;
        break;
      }
        
      if (item.Is_SymLink() && dataSize <= (1 << 12) && item.Size != 0)
      {
        size_t cur = (size_t)dataSize;
        CByteBuffer buf;
        buf.Alloc(cur);
        RINOK(ReadStream(stream, buf, &cur))
        if (cur != dataSize)
        {
          _error = k_ErrorType_UnexpectedEnd;
          break;
        }
        size_t i;
        
        for (i = (size_t)item.Size; i < dataSize; i++)
          if (buf[i] != 0)
            break;
        if (i != dataSize)
          _pad_Error = true;

        for (i = 0; i < (size_t)item.Size; i++)
          if (buf[i] == 0)
            break;
        if (i != (size_t)item.Size)
          _symLink_Error = true;
        else
          _items.Back().Data.CopyFrom(buf, (size_t)item.Size);
      }
      else if (dataSize != 0)
      {
        UInt64 newPos;
        RINOK(stream->Seek((Int64)dataSize, STREAM_SEEK_CUR, &newPos))
        if (arc.Processed != newPos)
          return E_FAIL;
      }

      if (callback && (_items.Size() & 0xFFF) == 0)
      {
        const UInt64 numFiles = _items.Size();
        RINOK(callback->SetCompleted(&numFiles, &item.HeaderPos))
      }
    }

    _phySize = arc.Processed;
  }

  {
    if (_error != k_ErrorType_OK)
    {
      // we try to reduce probability of false detection
      if (_items.Size() == 0)
        return S_FALSE;
      // bin file uses small signature. So we do additional check for single item case.
      if (_items.Size() == 1 && _items[0].IsBin())
        return S_FALSE;
    }
    else
    {
      // Read tailing zeros.
      // Most of cpio files use 512-bytes aligned zeros
      // rare case: 4K/8K aligment is possible also
      const unsigned kTailSize_MAX = 1 << 9;
      Byte buf[kTailSize_MAX];
      
      unsigned pos = (unsigned)_phySize & (kTailSize_MAX - 1);
      if (pos != 0) // use this check to support 512 bytes alignment only
      for (;;)
      {
        const unsigned rem = kTailSize_MAX - pos;
        size_t processed = rem;
        RINOK(ReadStream(stream, buf + pos, &processed))
        if (processed != rem)
          break;
        for (; pos < kTailSize_MAX && buf[pos] == 0; pos++)
        {}
        if (pos != kTailSize_MAX)
          break;
        _phySize += processed;
        pos = 0;

        //       use break to support 512   bytes alignment zero tail
        // don't use break to support 512*n bytes alignment zero tail
        break;
      }
    }
  }
  
  {
    /* there was such cpio archive example with hard links:
       {
         all hard links (same dev/inode) are stored in neighboring items, and
           (item.Size == 0)  for non last hard link items
           (item.Size != 0)  for     last hard link item
       }
       but here we sort items by (dev/inode) to support cases
       where hard links (same dev/inode) are not stored in neighboring items.

       // note: some cpio files have (numLinks == 0) ??
    */

    CUIntVector indices;
    {
      const unsigned numItems = _items.Size();
      indices.ClearAndSetSize(numItems);
      if (numItems != 0)
      {
        unsigned *vals = &indices[0];
        for (unsigned i = 0; i < numItems; i++)
          vals[i] = i;
        indices.Sort(CompareItems, (void *)&_items);
      }
    }

    /* Note: if cpio archive (maybe incorrect) contains
       more then one non empty streams with identical inode number,
       we want to extract all such data streams too.
       
       So we place items with identical inode to groups:
       all items in group will have same MainIndex_ForInode,
       that is index of last item in group with (Size != 0).
       Another (non last) items in group have (Size == 0).
       If there are another hard links with same inode number
       after (Size != 0) item, we place them to another next group(s).
       
       Check it: maybe we should use single group for items
       with identical inode instead, and ignore some extra data streams ?
    */
    
    for (unsigned i = 0; i < indices.Size();)
    {
      unsigned k;
      {
        const CItem &item_Base = _items[indices[i]];

        if (item_Base.IsDir())
        {
          i++;
          continue;
        }
        
        if (i != 0)
        {
          const CItem &item_Prev = _items[indices[i - 1]];
          if (!item_Prev.IsDir())
            if (item_Base.IsSame_inode_Dev(item_Prev))
              _moreThanOneHardLinks_Error = true;
        }
        
        if (item_Base.Size != 0)
        {
          if (item_Base.NumLinks != 1)
            _numLinks_Error = true;
          i++;
          continue;
        }
        
        for (k = i + 1; k < indices.Size();)
        {
          const CItem &item = _items[indices[k]];
          if (item.IsDir())
            break;
          if (!item.IsSame_inode_Dev(item_Base))
            break;
          k++;
          if (item.Size != 0)
            break;
        }
      }

      const unsigned numLinks = k - i;
      for (;;)
      {
        CItem &item = _items[indices[i]];
        if (item.NumLinks != numLinks)
          _numLinks_Error = true;
        if (++i == k)
          break;
        // if (item.Size == 0)
        item.MainIndex_ForInode = indices[k - 1];
      }
    }
  }

  _isArc = true;
  _stream = stream;

  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::Close())
{
  _items.Clear();
  _stream.Release();
  _phySize = 0;
  _type = k_Type_BinLe;
  _isArc = false;
  _moreThanOneHardLinks_Error = false;
  _numLinks_Error = false;
  _pad_Error = false;
  _symLink_Error = false;
  _error = k_ErrorType_OK;
  return S_OK;
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
      UString res;
      bool needConvert = true;
      #ifdef _WIN32
      // if (
      ConvertUTF8ToUnicode(item.Name, res);
      // )
        needConvert = false;
      #endif
      if (needConvert)
        res = MultiByteToUnicodeString(item.Name, CP_OEMCP);
      prop = NItemName::GetOsPath(res);
      break;
    }
    case kpidIsDir: prop = item.IsDir(); break;

    case kpidSize:
      prop = (UInt64)_items[item.MainIndex_ForInode].Size;
      break;
    
    case kpidPackSize:
      prop = (UInt64)item.GetPackSize();
      break;
    
    case kpidMTime:
    {
      if (item.MTime != 0)
        PropVariant_SetFrom_UnixTime(prop, item.MTime);
      break;
    }
    case kpidPosixAttrib: prop = item.Mode; break;
    case kpidINode: prop = item.inode; break;
    case kpidStreamId:
      if (!item.IsDir())
        prop = (UInt32)item.MainIndex_ForInode;
      break;
    case kpidDevMajor: prop = (UInt32)item.DevMajor; break;
    case kpidDevMinor: prop = (UInt32)item.DevMinor; break;

    case kpidUserId: prop = item.UID; break;
    case kpidGroupId: prop = item.GID; break;

    case kpidSymLink:
      if (item.Is_SymLink() && item.Data.Size() != 0)
      {
        AString s;
        s.SetFrom_CalcLen((const char *)(const void *)(const Byte *)item.Data, (unsigned)item.Data.Size());
        if (s.Len() == item.Data.Size())
        {
          UString u;
          bool needConvert = true;
          #ifdef _WIN32
            // if (
            ConvertUTF8ToUnicode(item.Name, u);
            // )
            needConvert = false;
          #endif
          if (needConvert)
            u = MultiByteToUnicodeString(s, CP_OEMCP);
          prop = u;
        }
      }
      break;

    case kpidLinks: prop = item.NumLinks; break;
    case kpidDeviceMajor:
      // if (item.RDevMajor != 0)
        prop = (UInt32)item.RDevMajor;
      break;
    case kpidDeviceMinor:
      // if (item.RDevMinor != 0)
        prop = (UInt32)item.RDevMinor;
      break;
    case kpidChecksum:
      if (item.IsCrcFormat())
        prop = item.ChkSum;
      break;
    case kpidOffset: prop = item.GetDataPosition(); break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


Z7_CLASS_IMP_NOQIB_1(
  COutStreamWithSum
  , ISequentialOutStream
)
  CMyComPtr<ISequentialOutStream> _stream;
  UInt64 _size;
  UInt32 _crc;
  bool _calculate;
public:
  void SetStream(ISequentialOutStream *stream) { _stream = stream; }
  void ReleaseStream() { _stream.Release(); }
  void Init(bool calculate = true)
  {
    _size = 0;
    _calculate = calculate;
    _crc = 0;
  }
  void EnableCalc(bool calculate) { _calculate = calculate; }
  void InitCRC() { _crc = 0; }
  UInt64 GetSize() const { return _size; }
  UInt32 GetCRC() const { return _crc; }
};

Z7_COM7F_IMF(COutStreamWithSum::Write(const void *data, UInt32 size, UInt32 *processedSize))
{
  HRESULT result = S_OK;
  if (_stream)
    result = _stream->Write(data, size, &size);
  if (_calculate)
  {
    UInt32 crc = 0;
    for (UInt32 i = 0; i < size; i++)
      crc += (UInt32)(((const Byte *)data)[i]);
    _crc += crc;
  }
  if (processedSize)
    *processedSize = size;
  return result;
}

Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _items.Size();
  if (numItems == 0)
    return S_OK;
  UInt64 totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    const UInt32 index = allFilesMode ? i : indices[i];
    const CItem &item2 = _items[index];
    const CItem &item = _items[item2.MainIndex_ForInode];
    totalSize += item.Size;
  }
  RINOK(extractCallback->SetTotal(totalSize))

  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder();
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  CLimitedSequentialInStream *streamSpec = new CLimitedSequentialInStream;
  CMyComPtr<ISequentialInStream> inStream(streamSpec);
  streamSpec->SetStream(_stream);

  COutStreamWithSum *outStreamSumSpec = new COutStreamWithSum;
  CMyComPtr<ISequentialOutStream> outStreamSum(outStreamSumSpec);

  UInt64 total_PackSize = 0;
  UInt64 total_UnpackSize = 0;

  for (i = 0;; i++)
  {
    lps->InSize = total_PackSize;
    lps->OutSize = total_UnpackSize;
    RINOK(lps->SetCur())
    if (i >= numItems)
      break;
    CMyComPtr<ISequentialOutStream> outStream;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    const UInt32 index = allFilesMode ? i : indices[i];
    const CItem &item2 = _items[index];
    const CItem &item = _items[item2.MainIndex_ForInode];
    RINOK(extractCallback->GetStream(index, &outStream, askMode))
    
    total_PackSize += item2.GetPackSize();
    total_UnpackSize += item.Size;

    if (item2.IsDir())
    {
      RINOK(extractCallback->PrepareOperation(askMode))
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
      continue;
    }
    if (!testMode && !outStream)
      continue;
    outStreamSumSpec->Init(item.IsCrcFormat());
    outStreamSumSpec->SetStream(outStream);
    outStream.Release();

    RINOK(extractCallback->PrepareOperation(askMode))
    RINOK(InStream_SeekSet(_stream, item.GetDataPosition()))
    streamSpec->Init(item.Size);
    RINOK(copyCoder->Code(inStream, outStreamSum, NULL, NULL, progress))
    outStreamSumSpec->ReleaseStream();
    Int32 res = NExtract::NOperationResult::kDataError;
    if (copyCoderSpec->TotalSize == item.Size)
    {
      res = NExtract::NOperationResult::kOK;
      if (item.IsCrcFormat() && item.ChkSum != outStreamSumSpec->GetCRC())
        res = NExtract::NOperationResult::kCRCError;
    }
    RINOK(extractCallback->SetOperationResult(res))
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetStream(UInt32 index, ISequentialInStream **stream))
{
  COM_TRY_BEGIN
  const CItem &item2 = _items[index];
  const CItem &item = _items[item2.MainIndex_ForInode];
  return CreateLimitedInStream(_stream, item.GetDataPosition(), item.Size, stream);
  COM_TRY_END
}

static const Byte k_Signature[] = {
    5, '0', '7', '0', '7', '0',
    2, kMagicBin0, kMagicBin1,
    2, kMagicBin1, kMagicBin0 };

REGISTER_ARC_I(
  "Cpio", "cpio", NULL, 0xED,
  k_Signature,
  0,
  NArcInfoFlags::kMultiSignature,
  IsArc_Cpio)

}}

// VhdHandler.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"
#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"

#include "../../Windows/PropVariant.h"

#include "../Common/LimitedStreams.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "HandlerCont.h"

// pointers in all Parse() functions are aligned
#define Get16(p) GetBe16a(p)
#define Get32(p) GetBe32a(p)
#define Get64(p) GetBe64a(p)
#define G32_LE(_offs_, dest) dest = GetUi32a(p + (_offs_))
#define G32(_offs_, dest) dest = Get32(p + (_offs_))
#define G64(_offs_, dest) dest = Get64(p + (_offs_))

using namespace NWindows;

namespace NArchive {
namespace NVhd {

static const unsigned kSignatureSize = 11;
static const Byte kSignature[kSignatureSize] =
  { 'c', 'o', 'n', 'e', 'c', 't', 'i', 'x', 0, 0, 0 };

static const UInt32 kUnusedBlock = 0xFFFFFFFF;

static const UInt32 kDiskType_Fixed = 2;
// static const UInt32 kDiskType_Dynamic = 3;
static const UInt32 kDiskType_Diff = 4;

static const char * const kDiskTypes[] =
{
    "0"
  , "1"
  , "Fixed"
  , "Dynamic"
  , "Differencing"
};

struct CFooter
{
  // UInt32 Features;
  // UInt32 FormatVersion;
  UInt64 DataOffset;
  UInt32 CTime;
  UInt32 CreatorApp;
  UInt32 CreatorVersion;
  UInt32 CreatorHostOS;
  // UInt64 OriginalSize;
  UInt64 CurrentSize;
  // UInt32 DiskGeometry;
  UInt32 Type;
  Byte Id[16];
  Byte SavedState;

  bool IsFixed() const { return Type == kDiskType_Fixed; }
  /*
  UInt32 NumCyls() const { return DiskGeometry >> 16; }
  UInt32 NumHeads() const { return (DiskGeometry >> 8) & 0xFF; }
  UInt32 NumSectorsPerTrack() const { return DiskGeometry & 0xFF; }
  */
  void AddTypeString(AString &s) const;
  bool Parse(const Byte *p);
};

void CFooter::AddTypeString(AString &s) const
{
  if (Type < Z7_ARRAY_SIZE(kDiskTypes))
    s += kDiskTypes[Type];
  else
    s.Add_UInt32(Type);
}

// (p) and (checkSumOffset) are aligned for 4-bytes
static bool CheckBlock(const Byte *p, size_t size, size_t checkSumOffset, size_t zeroOffset)
{
  UInt32 sum = 0;
  size_t i;
  for (i = 0; i < checkSumOffset; i++)
    sum += p[i];
  for (i = checkSumOffset + 4; i < size; i++)
    sum += p[i];
  if (~sum != Get32(p + checkSumOffset))
    return false;
  for (i = zeroOffset; i < size; i++)
    if (p[i] != 0)
      return false;
  return true;
}

static const unsigned kSectorSize_Log = 9;
static const unsigned kSectorSize = 1 << kSectorSize_Log;
static const unsigned kHeaderSize = 512;
static const unsigned kDynSize = 1024;

bool CFooter::Parse(const Byte *p)
{
  if (memcmp(p, kSignature, kSignatureSize) != 0)
    return false;
  // G32(0x08, Features);
  if ((p[11] & 0xfe) != 2) // check supported Features.
    return false;
  // G32(0x0C, FormatVersion);
  // if (*(const UInt32 *)(const void *)(p + 0x0C) != Z7_CONV_BE_TO_NATIVE_CONST32(0x10000)) return false; // FormatVersion (1.0)
  G64(0x10, DataOffset);
  G32(0x18, CTime);
  G32_LE(0x1C, CreatorApp);
  G32(0x20, CreatorVersion);
  G32_LE(0x24, CreatorHostOS);
  // G64(0x28, OriginalSize);
  G64(0x30, CurrentSize);
  // 2040 GB - maximum virtual size of VHD (from some DOCs)
  // 2048 GB - our relaxed limit
  if (CurrentSize > ((UInt64)1 << 41))
    return S_FALSE; // E_NOTIMPL
  // G32(0x38, DiskGeometry);
  G32(0x3C, Type);
  if (Type < kDiskType_Fixed)
    return false;
  if (Type == kDiskType_Fixed)
  {
    if (DataOffset != (UInt64)(Int64)-1)
      return false;
  }
  else
  {
    if (Type > kDiskType_Diff)
      return false;
    // usually (DataOffset == 512)
    if (DataOffset >= ((UInt64)1 << 42))
      return false;
  }
  memcpy(Id, p + 0x44, 16);
  SavedState = p[0x54];
  return CheckBlock(p, kHeaderSize, 0x40, 0x55);
}

struct CParentLocatorEntry
{
  UInt32 Code_Native;
  // UInt32 DataSpace;
  UInt32 DataLen;
  UInt64 DataOffset;

  bool Parse(const Byte *p)
  {
    Code_Native = *(UInt32 *)(void *)(p);
    // G32(0x04, DataSpace);
    G32(0x08, DataLen);
    G64(0x10, DataOffset);
    return Get32(p + 0x0C) == 0; // Reserved
  }
};

struct CDynHeader
{
  // UInt64 DataOffset;
  UInt64 TableOffset;
  // UInt32 HeaderVersion;
  UInt32 NumBlocks;
  unsigned BlockSizeLog;
  // UInt32 ParentTime;
  Byte ParentId[16];
  bool RelativeNameWasUsed;
  UString ParentName;
  UString RelativeParentNameFromLocator;
  CParentLocatorEntry ParentLocators[8];

  bool Parse(const Byte *p);
  UInt32 NumBitMapBytes() const
  {
    const UInt32 step = (UInt32)kSectorSize * kSectorSize * 8;
    const UInt32 numSectors = (((UInt32)1 << BlockSizeLog) + step - 1) / step;
    return numSectors << kSectorSize_Log;
  }
  void Clear()
  {
    RelativeNameWasUsed = false;
    ParentName.Empty();
    RelativeParentNameFromLocator.Empty();
    // NumBlocks = 0; // optional
  }
};

bool CDynHeader::Parse(const Byte *p)
{
  // if (memcmp(p, "cxsparse", 8) != 0)
  if (*(const UInt64 *)(const void *)p != Z7_CONV_BE_TO_NATIVE_CONST64(0x6378737061727365))
    return false;
  // G64(0x08, DataOffset);
  if (*(const UInt64 *)(const void *)(p + 8) != (UInt64)(Int64)-1) // unused DataOffset
    return false;
  G64(0x10, TableOffset);
  // G32(0x18, HeaderVersion);
  // if (*(const UInt32 *)(const void *)(p + 0x18) != Z7_CONV_BE_TO_NATIVE_CONST32(0x10000)) return false; // HeaderVersion (1.0)
  G32(0x1C, NumBlocks);
  {
    const UInt32 blockSize = Get32(p + 0x20);
    unsigned i;
    // some docs mention that 512 KB and 2 MB blocks are possible,
    // but any (blockSize >= 512KB) is not problem for our code.
    for (i = 19 /* kSectorSize_Log */;; i++)
    {
      if (i > 31)
        return false;
      if (((UInt32)1 << i) == blockSize)
        break;
    }
    // if (i != 19 && i != 21) return false;
    BlockSizeLog = i;
  }
  // G32(0x38, ParentTime);
  if (*(const UInt32 *)(const void *)(p + 0x3C)) // reserved
    return false;
  memcpy(ParentId, p + 0x28, 16);
  {
    const unsigned kNameLen = 256;
    wchar_t *s = ParentName.GetBuf(kNameLen);
    size_t i;
    for (i = 0; i < kNameLen; i++)
    {
      const wchar_t c = Get16(p + 0x40 + i * 2);
      if (c == 0)
        break;
      s[i] = c;
    }
    s[i] = 0;
    ParentName.ReleaseBuf_SetLen((unsigned)i);
  }
  for (size_t i = 0; i < 8; i++)
    if (!ParentLocators[i].Parse(p + 0x240 + i * 24))
      return false;
  return CheckBlock(p, kDynSize, 0x24, 0x240 + 8 * 24);
}


Z7_class_CHandler_final: public CHandlerImg
{
  UInt64 _posInArcLimit;
  UInt64 _startOffset;
  UInt64 _phySize;

  CFooter Footer;
  CDynHeader Dyn;
  UInt32 *Bat;
  CByteBuffer BitMap;
  UInt32 BitMapTag;
  UInt32 NumUsedBlocks;
  CMyComPtr<IInStream> ParentStream;
  CHandler *Parent;
  unsigned NumLevels;
  UString _errorMessage;
  // bool _unexpectedEnd;

  void AddErrorMessage(const char *message, const wchar_t *name = NULL);
  void UpdatePhySize(UInt64 value)
  {
    if (_phySize < value)
      _phySize = value;
  }

  HRESULT Seek2(UInt64 offset);
  HRESULT InitAndSeek();
  HRESULT ReadPhy_noCheckLimit(UInt64 offset, void *data, size_t size);
  HRESULT ReadPhy(UInt64 offset, void *data, size_t size);

  bool ThereIsDynamic() const { return !Footer.IsFixed(); }
  bool NeedParent() const { return Footer.Type == kDiskType_Diff; }
  UInt64 GetPackSize() const
    { return ThereIsDynamic() ? ((UInt64)NumUsedBlocks << Dyn.BlockSizeLog) : Footer.CurrentSize; }
  bool AreParentsOK() const
  {
    const CHandler *p = this;
    while (p->NeedParent())
    {
      p = p->Parent;
      if (!p)
        return false;
    }
    return true;
  }

  HRESULT Open3();
  HRESULT Open2(IInStream *stream, IArchiveOpenCallback *openArchiveCallback) Z7_override;
  void CloseAtError() Z7_override;

public:
  Z7_IFACE_COM7_IMP(IInArchive_Img)

  Z7_IFACE_COM7_IMP(IInArchiveGetStream)
  Z7_IFACE_COM7_IMP(ISequentialInStream)

  CHandler();
  ~CHandler() Z7_DESTRUCTOR_override;
};

CHandler::CHandler():
    Bat(NULL),
    Parent(NULL)
    {}

CHandler::~CHandler()
{
  z7_AlignedFree(Bat);
}

void CHandler::AddErrorMessage(const char *message, const wchar_t *name)
{
  if (!_errorMessage.IsEmpty())
    _errorMessage.Add_LF();
  _errorMessage += message;
  if (name)
  {
    _errorMessage += ": ";
    _errorMessage += name;
  }
}

HRESULT CHandler::Seek2(UInt64 offset) { return InStream_SeekSet(Stream, _startOffset + offset); }

HRESULT CHandler::InitAndSeek()
{
  if (ParentStream)
  {
    RINOK(Parent->InitAndSeek())
  }
  _virtPos = _posInArc = 0;
  BitMapTag = kUnusedBlock;
  if (ThereIsDynamic())
    BitMap.Alloc(Dyn.NumBitMapBytes());
  return Seek2(0);
}

HRESULT CHandler::ReadPhy_noCheckLimit(const UInt64 offset, void *data, const size_t size)
{
  if (offset != _posInArc)
  {
    _posInArc = offset;
    RINOK(Seek2(offset))
  }
  HRESULT res = ReadStream_FALSE(Stream, data, size);
  if (res == S_OK)
    _posInArc += size;
  else
    Reset_PosInArc();
  return res;
}

HRESULT CHandler::ReadPhy(const UInt64 offset, void *data, const size_t size)
{
  if (offset > _posInArcLimit || size > _posInArcLimit - offset)
    return S_FALSE;
  return ReadPhy_noCheckLimit(offset, data, size);
}

HRESULT CHandler::Open3()
{
  // Fixed archive uses only footer

  UInt64 startPos;
  RINOK(InStream_GetPos(Stream, startPos))
  _startOffset = startPos;
  UInt64 header[kHeaderSize / 8];
  RINOK(ReadStream_FALSE(Stream, header, kHeaderSize))
  const bool headerIsOK = Footer.Parse((const Byte *)(const void *)header);
  _size = Footer.CurrentSize;

  if (headerIsOK && Footer.IsFixed())
  {
    // fixed archive
    if (startPos < Footer.CurrentSize)
      return S_FALSE;
    _posInArcLimit = Footer.CurrentSize;
    _phySize = Footer.CurrentSize + kHeaderSize;
    _startOffset = startPos - Footer.CurrentSize;
    _posInArc = _phySize;
    return S_OK;
  }

  UInt64 fileSize;
  RINOK(InStream_GetSize_SeekToEnd(Stream, fileSize))
  if (fileSize < kHeaderSize)
    return S_FALSE;

  UInt64 buf64[kDynSize / 8];
  Byte * const buf = (Byte *)(void *)buf64;
  RINOK(InStream_SeekSet(Stream, fileSize - kHeaderSize))
  RINOK(ReadStream_FALSE(Stream, buf, kHeaderSize))

  if (!headerIsOK)
  {
    if (!Footer.Parse(buf))
      return S_FALSE;
    _size = Footer.CurrentSize;
    if (!Footer.IsFixed())
      return S_FALSE; // we can't open Dynamic Archive backward.
    // fixed archive
    _posInArcLimit = Footer.CurrentSize;
    _phySize = Footer.CurrentSize + kHeaderSize;
    if (fileSize < _phySize) // we don't allow negative _startOffset
      return S_FALSE;
    _startOffset = fileSize - _phySize;
    _posInArc = _phySize;
    return S_OK;
  }

  // (headerIsOK && Footer.ThereIsDynamic())

  // if (Footer.CurrentSize > ((UInt64)1 << 41)) return S_FALSE; // E_NOTIMPL

  _phySize = kHeaderSize;
  _posInArc = fileSize - startPos;
  _posInArcLimit = _posInArc;

  bool headerAndFooterAreEqual = false;
  if (memcmp(header, buf, kHeaderSize) == 0)
  {
    headerAndFooterAreEqual = true;
    _posInArcLimit = _posInArc - kHeaderSize;
    _phySize = fileSize - _startOffset;
  }

  RINOK(ReadPhy(Footer.DataOffset, buf, kDynSize))
  if (!Dyn.Parse(buf))
    return S_FALSE;

  UpdatePhySize(Footer.DataOffset + kDynSize);

  for (int i = 0; i < 8; i++)
  {
    const CParentLocatorEntry &locator = Dyn.ParentLocators[i];
    if (locator.DataOffset < ((UInt64)1 << 42) && locator.DataLen < (1u << 30))
      UpdatePhySize(locator.DataOffset + locator.DataLen);
    const unsigned kNameBufSizeMax = 1024;
    if (locator.DataLen <= kNameBufSizeMax &&
        locator.DataOffset < _posInArcLimit &&
        locator.DataOffset + locator.DataLen <= _posInArcLimit)
    {
      if (locator.Code_Native == Z7_CONV_BE_TO_NATIVE_CONST32(0x57327275) && (locator.DataLen & 1) == 0)
      {
        // "W2ru" locator
        // Path is encoded as little-endian UTF-16
        Byte nameBuf[kNameBufSizeMax];
        const size_t len = locator.DataLen >> 1;
        RINOK(ReadPhy(locator.DataOffset, nameBuf, locator.DataLen))
        UString &dest = Dyn.RelativeParentNameFromLocator;
        wchar_t *s = dest.GetBuf((unsigned)len);
        size_t j;
        for (j = 0; j < len; j++)
        {
          const wchar_t c = GetUi16a(nameBuf + j * 2);
          if (c == 0)
            break;
          s[j] = c;
        }
        s[j] = 0;
        dest.ReleaseBuf_SetLen((unsigned)j);
        if (dest[0] == L'.' && (dest[1] == L'\\' /* || dest[1] == L'/' */))
          dest.DeleteFrontal(2);
      }
    }
  }
  
  if (Dyn.NumBlocks >= (UInt32)1 << 31) // actual limit is smaller about (1 << 22)
    return S_FALSE;
  if (Footer.CurrentSize == 0)
  {
    if (Dyn.NumBlocks != 0)
      return S_FALSE;
    UpdatePhySize(Dyn.TableOffset);
  }
  else
  {
    if (((Footer.CurrentSize - 1) >> Dyn.BlockSizeLog) + 1 != Dyn.NumBlocks)
      return S_FALSE;
    // Dyn.NumBlocks != 0
    const size_t numBatBytes = (size_t)Dyn.NumBlocks * 4;
    const size_t numBatBytes_aligned = (numBatBytes + kSectorSize - 1) & ~(size_t)(kSectorSize - 1);
    if (numBatBytes_aligned / 4 < Dyn.NumBlocks)
      return E_OUTOFMEMORY;
    Bat = (UInt32 *)z7_AlignedAlloc(numBatBytes_aligned);
    if (!Bat)
      return E_OUTOFMEMORY;
    
    UpdatePhySize(Dyn.TableOffset + numBatBytes_aligned);

    UInt32 maxBatVal = 0;
    size_t batIndex = 0;
    /*
    Number of blocks is limited, if (blockSize >= 512KB).
    so we read fill BAT as single operation instead of loop:
    */
    // for (;;)
    {
      size_t cur = numBatBytes_aligned - batIndex;
      // if (cur == 0) break; cur = MyMin(cur, (size_t)(4 << 13));
      // (cur % 4) == 0 && cur != 0
      UInt32 *p = (UInt32 *)(void *)((Byte *)(void *)Bat + batIndex);
      RINOK(ReadPhy(Dyn.TableOffset + batIndex, p, cur))
      do
      {
        const UInt32 v = Get32(p);
        *p++ = v;
        if (batIndex < numBatBytes && v != kUnusedBlock)
        {
          if (maxBatVal < v)
              maxBatVal = v;
          NumUsedBlocks++;
        }
        batIndex += 4;
      }
      while (cur -= 4);
    }
    if (NumUsedBlocks)
    {
      const UInt32 blockSize = (UInt32)1 << Dyn.BlockSizeLog;
      UpdatePhySize(((UInt64)maxBatVal << kSectorSize_Log)
          + Dyn.NumBitMapBytes() + blockSize);
    }
  }

  if (headerAndFooterAreEqual)
    return S_OK;

  if (_startOffset + _phySize + kHeaderSize > fileSize)
  {
    // _unexpectedEnd = true;
    _posInArcLimit = _phySize;
    _phySize += kHeaderSize;
    return S_OK;
  }

  RINOK(ReadPhy_noCheckLimit(_phySize, buf, kHeaderSize))
  if (memcmp(header, buf, kHeaderSize) == 0)
  {
    _posInArcLimit = _phySize;
    _phySize += kHeaderSize;
    return S_OK;
  }

  if (_phySize == 0x800)
  {
    /* WHY does empty archive contain additional empty sector?
       We skip that sector and check footer again. */
    size_t i;
    for (i = 0; i < kSectorSize && buf[i] == 0; i++);
    if (i == kSectorSize)
    {
      RINOK(ReadPhy(_phySize + kSectorSize, buf, kHeaderSize))
      if (memcmp(header, buf, kHeaderSize) == 0)
      {
        _phySize += kSectorSize;
        _posInArcLimit = _phySize;
        _phySize += kHeaderSize;
        return S_OK;
      }
    }
  }
  AddErrorMessage("Can't find footer");
  _posInArcLimit = _phySize;
  _phySize += kHeaderSize;
  return S_OK;
}


Z7_COM7F_IMF(CHandler::Read(void *data, UInt32 size, UInt32 *processedSize))
{
  if (processedSize)
    *processedSize = 0;
  if (_virtPos >= Footer.CurrentSize)
    return S_OK;
  {
    const UInt64 rem = Footer.CurrentSize - _virtPos;
    if (size > rem)
      size = (UInt32)rem;
  }
  if (size == 0)
    return S_OK;

  if (Footer.IsFixed())
  {
    if (_virtPos > _posInArcLimit)
      return S_FALSE;
    {
      const UInt64 rem = _posInArcLimit - _virtPos;
      if (size > rem)
        size = (UInt32)rem;
    }
    HRESULT res = S_OK;
    if (_virtPos != _posInArc)
    {
      _posInArc = _virtPos;
      res = Seek2(_virtPos);
    }
    if (res == S_OK)
    {
      UInt32 processedSize2 = 0;
      res = Stream->Read(data, size, &processedSize2);
      if (processedSize)
        *processedSize = processedSize2;
      _posInArc += processedSize2;
      _virtPos += processedSize2;
    }
    if (res != S_OK)
      Reset_PosInArc();
    return res;
  }

  const UInt32 blockIndex = (UInt32)(_virtPos >> Dyn.BlockSizeLog);
  if (blockIndex >= Dyn.NumBlocks)
    return E_FAIL; // it's some unexpected case
  const UInt32 blockSectIndex = ((const UInt32 *)(const void *)Bat)[blockIndex];
  const UInt32 blockSize = (UInt32)1 << Dyn.BlockSizeLog;
  UInt32 offsetInBlock = (UInt32)_virtPos & (blockSize - 1);
  size = MyMin(blockSize - offsetInBlock, size);
  // size != 0
  HRESULT res = S_OK;
  if (blockSectIndex == kUnusedBlock)
  {
    if (ParentStream)
    {
      RINOK(InStream_SeekSet(ParentStream, _virtPos))
      UInt32 processedSize2 = 0;
      res = ParentStream->Read(data, size, &processedSize2);
      size = processedSize2;
    }
    else
      memset(data, 0, size);
  }
  else
  {
    const UInt64 newPos = (UInt64)blockSectIndex << kSectorSize_Log;
    if (BitMapTag != blockIndex)
    {
      RINOK(ReadPhy(newPos, BitMap, BitMap.Size()))
      BitMapTag = blockIndex;
    }
    RINOK(ReadPhy(newPos + BitMap.Size() + offsetInBlock, data, size))
    UInt32 rem = size;
    size = 0;
    while (rem)
    {
      const UInt32 cur = MyMin(0x200 - (offsetInBlock & 0x1FF), rem);
      // 0 < cur <= rem
      const UInt32 bmi = offsetInBlock >> kSectorSize_Log;
      if (((BitMap[bmi >> 3] >> (7 ^ (bmi & 7))) & 1) == 0)
      {
        if (ParentStream)
        {
          res = InStream_SeekSet(ParentStream, _virtPos + size);
          if (res == S_OK)
            res = ReadStream_FALSE(ParentStream, (Byte *)data + size, cur);
          if (res != S_OK)
            break;
        }
        else
        {
          const Byte *p = (const Byte *)data + size;
          UInt32 i = cur;
          do
          {
            if (*p)
            {
              res = S_FALSE;
              break;
            }
            p++;
          }
          while(--i);
        }
      }
      offsetInBlock += cur;
      size += cur;
      rem -= cur;
    }
  }
  if (processedSize)
    *processedSize = size;
  _virtPos += size;
  return res;
}


enum
{
  kpidParent = kpidUserDefined,
  kpidSavedState
  // , kpidParentTime
};

static const CStatProp kArcProps[] =
{
  { NULL, kpidOffset, VT_UI8},
  { NULL, kpidCTime, VT_FILETIME},
  // { "Parent Time", kpidParentTime, VT_FILETIME},
  { NULL, kpidClusterSize, VT_UI4},
  { NULL, kpidMethod, VT_BSTR},
  { NULL, kpidNumVolumes, VT_UI4},
  { NULL, kpidTotalPhySize, VT_UI8},
  { "Parent", kpidParent, VT_BSTR},
  { NULL, kpidCreatorApp, VT_BSTR},
  { NULL, kpidHostOS, VT_BSTR},
  { "Saved State", kpidSavedState, VT_BOOL},
  { NULL, kpidId, VT_BSTR}
 };

static const Byte kProps[] =
{
  kpidSize,
  kpidPackSize,
  kpidCTime
  /*
  { kpidNumCyls, VT_UI4},
  { kpidNumHeads, VT_UI4},
  { kpidSectorsPerTrack, VT_UI4}
  */
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps_WITH_NAME

// VHD start time: 2000-01-01
static const UInt64 kVhdTimeStartValue = (UInt64)3600 * 24 * (399 * 365 + 24 * 4);

static void VhdTimeToFileTime(UInt32 vhdTime, NCOM::CPropVariant &prop)
{
  FILETIME ft, utc;
  const UInt64 v = (kVhdTimeStartValue + vhdTime) * 10000000;
  ft.dwLowDateTime = (DWORD)v;
  ft.dwHighDateTime = (DWORD)(v >> 32);
  // specification says that it's UTC time, but Virtual PC 6 writes local time. Why?
  LocalFileTimeToFileTime(&ft, &utc);
  prop = utc;
}

static void DecodeString(char *dest, UInt32 val)
{
  do
  {
    const Byte b = (Byte)val;
    if (b < 0x20 || b > 0x7F)
      break;
    *dest++ = (char)b;
  }
  while (val >>= 8);
  *dest = 0;
}

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidMainSubfile: prop = (UInt32)0; break;
    case kpidCTime: VhdTimeToFileTime(Footer.CTime, prop); break;
    // case kpidParentTime: if (Footer.ThereIsDynamic()) VhdTimeToFileTime(Dyn.ParentTime, prop); break;
    case kpidClusterSize: if (ThereIsDynamic()) prop = (UInt32)1 << Dyn.BlockSizeLog; break;
    case kpidShortComment:
    case kpidMethod:
    {
      AString s;
      Footer.AddTypeString(s);
      if (NeedParent())
      {
        s += " -> ";
        const CHandler *p = this;
        while (p && p->NeedParent())
          p = p->Parent;
        if (!p)
          s.Add_Char('?');
        else
          p->Footer.AddTypeString(s);
      }
      prop = s;
      break;
    }
    case kpidParent:
      if (NeedParent())
      {
        const CHandler *p = this;
        UString res;
        while (p && p->NeedParent())
        {
          if (!res.IsEmpty())
            res += " -> ";
          const UString *mainName;
          const UString *anotherName;
          if (Dyn.RelativeNameWasUsed)
          {
            mainName = &p->Dyn.RelativeParentNameFromLocator;
            anotherName = &p->Dyn.ParentName;
          }
          else
          {
            mainName = &p->Dyn.ParentName;
            anotherName = &p->Dyn.RelativeParentNameFromLocator;
          }
          res += *mainName;
          if (*mainName != *anotherName && !anotherName->IsEmpty())
          {
            res.Add_Space();
            res.Add_Char('(');
            res += *anotherName;
            res.Add_Char(')');
          }
          p = p->Parent;
        }
        prop = res;
      }
      break;
    case kpidCreatorApp:
    {
      char s[16];
      DecodeString(s, Footer.CreatorApp);
      AString res (s);
      res.TrimRight();
      res.Add_Space();
      res.Add_UInt32(Footer.CreatorVersion >> 16);
      res.Add_Dot();
      res.Add_UInt32(Footer.CreatorVersion & 0xFFFF);
      prop = res;
      break;
    }
    case kpidHostOS:
    {
      char s[16];
      const char *p = "Windows";
      if (Footer.CreatorHostOS != 0x6B326957) // "Wi2k"
      {
        DecodeString(s, Footer.CreatorHostOS);
        p = s;
      }
      prop = p;
      break;
    }
    case kpidId:
    {
      char s[sizeof(Footer.Id) * 2 + 2];
      ConvertDataToHex_Upper(s, Footer.Id, sizeof(Footer.Id));
      prop = s;
      break;
    }
    case kpidSavedState: prop = Footer.SavedState ? true : false; break;
    case kpidOffset: prop = _startOffset; break;
    case kpidPhySize: prop = _phySize; break;
    case kpidTotalPhySize:
    {
      const CHandler *p = this;
      UInt64 sum = 0;
      do
      {
        sum += p->_phySize;
        p = p->Parent;
      }
      while (p);
      prop = sum;
      break;
    }
    case kpidNumVolumes: if (NumLevels != 1) prop = (UInt32)NumLevels; break;
    /*
    case kpidErrorFlags:
    {
      UInt32 flags = 0;
      if (_unexpectedEnd)
        flags |= kpv_ErrorFlags_UnexpectedEndOfArc;
      if (flags != 0)
        prop = flags;
      break;
    }
    */
    case kpidError: if (!_errorMessage.IsEmpty()) prop = _errorMessage; break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


static bool FindString(const UStringVector &list, const UString &s)
{
  FOR_VECTOR (i, list)
    if (s.IsEqualTo_NoCase(list[i]))
      return true;
  return false;
}


HRESULT CHandler::Open2(IInStream * const stream, IArchiveOpenCallback * const openArchiveCallback)
{
  CMyComPtr<IInStream> nextStream = stream;
  CMyComPtr<IArchiveOpenVolumeCallback> openVolumeCallback;
  UStringVector names;
#if WCHAR_PATH_SEPARATOR != L'\\'
#define Z7_VHD_CONVERT_BACKSLASHES
#endif
#ifdef Z7_VHD_CONVERT_BACKSLASHES
  UString temp, temp2;
#endif

  CMyComPtr<IInStream> parentStream;
  CHandler *handler = this;
  CHandler *child = NULL;
  const UString *name = NULL;
  const char *message = NULL;
  bool isCyclic = false;

  for (;;)
  {
    handler->Close();
    handler->Stream = nextStream;

    HRESULT res = handler->Open3();
    if (res != S_OK)
    {
      if (!child)
        return res;
      message = "Parent volume parsing error";
      break;
    }
  
    if (child && memcmp(child->Dyn.ParentId, handler->Footer.Id, 16) != 0)
    {
      message = "Different ID in parent volume";
      break;
    }
    
    NumLevels++;
    if (child)
    {
      child->Parent = handler;
      child->ParentStream = parentStream;
      parentStream.Release();
      // child = NULL; // optional
    }

    if (handler->Footer.Type != kDiskType_Diff)
      break;
    
    bool useRelative = true;
    name = &handler->Dyn.RelativeParentNameFromLocator;
    if (name->IsEmpty())
    {
      useRelative = false;
      name = &handler->Dyn.ParentName;
    }
    handler->Dyn.RelativeNameWasUsed = useRelative;

    if (NumLevels >= (1 << 8))
    {
      message = "Too many parent volumes";
      break;
    }

    if (!openVolumeCallback &&
        (!openArchiveCallback
        || openArchiveCallback->QueryInterface(IID_IArchiveOpenVolumeCallback, (void **)&openVolumeCallback) != S_OK
        || !openVolumeCallback))
    {
      message = "volume open interface is not implemented";
      break;
    }
    
    nextStream.Release();
    if (FindString(names, *name))
    {
      isCyclic = true;
      break;
    }

#ifdef Z7_VHD_CONVERT_BACKSLASHES
    // we convert Windows backslashes to OS path separators.
    temp = *name;
    temp.Replace(L'\\', WCHAR_PATH_SEPARATOR);
    name = &temp;
#endif
    res = openVolumeCallback->GetStream(*name, &nextStream);
    
    if (res == S_FALSE || !nextStream)
    {
      if (useRelative
          && !handler->Dyn.ParentName.IsEmpty()
          && handler->Dyn.ParentName != handler->Dyn.RelativeParentNameFromLocator)
      {
        nextStream.Release();
        if (FindString(names, handler->Dyn.ParentName))
        {
          isCyclic = true;
          break;
        }
        const UString *name2 = &handler->Dyn.ParentName;
#ifdef Z7_VHD_CONVERT_BACKSLASHES
        temp2 = handler->Dyn.ParentName;
        temp2.Replace(L'\\', WCHAR_PATH_SEPARATOR);
        name2 = &temp2;
#endif
        res = openVolumeCallback->GetStream(*name2, &nextStream);
        if (nextStream && res == S_OK)
        {
          name = &handler->Dyn.ParentName;
          handler->Dyn.RelativeNameWasUsed = false;
        }
      }
    }
      
    if (res != S_OK || !nextStream)
    {
      message = res == S_FALSE ? "Missing volume" : "Volume open error";
      break;
    }
    
    names.Add(*name);
    child = handler;
    handler = new CHandler;
    parentStream = handler;
  }

  if (isCyclic)
    message = "cyclic chain to parent volume";
  if (message)
    AddErrorMessage(message, name ? name->Ptr() : NULL);

  return S_OK;
}


void CHandler::CloseAtError()
{
  // CHandlerImg:
  Stream.Release();
  Clear_HandlerImg_Vars();

  _phySize = 0;
  NumLevels = 0;
  z7_AlignedFree(Bat);
  Bat = NULL;
  NumUsedBlocks = 0;
  Parent = NULL;
  ParentStream.Release();
  Dyn.Clear();
  _errorMessage.Empty();
  // _unexpectedEnd = false;
}

Z7_COM7F_IMF(CHandler::Close())
{
  CloseAtError();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 /* index */, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidSize: prop = Footer.CurrentSize; break;
    case kpidPackSize: prop = GetPackSize(); break;
    case kpidCTime: VhdTimeToFileTime(Footer.CTime, prop); break;
    case kpidExtension: prop = (_imgExt ? _imgExt : "img"); break;
    /*
    case kpidNumCyls: prop = Footer.NumCyls(); break;
    case kpidNumHeads: prop = Footer.NumHeads(); break;
    case kpidSectorsPerTrack: prop = Footer.NumSectorsPerTrack(); break;
    */
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::GetStream(UInt32 /* index */, ISequentialInStream **stream))
{
  COM_TRY_BEGIN
  *stream = NULL;
  if (Footer.IsFixed())
    return CreateLimitedInStream(Stream, _startOffset, Footer.CurrentSize, stream);
  if (/* !ThereIsDynamic() || */ !AreParentsOK())
    return S_FALSE;
  CMyComPtr<ISequentialInStream> streamTemp = this;
  RINOK(InitAndSeek())
  *stream = streamTemp.Detach();
  return S_OK;
  COM_TRY_END
}

REGISTER_ARC_I(
  "VHD", "vhd", NULL, 0xDC,
  kSignature,
  0,
  NArcInfoFlags::kUseGlobalOffset,
  NULL)

}}

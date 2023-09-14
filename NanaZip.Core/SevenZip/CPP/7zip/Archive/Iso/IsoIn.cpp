// Archive/IsoIn.cpp

#include "StdAfx.h"

#include "../../../../C/CpuArch.h"

#include "../../../Common/MyException.h"

#include "../../Common/StreamUtils.h"

#include "../HandlerCont.h"

#include "IsoIn.h"

namespace NArchive {
namespace NIso {

struct CUnexpectedEndException {};
struct CHeaderErrorException {};
struct CEndianErrorException {};

static const char * const kMediaTypes[] =
{
    "NoEmul"
  , "1.2M"
  , "1.44M"
  , "2.88M"
  , "HardDisk"
};

bool CBootInitialEntry::Parse(const Byte *p)
{
  Bootable = (p[0] == NBootEntryId::kInitialEntryBootable);
  BootMediaType = p[1];
  LoadSegment = GetUi16(p + 2);
  SystemType = p[4];
  SectorCount = GetUi16(p + 6);
  LoadRBA = GetUi32(p + 8);
  memcpy(VendorSpec, p + 12, 20);
  if (p[5] != 0)
    return false;
  if (p[0] != NBootEntryId::kInitialEntryBootable
      && p[0] != NBootEntryId::kInitialEntryNotBootable)
    return false;
  return true;
}

AString CBootInitialEntry::GetName() const
{
  AString s (Bootable ? "Boot" : "NotBoot");
  s.Add_Minus();
  
  if (BootMediaType < Z7_ARRAY_SIZE(kMediaTypes))
    s += kMediaTypes[BootMediaType];
  else
    s.Add_UInt32(BootMediaType);
  
  if (VendorSpec[0] == 1)
  {
    // "Language and Version Information (IBM)"

    unsigned i;
    for (i = 1; i < sizeof(VendorSpec); i++)
      if (VendorSpec[i] > 0x7F)
        break;
    if (i == sizeof(VendorSpec))
    {
      s.Add_Minus();
      for (i = 1; i < sizeof(VendorSpec); i++)
      {
        char c = (char)VendorSpec[i];
        if (c == 0)
          break;
        if (c == '\\' || c == '/')
          c = '_';
        s += c;
      }
    }
  }

  s += ".img";
  return s;
}

Byte CInArchive::ReadByte()
{
  if (m_BufferPos >= kBlockSize)
    m_BufferPos = 0;
  if (m_BufferPos == 0)
  {
    size_t processed = kBlockSize;
    HRESULT res = ReadStream(_stream, m_Buffer, &processed);
    if (res != S_OK)
      throw CSystemException(res);
    if (processed != kBlockSize)
      throw CUnexpectedEndException();
    UInt64 end = _position + processed;
    if (PhySize < end)
      PhySize = end;
  }
  Byte b = m_Buffer[m_BufferPos++];
  _position++;
  return b;
}

void CInArchive::ReadBytes(Byte *data, UInt32 size)
{
  for (UInt32 i = 0; i < size; i++)
    data[i] = ReadByte();
}

void CInArchive::Skip(size_t size)
{
  while (size-- != 0)
    ReadByte();
}

void CInArchive::SkipZeros(size_t size)
{
  while (size-- != 0)
  {
    Byte b = ReadByte();
    if (b != 0)
      throw CHeaderErrorException();
  }
}

UInt16 CInArchive::ReadUInt16()
{
  Byte b[4];
  ReadBytes(b, 4);
  UInt32 val = 0;
  for (int i = 0; i < 2; i++)
  {
    if (b[i] != b[3 - i])
      IncorrectBigEndian = true;
    val |= ((UInt32)(b[i]) << (8 * i));
  }
  return (UInt16)val;
}

UInt32 CInArchive::ReadUInt32Le()
{
  UInt32 val = 0;
  for (int i = 0; i < 4; i++)
    val |= ((UInt32)(ReadByte()) << (8 * i));
  return val;
}

UInt32 CInArchive::ReadUInt32Be()
{
  UInt32 val = 0;
  for (int i = 0; i < 4; i++)
  {
    val <<= 8;
    val |= ReadByte();
  }
  return val;
}

UInt32 CInArchive::ReadUInt32()
{
  Byte b[8];
  ReadBytes(b, 8);
  UInt32 val = 0;
  for (int i = 0; i < 4; i++)
  {
    if (b[i] != b[7 - i])
      throw CEndianErrorException();
    val |= ((UInt32)(b[i]) << (8 * i));
  }
  return val;
}

UInt32 CInArchive::ReadDigits(int numDigits)
{
  UInt32 res = 0;
  for (int i = 0; i < numDigits; i++)
  {
    Byte b = ReadByte();
    if (b < '0' || b > '9')
    {
      if (b == 0 || b == ' ') // it's bug in some CD's
        b = '0';
      else
        throw CHeaderErrorException();
    }
    UInt32 d = (UInt32)(b - '0');
    res *= 10;
    res += d;
  }
  return res;
}

void CInArchive::ReadDateTime(CDateTime &d)
{
  d.Year = (UInt16)ReadDigits(4);
  d.Month = (Byte)ReadDigits(2);
  d.Day = (Byte)ReadDigits(2);
  d.Hour = (Byte)ReadDigits(2);
  d.Minute = (Byte)ReadDigits(2);
  d.Second = (Byte)ReadDigits(2);
  d.Hundredths = (Byte)ReadDigits(2);
  d.GmtOffset = (signed char)ReadByte();
}

void CInArchive::ReadBootRecordDescriptor(CBootRecordDescriptor &d)
{
  ReadBytes(d.BootSystemId, sizeof(d.BootSystemId));
  ReadBytes(d.BootId, sizeof(d.BootId));
  ReadBytes(d.BootSystemUse, sizeof(d.BootSystemUse));
}

void CInArchive::ReadRecordingDateTime(CRecordingDateTime &t)
{
  t.Year = ReadByte();
  t.Month = ReadByte();
  t.Day = ReadByte();
  t.Hour = ReadByte();
  t.Minute = ReadByte();
  t.Second = ReadByte();
  t.GmtOffset = (signed char)ReadByte();
}

void CInArchive::ReadDirRecord2(CDirRecord &r, Byte len)
{
  r.ExtendedAttributeRecordLen = ReadByte();
  if (r.ExtendedAttributeRecordLen != 0)
    throw CHeaderErrorException();
  r.ExtentLocation = ReadUInt32();
  r.Size = ReadUInt32();
  ReadRecordingDateTime(r.DateTime);
  r.FileFlags = ReadByte();
  r.FileUnitSize = ReadByte();
  r.InterleaveGapSize = ReadByte();
  r.VolSequenceNumber = ReadUInt16();
  Byte idLen = ReadByte();
  r.FileId.Alloc(idLen);
  ReadBytes((Byte *)r.FileId, idLen);
  unsigned padSize = 1 - (idLen & 1);
  
  // SkipZeros(padSize);
  Skip(padSize); // it's bug in some cd's. Must be zeros

  unsigned curPos = 33 + idLen + padSize;
  if (curPos > len)
    throw CHeaderErrorException();
  unsigned rem = len - curPos;
  r.SystemUse.Alloc(rem);
  ReadBytes((Byte *)r.SystemUse, rem);
}

void CInArchive::ReadDirRecord(CDirRecord &r)
{
  Byte len = ReadByte();
  // Some CDs can have incorrect value len = 48 ('0') in VolumeDescriptor.
  // But maybe we must use real "len" for other records.
  len = 34;
  ReadDirRecord2(r, len);
}

void CInArchive::ReadVolumeDescriptor(CVolumeDescriptor &d)
{
  d.VolFlags = ReadByte();
  ReadBytes(d.SystemId, sizeof(d.SystemId));
  ReadBytes(d.VolumeId, sizeof(d.VolumeId));
  SkipZeros(8);
  d.VolumeSpaceSize = ReadUInt32();
  ReadBytes(d.EscapeSequence, sizeof(d.EscapeSequence));
  d.VolumeSetSize = ReadUInt16();
  d.VolumeSequenceNumber = ReadUInt16();
  d.LogicalBlockSize = ReadUInt16();
  d.PathTableSize = ReadUInt32();
  d.LPathTableLocation = ReadUInt32Le();
  d.LOptionalPathTableLocation = ReadUInt32Le();
  d.MPathTableLocation = ReadUInt32Be();
  d.MOptionalPathTableLocation = ReadUInt32Be();
  ReadDirRecord(d.RootDirRecord);
  ReadBytes(d.VolumeSetId, sizeof(d.VolumeSetId));
  ReadBytes(d.PublisherId, sizeof(d.PublisherId));
  ReadBytes(d.DataPreparerId, sizeof(d.DataPreparerId));
  ReadBytes(d.ApplicationId, sizeof(d.ApplicationId));
  ReadBytes(d.CopyrightFileId, sizeof(d.CopyrightFileId));
  ReadBytes(d.AbstractFileId, sizeof(d.AbstractFileId));
  ReadBytes(d.BibFileId, sizeof(d.BibFileId));
  ReadDateTime(d.CTime);
  ReadDateTime(d.MTime);
  ReadDateTime(d.ExpirationTime);
  ReadDateTime(d.EffectiveTime);
  d.FileStructureVersion = ReadByte(); // = 1
  SkipZeros(1);
  ReadBytes(d.ApplicationUse, sizeof(d.ApplicationUse));

  // Most ISO contains zeros in the following field (reserved for future standardization).
  // But some ISO programs write some data to that area.
  // So we disable check for zeros.
  Skip(653); // SkipZeros(653);
}

static const Byte kSig_CD001[5] = { 'C', 'D', '0', '0', '1' };

/*
static const Byte kSig_NSR02[5] = { 'N', 'S', 'R', '0', '2' };
static const Byte kSig_NSR03[5] = { 'N', 'S', 'R', '0', '3' };
static const Byte kSig_BEA01[5] = { 'B', 'E', 'A', '0', '1' };
static const Byte kSig_TEA01[5] = { 'T', 'E', 'A', '0', '1' };
*/

static inline bool CheckSignature(const Byte *sig, const Byte *data)
{
  for (int i = 0; i < 5; i++)
    if (sig[i] != data[i])
      return false;
  return true;
}

void CInArchive::SeekToBlock(UInt32 blockIndex)
{
  const HRESULT res = _stream->Seek(
      (Int64)((UInt64)blockIndex * VolDescs[MainVolDescIndex].LogicalBlockSize),
      STREAM_SEEK_SET, &_position);
  if (res != S_OK)
    throw CSystemException(res);
  m_BufferPos = 0;
}

static const int kNumLevelsMax = 256;

void CInArchive::ReadDir(CDir &d, int level)
{
  if (!d.IsDir())
    return;
  if (level > kNumLevelsMax)
  {
    TooDeepDirs = true;
    return;
  }

  {
    FOR_VECTOR (i, UniqStartLocations)
      if (UniqStartLocations[i] == d.ExtentLocation)
      {
        SelfLinkedDirs = true;
        return;
      }
    UniqStartLocations.Add(d.ExtentLocation);
  }

  SeekToBlock(d.ExtentLocation);
  UInt64 startPos = _position;

  bool firstItem = true;
  for (;;)
  {
    UInt64 offset = _position - startPos;
    if (offset >= d.Size)
      break;
    Byte len = ReadByte();
    if (len == 0)
      continue;
    CDir subItem;
    ReadDirRecord2(subItem, len);
    if (firstItem && level == 0)
      IsSusp = subItem.CheckSusp(SuspSkipSize);
      
    if (!subItem.IsSystemItem())
      d._subItems.Add(subItem);

    firstItem = false;
  }
  FOR_VECTOR (i, d._subItems)
    ReadDir(d._subItems[i], level + 1);

  UniqStartLocations.DeleteBack();
}

void CInArchive::CreateRefs(CDir &d)
{
  if (!d.IsDir())
    return;
  for (unsigned i = 0; i < d._subItems.Size();)
  {
    CRef ref;
    CDir &subItem = d._subItems[i];
    subItem.Parent = &d;
    ref.Dir = &d;
    ref.Index = i++;
    ref.NumExtents = 1;
    ref.TotalSize = subItem.Size;
    if (subItem.IsNonFinalExtent())
    {
      for (;;)
      {
        if (i == d._subItems.Size())
        {
          HeadersError = true;
          break;
        }
        const CDir &next = d._subItems[i];
        if (!subItem.AreMultiPartEqualWith(next))
          break;
        i++;
        ref.NumExtents++;
        ref.TotalSize += next.Size;
        if (!next.IsNonFinalExtent())
          break;
      }
    }
    Refs.Add(ref);
    CreateRefs(subItem);
  }
}

void CInArchive::ReadBootInfo()
{
  if (!_bootIsDefined)
    return;
  HeadersError = true;

  if (memcmp(_bootDesc.BootSystemId, kElToritoSpec, sizeof(_bootDesc.BootSystemId)) != 0)
    return;
  
  UInt32 blockIndex = GetUi32(_bootDesc.BootSystemUse);
  SeekToBlock(blockIndex);
  
  Byte buf[32];
  ReadBytes(buf, 32);

  if (buf[0] != NBootEntryId::kValidationEntry
      || buf[2] != 0
      || buf[3] != 0
      || buf[30] != 0x55
      || buf[31] != 0xAA)
    return;

  {
    UInt32 sum = 0;
    for (unsigned i = 0; i < 32; i += 2)
      sum += GetUi16(buf + i);
    if ((sum & 0xFFFF) != 0)
      return;
    /*
    CBootValidationEntry e;
    e.PlatformId = buf[1];
    memcpy(e.Id, buf + 4, sizeof(e.Id));
    // UInt16 checkSum = GetUi16(p + 28);
    */
  }

  ReadBytes(buf, 32);
  {
    CBootInitialEntry e;
    if (!e.Parse(buf))
      return;
    BootEntries.Add(e);
  }

  bool error = false;
  
  for (;;)
  {
    ReadBytes(buf, 32);
    Byte headerIndicator = buf[0];
    if (headerIndicator != NBootEntryId::kMoreHeaders
        && headerIndicator != NBootEntryId::kFinalHeader)
      break;

    // Section Header
    // Byte platform = p[1];
    unsigned numEntries = GetUi16(buf + 2);
    // id[28]
      
    for (unsigned i = 0; i < numEntries; i++)
    {
      ReadBytes(buf, 32);
      CBootInitialEntry e;
      if (!e.Parse(buf))
      {
        error = true;
        break;
      }
      if (e.BootMediaType & (1 << 5))
      {
        // Section entry extension
        for (unsigned j = 0;; j++)
        {
          ReadBytes(buf, 32);
          if (j > 32 || buf[0] != NBootEntryId::kExtensionIndicator)
          {
            error = true;
            break;
          }
          if ((buf[1] & (1 << 5)) == 0)
            break;
          // info += (buf + 2, 30)
        }
      }
      BootEntries.Add(e);
    }
  
    if (headerIndicator != NBootEntryId::kMoreHeaders)
      break;
  }
    
  HeadersError = error;
}

HRESULT CInArchive::Open2()
{
  _position = 0;
  RINOK(InStream_GetSize_SeekToEnd(_stream, _fileSize))
  if (_fileSize < kStartPos)
    return S_FALSE;
  RINOK(_stream->Seek(kStartPos, STREAM_SEEK_SET, &_position))

  PhySize = _position;
  m_BufferPos = 0;
  // BlockSize = kBlockSize;
  
  for (;;)
  {
    Byte sig[7];
    ReadBytes(sig, 7);
    Byte ver = sig[6];
    
    if (!CheckSignature(kSig_CD001, sig + 1))
    {
      return S_FALSE;
      /*
      if (sig[0] != 0 || ver != 1)
        break;
      if (CheckSignature(kSig_BEA01, sig + 1))
      {
      }
      else if (CheckSignature(kSig_TEA01, sig + 1))
      {
        break;
      }
      else if (CheckSignature(kSig_NSR02, sig + 1))
      {
      }
      else
        break;
      SkipZeros(0x800 - 7);
      continue;
      */
    }
    
    // version = 2 for ISO 9660:1999?
    if (ver > 2)
      return S_FALSE;

    if (sig[0] == NVolDescType::kTerminator)
    {
      break;
      // Skip(0x800 - 7);
      // continue;
    }
    
    switch (sig[0])
    {
      case NVolDescType::kBootRecord:
      {
        _bootIsDefined = true;
        ReadBootRecordDescriptor(_bootDesc);
        break;
      }
      case NVolDescType::kPrimaryVol:
      case NVolDescType::kSupplementaryVol:
      {
        // some ISOs have two PrimaryVols.
        CVolumeDescriptor vd;
        ReadVolumeDescriptor(vd);
        if (sig[0] == NVolDescType::kPrimaryVol)
        {
          // some burners write "Joliet" Escape Sequence to primary volume
          memset(vd.EscapeSequence, 0, sizeof(vd.EscapeSequence));
        }
        VolDescs.Add(vd);
        break;
      }
      default:
        break;
    }
  }
  
  if (VolDescs.IsEmpty())
    return S_FALSE;
  for (MainVolDescIndex = (int)VolDescs.Size() - 1; MainVolDescIndex > 0; MainVolDescIndex--)
    if (VolDescs[MainVolDescIndex].IsJoliet())
      break;
  /* FIXME: some volume can contain Rock Ridge, that is better than
     Joliet volume. So we need some way to detect such case */
  // MainVolDescIndex = 0; // to read primary volume
  const CVolumeDescriptor &vd = VolDescs[MainVolDescIndex];
  if (vd.LogicalBlockSize != kBlockSize)
    return S_FALSE;
  
  IsArc = true;

  (CDirRecord &)_rootDir = vd.RootDirRecord;
  ReadDir(_rootDir, 0);
  CreateRefs(_rootDir);
  ReadBootInfo();

  {
    FOR_VECTOR (i, Refs)
    {
      const CRef &ref = Refs[i];
      for (UInt32 j = 0; j < ref.NumExtents; j++)
      {
        const CDir &item = ref.Dir->_subItems[ref.Index + j];
        if (!item.IsDir() && item.Size != 0)
          UpdatePhySize(item.ExtentLocation, item.Size);
      }
    }
  }
  {
    FOR_VECTOR (i, BootEntries)
    {
      const CBootInitialEntry &be = BootEntries[i];
      UpdatePhySize(be.LoadRBA, GetBootItemSize(i));
    }
  }

  if (PhySize < _fileSize)
  {
    UInt64 rem = _fileSize - PhySize;
    const UInt64 kRemMax = 1 << 21;
    if (rem <= kRemMax)
    {
      RINOK(InStream_SeekSet(_stream, PhySize))
      bool areThereNonZeros = false;
      UInt64 numZeros = 0;
      RINOK(ReadZeroTail(_stream, areThereNonZeros, numZeros, kRemMax))
      if (!areThereNonZeros)
        PhySize += numZeros;
    }
  }

  return S_OK;
}

HRESULT CInArchive::Open(IInStream *inStream)
{
  Clear();
  _stream = inStream;
  try { return Open2(); }
  catch(const CSystemException &e) { return e.ErrorCode; }
  catch(CUnexpectedEndException &) { UnexpectedEnd = true; return S_FALSE; }
  catch(CHeaderErrorException &) { HeadersError = true; return S_FALSE; }
  catch(CEndianErrorException &) { IncorrectBigEndian = true; return S_FALSE; }
}

void CInArchive::Clear()
{
  IsArc = false;
  UnexpectedEnd = false;
  HeadersError = false;
  IncorrectBigEndian = false;
  TooDeepDirs = false;
  SelfLinkedDirs = false;

  UniqStartLocations.Clear();

  Refs.Clear();
  _rootDir.Clear();
  VolDescs.Clear();
  _bootIsDefined = false;
  BootEntries.Clear();
  SuspSkipSize = 0;
  IsSusp = false;
}


UInt64 CInArchive::GetBootItemSize(unsigned index) const
{
  const CBootInitialEntry &be = BootEntries[index];
  UInt64 size = be.GetSize();
       if (be.BootMediaType == NBootMediaType::k1d2Floppy)  size = 1200 << 10;
  else if (be.BootMediaType == NBootMediaType::k1d44Floppy) size = 1440 << 10;
  else if (be.BootMediaType == NBootMediaType::k2d88Floppy) size = 2880 << 10;
  const UInt64 startPos = (UInt64)be.LoadRBA * kBlockSize;
  if (startPos < _fileSize)
  {
    const UInt64 rem = _fileSize - startPos;
    if (rem < size)
      size = rem;
  }
  return size;
}

}}

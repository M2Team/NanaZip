// Archive/IsoIn.h

#ifndef ZIP7_INC_ARCHIVE_ISO_IN_H
#define ZIP7_INC_ARCHIVE_ISO_IN_H

#include "../../../Common/MyCom.h"

#include "../../IStream.h"

#include "IsoHeader.h"
#include "IsoItem.h"

namespace NArchive {
namespace NIso {

struct CDir: public CDirRecord
{
  CDir *Parent;
  CObjectVector<CDir> _subItems;

  void Clear()
  {
    Parent = NULL;
    _subItems.Clear();
  }
  
  AString GetPath(bool checkSusp, unsigned skipSize) const
  {
    AString s;

    unsigned len = 0;
    const CDir *cur = this;

    for (;;)
    {
      unsigned curLen;
      cur->GetNameCur(checkSusp, skipSize, curLen);
      len += curLen;
      cur = cur->Parent;
      if (!cur || !cur->Parent)
        break;
      len++;
    }

    char *p = s.GetBuf_SetEnd(len) + len;
    
    cur = this;
    
    for (;;)
    {
      unsigned curLen;
      const Byte *name = cur->GetNameCur(checkSusp, skipSize, curLen);
      p -= curLen;
      if (curLen != 0)
        memcpy(p, name, curLen);
      cur = cur->Parent;
      if (!cur || !cur->Parent)
        break;
      p--;
      *p = CHAR_PATH_SEPARATOR;
    }
    
    return s;
  }

  void GetPathU(UString &s) const
  {
    s.Empty();
    
    unsigned len = 0;
    const CDir *cur = this;

    for (;;)
    {
      unsigned curLen = (unsigned)(cur->FileId.Size() / 2);
      const Byte *fid = cur->FileId;

      unsigned i;
      for (i = 0; i < curLen; i++)
        if (fid[i * 2] == 0 && fid[i * 2 + 1] == 0)
          break;
      len += i;
      cur = cur->Parent;
      if (!cur || !cur->Parent)
        break;
      len++;
    }

    wchar_t *p = s.GetBuf_SetEnd(len) + len;
    
    cur = this;
    
    for (;;)
    {
      unsigned curLen = (unsigned)(cur->FileId.Size() / 2);
      const Byte *fid = cur->FileId;

      unsigned i;
      for (i = 0; i < curLen; i++)
        if (fid[i * 2] == 0 && fid[i * 2 + 1] == 0)
          break;
      curLen = i;

      p -= curLen;
      for (i = 0; i < curLen; i++)
        p[i] = (wchar_t)(((wchar_t)fid[i * 2] << 8) | fid[i * 2 + 1]);
      cur = cur->Parent;
      if (!cur || !cur->Parent)
        break;
      p--;
      *p = WCHAR_PATH_SEPARATOR;
    }
  }
};

struct CDateTime
{
  UInt16 Year;
  Byte Month;
  Byte Day;
  Byte Hour;
  Byte Minute;
  Byte Second;
  Byte Hundredths;
  signed char GmtOffset; // min intervals from -48 (West) to +52 (East) recorded.
  
  bool NotSpecified() const { return Year == 0 && Month == 0 && Day == 0 &&
      Hour == 0 && Minute == 0 && Second == 0 && GmtOffset == 0; }

  bool GetFileTime(NWindows::NCOM::CPropVariant &prop) const
  {
    UInt64 v;
    const bool res = NWindows::NTime::GetSecondsSince1601(Year, Month, Day, Hour, Minute, Second, v);
    if (res)
    {
      v = (UInt64)((Int64)v - (Int64)((Int32)GmtOffset * 15 * 60));
      v *= 10000000;
      if (Hundredths < 100)
        v += (UInt32)Hundredths * 100000;
      prop.SetAsTimeFrom_Ft64_Prec(v, k_PropVar_TimePrec_Base + 2);
    }
    return res;
  }
};

struct CBootRecordDescriptor
{
  Byte BootSystemId[32];  // a-characters
  Byte BootId[32];        // a-characters
  Byte BootSystemUse[1977];
};

struct CBootValidationEntry
{
  Byte PlatformId;
  Byte Id[24]; // to identify the manufacturer/developer of the CD-ROM.
};

struct CBootInitialEntry
{
  bool Bootable;
  Byte BootMediaType;
  UInt16 LoadSegment;
  /* This is the load segment for the initial boot image. If this
     value is 0 the system will use the traditional segment of 7C0. If this value
     is non-zero the system will use the specified segment. This applies to x86
     architectures only. For "flat" model architectures (such as Motorola) this
     is the address divided by 10. */
  Byte SystemType;    // This must be a copy of byte 5 (System Type) from the
                      // Partition Table found in the boot image.
  UInt16 SectorCount; // This is the number of virtual/emulated sectors the system
                      // will store at Load Segment during the initial boot procedure.
  UInt32 LoadRBA;     // This is the start address of the virtual disk. CDs use
                      // Relative/Logical block addressing.

  Byte VendorSpec[20];

  UInt32 GetSize() const
  {
    // if (BootMediaType == NBootMediaType::k1d44Floppy) (1440 << 10);
    return (UInt32)SectorCount * 512;
  }

  bool Parse(const Byte *p);
  AString GetName() const;
};

struct CVolumeDescriptor
{
  Byte VolFlags;
  Byte SystemId[32]; // a-characters. An identification of a system
                     // which can recognize and act upon the content of the Logical
                     // Sectors with logical Sector Numbers 0 to 15 of the volume.
  Byte VolumeId[32]; // d-characters. An identification of the volume.
  UInt32 VolumeSpaceSize; // the number of Logical Blocks in which the Volume Space of the volume is recorded
  Byte EscapeSequence[32];
  UInt16 VolumeSetSize;
  UInt16 VolumeSequenceNumber; // the ordinal number of the volume in the Volume Set of which the volume is a member.
  UInt16 LogicalBlockSize;
  UInt32 PathTableSize;
  UInt32 LPathTableLocation;
  UInt32 LOptionalPathTableLocation;
  UInt32 MPathTableLocation;
  UInt32 MOptionalPathTableLocation;
  CDirRecord RootDirRecord;
  Byte VolumeSetId[128];
  Byte PublisherId[128];
  Byte DataPreparerId[128];
  Byte ApplicationId[128];
  Byte CopyrightFileId[37];
  Byte AbstractFileId[37];
  Byte BibFileId[37];
  CDateTime CTime;
  CDateTime MTime;
  CDateTime ExpirationTime;
  CDateTime EffectiveTime;
  Byte FileStructureVersion; // = 1;
  Byte ApplicationUse[512];

  bool IsJoliet() const
  {
    if ((VolFlags & 1) != 0)
      return false;
    Byte b = EscapeSequence[2];
    return (EscapeSequence[0] == 0x25 && EscapeSequence[1] == 0x2F &&
      (b == 0x40 || b == 0x43 || b == 0x45));
  }
};

struct CRef
{
  const CDir *Dir;
  UInt32 Index;
  UInt32 NumExtents;
  UInt64 TotalSize;
};

const UInt32 kBlockSize = 1 << 11;

class CInArchive
{
  IInStream *_stream;
  UInt64 _position;

  UInt32 m_BufferPos;
  
  void Skip(size_t size);
  void SkipZeros(size_t size);
  Byte ReadByte();
  void ReadBytes(Byte *data, UInt32 size);
  UInt16 ReadUInt16();
  UInt32 ReadUInt32Le();
  UInt32 ReadUInt32Be();
  UInt32 ReadUInt32();
  UInt64 ReadUInt64();
  UInt32 ReadDigits(int numDigits);
  void ReadDateTime(CDateTime &d);
  void ReadRecordingDateTime(CRecordingDateTime &t);
  void ReadDirRecord2(CDirRecord &r, Byte len);
  void ReadDirRecord(CDirRecord &r);

  void ReadBootRecordDescriptor(CBootRecordDescriptor &d);
  void ReadVolumeDescriptor(CVolumeDescriptor &d);

  void SeekToBlock(UInt32 blockIndex);
  void ReadDir(CDir &d, int level);
  void CreateRefs(CDir &d);

  void ReadBootInfo();
  HRESULT Open2();
public:
  HRESULT Open(IInStream *inStream);
  void Clear();

  UInt64 _fileSize;
  UInt64 PhySize;

  CRecordVector<CRef> Refs;
  CObjectVector<CVolumeDescriptor> VolDescs;
  int MainVolDescIndex;
  // UInt32 BlockSize;
  CObjectVector<CBootInitialEntry> BootEntries;

private:
  bool _bootIsDefined;
public:
  bool IsArc;
  bool UnexpectedEnd;
  bool HeadersError;
  bool IncorrectBigEndian;
  bool TooDeepDirs;
  bool SelfLinkedDirs;
  bool IsSusp;
  unsigned SuspSkipSize;

  CRecordVector<UInt32> UniqStartLocations;

  void UpdatePhySize(const UInt32 blockIndex, const UInt64 size)
  {
    const UInt64 alignedSize = (size + kBlockSize - 1) & ~((UInt64)kBlockSize - 1);
    const UInt64 end = (UInt64)blockIndex * kBlockSize + alignedSize;
    if (PhySize < end)
      PhySize = end;
  }

  bool IsJoliet() const { return VolDescs[MainVolDescIndex].IsJoliet(); }

  UInt64 GetBootItemSize(unsigned index) const;

private:
  CDir _rootDir;
  Byte m_Buffer[kBlockSize];
  CBootRecordDescriptor _bootDesc;
};
  
}}
  
#endif

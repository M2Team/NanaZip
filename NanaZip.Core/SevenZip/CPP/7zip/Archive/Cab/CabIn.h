// Archive/CabIn.h

#ifndef ZIP7_INC_ARCHIVE_CAB_IN_H
#define ZIP7_INC_ARCHIVE_CAB_IN_H

#include "../../../Common/MyBuffer.h"
#include "../../../Common/MyCom.h"

#include "../../Common/InBuffer.h"

#include "CabItem.h"

namespace NArchive {
namespace NCab {

struct COtherArc
{
  AString FileName;
  AString DiskName;
  
  void Clear()
  {
    FileName.Empty();
    DiskName.Empty();
  }
};


struct CArchInfo
{
  Byte VersionMinor; // cabinet file format version, minor
  Byte VersionMajor; // cabinet file format version, major
  UInt32 NumFolders; // number of CFFOLDER entries in this cabinet
  UInt32 NumFiles;   // number of CFFILE entries in this cabinet
  UInt32 Flags;      // cabinet file option indicators
  UInt32 SetID;      // must be the same for all cabinets in a set
  UInt32 CabinetNumber; // number of this cabinet file in a set

  UInt16 PerCabinet_AreaSize; // (optional) size of per-cabinet reserved area
  Byte PerFolder_AreaSize;    // (optional) size of per-folder reserved area
  Byte PerDataBlock_AreaSize; // (optional) size of per-datablock reserved area

  COtherArc PrevArc; // prev link can skip some volumes !!!
  COtherArc NextArc;

  bool ReserveBlockPresent() const { return (Flags & NHeader::NArcFlags::kReservePresent) != 0; }
  bool IsTherePrev() const { return (Flags & NHeader::NArcFlags::kPrevCabinet) != 0; }
  bool IsThereNext() const { return (Flags & NHeader::NArcFlags::kNextCabinet) != 0; }
  Byte GetDataBlockReserveSize() const { return (Byte)(ReserveBlockPresent() ? PerDataBlock_AreaSize : 0); }

  CArchInfo()
  {
    PerCabinet_AreaSize = 0;
    PerFolder_AreaSize = 0;
    PerDataBlock_AreaSize = 0;
  }

  void Clear()
  {
    PerCabinet_AreaSize = 0;
    PerFolder_AreaSize = 0;
    PerDataBlock_AreaSize = 0;

    PrevArc.Clear();
    NextArc.Clear();
  }
};


struct CInArcInfo: public CArchInfo
{
  UInt32 Size; // size of this cabinet file in bytes
  UInt32 FileHeadersOffset; // offset of the first CFFILE entry

  bool Parse(const Byte *p);
};


struct CDatabase
{
  CRecordVector<CFolder> Folders;
  CObjectVector<CItem> Items;
  UInt64 StartPosition;
  CInArcInfo ArcInfo;
  
  void Clear()
  {
    ArcInfo.Clear();
    Folders.Clear();
    Items.Clear();
  }
  
  bool IsTherePrevFolder() const
  {
    FOR_VECTOR (i, Items)
      if (Items[i].ContinuedFromPrev())
        return true;
    return false;
  }
  
  int GetNumberOfNewFolders() const
  {
    int res = (int)Folders.Size();
    if (IsTherePrevFolder())
      res--;
    return res;
  }
};


struct CDatabaseEx: public CDatabase
{
  CMyComPtr<IInStream> Stream;
};


struct CMvItem
{
  unsigned VolumeIndex;
  unsigned ItemIndex;
};


class CMvDatabaseEx
{
  bool AreItemsEqual(unsigned i1, unsigned i2);

public:
  CObjectVector<CDatabaseEx> Volumes;
  CRecordVector<CMvItem> Items;
  CRecordVector<int> StartFolderOfVol; // can be negative
  CRecordVector<unsigned> FolderStartFileIndex;

  int GetFolderIndex(const CMvItem *mvi) const
  {
    const CDatabaseEx &db = Volumes[mvi->VolumeIndex];
    return StartFolderOfVol[mvi->VolumeIndex] +
        db.Items[mvi->ItemIndex].GetFolderIndex(db.Folders.Size());
  }

  void Clear()
  {
    Volumes.Clear();
    Items.Clear();
    StartFolderOfVol.Clear();
    FolderStartFileIndex.Clear();
  }
  
  void FillSortAndShrink();
  bool Check();
};


class CInArchive
{
  CInBufferBase _inBuffer;
  CByteBuffer _tempBuf;

  void Skip(unsigned size);
  void Read(Byte *data, unsigned size);
  void ReadName(AString &s);
  void ReadOtherArc(COtherArc &oa);
  HRESULT Open2(CDatabaseEx &db, const UInt64 *searchHeaderSizeLimit);

public:
  bool IsArc;
  bool ErrorInNames;
  bool UnexpectedEnd;
  bool HeaderError;

  HRESULT Open(CDatabaseEx &db, const UInt64 *searchHeaderSizeLimit);
};
  
}}
  
#endif

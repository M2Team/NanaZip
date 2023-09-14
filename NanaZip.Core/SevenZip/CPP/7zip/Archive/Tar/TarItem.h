// TarItem.h

#ifndef ZIP7_INC_ARCHIVE_TAR_ITEM_H
#define ZIP7_INC_ARCHIVE_TAR_ITEM_H

#include "../../../Common/MyLinux.h"
#include "../../../Common/UTFConvert.h"

#include "TarHeader.h"

namespace NArchive {
namespace NTar {

struct CSparseBlock
{
  UInt64 Offset;
  UInt64 Size;
};


enum EPaxTimeRemoveZeroMode
{
  k_PaxTimeMode_DontRemoveZero,
  k_PaxTimeMode_RemoveZero_if_PureSecondOnly,
  k_PaxTimeMode_RemoveZero_Always
};

struct CTimeOptions
{
  EPaxTimeRemoveZeroMode RemoveZeroMode;
  unsigned NumDigitsMax;

  void Init()
  {
    RemoveZeroMode = k_PaxTimeMode_RemoveZero_if_PureSecondOnly;
    NumDigitsMax = 0;
  }
  CTimeOptions() { Init(); }
};


struct CPaxTime
{
  Int32 NumDigits; // -1 means undefined
  UInt32 Ns;  // it's smaller than 1G. Even if (Sec < 0), larger (Ns) value means newer files.
  Int64 Sec;  // can be negative

  Int64 GetSec() const { return NumDigits != -1 ? Sec : 0; }

  bool IsDefined() const { return NumDigits != -1; }
  // bool IsDefined_And_nonZero() const { return NumDigits != -1 && (Sec != 0 || Ns != 0); }
  
  void Clear()
  {
    NumDigits = -1;
    Ns = 0;
    Sec = 0;
  }
  CPaxTime() { Clear(); }

  /*
  void ReducePrecison(int numDigits)
  {
    // we don't use this->NumDigits here
    if (numDigits > 0)
    {
      if (numDigits >= 9)
        return;
      UInt32 r = 1;
      for (unsigned i = numDigits; i < 9; i++)
        r *= 10;
      Ns /= r;
      Ns *= r;
      return;
    }
    Ns = 0;
    if (numDigits == 0)
      return;
    UInt32 r;
         if (numDigits == -1) r = 60;
    else if (numDigits == -2) r = 60 * 60;
    else if (numDigits == -3) r = 60 * 60 * 24;
    else return;
    Sec /= r;
    Sec *= r;
  }
  */
};


struct CPaxTimes
{
  CPaxTime MTime;
  CPaxTime ATime;
  CPaxTime CTime;
  
  void Clear()
  {
    MTime.Clear();
    ATime.Clear();
    CTime.Clear();
  }

  /*
  void ReducePrecison(int numDigits)
  {
    MTime.ReducePrecison(numDigits);
    CTime.ReducePrecison(numDigits);
    ATime.ReducePrecison(numDigits);
  }
  */
};


struct CItem
{
  UInt64 PackSize;
  UInt64 Size;
  Int64 MTime;

  char LinkFlag;
  bool DeviceMajor_Defined;
  bool DeviceMinor_Defined;

  UInt32 Mode;
  UInt32 UID;
  UInt32 GID;
  UInt32 DeviceMajor;
  UInt32 DeviceMinor;

  AString Name;
  AString LinkName;
  AString User;
  AString Group;

  char Magic[8];

  CPaxTimes PaxTimes;

  CRecordVector<CSparseBlock> SparseBlocks;

  void SetMagic_Posix(bool posixMode)
  {
    memcpy(Magic, posixMode ?
        NFileHeader::NMagic::k_Posix_ustar_00 :
        NFileHeader::NMagic::k_GNU_ustar,
        8);
  }

  bool Is_SymLink()  const { return LinkFlag == NFileHeader::NLinkFlag::kSymLink && (Size == 0); }
  bool Is_HardLink() const { return LinkFlag == NFileHeader::NLinkFlag::kHardLink; }
  bool Is_Sparse()   const { return LinkFlag == NFileHeader::NLinkFlag::kSparse; }
  
  UInt64 Get_UnpackSize() const { return Is_SymLink() ? LinkName.Len() : Size; }
  
  bool Is_PaxExtendedHeader() const
  {
    switch (LinkFlag)
    {
      case NFileHeader::NLinkFlag::kPax:
      case NFileHeader::NLinkFlag::kPax_2:
      case NFileHeader::NLinkFlag::kGlobal:
        return true;
    }
    return false;
  }

  UInt32 Get_Combined_Mode() const
  {
    return (Mode & ~(UInt32)MY_LIN_S_IFMT) | Get_FileTypeMode_from_LinkFlag();
  }

  void Set_LinkFlag_for_File(UInt32 mode)
  {
    char                            lf = NFileHeader::NLinkFlag::kNormal;
         if (MY_LIN_S_ISCHR(mode))  lf = NFileHeader::NLinkFlag::kCharacter;
    else if (MY_LIN_S_ISBLK(mode))  lf = NFileHeader::NLinkFlag::kBlock;
    else if (MY_LIN_S_ISFIFO(mode)) lf = NFileHeader::NLinkFlag::kFIFO;
    // else if (MY_LIN_S_ISDIR(mode))  lf = NFileHeader::NLinkFlag::kDirectory;
    // else if (MY_LIN_S_ISLNK(mode))  lf = NFileHeader::NLinkFlag::kSymLink;
    LinkFlag = lf;
  }
  
  UInt32 Get_FileTypeMode_from_LinkFlag() const
  {
    switch (LinkFlag)
    {
      /*
      case NFileHeader::NLinkFlag::kDirectory:
      case NFileHeader::NLinkFlag::kDumpDir:
        return MY_LIN_S_IFDIR;
      */
      case NFileHeader::NLinkFlag::kSymLink:    return MY_LIN_S_IFLNK;
      case NFileHeader::NLinkFlag::kBlock:      return MY_LIN_S_IFBLK;
      case NFileHeader::NLinkFlag::kCharacter:  return MY_LIN_S_IFCHR;
      case NFileHeader::NLinkFlag::kFIFO:       return MY_LIN_S_IFIFO;
      // case return MY_LIN_S_IFSOCK;
    }

    if (IsDir())
      return MY_LIN_S_IFDIR;
    return MY_LIN_S_IFREG;
  }

  bool IsDir() const
  {
    switch (LinkFlag)
    {
      case NFileHeader::NLinkFlag::kDirectory:
      case NFileHeader::NLinkFlag::kDumpDir:
        return true;
      case NFileHeader::NLinkFlag::kOldNormal:
      case NFileHeader::NLinkFlag::kNormal:
      case NFileHeader::NLinkFlag::kSymLink:
        if (Name.IsEmpty())
          return false;
        // GNU TAR uses last character as directory marker
        // we also do it
        return Name.Back() == '/';
        // return NItemName::HasTailSlash(Name, CP_OEMCP);
    }
    return false;
  }

  bool IsMagic_ustar_5chars() const
  {
    for (unsigned i = 0; i < 5; i++)
      if (Magic[i] != NFileHeader::NMagic::k_GNU_ustar[i])
        return false;
    return true;
  }

  bool IsMagic_Posix_ustar_00() const
  {
    for (unsigned i = 0; i < 8; i++)
      if (Magic[i] != NFileHeader::NMagic::k_Posix_ustar_00[i])
        return false;
    return true;
  }

  bool IsMagic_GNU() const
  {
    for (unsigned i = 0; i < 8; i++)
      if (Magic[i] != NFileHeader::NMagic::k_GNU_ustar[i])
        return false;
    return true;
  }

  UInt64 Get_PackSize_Aligned() const { return (PackSize + 0x1FF) & (~((UInt64)0x1FF)); }

  bool IsThereWarning() const
  {
    // that Header Warning is possible if (Size != 0) for dir item
    return (PackSize < Size) && (LinkFlag == NFileHeader::NLinkFlag::kDirectory);
  }
};



struct CEncodingCharacts
{
  bool IsAscii;
  // bool Oem_Checked;
  // bool Oem_Ok;
  // bool Utf_Checked;
  CUtf8Check UtfCheck;
  
  void Clear()
  {
    IsAscii = true;
    // Oem_Checked = false;
    // Oem_Ok = false;
    // Utf_Checked = false;
    UtfCheck.Clear();
  }

  void Update(const CEncodingCharacts &ec)
  {
    if (!ec.IsAscii)
      IsAscii = false;

    // if (ec.Utf_Checked)
    {
      UtfCheck.Update(ec.UtfCheck);
      // Utf_Checked = true;
    }
  }

  CEncodingCharacts() { Clear(); }
  void Check(const AString &s);
  AString GetCharactsString() const;
};


struct CPaxExtra
{
  AString RecordPath;
  AString RawLines;

  void Clear()
  {
    RecordPath.Empty();
    RawLines.Empty();
  }

  void Print_To_String(AString &s) const
  {
    if (!RecordPath.IsEmpty())
    {
      s += RecordPath;
      s.Add_LF();
    }
    if (!RawLines.IsEmpty())
      s += RawLines;
  }
};


struct CItemEx: public CItem
{
  bool HeaderError;
  
  bool IsSignedChecksum;
  bool Prefix_WasUsed;

  bool Pax_Error;
  bool Pax_Overflow;
  bool pax_path_WasUsed;
  bool pax_link_WasUsed;
  bool pax_size_WasUsed;

  bool MTime_IsBin;
  bool PackSize_IsBin;
  bool Size_IsBin;

  bool LongName_WasUsed;
  bool LongName_WasUsed_2;

  bool LongLink_WasUsed;
  bool LongLink_WasUsed_2;

  // bool Name_CouldBeReduced;
  // bool LinkName_CouldBeReduced;

  UInt64 HeaderPos;
  UInt64 HeaderSize;
  
  UInt64 Num_Pax_Records;
  CPaxExtra PaxExtra;

  CEncodingCharacts EncodingCharacts;

  UInt64 Get_DataPos() const { return HeaderPos + HeaderSize; }
  // UInt64 GetFullSize() const { return HeaderSize + PackSize; }
  UInt64 Get_FullSize_Aligned() const { return HeaderSize + Get_PackSize_Aligned(); }
};

}}

#endif

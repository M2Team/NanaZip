// TarItem.h

#ifndef __ARCHIVE_TAR_ITEM_H
#define __ARCHIVE_TAR_ITEM_H

#include "../../../Common/MyLinux.h"
#include "../../../Common/UTFConvert.h"

#include "../Common/ItemNameUtils.h"

#include "TarHeader.h"

namespace NArchive {
namespace NTar {

struct CSparseBlock
{
  UInt64 Offset;
  UInt64 Size;
};

struct CItem
{
  AString Name;
  UInt64 PackSize;
  UInt64 Size;
  Int64 MTime;

  UInt32 Mode;
  UInt32 UID;
  UInt32 GID;
  UInt32 DeviceMajor;
  UInt32 DeviceMinor;

  AString LinkName;
  AString User;
  AString Group;

  char Magic[8];
  char LinkFlag;
  bool DeviceMajorDefined;
  bool DeviceMinorDefined;

  CRecordVector<CSparseBlock> SparseBlocks;

  bool IsSymLink() const { return LinkFlag == NFileHeader::NLinkFlag::kSymLink && (Size == 0); }
  bool IsHardLink() const { return LinkFlag == NFileHeader::NLinkFlag::kHardLink; }
  bool IsSparse() const { return LinkFlag == NFileHeader::NLinkFlag::kSparse; }
  UInt64 GetUnpackSize() const { return IsSymLink() ? LinkName.Len() : Size; }
  bool IsPaxExtendedHeader() const
  {
    switch (LinkFlag)
    {
      case 'g':
      case 'x':
      case 'X':  // Check it
        return true;
    }
    return false;
  }

  UInt32 Get_Combined_Mode() const
  {
    return (Mode & ~(UInt32)MY_LIN_S_IFMT) | Get_FileTypeMode_from_LinkFlag();
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
      case NFileHeader::NLinkFlag::kSymLink: return MY_LIN_S_IFLNK;
      case NFileHeader::NLinkFlag::kBlock: return MY_LIN_S_IFBLK;
      case NFileHeader::NLinkFlag::kCharacter: return MY_LIN_S_IFCHR;
      case NFileHeader::NLinkFlag::kFIFO: return MY_LIN_S_IFIFO;
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
        return NItemName::HasTailSlash(Name, CP_OEMCP);
    }
    return false;
  }

  bool IsUstarMagic() const
  {
    for (int i = 0; i < 5; i++)
      if (Magic[i] != NFileHeader::NMagic::kUsTar_00[i])
        return false;
    return true;
  }

  UInt64 GetPackSizeAligned() const { return (PackSize + 0x1FF) & (~((UInt64)0x1FF)); }

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



struct CItemEx: public CItem
{
  UInt64 HeaderPos;
  unsigned HeaderSize;
  bool NameCouldBeReduced;
  bool LinkNameCouldBeReduced;

  CEncodingCharacts EncodingCharacts;

  UInt64 GetDataPosition() const { return HeaderPos + HeaderSize; }
  UInt64 GetFullSize() const { return HeaderSize + PackSize; }
};

}}

#endif

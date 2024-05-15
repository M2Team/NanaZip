// Rar5Handler.h

#ifndef ZIP7_INC_RAR5_HANDLER_H
#define ZIP7_INC_RAR5_HANDLER_H

#include "../../../../C/Blake2.h"

#include "../../../Common/MyBuffer.h"

#include "../../../Windows/PropVariant.h"

#include "../../Common/CreateCoder.h"

#include "../IArchive.h"

namespace NArchive {
namespace NRar5 {

namespace NHeaderFlags
{
  const unsigned kExtra   = 1 << 0;
  const unsigned kData    = 1 << 1;
  // const unsigned kUnknown = 1 << 2;
  const unsigned kPrevVol = 1 << 3;
  const unsigned kNextVol = 1 << 4;
  // const unsigned kIsChild = 1 << 5;
  // const unsigned kPreserveChild = 1 << 6;
}
  
namespace NHeaderType
{
  enum
  {
    kArc = 1,
    kFile,
    kService,
    kArcEncrypt,
    kEndOfArc
  };
}

namespace NArcFlags
{
  const unsigned kVol       = 1 << 0;
  const unsigned kVolNumber = 1 << 1;
  const unsigned kSolid     = 1 << 2;
  // const unsigned kRecovery  = 1 << 3;
  // const unsigned kLocked    = 1 << 4;
}

const unsigned kArcExtraRecordType_Locator  = 1;
const unsigned kArcExtraRecordType_Metadata = 2;

namespace NLocatorFlags
{
  const unsigned kQuickOpen  = 1 << 0;
  const unsigned kRecovery   = 1 << 1;
}

namespace NMetadataFlags
{
  const unsigned kArcName   = 1 << 0;
  const unsigned kCTime     = 1 << 1;
  const unsigned kUnixTime  = 1 << 2;
  const unsigned kNanoSec   = 1 << 3;
}

namespace NFileFlags
{
  const unsigned kIsDir    = 1 << 0;
  const unsigned kUnixTime = 1 << 1;
  const unsigned kCrc32    = 1 << 2;
  const unsigned kUnknownSize = 1 << 3;
}

namespace NMethodFlags
{
  // const unsigned kVersionMask = 0x3F;
  const unsigned kSolid = 1 << 6;
  const unsigned kRar5_Compat = 1u << 20;
}

namespace NArcEndFlags
{
  const unsigned kMoreVols = 1 << 0;
}

enum EHostOS
{
  kHost_Windows = 0,
  kHost_Unix
};



// ---------- Extra ----------

namespace NExtraID
{
  enum
  {
    kCrypto = 1,
    kHash,
    kTime,
    kVersion,
    kLink,
    kUnixOwner,
    kSubdata
  };
}

const unsigned kCryptoAlgo_AES = 0;

namespace NCryptoFlags
{
  const unsigned kPswCheck = 1 << 0;
  const unsigned kUseMAC   = 1 << 1;
}

struct CCryptoInfo
{
  UInt64 Algo;
  UInt64 Flags;
  Byte Cnt;

  bool UseMAC()       const { return (Flags & NCryptoFlags::kUseMAC) != 0; }
  bool IsThereCheck() const { return (Flags & NCryptoFlags::kPswCheck) != 0; }
  bool Parse(const Byte *p, size_t size);
};

const unsigned kHashID_Blake2sp = 0;

namespace NTimeRecord
{
  enum
  {
    k_Index_MTime = 0,
    k_Index_CTime,
    k_Index_ATime
  };
  
  namespace NFlags
  {
    const unsigned kUnixTime = 1 << 0;
    const unsigned kMTime    = 1 << 1;
    const unsigned kCTime    = 1 << 2;
    const unsigned kATime    = 1 << 3;
    const unsigned kUnixNs   = 1 << 4;
  }
}

namespace NLinkType
{
  enum
  {
    kUnixSymLink = 1,
    kWinSymLink,
    kWinJunction,
    kHardLink,
    kFileCopy
  };
}

namespace NLinkFlags
{
  const unsigned kTargetIsDir = 1 << 0;
}


struct CLinkInfo
{
  UInt64 Type;
  UInt64 Flags;
  unsigned NameOffset;
  unsigned NameLen;
  
  bool Parse(const Byte *p, unsigned size);
};


struct CItem
{
  UInt32 CommonFlags;
  UInt32 Flags;

  Byte RecordType;
  bool Version_Defined;

  int ACL;

  AString Name;

  unsigned VolIndex;
  int NextItem;        // in _items{}

  UInt32 UnixMTime;
  UInt32 CRC;
  UInt32 Attrib;
  UInt32 Method;

  CByteBuffer Extra;

  UInt64 Size;
  UInt64 PackSize;
  UInt64 HostOS;
  
  UInt64 DataPos;
  UInt64 Version;

  CItem() { Clear(); }

  void Clear()
  {
    // CommonFlags = 0;
    // Flags = 0;
    
    // UnixMTime = 0;
    // CRC = 0;

    VolIndex = 0;
    NextItem = -1;

    Version_Defined = false;
    Version = 0;

    Name.Empty();
    Extra.Free();
    ACL = -1;
  }

  bool IsSplitBefore()  const { return (CommonFlags & NHeaderFlags::kPrevVol) != 0; }
  bool IsSplitAfter()   const { return (CommonFlags & NHeaderFlags::kNextVol) != 0; }
  bool IsSplit()        const { return (CommonFlags & (NHeaderFlags::kPrevVol | NHeaderFlags::kNextVol)) != 0; }

  bool IsDir()          const { return (Flags & NFileFlags::kIsDir) != 0; }
  bool Has_UnixMTime()  const { return (Flags & NFileFlags::kUnixTime) != 0; }
  bool Has_CRC()        const { return (Flags & NFileFlags::kCrc32) != 0; }
  bool Is_UnknownSize() const { return (Flags & NFileFlags::kUnknownSize) != 0; }

  bool IsNextForItem(const CItem &prev) const
  {
    return !IsDir() && !prev.IsDir() && IsSplitBefore() && prev.IsSplitAfter() && (Name == prev.Name);
      // && false;
  }

  // rar docs: Solid flag can be set only for file headers and is never set for service headers.
  bool IsSolid()        const { return ((UInt32)Method & NMethodFlags::kSolid) != 0; }
  bool Is_Rar5_Compat() const { return ((UInt32)Method & NMethodFlags::kRar5_Compat) != 0; }
  unsigned Get_Rar5_CompatBit() const { return ((UInt32)Method >> 20) & 1; }

  unsigned Get_AlgoVersion_RawBits() const { return (unsigned)Method & 0x3F; }
  unsigned Get_AlgoVersion_HuffRev() const
  {
    unsigned w = (unsigned)Method & 0x3F;
    if (w == 1 && Is_Rar5_Compat())
      w = 0;
    return w;
  }
  unsigned Get_Method() const { return ((unsigned)Method >> 7) & 0x7; }

  unsigned Get_DictSize_Main() const
    { return ((UInt32)Method >> 10) & (Get_AlgoVersion_RawBits() == 0 ? 0xf : 0x1f); }
  unsigned Get_DictSize_Frac() const
  {
    // original-unrar ignores Frac, if (algo==0) (rar5):
    if (Get_AlgoVersion_RawBits() == 0)
      return 0;
    return ((UInt32)Method >> 15) & 0x1f;
  }
  UInt64 Get_DictSize64() const
  {
    // ver 6.* check
    // return (((UInt32)Method >> 10) & 0xF);
    UInt64 winSize = 0;
    const unsigned algo = Get_AlgoVersion_RawBits();
    if (algo <= 1)
    {
      UInt32 w = 32;
      if (algo == 1)
        w += Get_DictSize_Frac();
      winSize = (UInt64)w << (12 + Get_DictSize_Main());
    }
    return winSize;
  }


  bool IsService() const { return RecordType == NHeaderType::kService; }
  
  bool Is_STM() const { return IsService() && Name == "STM"; }
  bool Is_CMT() const { return IsService() && Name == "CMT"; }
  bool Is_ACL() const { return IsService() && Name == "ACL"; }
  // bool Is_QO()  const { return IsService() && Name == "QO"; }

  int FindExtra(unsigned extraID, unsigned &recordDataSize) const;
  void PrintInfo(AString &s) const;


  bool IsEncrypted() const
  {
    unsigned size;
    return FindExtra(NExtraID::kCrypto, size) >= 0;
  }

  int FindExtra_Blake() const
  {
    unsigned size = 0;
    const int offset = FindExtra(NExtraID::kHash, size);
    if (offset >= 0
        && size == Z7_BLAKE2S_DIGEST_SIZE + 1
        && Extra[(unsigned)offset] == kHashID_Blake2sp)
      return offset + 1;
    return -1;
  }

  bool FindExtra_Version(UInt64 &version) const;

  bool FindExtra_Link(CLinkInfo &link) const;
  void Link_to_Prop(unsigned linkType, NWindows::NCOM::CPropVariant &prop) const;
  bool Is_CopyLink() const;
  bool Is_HardLink() const;
  bool Is_CopyLink_or_HardLink() const;

  bool NeedUse_as_CopyLink() const { return PackSize == 0 && Is_CopyLink(); }
  bool NeedUse_as_HardLink() const { return PackSize == 0 && Is_HardLink(); }
  bool NeedUse_as_CopyLink_or_HardLink() const { return PackSize == 0 && Is_CopyLink_or_HardLink(); }

  bool GetAltStreamName(AString &name) const;

  UInt32 GetWinAttrib() const
  {
    UInt32 a;
    switch (HostOS)
    {
      case kHost_Windows:
          a = Attrib;
          break;
      case kHost_Unix:
          a = Attrib << 16;
          a |= 0x8000; // add posix mode marker
          break;
      default:
          a = 0;
    }
    if (IsDir()) a |= FILE_ATTRIBUTE_DIRECTORY;
    return a;
  }

  UInt64 GetDataPosition() const { return DataPos; }
};


struct CInArcInfo
{
  UInt64 Flags;
  UInt64 VolNumber;
  UInt64 StartPos;
  UInt64 EndPos;

  UInt64 EndFlags;
  bool EndOfArchive_was_Read;

  bool IsEncrypted;
  bool Locator_Defined;
  bool Locator_Error;
  bool Metadata_Defined;
  bool Metadata_Error;
  bool UnknownExtraRecord;
  bool Extra_Error;
  bool UnsupportedFeature;

  struct CLocator
  {
    UInt64 Flags;
    UInt64 QuickOpen;
    UInt64 Recovery;
    
    bool Is_QuickOpen() const { return (Flags & NLocatorFlags::kQuickOpen) != 0; }
    bool Is_Recovery() const { return (Flags & NLocatorFlags::kRecovery) != 0; }

    bool Parse(const Byte *p, size_t size);
    CLocator():
      Flags(0),
      QuickOpen(0),
      Recovery(0)
      {}
  };

  struct CMetadata
  {
    UInt64 Flags;
    UInt64 CTime;
    AString ArcName;

    bool Parse(const Byte *p, size_t size);
    CMetadata():
      Flags(0),
      CTime(0)
      {}
  };

  CLocator Locator;
  CMetadata Metadata;

  bool ParseExtra(const Byte *p, size_t size);

  CInArcInfo():
    Flags(0),
    VolNumber(0),
    StartPos(0),
    EndPos(0),
    EndFlags(0),
    EndOfArchive_was_Read(false),
    IsEncrypted(false),
    Locator_Defined(false),
    Locator_Error(false),
    Metadata_Defined(false),
    Metadata_Error(false),
    UnknownExtraRecord(false),
    Extra_Error(false),
    UnsupportedFeature(false)
      {}

  /*
  void Clear()
  {
    Flags = 0;
    VolNumber = 0;
    StartPos = 0;
    EndPos = 0;
    EndFlags = 0;
    EndOfArchive_was_Read = false;
  }
  */

  UInt64 GetPhySize() const { return EndPos - StartPos; }

  bool AreMoreVolumes()  const { return (EndFlags & NArcEndFlags::kMoreVols) != 0; }

  bool IsVolume()             const { return (Flags & NArcFlags::kVol) != 0; }
  bool IsSolid()              const { return (Flags & NArcFlags::kSolid) != 0; }
  bool Is_VolNumber_Defined() const { return (Flags & NArcFlags::kVolNumber) != 0; }

  UInt64 GetVolIndex() const { return Is_VolNumber_Defined() ? VolNumber : 0; }
};


struct CRefItem
{
  unsigned Item;   // First item in _items[]
  unsigned Last;   // Last  item in _items[]
  int Parent;      // in _refs[], if alternate stream
  int Link;        // in _refs[]
};


struct CArc
{
  CMyComPtr<IInStream> Stream;
  CInArcInfo Info;
};


class CHandler Z7_final:
  public IInArchive,
  public IArchiveGetRawProps,
  public ISetProperties,
  Z7_PUBLIC_ISetCompressCodecsInfo_IFEC
  public CMyUnknownImp
{
  Z7_COM_QI_BEGIN2(IInArchive)
  Z7_COM_QI_ENTRY(IArchiveGetRawProps)
  Z7_COM_QI_ENTRY(ISetProperties)
  Z7_COM_QI_ENTRY_ISetCompressCodecsInfo_IFEC
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE
  
  Z7_IFACE_COM7_IMP(IInArchive)
  Z7_IFACE_COM7_IMP(IArchiveGetRawProps)
  Z7_IFACE_COM7_IMP(ISetProperties)
  DECL_ISetCompressCodecsInfo

  void InitDefaults();

  bool _isArc;
  bool _needChecksumCheck;
  bool _memUsage_WasSet;
  bool _comment_WasUsedInArc;
  bool _acl_Used;
  bool _error_in_ACL;
  bool _split_Error;
public:
  CRecordVector<CRefItem> _refs;
  CObjectVector<CItem> _items;

  CHandler();
private:
  CObjectVector<CArc> _arcs;
  CObjectVector<CByteBuffer> _acls;

  UInt32 _errorFlags;
  // UInt32 _warningFlags;

  UInt32 _numBlocks;
  unsigned _rar5comapt_mask;
  unsigned _methodMasks[2];
  UInt64 _algo_Mask;
  UInt64 _dictMaxSizes[2];

  CByteBuffer _comment;
  UString _missingVolName;

  UInt64 _memUsage_Decompress;

  DECL_EXTERNAL_CODECS_VARS

  UInt64 GetPackSize(unsigned refIndex) const;
  
  void FillLinks();
  
  HRESULT Open2(IInStream *stream,
      const UInt64 *maxCheckStartPosition,
      IArchiveOpenCallback *openCallback);
};

}}

#endif

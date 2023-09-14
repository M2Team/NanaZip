// PeHandler.cpp

#include "StdAfx.h"

// #include <stdio.h>

#include "../../../C/CpuArch.h"

#include "../../Common/DynamicBuffer.h"
#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/StringConvert.h"

#include "../../Windows/PropVariantUtils.h"
#include "../../Windows/TimeUtils.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)
#define Get64(p) GetUi64(p)

#define G16(offs, v) v = Get16(p + (offs))
#define G32(offs, v) v = Get32(p + (offs))
#define G32_signed(offs, v) v = (Int32)Get32(p + (offs))
#define G64(offs, v) v = Get64(p + (offs))

#define RINOZ(x) { int _tt_ = (x); if (_tt_ != 0) return _tt_; }

using namespace NWindows;

namespace NArchive {
namespace NPe {

static const UInt32 k_Signature32 = 0x00004550;

static HRESULT CalcCheckSum(ISequentialInStream *stream, UInt32 size, UInt32 excludePos, UInt32 &res)
{
  const UInt32 kBufSizeMax = (UInt32)1 << 15;
  UInt32 bufSize = kBufSizeMax;
  CByteBuffer buffer(bufSize);
  Byte *buf = buffer;
  UInt32 sum = 0;
  UInt32 pos = 0;
  for (;;)
  {
    UInt32 rem = size - pos;
    if (rem > bufSize)
      rem = bufSize;
    if (rem == 0)
      break;
    size_t processed = rem;
    RINOK(ReadStream(stream, buf, &processed))
    
    for (unsigned j = 0; j < 4; j++)
    {
      UInt32 e = excludePos + j;
      if (pos <= e)
      {
        e -= pos;
        if (e < processed)
          buf[e] = 0;
      }
    }

    const unsigned kStep = (1 << 4);
    {
      for (size_t i = processed; (i & (kStep - 1)) != 0; i++)
        buf[i] = 0;
    }
    {
      const Byte *buf2 = buf;
      const Byte *bufLimit = buf + processed;
      UInt64 sum2 = 0;
      for (; buf2 < bufLimit; buf2 += kStep)
      {
        UInt64 sum3 = (UInt64)Get32(buf2)
            + Get32(buf2 + 4)
            + Get32(buf2 + 8)
            + Get32(buf2 + 12);
        sum2 += sum3;
      }
      sum2 = (UInt32)(sum2) + (UInt64)(sum2 >> 32);
      UInt32 sum3 = ((UInt32)sum2 + (UInt32)(sum2 >> 32));
      sum += (sum3 & 0xFFFF) + (sum3 >> 16);
      sum = (sum & 0xFFFF) + (sum >> 16);
      sum = (sum & 0xFFFF) + (sum >> 16);
    }

    pos += (UInt32)processed;
    if (rem != processed)
      break;
  }
  res = sum + pos;
  return S_OK;
}


struct CVersion
{
  UInt16 Major;
  UInt16 Minor;

  void Parse(const Byte *p)
  {
    G16(0, Major);
    G16(2, Minor);
  }
  void ToProp(NCOM::CPropVariant &prop);
};

void CVersion::ToProp(NCOM::CPropVariant &prop)
{
  char sz[32];
  ConvertUInt32ToString(Major, sz);
  unsigned len = MyStringLen(sz);
  sz[len] = '.';
  ConvertUInt32ToString(Minor, sz + len + 1);
  prop = sz;
}

static const unsigned kCoffHeaderSize = 20;
static const unsigned kPeHeaderSize = 4 + kCoffHeaderSize;
static const unsigned k_OptHeader32_Size_MIN = 96;
static const unsigned k_OptHeader64_Size_MIN = 112;

static const UInt32 PE_IMAGE_FILE_DLL  = (1 << 13);

struct CHeader
{
  UInt16 Machine;
  UInt16 NumSections;
  UInt32 Time;
  UInt32 PointerToSymbolTable;
  UInt32 NumSymbols;
  UInt16 OptHeaderSize;
  UInt16 Flags;

  void ParseBase(const Byte *p);
  bool ParseCoff(const Byte *p);
  bool ParsePe(const Byte *p);
  bool IsDll() const { return (Flags & PE_IMAGE_FILE_DLL) != 0; }
};

void CHeader::ParseBase(const Byte *p)
{
  G16( 0, Machine);
  G16( 2, NumSections);
  G32( 4, Time);
  G32( 8, PointerToSymbolTable);
  G32(12, NumSymbols);
  G16(16, OptHeaderSize);
  G16(18, Flags);
}

bool CHeader::ParsePe(const Byte *p)
{
  if (Get32(p) != k_Signature32)
    return false;
  ParseBase(p + 4);
  return OptHeaderSize >= k_OptHeader32_Size_MIN;
}

struct CDirLink
{
  UInt32 Va;
  UInt32 Size;
  
  CDirLink(): Va(0), Size(0) {}
  void Parse(const Byte *p)
  {
    G32(0, Va);
    G32(4, Size);
  }
};

enum
{
  kDirLink_Certificate = 4,
  kDirLink_Debug = 6
};

static const UInt32 kNumDirItemsMax = 16;

struct CDebugEntry
{
  UInt32 Flags;
  UInt32 Time;
  CVersion Ver;
  UInt32 Type;
  UInt32 Size;
  UInt32 Va;
  UInt32 Pa;
  
  void Parse(const Byte *p)
  {
    G32(0, Flags);
    G32(4, Time);
    Ver.Parse(p + 8);
    G32(12, Type);
    G32(16, Size);
    G32(20, Va);
    G32(24, Pa);
  }
};

static const UInt32 k_CheckSum_Field_Offset = 64;

static const UInt32 PE_OptHeader_Magic_32 = 0x10B;
static const UInt32 PE_OptHeader_Magic_64 = 0x20B;

static const UInt32 k_SubSystems_EFI_First = 10;
static const UInt32 k_SubSystems_EFI_Last = 13;

struct COptHeader
{
  UInt16 Magic;
  Byte LinkerVerMajor;
  Byte LinkerVerMinor;

  UInt32 CodeSize;
  UInt32 InitDataSize;
  UInt32 UninitDataSize;
  
  // UInt32 AddressOfEntryPoint;
  // UInt32 BaseOfCode;
  // UInt32 BaseOfData32;
  UInt64 ImageBase;

  UInt32 SectAlign;
  UInt32 FileAlign;

  CVersion OsVer;
  CVersion ImageVer;
  CVersion SubsysVer;
  
  UInt32 ImageSize;
  UInt32 HeadersSize;
  UInt32 CheckSum;
  UInt16 SubSystem;
  UInt16 DllCharacts;

  UInt64 StackReserve;
  UInt64 StackCommit;
  UInt64 HeapReserve;
  UInt64 HeapCommit;

  UInt32 NumDirItems;
  CDirLink DirItems[kNumDirItemsMax];

  bool Is64Bit() const { return Magic == PE_OptHeader_Magic_64; }
  bool Parse(const Byte *p, UInt32 size);

  int GetNumFileAlignBits() const
  {
    for (unsigned i = 0; i < 32; i++)
      if (((UInt32)1 << i) == FileAlign)
        return (int)i;
    return -1;
  }

  bool IsSybSystem_EFI() const
  {
    return
        SubSystem >= k_SubSystems_EFI_First &&
        SubSystem <= k_SubSystems_EFI_Last;
  }
};

bool COptHeader::Parse(const Byte *p, UInt32 size)
{
  if (size < k_OptHeader32_Size_MIN)
    return false;
  Magic = Get16(p);
  switch (Magic)
  {
    case PE_OptHeader_Magic_32:
    case PE_OptHeader_Magic_64:
      break;
    default:
      return false;
  }
  LinkerVerMajor = p[2];
  LinkerVerMinor = p[3];
  
  G32( 4, CodeSize);
  G32( 8, InitDataSize);
  G32(12, UninitDataSize);
  // G32(16, AddressOfEntryPoint);
  // G32(20, BaseOfCode);
  
  G32(32, SectAlign);
  G32(36, FileAlign);

  OsVer.Parse(p + 40);
  ImageVer.Parse(p + 44);
  SubsysVer.Parse(p + 48);

  // reserved = Get32(p + 52);

  G32(56, ImageSize);
  G32(60, HeadersSize);
  G32(64, CheckSum);
  G16(68, SubSystem);
  G16(70, DllCharacts);

  UInt32 pos;
  if (Is64Bit())
  {
    if (size < k_OptHeader64_Size_MIN)
      return false;
    // BaseOfData32 = 0;
    G64(24, ImageBase);
    G64(72, StackReserve);
    G64(80, StackCommit);
    G64(88, HeapReserve);
    G64(96, HeapCommit);
    pos = 108;
  }
  else
  {
    // G32(24, BaseOfData32);
    G32(28, ImageBase);
    G32(72, StackReserve);
    G32(76, StackCommit);
    G32(80, HeapReserve);
    G32(84, HeapCommit);
    pos = 92;
  }

  G32(pos, NumDirItems);
  if (NumDirItems > (1 << 16))
    return false;
  pos += 4;
  if (pos + 8 * NumDirItems > size)
    return false;
  memset((void *)DirItems, 0, sizeof(DirItems));
  for (UInt32 i = 0; i < NumDirItems && i < kNumDirItemsMax; i++)
    DirItems[i].Parse(p + pos + i * 8);
  return true;
}

static const UInt32 kSectionSize = 40;

struct CSection
{
  AString Name;

  UInt32 VSize;
  UInt32 Va;
  UInt32 PSize;
  UInt32 Pa;
  UInt32 Flags;
  UInt32 Time;
  // UInt16 NumRelocs;
  bool IsRealSect;
  bool IsDebug;
  bool IsAdditionalSection;

  CSection(): IsRealSect(false), IsDebug(false), IsAdditionalSection(false) {}

  UInt32 GetSizeExtract() const { return PSize; }
  UInt32 GetSizeMin() const { return MyMin(PSize, VSize); }

  void UpdateTotalSize(UInt32 &totalSize) const
  {
    UInt32 t = Pa + PSize;
    if (totalSize < t)
      totalSize = t;
  }
  
  void Parse(const Byte *p);

  int Compare(const CSection &s) const
  {
    RINOZ(MyCompare(Pa, s.Pa))
    UInt32 size1 = GetSizeExtract();
    UInt32 size2 = s.GetSizeExtract();
    return MyCompare(size1, size2);
  }
};

static const unsigned kNameSize = 8;

static void GetName(const Byte *name, AString &res)
{
  res.SetFrom_CalcLen((const char *)name, kNameSize);
}

void CSection::Parse(const Byte *p)
{
  GetName(p, Name);
  G32( 8, VSize);
  G32(12, Va);
  G32(16, PSize);
  G32(20, Pa);
  // G16(32, NumRelocs);
  G32(36, Flags);
}



// IMAGE_FILE_*

static const CUInt32PCharPair g_HeaderCharacts[] =
{
  {  1, "Executable" },
  { 13, "DLL" },
  {  8, "32-bit" },
  {  5, "LargeAddress" },
  {  0, "NoRelocs" },
  {  2, "NoLineNums" },
  {  3, "NoLocalSyms" },
  {  4, "AggressiveWsTrim" },
  {  9, "NoDebugInfo" },
  { 10, "RemovableRun" },
  { 11, "NetRun" },
  { 12, "System" },
  { 14, "UniCPU" },
  {  7, "Little-Endian" },
  { 15, "Big-Endian" }
};

// IMAGE_DLLCHARACTERISTICS_*

static const char * const g_DllCharacts[] =
{
    NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , "HighEntropyVA"
  , "Relocated"
  , "Integrity"
  , "NX-Compatible"
  , "NoIsolation"
  , "NoSEH"
  , "NoBind"
  , "AppContainer"
  , "WDM"
  , "GuardCF"
  , "TerminalServerAware"
};


// IMAGE_SCN_* constants:

static const char * const g_SectFlags[] =
{
    NULL
  , NULL
  , NULL
  , "NoPad"
  , NULL
  , "Code"
  , "InitializedData"
  , "UninitializedData"
  , "Other"
  , "Comments"
  , NULL // OVER
  , "Remove"
  , "COMDAT"
  , NULL
  , "NO_DEFER_SPEC_EXC"
  , "GP" // MEM_FARDATA
  , NULL // SYSHEAP
  , "PURGEABLE" // 16BIT
  , "LOCKED"
  , "PRELOAD"
  , NULL
  , NULL
  , NULL
  , NULL
  , "ExtendedRelocations"
  , "Discardable"
  , "NotCached"
  , "NotPaged"
  , "Shared"
  , "Execute"
  , "Read"
  , "Write"
};

static const CUInt32PCharPair g_MachinePairs[] =
{
  { 0x014C, "x86" },
  { 0x014D, "I860" },
  { 0x0162, "MIPS-R3000" },
  { 0x0166, "MIPS-R4000" },
  { 0x0168, "MIPS-R10000" },
  { 0x0169, "MIPS-V2" },
  { 0x0184, "Alpha" },
  { 0x01A2, "SH3" },
  { 0x01A3, "SH3-DSP" },
  { 0x01A4, "SH3E" },
  { 0x01A6, "SH4" },
  { 0x01A8, "SH5" },
  { 0x01C0, "ARM" },
  { 0x01C2, "ARM-Thumb" },
  { 0x01C4, "ARM-NT" },
  { 0x01D3, "AM33" },
  { 0x01F0, "PPC" },
  { 0x01F1, "PPC-FP" },
  { 0x0200, "IA-64" },
  { 0x0266, "MIPS-16" },
  { 0x0284, "Alpha-64" },
  { 0x0366, "MIPS-FPU" },
  { 0x0466, "MIPS-FPU16" },
  { 0x0520, "TriCore" },
  { 0x0CEF, "CEF" },
  { 0x0EBC, "EFI" },
  { 0x8664, "x64" },
  { 0x9041, "M32R" },
  { 0xAA64, "ARM64" },
  { 0xC0EE, "CEE" }
};

static const char * const g_SubSystems[] =
{
    "Unknown"
  , "Native"
  , "Windows GUI"
  , "Windows CUI"
  , NULL // "Old Windows CE"
  , "OS2"
  , NULL
  , "Posix"
  , "Win9x"
  , "Windows CE"
  , "EFI"
  , "EFI Boot"
  , "EFI Runtime"
  , "EFI ROM"
  , "XBOX"
  , NULL
  , "Windows Boot"
  , "XBOX Catalog" // 17
};

static const char * const g_ResTypes[] =
{
    NULL
  , "CURSOR"
  , "BITMAP"
  , "ICON"
  , "MENU"
  , "DIALOG"
  , "STRING"
  , "FONTDIR"
  , "FONT"
  , "ACCELERATOR"
  , "RCDATA"
  , "MESSAGETABLE"
  , "GROUP_CURSOR"
  , NULL
  , "GROUP_ICON"
  , NULL
  , "VERSION"
  , "DLGINCLUDE"
  , NULL
  , "PLUGPLAY"
  , "VXD"
  , "ANICURSOR"
  , "ANIICON"
  , "HTML"
  , "MANIFEST"
};

static const UInt32 kFlag = (UInt32)1 << 31;
static const UInt32 kMask = ~kFlag;

struct CTableItem
{
  UInt32 Offset;
  UInt32 ID;
};


static const UInt32 kBmpHeaderSize = 14;
static const UInt32 kIconHeaderSize = 22;

struct CResItem
{
  UInt32 Type;
  UInt32 ID;
  UInt32 Lang;

  UInt32 Size;
  UInt32 Offset;

  UInt32 HeaderSize;
  Byte Header[kIconHeaderSize]; // it must be enough for max size header.
  bool Enabled;

  bool IsNameEqual(const CResItem &item) const { return Lang == item.Lang; }
  UInt32 GetSize() const { return Size + HeaderSize; }
  bool IsBmp() const { return Type == 2; }
  bool IsIcon() const { return Type == 3; }
  bool IsString() const { return Type == 6; }
  bool IsRcData() const { return Type == 10; }
  bool IsVersion() const { return Type == 16; }
  bool IsRcDataOrUnknown() const { return IsRcData() || Type > 64; }
};

struct CTextFile
{
  CByteDynamicBuffer Buf;

  size_t FinalSize() const { return Buf.GetPos(); }

  void AddChar(char c);
  void AddWChar(UInt16 c);
  void AddWChar_Smart(UInt16 c);
  void NewLine();
  void AddString(const char *s);
  void AddSpaces(int num);
  void AddBytes(const Byte *p, size_t size)
  {
    Buf.AddData(p, size);
  }
  
  void OpenBlock(int num)
  {
    AddSpaces(num);
    AddChar('{');
    NewLine();
  }
  void CloseBlock(int num)
  {
    AddSpaces(num);
    AddChar('}');
    NewLine();
  }
};

void CTextFile::AddChar(char c)
{
  Byte *p = Buf.GetCurPtrAndGrow(2);
  p[0] = (Byte)c;
  p[1] = 0;
}

void CTextFile::AddWChar(UInt16 c)
{
  Byte *p = Buf.GetCurPtrAndGrow(2);
  SetUi16(p, c)
}

void CTextFile::AddWChar_Smart(UInt16 c)
{
  if (c == '\n')
  {
    AddChar('\\');
    c = 'n';
  }
  AddWChar(c);
}

void CTextFile::NewLine()
{
  AddChar(0x0D);
  AddChar(0x0A);
}

void CTextFile::AddString(const char *s)
{
  for (;; s++)
  {
    char c = *s;
    if (c == 0)
      return;
    AddChar(c);
  }
}

void CTextFile::AddSpaces(int num)
{
  for (int i = 0; i < num; i++)
    AddChar(' ');
}

struct CStringItem: public CTextFile
{
  UInt32 Lang;
};

struct CByteBuffer_WithLang: public CByteBuffer
{
  UInt32 Lang;
};


struct CMixItem
{
  int SectionIndex;
  int ResourceIndex;
  int StringIndex;
  int VersionIndex;

  CMixItem(): SectionIndex(-1), ResourceIndex(-1), StringIndex(-1), VersionIndex(-1) {}
  bool IsSectionItem() const { return ResourceIndex < 0 && StringIndex < 0 && VersionIndex < 0; }
};

struct CUsedBitmap
{
  CByteBuffer Buf;
public:
  void Alloc(size_t size)
  {
    size = (size + 7) / 8;
    Buf.Alloc(size);
    memset(Buf, 0, size);
  }
  
  void Free()
  {
    Buf.Free();
  }
  
  bool SetRange(size_t from, unsigned size)
  {
    for (unsigned i = 0; i < size; i++)
    {
      size_t pos = (from + i) >> 3;
      Byte mask = (Byte)(1 << ((from + i) & 7));
      Byte b = Buf[pos];
      if ((b & mask) != 0)
        return false;
      Buf[pos] = (Byte)(b | mask);
    }
    return true;
  }
};
 
struct CStringKeyValue
{
  UString Key;
  UString Value;
};


Z7_CLASS_IMP_CHandler_IInArchive_2(
  IInArchiveGetStream,
  IArchiveAllowTail
)
  CMyComPtr<IInStream> _stream;
  CObjectVector<CSection> _sections;
  CHeader _header;
  UInt32 _totalSize;
  Int32 _mainSubfile;

  CRecordVector<CMixItem> _mixItems;
  CRecordVector<CResItem> _items;
  CObjectVector<CStringItem> _strings;
  CObjectVector<CByteBuffer_WithLang> _versionFiles;
  UString _versionFullString;
  UString _versionShortString;
  UString _originalFilename;
  CObjectVector<CStringKeyValue> _versionKeys;

  CByteBuffer _buf;
  bool _oneLang;
  UString _resourcesPrefix;
  CUsedBitmap _usedRes;
  // bool _parseResources;
  bool _checksumError;
  bool _sectionsError;

  bool IsOpt() const { return _header.OptHeaderSize != 0; }

  COptHeader _optHeader;

  bool _coffMode;
  bool _allowTail;

  HRESULT LoadDebugSections(IInStream *stream, bool &thereIsSection);
  HRESULT Open2(IInStream *stream, IArchiveOpenCallback *callback);

  void AddResNameToString(UString &s, UInt32 id) const;
  void AddLangPrefix(UString &s, UInt32 lang) const;
  HRESULT ReadString(UInt32 offset, UString &dest) const;
  HRESULT ReadTable(UInt32 offset, CRecordVector<CTableItem> &items);
  bool ParseStringRes(UInt32 id, UInt32 lang, const Byte *src, UInt32 size);
  HRESULT OpenResources(unsigned sectIndex, IInStream *stream, IArchiveOpenCallback *callback);
  void CloseResources();


  bool CheckItem(const CSection &sect, const CResItem &item, size_t offset) const
  {
    return item.Offset >= sect.Va && offset <= _buf.Size() && _buf.Size() - offset >= item.Size;
  }

public:
  CHandler(bool coffMode = false):
        _coffMode(coffMode),
        _allowTail(coffMode)
        {}
};


enum
{
  kpidSectAlign = kpidUserDefined,
  kpidFileAlign,
  kpidLinkerVer,
  kpidOsVer,
  kpidImageVer,
  kpidSubsysVer,
  kpidCodeSize,
  kpidImageSize,
  kpidInitDataSize,
  kpidUnInitDataSize,
  kpidHeadersSizeUnInitDataSize,
  kpidSubSystem,
  kpidDllCharacts,
  kpidStackReserve,
  kpidStackCommit,
  kpidHeapReserve,
  kpidHeapCommit,
  kpidImageBase
  // kpidAddressOfEntryPoint,
  // kpidBaseOfCode,
  // kpidBaseOfData32,
};

static const CStatProp kArcProps[] =
{
  // { NULL, kpidWarning, VT_BSTR},
  { NULL, kpidCpu, VT_BSTR},
  { NULL, kpidBit64, VT_BOOL},
  { NULL, kpidCharacts, VT_BSTR},
  { NULL, kpidCTime, VT_FILETIME},
  { NULL, kpidHeadersSize, VT_UI4},
  { NULL, kpidChecksum, VT_UI4},
  { NULL, kpidName, VT_BSTR},

  { "Image Size", kpidImageSize, VT_UI4},
  { "Section Alignment", kpidSectAlign, VT_UI4},
  { "File Alignment", kpidFileAlign, VT_UI4},
  { "Code Size", kpidCodeSize, VT_UI4},
  { "Initialized Data Size", kpidInitDataSize, VT_UI4},
  { "Uninitialized Data Size", kpidUnInitDataSize, VT_UI4},
  { "Linker Version", kpidLinkerVer, VT_BSTR},
  { "OS Version", kpidOsVer, VT_BSTR},
  { "Image Version", kpidImageVer, VT_BSTR},
  { "Subsystem Version", kpidSubsysVer, VT_BSTR},
  { "Subsystem", kpidSubSystem, VT_BSTR},
  { "DLL Characteristics", kpidDllCharacts, VT_BSTR},
  { "Stack Reserve", kpidStackReserve, VT_UI8},
  { "Stack Commit", kpidStackCommit, VT_UI8},
  { "Heap Reserve", kpidHeapReserve, VT_UI8},
  { "Heap Commit", kpidHeapCommit, VT_UI8},
  { "Image Base", kpidImageBase, VT_UI8},
  { NULL, kpidComment, VT_BSTR},
  
  // { "Address Of Entry Point", kpidAddressOfEntryPoint, VT_UI8},
  // { "Base Of Code", kpidBaseOfCode, VT_UI8},
  // { "Base Of Data", kpidBaseOfData32, VT_UI8},
};

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidPackSize,
  kpidVirtualSize,
  kpidCharacts,
  kpidOffset,
  kpidVa,
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps_WITH_NAME

static void TimeToProp(UInt32 unixTime, NCOM::CPropVariant &prop)
{
  if (unixTime != 0)
    PropVariant_SetFrom_UnixTime(prop, unixTime);
}

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPhySize: prop = _totalSize; break;
    case kpidComment: if (!_versionFullString.IsEmpty()) prop = _versionFullString; break;
    case kpidShortComment:
      if (!_versionShortString.IsEmpty())
        prop = _versionShortString;
      else
      {
        PAIR_TO_PROP(g_MachinePairs, _header.Machine, prop);
      }
      break;

    case kpidName: if (!_originalFilename.IsEmpty()) prop = _originalFilename; break;
      
    // case kpidIsSelfExe: prop = !_header.IsDll(); break;
    // case kpidError:
    case kpidWarning: if (_checksumError) prop = "Checksum error"; break;

    case kpidWarningFlags:
    {
      UInt32 v = 0;
      if (_sectionsError) v |= kpv_ErrorFlags_HeadersError;
      if (v != 0)
        prop = v;
      break;
    }

    case kpidCpu: PAIR_TO_PROP(g_MachinePairs, _header.Machine, prop); break;
    case kpidMTime:
    case kpidCTime: TimeToProp(_header.Time, prop); break;
    case kpidCharacts: FLAGS_TO_PROP(g_HeaderCharacts, _header.Flags, prop); break;
    case kpidMainSubfile: if (_mainSubfile >= 0) prop = (UInt32)_mainSubfile; break;
    
    default:
    if (IsOpt())
    switch (propID)
    {

    case kpidSectAlign: prop = _optHeader.SectAlign; break;
    case kpidFileAlign: prop = _optHeader.FileAlign; break;
    case kpidLinkerVer:
    {
      CVersion v = { _optHeader.LinkerVerMajor, _optHeader.LinkerVerMinor };
      v.ToProp(prop);
      break;
    }
  
    case kpidOsVer: _optHeader.OsVer.ToProp(prop); break;
    case kpidImageVer: _optHeader.ImageVer.ToProp(prop); break;
    case kpidSubsysVer: _optHeader.SubsysVer.ToProp(prop); break;
    case kpidCodeSize: prop = _optHeader.CodeSize; break;
    case kpidInitDataSize: prop = _optHeader.InitDataSize; break;
    case kpidUnInitDataSize: prop = _optHeader.UninitDataSize; break;
    case kpidImageSize: prop = _optHeader.ImageSize; break;
    case kpidHeadersSize: prop = _optHeader.HeadersSize; break;
    case kpidChecksum: prop = _optHeader.CheckSum; break;

    case kpidExtension:
      if (_header.IsDll())
        prop = "dll";
      else if (_optHeader.IsSybSystem_EFI())
        prop = "efi";
      break;

    case kpidBit64: if (_optHeader.Is64Bit()) prop = true; break;
    case kpidSubSystem: TYPE_TO_PROP(g_SubSystems, _optHeader.SubSystem, prop); break;

    case kpidDllCharacts: FLAGS_TO_PROP(g_DllCharacts, _optHeader.DllCharacts, prop); break;
    case kpidStackReserve: prop = _optHeader.StackReserve; break;
    case kpidStackCommit: prop = _optHeader.StackCommit; break;
    case kpidHeapReserve: prop = _optHeader.HeapReserve; break;
    case kpidHeapCommit: prop = _optHeader.HeapCommit; break;

    case kpidImageBase: prop = _optHeader.ImageBase; break;
    // case kpidAddressOfEntryPoint: prop = _optHeader.AddressOfEntryPoint; break;
    // case kpidBaseOfCode: prop = _optHeader.BaseOfCode; break;
    // case kpidBaseOfData32: if (!_optHeader.Is64Bit()) prop = _optHeader.BaseOfData32; break;
  }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

HRESULT CHandler::ReadString(UInt32 offset, UString &dest) const
{
  if ((offset & 1) != 0 || offset >= _buf.Size())
    return S_FALSE;
  size_t rem = _buf.Size() - offset;
  if (rem < 2)
    return S_FALSE;
  unsigned len = Get16(_buf + offset);
  if ((rem - 2) / 2 < len)
    return S_FALSE;
  dest.Empty();
  wchar_t *destBuf = dest.GetBuf(len);
  offset += 2;
  const Byte *src = _buf + offset;
  unsigned i;
  for (i = 0; i < len; i++)
  {
    wchar_t c = (wchar_t)Get16(src + i * 2);
    if (c == 0)
      break;
    destBuf[i] = c;
  }
  destBuf[i] = 0;
  dest.ReleaseBuf_SetLen(i);
  return S_OK;
}

void CHandler::AddResNameToString(UString &s, UInt32 id) const
{
  if ((id & kFlag) != 0)
  {
    UString name;
    if (ReadString(id & kMask, name) == S_OK)
    {
      const wchar_t *str = L"[]";
      if (name.Len() > 1 && name[0] == '"' && name.Back() == '"')
      {
        if (name.Len() != 2)
        {
          name.DeleteBack();
          str = name.Ptr(1);
        }
      }
      else if (!name.IsEmpty())
        str = name;
      s += str;
      return;
    }
  }
  s.Add_UInt32(id);
}

void CHandler::AddLangPrefix(UString &s, UInt32 lang) const
{
  if (!_oneLang)
  {
    AddResNameToString(s, lang);
    s.Add_PathSepar();
  }
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  const CMixItem &mixItem = _mixItems[index];
  if (mixItem.StringIndex >= 0)
  {
    const CStringItem &item = _strings[mixItem.StringIndex];
    switch (propID)
    {
      case kpidPath:
      {
        UString s = _resourcesPrefix;
        AddLangPrefix(s, item.Lang);
        s += "string.txt";
        prop = s;
        break;
      }
      case kpidSize:
      case kpidPackSize:
        prop = (UInt64)item.FinalSize(); break;
    }
  }
  else if (mixItem.VersionIndex >= 0)
  {
    const CByteBuffer_WithLang &item = _versionFiles[mixItem.VersionIndex];
    switch (propID)
    {
      case kpidPath:
      {
        UString s = _resourcesPrefix;
        AddLangPrefix(s, item.Lang);
        s += "version.txt";
        prop = s;
        break;
      }
      case kpidSize:
      case kpidPackSize:
        prop = (UInt64)item.Size(); break;
    }
  }
  else if (mixItem.ResourceIndex >= 0)
  {
    const CResItem &item = _items[mixItem.ResourceIndex];
    switch (propID)
    {
      case kpidPath:
      {
        UString s = _resourcesPrefix;
        AddLangPrefix(s, item.Lang);
        {
          const char *p = NULL;
          if (item.Type < Z7_ARRAY_SIZE(g_ResTypes))
            p = g_ResTypes[item.Type];
          if (p)
            s += p;
          else
            AddResNameToString(s, item.Type);
        }
        s.Add_PathSepar();
        AddResNameToString(s, item.ID);
        if (item.HeaderSize != 0)
        {
          if (item.IsBmp())
            s += ".bmp";
          else if (item.IsIcon())
            s += ".ico";
        }
        prop = s;
        break;
      }
      case kpidSize: prop = (UInt64)item.GetSize(); break;
      case kpidPackSize: prop = (UInt64)item.Size; break;
    }
  }
  else
  {
    const CSection &item = _sections[mixItem.SectionIndex];
    switch (propID)
    {
      case kpidPath:
      {
        AString s = item.Name;
        s.Replace('/', '_');
        s.Replace('\\', '_');
        prop = MultiByteToUnicodeString(s);
        break;
      }
      case kpidSize: prop = (UInt64)item.PSize; break;
      case kpidPackSize: prop = (UInt64)item.PSize; break;
      case kpidVirtualSize: prop = (UInt64)item.VSize; break;
      case kpidOffset: prop = item.Pa; break;
      case kpidVa: if (item.IsRealSect) prop = item.Va; break;
      case kpidMTime:
      case kpidCTime:
        TimeToProp(item.IsDebug ? item.Time : _header.Time, prop); break;
      case kpidCharacts:
       if (item.IsRealSect)
       {
         UInt32 flags = item.Flags;
         const UInt32 MY_IMAGE_SCN_ALIGN_MASK = 0x00F00000;
         AString s = FlagsToString(g_SectFlags, Z7_ARRAY_SIZE(g_SectFlags), item.Flags & ~MY_IMAGE_SCN_ALIGN_MASK);
         const UInt32 align = ((flags >> 20) & 0xF);
         if (align != 0)
         {
           char sz[32];
           ConvertUInt32ToString(1 << (align - 1), sz);
           s.Add_Space();
           s += "align_";
           s += sz;
         }
         prop = s;
       }
       break;
      case kpidZerosTailIsAllowed: if (!item.IsRealSect) prop = true; break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

HRESULT CHandler::LoadDebugSections(IInStream *stream, bool &thereIsSection)
{
  thereIsSection = false;
  const CDirLink &debugLink = _optHeader.DirItems[kDirLink_Debug];
  if (debugLink.Size == 0)
    return S_OK;
  const unsigned kEntrySize = 28;
  UInt32 numItems = debugLink.Size / kEntrySize;
  if (numItems > 16)
    return S_FALSE;

  // MAC's EFI file: numItems can be incorrect. Only first CDebugEntry entry is correct.
  // debugLink.Size = kEntrySize + some_data, pointed by entry[0].
  if (numItems * kEntrySize != debugLink.Size)
  {
    // return S_FALSE;
    if (numItems > 1)
      numItems = 1;
  }
  
  UInt64 pa = 0;
  unsigned i;
  for (i = 0; i < _sections.Size(); i++)
  {
    const CSection &sect = _sections[i];
    if (sect.Va <= debugLink.Va && debugLink.Va + debugLink.Size <= sect.Va + sect.PSize)
    {
      pa = sect.Pa + (debugLink.Va - sect.Va);
      break;
    }
  }
  if (i == _sections.Size())
  {
    // Exe for ARM requires S_OK
    // return S_FALSE;
    return S_OK;
  }
  
  CByteBuffer buffer(debugLink.Size);
  Byte *buf = buffer;
  
  RINOK(InStream_SeekSet(stream, pa))
  RINOK(ReadStream_FALSE(stream, buf, debugLink.Size))

  for (i = 0; i < numItems; i++)
  {
    CDebugEntry de;
    de.Parse(buf);

    if (de.Size == 0)
      continue;
    
    UInt32 totalSize = de.Pa + de.Size;
    if (totalSize > _totalSize)
    {
      _totalSize = totalSize;
      thereIsSection = true;

      CSection &sect = _sections.AddNew();
      sect.Name = ".debug";
      sect.Name.Add_UInt32(i);
      sect.IsDebug = true;
      sect.Time = de.Time;
      sect.Va = de.Va;
      sect.Pa = de.Pa;
      sect.PSize = sect.VSize = de.Size;
    }
    buf += kEntrySize;
  }

  return S_OK;
}

HRESULT CHandler::ReadTable(UInt32 offset, CRecordVector<CTableItem> &items)
{
  if ((offset & 3) != 0 || offset >= _buf.Size())
    return S_FALSE;
  size_t rem = _buf.Size() - offset;
  if (rem < 16)
    return S_FALSE;
  unsigned numNameItems = Get16(_buf + offset + 12);
  unsigned numIdItems = Get16(_buf + offset + 14);
  unsigned numItems = numNameItems + numIdItems;
  if ((rem - 16) / 8 < numItems)
    return S_FALSE;
  if (!_usedRes.SetRange(offset, 16 + numItems * 8))
    return S_FALSE;
  offset += 16;
  items.ClearAndReserve(numItems);
  for (unsigned i = 0; i < numItems; i++, offset += 8)
  {
    const Byte *buf = _buf + offset;
    CTableItem item;
    item.ID = Get32(buf + 0);
    if ((bool)((item.ID & kFlag) != 0) != (bool)(i < numNameItems))
      return S_FALSE;
    item.Offset = Get32(buf + 4);
    items.AddInReserved(item);
  }
  return S_OK;
}

static const UInt32 kFileSizeMax = (UInt32)1 << 31;
static const unsigned kNumResItemsMax = (unsigned)1 << 23;
static const unsigned kNumStringLangsMax = 256;

// BITMAPINFOHEADER
struct CBitmapInfoHeader
{
  // UInt32 HeaderSize;
  UInt32 XSize;
  Int32 YSize;
  UInt16 Planes;
  UInt16 BitCount;
  UInt32 Compression;
  UInt32 SizeImage;

  bool Parse(const Byte *p, size_t size);
};

static const UInt32 kBitmapInfoHeader_Size = 0x28;

bool CBitmapInfoHeader::Parse(const Byte *p, size_t size)
{
  if (size < kBitmapInfoHeader_Size || Get32(p) != kBitmapInfoHeader_Size)
    return false;
  G32( 4, XSize);
  G32_signed( 8, YSize);
  G16(12, Planes);
  G16(14, BitCount);
  G32(16, Compression);
  G32(20, SizeImage);
  return true;
}

static UInt32 GetImageSize(UInt32 xSize, UInt32 ySize, UInt32 bitCount)
{
  return ((xSize * bitCount + 7) / 8 + 3) / 4 * 4 * ySize;
}
  
static UInt32 SetBitmapHeader(Byte *dest, const Byte *src, UInt32 size)
{
  CBitmapInfoHeader h;
  if (!h.Parse(src, size))
    return 0;
  if (h.YSize < 0)
    h.YSize = -h.YSize;
  if (h.XSize > (1 << 26)
      || h.YSize > (1 << 26)
      || h.YSize < 0
      || h.Planes != 1 || h.BitCount > 32)
    return 0;
  if (h.SizeImage == 0)
  {
    if (h.Compression != 0) // BI_RGB
      return 0;
    h.SizeImage = GetImageSize(h.XSize, (UInt32)h.YSize, h.BitCount);
  }
  UInt32 totalSize = kBmpHeaderSize + size;
  UInt32 offBits = totalSize - h.SizeImage;
  // BITMAPFILEHEADER
  SetUi16(dest, 0x4D42)
  SetUi32(dest + 2, totalSize)
  SetUi32(dest + 6, 0)
  SetUi32(dest + 10, offBits)
  return kBmpHeaderSize;
}

static UInt32 SetIconHeader(Byte *dest, const Byte *src, UInt32 size)
{
  CBitmapInfoHeader h;
  if (!h.Parse(src, size))
    return 0;
  if (h.YSize < 0)
    h.YSize = -h.YSize;
  if (h.XSize > (1 << 26)
      || h.YSize > (1 << 26)
      || h.YSize < 0
      || h.Planes != 1
      || h.Compression != 0) // BI_RGB
    return 0;

  const UInt32 numBitCount = h.BitCount;
  if (numBitCount != 1 &&
      numBitCount != 4 &&
      numBitCount != 8 &&
      numBitCount != 24 &&
      numBitCount != 32)
    return 0;

  if ((h.YSize & 1) != 0)
    return 0;
  h.YSize /= 2;
  if (h.XSize > 0x100 || h.YSize > 0x100)
    return 0;

  UInt32 imageSize;
  // imageSize is not correct if AND mask array contains zeros
  // in this case it is equal image1Size

  // UInt32 imageSize = h.SizeImage;
  // if (imageSize == 0)
  // {
    const UInt32 image1Size = GetImageSize(h.XSize, (UInt32)h.YSize, h.BitCount);
    const UInt32 image2Size = GetImageSize(h.XSize, (UInt32)h.YSize, 1);
    imageSize = image1Size + image2Size;
  // }
  UInt32 numColors = 0;
  if (numBitCount < 16)
    numColors = 1 << numBitCount;

  SetUi16(dest, 0) // Reserved
  SetUi16(dest + 2, 1) // RES_ICON
  SetUi16(dest + 4, 1) // ResCount

  dest[6] = (Byte)h.XSize; // Width
  dest[7] = (Byte)h.YSize; // Height
  dest[8] = (Byte)numColors; // ColorCount
  dest[9] = 0; // Reserved
  
  SetUi32(dest + 10, 0) // Reserved1 / Reserved2

  UInt32 numQuadsBytes = numColors * 4;
  UInt32 BytesInRes = kBitmapInfoHeader_Size + numQuadsBytes + imageSize;
  SetUi32(dest + 14, BytesInRes)
  SetUi32(dest + 18, kIconHeaderSize)

  /*
  Description = DWORDToString(xSize) +
      kDelimiterChar + DWORDToString(ySize) +
      kDelimiterChar + DWORDToString(numBitCount);
  */
  return kIconHeaderSize;
}

bool CHandler::ParseStringRes(UInt32 id, UInt32 lang, const Byte *src, UInt32 size)
{
  if ((size & 1) != 0)
    return false;

  unsigned i;
  for (i = 0; i < _strings.Size(); i++)
    if (_strings[i].Lang == lang)
      break;
  if (i == _strings.Size())
  {
    if (_strings.Size() >= kNumStringLangsMax)
      return false;
    CStringItem &item = _strings.AddNew();
    item.Lang = lang;
  }
  
  CStringItem &item = _strings[i];
  id = (id - 1) << 4;
  UInt32 pos = 0;
  for (i = 0; i < 16; i++)
  {
    if (size - pos < 2)
      return false;
    UInt32 len = Get16(src + pos);
    pos += 2;
    if (len != 0)
    {
      if (size - pos < len * 2)
        return false;
      char temp[32];
      ConvertUInt32ToString(id + i, temp);
      size_t tempLen = strlen(temp);
      size_t j;
      for (j = 0; j < tempLen; j++)
        item.AddChar(temp[j]);
      item.AddChar('\t');
      for (j = 0; j < len; j++, pos += 2)
        item.AddWChar_Smart(Get16(src + pos));
      item.NewLine();
    }
  }
  if (size == pos)
    return true;
  
  // Some rare case files have additional ZERO.
  if (size == pos + 2 && Get16(src + pos) == 0)
    return true;
  
  return false;
}


// ---------- VERSION ----------

static const UInt32 kMy_VS_FFI_SIGNATURE = 0xFEEF04BD;

struct CMy_VS_FIXEDFILEINFO
{
  // UInt32 Signature;
  // UInt32 StrucVersion;
  UInt32 VersionMS;
  UInt32 VersionLS;
  UInt32 ProductVersionMS;
  UInt32 ProductVersionLS;
  UInt32 FlagsMask;
  UInt32 Flags;
  UInt32 OS;
  UInt32 Type;
  UInt32 Subtype;
  UInt32 DateMS;
  UInt32 DateLS;

  bool Parse(const Byte *p);
  void PrintToTextFile(CTextFile &f, CObjectVector<CStringKeyValue> &keys);
};

bool CMy_VS_FIXEDFILEINFO::Parse(const Byte *p)
{
  if (Get32(p) != kMy_VS_FFI_SIGNATURE) // signature;
    return false;
  // G32(0x04, StrucVersion);
  G32(0x08, VersionMS);
  G32(0x0C, VersionLS);
  G32(0x10, ProductVersionMS);
  G32(0x14, ProductVersionLS);
  G32(0x18, FlagsMask);
  G32(0x1C, Flags);
  G32(0x20, OS);
  G32(0x24, Type);
  G32(0x28, Subtype);
  G32(0x2C, DateMS);
  G32(0x40, DateLS);
  return true;
}

static void PrintUInt32(CTextFile &f, UInt32 v)
{
  char s[16];
  ConvertUInt32ToString(v, s);
  f.AddString(s);
}

static inline void PrintUInt32(UString &dest, UInt32 v)
{
  dest.Add_UInt32(v);
}

static void PrintHex(CTextFile &f, UInt32 val)
{
  char temp[16];
  temp[0] = '0';
  temp[1] = 'x';
  ConvertUInt32ToHex(val, temp + 2);
  f.AddString(temp);
}

static void PrintVersion(CTextFile &f, UInt32 ms, UInt32 ls)
{
  PrintUInt32(f, HIWORD(ms));  f.AddChar(',');
  PrintUInt32(f, LOWORD(ms));  f.AddChar(',');
  PrintUInt32(f, HIWORD(ls));  f.AddChar(',');
  PrintUInt32(f, LOWORD(ls));
}

static void PrintVersion(UString &s, UInt32 ms, UInt32 ls)
{
  PrintUInt32(s, HIWORD(ms));  s.Add_Dot();
  PrintUInt32(s, LOWORD(ms));  s.Add_Dot();
  PrintUInt32(s, HIWORD(ls));  s.Add_Dot();
  PrintUInt32(s, LOWORD(ls));
}

static const char * const k_VS_FileFlags[] =
{
    "DEBUG"
  , "PRERELEASE"
  , "PATCHED"
  , "PRIVATEBUILD"
  , "INFOINFERRED"
  , "SPECIALBUILD"
};

static const CUInt32PCharPair k_VS_FileOS[] =
{
  {  0x10001, "VOS_DOS_WINDOWS16" },
  {  0x10004, "VOS_DOS_WINDOWS32" },
  {  0x20002, "VOS_OS216_PM16" },
  {  0x30003, "VOS_OS232_PM32" },
  {  0x40004, "VOS_NT_WINDOWS32" }
};

static const char * const k_VS_FileOS_High[] =
{
    "VOS_UNKNOWN"
  , "VOS_DOS"
  , "VOS_OS216"
  , "VOS_OS232"
  , "VOS_NT"
  , "VOS_WINCE"
};

static const UInt32 kMY_VFT_DRV  = 3;
static const UInt32 kMY_VFT_FONT = 4;

static const char * const k_VS_FileOS_Low[] =
{
    "VOS__BASE"
  , "VOS__WINDOWS16"
  , "VOS__PM16"
  , "VOS__PM32"
  , "VOS__WINDOWS32"
};

static const char * const k_VS_FileType[] =
{
    "VFT_UNKNOWN"
  , "VFT_APP"
  , "VFT_DLL"
  , "VFT_DRV"
  , "VFT_FONT"
  , "VFT_VXD"
  , "0x6"
  , "VFT_STATIC_LIB"
};

// Subtype for VFT_DRV Type
static const char * const k_VS_FileSubType_DRV[] =
{
    "0"
  , "PRINTER"
  , "KEYBOARD"
  , "LANGUAGE"
  , "DISPLAY"
  , "MOUSE"
  , "NETWORK"
  , "SYSTEM"
  , "INSTALLABLE"
  , "SOUND"
  , "COMM"
  , "INPUTMETHOD"
  , "VERSIONED_PRINTER"
};

// Subtype for VFT_FONT Type
static const char * const k_VS_FileSubType_FONT[] =
{
    "0"
  , "VFT2_FONT_RASTER"
  , "VFT2_FONT_VECTOR"
  , "VFT2_FONT_TRUETYPE"
};

static int FindKey(CObjectVector<CStringKeyValue> &v, const char *key)
{
  FOR_VECTOR (i, v)
    if (v[i].Key.IsEqualTo(key))
      return (int)i;
  return -1;
}

static void AddToUniqueUStringVector(CObjectVector<CStringKeyValue> &v, const UString &key, const UString &value)
{
  bool needInsert = false;
  unsigned i;
  for (i = 0; i < v.Size(); i++)
  {
    if (v[i].Key == key)
    {
      if (v[i].Value == value)
        return;
      needInsert = true;
    }
    else if (needInsert)
      break;
  }
  CStringKeyValue &pair = v.InsertNew(i);
  pair.Key = key;
  pair.Value = value;
}

void CMy_VS_FIXEDFILEINFO::PrintToTextFile(CTextFile &f, CObjectVector<CStringKeyValue> &keys)
{
  f.AddString("FILEVERSION    ");
  PrintVersion(f, VersionMS, VersionLS);
  f.NewLine();

  f.AddString("PRODUCTVERSION ");
  PrintVersion(f, ProductVersionMS, ProductVersionLS);
  f.NewLine();

  {
    UString s;
    PrintVersion(s, VersionMS, VersionLS);
    AddToUniqueUStringVector(keys, L"FileVersion", s);
  }
  {
    UString s;
    PrintVersion(s, ProductVersionMS, ProductVersionLS);
    AddToUniqueUStringVector(keys, L"ProductVersion", s);
  }
 
  f.AddString("FILEFLAGSMASK  ");
  PrintHex(f, FlagsMask);
  f.NewLine();

  f.AddString("FILEFLAGS      ");
  {
    bool wasPrinted = false;
    for (unsigned i = 0; i < Z7_ARRAY_SIZE(k_VS_FileFlags); i++)
    {
      if ((Flags & ((UInt32)1 << i)) != 0)
      {
        if (wasPrinted)
          f.AddString(" | ");
        f.AddString("VS_FF_");
        f.AddString(k_VS_FileFlags[i]);
        wasPrinted = true;
      }
    }
    UInt32 v = Flags & ~(((UInt32)1 << Z7_ARRAY_SIZE(k_VS_FileFlags)) - 1);
    if (v != 0 || !wasPrinted)
    {
      if (wasPrinted)
        f.AddString(" | ");
      PrintHex(f, v);
    }
  }
  f.NewLine();

  // OS = 0x111230;
  f.AddString("FILEOS         ");
  unsigned i;
  for (i = 0; i < Z7_ARRAY_SIZE(k_VS_FileOS); i++)
  {
    const CUInt32PCharPair &pair = k_VS_FileOS[i];
    if (OS == pair.Value)
    {
      // continue;
      // f.AddString("VOS_");
      f.AddString(pair.Name);
      break;
    }
  }
  if (i == Z7_ARRAY_SIZE(k_VS_FileOS))
  {
    UInt32 high = OS >> 16;
    if (high < Z7_ARRAY_SIZE(k_VS_FileOS_High))
      f.AddString(k_VS_FileOS_High[high]);
    else
      PrintHex(f, high << 16);
    UInt32 low = OS & 0xFFFF;
    if (low != 0)
    {
      f.AddString(" | ");
      if (low < Z7_ARRAY_SIZE(k_VS_FileOS_Low))
        f.AddString(k_VS_FileOS_Low[low]);
      else
        PrintHex(f, low);
    }
  }
  f.NewLine();

  f.AddString("FILETYPE       ");
  if (Type < Z7_ARRAY_SIZE(k_VS_FileType))
    f.AddString(k_VS_FileType[Type]);
  else
    PrintHex(f, Type);
  f.NewLine();

  f.AddString("FILESUBTYPE    ");
  bool needPrintSubType = true;
  if (Type == kMY_VFT_DRV)
  {
    if (Subtype != 0 && Subtype < Z7_ARRAY_SIZE(k_VS_FileSubType_DRV))
    {
      f.AddString("VFT2_DRV_");
      f.AddString(k_VS_FileSubType_DRV[Subtype]);
      needPrintSubType = false;
    }
  }
  else if (Type == kMY_VFT_FONT)
  {
    if (Subtype != 0 && Subtype < Z7_ARRAY_SIZE(k_VS_FileSubType_FONT))
    {
      f.AddString(k_VS_FileSubType_FONT[Subtype]);
      needPrintSubType = false;
    }
  }
  if (needPrintSubType)
    PrintHex(f, Subtype);
  f.NewLine();
}

static void CopyToUString(const Byte *p, UString &s)
{
  for (;;)
  {
    wchar_t c = (wchar_t)Get16(p);
    p += 2;
    if (c == 0)
      return;
    s += c;
  }
}

static bool CompareWStrStrings(const Byte *p, const char *s)
{
  unsigned pos = 0;
  for (;;)
  {
    const Byte c = (Byte)*s++;
    if (Get16(p + pos) != c)
      return false;
    pos += 2;
    if (c == 0)
      return true;
  }
}

struct CVersionBlock
{
  UInt32 TotalLen;
  UInt32 ValueLen;
  bool IsTextValue;
  unsigned StrSize;

  bool Parse(const Byte *p, UInt32 size);
};

static int Get_Utf16Str_Len_InBytes(const Byte *p, size_t size)
{
  unsigned pos = 0;
  for (;;)
  {
    if (pos + 1 >= size)
      return -1;
    if (Get16(p + pos) == 0)
      return (int)pos;
    pos += 2;
  }
}

static const unsigned k_ResoureBlockHeader_Size = 6;

bool CVersionBlock::Parse(const Byte *p, UInt32 size)
{
  if (size < k_ResoureBlockHeader_Size)
    return false;
  TotalLen = Get16(p);
  ValueLen = Get16(p + 2);
  if (TotalLen < k_ResoureBlockHeader_Size || TotalLen > size)
    return false;
  switch (Get16(p + 4))
  {
    case 0: IsTextValue = false; break;
    case 1: IsTextValue = true; break;
    default: return false;
  }
  StrSize = 0;
  const int t = Get_Utf16Str_Len_InBytes(p + k_ResoureBlockHeader_Size, TotalLen - k_ResoureBlockHeader_Size);
  if (t < 0)
    return false;
  StrSize = (unsigned)t;
  return true;
}

static void AddParamString(CTextFile &f, const Byte *p, size_t sLen)
{
  f.AddChar(' ');
  f.AddChar('\"');
  f.AddBytes(p, sLen);
  f.AddChar('\"');
}

static bool ParseVersion(const Byte *p, UInt32 size, CTextFile &f, CObjectVector<CStringKeyValue> &keys)
{
  UInt32 pos;
  {
    const unsigned k_sizeof_VS_FIXEDFILEINFO = 13 * 4;

    CVersionBlock vb;
    if (!vb.Parse(p, size))
      return false;
    if (vb.ValueLen != k_sizeof_VS_FIXEDFILEINFO) // maybe 0 is allowed here?
      return false;
    if (vb.IsTextValue)
      return false;
    pos = k_ResoureBlockHeader_Size;
    if (!CompareWStrStrings(p + pos, "VS_VERSION_INFO"))
      return false;
    pos += vb.StrSize + 2;
    pos += (4 - pos) & 3;
    if (pos + vb.ValueLen > vb.TotalLen)
      return false;
    /* sometimes resource contains zeros in remainder.
       So we don't check that size != vb.TotalLen
    // if (size != vb.TotalLen) return false;
    */
    if (size > vb.TotalLen)
      size = vb.TotalLen;
    CMy_VS_FIXEDFILEINFO FixedFileInfo;
    if (!FixedFileInfo.Parse(p + pos))
      return false;
    FixedFileInfo.PrintToTextFile(f, keys);
    pos += vb.ValueLen;
  }
  
  f.OpenBlock(0);
  
  for (;;)
  {
    pos += (4 - pos) & 3;
    if (pos >= size)
      break;
    
    CVersionBlock vb;
    if (!vb.Parse(p + pos, size - pos))
      return false;
    if (vb.ValueLen != 0)
      return false;
    UInt32 endPos = pos + vb.TotalLen;
    pos += k_ResoureBlockHeader_Size;
    
    f.AddSpaces(2);
    f.AddString("BLOCK");
    AddParamString(f, p + pos, vb.StrSize);
    
    f.NewLine();
    f.OpenBlock(2);
    
    if (CompareWStrStrings(p + pos, "VarFileInfo"))
    {
      pos += vb.StrSize + 2;
      for (;;)
      {
        pos += (4 - pos) & 3;
        if (pos >= endPos)
          break;
        CVersionBlock vb2;
        if (!vb2.Parse(p + pos, endPos - pos))
          return false;
        UInt32 endPos2 = pos + vb2.TotalLen;
        if (vb2.IsTextValue)
          return false;
        pos += k_ResoureBlockHeader_Size;
        f.AddSpaces(4);
        f.AddString("VALUE");
        AddParamString(f, p + pos, vb2.StrSize);
        if (!CompareWStrStrings(p + pos, "Translation"))
          return false;
        pos += vb2.StrSize + 2;
        pos += (4 - pos) & 3;
        if (pos + vb2.ValueLen != endPos2)
          return false;
        if ((vb2.ValueLen & 3) != 0)
          return false;
        UInt32 num = (vb2.ValueLen >> 2);
        for (; num != 0; num--, pos += 4)
        {
          UInt32 dw = Get32(p + pos);
          UInt32 lang = LOWORD(dw);
          UInt32 codePage = HIWORD(dw);

          f.AddString(", ");
          PrintHex(f, lang);
          f.AddString(", ");
          PrintUInt32(f, codePage);
        }
        f.NewLine();
      }
    }
    else
    {
      if (!CompareWStrStrings(p + pos, "StringFileInfo"))
        return false;
      pos += vb.StrSize + 2;
  
      for (;;)
      {
        pos += (4 - pos) & 3;
        if (pos >= endPos)
          break;
        CVersionBlock vb2;
        if (!vb2.Parse(p + pos, endPos - pos))
          return false;
        UInt32 endPos2 = pos + vb2.TotalLen;
        if (vb2.ValueLen != 0)
          return false;
        pos += k_ResoureBlockHeader_Size;

        f.AddSpaces(4);
        f.AddString("BLOCK");
        AddParamString(f, p + pos, vb2.StrSize);
        pos += vb2.StrSize + 2;

        f.NewLine();
        f.OpenBlock(4);

        for (;;)
        {
          pos += (4 - pos) & 3;
          if (pos >= endPos2)
            break;

          CVersionBlock vb3;
          if (!vb3.Parse(p + pos, endPos2 - pos))
            return false;
          // ValueLen sometimes is a number of characters (not bytes)?
          // So we don't use it.
          UInt32 endPos3 = pos + vb3.TotalLen;
          pos += k_ResoureBlockHeader_Size;

          // we don't write string if it's not text
          if (vb3.IsTextValue)
          {
            f.AddSpaces(6);
            f.AddString("VALUE");
            AddParamString(f, p + pos, vb3.StrSize);
            UString key;
            UString value;
            CopyToUString(p + pos, key);
            pos += vb3.StrSize + 2;

            pos += (4 - pos) & 3;
            if (vb3.ValueLen > 0 && pos + 2 <= endPos3)
            {
              f.AddChar(',');
              f.AddSpaces((34 - (int)vb3.StrSize) / 2);
              const int sLen = Get_Utf16Str_Len_InBytes(p + pos, endPos3 - pos);
              if (sLen < 0)
                return false;
              AddParamString(f, p + pos, (unsigned)sLen);
              CopyToUString(p + pos, value);
              pos += (unsigned)sLen + 2;
            }
            AddToUniqueUStringVector(keys, key, value);
          }
          pos = endPos3;
          f.NewLine();
        }
        f.CloseBlock(4);
      }
    }
    f.CloseBlock(2);
  }

  f.CloseBlock(0);
  return true;
}


HRESULT CHandler::OpenResources(unsigned sectionIndex, IInStream *stream, IArchiveOpenCallback *callback)
{
  const CSection &sect = _sections[sectionIndex];
  size_t fileSize = sect.PSize;
  {
    size_t fileSizeMin = sect.PSize;
    
    if (sect.VSize < sect.PSize)
    {
      fileSize = fileSizeMin = sect.VSize;
      const int numBits = _optHeader.GetNumFileAlignBits();
      if (numBits > 0)
      {
        const UInt32 mask = ((UInt32)1 << numBits) - 1;
        const size_t end = (size_t)((sect.VSize + mask) & (UInt32)~mask);
        if (end > sect.VSize)
        {
          if (end <= sect.PSize)
            fileSize = end;
          else
            fileSize = sect.PSize;
        }
      }
    }

    if (fileSize > kFileSizeMax)
      return S_FALSE;

    {
      const UInt64 fileSize64 = fileSize;
      if (callback)
        RINOK(callback->SetTotal(NULL, &fileSize64))
    }
    
    RINOK(InStream_SeekSet(stream, sect.Pa))
    
    _buf.Alloc(fileSize);
    
    size_t pos;
    
    for (pos = 0; pos < fileSize;)
    {
      {
        const UInt64 offset64 = pos;
        if (callback)
          RINOK(callback->SetCompleted(NULL, &offset64))
      }
      size_t rem = MyMin(fileSize - pos, (size_t)(1 << 22));
      RINOK(ReadStream(stream, _buf + pos, &rem))
      if (rem == 0)
      {
        if (pos < fileSizeMin)
          return S_FALSE;
        break;
      }
      pos += rem;
    }
    
    if (pos < fileSize)
      memset(_buf + pos, 0, fileSize - pos);
  }
  
  _usedRes.Alloc(fileSize);
  CRecordVector<CTableItem> specItems;
  RINOK(ReadTable(0, specItems))

  _oneLang = true;
  bool stringsOk = true;
  size_t maxOffset = 0;
  
  FOR_VECTOR (i, specItems)
  {
    const CTableItem &item1 = specItems[i];
    if ((item1.Offset & kFlag) == 0)
      return S_FALSE;

    CRecordVector<CTableItem> specItems2;
    RINOK(ReadTable(item1.Offset & kMask, specItems2))

    FOR_VECTOR (j, specItems2)
    {
      const CTableItem &item2 = specItems2[j];
      if ((item2.Offset & kFlag) == 0)
        return S_FALSE;
      
      CRecordVector<CTableItem> specItems3;
      RINOK(ReadTable(item2.Offset & kMask, specItems3))
      
      CResItem item;
      item.Type = item1.ID;
      item.ID = item2.ID;
      
      FOR_VECTOR (k, specItems3)
      {
        if (_items.Size() >= kNumResItemsMax)
          return S_FALSE;
        const CTableItem &item3 = specItems3[k];
        if ((item3.Offset & kFlag) != 0)
          return S_FALSE;
        if (item3.Offset >= _buf.Size() || _buf.Size() - item3.Offset < 16)
          return S_FALSE;
        const Byte *buf = _buf + item3.Offset;
        item.Lang = item3.ID;
        item.Offset = Get32(buf + 0);
        item.Size = Get32(buf + 4);
        // UInt32 codePage = Get32(buf + 8);
        if (Get32(buf + 12) != 0)
          return S_FALSE;
        if (!_items.IsEmpty() && _oneLang && !item.IsNameEqual(_items.Back()))
          _oneLang = false;

        item.HeaderSize = 0;
      
        size_t offset = item.Offset - sect.Va;
        if (offset > maxOffset)
          maxOffset = offset;
        if (offset + item.Size > maxOffset)
          maxOffset = offset + item.Size;

        if (CheckItem(sect, item, offset))
        {
          const Byte *data = _buf + offset;
          if (item.IsBmp())
            item.HeaderSize = SetBitmapHeader(item.Header, data, item.Size);
          else if (item.IsIcon())
            item.HeaderSize = SetIconHeader(item.Header, data, item.Size);
          else if (item.IsString())
          {
            if (stringsOk)
              stringsOk = ParseStringRes(item.ID, item.Lang, data, item.Size);
          }
        }

        if (item.IsVersion())
        {
          if (offset > _buf.Size() || _buf.Size() - offset < item.Size)
            continue;
          CTextFile f;
          if (ParseVersion((const Byte *)_buf + offset, item.Size, f, _versionKeys))
          {
            CMixItem mixItem;
            mixItem.VersionIndex = (int)_versionFiles.Size();
            mixItem.SectionIndex = (int)sectionIndex; // check it !!!!
            CByteBuffer_WithLang &vf = _versionFiles.AddNew();
            vf.Lang = item.Lang;
            vf.CopyFrom(f.Buf, f.Buf.GetPos());
            _mixItems.Add(mixItem);
            continue;
          }
          // PrintError("ver.Parse error");
        }
  
        item.Enabled = true;
        _items.Add(item);
      }
    }
  }
  
  if (stringsOk && !_strings.IsEmpty())
  {
    unsigned i;
    for (i = 0; i < _items.Size(); i++)
    {
      CResItem &item = _items[i];
      if (item.IsString())
        item.Enabled = false;
    }
    for (i = 0; i < _strings.Size(); i++)
    {
      if (_strings[i].FinalSize() == 0)
        continue;
      CMixItem mixItem;
      mixItem.StringIndex = (int)i;
      mixItem.SectionIndex = (int)sectionIndex;
      _mixItems.Add(mixItem);
    }
  }

  _usedRes.Free();

  {
    // PSize can be much larger than VSize in some exe installers.
    // it contains archive data after PE resources.
    // So we need to use PSize here!
    if (maxOffset < sect.PSize)
    {
      size_t end = fileSize;

      // we skip Zeros to start of aligned block
      size_t i;
      for (i = maxOffset; i < end; i++)
        if (_buf[i] != 0)
          break;
      if (i == end)
        maxOffset = end;
      
      CSection sect2;
      sect2.Flags = 0;
      sect2.Pa = sect.Pa + (UInt32)maxOffset;
      sect2.Va = sect.Va + (UInt32)maxOffset;

      // 9.29: we use sect.PSize instead of sect.VSize to support some CAB-SFX
      // the code for .rsrc_2 is commented.
      sect2.PSize = sect.PSize - (UInt32)maxOffset;

      if (sect2.PSize != 0)
      {
        sect2.VSize = sect2.PSize;
        sect2.Name = ".rsrc_1";
        sect2.Time = 0;
        sect2.IsAdditionalSection = true;
        _sections.Add(sect2);
      }
    }
  }

  return S_OK;
}


bool CHeader::ParseCoff(const Byte *p)
{
  ParseBase(p);
  if (PointerToSymbolTable < kCoffHeaderSize)
    return false;
  if (NumSymbols >= (1 << 24))
    return false;
  if (OptHeaderSize != 0 && OptHeaderSize < k_OptHeader32_Size_MIN)
    return false;

  // 18.04: we reduce false detections
  if (NumSections == 0 && OptHeaderSize == 0)
    return false;

  for (unsigned i = 0; i < Z7_ARRAY_SIZE(g_MachinePairs); i++)
    if (Machine == g_MachinePairs[i].Value)
      return true;
  if (Machine == 0)
    return true;

  return false;
}


static inline bool CheckPeOffset(UInt32 pe)
{
  // ((pe & 7) == 0) is for most PE files. But there is unusual EFI-PE file that uses unaligned pe value.
  return pe >= 0x40 && pe <= 0x1000 /* && (pe & 7) == 0 */ ;
}

static const unsigned kStartSize = 0x40;

API_FUNC_static_IsArc IsArc_Pe(const Byte *p, size_t size)
{
  if (size < 2)
    return k_IsArc_Res_NEED_MORE;
  if (p[0] != 'M' || p[1] != 'Z')
    return k_IsArc_Res_NO;
  if (size < kStartSize)
    return k_IsArc_Res_NEED_MORE;
  UInt32 pe = Get32(p + 0x3C);
  if (!CheckPeOffset(pe))
    return k_IsArc_Res_NO;
  if (pe + kPeHeaderSize > size)
    return k_IsArc_Res_NEED_MORE;
  CHeader header;
  if (!header.ParsePe(p + pe))
    return k_IsArc_Res_NO;
  return k_IsArc_Res_YES;
}
}

HRESULT CHandler::Open2(IInStream *stream, IArchiveOpenCallback *callback)
{
  UInt32 coffOffset = 0;
  if (_coffMode)
  {
    Byte h[kCoffHeaderSize];
    RINOK(ReadStream_FALSE(stream, h, kCoffHeaderSize))
    if (!_header.ParseCoff(h))
      return S_FALSE;
  }
  else
  {
    UInt32 _peOffset;
    {
      Byte h[kStartSize];
      RINOK(ReadStream_FALSE(stream, h, kStartSize))
      if (h[0] != 'M' || h[1] != 'Z')
        return S_FALSE;
        /* most of PE files contain 0x0090 at offset 2.
        But some rare PE files contain another values. So we don't use that check.
      if (Get16(h + 2) != 0x90) return false; */
      _peOffset = Get32(h + 0x3C);
      if (!CheckPeOffset(_peOffset))
        return S_FALSE;
      coffOffset = _peOffset + 4;
    }
    {
      Byte h[kPeHeaderSize];
      RINOK(InStream_SeekSet(stream, _peOffset))
      RINOK(ReadStream_FALSE(stream, h, kPeHeaderSize))
      if (!_header.ParsePe(h))
        return S_FALSE;
    }
  }

  const UInt32 optStart = coffOffset + kCoffHeaderSize;
  const UInt32 bufSize = _header.OptHeaderSize + (UInt32)_header.NumSections * kSectionSize;
  _totalSize = optStart + bufSize;
  CByteBuffer buffer(bufSize);

  RINOK(ReadStream_FALSE(stream, buffer, bufSize))
  
  // memset((void *)&_optHeader, 0, sizeof(_optHeader));
  if (_header.OptHeaderSize != 0)
  if (!_optHeader.Parse(buffer, _header.OptHeaderSize))
    return S_FALSE;

  UInt32 pos = _header.OptHeaderSize;
  unsigned i;
  for (i = 0; i < _header.NumSections; i++, pos += kSectionSize)
  {
    CSection &sect = _sections.AddNew();
    sect.Parse(buffer + pos);
    sect.IsRealSect = true;
    
    /* PE pre-file in .hxs file has errors:
         PSize of resource is larger than real size.
         So it overlaps next ".its" section.
       7-zip before 22.02: we corrected it.

       22.02: another bad case is possible in incorrect pe (exe) file:
         PSize in .rsrc section is correct,
         but next .reloc section has incorrect (Pa) that overlaps with .rsrc.
    */

    if (i != 0)
    {
      const CSection &prev = _sections[i - 1];
      if (prev.Pa < sect.Pa
          && prev.Pa + prev.PSize > sect.Pa
          && sect.PSize != 0
          && prev.PSize != 0)
      {
        _sectionsError = true;
        // PRF(printf("\n !!!! Section correction: %s\n ", prev.Name));

        /* we corrected that case in 7-zip before 22.02: */
        // prev.PSize = sect.Pa - prev.Pa;

        /* 22.02: here we can try to change bad section position to expected postion.
           but original Windows code probably will not do same things. */
        // if (prev.PSize <= sect.Va - prev.Va) sect.Pa = prev.Pa + prev.PSize;
      }
    }
    /* last ".its" section in hxs file has incorrect sect.PSize.
       7-zip before 22.02: we reduced section to real sect.VSize */
    /*
    if (sect.VSize == 24 && sect.PSize == 512 && i == (unsigned)_header.NumSections - 1)
      sect.PSize = sect.VSize;
    */
  }

  for (i = 0; i < _sections.Size(); i++)
    _sections[i].UpdateTotalSize(_totalSize);

  bool thereISDebug = false;
  if (IsOpt())
  {
  RINOK(LoadDebugSections(stream, thereISDebug))

  const CDirLink &certLink = _optHeader.DirItems[kDirLink_Certificate];
  if (certLink.Size != 0)
  {
    CSection &sect = _sections.AddNew();
    sect.Name = "CERTIFICATE";
    sect.Va = 0;
    sect.Pa = certLink.Va;
    sect.PSize = sect.VSize = certLink.Size;
    sect.UpdateTotalSize(_totalSize);
  }

  if (thereISDebug)
  {
    /* sometime there is some data after debug section.
       We don't see any reference in exe file to that data.
       But we suppose that it's part of EXE file */

    const UInt32 kAlign = 1 << 12;
    UInt32 alignPos = _totalSize & (kAlign - 1);
    if (alignPos != 0)
    {
      UInt32 size = kAlign - alignPos;
      RINOK(InStream_SeekSet(stream, _totalSize))
      buffer.Alloc(kAlign);
      Byte *buf = buffer;
      size_t processed = size;
      RINOK(ReadStream(stream, buf, &processed))

      /*
      if (processed != 0)
      {
        printf("\ndata after debug %d, %d \n", (int)size, (int)processed);
        fflush(stdout);
      }
      */

      size_t k;
      for (k = 0; k < processed; k++)
        if (buf[k] != 0)
          break;
      if (processed < size && processed < 100)
        _totalSize += (UInt32)processed;
      else if (((_totalSize + k) & 0x1FF) == 0 || processed < size)
        _totalSize += (UInt32)k;
    }
  }
  }

  if (_header.NumSymbols > 0 && _header.PointerToSymbolTable >= optStart)
  {
    if (_header.NumSymbols >= (1 << 24))
      return S_FALSE;
    UInt32 size = _header.NumSymbols * 18;
    RINOK(InStream_SeekSet(stream, (UInt64)_header.PointerToSymbolTable + size))
    Byte buf[4];
    RINOK(ReadStream_FALSE(stream, buf, 4))
    UInt32 size2 = Get32(buf);
    if (size2 >= (1 << 28))
      return S_FALSE;
    size += size2;

    CSection &sect = _sections.AddNew();
    sect.Name = "COFF_SYMBOLS";
    sect.Va = 0;
    sect.Pa = _header.PointerToSymbolTable;
    sect.PSize = sect.VSize = size;
    sect.UpdateTotalSize(_totalSize);
  }

  {
    CObjectVector<CSection> sections = _sections;
    sections.Sort();
    UInt32 limit = (1 << 12);
    unsigned num = 0;
    FOR_VECTOR (k, sections)
    {
      const CSection &s = sections[k];
      if (s.Pa > limit)
      {
        CSection &s2 = _sections.AddNew();
        s2.Pa = s2.Va = limit;
        s2.PSize = s2.VSize = s.Pa - limit;
        s2.IsAdditionalSection = true;
        s2.Name = '[';
        s2.Name.Add_UInt32(num++);
        s2.Name += ']';
        limit = s.Pa;
      }
      UInt32 next = s.Pa + s.PSize;
      if (next < s.Pa)
        break;
      if (next >= limit)
        limit = next;
    }
  }


  if (IsOpt())
  if (_optHeader.CheckSum != 0)
  {
    RINOK(InStream_SeekToBegin(stream))
    UInt32 checkSum = 0;
    RINOK(CalcCheckSum(stream, _totalSize, optStart + k_CheckSum_Field_Offset, checkSum))
    _checksumError = (checkSum != _optHeader.CheckSum);
  }


  if (!_allowTail)
  {
    UInt64 fileSize;
    RINOK(InStream_GetSize_SeekToEnd(stream, fileSize))
    if (fileSize > _totalSize)
      return S_FALSE;
  }

  bool _parseResources = true;
  // _parseResources = false; // for debug

  UInt64 mainSize = 0, mainSize2 = 0;

  for (i = 0; i < _sections.Size(); i++)
  {
    const CSection &sect = _sections[i];
    if (IsOpt())
    if (_parseResources && sect.Name == ".rsrc")
    {
      // 20.01: we try to parse only first copy of .rsrc section.
      _parseResources = false;
      const unsigned numMixItems = _mixItems.Size();
      HRESULT res = OpenResources(i, stream, callback);
      if (res == S_OK)
      {
        _resourcesPrefix = sect.Name.Ptr();
        _resourcesPrefix.Add_PathSepar();
        FOR_VECTOR (j, _items)
        {
          const CResItem &item = _items[j];
          if (item.Enabled)
          {
            CMixItem mixItem;
            mixItem.SectionIndex = (int)i;
            mixItem.ResourceIndex = (int)j;
            if (item.IsRcDataOrUnknown())
            {
              if (item.Size >= mainSize)
              {
                mainSize2 = mainSize;
                mainSize = item.Size;
                _mainSubfile = (Int32)(int)_mixItems.Size();
              }
              else if (item.Size >= mainSize2)
                mainSize2 = item.Size;
            }
            _mixItems.Add(mixItem);
          }
        }
        // 9.29: .rsrc_2 code was commented.
        // .rsrc_1 now must include that .rsrc_2 block.
        /*
        if (sect.PSize > sect.VSize)
        {
          int numBits = _optHeader.GetNumFileAlignBits();
          if (numBits >= 0)
          {
            UInt32 mask = (1 << numBits) - 1;
            UInt32 end = ((sect.VSize + mask) & ~mask);

            if (sect.PSize > end)
            {
              CSection &sect2 = _sections.AddNew();
              sect2.Flags = 0;
              sect2.Pa = sect.Pa + end;
              sect2.Va = sect.Va + end;
              sect2.PSize = sect.PSize - end;
              sect2.VSize = sect2.PSize;
              sect2.Name = ".rsrc_2";
              sect2.Time = 0;
              sect2.IsAdditionalSection = true;
            }
          }
        }
        */
        continue;
      }
      if (res != S_FALSE)
        return res;
      _mixItems.DeleteFrom(numMixItems);
      CloseResources();
    }

    if (sect.IsAdditionalSection)
    {
      if (sect.PSize >= mainSize)
      {
        mainSize2 = mainSize;
        mainSize = sect.PSize;
        _mainSubfile = (Int32)(int)_mixItems.Size();
      }
      else if (sect.PSize >= mainSize2)
        mainSize2 = sect.PSize;
    }
    
    CMixItem mixItem;
    mixItem.SectionIndex = (int)i;
    _mixItems.Add(mixItem);
  }
  
  if (mainSize2 >= (1 << 20) && mainSize < mainSize2 * 2)
    _mainSubfile = -1;

  for (i = 0; i < _mixItems.Size(); i++)
  {
    const CMixItem &mixItem = _mixItems[i];
    if (mixItem.StringIndex < 0 && mixItem.ResourceIndex < 0 && _sections[mixItem.SectionIndex].Name == "_winzip_")
    {
      _mainSubfile = (Int32)(int)i;
      break;
    }
  }

  for (i = 0; i < _versionKeys.Size(); i++)
  {
    if (i != 0)
      _versionFullString.Add_LF();
    const CStringKeyValue &k = _versionKeys[i];
    _versionFullString += k.Key;
    _versionFullString += ": ";
    _versionFullString += k.Value;
  }

  {
    int keyIndex = FindKey(_versionKeys, "OriginalFilename");
    if (keyIndex >= 0)
      _originalFilename = _versionKeys[keyIndex].Value;
  }
  {
    int keyIndex = FindKey(_versionKeys, "FileDescription");
    if (keyIndex >= 0)
      _versionShortString = _versionKeys[keyIndex].Value;
  }
  {
    int keyIndex = FindKey(_versionKeys, "FileVersion");
    if (keyIndex >= 0)
    {
      _versionShortString.Add_Space();
      _versionShortString += _versionKeys[keyIndex].Value;
    }
  }

  return S_OK;
}

Z7_COM7F_IMF(CHandler::Open(IInStream *inStream, const UInt64 *, IArchiveOpenCallback *callback))
{
  COM_TRY_BEGIN
  Close();
  RINOK(Open2(inStream, callback))
  _stream = inStream;
  return S_OK;
  COM_TRY_END
}

void CHandler::CloseResources()
{
  _usedRes.Free();
  _items.Clear();
  _strings.Clear();
  _versionFiles.Clear();
  _buf.Free();
  _versionFullString.Empty();
  _versionShortString.Empty();
  _originalFilename.Empty();
  _versionKeys.Clear();
}

Z7_COM7F_IMF(CHandler::Close())
{
  _totalSize = 0;
  _checksumError = false;
  _sectionsError = false;
  _mainSubfile = -1;

  _stream.Release();
  _sections.Clear();
  _mixItems.Clear();
  CloseResources();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _mixItems.Size();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _mixItems.Size();
  if (numItems == 0)
    return S_OK;
  UInt64 totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    const CMixItem &mixItem = _mixItems[allFilesMode ? i : indices[i]];
    UInt64 size;
    if (mixItem.StringIndex >= 0)
      size = _strings[mixItem.StringIndex].FinalSize();
    else if (mixItem.VersionIndex >= 0)
      size = _versionFiles[mixItem.VersionIndex].Size();
    else if (mixItem.ResourceIndex >= 0)
      size = _items[mixItem.ResourceIndex].GetSize();
    else
      size = _sections[mixItem.SectionIndex].GetSizeExtract();
    totalSize += size;
  }
  extractCallback->SetTotal(totalSize);

  UInt64 currentTotalSize = 0;
  UInt64 currentItemSize;
  
  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder();
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  CLimitedSequentialInStream *streamSpec = new CLimitedSequentialInStream;
  CMyComPtr<ISequentialInStream> inStream(streamSpec);
  streamSpec->SetStream(_stream);

  for (i = 0; i < numItems; i++, currentTotalSize += currentItemSize)
  {
    lps->InSize = lps->OutSize = currentTotalSize;
    RINOK(lps->SetCur())
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    const UInt32 index = allFilesMode ? i : indices[i];

    CMyComPtr<ISequentialOutStream> outStream;
    RINOK(extractCallback->GetStream(index, &outStream, askMode))
    const CMixItem &mixItem = _mixItems[index];

    const CSection &sect = _sections[mixItem.SectionIndex];
    bool isOk = true;
    if (mixItem.StringIndex >= 0)
    {
      const CStringItem &item = _strings[mixItem.StringIndex];
      currentItemSize = item.FinalSize();
      if (!testMode && !outStream)
        continue;

      RINOK(extractCallback->PrepareOperation(askMode))
      if (outStream)
        RINOK(WriteStream(outStream, item.Buf, item.FinalSize()))
    }
    else if (mixItem.VersionIndex >= 0)
    {
      const CByteBuffer &item = _versionFiles[mixItem.VersionIndex];
      currentItemSize = item.Size();
      if (!testMode && !outStream)
        continue;

      RINOK(extractCallback->PrepareOperation(askMode))
      if (outStream)
        RINOK(WriteStream(outStream, item, item.Size()))
    }
    else if (mixItem.ResourceIndex >= 0)
    {
      const CResItem &item = _items[mixItem.ResourceIndex];
      currentItemSize = item.GetSize();
      if (!testMode && !outStream)
        continue;

      RINOK(extractCallback->PrepareOperation(askMode))
      size_t offset = item.Offset - sect.Va;
      if (!CheckItem(sect, item, offset))
        isOk = false;
      else if (outStream)
      {
        if (item.HeaderSize != 0)
          RINOK(WriteStream(outStream, item.Header, item.HeaderSize))
        RINOK(WriteStream(outStream, _buf + offset, item.Size))
      }
    }
    else
    {
      currentItemSize = sect.GetSizeExtract();
      if (!testMode && !outStream)
        continue;
      
      RINOK(extractCallback->PrepareOperation(askMode))
      RINOK(InStream_SeekSet(_stream, sect.Pa))
      streamSpec->Init(currentItemSize);
      RINOK(copyCoder->Code(inStream, outStream, NULL, NULL, progress))
      isOk = (copyCoderSpec->TotalSize == currentItemSize);
    }
    
    outStream.Release();
    RINOK(extractCallback->SetOperationResult(isOk ?
        NExtract::NOperationResult::kOK :
        NExtract::NOperationResult::kDataError))
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetStream(UInt32 index, ISequentialInStream **stream))
{
  COM_TRY_BEGIN
  *stream = NULL;

  const CMixItem &mixItem = _mixItems[index];
  const CSection &sect = _sections[mixItem.SectionIndex];
  if (mixItem.IsSectionItem())
    return CreateLimitedInStream(_stream, sect.Pa, sect.PSize, stream);

  CBufInStream *inStreamSpec = new CBufInStream;
  CMyComPtr<ISequentialInStream> streamTemp = inStreamSpec;
  CReferenceBuf *referenceBuf = new CReferenceBuf;
  CMyComPtr<IUnknown> ref = referenceBuf;
  if (mixItem.StringIndex >= 0)
  {
    const CStringItem &item = _strings[mixItem.StringIndex];
    referenceBuf->Buf.CopyFrom(item.Buf, item.FinalSize());
  }
  else if (mixItem.VersionIndex >= 0)
  {
    const CByteBuffer &item = _versionFiles[mixItem.VersionIndex];
    referenceBuf->Buf.CopyFrom(item, item.Size());
  }
  else
  {
    const CResItem &item = _items[mixItem.ResourceIndex];
    size_t offset = item.Offset - sect.Va;
    if (!CheckItem(sect, item, offset))
      return S_FALSE;
    if (item.HeaderSize == 0)
    {
      CBufInStream *streamSpec = new CBufInStream;
      CMyComPtr<IInStream> streamTemp2 = streamSpec;
      streamSpec->Init(_buf + offset, item.Size, (IInArchive *)this);
      *stream = streamTemp2.Detach();
      return S_OK;
    }
    referenceBuf->Buf.Alloc(item.HeaderSize + item.Size);
    memcpy(referenceBuf->Buf, item.Header, item.HeaderSize);
    if (item.Size != 0)
      memcpy(referenceBuf->Buf + item.HeaderSize, _buf + offset, item.Size);
  }
  inStreamSpec->Init(referenceBuf);

  *stream = streamTemp.Detach();
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::AllowTail(Int32 allowTail))
{
  _allowTail = IntToBool(allowTail);
  return S_OK;
}

static const Byte k_Signature[] = { 'M', 'Z' };

REGISTER_ARC_I(
  "PE", "exe dll sys", NULL, 0xDD,
  k_Signature,
  0,
  NArcInfoFlags::kPreArc,
  IsArc_Pe)

}

namespace NCoff {

API_FUNC_static_IsArc IsArc_Coff(const Byte *p, size_t size)
{
  if (size < NPe::kCoffHeaderSize)
    return k_IsArc_Res_NEED_MORE;
  NPe::CHeader header;
  if (!header.ParseCoff(p))
    return k_IsArc_Res_NO;
  return k_IsArc_Res_YES;
}
}

/*
static const Byte k_Signature[] =
{
    2, 0x4C, 0x01, // x86
    2, 0x64, 0x86, // x64
    2, 0x64, 0xAA  // ARM64
};
REGISTER_ARC_I_CLS(
*/

REGISTER_ARC_I_CLS_NO_SIG(
  NPe::CHandler(true),
  "COFF", "obj", NULL, 0xC6,
  // k_Signature,
  0,
  // NArcInfoFlags::kMultiSignature |
  NArcInfoFlags::kStartOpen,
  IsArc_Coff)
}


namespace NTe {

// Terse Executable (TE) image

struct CDataDir
{
  UInt32 Va;
  UInt32 Size;

  void Parse(const Byte *p)
  {
    G32(0, Va);
    G32(4, Size);
  }
};

static const UInt32 kHeaderSize = 40;

static bool FindValue(const CUInt32PCharPair *pairs, unsigned num, UInt32 value)
{
  for (unsigned i = 0; i < num; i++)
    if (pairs[i].Value == value)
      return true;
  return false;
}

#define MY_FIND_VALUE(pairs, val) FindValue(pairs, Z7_ARRAY_SIZE(pairs), val)
#define MY_FIND_VALUE_2(strings, val) (val < Z7_ARRAY_SIZE(strings) && strings[val])
 
static const UInt32 kNumSection_MAX = 32;

struct CHeader
{
  UInt16 Machine;
  Byte NumSections;
  Byte SubSystem;
  UInt16 StrippedSize;
  /*
  UInt32 AddressOfEntryPoint;
  UInt32 BaseOfCode;
  UInt64 ImageBase;
  */
  CDataDir DataDir[2]; // base relocation and debug directory

  bool ConvertPa(UInt32 &pa) const
  {
    if (pa < StrippedSize)
      return false;
    pa = pa - StrippedSize + kHeaderSize;
    return true;
  }
  bool Parse(const Byte *p);
};

bool CHeader::Parse(const Byte *p)
{
  NumSections = p[4];
  if (NumSections > kNumSection_MAX)
    return false;
  SubSystem = p[5];
  G16(2, Machine);
  G16(6, StrippedSize);
  /*
  G32(8, AddressOfEntryPoint);
  G32(12, BaseOfCode);
  G64(16, ImageBase);
  */
  for (int i = 0; i < 2; i++)
  {
    CDataDir &dd = DataDir[i];
    dd.Parse(p + 24 + i * 8);
    if (dd.Size >= ((UInt32)1 << 28))
      return false;
  }
  return
      MY_FIND_VALUE(NPe::g_MachinePairs, Machine) &&
      MY_FIND_VALUE_2(NPe::g_SubSystems, SubSystem);
}

API_FUNC_static_IsArc IsArc_Te(const Byte *p, size_t size)
{
  if (size < 2)
    return k_IsArc_Res_NEED_MORE;
  if (p[0] != 'V' || p[1] != 'Z')
    return k_IsArc_Res_NO;
  if (size < kHeaderSize)
    return k_IsArc_Res_NEED_MORE;
  
  CHeader h;
  if (!h.Parse(p))
    return k_IsArc_Res_NO;
  return k_IsArc_Res_YES;
}
}


struct CSection
{
  Byte Name[NPe::kNameSize];

  UInt32 VSize;
  UInt32 Va;
  UInt32 PSize;
  UInt32 Pa;
  UInt32 Flags;
  // UInt16 NumRelocs;

  void Parse(const Byte *p)
  {
    memcpy(Name, p, NPe::kNameSize);
    G32(8, VSize);
    G32(12, Va);
    G32(16, PSize);
    G32(20, Pa);
    // G32(p + 32, NumRelocs);
    G32(36, Flags);
  }

  bool Check() const
  {
    return
        Pa <= ((UInt32)1 << 30) &&
        PSize <= ((UInt32)1 << 30);
  }

  void UpdateTotalSize(UInt32 &totalSize)
  {
    UInt32 t = Pa + PSize;
    if (t > totalSize)
      totalSize = t;
  }
};


Z7_CLASS_IMP_CHandler_IInArchive_2(
  IInArchiveGetStream,
  IArchiveAllowTail
)
  CRecordVector<CSection> _items;
  CMyComPtr<IInStream> _stream;
  UInt32 _totalSize;
  bool _allowTail;
  CHeader _h;
  
  HRESULT Open2(IInStream *stream);
public:
  CHandler(): _allowTail(false) {}
};

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidVirtualSize,
  kpidCharacts,
  kpidOffset,
  kpidVa
};

enum
{
  kpidSubSystem = kpidUserDefined
  // , kpidImageBase
};

static const CStatProp kArcProps[] =
{
  // { NULL, kpidHeadersSize, VT_UI4 },
  { NULL, kpidCpu, VT_BSTR},
  { "Subsystem", kpidSubSystem, VT_BSTR },
  // { "Image Base", kpidImageBase, VT_UI8 }
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps_WITH_NAME

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPhySize: prop = _totalSize; break;
    case kpidCpu: PAIR_TO_PROP(NPe::g_MachinePairs, _h.Machine, prop); break;
    case kpidSubSystem: TYPE_TO_PROP(NPe::g_SubSystems, _h.SubSystem, prop); break;
    /*
    case kpidImageBase: prop = _h.ImageBase; break;
    case kpidAddressOfEntryPoint: prop = _h.AddressOfEntryPoint; break;
    case kpidBaseOfCode: prop = _h.BaseOfCode; break;
    */
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  {
    const CSection &item = _items[index];
    switch (propID)
    {
      case kpidPath:
      {
        AString name;
        NPe::GetName(item.Name, name);
        prop = MultiByteToUnicodeString(name);
        break;
      }
      case kpidSize:
      case kpidPackSize: prop = (UInt64)item.PSize; break;
      case kpidVirtualSize: prop = (UInt64)item.VSize; break;
      case kpidOffset: prop = item.Pa; break;
      case kpidVa: prop = item.Va; break;
      case kpidCharacts: FLAGS_TO_PROP(NPe::g_SectFlags, item.Flags, prop); break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

HRESULT CHandler::Open2(IInStream *stream)
{
  Byte h[kHeaderSize];
  RINOK(ReadStream_FALSE(stream, h, kHeaderSize))
  if (h[0] != 'V' || h[1] != 'Z')
    return S_FALSE;
  if (!_h.Parse(h))
    return S_FALSE;

  UInt32 headerSize = NPe::kSectionSize * (UInt32)_h.NumSections;
  CByteArr buf(headerSize);
  RINOK(ReadStream_FALSE(stream, buf, headerSize))
  headerSize += kHeaderSize;

  _totalSize = headerSize;
  _items.ClearAndReserve(_h.NumSections);
  for (UInt32 i = 0; i < _h.NumSections; i++)
  {
    CSection sect;
    sect.Parse(buf + i * NPe::kSectionSize);
    if (!_h.ConvertPa(sect.Pa))
      return S_FALSE;
    if (sect.Pa < headerSize)
      return S_FALSE;
    if (!sect.Check())
      return S_FALSE;
    _items.AddInReserved(sect);
    sect.UpdateTotalSize(_totalSize);
  }

  if (!_allowTail)
  {
    UInt64 fileSize;
    RINOK(InStream_GetSize_SeekToEnd(stream, fileSize))
    if (fileSize > _totalSize)
      return S_FALSE;
  }
  
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Open(IInStream *inStream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback * /* openArchiveCallback */))
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

Z7_COM7F_IMF(CHandler::Close())
{
  _totalSize = 0;
  _stream.Release();
  _items.Clear();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _items.Size();
  return S_OK;
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
    totalSize += _items[allFilesMode ? i : indices[i]].PSize;
  extractCallback->SetTotal(totalSize);

  UInt64 currentTotalSize = 0;
  
  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder();
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  CLimitedSequentialInStream *streamSpec = new CLimitedSequentialInStream;
  CMyComPtr<ISequentialInStream> inStream(streamSpec);
  streamSpec->SetStream(_stream);

  for (i = 0; i < numItems; i++)
  {
    lps->InSize = lps->OutSize = currentTotalSize;
    RINOK(lps->SetCur())
    CMyComPtr<ISequentialOutStream> realOutStream;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    const UInt32 index = allFilesMode ? i : indices[i];
    const CSection &item = _items[index];
    RINOK(extractCallback->GetStream(index, &realOutStream, askMode))
    currentTotalSize += item.PSize;
    
    if (!testMode && !realOutStream)
      continue;
    RINOK(extractCallback->PrepareOperation(askMode))
    int res = NExtract::NOperationResult::kDataError;

    RINOK(InStream_SeekSet(_stream, item.Pa))
    streamSpec->Init(item.PSize);
    RINOK(copyCoder->Code(inStream, realOutStream, NULL, NULL, progress))
    if (copyCoderSpec->TotalSize == item.PSize)
      res = NExtract::NOperationResult::kOK;

    realOutStream.Release();
    RINOK(extractCallback->SetOperationResult(res))
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetStream(UInt32 index, ISequentialInStream **stream))
{
  COM_TRY_BEGIN
  const CSection &item = _items[index];
  return CreateLimitedInStream(_stream, item.Pa, item.PSize, stream);
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::AllowTail(Int32 allowTail))
{
  _allowTail = IntToBool(allowTail);
  return S_OK;
}

static const Byte k_Signature[] = { 'V', 'Z' };

REGISTER_ARC_I(
  "TE", "te", NULL, 0xCF,
  k_Signature,
  0,
  NArcInfoFlags::kPreArc,
  IsArc_Te)

}
}

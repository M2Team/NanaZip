// ElfHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/MyBuffer.h"

#include "../../Windows/PropVariantUtils.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"

// #define Z7_ELF_SHOW_DETAILS

using namespace NWindows;

static UInt16 Get16(const Byte *p, bool be) { if (be) return GetBe16a(p); return GetUi16a(p); }
static UInt32 Get32(const Byte *p, bool be) { if (be) return GetBe32a(p); return GetUi32a(p); }
static UInt64 Get64(const Byte *p, bool be) { if (be) return GetBe64a(p); return GetUi64a(p); }

#define G16(offs, v) v = Get16(p + (offs), be)
#define G32(offs, v) v = Get32(p + (offs), be)
#define G64(offs, v) v = Get64(p + (offs), be)

namespace NArchive {
namespace NElf {

/*
ELF Structure example:
{
  Header
  Program header table (is used at runtime) (list of segment metadata records)
  {
    Segment (Read)
      Segment : PT_PHDR : header table itself
      Segment : PT_INTERP
      Segment : PT_NOTE
      .rela.dyn (RELA, ALLOC)
    Segment (Execute/Read)
      .text section (PROGBITS, SHF_ALLOC | SHF_EXECINSTR)
    Segment (Read)
      .rodata (PROGBITS, SHF_ALLOC | SHF_WRITE)
      Segment : PT_GNU_EH_FRAME
        .eh_frame_hdr
      .eh_frame
      .gcc_except_table
    ...
    Segment (Write/Read) (VaSize > Size)
      Segment (Read) : PT_GNU_RELRO
      Segment (Write/Read)
      .data
      .bss (Size == 0) (VSize != 0)
  }
  .comment (VA == 0)
  .shstrtab (VA == 0)
  Section header table (the data for linking and relocation)
}

  Last top level segment contains .bss section that requires additional VA space.
  So (VaSize > Size) for that segment.

  Segments can be unsorted (by offset) in table.
  Top level segments has Type=PT_LOAD : "Loadable segment".
  Top level segments usually are aligned for page size (4 KB).
  Another segments (non PT_LOAD segments) are inside PT_LOAD segments.

  (VA-offset == 0) is possible for some sections and segments at the beginning of file.
  (VA-offset == 4KB*N) for most sections and segments where (Size != 0),
  (VA-offset != 4KB*N) for .bss section (last section), because (Size == 0),
    and that section is not mapped from image file.
  Some files contain additional "virtual" 4 KB page in VA space after
  end of data of top level segments (PT_LOAD) before new top level segments.
  So (VA-offset) value can increase by 4 KB step.
*/

#define ELF_CLASS_32 1
#define ELF_CLASS_64 2

#define ELF_DATA_2LSB 1
#define ELF_DATA_2MSB 2

static const unsigned kHeaderSize32 = 0x34;
static const unsigned kHeaderSize64 = 0x40;

static const unsigned kSegmentSize32 = 0x20;
static const unsigned kSegmentSize64 = 0x38;

static const unsigned kSectionSize32 = 0x28;
static const unsigned kSectionSize64 = 0x40;

struct CHeader
{
  bool Mode64;
  bool Be;
  Byte Os;
  Byte AbiVer;

  UInt16 Type;
  UInt16 Machine;
  // UInt32 Version;

  // UInt64 EntryVa;
  UInt64 ProgOffset;
  UInt64 SectOffset;
  UInt32 Flags;
  UInt16 HeaderSize;
  UInt16 SegmentEntrySize;
  UInt16 NumSegments;
  UInt16 SectionEntrySize;
  UInt16 NumSections;
  UInt16 NamesSectIndex;

  bool Parse(const Byte *p);

  UInt32 GetHeadersSize() const { return (UInt32)HeaderSize +
      (UInt32)NumSegments * SegmentEntrySize +
      (UInt32)NumSections * SectionEntrySize; }
};

bool CHeader::Parse(const Byte *p)
{
  switch (p[4])
  {
    case ELF_CLASS_32: Mode64 = false; break;
    case ELF_CLASS_64: Mode64 = true; break;
    default: return false;
  }
  bool be;
  switch (p[5])
  {
    case ELF_DATA_2LSB: be = false; break;
    case ELF_DATA_2MSB: be = true; break;
    default: return false;
  }
  Be = be;
  if (p[6] != 1) // Version
    return false;
  Os = p[7];
  // AbiVer = p[8];
  for (int i = 9; i < 16; i++)
    if (p[i] != 0)
      return false;

  G16(0x10, Type);
  G16(0x12, Machine);
  if (Get32(p + 0x14, be) != 1) // Version
    return false;

  if (Mode64)
  {
    // G64(0x18, EntryVa);
    G64(0x20, ProgOffset); // == kHeaderSize64 == 0x40 usually
    G64(0x28, SectOffset);
    p += 0x30;
    // we expect that fields are aligned
    if (ProgOffset & 7) return false;
    if (SectOffset & 7) return false;
  }
  else
  {
    // G32(0x18, EntryVa);
    G32(0x1C, ProgOffset); // == kHeaderSize32 == 0x34 usually
    G32(0x20, SectOffset);
    p += 0x24;
    if (ProgOffset & 3) return false;
    if (SectOffset & 3) return false;
  }

  G32(0, Flags);
  G16(4, HeaderSize);
  if (HeaderSize != (Mode64 ? kHeaderSize64 : kHeaderSize32))
    return false;

  G16(6, SegmentEntrySize);
  G16(8, NumSegments);
  G16(10, SectionEntrySize);
  G16(12, NumSections);
  G16(14, NamesSectIndex);

  if (ProgOffset < HeaderSize && (ProgOffset || NumSegments)) return false;
  if (SectOffset < HeaderSize && (SectOffset || NumSections)) return false;

  if (SegmentEntrySize == 0) { if (NumSegments) return false; }
  else if (SegmentEntrySize != (Mode64 ? kSegmentSize64 : kSegmentSize32)) return false;

  if (SectionEntrySize == 0) { if (NumSections) return false; }
  else if (SectionEntrySize != (Mode64 ? kSectionSize64 : kSectionSize32)) return false;

  return true;
}


#define PT_PHDR 6  // The program header table itself.
#define PT_GNU_STACK 0x6474e551

static const CUInt32PCharPair g_SegnmentTypes[] =
{
  { 0, "Unused" },
  { 1, "Loadable segment" },
  { 2, "Dynamic linking tables" },
  { 3, "Program interpreter path name" },
  { 4, "Note section" },
  { 5, "SHLIB" },
  { 6, "Program header table" },
  { 7, "TLS" },
  { 0x6474e550, "GNU_EH_FRAME" },
  { PT_GNU_STACK, "GNU_STACK" },
  { 0x6474e552, "GNU_RELRO" }
};


static const char * const g_SegmentFlags[] =
{
    "Execute"
  , "Write"
  , "Read"
};

struct CSegment
{
  UInt32 Type;
  UInt32 Flags;
  UInt64 Offset;
  UInt64 Va;
  UInt64 Size;  // size in file
  UInt64 VSize; // size in memory
#ifdef Z7_ELF_SHOW_DETAILS
  UInt64 Pa;    // usually == Va, or == 0
  UInt64 Align; // if (Align != 0), condition must be met:
                //   (VSize % Align == Offset % Alig)
#endif
  void UpdateTotalSize(UInt64 &totalSize)
  {
    const UInt64 t = Offset + Size;
    if (totalSize < t)
        totalSize = t;
  }
  void Parse(const Byte *p, bool mode64, bool be);
};

void CSegment::Parse(const Byte *p, bool mode64, bool be)
{
  G32(0, Type);
  if (mode64)
  {
    G32(4, Flags);
    G64(8, Offset);
    G64(0x10, Va);
    G64(0x20, Size);
    G64(0x28, VSize);
#ifdef Z7_ELF_SHOW_DETAILS
    G64(0x18, Pa);
    G64(0x30, Align);
#endif
  }
  else
  {
    G32(4, Offset);
    G32(8, Va);
    G32(0x10, Size);
    G32(0x14, VSize);
    G32(0x18, Flags);
#ifdef Z7_ELF_SHOW_DETAILS
    G32(0x0C, Pa);
    G32(0x1C, Align);
#endif
  }
}

// Section_index = 0 means NO section

#define SHN_UNDEF 0

// Section types

/*
#define SHT_NULL           0
#define SHT_PROGBITS       1
#define SHT_SYMTAB         2
#define SHT_STRTAB         3
#define SHT_RELA           4
#define SHT_HASH           5
#define SHT_DYNAMIC        6
#define SHT_NOTE           7
*/
#define SHT_NOBITS         8
/*
#define SHT_REL            9
#define SHT_SHLIB         10
#define SHT_DYNSYM        11
#define SHT_UNKNOWN12     12
#define SHT_UNKNOWN13     13
#define SHT_INIT_ARRAY    14
#define SHT_FINI_ARRAY    15
#define SHT_PREINIT_ARRAY 16
#define SHT_GROUP         17
#define SHT_SYMTAB_SHNDX  18
*/

static const CUInt32PCharPair g_SectTypes[] =
{
  { 0, "NULL" },
  { 1, "PROGBITS" },
  { 2, "SYMTAB" },
  { 3, "STRTAB" },
  { 4, "RELA" },
  { 5, "HASH" },
  { 6, "DYNAMIC" },
  { 7, "NOTE" },
  { 8, "NOBITS" },
  { 9, "REL" },
  { 10, "SHLIB" },
  { 11, "DYNSYM" },
  { 12, "UNKNOWN12" },
  { 13, "UNKNOWN13" },
  { 14, "INIT_ARRAY" },
  { 15, "FINI_ARRAY" },
  { 16, "PREINIT_ARRAY" },
  { 17, "GROUP" },
  { 18, "SYMTAB_SHNDX" },

  { 0x6ffffff5, "GNU_ATTRIBUTES" },
  { 0x6ffffff6, "GNU_HASH" },
  { 0x6ffffffd, "GNU_verdef" },
  { 0x6ffffffe, "GNU_verneed" },
  { 0x6fffffff, "GNU_versym" },
  // { 0x70000001, "X86_64_UNWIND" },
  { 0x70000001, "ARM_EXIDX" },
  { 0x70000002, "ARM_PREEMPTMAP" },
  { 0x70000003, "ARM_ATTRIBUTES" },
  { 0x70000004, "ARM_DEBUGOVERLAY" },
  { 0x70000005, "ARM_OVERLAYSECTION" }
};


// SHF_ flags
static const CUInt32PCharPair g_SectionFlags[] =
{
  { 0, "WRITE" },
  { 1, "ALLOC" },
  { 2, "EXECINSTR" },

  { 4, "MERGE" },
  { 5, "STRINGS" },
  { 6, "INFO_LINK" },
  { 7, "LINK_ORDER" },
  { 8, "OS_NONCONFORMING" },
  { 9, "GROUP" },
  { 10, "TLS" },
  { 11, "COMPRESSED" },
  { 12, "DP_SECTION" },
  { 13, "XCORE_SHF_CP_SECTION" },
  { 28, "64_LARGE" },
};

struct CSection
{
  UInt32 Name;
  UInt32 Type;
  UInt64 Flags;
  UInt64 Va;
  UInt64 Offset;
  UInt64 VSize;
  UInt32 Link;
  UInt32 Info;
  UInt64 AddrAlign;
  UInt64 EntSize;

  UInt64 GetSize() const { return Type == SHT_NOBITS ? 0 : VSize; }

  void UpdateTotalSize(UInt64 &totalSize)
  {
    const UInt64 t = Offset + GetSize();
    if (totalSize < t)
        totalSize = t;
  }
  bool Parse(const Byte *p, bool mode64, bool be);
};

bool CSection::Parse(const Byte *p, bool mode64, bool be)
{
  G32(0, Name);
  G32(4, Type);
  if (mode64)
  {
    G64(0x08, Flags);
    G64(0x10, Va);
    G64(0x18, Offset);
    G64(0x20, VSize);
    G32(0x28, Link);
    G32(0x2C, Info);
    G64(0x30, AddrAlign);
    G64(0x38, EntSize);
  }
  else
  {
    G32(0x08, Flags);
    G32(0x0C, Va);
    G32(0x10, Offset);
    G32(0x14, VSize);
    G32(0x18, Link);
    G32(0x1C, Info);
    G32(0x20, AddrAlign);
    G32(0x24, EntSize);
  }
  if (EntSize >= ((UInt32)1 << 31))
    return false;
  if (EntSize >= ((UInt32)1 << 10) &&
      EntSize >= VSize &&
      VSize != 0)
    return false;
  return true;
}


static const char * const g_Machines[] =
{
    "None"
  , "AT&T WE 32100"
  , "SPARC"
  , "Intel 386"
  , "Motorola 68000"
  , "Motorola 88000"
  , "Intel 486"
  , "Intel i860"
  , "MIPS"
  , "IBM S/370"
  , "MIPS RS3000 LE"
  , "RS6000"
  , NULL
  , NULL
  , NULL
  , "PA-RISC"
  , "nCUBE"
  , "Fujitsu VPP500"
  , "SPARC 32+"
  , "Intel i960"
  , "PowerPC"
  , "PowerPC 64-bit"
  , "IBM S/390"
  , "SPU"
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , "NEX v800"
  , "Fujitsu FR20"
  , "TRW RH-32"
  , "Motorola RCE"
  , "ARM"
  , "Alpha-STD"
  , "Hitachi SH"
  , "SPARC-V9"
  , "Siemens Tricore"
  , "ARC"
  , "H8/300"
  , "H8/300H"
  , "H8S"
  , "H8/500"
  , "IA-64"
  , "Stanford MIPS-X"
  , "Motorola ColdFire"
  , "M68HC12"
  , "Fujitsu MMA"
  , "Siemens PCP"
  , "Sony nCPU"
  , "Denso NDR1"
  , "Motorola StarCore"
  , "Toyota ME16"
  , "ST100"
  , "Advanced Logic TinyJ"
  , "AMD64"
  , "Sony DSP"
  , NULL
  , NULL
  , "Siemens FX66"
  , "ST9+"
  , "ST7"
  , "MC68HC16"
  , "MC68HC11"
  , "MC68HC08"
  , "MC68HC05"
  , "Silicon Graphics SVx"
  , "ST19"
  , "Digital VAX"
  , "Axis CRIS"
  , "Infineon JAVELIN"
  , "Element 14 FirePath"
  , "LSI ZSP"
  , "MMIX"
  , "HUANY"
  , "SiTera Prism"
  , "Atmel AVR"
  , "Fujitsu FR30"
  , "Mitsubishi D10V"
  , "Mitsubishi D30V"
  , "NEC v850"
  , "Mitsubishi M32R"
  , "Matsushita MN10300"
  , "Matsushita MN10200"
  , "picoJava"
  , "OpenRISC"
  , "ARC Tangent-A5"
  , "Tensilica Xtensa"
  , "Alphamosaic VideoCore"
  , "Thompson MM GPP"
  , "National Semiconductor 32K"
  , "Tenor Network TPC"
  , "Trebia SNP 1000"
  , "ST200"
  , "Ubicom IP2xxx"
  , "MAX"
  , "NS CompactRISC"
  , "Fujitsu F2MC16"
  , "TI msp430"
  , "Blackfin (DSP)"
  , "SE S1C33"
  , "Sharp embedded"
  , "Arca RISC"
  , "Unicore"
  , "eXcess"
  , "DXP"
  , "Altera Nios II"
  , "NS CRX"
  , "Motorola XGATE"
  , "Infineon C16x/XC16x"
  , "Renesas M16C"
  , "Microchip Technology dsPIC30F"
  , "Freescale CE"
  , "Renesas M32C"
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , "Altium TSK3000"
  , "Freescale RS08"
  , "Analog Devices SHARC"
  , "Cyan Technology eCOG2"
  , "Sunplus S+core7 RISC"
  , "NJR 24-bit DSP"
  , "Broadcom VideoCore III"
  , "Lattice FPGA"
  , "SE C17"
  , "TI TMS320C6000"
  , "TI TMS320C2000"
  , "TI TMS320C55x"
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , "STM 64bit VLIW Data Signal"
  , "Cypress M8C"
  , "Renesas R32C"
  , "NXP TriMedia"
  , "Qualcomm Hexagon"
  , "Intel 8051"
  , "STMicroelectronics STxP7x"
  , "Andes"
  , "Cyan Technology eCOG1X"
  , "Dallas Semiconductor MAXQ30"
  , "NJR 16-bit DSP"
  , "M2000"
  , "Cray NV2"
  , "Renesas RX"
  , "Imagination Technologies META"
  , "MCST Elbrus"
  , "Cyan Technology eCOG16"
  , "National Semiconductor CR16"
  , "Freescale ETPUnit"
  , "Infineon SLE9X"
  , "Intel L10M"
  , "Intel K10M"
  , NULL
  , "ARM64"
  , NULL
  , "Atmel AVR32"
  , "STM8"
  , "Tilera TILE64"
  , "Tilera TILEPro"
  , "Xilinx MicroBlaze"
  , "NVIDIA CUDA"
  , "Tilera TILE-Gx"
  , "CloudShield"
  , "KIPO-KAIST Core-A 1st"
  , "KIPO-KAIST Core-A 2nd"
  , "Synopsys ARCompact V2"
  , "Open8"
  , "Renesas RL78"
  , "Broadcom VideoCore V"
  , "Renesas 78KOR"
  , "Freescale 56800EX" // 200
};

static const CUInt32PCharPair g_MachinePairs[] =
{
  { 243, "RISC-V" },
  { 258, "LoongArch" },
  { 0x9026, "Alpha" },  // EM_ALPHA_EXP, obsolete, (used by NetBSD/alpha) (written in the absence of an ABI)
  { 0xbaab, "Xilinx MicroBlaze" }
};

static const CUInt32PCharPair g_OS[] =
{
  { 0, "None" },
  { 1, "HP-UX" },
  { 2, "NetBSD" },
  { 3, "Linux" },
  { 4, "Hurd" },

  { 6, "Solaris" },
  { 7, "AIX" },
  { 8, "IRIX" },
  { 9, "FreeBSD" },
  { 10, "TRU64" },
  { 11, "Novell Modesto" },
  { 12, "OpenBSD" },
  { 13, "OpenVMS" },
  { 14, "HP NSK" },
  { 15, "AROS" },
  { 16, "FenixOS" },
  { 17, "CloudABI" },
  { 18, "OpenVOS" },
  { 64, "Bare-metal TMS320C6000" },
  { 65, "Linux TMS320C6000" },
  { 97, "ARM" },
  { 255, "Standalone" }
};

#define k_Machine_MIPS 8
#define k_Machine_ARM 40
#define k_Machine_RISCV 243

/*
#define EF_ARM_ABIMASK        0xFF000000
#define EF_ARM_BE8            0x00800000
#define EF_ARM_GCCMASK        0x00400FFF
#define EF_ARM_ABI_FLOAT_SOFT 0x00000200
#define EF_ARM_ABI_FLOAT_HARD 0x00000400
*/

static const CUInt32PCharPair g_ARM_Flags[] =
{
  { 1, "HasEntry" },
  { 9, "SF" },
  { 10, "HF" },
  { 23, "BE8" }
};


static const CUInt32PCharPair g_MIPS_Flags[] =
{
  { 0, "NOREORDER" },
  { 1, "PIC" },
  { 2, "CPIC" },
  { 3, "XGOT" },
  { 4, "64BIT_WHIRL" },
  { 5, "ABI2" },
  { 6, "ABI_ON32" },
  { 10, "NAN2008" },
  { 25, "MicroMIPS" },
  { 26, "M16" },
  { 27, "MDMX" }
};

static const char * const g_RISCV_Flags[] =
{
  "RVC",
  NULL,
  NULL,
  "RVE",
  "TSO"
};


// #define ET_NONE 0
#define ET_REL  1
// #define ET_EXEC 2
#define ET_DYN  3
// #define ET_CORE 4

static const char * const g_Types[] =
{
    "None"
  , "Relocatable file"
  , "Executable file"
  , "Shared object file"
  , "Core file"
};




Z7_CLASS_IMP_CHandler_IInArchive_1(
    IArchiveAllowTail
)
  CRecordVector<CSegment> _segments;
  CRecordVector<CSection> _sections;
  CByteBuffer _namesData;
  CMyComPtr<IInStream> _inStream;
  UInt64 _totalSize;
  CHeader _header;
  bool _headersError;
  bool _allowTail;
  bool _stackFlags_Defined;
  UInt32 _stackFlags;

  void GetSectionName(UInt32 index, NCOM::CPropVariant &prop, bool showNULL) const;
  HRESULT Open2(IInStream *stream);
public:
  CHandler(): _allowTail(false) {}
};

void CHandler::GetSectionName(UInt32 index, NCOM::CPropVariant &prop, bool showNULL) const
{
  if (index >= _sections.Size())
    prop = index; // it's possible for some file, but maybe it's ERROR case
  else
  {
    const CSection &section = _sections[index];
    const UInt32 offset = section.Name;
    if (index == SHN_UNDEF /* && section.Type == SHT_NULL && offset == 0 */)
    {
      if (showNULL)
        prop = "NULL";
      return;
    }
    const Byte *p = _namesData;
    const size_t size = _namesData.Size();
    for (size_t i = offset; i < size; i++)
      if (p[i] == 0)
      {
        prop = (const char *)(p + offset);
        return;
      }
    prop = "ERROR";
  }
}

static const Byte kArcProps[] =
{
  kpidCpu,
  kpidBit64,
  kpidBigEndian,
  kpidHostOS,
  kpidCharacts,
  kpidComment,
  kpidHeadersSize
};

enum
{
  kpidLinkSection = kpidUserDefined,
  kpidInfoSection,
  kpidEntrySize
#ifdef Z7_ELF_SHOW_DETAILS
  // , kpidAlign
  , kpidPa
  , kpidDelta
  , kpidOffsetEnd
#endif
};

static const CStatProp kProps[] =
{
  { NULL, kpidPath, VT_BSTR },
  { NULL, kpidSize, VT_UI8 },
  { NULL, kpidVirtualSize, VT_UI8 },
  { NULL, kpidOffset, VT_UI8 },
  { NULL, kpidVa, VT_UI8 },
  { NULL, kpidType, VT_BSTR },
  { NULL, kpidCharacts, VT_BSTR }
#ifdef Z7_ELF_SHOW_DETAILS
  // , { "Align", kpidAlign, VT_UI8 }
  , { NULL, kpidClusterSize, VT_UI8 }
  , { "PA", kpidPa, VT_UI8 }
  , { "End offset", kpidOffsetEnd, VT_UI8 }
  , { "Delta (VA-Offset)", kpidDelta, VT_UI8 }
#endif
  , { "Entry Size", kpidEntrySize, VT_UI8}
  , { "Link Section", kpidLinkSection, VT_BSTR}
  , { "Info Section", kpidInfoSection, VT_BSTR}
};

IMP_IInArchive_Props_WITH_NAME
IMP_IInArchive_ArcProps

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPhySize: prop = _totalSize; break;
    case kpidHeadersSize: prop = _header.GetHeadersSize(); break;
    case kpidBit64: if (_header.Mode64) prop = _header.Mode64; break;
    case kpidBigEndian: if (_header.Be) prop = _header.Be; break;
    case kpidShortComment:

    case kpidCpu:
    {
      AString s;
      if (_header.Machine < Z7_ARRAY_SIZE(g_Machines))
      {
        const char *name = g_Machines[_header.Machine];
        if (name)
          s = name;
      }
      if (s.IsEmpty())
        s = TypePairToString(g_MachinePairs, Z7_ARRAY_SIZE(g_MachinePairs), _header.Machine);
      UInt32 flags = _header.Flags;
      if (flags)
      {
        s.Add_Space();
        if (_header.Machine == k_Machine_ARM)
        {
          s += FlagsToString(g_ARM_Flags, Z7_ARRAY_SIZE(g_ARM_Flags), flags & (((UInt32)1 << 24) - 1));
          s += " ABI:";
          s.Add_UInt32(flags >> 24);
        }
        else if (_header.Machine == k_Machine_MIPS)
        {
          const UInt32 ver = flags >> 28;
          s.Add_Char('v');
          s.Add_UInt32(ver);
          flags &= ((UInt32)1 << 28) - 1;
          const UInt32 abi = (flags >> 12) & 7;
          if (abi)
          {
            s += " ABI:";
            s.Add_UInt32(abi);
          }
          flags &= ~((UInt32)7 << 12);
          s.Add_Space();
          s += FlagsToString(g_MIPS_Flags, Z7_ARRAY_SIZE(g_MIPS_Flags), flags);
        }
        else if (_header.Machine == k_Machine_RISCV)
        {
          s += "FLOAT_";
          const UInt32 fl = (flags >> 1) & 3;
          /*
          static const char * const g_RISCV_Flags_Float[] =
            { "SOFT", "SINGLE", "DOUBLE", "QUAD" };
          s += g_RISCV_Flags_Float[fl];
          */
          if (fl)
            s.Add_UInt32(16u << fl);
          else
            s += "SOFT";
          s.Add_Space();
          flags &= ~(UInt32)6;
          s += FlagsToString(g_RISCV_Flags, Z7_ARRAY_SIZE(g_RISCV_Flags), flags);
        }
#if 0
#define k_Machine_LOONGARCH 258
        else if (_header.Machine == k_Machine_LOONGARCH)
        {
          s += "ABI:";
          s.Add_UInt32((flags >> 6) & 3);
          s.Add_Dot();
          s.Add_UInt32((flags >> 3) & 7);
          s.Add_Dot();
#if 1
          s.Add_UInt32(flags & 7);
#else
          static const char k_LoongArch_Float_Type[8] = { '0', 's', 'f', 'd', '4' ,'5', '6', '7' };
          s.Add_Char(k_LoongArch_Float_Type[flags & 7]);
#endif
          flags &= ~(UInt32)0xff;
          if (flags)
          {
            s.Add_Colon();
            char sz[16];
            ConvertUInt32ToHex(flags, sz);
            s += sz;
          }
        }
#endif
        else
        {
          char sz[16];
          ConvertUInt32ToHex(flags, sz);
          s += sz;
        }
      }
      prop = s;
      break;
    }

    case kpidHostOS: PAIR_TO_PROP(g_OS, _header.Os, prop); break;
    case kpidCharacts: TYPE_TO_PROP(g_Types, _header.Type, prop); break;
    case kpidComment:
    {
      AString s;
      if (_stackFlags_Defined)
      {
        s += "STACK: ";
        s += FlagsToString(g_SegmentFlags, Z7_ARRAY_SIZE(g_SegmentFlags), _stackFlags);
        s.Add_LF();
        /*
        if (_header.EntryVa)
        {
          s += "Entry point: 0x";
          char temp[16 + 4];
          ConvertUInt64ToHex(_header.EntryVa, temp);
          s += temp;
          s.Add_LF();
        }
        */
      }
      if (_header.NumSegments)
      {
        s += "Segments: ";
        s.Add_UInt32(_header.NumSegments);
        s.Add_LF();
      }
      if (_header.NumSections)
      {
        s += "Sections: ";
        s.Add_UInt32(_header.NumSections);
        s.Add_LF();
      }
      prop = s;
      break;
    }
    case kpidExtension:
    {
      const char *s = NULL;
      if (_header.Type == ET_DYN)
        s = "so";
      else if (_header.Type == ET_REL)
        s = "o";
      if (s)
        prop = s;
      break;
    }
    // case kpidIsSelfExe: prop = (_header.Type != ET_DYN)  && (_header.Type == ET_REL); break;
    case kpidErrorFlags:
    {
      UInt32 flags = 0;
      if (_headersError) flags |= kpv_ErrorFlags_HeadersError;
      if (flags != 0)
        prop = flags;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  if (index < _segments.Size())
  {
    const CSegment &item = _segments[index];
    switch (propID)
    {
      case kpidPath:
      {
        char sz[16];
        ConvertUInt32ToString(index, sz);
        prop = sz;
        break;
      }
      case kpidOffset: prop = item.Offset; break;
      case kpidVa: prop = item.Va; break;
#ifdef Z7_ELF_SHOW_DETAILS
      case kpidDelta: if (item.Va) { prop = item.Va - item.Offset; } break;
      case kpidOffsetEnd: prop = item.Offset + item.Size; break;
      case kpidPa: prop = item.Pa; break;
      case kpidClusterSize: prop = item.Align; break;
#endif
      case kpidSize:
      case kpidPackSize: prop = (UInt64)item.Size; break;
      case kpidVirtualSize: prop = (UInt64)item.VSize; break;
      case kpidType: PAIR_TO_PROP(g_SegnmentTypes, item.Type, prop); break;
      case kpidCharacts: FLAGS_TO_PROP(g_SegmentFlags, item.Flags, prop); break;
    }
  }
  else
  {
    index -= _segments.Size();
    const CSection &item = _sections[index];
    switch (propID)
    {
      case kpidPath: GetSectionName(index, prop, true); break;
      case kpidOffset: prop = item.Offset; break;
      case kpidVa: prop = item.Va; break;
#ifdef Z7_ELF_SHOW_DETAILS
      case kpidDelta: if (item.Va) { prop = item.Va - item.Offset; } break;
      case kpidOffsetEnd: prop = item.Offset + item.GetSize(); break;
#endif
      case kpidSize:
      case kpidPackSize: prop = (UInt64)(item.Type == SHT_NOBITS ? 0 : item.VSize); break;
      case kpidVirtualSize: prop = item.GetSize(); break;
      case kpidType: PAIR_TO_PROP(g_SectTypes, item.Type, prop); break;
      case kpidCharacts: FLAGS_TO_PROP(g_SectionFlags, (UInt32)item.Flags, prop); break;
      // case kpidAlign: prop = item.Align; break;
      case kpidLinkSection: GetSectionName(item.Link, prop, false); break;
      case kpidInfoSection: GetSectionName(item.Info, prop, false); break;
      case kpidEntrySize: prop = (UInt64)item.EntSize; break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

HRESULT CHandler::Open2(IInStream *stream)
{
  {
    const UInt32 kStartSize = kHeaderSize64;
    UInt64 h64[kStartSize / 8];
    RINOK(ReadStream_FALSE(stream, h64, kStartSize))
    const Byte *h = (const Byte *)(const void *)h64;
    if (GetUi32a(h) != 0x464c457f)
      return S_FALSE;
    if (!_header.Parse(h))
      return S_FALSE;
  }

  _totalSize = _header.HeaderSize;

  bool addSegments = false;
  bool addSections = false;
  // first section usually is NULL (with zero offsets and zero sizes).
  if (_header.NumSegments == 0 || _header.NumSections > 1)
    addSections = true;
  else
    addSegments = true;
#ifdef Z7_ELF_SHOW_DETAILS
  addSections = true;
  addSegments = true;
#endif

  if (_header.NumSegments)
  {
    if (_header.ProgOffset > (UInt64)1 << 60) return S_FALSE;
    RINOK(InStream_SeekSet(stream, _header.ProgOffset))
    const size_t size = (size_t)_header.SegmentEntrySize * _header.NumSegments;
    CByteArr buf(size);
    RINOK(ReadStream_FALSE(stream, buf, size))
    {
      const UInt64 total = _header.ProgOffset + size;
      if (_totalSize < total)
          _totalSize = total;
    }
    if (addSegments)
      _segments.ClearAndReserve(_header.NumSegments);
    const Byte *p = buf;
    for (unsigned i = 0; i < _header.NumSegments; i++, p += _header.SegmentEntrySize)
    {
      CSegment seg;
      seg.Parse(p, _header.Mode64, _header.Be);
      seg.UpdateTotalSize(_totalSize);
      if (seg.Type == PT_GNU_STACK && !_stackFlags_Defined)
      {
        _stackFlags = seg.Flags;
        _stackFlags_Defined = true;
      }
      if (addSegments
          // we don't show program header table segment
          && seg.Type != PT_PHDR
          )
        _segments.AddInReserved(seg);
    }
  }

  if (_header.NumSections)
  {
    if (_header.SectOffset > (UInt64)1 << 60) return S_FALSE;
    RINOK(InStream_SeekSet(stream, _header.SectOffset))
    const size_t size = (size_t)_header.SectionEntrySize * _header.NumSections;
    CByteArr buf(size);
    RINOK(ReadStream_FALSE(stream, buf, size))
    {
      const UInt64 total = _header.SectOffset + size;
      if (_totalSize < total)
          _totalSize = total;
    }
    if (addSections)
      _sections.ClearAndReserve(_header.NumSections);
    const Byte *p = buf;
    for (unsigned i = 0; i < _header.NumSections; i++, p += _header.SectionEntrySize)
    {
      CSection sect;
      if (!sect.Parse(p, _header.Mode64, _header.Be))
      {
        _headersError = true;
        return S_FALSE;
      }
      sect.UpdateTotalSize(_totalSize);
      if (addSections)
        _sections.AddInReserved(sect);
    }
  }

  if (addSections)
  {
    if (_header.NamesSectIndex < _sections.Size())
    {
      const CSection &sect = _sections[_header.NamesSectIndex];
      const UInt64 size = sect.GetSize();
      if (size && size < ((UInt64)1 << 31)
          && (Int64)sect.Offset >= 0)
      {
        _namesData.Alloc((size_t)size);
        RINOK(InStream_SeekSet(stream, sect.Offset))
        RINOK(ReadStream_FALSE(stream, _namesData, (size_t)size))
      }
    }
    /*
    // we cannot delete "NULL" sections,
    // because we have links to sections array via indexes
    for (int i = _sections.Size() - 1; i >= 0; i--)
      if (_sections[i].Type == SHT_NULL)
        _items.Delete(i);
    */
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
  RINOK(Open2(inStream))
  _inStream = inStream;
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _totalSize = 0;
  _headersError = false;
  _stackFlags_Defined = false;

  _inStream.Release();
  _segments.Clear();
  _sections.Clear();
  _namesData.Free();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _segments.Size() + _sections.Size();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _segments.Size() + _sections.Size();
  if (numItems == 0)
    return S_OK;
  UInt64 totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    const UInt32 index = allFilesMode ? i : indices[i];
    totalSize += (index < _segments.Size()) ?
        _segments[index].Size :
        _sections[index - _segments.Size()].GetSize();
  }
  RINOK(extractCallback->SetTotal(totalSize))

  totalSize = 0;
  UInt64 currentItemSize;
  
  CMyComPtr2_Create<ICompressCoder, NCompress::CCopyCoder> copyCoder;
  CMyComPtr2_Create<ICompressProgressInfo, CLocalProgress> lps;
  lps->Init(extractCallback, false);
  CMyComPtr2_Create<ISequentialInStream, CLimitedSequentialInStream> inStream;
  inStream->SetStream(_inStream);

  for (i = 0;; i++, totalSize += currentItemSize)
  {
    lps->InSize = lps->OutSize = totalSize;
    RINOK(lps->SetCur())
    if (i >= numItems)
      break;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    const UInt32 index = allFilesMode ? i : indices[i];
    UInt64 offset;
    if (index < _segments.Size())
    {
      const CSegment &item = _segments[index];
      currentItemSize = item.Size;
      offset = item.Offset;
    }
    else
    {
      const CSection &item = _sections[index - _segments.Size()];
      currentItemSize = item.GetSize();
      offset = item.Offset;
    }
    {
      CMyComPtr<ISequentialOutStream> outStream;
      RINOK(extractCallback->GetStream(index, &outStream, askMode))
      if (!testMode && !outStream)
        continue;
      RINOK(extractCallback->PrepareOperation(askMode))
      RINOK(InStream_SeekSet(_inStream, offset))
      inStream->Init(currentItemSize);
      RINOK(copyCoder.Interface()->Code(inStream, outStream, NULL, NULL, lps))
    }
    RINOK(extractCallback->SetOperationResult(copyCoder->TotalSize == currentItemSize ?
        NExtract::NOperationResult::kOK:
        NExtract::NOperationResult::kDataError))
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::AllowTail(Int32 allowTail))
{
  _allowTail = IntToBool(allowTail);
  return S_OK;
}

static const Byte k_Signature[] = { 0x7F, 'E', 'L', 'F' };

REGISTER_ARC_I(
  "ELF", "elf", NULL, 0xDE,
  k_Signature,
  0,
  NArcInfoFlags::kPreArc,
  NULL)

}}

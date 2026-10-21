// MachoHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"
#include "../../../C/SwapBytes.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"

#include "../../Windows/PropVariantUtils.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"

static UInt32 Get32(const Byte *p, bool be) { if (be) return GetBe32a(p); return GetUi32a(p); }
static UInt64 Get64(const Byte *p, bool be) { if (be) return GetBe64a(p); return GetUi64a(p); }

using namespace NWindows;
using namespace NCOM;

namespace NArchive {
namespace NMacho {

#define CPU_ARCH_ABI64      (1 << 24)
#define CPU_ARCH_ABI64_32   (1 << 25)
#define CPU_ARCH_MASK_CPU   (~(UInt32)(CPU_ARCH_ABI64 | CPU_ARCH_ABI64_32))

#define CPU_TYPE_I386   7
#define CPU_TYPE_ARM    12
#define CPU_TYPE_PPC    18
#define CPU_TYPE_AMD64  (CPU_ARCH_ABI64 | CPU_TYPE_I386)
#define CPU_TYPE_ARM64  (CPU_ARCH_ABI64 | CPU_TYPE_ARM)
#define CPU_TYPE_PPC64  (CPU_ARCH_ABI64 | CPU_TYPE_PPC)
#define CPU_TYPE_ARM64_32 (CPU_ARCH_ABI64_32 | CPU_TYPE_ARM)

// #define CPU_SUBTYPE_I386_ALL   3
// #define CPU_SUBTYPE_X86_64_H   8  // Haswell+
// #define CPU_SUBTYPE_ARM64_ALL  0
// #define CPU_SUBTYPE_ARM64_V8   1
// #define CPU_SUBTYPE_ARM64E     2  // ARMv8.3+ and Pointer Authentication / PAC (A12/M1)+
#define CPU_SUBTYPE_MASK_FLAGS  0xff000000  // mask for feature flags
#define CPU_SUBTYPE_LIB64       ((UInt32)1 << 31)
// #define CPU_SUBTYPE_ARM64E_VERSIONED_ABI_MASK  CPU_SUBTYPE_LIB64 // it's same bit
// #define CPU_SUBTYPE_ARM64_PTR_AUTH_MASK       0x0f000000
// #define CPU_SUBTYPE_ARM64E_KERNEL_ABI_MASK    0x40000000
// #define CPU_SUBTYPE_ARM64E_PTR_AUTH_MASK      0x3f000000

struct CCpuRecord
{
  UInt32 Type;
  UInt32 SubType;
  const char *Name;
};

static const CCpuRecord k_CpuRecords[] =
{
  { CPU_TYPE_ARM64, 0, "arm64" }, // CPU_SUBTYPE_ARM64_ALL
  { CPU_TYPE_ARM64, 1, "v8" },    // CPU_SUBTYPE_ARM64_V8
  { CPU_TYPE_ARM64, 2, "e" },     // CPU_SUBTYPE_ARM64E
  { CPU_TYPE_ARM64, 12, "e.x1" }, // CPU_SUBTYPE_ARM64E_X1

  { CPU_TYPE_ARM64_32, 1, "arm64_32" }, // CPU_SUBTYPE_ARM64_32_V8

  { CPU_TYPE_AMD64, 3, "x64" }, // "x86_64"  : CPU_SUBTYPE_X86_64_ALL
  { CPU_TYPE_AMD64, 8, "h" },   // "x86_64h" : CPU_SUBTYPE_X86_64_H

  { CPU_TYPE_PPC64, 0, "ppc64" },
  { CPU_TYPE_PPC64, 100, "-970" }, // "ppc970-64"

// 32-bit types must after 64-bit types:

  { CPU_TYPE_ARM, 0, "arm" },
  { CPU_TYPE_ARM, 1, "a500_arch" },
  { CPU_TYPE_ARM, 2, "a500" },
  { CPU_TYPE_ARM, 3, "a440" },
  { CPU_TYPE_ARM, 4, "m4" },
  { CPU_TYPE_ARM, 5, "v4t" },
  { CPU_TYPE_ARM, 6, "v6" },
  { CPU_TYPE_ARM, 7, "v5tej" },
  { CPU_TYPE_ARM, 8, "xscale" },
  { CPU_TYPE_ARM, 9, "v7" },
  { CPU_TYPE_ARM, 10, "v7f" },
  { CPU_TYPE_ARM, 11, "v7s" },
  { CPU_TYPE_ARM, 12, "v7k" },
  { CPU_TYPE_ARM, 13, "v8" },
  { CPU_TYPE_ARM, 14, "v6m" },
  { CPU_TYPE_ARM, 15, "v7m" },
  { CPU_TYPE_ARM, 16, "v7em" },
  { CPU_TYPE_ARM, 17, "v8m.main" }, // V8M_MAIN
  { CPU_TYPE_ARM, 18, "v8m.base" },
  { CPU_TYPE_ARM, 19, "v8.1m.main" },

  { CPU_TYPE_I386, 3, "x86" }, // "i386" : CPU_SUBTYPE_I386_ALL
  { CPU_TYPE_I386, 4, "_i486" },
  { CPU_TYPE_I386, 0x84, "_i486sx" },
  { CPU_TYPE_I386, 5, "_pentium" },
  { CPU_TYPE_I386, 0x16, "_pentpro" }, // "i686"
  { CPU_TYPE_I386, 0x36, "_pentIIm3" },
  { CPU_TYPE_I386, 0x56, "_pentIIm5" },
  { CPU_TYPE_I386, 10, "_pentium4" },

  { CPU_TYPE_PPC, 0, "ppc" },
  { CPU_TYPE_PPC, 1, "601" },
  { CPU_TYPE_PPC, 2, "602" },
  { CPU_TYPE_PPC, 3, "603" },
  { CPU_TYPE_PPC, 4, "603e" },
  { CPU_TYPE_PPC, 5, "603ev" },
  { CPU_TYPE_PPC, 6, "604" },
  { CPU_TYPE_PPC, 7, "604e" },
  { CPU_TYPE_PPC, 8, "620" },
  { CPU_TYPE_PPC, 9, "750" },
  { CPU_TYPE_PPC, 10, "7400" },
  { CPU_TYPE_PPC, 11, "7450" },
  { CPU_TYPE_PPC, 100, "970" },

  { 24, 0, "riscv" },
  { 6, 1, "m68k" },
  { 11, 0, "hppa" },
  { 14, 0, "sparc" }
  // { 1, 0, "vax" },
  // { 2, 0, "romp" },
  // { 4, 0, "ns32032" },
  // { 5, 0, "ns32332" },
  // { 8, 0, "mips" },
  // { 9, 0, "ns32532" },
  // { 10, 0, "mc98000 " },
  // { 13, 0, "m88k" },
  // { 15, 0, "i860" },
  // { 16, 0, "i860_little" },
  // { 17, 0, "rs6000" },
  // { 255, 0, "veo" }
};


// LC_* constants
#define LC_SEGMENT_32           1
#define LC_SYMTAB               2
#define LC_SEGMENT_64           0x19
#define LC_UUID                 0x1b
#define LC_VERSION_MIN_MACOSX   0x24
#define LC_VERSION_MIN_IPHONEOS 0x25
#define LC_VERSION_MIN_TVOS     0x2f
#define LC_VERSION_MIN_WATCHOS  0x30
#define LC_SOURCE_VERSION       0x2a
#define LC_BUILD_VERSION        0x32

#define SECT_TYPE_MASK 0x000000ff
#define SECT_ATTR_MASK 0xffffff00

#define SECT_ATTR_ZEROFILL 1

static const char * const g_SectTypes[] =
{
    "REGULAR"
  , "ZEROFILL"
  , "CSTRINGS"
  , "4BYTE_LITERALS"
  , "8BYTE_LITERALS"
  , "LITERAL_POINTERS"
  , "NON_LAZY_SYMBOL_POINTERS"
  , "LAZY_SYMBOL_POINTERS"
  , "SYMBOL_STUBS"
  , "MOD_INIT_FUNC_POINTERS"
  , "MOD_TERM_FUNC_POINTERS"
  , "COALESCED"
  , "GB_ZEROFILL"
  , "INTERPOSING"
  , "16BYTE_LITERALS"
  , "DTRACE_DOF"
  , "LAZY_DYLIB_SYMBOL_POINTERS"
  , "THREAD_LOCAL_REGULAR"
  , "THREAD_LOCAL_ZEROFILL"
  , "THREAD_LOCAL_VARIABLES"
  , "THREAD_LOCAL_VARIABLE_POINTERS"
  , "THREAD_LOCAL_INIT_FUNCTION_POINTERS"
};

// MH_*
enum EFileType
{
  kType_OBJECT = 1,
  kType_EXECUTE,
  kType_FVMLIB,
  kType_CORE,
  kType_PRELOAD,
  kType_DYLIB,
  kType_DYLINKER,
  kType_BUNDLE,
  kType_DYLIB_STUB,
  kType_DSYM
};

static const char * const g_FileTypes[] =
{
    "0"
  , "OBJECT"
  , "EXECUTE"
  , "FVMLIB"
  , "CORE"
  , "PRELOAD"
  , "DYLIB"
  , "DYLINKER"
  , "BUNDLE"
  , "DYLIB_STUB"
  , "DSYM"
};


static const char * const g_ArcFlags[] =
{
    "NOUNDEFS"
  , "INCRLINK"
  , "DYLDLINK"
  , "BINDATLOAD"
  , "PREBOUND"
  , "SPLIT_SEGS"
  , "LAZY_INIT"
  , "TWOLEVEL"
  , "FORCE_FLAT"
  , "NOMULTIDEFS"
  , "NOFIXPREBINDING"
  , "PREBINDABLE"
  , "ALLMODSBOUND"
  , "SUBSECTIONS_VIA_SYMBOLS"
  , "CANONICAL"
  , "WEAK_DEFINES"
  , "BINDS_TO_WEAK"
  , "ALLOW_STACK_EXECUTION"
  , "ROOT_SAFE"
  , "SETUID_SAFE"
  , "NO_REEXPORTED_DYLIBS"
  , "PIE"
  , "DEAD_STRIPPABLE_DYLIB"
  , "HAS_TLV_DESCRIPTORS"
  , "NO_HEAP_EXECUTION"
};


static const CUInt32PCharPair g_SectFlags[] =
{
  { 31, "PURE_INSTRUCTIONS" },
  { 30, "NO_TOC" },
  { 29, "STRIP_STATIC_SYMS" },
  { 28, "NO_DEAD_STRIP" },
  { 27, "LIVE_SUPPORT" },
  { 26, "SELF_MODIFYING_CODE" },
  { 25, "DEBUG" },
  { 10, "SOME_INSTRUCTIONS" },
  {  9, "EXT_RELOC" },
  {  8, "LOC_RELOC" }
};


// VM_PROT_*
static const char * const g_SegmentProt[] =
{
    "READ"
  , "WRITE"
  , "EXECUTE"
  /*
  , "NO_CHANGE"
  , "COPY"
  , "TRUSTED"
  , "IS_MASK"
  */
};

// SG_*
static const char * const g_SegmentFlags[] =
{
    "SG_HIGHVM"
  , "SG_FVMLIB"
  , "SG_NORELOC"
  , "SG_PROTECTED_VERSION_1"
  , "SG_READ_ONLY"
};

// PLATFORM_*
static const char * const g_Platform_Names[] =
{
    NULL
  , "MACOS"
  , "IOS"
  , "TVOS"
  , "WATCHOS"
  , "BRIDGEOS"
  , "MACCATALYST"
  , "IOSSIM"
  , "TVOSSIM"
  , "WATCHOSSIM"
  , "DRIVERKIT"
};

// build_tool_version::tool
// TOOL_*
static const char * const g_Tool_Names[] =
{
    NULL
  , "CLANG"
  , "SWIFT"
  , "LD"
  , "LLD"
};

static const unsigned kNameSize = 16;

struct CSegment
{
  // char Name[kNameSize]; // can be empty in some cases
  UInt32 MaxProt;
  UInt32 InitProt;
  UInt32 Flags;
};

struct CSection
{
  char Name[kNameSize * 2 + 1];
  // char SegName[kNameSize];
  bool IsSection;
  bool Seg_isDefined;
  CSegment Seg;

  UInt64 Va;
  UInt64 VSize;
  UInt64 Pa;
  UInt64 PSize;
  UInt32 Align;
  UInt32 Flags;
  // int SegmentIndex;

  // UInt64 GetPackSize() const { return Flags == SECT_ATTR_ZEROFILL ? 0 : Size; }
  UInt64 GetPackSize() const { return PSize; }
};


static const unsigned k_uuidSize = 16;

Z7_CLASS_IMP_CHandler_IInArchive_1(
  IArchiveAllowTail
)
  CMyComPtr<IInStream> _inStream;
  // CRecordVector<CSegment> _segments;
  CRecordVector<CSection> _sections;
  bool _allowTail;
  bool _be;
  bool _uuid_IsDefined;
  Byte _mode64;
  UInt32 _cpuType;
  UInt32 _cpuSubType;
  UInt32 _type;
  UInt32 _flags;
  UInt32 _headersSize;
  UInt64 _totalSize;
  AString _comment;
  Byte _uuid[k_uuidSize];

  void AddSpecSection(const UInt32 offset, const UInt32 size, const char * const name);
  HRESULT Open2(ISequentialInStream *stream);
public:
  CHandler(): _allowTail(false) {}
};

static const Byte kArcProps[] =
{
  kpidCpu,
  kpidBit64,
  kpidBigEndian,
  kpidCharacts,
  kpidId,
  kpidComment,
  kpidHeadersSize
};

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidPackSize,
  kpidCharacts,
  kpidOffset,
  kpidVa,
  kpidClusterSize // Align
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

static void Add_Dot_UInt32(AString &s, UInt32 v)
{
  s.Add_Dot();
  s.Add_UInt32(v);
}

static void PrintVersion(AString &s, const char *name, UInt32 ver)
{
  s += name;
  s.Add_Space();
  s.Add_UInt32(ver >> 16);
  Add_Dot_UInt32(s, (ver >> 8) & 0xff);
  if (ver &= 0xff)
    Add_Dot_UInt32(s, ver);
  s.Add_LF();
}

static void PrintFlagsString(AString &res, const char *name,
    const char * const *names, const unsigned num, const UInt32 flags)
{
  const AString s (FlagsToString(names, num, flags));
  if (!s.IsEmpty())
  {
    if (name)
      res.Add_OptSpaced(name);
    res.Add_OptSpaced(s);
  }
}

static void AddTypeString_with_Prefix(AString &dest, const char *prefix,
    const char * const table[], const unsigned num, const UInt32 value)
{
  if (value < num)
  {
    const char *p = table[value];
    if (p)
    {
      dest += p;
      return;
    }
  }
  dest += prefix;
  dest.Add_UInt32(value);
}

void GetCpuName(AString &s, const UInt32 cpuType, const UInt32 subType);
void GetCpuName(AString &s, const UInt32 cpuType, const UInt32 subType)
{
  s.Empty();
  const UInt32 cpu32 = cpuType & CPU_ARCH_MASK_CPU;
  const UInt32 sub = subType & ~(UInt32)CPU_SUBTYPE_MASK_FLAGS;
  int iType = -1;
  bool needType = true, needSubType = true;
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(k_CpuRecords); i++)
  {
    const CCpuRecord &r = k_CpuRecords[i];
    if (r.Type == cpuType)
    {
      if (r.SubType == sub)
        needSubType = false;
      if (r.SubType == sub || needType)
        s += r.Name;
      needType = false;
    }
    if (r.Type == cpu32 && iType < 0)
      iType = (int)i; // it sets for first 32-bit record
  }

  if (needType)
  {
    if (iType >= 0)
      s += k_CpuRecords[(unsigned)iType].Name;
    else
    {
      s += "cpu";
      s.Add_UInt32(cpu32);
    }
    if (cpuType & CPU_ARCH_ABI64)
      s += "_64";
    if (cpuType & CPU_ARCH_ABI64_32)
      s += "_64_32";
  }

  if (needSubType)
  {
    s += "_r";
    s.Add_UInt32(sub);
  }

  if (subType & (CPU_SUBTYPE_MASK_FLAGS & ~CPU_SUBTYPE_LIB64))
  {
    s += "_caps_";
    s.Add_Char(GET_HEX_CHAR_LOWER(subType >> 28));
    s.Add_Char(GET_HEX_CHAR_LOWER((subType >> 24) & 0xf));
  }
  else if ((subType & CPU_SUBTYPE_LIB64) != 0 && (cpuType & CPU_ARCH_ABI64) == 0)
    s += "_lib64";
}

static char *RawBeGuidToString(const Byte *g, char *s Z7_lifetimebound) throw()
{
  for (unsigned i = 12; i < 16 + 12; i++)
  {
    if ((i & 9) == 0)
      *s++ = '-';
    const unsigned v = *g++;
    s[0] = GET_HEX_CHAR_UPPER(v >> 4);
    s[1] = GET_HEX_CHAR_UPPER(v & 0xF);
    s += 2;
  }
  *s = 0;
  return s;
}

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  CPropVariant prop;
  switch (propID)
  {
    case kpidShortComment:
    case kpidCpu:
    {
      AString s;
      GetCpuName(s, _cpuType, _cpuSubType);
      prop = s;
      break;
    }
    case kpidCharacts:
    {
      // TYPE_TO_PROP(g_FileTypes, _type, prop); break;
      AString res (TypeToString(g_FileTypes, Z7_ARRAY_SIZE(g_FileTypes), _type));
      PrintFlagsString(res, NULL, g_ArcFlags, Z7_ARRAY_SIZE(g_ArcFlags), _flags);
      prop = res;
      break;
    }
    case kpidPhySize:  prop = _totalSize; break;
    case kpidHeadersSize:  prop = _headersSize; break;
    case kpidBit64:  if (_mode64) prop = true; break;
    case kpidBigEndian:  if (_be) prop = _be; break;
    case kpidExtension:
    {
      const char *ext = NULL;
      if (_type == kType_OBJECT)
        ext = "o";
      else if (_type == kType_BUNDLE)
        ext = "bundle";
      else if (_type == kType_DYLIB)
        ext = "dylib"; // main shared library usually does not have extension
      if (ext)
        prop = ext;
      break;
    }
    // case kpidIsSelfExe: prop = (_type == kType_EXECUTE); break;
    case kpidComment:
      if (!_comment.IsEmpty())
        prop = _comment;
      break;
    case kpidId:
      if (_uuid_IsDefined)
      {
        char s[48];
        RawBeGuidToString(_uuid, s);
        prop = s;
      }
      break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

static char *AddName(char *dest Z7_lifetimebound, const Byte *name)
{
  for (unsigned i = 0; i < kNameSize; i++)
  {
    Byte c = name[i];
    if (c == 0)
      break;
    if ((Byte)c < 0x20 || (Byte)c > 0x7f || c == '/' || c == '\\')
      c = '_';
    *dest++ = (char)c;
  }
  *dest = 0;
  return dest;
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  CPropVariant prop;
  const CSection &item = _sections[index];
  switch (propID)
  {
    case kpidPath:
      prop = item.Name;
      break;
    case kpidSize:  /* prop = (UInt64)item.VSize; break; */
    case kpidPackSize:  prop = (UInt64)item.GetPackSize(); break;
    case kpidCharacts:
    {
      AString res;
      {
        if (item.IsSection)
        {
          {
            const AString s (TypeToString(g_SectTypes, Z7_ARRAY_SIZE(g_SectTypes), item.Flags & SECT_TYPE_MASK));
            if (!s.IsEmpty())
            {
              res.Add_OptSpaced("sect_type:");
              res.Add_OptSpaced(s);
            }
          }
          {
            const AString s (FlagsToString(g_SectFlags, Z7_ARRAY_SIZE(g_SectFlags), item.Flags & SECT_ATTR_MASK));
            if (!s.IsEmpty())
            {
              res.Add_OptSpaced("sect_flags:");
              res.Add_OptSpaced(s);
            }
          }
        }
        // if (item.SegmentIndex >= 0)
        if (item.Seg_isDefined)
        {
          const CSegment &seg = item.Seg;// _segments[item.SegmentIndex];
          PrintFlagsString(res, "seg_flags:", g_SegmentFlags, Z7_ARRAY_SIZE(g_SegmentFlags), seg.Flags);
          PrintFlagsString(res, "max_prot:", g_SegmentProt, Z7_ARRAY_SIZE(g_SegmentProt), seg.MaxProt);
          PrintFlagsString(res, "init_prot:", g_SegmentProt, Z7_ARRAY_SIZE(g_SegmentProt), seg.InitProt);
        }
      }
      if (!res.IsEmpty())
        prop = res;
      break;
    }
    case kpidOffset:  prop = item.Pa; break;
    case kpidVa:  prop = item.Va; break;
    case kpidClusterSize:  prop = (UInt32)1 << item.Align; break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


#if defined(MY_CPU_BE)
#define INVERT_IF_BE_NATIVE(a)  (!(a))
#else
#define INVERT_IF_BE_NATIVE(a)  (a)
#endif

#define SWAP_BYTES_4_IF_NOT_NATIVE(data, num) \
  { if (INVERT_IF_BE_NATIVE(be)) z7_SwapBytes4(data, num); }


void CHandler::AddSpecSection(const UInt32 addr, const UInt32 size, const char * const name)
{
  CSection &sect = _sections.AddNew_NoConstruct();
  Z7_memset_0_VAR(sect);
  // sect.IsSection = false;
  // sect.Seg_isDefined = false;
  // sect.SegmentIndex = -1;
  sect.Va = addr;
  sect.VSize = size;
  sect.Pa = addr;
  sect.PSize = size;
  // sect.Align = 0;
  // sect.Flags = 0;
  MyStringCopy(sect.Name, name);
}

HRESULT CHandler::Open2(ISequentialInStream *stream)
{
  const unsigned kStartHeaderSize = 7 * 4;
  UInt32 header32[kStartHeaderSize / 4];
  RINOK(ReadStream_FALSE(stream, header32, kStartHeaderSize))
  /*  LE-value:
    0xCEFAEDFE:  BE : 32-bit
    0xCFFAEDFE:  BE : 64-bit
    0xFEEDFACE:  LE : 32-bit
    0xFEEDFACF:  LE : 64-bit
  */
  const bool be = (GetUi32a(header32) >> 4) & 1;
  _be = be;
  SWAP_BYTES_4_IF_NOT_NATIVE(header32, kStartHeaderSize / 4)
  const UInt32 mode64 = header32[0] - 0xFEEDFACE;
  if (mode64 >= 2)
    return S_FALSE;
  _mode64 = (Byte)mode64;

  UInt32 numCommands = header32[4];
  const UInt32 commandsSize = header32[5];
  if (numCommands == 0
      || commandsSize > (1 << 20) // probably macos have such limit
      || numCommands > commandsSize / 8)
    return S_FALSE;
  _cpuType = header32[1];
  _cpuSubType = header32[2];
  _type = header32[3];
  _flags = header32[6];

  const unsigned startHeaderSize = kStartHeaderSize + 4 * (unsigned)mode64;
  _headersSize = startHeaderSize + commandsSize;
  _totalSize = _headersSize;
  CByteArr buffer(_headersSize);
  RINOK(ReadStream_FALSE(stream, buffer + kStartHeaderSize, _headersSize - kStartHeaderSize))

// Probably the sections are in first commands. So we can reduce the number of parsed commands.
// #define Z7_MACHO_REDUCE_COMMANDS // define to reduce number of parsed commands
#ifdef Z7_MACHO_REDUCE_COMMANDS
  bool reduceCommands = false;
  const UInt32 kNumReduceCommands = 16;
  if (numCommands > kNumReduceCommands)
  {
    reduceCommands = true;
    numCommands = kNumReduceCommands;
  }
#endif

  Byte *buf = buffer + startHeaderSize;
  // (buf) is aligned for (mode64 ? 8 : 4)
  UInt32 size = commandsSize; // _headersSize - startHeaderSize;
  do
  {
    if (size < 8)
      return S_FALSE;
    const UInt32 cmd = Get32(buf, be);
    const UInt32 cmdSize = Get32(buf + 4, be);
    if ((cmdSize & (mode64 * 4 + 3)) || cmdSize < 8 || size < cmdSize)
      return S_FALSE;

    if (cmd == LC_SEGMENT_32 || cmd == LC_SEGMENT_64)
    {
      UInt32 offs = (cmd == LC_SEGMENT_64) ? 0x48 : 0x38;
      if (cmdSize < offs)
        return S_FALSE;
      UInt64 vmAddr, vmSize, phAddr, phSize;
      {
        if (cmd == LC_SEGMENT_64)
        {
          if (!mode64)
            return S_FALSE;
          vmAddr = Get64(buf + 0x18, be);
          vmSize = Get64(buf + 0x20, be);
          phAddr = Get64(buf + 0x28, be);
          phSize = Get64(buf + 0x30, be);
        }
        else
        {
          UInt32 * const dest = (UInt32 *)(void *)(buf + 0x18);
          SWAP_BYTES_4_IF_NOT_NATIVE(dest, 4)
          vmAddr = dest[0];
          vmSize = dest[1];
          phAddr = dest[2];
          phSize = dest[3];
        }
        {
          const UInt64 totalSize = phAddr + phSize;
          if (totalSize < phAddr || totalSize >= (UInt64)1 << 62)
            return S_FALSE;
          if (_totalSize < totalSize)
            _totalSize = totalSize;
        }
      }
      // CSegment &seg = _segments.AddNew_NoConstruct();
      // memcpy(seg.Name, buf + 8, kNameSize);
      UInt32 * const seg_words = (UInt32 *)(void *)(buf + offs - 16);
      SWAP_BYTES_4_IF_NOT_NATIVE(seg_words, 4)
      UInt32 numSections = seg_words[2];
      if (numSections > (1 << 8))
        return S_FALSE;

      do
      {
        CSection &sect = _sections.AddNew_NoConstruct();
        Z7_memset_0_VAR(sect);
        // sect.IsSection = false;
        // sect.SegmentIndex = _segments.Size() - 1;
        sect.Seg_isDefined = true;
        sect.Seg.MaxProt = seg_words[0];
        sect.Seg.InitProt = seg_words[1];
        sect.Seg.Flags = seg_words[3];

        // name of segment is empty for object files ".o"
        char *segNameEnd = AddName(sect.Name, buf + 8);
        if (numSections == 0)
        {
          sect.Va = vmAddr;
          sect.VSize = vmSize;
          sect.Pa = phAddr;
          sect.PSize = phSize;
          // sect.Align = 0;
          // sect.Flags = 0;
          break;
        }

        const Byte *p = buf + offs;
        const UInt32 headSize = (cmd == LC_SEGMENT_64) ? 0x50 : 0x44;
        offs += headSize;
        if (offs > cmdSize)
          return S_FALSE;
        sect.IsSection = true;
        UInt32 *dest;
        if (cmd == LC_SEGMENT_64)
        {
          sect.Va    = Get64(p + 0x20, be);
          sect.VSize = Get64(p + 0x28, be);
          dest = (UInt32 *)(void *)(p + 0x30);
        }
        else
        {
          sect.Va    = Get32(p + 0x20, be);
          sect.VSize = Get32(p + 0x24, be);
          dest = (UInt32 *)(void *)(p + 0x28);
        }
        SWAP_BYTES_4_IF_NOT_NATIVE(dest, 5)
        sect.Pa    = dest[0];
        sect.Align = dest[1];
        // sect.reloff = dest[2];
        // sect.nreloc = dest[3];
        sect.Flags = dest[4];
        if ((sect.Flags & SECT_TYPE_MASK) == SECT_ATTR_ZEROFILL)
          sect.PSize = 0;
        else
          sect.PSize = sect.VSize;
        /* note: object file ".o" has empty name in SEGMENT record.
           But there is Segment_Name in SECTION record.
           so we can use Segment_Name from section record.
        */
        // if (p[kNameSize])
        segNameEnd = AddName(sect.Name, p + kNameSize);
        AddName(segNameEnd, p);
        // memcpy(sect.SegName, p + kNameSize, kNameSize);
        // memcpy(sect.Name, p, kNameSize);
        // sect.SegName[4] = '\\';
      }
      while (--numSections);

      if (offs != cmdSize)
        return S_FALSE;
    }
    else if (cmd == LC_SYMTAB)
    {
      /* "__LINKEDIT" segment contains SYMTAB blocks for non-object macho files.
         But object macho files contain SYMTAB blocks outside of single unnamed segment.
         We parse LC_SYMTAB to update (_totalSize).
      */
      if (cmdSize != 6 * 4)
        return S_FALSE;
      UInt32 * const dest = (UInt32 *)(void *)(buf + 8);
      SWAP_BYTES_4_IF_NOT_NATIVE(dest, 4)
      const UInt32 symoff = dest[0];  // symbol table offset
      const UInt32 nsyms = dest[1];   // number of symbol table entries
      const UInt32 stroff = dest[2];  // string table offset
      const UInt32 strsize = dest[3]; // string table size in bytes
      const UInt32 recordSize = 12 + 4 * (unsigned)mode64;
      const UInt32 tableSize = recordSize * nsyms;
      if (tableSize / recordSize != nsyms)
        return S_FALSE;
      {
        const UInt32 end1 = symoff + tableSize;
        if (end1 < symoff)
          return S_FALSE;
        // (end1 != stroff) is possible
        const UInt32 end2 = stroff + strsize;
        if (end2 < stroff)
          return S_FALSE;
        const UInt32 end = MyMax(end1, end2);
        if (_totalSize < end)
        {
          _totalSize = end;
          // usually we go here for object ".o" files only.
          // we show both SYMTAB blocks:
          AddSpecSection(symoff, tableSize, "_SYMTAB_TABLE");
          AddSpecSection(stroff, strsize, "_SYMTAB_STRINGS");
        }
      }
    }
    else if (cmd == LC_BUILD_VERSION)
    {
      const unsigned fixedSize = 4 * 6;
      if (cmdSize < fixedSize)
        return S_FALSE;
#define MAX_COMMENT_LEN (1 << 12)
      AString &s = _comment;
      if (s.Len() < MAX_COMMENT_LEN)
      {
        UInt32 * const dest = (UInt32 *)(void *)(buf + 8);
        SWAP_BYTES_4_IF_NOT_NATIVE(dest, cmdSize / 4 - 2)
        s += "BUILD_VERSION:\n";
        const UInt32 platform = dest[0];
        AddTypeString_with_Prefix(s, "platform_",
              g_Platform_Names, Z7_ARRAY_SIZE(g_Platform_Names), platform);
        PrintVersion(s, "", dest[1]); // minos
        PrintVersion(s, "sdk", dest[2]);
        UInt32 ntools = dest[3];    // number of tool entries following this
        if ((cmdSize - fixedSize) / 8 != ntools)
          return S_FALSE;
        const unsigned ntools_max = 32; // we limit max number of lines in Comments string
        if (ntools > ntools_max)
            ntools = ntools_max;
        {
          const UInt32 *data = (const UInt32 *)(const void *)(buf + fixedSize);
          for (unsigned i = 0; i < ntools; i++, data += 2)
          {
            AddTypeString_with_Prefix(s, "tool_",
                g_Tool_Names, Z7_ARRAY_SIZE(g_Tool_Names), data[0]);
            PrintVersion(s, "", data[1]);
          }
        }
      }
    }
    else if (cmd == LC_SOURCE_VERSION)
    {
      if (cmdSize != 4 * 4)
        return S_FALSE;
      AString &s = _comment;
      if (s.Len() < MAX_COMMENT_LEN)
      {
        s += "SOURCE_VERSION: ";
        const UInt64 v64 = Get64(buf + 8, be);
        s.Add_UInt32((UInt32)(v64 >> 40));
        Add_Dot_UInt32(s, (UInt32)(v64 >> 30) & 0x3ff);
        UInt32 v = (UInt32)v64;
        if (v &= ((1 << 30) - 1)) { Add_Dot_UInt32(s, v >> 20);
        if (v &= ((1 << 20) - 1)) { Add_Dot_UInt32(s, v >> 10);
        if (v &= ((1 << 10) - 1)) { Add_Dot_UInt32(s, v); }}}
        s.Add_LF();
      }
    }
    else if (cmd == LC_UUID)
    {
      if (cmdSize != 8 + k_uuidSize)
        return S_FALSE;
      memcpy(_uuid, buf + 8, k_uuidSize);
      _uuid_IsDefined = true;
    }
    else
    {
      const char *os = NULL;
           if (cmd == LC_VERSION_MIN_MACOSX)   os = "MACOS";
      else if (cmd == LC_VERSION_MIN_IPHONEOS) os = "IOS";
      else if (cmd == LC_VERSION_MIN_TVOS)     os = "TVOS";
      else if (cmd == LC_VERSION_MIN_WATCHOS)  os = "WATCHOS";
      if (os)
      {
        if (cmdSize != 4 * 4)
          return S_FALSE;
        AString &s = _comment;
        if (s.Len() < MAX_COMMENT_LEN)
        {
          s += "VERSION_MIN:\n";
          UInt32 * const dest = (UInt32 *)(void *)(buf + 8);
          SWAP_BYTES_4_IF_NOT_NATIVE(dest, 2)
          PrintVersion(s, os, dest[0]);
          PrintVersion(s, "sdk", dest[1]);
        }
      }
    }
    buf += cmdSize;
    size -= cmdSize;
  }
  while (--numCommands);

#ifndef Z7_MACHO_REDUCE_COMMANDS
  if (size)
    return S_FALSE;
#endif
  return S_OK;
}


Z7_COM7F_IMF(CHandler::Open(IInStream *inStream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback * /* openArchiveCallback */))
{
  COM_TRY_BEGIN
  Close();
  RINOK(Open2(inStream))
  if (!_allowTail)
  {
    UInt64 fileSize;
    RINOK(InStream_GetSize_SeekToEnd(inStream, fileSize))
    if (fileSize > _totalSize)
      return S_FALSE;
  }
  _inStream = inStream;
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _uuid_IsDefined = false;
  _comment.Empty();
  _totalSize = 0;
  _inStream.Release();
  _sections.Clear();
  // _segments.Clear();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _sections.Size();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Extract(const UInt32 * const indices, UInt32 numItems,
    const Int32 testMode, IArchiveExtractCallback * const extractCallback))
{
  COM_TRY_BEGIN
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _sections.Size();
  if (numItems == 0)
    return S_OK;
  UInt64 totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
    totalSize += _sections[allFilesMode ? i : indices[i]].GetPackSize();
  RINOK(extractCallback->SetTotal(totalSize))

  CMyComPtr2_Create<ICompressCoder, NCompress::CCopyCoder> copyCoder;
  CMyComPtr2_Create<ICompressProgressInfo, CLocalProgress> lps;
  lps->Init(extractCallback, false);
  CMyComPtr2_Create<ISequentialInStream, CLimitedSequentialInStream> inStream;
  inStream->SetStream(_inStream);
  totalSize = 0;

  for (i = 0;; i++)
  {
    CLocalProgress *lps2 = lps.ClsPtr();
    lps2->InSize = lps2->OutSize = totalSize;
    RINOK(lps2->SetCur())
    if (i >= numItems)
      break;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    const UInt32 index = allFilesMode ? i : indices[i];
    const CSection &item = _sections[index];
    const UInt64 currentItemSize = item.GetPackSize();
    totalSize += currentItemSize;
    {
      CMyComPtr<ISequentialOutStream> outStream;
      RINOK(extractCallback->GetStream(index, &outStream, askMode))
      if (!testMode && !outStream)
        continue;
      RINOK(extractCallback->PrepareOperation(askMode))
      RINOK(InStream_SeekSet(_inStream, item.Pa))
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

static const Byte k_Signature[] = {
  4, 0xCE, 0xFA, 0xED, 0xFE,
  4, 0xCF, 0xFA, 0xED, 0xFE,
  4, 0xFE, 0xED, 0xFA, 0xCE,
  4, 0xFE, 0xED, 0xFA, 0xCF };

REGISTER_ARC_I(
  "MachO", "macho", NULL, 0xDF,
  k_Signature,
  0,
  NArcInfoFlags::kMultiSignature |
  NArcInfoFlags::kPreArc,
  NULL)

}}

// MachoHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/MyBuffer.h"
#include "../../Common/StringConvert.h"
#include "../../Common/IntToString.h"

#include "../../Windows/PropVariantUtils.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"

static UInt32 Get32(const Byte *p, bool be) { if (be) return GetBe32(p); return GetUi32(p); }
static UInt64 Get64(const Byte *p, bool be) { if (be) return GetBe64(p); return GetUi64(p); }

using namespace NWindows;
using namespace NCOM;

namespace NArchive {
namespace NMacho {

#define CPU_ARCH_ABI64 (1 << 24)
#define CPU_TYPE_386    7
#define CPU_TYPE_ARM   12
#define CPU_TYPE_SPARC 14
#define CPU_TYPE_PPC   18

#define CPU_SUBTYPE_I386_ALL 3

// #define CPU_TYPE_PPC64 (CPU_ARCH_ABI64 | CPU_TYPE_PPC)
#define CPU_TYPE_AMD64 (CPU_ARCH_ABI64 | CPU_TYPE_386)
#define CPU_TYPE_ARM64 (CPU_ARCH_ABI64 | CPU_TYPE_ARM)

#define CPU_SUBTYPE_LIB64 ((UInt32)1 << 31)

#define CPU_SUBTYPE_POWERPC_970 100

static const char * const k_PowerPc_SubTypes[] =
{
  NULL
  , "601"
  , "602"
  , "603"
  , "603e"
  , "603ev"
  , "604"
  , "604e"
  , "620"
  , "750"
  , "7400"
  , "7450"
};

static const CUInt32PCharPair g_CpuPairs[] =
{
  { CPU_TYPE_AMD64, "x64" },
  { CPU_TYPE_ARM64, "ARM64" },
  { CPU_TYPE_386, "x86" },
  { CPU_TYPE_ARM, "ARM" },
  { CPU_TYPE_SPARC, "SPARC" },
  { CPU_TYPE_PPC, "PowerPC" }
};


#define CMD_SEGMENT_32 1
#define CMD_SEGMENT_64 0x19

#define SECT_TYPE_MASK 0x000000FF
#define SECT_ATTR_MASK 0xFFFFFF00

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

static const unsigned kNameSize = 16;

struct CSegment
{
  char Name[kNameSize];
  UInt32 MaxProt;
  UInt32 InitProt;
  UInt32 Flags;
};

struct CSection
{
  char Name[kNameSize];
  // char SegName[kNameSize];
  UInt64 Va;
  UInt64 Pa;
  UInt64 VSize;
  UInt64 PSize;

  UInt32 Align;
  UInt32 Flags;
  unsigned SegmentIndex;
  bool IsDummy;

  CSection(): IsDummy(false) {}
  // UInt64 GetPackSize() const { return Flags == SECT_ATTR_ZEROFILL ? 0 : Size; }
  UInt64 GetPackSize() const { return PSize; }
};


Z7_CLASS_IMP_CHandler_IInArchive_1(
  IArchiveAllowTail
)
  CMyComPtr<IInStream> _inStream;
  CObjectVector<CSegment> _segments;
  CObjectVector<CSection> _sections;
  bool _allowTail;
  bool _mode64;
  bool _be;
  UInt32 _cpuType;
  UInt32 _cpuSubType;
  UInt32 _type;
  UInt32 _flags;
  UInt32 _headersSize;
  UInt64 _totalSize;

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
      const UInt32 cpu = _cpuType & ~(UInt32)CPU_ARCH_ABI64;
      UInt32 flag64 = _cpuType & (UInt32)CPU_ARCH_ABI64;
      {
        s.Add_UInt32(cpu);
        for (unsigned i = 0; i < Z7_ARRAY_SIZE(g_CpuPairs); i++)
        {
          const CUInt32PCharPair &pair = g_CpuPairs[i];
          if (pair.Value == cpu || pair.Value == _cpuType)
          {
            if (pair.Value == _cpuType)
              flag64 = 0;
            s = pair.Name;
            break;
          }
        }
       
        if (flag64 != 0)
          s.Add_OptSpaced("64-bit");
        else if ((_cpuSubType & CPU_SUBTYPE_LIB64) && _cpuType != CPU_TYPE_AMD64)
          s.Add_OptSpaced("64-bit-lib");
      }
      const UInt32 t = _cpuSubType & ~(UInt32)CPU_SUBTYPE_LIB64;
      if (t != 0 && (t != CPU_SUBTYPE_I386_ALL || cpu != CPU_TYPE_386))
      {
        const char *n = NULL;
        if (cpu == CPU_TYPE_PPC)
        {
          if (t == CPU_SUBTYPE_POWERPC_970)
            n = "970";
          else if (t < Z7_ARRAY_SIZE(k_PowerPc_SubTypes))
            n = k_PowerPc_SubTypes[t];
        }
        s.Add_Space();
        if (n)
          s += n;
        else
          s.Add_UInt32(t);
      }
      prop = s;
      break;
    }
    case kpidCharacts:
    {
      // TYPE_TO_PROP(g_FileTypes, _type, prop); break;
      AString res (TypeToString(g_FileTypes, Z7_ARRAY_SIZE(g_FileTypes), _type));
      const AString s (FlagsToString(g_ArcFlags, Z7_ARRAY_SIZE(g_ArcFlags), _flags));
      if (!s.IsEmpty())
      {
        res.Add_Space();
        res += s;
      }
      prop = res;
      break;
    }
    case kpidPhySize:  prop = _totalSize; break;
    case kpidHeadersSize:  prop = _headersSize; break;
    case kpidBit64:  if (_mode64) prop = _mode64; break;
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
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

static void AddName(AString &s, const char *name)
{
  char temp[kNameSize + 1];
  memcpy(temp, name, kNameSize);
  temp[kNameSize] = 0;
  s += temp;
}


Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  CPropVariant prop;
  const CSection &item = _sections[index];
  switch (propID)
  {
    case kpidPath:
    {
      AString s;
      AddName(s, _segments[item.SegmentIndex].Name);
      if (!item.IsDummy)
      {
        // CSection::SegName and CSegment::Name are same in real cases.
        // AddName(s, item.SegName);
        AddName(s, item.Name);
      }
      prop = MultiByteToUnicodeString(s);
      break;
    }
    case kpidSize:  /* prop = (UInt64)item.VSize; break; */
    case kpidPackSize:  prop = (UInt64)item.GetPackSize(); break;
    case kpidCharacts:
    {
      AString res;
      {
        if (!item.IsDummy)
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
        const CSegment &seg = _segments[item.SegmentIndex];
        {
          const AString s (FlagsToString(g_SegmentFlags, Z7_ARRAY_SIZE(g_SegmentFlags), seg.Flags));
          if (!s.IsEmpty())
          {
            res.Add_OptSpaced("seg_flags:");
            res.Add_OptSpaced(s);
          }
        }
        {
          const AString s (FlagsToString(g_SegmentProt, Z7_ARRAY_SIZE(g_SegmentProt), seg.MaxProt));
          if (!s.IsEmpty())
          {
            res.Add_OptSpaced("max_prot:");
            res.Add_OptSpaced(s);
          }
        }
        {
          const AString s (FlagsToString(g_SegmentProt, Z7_ARRAY_SIZE(g_SegmentProt), seg.InitProt));
          if (!s.IsEmpty())
          {
            res.Add_OptSpaced("init_prot:");
            res.Add_OptSpaced(s);
          }
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


HRESULT CHandler::Open2(ISequentialInStream *stream)
{
  const UInt32 kStartHeaderSize = 7 * 4;

  Byte header[kStartHeaderSize];
  RINOK(ReadStream_FALSE(stream, header, kStartHeaderSize))
  bool be, mode64;
  switch (GetUi32(header))
  {
    case 0xCEFAEDFE:  be = true; mode64 = false; break;
    case 0xCFFAEDFE:  be = true; mode64 = true; break;
    case 0xFEEDFACE:  be = false; mode64 = false; break;
    case 0xFEEDFACF:  be = false; mode64 = true; break;
    default: return S_FALSE;
  }
  
  _mode64 = mode64;
  _be = be;

  const UInt32 numCommands = Get32(header + 0x10, be);
  const UInt32 commandsSize = Get32(header + 0x14, be);

  if (numCommands == 0)
    return S_FALSE;

  if (commandsSize > (1 << 24) ||
      numCommands > (1 << 21) ||
      numCommands * 8 > commandsSize)
    return S_FALSE;

  _cpuType = Get32(header + 4, be);
  _cpuSubType = Get32(header + 8, be);
  _type = Get32(header + 0xC, be);
  _flags = Get32(header + 0x18, be);

  /*
  // Probably the sections are in first commands. So we can reduce the number of commands.
  bool reduceCommands = false;
  const UInt32 kNumReduceCommands = 16;
  if (numCommands > kNumReduceCommands)
  {
    reduceCommands = true;
    numCommands = kNumReduceCommands;
  }
  */

  UInt32 startHeaderSize = kStartHeaderSize;
  if (mode64)
    startHeaderSize += 4;
  _headersSize = startHeaderSize + commandsSize;
  _totalSize = _headersSize;
  CByteArr buffer(_headersSize);
  RINOK(ReadStream_FALSE(stream, buffer + kStartHeaderSize, _headersSize - kStartHeaderSize))
  const Byte *buf = buffer + startHeaderSize;
  size_t size = _headersSize - startHeaderSize;
  for (UInt32 cmdIndex = 0; cmdIndex < numCommands; cmdIndex++)
  {
    if (size < 8)
      return S_FALSE;
    UInt32 cmd = Get32(buf, be);
    UInt32 cmdSize = Get32(buf + 4, be);
    if (cmdSize < 8)
      return S_FALSE;
    if (size < cmdSize)
      return S_FALSE;
    if (cmd == CMD_SEGMENT_32 || cmd == CMD_SEGMENT_64)
    {
      UInt32 offs = (cmd == CMD_SEGMENT_64) ? 0x48 : 0x38;
      if (cmdSize < offs)
        break;

      UInt64 vmAddr, vmSize, phAddr, phSize;

      {
        if (cmd == CMD_SEGMENT_64)
        {
          vmAddr = Get64(buf + 0x18, be);
          vmSize = Get64(buf + 0x20, be);
          phAddr = Get64(buf + 0x28, be);
          phSize = Get64(buf + 0x30, be);
        }
        else
        {
          vmAddr = Get32(buf + 0x18, be);
          vmSize = Get32(buf + 0x1C, be);
          phAddr = Get32(buf + 0x20, be);
          phSize = Get32(buf + 0x24, be);
        }
        {
          UInt64 totalSize = phAddr + phSize;
          if (totalSize < phAddr)
            return S_FALSE;
          if (_totalSize < totalSize)
            _totalSize = totalSize;
        }
      }
      
      CSegment seg;
      seg.MaxProt = Get32(buf + offs - 16, be);
      seg.InitProt = Get32(buf + offs - 12, be);
      seg.Flags = Get32(buf + offs - 4, be);
      memcpy(seg.Name, buf + 8, kNameSize);
      _segments.Add(seg);

      UInt32 numSections = Get32(buf + offs - 8, be);
      if (numSections > (1 << 8))
        return S_FALSE;

      if (numSections == 0)
      {
        CSection &sect = _sections.AddNew();
        sect.IsDummy = true;
        sect.SegmentIndex = _segments.Size() - 1;
        sect.Va = vmAddr;
        sect.PSize = phSize;
        sect.VSize = vmSize;
        sect.Pa = phAddr;
        sect.Align = 0;
        sect.Flags = 0;
      }
      else do
      {
        const UInt32 headSize = (cmd == CMD_SEGMENT_64) ? 0x50 : 0x44;
        const Byte *p = buf + offs;
        if (cmdSize - offs < headSize)
          break;
        CSection &sect = _sections.AddNew();
        unsigned f32Offset;
        if (cmd == CMD_SEGMENT_64)
        {
          sect.Va    = Get64(p + 0x20, be);
          sect.VSize = Get64(p + 0x28, be);
          f32Offset = 0x30;
        }
        else
        {
          sect.Va    = Get32(p + 0x20, be);
          sect.VSize = Get32(p + 0x24, be);
          f32Offset = 0x28;
        }
        sect.Pa    = Get32(p + f32Offset, be);
        sect.Align = Get32(p + f32Offset + 4, be);
        // sect.reloff = Get32(p + f32Offset + 8, be);
        // sect.nreloc = Get32(p + f32Offset + 12, be);
        sect.Flags = Get32(p + f32Offset + 16, be);
        if ((sect.Flags & SECT_TYPE_MASK) == SECT_ATTR_ZEROFILL)
          sect.PSize = 0;
        else
          sect.PSize = sect.VSize;
        memcpy(sect.Name, p, kNameSize);
        // memcpy(sect.SegName, p + kNameSize, kNameSize);
        sect.SegmentIndex = _segments.Size() - 1;
        offs += headSize;
      }
      while (--numSections);

      if (offs != cmdSize)
        return S_FALSE;
    }
    buf += cmdSize;
    size -= cmdSize;
  }
  // return (reduceCommands || (size == 0)) ? S_OK : S_FALSE;
  if (size != 0)
    return S_FALSE;

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
  _totalSize = 0;
  _inStream.Release();
  _sections.Clear();
  _segments.Clear();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _sections.Size();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
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
  streamSpec->SetStream(_inStream);

  for (i = 0; i < numItems; i++, currentTotalSize += currentItemSize)
  {
    lps->InSize = lps->OutSize = currentTotalSize;
    RINOK(lps->SetCur())
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    const UInt32 index = allFilesMode ? i : indices[i];
    const CSection &item = _sections[index];
    currentItemSize = item.GetPackSize();

    CMyComPtr<ISequentialOutStream> outStream;
    RINOK(extractCallback->GetStream(index, &outStream, askMode))
    if (!testMode && !outStream)
      continue;
    
    RINOK(extractCallback->PrepareOperation(askMode))
    RINOK(InStream_SeekSet(_inStream, item.Pa))
    streamSpec->Init(currentItemSize);
    RINOK(copyCoder->Code(inStream, outStream, NULL, NULL, progress))
    outStream.Release();
    RINOK(extractCallback->SetOperationResult(copyCoderSpec->TotalSize == currentItemSize ?
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

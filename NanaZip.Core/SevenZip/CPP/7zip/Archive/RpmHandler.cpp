// RpmHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/MyBuffer.h"
#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/StringConvert.h"
#include "../../Common/UTFConvert.h"

#include "../../Windows/PropVariant.h"
#include "../../Windows/PropVariantUtils.h"
#include "../../Windows/TimeUtils.h"

#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "HandlerCont.h"

// #define Z7_RPM_SHOW_METADATA

using namespace NWindows;

#define Get16(p) GetBe16(p)
#define Get32(p) GetBe32(p)

namespace NArchive {
namespace NRpm {

static const unsigned kNameSize = 66;
static const unsigned kLeadSize = kNameSize + 30;
static const unsigned k_HeaderSig_Size = 16;
static const unsigned k_Entry_Size = 16;

#define RPMSIG_NONE         0  // Old signature
#define RPMSIG_PGP262_1024  1  // Old signature
#define RPMSIG_HEADERSIG    5  // New signature

enum
{
  kRpmType_Bin = 0,
  kRpmType_Src = 1
};

// There are two sets of TAGs: signature tags and header tags

// ----- Signature TAGs -----

#define RPMSIGTAG_SIZE 1000 // Header + Payload size (32bit)

// ----- Header TAGs -----

#define RPMTAG_NAME       1000
#define RPMTAG_VERSION    1001
#define RPMTAG_RELEASE    1002
#define RPMTAG_BUILDTIME  1006
#define RPMTAG_OS         1021  // string (old version used int?)
#define RPMTAG_ARCH       1022  // string (old version used int?)
#define RPMTAG_PAYLOADFORMAT      1124
#define RPMTAG_PAYLOADCOMPRESSOR  1125
// #define RPMTAG_PAYLOADFLAGS       1126

enum
{
  k_EntryType_NULL,
  k_EntryType_CHAR,
  k_EntryType_INT8,
  k_EntryType_INT16,
  k_EntryType_INT32,
  k_EntryType_INT64,
  k_EntryType_STRING,
  k_EntryType_BIN,
  k_EntryType_STRING_ARRAY,
  k_EntryType_I18NSTRING
};

static const char * const k_CPUs[] =
{
    "noarch"
  , "i386"
  , "alpha"
  , "sparc"
  , "mips"
  , "ppc"
  , "m68k"
  , "sgi"
  , "rs6000"
  , "ia64"
  , "sparc64"  // 10 ???
  , "mipsel"
  , "arm"
  , "m68kmint"
  , "s390"
  , "s390x"
  , "ppc64"
  , "sh"
  , "xtensa"
  , "aarch64"  // 19
};

static const char * const k_OS[] =
{
    "0"
  , "Linux"
  , "Irix"
  , "solaris"
  , "SunOS"
  , "AmigaOS" // AIX
  , "HP-UX"
  , "osf"
  , "FreeBSD"
  , "SCO_SV"
  , "Irix64"
  , "NextStep"
  , "bsdi"
  , "machten"
  , "cygwin32-NT"
  , "cygwin32-95"
  , "MP_RAS"
  , "MiNT"
  , "OS/390"
  , "VM/ESA"
  , "Linux/390"  // "Linux/ESA"
  , "Darwin" // "MacOSX" 21
};

struct CLead
{
  unsigned char Major;
  unsigned char Minor;
  UInt16 Type;
  UInt16 Cpu;
  UInt16 Os;
  UInt16 SignatureType;
  char Name[kNameSize];
  // char Reserved[16];

  void Parse(const Byte *p)
  {
    Major = p[4];
    Minor = p[5];
    Type = Get16(p + 6);
    Cpu= Get16(p + 8);
    memcpy(Name, p + 10, kNameSize);
    p += 10 + kNameSize;
    Os = Get16(p);
    SignatureType = Get16(p + 2);
  }

  bool IsSupported() const { return Major >= 3 && Type <= 1; }
};

struct CEntry
{
  UInt32 Tag;
  UInt32 Type;
  UInt32 Offset;
  UInt32 Count;
  
  void Parse(const Byte *p)
  {
    Tag = Get32(p + 0);
    Type = Get32(p + 4);
    Offset = Get32(p + 8);
    Count = Get32(p + 12);
  }
};


#ifdef Z7_RPM_SHOW_METADATA
struct CMetaFile
{
  UInt32 Tag;
  UInt32 Offset;
  UInt32 Size;
};
#endif

Z7_class_CHandler_final: public CHandlerCont
{
  Z7_IFACE_COM7_IMP(IInArchive_Cont)

  UInt64 _headersSize; // is equal to start offset of payload data
  UInt64 _payloadSize;
  UInt64 _size;
    // _size = _payloadSize, if (_payloadSize_Defined)
    // _size = (fileSize - _headersSize), if (!_payloadSize_Defined)
  UInt64 _phySize; // _headersSize + _payloadSize, if (_phySize_Defined)
  UInt32 _headerPlusPayload_Size;
  UInt32 _buildTime;
  
  bool _payloadSize_Defined;
  bool _phySize_Defined;
  bool _headerPlusPayload_Size_Defined;
  bool _time_Defined;

  Byte _payloadSig[6]; // start data of payload

  AString _name;    // p7zip
  AString _version; // 9.20.1
  AString _release; // 8.1.1
  AString _arch;    // x86_64
  AString _os;      // linux
  
  AString _format;      // cpio
  AString _compressor;  // xz, gzip, bzip2

  CLead _lead;

  #ifdef Z7_RPM_SHOW_METADATA
  AString _metadata;
  CRecordVector<CMetaFile> _metaFiles;
  #endif

  void SetTime(NCOM::CPropVariant &prop) const
  {
    if (_time_Defined && _buildTime != 0)
      PropVariant_SetFrom_UnixTime(prop, _buildTime);
  }

  void SetStringProp(const AString &s, NCOM::CPropVariant &prop) const
  {
    UString us;
    if (!ConvertUTF8ToUnicode(s, us))
      us = GetUnicodeString(s);
    if (!us.IsEmpty())
      prop = us;
  }

  void AddCPU(AString &s) const;
  AString GetBaseName() const;
  void AddSubFileExtension(AString &res) const;

  HRESULT ReadHeader(ISequentialInStream *stream, bool isMainHeader);
  HRESULT Open2(ISequentialInStream *stream);

  virtual int GetItem_ExtractInfo(UInt32 /* index */, UInt64 &pos, UInt64 &size) const Z7_override
  {
    pos = _headersSize;
    size = _size;
    return NExtract::NOperationResult::kOK;
  }
};

static const Byte kArcProps[] =
{
  kpidHeadersSize,
  kpidCpu,
  kpidHostOS,
  kpidCTime
  #ifdef Z7_RPM_SHOW_METADATA
  , kpidComment
  #endif
};

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidCTime
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

void CHandler::AddCPU(AString &s) const
{
  if (!_arch.IsEmpty())
    s += _arch;
  else
  {
    if (_lead.Type == kRpmType_Bin)
    {
      if (_lead.Cpu < Z7_ARRAY_SIZE(k_CPUs))
        s += k_CPUs[_lead.Cpu];
      else
        s.Add_UInt32(_lead.Cpu);
    }
  }
}

AString CHandler::GetBaseName() const
{
  AString s;
  if (!_name.IsEmpty())
  {
    s = _name;
    if (!_version.IsEmpty())
    {
      s.Add_Minus();
      s += _version;
    }
    if (!_release.IsEmpty())
    {
      s.Add_Minus();
      s += _release;
    }
  }
  else
    s.SetFrom_CalcLen(_lead.Name, kNameSize);

  s.Add_Dot();
  if (_lead.Type == kRpmType_Src)
    s += "src";
  else
    AddCPU(s);
  return s;
}

void CHandler::AddSubFileExtension(AString &res) const
{
  if (!_format.IsEmpty())
    res += _format;
  else
    res += "cpio";
  res.Add_Dot();
  
  const char *s;
  
  if (!_compressor.IsEmpty())
  {
    s = _compressor;
    if (_compressor == "bzip2")
      s = "bz2";
    else if (_compressor == "gzip")
      s = "gz";
  }
  else
  {
    const Byte *p = _payloadSig;
    if (p[0] == 0x1F && p[1] == 0x8B)
      s = "gz";
    else if (p[0] == 0xFD && p[1] == '7' && p[2] == 'z' && p[3] == 'X' && p[4] == 'Z' && p[5] == 0)
      s = "xz";
    else if (p[0] == 'B' && p[1] == 'Z' && p[2] == 'h' && p[3] >= '1' && p[3] <= '9')
      s = "bz2";
    else
      s = "lzma";
  }
  
  res += s;
}

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidMainSubfile: prop = (UInt32)0; break;
    
    case kpidHeadersSize: prop = _headersSize; break;
    case kpidPhySize: if (_phySize_Defined) prop = _phySize; break;
    
    case kpidMTime:
    case kpidCTime:
      SetTime(prop);
      break;

    case kpidCpu:
      {
        AString s;
        AddCPU(s);
        /*
        if (_lead.Type == kRpmType_Src)
          s = "src";
        */
        SetStringProp(s, prop);
        break;
      }

    case kpidHostOS:
      {
        if (!_os.IsEmpty())
          SetStringProp(_os, prop);
        else
        {
          TYPE_TO_PROP(k_OS, _lead.Os, prop);
        }
        break;
      }

    #ifdef Z7_RPM_SHOW_METADATA
    // case kpidComment: SetStringProp(_metadata, prop); break;
    #endif

    case kpidName:
    {
      SetStringProp(GetBaseName() + ".rpm", prop);
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  NWindows::NCOM::CPropVariant prop;
  if (index == 0)
  switch (propID)
  {
    case kpidSize:
    case kpidPackSize:
      prop = _size;
      break;

    case kpidMTime:
    case kpidCTime:
      SetTime(prop);
      break;

    case kpidPath:
    {
      AString s (GetBaseName());
      s.Add_Dot();
      AddSubFileExtension(s);
      SetStringProp(s, prop);
      break;
    }

    /*
    case kpidExtension:
    {
      prop = GetSubFileExtension();
      break;
    }
    */
  }
  #ifdef Z7_RPM_SHOW_METADATA
  else
  {
    index--;
    if (index > _metaFiles.Size())
      return E_INVALIDARG;
    const CMetaFile &meta = _metaFiles[index];
    switch (propID)
    {
      case kpidSize:
      case kpidPackSize:
        prop = meta.Size;
        break;
      
      case kpidMTime:
      case kpidCTime:
        SetTime(prop);
        break;
      
      case kpidPath:
      {
        AString s ("[META]");
        s.Add_PathSepar();
        s.Add_UInt32(meta.Tag);
        prop = s;
        break;
      }
    }
  }
  #endif

  prop.Detach(value);
  return S_OK;
}

#ifdef Z7_RPM_SHOW_METADATA
static inline char GetHex(unsigned value)
{
  return (char)((value < 10) ? ('0' + value) : ('A' + (value - 10)));
}
#endif

HRESULT CHandler::ReadHeader(ISequentialInStream *stream, bool isMainHeader)
{
  UInt32 numEntries;
  UInt32 dataLen;
  {
    char buf[k_HeaderSig_Size];
    RINOK(ReadStream_FALSE(stream, buf, k_HeaderSig_Size))
    if (Get32(buf) != 0x8EADE801) // buf[3] = 0x01 - is version
      return S_FALSE;
    // reserved = Get32(buf + 4);
    numEntries = Get32(buf + 8);
    dataLen = Get32(buf + 12);
    if (numEntries >= 1 << 24)
      return S_FALSE;
  }
  size_t indexSize = (size_t)numEntries * k_Entry_Size;
  size_t headerSize = indexSize + dataLen;
  if (headerSize < dataLen)
    return S_FALSE;
  CByteBuffer buffer(headerSize);
  RINOK(ReadStream_FALSE(stream, buffer, headerSize))
  
  for (UInt32 i = 0; i < numEntries; i++)
  {
    CEntry entry;

    entry.Parse(buffer + (size_t)i * k_Entry_Size);
    if (entry.Offset > dataLen)
      return S_FALSE;

    const Byte *p = buffer + indexSize + entry.Offset;
    size_t rem = dataLen - entry.Offset;
    
    if (!isMainHeader)
    {
      if (entry.Tag == RPMSIGTAG_SIZE &&
          entry.Type == k_EntryType_INT32)
      {
        if (rem < 4 || entry.Count != 1)
          return S_FALSE;
        _headerPlusPayload_Size = Get32(p);
        _headerPlusPayload_Size_Defined = true;
      }
    }
    else
    {
      #ifdef Z7_RPM_SHOW_METADATA
      {
        _metadata.Add_UInt32(entry.Tag);
        _metadata += ": ";
      }
      #endif

      if (entry.Type == k_EntryType_STRING)
      {
        if (entry.Count != 1)
          return S_FALSE;
        size_t j;
        for (j = 0; j < rem && p[j] != 0; j++);
        if (j == rem)
          return S_FALSE;
        AString s((const char *)p);
        switch (entry.Tag)
        {
          case RPMTAG_NAME: _name = s; break;
          case RPMTAG_VERSION: _version = s; break;
          case RPMTAG_RELEASE: _release = s; break;
          case RPMTAG_ARCH: _arch = s; break;
          case RPMTAG_OS: _os = s; break;
          case RPMTAG_PAYLOADFORMAT: _format = s; break;
          case RPMTAG_PAYLOADCOMPRESSOR: _compressor = s; break;
        }

        #ifdef Z7_RPM_SHOW_METADATA
        _metadata += s;
        #endif
      }
      else if (entry.Type == k_EntryType_INT32)
      {
        if (rem / 4 < entry.Count)
          return S_FALSE;
        if (entry.Tag == RPMTAG_BUILDTIME)
        {
          if (entry.Count != 1)
            return S_FALSE;
          _buildTime = Get32(p);
          _time_Defined = true;
        }
        
        #ifdef Z7_RPM_SHOW_METADATA
        for (UInt32 t = 0; t < entry.Count; t++)
        {
          if (t != 0)
            _metadata.Add_Space();
          _metadata.Add_UInt32(Get32(p + t * 4));
        }
        #endif
      }

      #ifdef Z7_RPM_SHOW_METADATA

      else if (
          entry.Type == k_EntryType_STRING_ARRAY ||
          entry.Type == k_EntryType_I18NSTRING)
      {
        const Byte *p2 = p;
        size_t rem2 = rem;
        for (UInt32 t = 0; t < entry.Count; t++)
        {
          if (rem2 == 0)
            return S_FALSE;
          if (t != 0)
            _metadata += '\n';
          size_t j;
          for (j = 0; j < rem2 && p2[j] != 0; j++);
          if (j == rem2)
            return S_FALSE;
          _metadata += (const char *)p2;
          j++;
          p2 += j;
          rem2 -= j;
        }
      }
      else if (entry.Type == k_EntryType_INT16)
      {
        if (rem / 2 < entry.Count)
          return S_FALSE;
        for (UInt32 t = 0; t < entry.Count; t++)
        {
          if (t != 0)
            _metadata.Add_Space();
          _metadata.Add_UInt32(Get16(p + t * 2));
        }
      }
      else if (entry.Type == k_EntryType_BIN)
      {
        if (rem < entry.Count)
          return S_FALSE;
        for (UInt32 t = 0; t < entry.Count; t++)
        {
          const unsigned b = p[t];
          _metadata += GetHex((b >> 4) & 0xF);
          _metadata += GetHex(b & 0xF);
        }
      }
      else
      {
        // p = p;
      }

      _metadata += '\n';
      #endif
    }
    
    #ifdef Z7_RPM_SHOW_METADATA
    CMetaFile meta;
    meta.Offset = entry.Offset;
    meta.Tag = entry.Tag;
    meta.Size = entry.Count;
    _metaFiles.Add(meta);
    #endif
  }

  headerSize += k_HeaderSig_Size;
  _headersSize += headerSize;
  if (isMainHeader && _headerPlusPayload_Size_Defined)
  {
    if (_headerPlusPayload_Size < headerSize)
      return S_FALSE;
    _payloadSize = _headerPlusPayload_Size - headerSize;
    _size = _payloadSize;
    _phySize = _headersSize + _payloadSize;
    _payloadSize_Defined = true;
    _phySize_Defined = true;
  }
  return S_OK;
}

HRESULT CHandler::Open2(ISequentialInStream *stream)
{
  {
    Byte buf[kLeadSize];
    RINOK(ReadStream_FALSE(stream, buf, kLeadSize))
    if (Get32(buf) != 0xEDABEEDB)
      return S_FALSE;
    _lead.Parse(buf);
    if (!_lead.IsSupported())
      return S_FALSE;
  }

  _headersSize = kLeadSize;

  if (_lead.SignatureType == RPMSIG_NONE)
  {

  }
  else if (_lead.SignatureType == RPMSIG_PGP262_1024)
  {
    Byte temp[256];
    RINOK(ReadStream_FALSE(stream, temp, sizeof(temp)))
  }
  else if (_lead.SignatureType == RPMSIG_HEADERSIG)
  {
    RINOK(ReadHeader(stream, false))
    unsigned pos = (unsigned)_headersSize & 7;
    if (pos != 0)
    {
      Byte temp[8];
      unsigned num = 8 - pos;
      RINOK(ReadStream_FALSE(stream, temp, num))
      _headersSize += num;
    }
  }
  else
    return S_FALSE;

  return ReadHeader(stream, true);
}


Z7_COM7F_IMF(CHandler::Open(IInStream *inStream, const UInt64 *, IArchiveOpenCallback *))
{
  COM_TRY_BEGIN
  {
    Close();
    RINOK(Open2(inStream))

    // start of payload is allowed to be unaligned
    RINOK(ReadStream_FALSE(inStream, _payloadSig, sizeof(_payloadSig)))

    if (!_payloadSize_Defined)
    {
      UInt64 endPos;
      RINOK(InStream_GetSize_SeekToEnd(inStream, endPos))
      _size = endPos - _headersSize;
    }
    _stream = inStream;
    return S_OK;
  }
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _headersSize = 0;
  _payloadSize = 0;
  _size = 0;
  _phySize = 0;
  _headerPlusPayload_Size = 0;

  _payloadSize_Defined = false;
  _phySize_Defined = false;
  _headerPlusPayload_Size_Defined = false;
  _time_Defined = false;

  _name.Empty();
  _version.Empty();
  _release.Empty();
  _arch.Empty();
  _os.Empty();
  
  _format.Empty();
  _compressor.Empty();

  #ifdef Z7_RPM_SHOW_METADATA
  _metadata.Empty();
  _metaFiles.Size();
  #endif

  _stream.Release();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = 1
  #ifdef Z7_RPM_SHOW_METADATA
    + _metaFiles.Size()
  #endif
  ;

  return S_OK;
}

static const Byte k_Signature[] = { 0xED, 0xAB, 0xEE, 0xDB};

REGISTER_ARC_I(
  "Rpm", "rpm", NULL, 0xEB,
  k_Signature,
  0,
  0,
  NULL)

}}

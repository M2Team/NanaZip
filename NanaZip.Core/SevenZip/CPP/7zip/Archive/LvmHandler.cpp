// LvmHandler.cpp

#include "StdAfx.h"

#include "../../../C/7zCrc.h"
#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/MyBuffer.h"
#include "../../Common/StringToInt.h"

#include "../../Windows/PropVariantUtils.h"
#include "../../Windows/TimeUtils.h"

#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "HandlerCont.h"

#define Get32(p) GetUi32(p)
#define Get64(p) GetUi64(p)

#define LE_32(offs, dest) dest = Get32(p + (offs))
#define LE_64(offs, dest) dest = Get64(p + (offs))

using namespace NWindows;

namespace NArchive {
namespace NLvm {

#define SIGNATURE { 'L', 'A', 'B', 'E', 'L', 'O', 'N', 'E'  }
  
static const unsigned k_SignatureSize = 8;
static const Byte k_Signature[k_SignatureSize] = SIGNATURE;

static const unsigned k_Signature2Size = 8;
static const Byte k_Signature2[k_Signature2Size] =
    { 'L', 'V', 'M', '2', ' ', '0', '0', '1'  };

static const Byte FMTT_MAGIC[16] =
    { ' ', 'L', 'V', 'M', '2', ' ', 'x', '[', '5', 'A', '%', 'r', '0', 'N', '*', '>'  };

static const UInt32 kSectorSize = 512;


struct CPropVal
{
  bool IsNumber;
  AString String;
  UInt64 Number;
  
  CPropVal(): IsNumber(false), Number(0) {}
};


struct CConfigProp
{
  AString Name;
  
  bool IsVector;
  CPropVal Val;
  CObjectVector<CPropVal> Vector;

  CConfigProp(): IsVector(false) {}
};


class CConfigItem
{
public:
  AString Name;
  CObjectVector<CConfigProp> Props;
  CObjectVector<CConfigItem> Items;
  
  const char *ParseItem(const char *s, int numAllowedLevels);

  int FindProp(const char *name) const throw();
  bool GetPropVal_Number(const char *name, UInt64 &val) const throw();
  bool GetPropVal_String(const char *name, AString &val) const;

  int FindSubItem(const char *tag) const throw();
};

struct CConfig
{
  CConfigItem Root;

  bool Parse(const char *s);
};


static bool IsSpaceChar(char c)
{
  return (c == ' ' || c == '\t' || c == 0x0D || c == 0x0A);
}

static const char *SkipSpaces(const char * s)
{
  for (;; s++)
  {
    const char c = *s;
    if (c == 0)
      return s;
    if (!IsSpaceChar(c))
    {
      if (c != '#')
        return s;
      s++;
      for (;;)
      {
        const char c2 = *s;
        if (c2 == 0)
          return s;
        if (c2 == '\n')
          break;
        s++;
      }
    }
  }
}

#define SKIP_SPACES(s) s = SkipSpaces(s);

int CConfigItem::FindProp(const char *name) const throw()
{
  FOR_VECTOR (i, Props)
    if (Props[i].Name == name)
      return (int)i;
  return -1;
}

bool CConfigItem::GetPropVal_Number(const char *name, UInt64 &val) const throw()
{
  val = 0;
  int index = FindProp(name);
  if (index < 0)
    return false;
  const CConfigProp &prop = Props[index];
  if (prop.IsVector)
    return false;
  if (!prop.Val.IsNumber)
    return false;
  val = prop.Val.Number;
  return true;
}

bool CConfigItem::GetPropVal_String(const char *name, AString &val) const
{
  val.Empty();
  int index = FindProp(name);
  if (index < 0)
    return false;
  const CConfigProp &prop = Props[index];
  if (prop.IsVector)
    return false;
  if (prop.Val.IsNumber)
    return false;
  val = prop.Val.String;
  return true;
}

int CConfigItem::FindSubItem(const char *tag) const throw()
{
  FOR_VECTOR (i, Items)
    if (Items[i].Name == tag)
      return (int)i;
  return -1;
}

static const char *FillProp(const char *s, CPropVal &val)
{
  SKIP_SPACES(s)
  const char c = *s;
  if (c == 0)
    return NULL;
  
  if (c == '\"')
  {
    s++;
    val.IsNumber = false;
    val.String.Empty();

    for (;;)
    {
      const char c2 = *s;
      if (c2 == 0)
        return NULL;
      s++;
      if (c2 == '\"')
        break;
      val.String += c2;
    }
  }
  else
  {
    const char *end;
    val.IsNumber = true;
    val.Number = ConvertStringToUInt64(s, &end);
    if (s == end)
      return NULL;
    s = end;
  }

  SKIP_SPACES(s)
  return s;
}


const char *CConfigItem::ParseItem(const char *s, int numAllowedLevels)
{
  if (numAllowedLevels < 0)
    return NULL;

  for (;;)
  {
    SKIP_SPACES(s)
    const char *beg = s;
    
    for (;; s++)
    {
      char c = *s;
      if (c == 0 || c == '}')
      {
        if (s != beg)
          return NULL;
        return s;
      }
      if (IsSpaceChar(c) || c == '=' || c == '{')
        break;
    }
    
    if (s == beg)
      return NULL;

    AString name;
    name.SetFrom(beg, (unsigned)(s - beg));
    
    SKIP_SPACES(s)
    
    if (*s == 0 || *s == '}')
      return NULL;
    
    if (*s == '{')
    {
      s++;
      CConfigItem &item = Items.AddNew();
      item.Name = name;
      s = item.ParseItem(s, numAllowedLevels - 1);
      if (!s)
        return NULL;
      if (*s != '}')
        return NULL;
      s++;
      continue;
    }

    if (*s != '=')
      continue;

    s++;
    SKIP_SPACES(s)
    if (*s == 0)
      return NULL;
    CConfigProp &prop = Props.AddNew();
    
    prop.Name = name;
    
    if (*s == '[')
    {
      s++;
      prop.IsVector = true;
      
      for (;;)
      {
        SKIP_SPACES(s)
        char c = *s;
        if (c == 0)
          return NULL;
        if (c == ']')
        {
          s++;
          break;
        }
        
        CPropVal val;
        
        s = FillProp(s, val);
        if (!s)
          return NULL;
        prop.Vector.Add(val);
        SKIP_SPACES(s)
        
        if (*s == ',')
        {
          s++;
          continue;
        }
        if (*s != ']')
          return NULL;
        s++;
        break;
      }
    }
    else
    {
      prop.IsVector = false;
      s = FillProp(s, prop.Val);
      if (!s)
        return NULL;
    }
  }
}


bool CConfig::Parse(const char *s)
{
  s = Root.ParseItem(s, 10);
  if (!s)
    return false;
  SKIP_SPACES(s)
  return *s == 0;
}


/*
static const CUInt32PCharPair g_PartitionFlags[] =
{
  { 0, "Sys" },
  { 1, "Ignore" },
  { 2, "Legacy" },
  { 60, "Win-Read-only" },
  { 62, "Win-Hidden" },
  { 63, "Win-Not-Automount" }
};
*/

/*
static inline char GetHex(unsigned t) { return (char)(((t < 10) ? ('0' + t) : ('A' + (t - 10)))); }

static void PrintHex(unsigned v, char *s)
{
  s[0] = GetHex((v >> 4) & 0xF);
  s[1] = GetHex(v & 0xF);
}

static void ConvertUInt16ToHex4Digits(UInt32 val, char *s) throw()
{
  PrintHex(val >> 8, s);
  PrintHex(val & 0xFF, s + 2);
}

static void GuidToString(const Byte *g, char *s)
{
  ConvertUInt32ToHex8Digits(Get32(g   ),  s);  s += 8;  *s++ = '-';
  ConvertUInt16ToHex4Digits(Get16(g + 4), s);  s += 4;  *s++ = '-';
  ConvertUInt16ToHex4Digits(Get16(g + 6), s);  s += 4;  *s++ = '-';
  for (unsigned i = 0; i < 8; i++)
  {
    if (i == 2)
      *s++ = '-';
    PrintHex(g[8 + i], s);
    s += 2;
  }
  *s = 0;
}

*/

struct CPhyVol
{
  AString Name;

  // AString id;
  // AString device; // "/dev/sda2"
  // AString status; // ["ALLOCATABLE"]
  // AString flags; // []
  // UInt64 dev_size; // in sectors
  UInt64 pe_start; // in sectors
  UInt64 pe_count; // in extents

  bool Parse(const CConfigItem &ci)
  {
    Name = ci.Name;
    // ci.GetPropVal_String("id", id);
    // ci.GetPropVal_String("device", device);
    bool res = true;
    // if (!ci.GetPropVal_Number("dev_size", dev_size)) res = false;
    if (!ci.GetPropVal_Number("pe_start", pe_start)) res = false;
    if (!ci.GetPropVal_Number("pe_count", pe_count)) res = false;
    return res;
  }
};

struct CStripe
{
  AString Name; // "pv0";
  UInt64 ExtentOffset; // ????
};

struct CSegment
{
  UInt64 start_extent;
  UInt64 extent_count;
  AString type;
  CObjectVector<CStripe> stripes;

  bool IsPosSizeOk() const
  {
    return
        (start_extent < ((UInt64)1 << 63)) &&
        (extent_count < ((UInt64)1 << 63));
  }

  UInt64 GetEndExtent() const { return start_extent + extent_count; }

  bool Parse(const CConfigItem &si)
  {
    UInt64 stripe_count;

    if (!si.GetPropVal_Number("start_extent", start_extent)) return false;
    if (!si.GetPropVal_Number("extent_count", extent_count)) return false;
    if (!si.GetPropVal_Number("stripe_count", stripe_count)) return false;
    if (!si.GetPropVal_String("type", type)) return false;

    //if (stripe_count != 1) return false;

    const int spi = si.FindProp("stripes");
    if (spi < 0)
      return false;

    const CConfigProp &prop = si.Props[spi];
    if (!prop.IsVector)
      return false;

    if (stripe_count > (1 << 20))
      return false;

    const unsigned numStripes = (unsigned)stripe_count;
    if (prop.Vector.Size() != numStripes * 2)
      return false;

    for (unsigned i = 0; i < numStripes; i++)
    {
      const CPropVal &v0 = prop.Vector[i * 2];
      const CPropVal &v1 = prop.Vector[i * 2 + 1];
      if (v0.IsNumber || !v1.IsNumber)
        return false;
      CStripe stripe;
      stripe.Name = v0.String;
      stripe.ExtentOffset = v1.Number;
      stripes.Add(stripe);
    }
       
    return true;
  }
};


struct CLogVol
{
  bool IsSupported;

  AString Name;

  AString id;
  AString status; // ["READ", "WRITE", "VISIBLE"]
  AString flags; // []

  // UInt64 Pos;
  // UInt64 Size;
  
  // UInt64 GetSize() const { return Size; }
  // UInt64 GetPos() const { return Pos; }

  CObjectVector<CSegment> Segments;

  CLogVol(): /* Pos(0), Size(0), */ IsSupported(false) {}

  bool Parse(const CConfigItem &ci)
  {
    Name = ci.Name;

    UInt64 segment_count;
    if (!ci.GetPropVal_Number("segment_count", segment_count))
      return false;
    
    if (ci.Items.Size() != segment_count)
      return false;
    
    FOR_VECTOR (segIndex, ci.Items)
    {
      const CConfigItem &si = ci.Items[segIndex];
      {
        AString t ("segment");
        t.Add_UInt32(segIndex + 1);
        if (si.Name != t)
          return false;
      }
      
      CSegment segment;
      
      if (!segment.Parse(si))
        return false;
      
      // item.Size += (segment.extent_count * _extentSize) << 9;
      
      Segments.Add(segment);
    }
    
    IsSupported = true;
    return true;
  }

  bool GetNumExtents(UInt64 &numExtents) const
  {
    numExtents = 0;
    if (Segments.IsEmpty())
      return true;
    unsigned i;
    for (i = 1; i < Segments.Size(); i++)
      if (!Segments[i].IsPosSizeOk())
        return false;
    for (i = 1; i < Segments.Size(); i++)
      if (Segments[i - 1].GetEndExtent() != Segments[i].start_extent)
        return false;
    numExtents = Segments.Back().GetEndExtent();
    return true;
  }
};


struct CItem
{
  int LogVol;
  int PhyVol;
  UInt64 Pos;
  UInt64 Size;
  AString Name;
  bool IsSupported;

  CItem(): LogVol(-1), PhyVol(-1), Pos(0), Size(0), IsSupported(false) {}
};


struct CVolGroup
{
  CObjectVector<CLogVol> _logVols;
  CObjectVector<CPhyVol> _phyVols;
  AString _id;
  int _extentSizeBits;

  /*
  UInt64 secno; // 3
  AString status; // ["RESIZEABLE", "READ", "WRITE"]
  AString flags; // []
  UInt64 max_lv; // 0
  UInt64 max_pv; // 0
  UInt64 metadata_copies; // 0
  */

  void Clear()
  {
    _logVols.Clear();
    _phyVols.Clear();
    _id.Empty();
    _extentSizeBits = -1;
  }
};


Z7_class_CHandler_final: public CHandlerCont, public CVolGroup
{
  Z7_IFACE_COM7_IMP(IInArchive_Cont)

  CObjectVector<CItem> _items;

  UInt64 _cTime;

  bool _isArc;

  UInt64 _phySize;
  CByteBuffer _buffer;

  UInt64 _cfgPos;
  UInt64 _cfgSize;

  HRESULT Open2(IInStream *stream);
  
  virtual int GetItem_ExtractInfo(UInt32 index, UInt64 &pos, UInt64 &size) const Z7_override
  {
    if (index >= _items.Size())
    {
      pos = _cfgPos;
      size = _cfgSize;
      return NExtract::NOperationResult::kOK;
    }
    const CItem &item = _items[index];
    if (!item.IsSupported)
      return NExtract::NOperationResult::kUnsupportedMethod;
    pos = item.Pos;
    size = item.Size;
    return NExtract::NOperationResult::kOK;
  }
};

static const UInt32 LVM_CRC_INIT_VAL = 0xf597a6cf;

static UInt32 Z7_FASTCALL LvmCrcCalc(const void *data, size_t size)
{
  return CrcUpdate(LVM_CRC_INIT_VAL, data, size);
}

struct CRawLocn
{
  UInt64 Offset;  /* Offset in bytes to start sector */
  UInt64 Size;    /* Bytes */
  UInt32 Checksum;
  UInt32 Flags;

  bool IsEmpty() const { return Offset == 0 && Size == 0; }

  void Parse(const Byte *p)
  {
    LE_64(0x00, Offset);
    LE_64(0x08, Size);
    LE_32(0x10, Checksum);
    LE_32(0x14, Flags);
  }
};

// #define MDA_HEADER_SIZE 512

struct mda_header
{
  UInt64 Start;   /* Absolute start byte of mda_header */
  UInt64 Size;    /* Size of metadata area */

  CRecordVector<CRawLocn> raw_locns;

  bool Parse(const Byte *p, size_t size)
  {
    if (memcmp(p + 4, FMTT_MAGIC, 16) != 0)
      return false;
    UInt32 version;
    LE_32(0x14, version);
    if (version != 1)
      return false;
    LE_64(0x18, Start);
    LE_64(0x20, Size);

    unsigned pos = 0x28;

    for (;;)
    {
      if (pos + 0x18 > size)
        return false;
      CRawLocn locn;
      locn.Parse(p + pos);
      if (locn.IsEmpty())
        break;
      pos += 0x18;
      raw_locns.Add(locn);
    }

    return true;
  }
};


static int inline GetLog(UInt64 num)
{
  for (unsigned i = 0; i < 64; i++)
    if (((UInt64)1 << i) == num)
      return (int)i;
  return -1;
}

#define ID_LEN 32

HRESULT CHandler::Open2(IInStream *stream)
{
  _buffer.Alloc(kSectorSize * 2);
  RINOK(ReadStream_FALSE(stream, _buffer, kSectorSize * 2))
  
  const Byte *buf = _buffer;
  
  buf += kSectorSize;

  // label_header

  if (memcmp(buf, k_Signature, k_SignatureSize) != 0)
    return S_FALSE;
  const UInt64 sectorNumber = Get64(buf + 8);
  if (sectorNumber != 1)
    return S_FALSE;
  if (Get32(buf + 16) != LvmCrcCalc(buf + 20, kSectorSize - 20))
    return S_FALSE;

  const UInt32 offsetToCont = Get32(buf + 20);
  if (memcmp(buf + 24, k_Signature2, k_Signature2Size) != 0)
    return S_FALSE;

  if (offsetToCont != 32)
    return S_FALSE;

  // pv_header

  size_t pos = offsetToCont;
  const Byte *p = buf;
  
  /*
  {
    Byte id[ID_LEN];
    memcpy(id, p + pos, ID_LEN);
  }
  */

  pos += ID_LEN;
  const UInt64 device_size_xl = Get64(p + pos);
  pos += 8;

  _phySize = device_size_xl;
  _isArc = true;

  for (;;)
  {
    if (pos > kSectorSize - 16)
      return S_FALSE;
    // disk_locn (data areas)
    UInt64 offset = Get64(p + pos);
    UInt64 size = Get64(p + pos + 8);
    pos += 16;
    if (offset == 0 && size == 0)
      break;
  }

  CConfig cfg;
  // bool isFinded = false;

  // for (;;)
  {
    if (pos > kSectorSize - 16)
      return S_FALSE;
    // disk_locn (metadata area headers)
    UInt64 offset = Get64(p + pos);
    UInt64 size = Get64(p + pos + 8);
    pos += 16;
    if (offset == 0 && size == 0)
    {
      // break;
      return S_FALSE;
    }

    CByteBuffer meta;
    const size_t sizeT = (size_t)size;
    if (sizeT != size)
      return S_FALSE;
    meta.Alloc(sizeT);
    RINOK(InStream_SeekSet(stream, offset))
    RINOK(ReadStream_FALSE(stream, meta, sizeT))
    if (Get32(meta) != LvmCrcCalc(meta + 4, kSectorSize - 4))
      return S_FALSE;
    mda_header mh;
    if (!mh.Parse(meta, kSectorSize))
      return S_FALSE;

    if (mh.raw_locns.Size() != 1)
      return S_FALSE;
    unsigned g = 0;
    // for (unsigned g = 0; g < mh.raw_locns.Size(); g++)
    {
      const CRawLocn &locn = mh.raw_locns[g];
      
      CByteBuffer vgBuf;
      if (locn.Size > ((UInt32)1 << 24))
        return S_FALSE;

      const size_t vgSize = (size_t)locn.Size;
      if (vgSize == 0)
        return S_FALSE;

      vgBuf.Alloc(vgSize);

      _cfgPos = offset + locn.Offset;
      _cfgSize = vgSize;
      RINOK(InStream_SeekSet(stream, _cfgPos))
      RINOK(ReadStream_FALSE(stream, vgBuf, vgSize))
      if (locn.Checksum != LvmCrcCalc(vgBuf, vgSize))
        return S_FALSE;

      {
        AString s;
        s.SetFrom_CalcLen((const char *)(const Byte *)vgBuf, (unsigned)vgSize);
        _cfgSize = s.Len();
        if (!cfg.Parse(s))
          return S_FALSE;
        // isFinded = true;
        // break;
      }
    }
    
    // if (isFinded) break;
  }

  // if (!isFinded) return S_FALSE;

  if (cfg.Root.Items.Size() != 1)
    return S_FALSE;
  const CConfigItem &volGroup = cfg.Root.Items[0];
  if (volGroup.Name != "VolGroup00")
    return S_FALSE;

  volGroup.GetPropVal_String("id", _id);

  if (!cfg.Root.GetPropVal_Number("creation_time", _cTime))
    _cTime = 0;

  UInt64 extentSize;
  if (!volGroup.GetPropVal_Number("extent_size", extentSize))
    return S_FALSE;

  _extentSizeBits = GetLog(extentSize);
  if (_extentSizeBits < 0 || _extentSizeBits > (62 - 9))
    return S_FALSE;

 
  {
    int pvsIndex = volGroup.FindSubItem("physical_volumes");
    if (pvsIndex < 0)
      return S_FALSE;
    
    const CConfigItem &phyVols = volGroup.Items[pvsIndex];
    
    FOR_VECTOR (i, phyVols.Items)
    {
      const CConfigItem &ci = phyVols.Items[i];
      CPhyVol pv;
      if (!pv.Parse(ci))
        return S_FALSE;
      _phyVols.Add(pv);
    }
  }

  {
    int lvIndex = volGroup.FindSubItem("logical_volumes");
    if (lvIndex < 0)
      return S_FALSE;
    
    const CConfigItem &logVolumes = volGroup.Items[lvIndex];
    
    FOR_VECTOR (i, logVolumes.Items)
    {
      const CConfigItem &ci = logVolumes.Items[i];
      CLogVol &lv = _logVols.AddNew();
      lv.Parse(ci) ; // check error
    }
  }

  {
    FOR_VECTOR (i, _logVols)
    {
      CLogVol &lv = _logVols[i];
      
      CItem item;
      
      item.LogVol = (int)i;
      item.Pos = 0;
      item.Size = 0;
      item.Name = lv.Name;

      if (lv.IsSupported)
      {
        UInt64 numExtents;
        lv.IsSupported = lv.GetNumExtents(numExtents);

        if (lv.IsSupported)
        {
          lv.IsSupported = false;
          item.Size = numExtents << (_extentSizeBits + 9);
          
          if (lv.Segments.Size() == 1)
          {
            const CSegment &segment = lv.Segments[0];
            if (segment.stripes.Size() == 1)
            {
              const CStripe &stripe = segment.stripes[0];
              FOR_VECTOR (pvIndex, _phyVols)
              {
                const CPhyVol &pv = _phyVols[pvIndex];
                if (pv.Name == stripe.Name)
                {
                  item.Pos = (pv.pe_start + (stripe.ExtentOffset << _extentSizeBits)) << 9;
                  lv.IsSupported = true;
                  item.IsSupported = true;
                  break;
                }
              }
            }
          }
        }
      }

      _items.Add(item);
    }
  }

  {
    FOR_VECTOR (i, _phyVols)
    {
      const CPhyVol &pv = _phyVols[i];

      if (pv.pe_start > (UInt64)1 << (62 - 9))
        return S_FALSE;
      if (pv.pe_count > (UInt64)1 << (62 - 9 - _extentSizeBits))
        return S_FALSE;

      CItem item;
      
      item.PhyVol = (int)i;
      item.Pos = pv.pe_start << 9;
      item.Size = pv.pe_count << (_extentSizeBits + 9);
      item.Name = pv.Name;
      item.IsSupported = true;

      _items.Add(item);
    }
  }

  return S_OK;
}


Z7_COM7F_IMF(CHandler::Open(IInStream *stream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback * /* openArchiveCallback */))
{
  COM_TRY_BEGIN
  Close();
  RINOK(Open2(stream))
  _stream = stream;
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::Close())
{
  CVolGroup::Clear();
  
  _cfgPos = 0;
  _cfgSize = 0;
  _cTime = 0;
  _phySize = 0;
  _isArc = false;
  _items.Clear();

  _stream.Release();
  return S_OK;
}


static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidFileSystem,
  kpidCharacts,
  kpidOffset,
  kpidId
};

static const Byte kArcProps[] =
{
  kpidId,
  kpidCTime,
  kpidClusterSize
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  
  switch (propID)
  {
    case kpidMainSubfile:
    {
      /*
      if (_items.Size() == 1)
        prop = (UInt32)0;
      */
      break;
    }
    case kpidPhySize: if (_phySize != 0) prop = _phySize; break;
    case kpidId:
    {
      prop = _id;
      break;
    }
    case kpidClusterSize:
    {
      if (_extentSizeBits >= 0)
        prop = ((UInt64)1 << (_extentSizeBits + 9));
      break;
    }
    case kpidCTime:
    {
      if (_cTime != 0)
      {
        FILETIME ft;
        NTime::UnixTime64_To_FileTime((Int64)_cTime, ft);
        prop = ft;
      }
      break;
    }
    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc) v |= kpv_ErrorFlags_IsNotArc;
      // if (_unsupported) v |= kpv_ErrorFlags_UnsupportedMethod;
      // if (_headerError) v |= kpv_ErrorFlags_HeadersError;
      if (v == 0 && !_stream)
        v |= kpv_ErrorFlags_UnsupportedMethod;
      if (v != 0)
        prop = v;
      break;
    }
  }

  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _items.Size() + (_cfgSize == 0 ? 0 : 1);
  return S_OK;
}


Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;

  const CItem &item = _items[index];

  // const CLogVol &item = _items[index];

  if (index >= _items.Size())
  {
    switch (propID)
    {
      case kpidPath:
      {
        prop = "meta.txt";
        break;
      }
    
      case kpidSize:
      case kpidPackSize: prop = _cfgSize; break;
    }
  }
  else
  {
    switch (propID)
    {
    case kpidPath:
      {
        AString s = item.Name;
        s += ".img";
        prop = s;
        break;
      }
      
    case kpidSize:
    case kpidPackSize: prop = item.Size; break;
    case kpidOffset: prop = item.Pos; break;
      
    case kpidId:
      {
        // prop = item.id;
        break;
      }
      
      // case kpidCharacts: FLAGS64_TO_PROP(g_PartitionFlags, item.Flags, prop); break;
    }
  }

  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

REGISTER_ARC_I(
  "LVM", "lvm", NULL, 0xBF,
  k_Signature,
  kSectorSize,
  0,
  NULL)

}}

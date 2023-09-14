// VmdkHandler.cpp

#include "StdAfx.h"

// #include <stdio.h>

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/StringConvert.h"
#include "../../Common/StringToInt.h"
#include "../../Common/UTFConvert.h"

#include "../../Windows/PropVariant.h"

#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#include "../Compress/ZlibDecoder.h"

#include "HandlerCont.h"

using namespace NWindows;

namespace NArchive {
namespace NVmdk {

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)
#define Get64(p) GetUi64(p)

#define LE_16(offs, dest) dest = Get16(p + (offs))
#define LE_32(offs, dest) dest = Get32(p + (offs))
#define LE_64(offs, dest) dest = Get64(p + (offs))


static const Byte k_Signature[] = { 'K', 'D', 'M', 'V' };

static const UInt32 k_Flags_NL         = (UInt32)1 << 0;
// static const UInt32 k_Flags_RGD        = (UInt32)1 << 1;
static const UInt32 k_Flags_ZeroGrain  = (UInt32)1 << 2;
static const UInt32 k_Flags_Compressed = (UInt32)1 << 16;
static const UInt32 k_Flags_Marker     = (UInt32)1 << 17;

static const unsigned k_NumMidBits = 9; // num bits for index in Grain Table

struct CHeader
{
  UInt32 flags;
  UInt32 version;

  UInt64 capacity;
  UInt64 grainSize;
  UInt64 descriptorOffset;
  UInt64 descriptorSize;

  UInt32 numGTEsPerGT;
  UInt16 algo;
  // Byte uncleanShutdown;
  // UInt64 rgdOffset;
  UInt64 gdOffset;
  UInt64 overHead;

  bool Is_NL()         const { return (flags & k_Flags_NL) != 0; }
  bool Is_ZeroGrain()  const { return (flags & k_Flags_ZeroGrain) != 0; }
  bool Is_Compressed() const { return (flags & k_Flags_Compressed) != 0; }
  bool Is_Marker()     const { return (flags & k_Flags_Marker) != 0; }

  bool Parse(const Byte *p);

  bool IsSameImageFor(const CHeader &h) const
  {
    return flags == h.flags
        && version == h.version
        && capacity == h.capacity
        && grainSize == h.grainSize
        && algo == h.algo;
  }
};

bool CHeader::Parse(const Byte *p)
{
  if (memcmp(p, k_Signature, sizeof(k_Signature)) != 0)
    return false;

  LE_32 (0x04, version);
  LE_32 (0x08, flags);
  LE_64 (0x0C, capacity);
  LE_64 (0x14, grainSize);
  LE_64 (0x1C, descriptorOffset);
  LE_64 (0x24, descriptorSize);
  LE_32 (0x2C, numGTEsPerGT);
  // LE_64 (0x30, rgdOffset);
  LE_64 (0x38, gdOffset);
  LE_64 (0x40, overHead);
  // uncleanShutdown = buf[0x48];
  LE_16(0x4D, algo);

  if (Is_NL() && Get32(p + 0x49) != 0x0A0D200A) // do we need Is_NL() check here?
    return false;

  return (numGTEsPerGT == (1 << k_NumMidBits)) && (version <= 3);
}


enum
{
  k_Marker_END_OF_STREAM = 0,
  k_Marker_GRAIN_TABLE   = 1,
  k_Marker_GRAIN_DIR     = 2,
  k_Marker_FOOTER        = 3
};

struct CMarker
{
  UInt64 NumSectors;
  UInt32 SpecSize; // = 0 for metadata sectors
  UInt32 Type;

  void Parse(const Byte *p)
  {
    LE_64 (0, NumSectors);
    LE_32 (8, SpecSize);
    LE_32 (12, Type);
  }
};


static bool Str_to_ValName(const AString &s, AString &name, AString &val)
{
  name.Empty();
  val.Empty();
  int qu = s.Find('"');
  int eq = s.Find('=');
  if (eq < 0 || (qu >= 0 && eq > qu))
    return false;
  name.SetFrom(s.Ptr(), eq);
  name.Trim();
  val = s.Ptr(eq + 1);
  val.Trim();
  return true;
}

static inline bool IsSpaceChar(char c)
{
  return (c == ' ' || c == '\t');
}

static const char *SkipSpaces(const char *s)
{
  for (;; s++)
  {
    char c = *s;
    if (c == 0 || !IsSpaceChar(c))
      return s;
  }
}

#define SKIP_SPACES(s) s = SkipSpaces(s);

static const char *GetNextWord(const char *s, AString &dest)
{
  dest.Empty();
  SKIP_SPACES(s)
  const char *start = s;
  for (;; s++)
  {
    char c = *s;
    if (c == 0 || IsSpaceChar(c))
    {
      dest.SetFrom(start, (unsigned)(s - start));
      return s;
    }
  }
}

static const char *GetNextNumber(const char *s, UInt64 &val)
{
  SKIP_SPACES(s)
  if (*s == 0)
    return s;
  const char *end;
  val = ConvertStringToUInt64(s, &end);
  char c = *end;
  if (c != 0 && !IsSpaceChar(c))
    return NULL;
  return end;
}


struct CExtentInfo
{
  AString Access;    // RW, RDONLY, or NOACCESS
  UInt64 NumSectors; // 512 bytes sectors
  AString Type;      // FLAT, SPARSE, ZERO, VMFS, VMFSSPARSE, VMFSRDM, VMFSRAW
  AString FileName;
  UInt64 StartSector; // used for FLAT

  // for VMWare Player 9:
  // PartitionUUID
  // DeviceIdentifier

  bool IsType_ZERO() const { return Type == "ZERO"; }
  // bool IsType_FLAT() const { return Type == "FLAT"; }
  bool IsType_Flat() const { return Type == "FLAT" || Type == "VMFS" || Type == "VMFSRAW"; }
  
  bool Parse(const char *s);
};

bool CExtentInfo::Parse(const char *s)
{
  NumSectors = 0;
  StartSector = 0;
  Access.Empty();
  Type.Empty();
  FileName.Empty();

  s = GetNextWord(s, Access);
  s = GetNextNumber(s, NumSectors);
  if (!s)
    return false;
  s = GetNextWord(s, Type);

  if (Type.IsEmpty())
    return false;

  SKIP_SPACES(s)

  if (IsType_ZERO())
    return (*s == 0);

  if (*s != '\"')
    return false;
  s++;
  {
    const char *s2 = strchr(s, '\"');
    if (!s2)
      return false;
    FileName.SetFrom(s, (unsigned)(s2 - s));
    s = s2 + 1;
  }
  SKIP_SPACES(s)
  if (*s == 0)
    return true;

  s = GetNextNumber(s, StartSector);
  if (!s)
    return false;
  return true;
  // SKIP_SPACES(s);
  // return (*s == 0);
}


struct CDescriptor
{
  AString CID;
  AString parentCID;
  AString createType;
  // AString encoding; // UTF-8, windows-1252 - default is UTF-8

  CObjectVector<CExtentInfo> Extents;

  static void GetUnicodeName(const AString &s, UString &res)
  {
    if (!ConvertUTF8ToUnicode(s, res))
      MultiByteToUnicodeString2(res, s);
  }

  void Clear()
  {
    CID.Empty();
    parentCID.Empty();
    createType.Empty();
    Extents.Clear();
  }

  bool IsThere_Parent() const
  {
    return !parentCID.IsEmpty() && !parentCID.IsEqualTo_Ascii_NoCase("ffffffff");
  }

  bool Parse(const Byte *p, size_t size);
};


bool CDescriptor::Parse(const Byte *p, size_t size)
{
  Clear();

  AString s;
  AString name;
  AString val;
  
  for (;;)
  {
    Byte c = 0;
    if (size != 0)
    {
      size--;
      c = *p++;
    }
    if (c == 0 || c == 0xA || c == 0xD)
    {
      if (!s.IsEmpty() && s[0] != '#')
      {
        if (Str_to_ValName(s, name, val))
        {
          if (name.IsEqualTo_Ascii_NoCase("CID"))
            CID = val;
          else if (name.IsEqualTo_Ascii_NoCase("parentCID"))
            parentCID = val;
          else if (name.IsEqualTo_Ascii_NoCase("createType"))
            createType = val;
        }
        else
        {
          CExtentInfo ei;
          if (!ei.Parse(s))
            return false;
          Extents.Add(ei);
        }
      }
      
      s.Empty();
      if (c == 0)
        return true;
    }
    else
      s += (char)c;
  }
}


struct CExtent
{
  bool IsOK;
  bool IsArc;
  bool NeedDeflate;
  bool Unsupported;
  bool IsZero;
  bool IsFlat;
  bool DescriptorOK;
  bool HeadersError;

  unsigned ClusterBits;
  UInt32 ZeroSector;

  CObjectVector<CByteBuffer> Tables;

  CMyComPtr<IInStream> Stream;
  UInt64 PosInArc;
  
  UInt64 PhySize;
  UInt64 VirtSize;     // from vmdk header of volume

  UInt64 StartOffset;  // virtual offset of this extent
  UInt64 NumBytes;     // from main descriptor, if multi-vol
  UInt64 FlatOffset;   // in Stream

  CByteBuffer DescriptorBuf;
  CDescriptor Descriptor;

  CHeader h;

  UInt64 GetEndOffset() const { return StartOffset + NumBytes; }
  
  bool IsVmdk() const { return !IsZero && !IsFlat; }
  // if (IsOK && IsVmdk()), then VMDK header of this extent was read
  
  CExtent():
      IsOK(false),
      IsArc(false),
      NeedDeflate(false),
      Unsupported(false),
      IsZero(false),
      IsFlat(false),
      DescriptorOK(false),
      HeadersError(false),

      ClusterBits(0),
      ZeroSector(0),
      
      PosInArc(0),
      
      PhySize(0),
      VirtSize(0),

      StartOffset(0),
      NumBytes(0),
      FlatOffset(0)
        {}
    

  HRESULT ReadForHeader(IInStream *stream, UInt64 sector, void *data, size_t numSectors);
  HRESULT Open3(IInStream *stream, IArchiveOpenCallback *openCallback,
        unsigned numVols, unsigned volIndex, UInt64 &complexity);

  HRESULT Seek(UInt64 offset)
  {
    PosInArc = offset;
    return InStream_SeekSet(Stream, offset);
  }

  HRESULT InitAndSeek()
  {
    if (Stream)
      return Seek(0);
    return S_OK;
  }

  HRESULT Read(void *data, size_t *size)
  {
    HRESULT res = ReadStream(Stream, data, size);
    PosInArc += *size;
    return res;
  }
};
  

Z7_class_CHandler_final: public CHandlerImg
{
  bool _isArc;
  bool _unsupported;
  bool _unsupportedSome;
  bool _headerError;
  bool _missingVol;
  bool _isMultiVol;
  bool _needDeflate;

  UInt64 _cacheCluster;
  unsigned _cacheExtent;
  CByteBuffer _cache;
  CByteBuffer _cacheCompressed;
  
  unsigned _clusterBitsMax;
  UInt64 _phySize;

  CObjectVector<CExtent> _extents;

  CBufInStream *_bufInStreamSpec;
  CMyComPtr<ISequentialInStream> _bufInStream;

  CBufPtrSeqOutStream *_bufOutStreamSpec;
  CMyComPtr<ISequentialOutStream> _bufOutStream;

  NCompress::NZlib::CDecoder *_zlibDecoderSpec;
  CMyComPtr<ICompressCoder> _zlibDecoder;

  CByteBuffer _descriptorBuf;
  CDescriptor _descriptor;

  UString _missingVolName;
  
  void InitAndSeekMain()
  {
    _virtPos = 0;
  }

  virtual HRESULT Open2(IInStream *stream, IArchiveOpenCallback *openCallback) Z7_override;
  virtual void CloseAtError() Z7_override;
public:
  Z7_IFACE_COM7_IMP(IInArchive_Img)

  Z7_IFACE_COM7_IMP(IInArchiveGetStream)
  Z7_IFACE_COM7_IMP(ISequentialInStream)
};


Z7_COM7F_IMF(CHandler::Read(void *data, UInt32 size, UInt32 *processedSize))
{
  if (processedSize)
    *processedSize = 0;
  if (_virtPos >= _size)
    return S_OK;
  {
    UInt64 rem = _size - _virtPos;
    if (size > rem)
      size = (UInt32)rem;
    if (size == 0)
      return S_OK;
  }

  unsigned extentIndex;
  {
    unsigned left = 0, right = _extents.Size();
    for (;;)
    {
      unsigned mid = (left + right) / 2;
      if (mid == left)
        break;
      if (_virtPos < _extents[mid].StartOffset)
        right = mid;
      else
        left = mid;
    }
    extentIndex = left;
  }
  
  CExtent &extent = _extents[extentIndex];

  {
    const UInt64 vir = _virtPos - extent.StartOffset;
    if (vir >= extent.NumBytes)
    {
      return E_FAIL;
      /*
      if (vir > extent.NumBytes)
        _stream_dataError = true;
      memset(data, 0, size);
      _virtPos += size;
      if (processedSize)
        *processedSize = size;
      return S_OK;
      */
    }

    {
      const UInt64 rem = extent.NumBytes - vir;
      if (size > rem)
        size = (UInt32)rem;
    }

    if (vir >= extent.VirtSize)
    {
      // if vmdk's VirtSize is smaller than VirtSize from main multi-volume descriptor
      _stream_dataError = true;
      return S_FALSE;
      /*
      memset(data, 0, size);
      _virtPos += size;
      if (processedSize)
        *processedSize = size;
      return S_OK;
      */
    }

    {
      const UInt64 rem = extent.VirtSize - vir;
      if (size > rem)
        size = (UInt32)rem;
    }

    if (extent.IsZero || !extent.IsOK || !extent.Stream || extent.Unsupported)
    {
      if (extent.Unsupported)
      {
        _stream_unsupportedMethod = true;
        return S_FALSE;
      }
      if (!extent.IsOK || !extent.Stream)
      {
        _stream_unavailData = true;
        return S_FALSE;
      }
      memset(data, 0, size);
      _virtPos += size;
      if (processedSize)
        *processedSize = size;
      return S_OK;
    }

    if (extent.IsFlat)
    {
      UInt64 offset = extent.FlatOffset + vir;
      if (offset != extent.PosInArc)
      {
        RINOK(extent.Seek(offset))
      }
      UInt32 size2 = 0;
      HRESULT res = extent.Stream->Read(data, size, &size2);
      if (res == S_OK && size2 == 0)
      {
        _stream_unavailData = true;
        /*
        memset(data, 0, size);
        _virtPos += size;
        if (processedSize)
          *processedSize = size;
        return S_OK;
        */
      }
      // _stream_PackSize += size2;
      extent.PosInArc += size2;
      _virtPos += size2;
      if (processedSize)
        *processedSize = size2;
      return res;
    }
  }

  
  for (;;)
  {
    const UInt64 vir = _virtPos - extent.StartOffset;
    const unsigned clusterBits = extent.ClusterBits;
    const UInt64 cluster = vir >> clusterBits;
    const size_t clusterSize = (size_t)1 << clusterBits;
    const size_t lowBits = (size_t)vir & (clusterSize - 1);
    {
      size_t rem = clusterSize - lowBits;
      if (size > rem)
        size = (UInt32)rem;
    }

    if (extentIndex == _cacheExtent && cluster == _cacheCluster)
    {
      memcpy(data, _cache + lowBits, size);
      _virtPos += size;
      if (processedSize)
        *processedSize = size;
      return S_OK;
    }
    
    const UInt64 high = cluster >> k_NumMidBits;
 
    if (high < extent.Tables.Size())
    {
      const CByteBuffer &table = extent.Tables[(unsigned)high];
    
      if (table.Size() != 0)
      {
        const size_t midBits = (size_t)cluster & ((1 << k_NumMidBits) - 1);
        const Byte *p = (const Byte *)table + (midBits << 2);
        const UInt32 v = Get32(p);
        
        if (v != 0 && v != extent.ZeroSector)
        {
          UInt64 offset = (UInt64)v << 9;
          if (extent.NeedDeflate)
          {
            if (offset != extent.PosInArc)
            {
              // printf("\n%12x %12x\n", (unsigned)offset, (unsigned)(offset - extent.PosInArc));
              RINOK(extent.Seek(offset))
            }
            
            const size_t kStartSize = 1 << 9;
            {
              size_t curSize = kStartSize;
              RINOK(extent.Read(_cacheCompressed, &curSize))
              // _stream_PackSize += curSize;
              if (curSize != kStartSize)
                return S_FALSE;
            }

            if (Get64(_cacheCompressed) != (cluster << (clusterBits - 9)))
              return S_FALSE;

            UInt32 dataSize = Get32(_cacheCompressed + 8);
            if (dataSize > ((UInt32)1 << 31))
              return S_FALSE;

            size_t dataSize2 = (size_t)dataSize + 12;
            
            if (dataSize2 > kStartSize)
            {
              dataSize2 = (dataSize2 + 511) & ~(size_t)511;
              if (dataSize2 > _cacheCompressed.Size())
                return S_FALSE;
              size_t curSize = dataSize2 - kStartSize;
              const size_t curSize2 = curSize;
              RINOK(extent.Read(_cacheCompressed + kStartSize, &curSize))
              // _stream_PackSize += curSize;
              if (curSize != curSize2)
                return S_FALSE;
            }

            _bufInStreamSpec->Init(_cacheCompressed + 12, dataSize);
            
            _cacheCluster = (UInt64)(Int64)-1;
            _cacheExtent = (unsigned)(int)-1;

            if (_cache.Size() < clusterSize)
              return E_FAIL;
            _bufOutStreamSpec->Init(_cache, clusterSize);
            
            // Do we need to use smaller block than clusterSize for last cluster?
            const UInt64 blockSize64 = clusterSize;
            HRESULT res = _zlibDecoder->Code(_bufInStream, _bufOutStream, NULL, &blockSize64, NULL);

            /*
            if (_bufOutStreamSpec->GetPos() != clusterSize)
            {
              _stream_dataError = true;
              memset(_cache + _bufOutStreamSpec->GetPos(), 0, clusterSize - _bufOutStreamSpec->GetPos());
            }
            */

              if (_bufOutStreamSpec->GetPos() != clusterSize
                  || _zlibDecoderSpec->GetInputProcessedSize() != dataSize)
              {
                _stream_dataError = true;
                if (res == S_OK)
                  res = S_FALSE;
              }

            RINOK(res)
            
            _cacheCluster = cluster;
            _cacheExtent = extentIndex;
            
            continue;
            /*
            memcpy(data, _cache + lowBits, size);
            _virtPos += size;
            if (processedSize)
              *processedSize = size;
            return S_OK;
            */
          }
          {
            offset += lowBits;
            if (offset != extent.PosInArc)
            {
              // printf("\n%12x %12x\n", (unsigned)offset, (unsigned)(offset - extent.PosInArc));
              RINOK(extent.Seek(offset))
            }
            UInt32 size2 = 0;
            HRESULT res = extent.Stream->Read(data, size, &size2);
            if (res == S_OK && size2 == 0)
            {
              _stream_unavailData = true;
              /*
              memset(data, 0, size);
              _virtPos += size;
              if (processedSize)
                *processedSize = size;
              return S_OK;
              */
            }
            extent.PosInArc += size2;
            // _stream_PackSize += size2;
            _virtPos += size2;
            if (processedSize)
              *processedSize = size2;
            return res;
          }
        }
      }
    }
    
    memset(data, 0, size);
    _virtPos += size;
    if (processedSize)
      *processedSize = size;
    return S_OK;
  }
}


static const Byte kProps[] =
{
  kpidSize,
  kpidPackSize
};

static const Byte kArcProps[] =
{
  kpidNumVolumes,
  kpidTotalPhySize,
  kpidMethod,
  kpidClusterSize,
  kpidHeadersSize,
  kpidId,
  kpidName,
  kpidComment
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps


Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  
  const CExtent *e = NULL;
  const CDescriptor *desc = NULL;
  
  if (_isMultiVol)
    desc = &_descriptor;
  else if (_extents.Size() == 1)
  {
    e = &_extents[0];
    desc = &e->Descriptor;
  }

  switch (propID)
  {
    case kpidMainSubfile: prop = (UInt32)0; break;
    case kpidPhySize: if (_phySize != 0) prop = _phySize; break;
    case kpidTotalPhySize:
    {
      UInt64 sum = _phySize;
      if (_isMultiVol)
      {
        FOR_VECTOR (i, _extents)
          sum += _extents[i].PhySize;
      }
      prop = sum;
      break;
    }
    case kpidClusterSize: prop = (UInt32)((UInt32)1 << _clusterBitsMax); break;
    case kpidHeadersSize: if (e) prop = (e->h.overHead << 9); break;
    case kpidMethod:
    {
      AString s;

      if (desc && !desc->createType.IsEmpty())
        s = desc->createType;

      bool zlib = false;
      bool marker = false;
      int algo = -1;

      FOR_VECTOR (i, _extents)
      {
        const CExtent &extent = _extents[i];
        if (!extent.IsOK || !extent.IsVmdk())
          continue;

        const CHeader &h = extent.h;

        if (h.algo != 0)
        {
          if (h.algo == 1)
            zlib = true;
          else if (algo != (int)h.algo)
          {
            s.Add_Space_if_NotEmpty();
            s.Add_UInt32(h.algo);
            algo = h.algo;
          }
        }
        
        if (h.Is_Marker())
          marker = true;
      }

      if (zlib)
        s.Add_OptSpaced("zlib");

      if (marker)
        s.Add_OptSpaced("Marker");
      
      if (!s.IsEmpty())
        prop = s;
      break;
    }
    
    case kpidComment:
    {
      if (e && e->DescriptorBuf.Size() != 0)
      {
        AString s;
        s.SetFrom_CalcLen((const char *)(const Byte *)e->DescriptorBuf, (unsigned)e->DescriptorBuf.Size());
        if (!s.IsEmpty() && s.Len() <= (1 << 16))
          prop = s;
      }
      break;
    }
    
    case kpidId:
    {
      if (desc && !desc->CID.IsEmpty())
      {
        prop = desc->CID;
      }
      break;
    }

    case kpidName:
    {
      if (!_isMultiVol && desc && desc->Extents.Size() == 1)
      {
        const CExtentInfo &ei = desc->Extents[0];
        if (!ei.FileName.IsEmpty())
        {
          UString u;
          CDescriptor::GetUnicodeName(ei.FileName, u);
          if (!u.IsEmpty())
            prop = u;
        }
      }
      break;
    }

    case kpidNumVolumes: if (_isMultiVol) prop = (UInt32)_extents.Size(); break;

    case kpidError:
    {
      if (_missingVol || !_missingVolName.IsEmpty())
      {
        UString s ("Missing volume : ");
        if (!_missingVolName.IsEmpty())
          s += _missingVolName;
        prop = s;
      }
      break;
    }

    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc) v |= kpv_ErrorFlags_IsNotArc;
      if (_unsupported) v |= kpv_ErrorFlags_UnsupportedMethod;
      if (_unsupportedSome) v |= kpv_ErrorFlags_UnsupportedMethod;
      if (_headerError) v |= kpv_ErrorFlags_HeadersError;
      // if (_missingVol)  v |= kpv_ErrorFlags_UnexpectedEnd;
      if (v != 0)
        prop = v;
      break;
    }
  }
  
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::GetProperty(UInt32 /* index */, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;

  switch (propID)
  {
    case kpidSize: prop = _size; break;
    case kpidPackSize:
    {
      UInt64 packSize = 0;
      FOR_VECTOR (i, _extents)
      {
        const CExtent &e = _extents[i];
        if (!e.IsOK)
          continue;
        if (e.IsVmdk() && !_isMultiVol)
        {
          UInt64 ov = (e.h.overHead << 9);
          if (e.PhySize >= ov)
            packSize += e.PhySize - ov;
        }
        else
          packSize += e.PhySize;
      }
      prop = packSize;
      break;
    }
    case kpidExtension: prop = (_imgExt ? _imgExt : "img"); break;
  }
  
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


static int inline GetLog(UInt64 num)
{
  for (int i = 0; i < 64; i++)
    if (((UInt64)1 << i) == num)
      return i;
  return -1;
}


HRESULT CExtent::ReadForHeader(IInStream *stream, UInt64 sector, void *data, size_t numSectors)
{
  sector <<= 9;
  RINOK(InStream_SeekSet(stream, sector))
  size_t size = numSectors << 9;
  RINOK(ReadStream_FALSE(stream, data, size))
  UInt64 end = sector + size;
  if (PhySize < end)
    PhySize = end;
  return S_OK;
}


void CHandler::CloseAtError()
{
  _extents.Clear();
  CHandlerImg::CloseAtError();
}


static const char * const kSignature_Descriptor = "# Disk DescriptorFile";


HRESULT CHandler::Open2(IInStream *stream, IArchiveOpenCallback *openCallback)
{
  const unsigned kSectoreSize = 512;
  Byte buf[kSectoreSize];
  size_t headerSize = kSectoreSize;
  RINOK(ReadStream(stream, buf, &headerSize))

  if (headerSize < sizeof(k_Signature))
    return S_FALSE;

  CMyComPtr<IArchiveOpenVolumeCallback> volumeCallback;

  if (memcmp(buf, k_Signature, sizeof(k_Signature)) != 0)
  {
    const size_t k_SigDesc_Size = strlen(kSignature_Descriptor);
    if (headerSize < k_SigDesc_Size)
      return S_FALSE;
    if (memcmp(buf, kSignature_Descriptor, k_SigDesc_Size) != 0)
      return S_FALSE;

    UInt64 endPos;
    RINOK(InStream_GetSize_SeekToEnd(stream, endPos))
    if (endPos > (1 << 20))
      return S_FALSE;
    const size_t numBytes = (size_t)endPos;
    _descriptorBuf.Alloc(numBytes);
    RINOK(InStream_SeekToBegin(stream))
    RINOK(ReadStream_FALSE(stream, _descriptorBuf, numBytes))

    if (!_descriptor.Parse(_descriptorBuf, _descriptorBuf.Size()))
      return S_FALSE;
    _isMultiVol = true;
    _isArc = true;
    _phySize = numBytes;
    if (_descriptor.IsThere_Parent())
      _unsupported = true;

    if (openCallback)
    {
      openCallback->QueryInterface(IID_IArchiveOpenVolumeCallback, (void **)&volumeCallback);
    }
    if (!volumeCallback)
    {
      _unsupported = true;
      return E_NOTIMPL;
    }

    /*
    UInt64 totalVirtSize = 0;
    FOR_VECTOR (i, _descriptor.Extents)
    {
      const CExtentInfo &ei = _descriptor.Extents[i];
      if (ei.NumSectors >= ((UInt64)1 << (63 - 9)))
        return S_FALSE;
      totalVirtSize += ei.NumSectors;
      if (totalVirtSize >= ((UInt64)1 << (63 - 9)))
        return S_FALSE;
    }
    totalVirtSize <<= 9;
    */

    if (_descriptor.Extents.Size() > 1)
    {
      const UInt64 numFiles = _descriptor.Extents.Size();
      RINOK(openCallback->SetTotal(&numFiles, NULL))
    }
  }

  UInt64 complexity = 0;

  for (;;)
  {
    CExtent *e = NULL;
    CMyComPtr<IInStream> nextStream;
    
    if (_isMultiVol)
    {
      const unsigned extentIndex = _extents.Size();
      if (extentIndex >= _descriptor.Extents.Size())
        break;
      const CExtentInfo &ei = _descriptor.Extents[extentIndex];
      e = &_extents.AddNew();
      e->StartOffset = 0;
      if (ei.NumSectors >= ((UInt64)1 << (62 - 9)) ||
          ei.StartSector >= ((UInt64)1 << (62 - 9)))
        return S_FALSE;
      e->NumBytes = ei.NumSectors << 9;
      e->IsZero = ei.IsType_ZERO();
      if (extentIndex != 0)
        e->StartOffset = _extents[extentIndex - 1].GetEndOffset();
      if (e->GetEndOffset() < e->StartOffset)
        return S_FALSE;

      e->VirtSize = e->NumBytes;
      if (e->IsZero)
      {
        e->IsOK = true;
        continue;
      }

      e->IsFlat = ei.IsType_Flat();
      e->FlatOffset = ei.StartSector << 9;

      UString u;
      CDescriptor::GetUnicodeName(ei.FileName, u);
      if (u.IsEmpty())
      {
        _missingVol = true;
        continue;
      }

      HRESULT result = volumeCallback->GetStream(u, &nextStream);

      if (result != S_OK && result != S_FALSE)
        return result;
      
      if (!nextStream || result != S_OK)
      {
        if (_missingVolName.IsEmpty())
          _missingVolName = u;
        _missingVol = true;
        continue;
      }

      if (e->IsFlat)
      {
        e->IsOK = true;
        e->Stream = nextStream;
        e->PhySize = e->NumBytes;
        continue;
      }

      stream = nextStream;

      headerSize = kSectoreSize;
      RINOK(ReadStream(stream, buf, &headerSize))

      if (headerSize != kSectoreSize)
        continue;
      if (memcmp(buf, k_Signature, sizeof(k_Signature)) != 0)
        continue;
    }
    else
    {
      if (headerSize != kSectoreSize)
        return S_FALSE;
      e = &_extents.AddNew();
      e->StartOffset = 0;
    }

    HRESULT res = S_FALSE;
    if (e->h.Parse(buf))
      res = e->Open3(stream, openCallback, _isMultiVol ? _descriptor.Extents.Size() : 1, _extents.Size() - 1, complexity);
    
    if (!_isMultiVol)
    {
      _isArc = e->IsArc;
      _phySize = e->PhySize;
      _unsupported = e->Unsupported;
    }

    if (e->Unsupported)
      _unsupportedSome = true;
    if (e->HeadersError)
      _headerError = true;

    if (res != S_OK)
    {
      if (res != S_FALSE)
        return res;
      if (!_isMultiVol)
        return res;
      continue;
    }

    e->Stream = stream;
    e->IsOK = true;
    
    if (!_isMultiVol)
    {
      e->NumBytes = e->VirtSize;
      break;
    }

    if (e->NumBytes != e->VirtSize)
      _headerError = true;
  }

  if (!_extents.IsEmpty())
    _size = _extents.Back().GetEndOffset();

  _needDeflate = false;
  _clusterBitsMax = 0;
  
  // unsigned numOKs = 0;
  unsigned numUnsupported = 0;

  FOR_VECTOR (i, _extents)
  {
    const CExtent &e = _extents[i];
    if (e.Unsupported)
      numUnsupported++;
    if (!e.IsOK)
      continue;
    // numOKs++;
    if (e.IsVmdk())
    {
      if (e.NeedDeflate)
        _needDeflate = true;
      if (_clusterBitsMax < e.ClusterBits)
        _clusterBitsMax = e.ClusterBits;
    }
  }

  if (numUnsupported != 0 && numUnsupported == _extents.Size())
    _unsupported = true;

  return S_OK;
}


HRESULT CExtent::Open3(IInStream *stream, IArchiveOpenCallback *openCallback,
    unsigned numVols, unsigned volIndex, UInt64 &complexity)
{
  if (h.descriptorSize != 0)
  {
    if (h.descriptorOffset == 0 ||
        h.descriptorSize > (1 << 10))
      return S_FALSE;
    DescriptorBuf.Alloc((size_t)h.descriptorSize << 9);
    RINOK(ReadForHeader(stream, h.descriptorOffset, DescriptorBuf, (size_t)h.descriptorSize))
    if (h.descriptorOffset == 1 && h.Is_Marker() && Get64(DescriptorBuf) == 0)
    {
      // We check data as end marker.
      // and if probably it's footer's copy of header, we don't want to open it.
      return S_FALSE;
    }

    DescriptorOK = Descriptor.Parse(DescriptorBuf, DescriptorBuf.Size());
    if (!DescriptorOK)
      HeadersError = true;
    if (Descriptor.IsThere_Parent())
      Unsupported = true;
  }

  if (h.gdOffset == (UInt64)(Int64)-1)
  {
    // Grain Dir is at end of file
    UInt64 endPos;
    RINOK(InStream_GetSize_SeekToEnd(stream, endPos))
    if ((endPos & 511) != 0)
      return S_FALSE;

    const size_t kEndSize = 512 * 3;
    Byte buf2[kEndSize];
    if (endPos < kEndSize)
      return S_FALSE;
    RINOK(InStream_SeekSet(stream, endPos - kEndSize))
    RINOK(ReadStream_FALSE(stream, buf2, kEndSize))
    
    CHeader h2;
    if (!h2.Parse(buf2 + 512))
      return S_FALSE;
    if (!h.IsSameImageFor(h2))
      return S_FALSE;

    h = h2;

    CMarker m;
    m.Parse(buf2);
    if (m.NumSectors != 1 || m.SpecSize != 0 || m.Type != k_Marker_FOOTER)
      return S_FALSE;
    m.Parse(buf2 + 512 * 2);
    if (m.NumSectors != 0 || m.SpecSize != 0 || m.Type != k_Marker_END_OF_STREAM)
      return S_FALSE;
    PhySize = endPos;
  }

  const int grainSize_Log = GetLog(h.grainSize);
  if (grainSize_Log < 3 || grainSize_Log > 30 - 9) // grain size must be >= 4 KB
    return S_FALSE;
  if (h.capacity >= ((UInt64)1 << (63 - 9)))
    return S_FALSE;
  if (h.overHead >= ((UInt64)1 << (63 - 9)))
    return S_FALSE;

  IsArc = true;
  ClusterBits = (9 + (unsigned)grainSize_Log);
  VirtSize = h.capacity << 9;
  NeedDeflate = (h.algo >= 1);

  if (h.Is_Compressed() ? (h.algo > 1 || !h.Is_Marker()) : (h.algo != 0))
  {
    Unsupported = true;
    PhySize = 0;
    return S_FALSE;
  }

  {
    const UInt64 overHeadBytes = h.overHead << 9;
    if (PhySize < overHeadBytes)
      PhySize = overHeadBytes;
  }

  ZeroSector = 0;
  if (h.Is_ZeroGrain())
    ZeroSector = 1;

  const UInt64 numSectorsPerGde = (UInt64)1 << ((unsigned)grainSize_Log + k_NumMidBits);
  const UInt64 numGdeEntries = (h.capacity + numSectorsPerGde - 1) >> ((unsigned)grainSize_Log + k_NumMidBits);
  CByteBuffer table;
  
  if (numGdeEntries != 0)
  {
    if (h.gdOffset == 0)
      return S_FALSE;

    size_t numSectors = (size_t)((numGdeEntries + ((1 << (9 - 2)) - 1)) >> (9 - 2));
    size_t t1SizeBytes = numSectors << 9;
    if ((t1SizeBytes >> 2) < numGdeEntries)
      return S_FALSE;
    table.Alloc(t1SizeBytes);

    if (h.Is_Marker())
    {
      Byte buf2[1 << 9];
      if (ReadForHeader(stream, h.gdOffset - 1, buf2, 1) != S_OK)
        return S_FALSE;
      {
        CMarker m;
        m.Parse(buf2);
        if (m.Type != k_Marker_GRAIN_DIR
            || m.NumSectors != numSectors
            || m.SpecSize != 0)
          return S_FALSE;
      }
    }

    RINOK(ReadForHeader(stream, h.gdOffset, table, numSectors))
  }

  const size_t clusterSize = (size_t)1 << ClusterBits;

  const UInt64 complexityStart = complexity;

  if (openCallback)
  {
    complexity += (UInt64)numGdeEntries << (k_NumMidBits + 2);
    {
      const UInt64 numVols2 = numVols;
      RINOK(openCallback->SetTotal((numVols == 1) ? NULL : &numVols2, &complexity))
    }
    if (numVols != 1)
    {
      const UInt64 volIndex2 = volIndex;
      RINOK(openCallback->SetCompleted(numVols == 1 ? NULL : &volIndex2, &complexityStart))
    }
  }

  UInt64 lastSector = 0;
  UInt64 lastVirtCluster = 0;
  size_t numProcessed_Prev = 0;

  for (size_t i = 0; i < numGdeEntries; i++)
  {
    const size_t k_NumSectors = (size_t)1 << (k_NumMidBits - 9 + 2);
    const size_t k_NumMidItems = (size_t)1 << k_NumMidBits;

    CByteBuffer &buf = Tables.AddNew();

    {
      const UInt32 v = Get32((const Byte *)table + (size_t)i * 4);
      if (v == 0 || v == ZeroSector)
        continue;
      if (openCallback && (i - numProcessed_Prev) >= 1024)
      {
        const UInt64 comp = complexityStart + ((UInt64)i << (k_NumMidBits + 2));
        const UInt64 volIndex2 = volIndex;
        RINOK(openCallback->SetCompleted(numVols == 1 ? NULL : &volIndex2, &comp))
        numProcessed_Prev = i;
      }
      
      if (h.Is_Marker())
      {
        Byte buf2[1 << 9];
        if (ReadForHeader(stream, v - 1, buf2, 1) != S_OK)
          return S_FALSE;
        {
          CMarker m;
          m.Parse(buf2);
          if (m.Type != k_Marker_GRAIN_TABLE
            || m.NumSectors != k_NumSectors
            || m.SpecSize != 0)
            return S_FALSE;
        }
      }
      
      buf.Alloc(k_NumMidItems * 4);
      RINOK(ReadForHeader(stream, v, buf, k_NumSectors))
    }

    for (size_t k = 0; k < k_NumMidItems; k++)
    {
      const UInt32 v = Get32((const Byte *)buf + (size_t)k * 4);
      if (v == 0 || v == ZeroSector)
        continue;
      if (v < h.overHead)
        return S_FALSE;
      if (lastSector < v)
      {
        lastSector = v;
        if (NeedDeflate)
          lastVirtCluster = ((UInt64)i << k_NumMidBits) + k;
      }
    }
  }

  if (!NeedDeflate)
  {
    UInt64 end = ((UInt64)lastSector << 9) + clusterSize;
    if (PhySize < end)
      PhySize = end;
  }
  else if (lastSector != 0)
  {
    Byte buf[1 << 9];
    if (ReadForHeader(stream, lastSector, buf, 1) == S_OK)
    {
      UInt64 lba = Get64(buf);
      if (lba == (lastVirtCluster << (ClusterBits - 9)))
      {
        UInt32 dataSize = Get32(buf + 8);
        size_t dataSize2 = (size_t)dataSize + 12;
        dataSize2 = (dataSize2 + 511) & ~(size_t)511;
        UInt64 end = ((UInt64)lastSector << 9) + dataSize2;
        if (PhySize < end)
          PhySize = end;
      }
    }
  }

  return S_OK;
}


Z7_COM7F_IMF(CHandler::Close())
{
  _phySize = 0;
  
  _cacheCluster = (UInt64)(Int64)-1;
  _cacheExtent = (unsigned)(int)-1;

  _clusterBitsMax = 0;

  _isArc = false;
  _unsupported = false;
  _unsupportedSome = false;
  _headerError = false;
  _missingVol = false;
  _isMultiVol = false;
  _needDeflate = false;

  _missingVolName.Empty();

  _descriptorBuf.Free();
  _descriptor.Clear();

  // CHandlerImg:
  Clear_HandlerImg_Vars();
  Stream.Release();

  _extents.Clear();
  return S_OK;
}


Z7_COM7F_IMF(CHandler::GetStream(UInt32 /* index */, ISequentialInStream **stream))
{
  COM_TRY_BEGIN
  *stream = NULL;

  if (_unsupported)
    return S_FALSE;

  ClearStreamVars();
  // _stream_UsePackSize = true;

  if (_needDeflate)
  {
    if (!_bufInStream)
    {
      _bufInStreamSpec = new CBufInStream;
      _bufInStream = _bufInStreamSpec;
    }
    
    if (!_bufOutStream)
    {
      _bufOutStreamSpec = new CBufPtrSeqOutStream();
      _bufOutStream = _bufOutStreamSpec;
    }

    if (!_zlibDecoder)
    {
      _zlibDecoderSpec = new NCompress::NZlib::CDecoder;
      _zlibDecoder = _zlibDecoderSpec;
    }
    
    const size_t clusterSize = (size_t)1 << _clusterBitsMax;
    _cache.AllocAtLeast(clusterSize);
    _cacheCompressed.AllocAtLeast(clusterSize * 2);
  }

  FOR_VECTOR (i, _extents)
  {
    RINOK(_extents[i].InitAndSeek())
  }

  CMyComPtr<ISequentialInStream> streamTemp = this;
  InitAndSeekMain();
  *stream = streamTemp.Detach();
  return S_OK;
  COM_TRY_END
}


REGISTER_ARC_I(
  "VMDK", "vmdk", NULL, 0xC8,
  k_Signature,
  0,
  0,
  NULL)

}}

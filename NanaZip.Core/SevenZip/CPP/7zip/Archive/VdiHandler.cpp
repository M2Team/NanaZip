// VdiHandler.cpp

#include "StdAfx.h"

// #include <stdio.h>

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/MyBuffer.h"

#include "../../Windows/PropVariant.h"
#include "../../Windows/PropVariantUtils.h"

#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "HandlerCont.h"

#define Get32(p) GetUi32(p)
#define Get64(p) GetUi64(p)

using namespace NWindows;

namespace NArchive {
namespace NVdi {

static const Byte k_Signature[] = { 0x7F, 0x10, 0xDA, 0xBE };

static const unsigned k_ClusterBits = 20;
static const UInt32 k_ClusterSize = (UInt32)1 << k_ClusterBits;


/*
VDI_IMAGE_BLOCK_FREE = (~0) // returns any random data
VDI_IMAGE_BLOCK_ZERO = (~1) // returns zeros
*/

// static const UInt32 k_ClusterType_Free  = 0xffffffff;
static const UInt32 k_ClusterType_Zero  = 0xfffffffe;

#define IS_CLUSTER_ALLOCATED(v) ((UInt32)(v) < k_ClusterType_Zero)


// static const UInt32 kDiskType_Dynamic = 1;
// static const UInt32 kDiskType_Static = 2;

static const char * const kDiskTypes[] =
{
    "0"
  , "Dynamic"
  , "Static"
  , "Undo"
  , "Diff"
};


enum EGuidType
{
  k_GuidType_Creat,
  k_GuidType_Modif,
  k_GuidType_Link,
  k_GuidType_PModif
};

static const unsigned kNumGuids = 4;
static const char * const kGuidNames[kNumGuids] =
{
    "Creat "
  , "Modif "
  , "Link  "
  , "PModif"
};

static bool IsEmptyGuid(const Byte *data)
{
  for (unsigned i = 0; i < 16; i++)
    if (data[i] != 0)
      return false;
  return true;
}



Z7_class_CHandler_final: public CHandlerImg
{
  UInt32 _dataOffset;
  CByteBuffer _table;
  UInt64 _phySize;
  UInt32 _imageType;
  bool _isArc;
  bool _unsupported;

  Byte Guids[kNumGuids][16];

  HRESULT Seek2(UInt64 offset)
  {
    _posInArc = offset;
    return InStream_SeekSet(Stream, offset);
  }

  HRESULT InitAndSeek()
  {
    _virtPos = 0;
    return Seek2(0);
  }

  HRESULT Open2(IInStream *stream, IArchiveOpenCallback *openCallback) Z7_override;

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
 
  {
    UInt64 cluster = _virtPos >> k_ClusterBits;
    UInt32 lowBits = (UInt32)_virtPos & (k_ClusterSize - 1);
    {
      UInt32 rem = k_ClusterSize - lowBits;
      if (size > rem)
        size = rem;
    }

    cluster <<= 2;
    if (cluster < _table.Size())
    {
      const Byte *p = (const Byte *)_table + (size_t)cluster;
      const UInt32 v = Get32(p);
      if (IS_CLUSTER_ALLOCATED(v))
      {
        UInt64 offset = _dataOffset + ((UInt64)v << k_ClusterBits);
        offset += lowBits;
        if (offset != _posInArc)
        {
          RINOK(Seek2(offset))
        }
        HRESULT res = Stream->Read(data, size, &size);
        _posInArc += size;
        _virtPos += size;
        if (processedSize)
          *processedSize = size;
        return res;
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
  kpidHeadersSize,
  kpidMethod,
  kpidComment,
  kpidName
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;

  switch (propID)
  {
    case kpidMainSubfile: prop = (UInt32)0; break;
    case kpidPhySize: if (_phySize != 0) prop = _phySize; break;
    case kpidHeadersSize: prop = _dataOffset; break;
    
    case kpidMethod:
    {
      TYPE_TO_PROP(kDiskTypes, _imageType, prop);
      break;
    }

    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc) v |= kpv_ErrorFlags_IsNotArc;
      if (_unsupported) v |= kpv_ErrorFlags_UnsupportedMethod;
      // if (_headerError) v |= kpv_ErrorFlags_HeadersError;
      if (!Stream && v == 0 && _isArc)
        v = kpv_ErrorFlags_HeadersError;
      if (v != 0)
        prop = v;
      break;
    }

    case kpidComment:
    {
      AString s;
      for (unsigned i = 0; i < kNumGuids; i++)
      {
        const Byte *guid = Guids[i];
        if (!IsEmptyGuid(guid))
        {
          s.Add_LF();
          s += kGuidNames[i];
          s += " : ";
          char temp[64];
          RawLeGuidToString_Braced(guid, temp);
          MyStringLower_Ascii(temp);
          s += temp;
        }
      }
      if (!s.IsEmpty())
        prop = s;
      break;
    }

    case kpidName:
    {
      const Byte *guid = Guids[k_GuidType_Creat];
      if (!IsEmptyGuid(guid))
      {
        char temp[64];
        RawLeGuidToString_Braced(guid, temp);
        MyStringLower_Ascii(temp);
        MyStringCat(temp, ".vdi");
        prop = temp;
      }
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
    case kpidPackSize: prop = _phySize - _dataOffset; break;
    case kpidExtension: prop = (_imgExt ? _imgExt : "img"); break;
  }

  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


HRESULT CHandler::Open2(IInStream *stream, IArchiveOpenCallback * /* openCallback */)
{
  const unsigned kHeaderSize = 512;
  Byte buf[kHeaderSize];
  RINOK(ReadStream_FALSE(stream, buf, kHeaderSize))

  if (memcmp(buf + 0x40, k_Signature, sizeof(k_Signature)) != 0)
    return S_FALSE;

  const UInt32 version = Get32(buf + 0x44);
  if (version >= 0x20000)
    return S_FALSE;
  if (version < 0x10000)
  {
    _unsupported = true;
    return S_FALSE;
  }
  
  const unsigned kHeaderOffset = 0x48;
  const unsigned kGuidsOffsets = 0x188;
  const UInt32 headerSize = Get32(buf + kHeaderOffset);
  if (headerSize < kGuidsOffsets - kHeaderOffset || headerSize > 0x200 - kHeaderOffset)
    return S_FALSE;

  _imageType = Get32(buf + 0x4C);
  // Int32 flags = Get32(buf + 0x50);
  // Byte Comment[0x100]

  const UInt32 tableOffset = Get32(buf + 0x154);
  if (tableOffset < 0x200)
    return S_FALSE;

  _dataOffset = Get32(buf + 0x158);

  // UInt32 geometry[3];
  
  const UInt32 sectorSize = Get32(buf + 0x168);
  if (sectorSize != 0x200)
    return S_FALSE;

  _size = Get64(buf + 0x170);
  const UInt32 blockSize = Get32(buf + 0x178);
  const UInt32 totalBlocks = Get32(buf + 0x180);
  const UInt32 numAllocatedBlocks = Get32(buf + 0x184);
  
  _isArc = true;

  if (_dataOffset < tableOffset)
    return S_FALSE;

  if (_imageType > 4)
    _unsupported = true;

  if (blockSize != k_ClusterSize)
  {
    _unsupported = true;
    return S_FALSE;
  }

  if (headerSize >= kGuidsOffsets + kNumGuids * 16 - kHeaderOffset)
  {
    for (unsigned i = 0; i < kNumGuids; i++)
      memcpy(Guids[i], buf + kGuidsOffsets + 16 * i, 16);

    if (!IsEmptyGuid(Guids[k_GuidType_Link]) ||
        !IsEmptyGuid(Guids[k_GuidType_PModif]))
      _unsupported = true;
  }

  {
    UInt64 size2 = (UInt64)totalBlocks << k_ClusterBits;
    if (size2 < _size)
    {
      _unsupported = true;
      return S_FALSE;
    }
    /*
    if (size2 > _size)
      _size = size2;
    */
  }

  {
    UInt32 tableReserved = _dataOffset - tableOffset;
    if ((tableReserved >> 2) < totalBlocks)
      return S_FALSE;
  }

  _phySize = _dataOffset + ((UInt64)numAllocatedBlocks << k_ClusterBits);

  const size_t numBytes = (size_t)totalBlocks * 4;
  if ((numBytes >> 2) != totalBlocks)
  {
    _unsupported = true;
    return E_OUTOFMEMORY;
  }

  _table.Alloc(numBytes);
  RINOK(InStream_SeekSet(stream, tableOffset))
  RINOK(ReadStream_FALSE(stream, _table, numBytes))
    
  const Byte *data = _table;
  for (UInt32 i = 0; i < totalBlocks; i++)
  {
    const UInt32 v = Get32(data + (size_t)i * 4);
    if (!IS_CLUSTER_ALLOCATED(v))
      continue;
    if (v >= numAllocatedBlocks)
    {
      _unsupported = true;
      return S_FALSE;
    }
  }
  
  Stream = stream;
  return S_OK;
}


Z7_COM7F_IMF(CHandler::Close())
{
  _table.Free();
  _phySize = 0;
  _isArc = false;
  _unsupported = false;

  for (unsigned i = 0; i < kNumGuids; i++)
    memset(Guids[i], 0, 16);

  // CHandlerImg:
  Clear_HandlerImg_Vars();
  Stream.Release();
  return S_OK;
}


Z7_COM7F_IMF(CHandler::GetStream(UInt32 /* index */, ISequentialInStream **stream))
{
  COM_TRY_BEGIN
  *stream = NULL;
  if (_unsupported)
    return S_FALSE;
  CMyComPtr<ISequentialInStream> streamTemp = this;
  RINOK(InitAndSeek())
  *stream = streamTemp.Detach();
  return S_OK;
  COM_TRY_END
}


REGISTER_ARC_I(
  "VDI", "vdi", NULL, 0xC9,
  k_Signature,
  0x40,
  0,
  NULL)

}}

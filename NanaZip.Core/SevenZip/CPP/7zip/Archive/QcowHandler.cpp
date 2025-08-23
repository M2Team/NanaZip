// QcowHandler.cpp

#include "StdAfx.h"

// #include <stdio.h>

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/MyBuffer2.h"

#include "../../Windows/PropVariant.h"
#include "../../Windows/PropVariantUtils.h"

#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#include "../Compress/DeflateDecoder.h"

#include "HandlerCont.h"

#define Get32(p) GetBe32a(p)
#define Get64(p) GetBe64a(p)

using namespace NWindows;

namespace NArchive {
namespace NQcow {

static const Byte k_Signature[] =  { 'Q', 'F', 'I', 0xFB, 0, 0, 0 };

/*
VA to PA maps:
  high bits (L1) :              : index in L1 (_dir) : _dir[high_index] points to Table.
  mid bits  (L2) : _numMidBits  : index in Table, Table[index] points to cluster start offset in arc file.
  low bits       : _clusterBits : offset inside cluster.
*/

Z7_class_CHandler_final: public CHandlerImg
{
  Z7_IFACE_COM7_IMP(IInArchive_Img)
  Z7_IFACE_COM7_IMP(IInArchiveGetStream)
  Z7_IFACE_COM7_IMP(ISequentialInStream)

  unsigned _clusterBits;
  unsigned _numMidBits;
  UInt64 _compressedFlag;

  CObjArray2<UInt32> _dir;
  CAlignedBuffer _table;
  CByteBuffer _cache;
  CByteBuffer _cacheCompressed;
  UInt64 _cacheCluster;

  UInt64 _comprPos;
  size_t _comprSize;

  bool _needCompression;
  bool _isArc;
  bool _unsupported;
  Byte _compressionType;

  UInt64 _phySize;

  CMyComPtr2<ISequentialInStream, CBufInStream> _bufInStream;
  CMyComPtr2<ISequentialOutStream, CBufPtrSeqOutStream> _bufOutStream;
  CMyComPtr2<ICompressCoder, NCompress::NDeflate::NDecoder::CCOMCoder> _deflateDecoder;

  UInt32 _version;
  UInt32 _cryptMethod;
  UInt64 _incompatFlags;
  
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
};


static const UInt32 kEmptyDirItem = (UInt32)0 - 1;

Z7_COM7F_IMF(CHandler::Read(void *data, UInt32 size, UInt32 *processedSize))
{
  if (processedSize)
    *processedSize = 0;
  // printf("\nRead _virtPos = %6d  size = %6d\n", (UInt32)_virtPos, size);
  if (_virtPos >= _size)
    return S_OK;
  {
    const UInt64 rem = _size - _virtPos;
    if (size > rem)
      size = (UInt32)rem;
    if (size == 0)
      return S_OK;
  }
 
  for (;;)
  {
    const UInt64 cluster = _virtPos >> _clusterBits;
    const size_t clusterSize = (size_t)1 << _clusterBits;
    const size_t lowBits = (size_t)_virtPos & (clusterSize - 1);
    {
      const size_t rem = clusterSize - lowBits;
      if (size > rem)
        size = (UInt32)rem;
    }
    if (cluster == _cacheCluster)
    {
      memcpy(data, _cache + lowBits, size);
      break;
    }
   
    const UInt64 high = cluster >> _numMidBits;
 
    if (high < _dir.Size())
    {
      const UInt32 tabl = _dir[(size_t)high];
      if (tabl != kEmptyDirItem)
      {
        const size_t midBits = (size_t)cluster & (((size_t)1 << _numMidBits) - 1);
        const Byte *p = _table + ((((size_t)tabl << _numMidBits) + midBits) << 3);
        UInt64 v = Get64(p);
        
        if (v)
        {
          if (v & _compressedFlag)
          {
            if (_version <= 1)
              return E_FAIL;
            /*
            the example of table record for 12-bit clusters (4KB uncompressed):
              2 bits : isCompressed status
              (4 == _clusterBits - 8) bits : (num_sectors - 1)
                  packSize = num_sectors * 512;
                  it uses one additional bit over unpacked cluster_bits.
              (49 == 61 - _clusterBits) bits : offset of 512-byte sector
              9 bits : offset in 512-byte sector
            */
            const unsigned numOffsetBits = 62 - (_clusterBits - 8);
            const UInt64 offset = v & (((UInt64)1 << 62) - 1);
            const size_t dataSize = ((size_t)(offset >> numOffsetBits) + 1) << 9;
            UInt64 sectorOffset = offset & (((UInt64)1 << numOffsetBits) - (1 << 9));
            const UInt64 offset2inCache = sectorOffset - _comprPos;
            
            // _comprPos is aligned for 512-bytes
            // we try to use previous _cacheCompressed that contains compressed data
            // that was read for previous unpacking

            if (sectorOffset >= _comprPos && offset2inCache < _comprSize)
            {
              if (offset2inCache)
              {
                _comprSize -= (size_t)offset2inCache;
                memmove(_cacheCompressed, _cacheCompressed + (size_t)offset2inCache, _comprSize);
                _comprPos = sectorOffset;
              }
              sectorOffset += _comprSize;
            }
            else
            {
              _comprPos = sectorOffset;
              _comprSize = 0;
            }
            
            if (dataSize > _comprSize)
            {
              if (sectorOffset != _posInArc)
              {
                // printf("\nDeflate-Seek %12I64x %12I64x\n", sectorOffset, sectorOffset - _posInArc);
                RINOK(Seek2(sectorOffset))
              }
              if (_cacheCompressed.Size() < dataSize)
                return E_FAIL;
              const size_t dataSize3 = dataSize - _comprSize;
              size_t dataSize2 = dataSize3;
              // printf("\n\n=======\nReadStream = %6d _comprPos = %6d \n", (UInt32)dataSize2, (UInt32)_comprPos);
              const HRESULT hres = ReadStream(Stream, _cacheCompressed + _comprSize, &dataSize2);
              _posInArc += dataSize2;
              RINOK(hres)
              if (dataSize2 != dataSize3)
                return E_FAIL;
              _comprSize += dataSize2;
            }
            
            const size_t kSectorMask = (1 << 9) - 1;
            const size_t offsetInSector = (size_t)offset & kSectorMask;
            _bufInStream->Init(_cacheCompressed + offsetInSector, dataSize - offsetInSector);
            _cacheCluster = (UInt64)(Int64)-1;
            if (_cache.Size() < clusterSize)
              return E_FAIL;
            _bufOutStream->Init(_cache, clusterSize);
            // Do we need to use smaller block than clusterSize for last cluster?
            const UInt64 blockSize64 = clusterSize;
            HRESULT res = _deflateDecoder.Interface()->Code(_bufInStream, _bufOutStream, NULL, &blockSize64, NULL);
            /*
            if (_bufOutStreamSpec->GetPos() != clusterSize)
              memset(_cache + _bufOutStreamSpec->GetPos(), 0, clusterSize - _bufOutStreamSpec->GetPos());
            */
            if (res == S_OK)
              if (!_deflateDecoder->IsFinished()
                  || _bufOutStream->GetPos() != clusterSize)
                res = S_FALSE;
            RINOK(res)
            _cacheCluster = cluster;
            continue;
            /*
            memcpy(data, _cache + lowBits, size);
            break;
            */
          }

          // version_3 supports zero clusters
          if (((UInt32)v & 511) != 1)
          {
            v &= _compressedFlag - 1;
            v += lowBits;
            if (v != _posInArc)
            {
              // printf("\n%12I64x\n", v - _posInArc);
              RINOK(Seek2(v))
            }
            const HRESULT res = Stream->Read(data, size, &size);
            _posInArc += size;
            _virtPos += size;
            if (processedSize)
              *processedSize = size;
            return res;
          }
        }
      }
    }
    
    memset(data, 0, size);
    break;
  }

  _virtPos += size;
  if (processedSize)
    *processedSize = size;
  return S_OK;
}


static const Byte kProps[] =
{
  kpidSize,
  kpidPackSize
};

static const Byte kArcProps[] =
{
  kpidClusterSize,
  kpidSectorSize, // actually we need variable to show table size
  kpidHeadersSize,
  kpidUnpackVer,
  kpidMethod,
  kpidCharacts
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

static const CUInt32PCharPair g_IncompatFlags_Characts[] =
{
  {  0, "Dirty" },
  {  1, "Corrupt" },
  {  2, "External_Data_File" },
  {  3, "Compression" },
  {  4, "Extended_L2" }
};

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;

  switch (propID)
  {
    case kpidMainSubfile: prop = (UInt32)0; break;
    case kpidClusterSize: prop = (UInt32)1 << _clusterBits; break;
    case kpidSectorSize: prop = (UInt32)1 << (_numMidBits + 3); break;
    case kpidHeadersSize: prop = _table.Size() + (UInt64)_dir.Size() * 8; break;
    case kpidPhySize: if (_phySize) prop = _phySize; break;
    case kpidUnpackVer: prop = _version; break;
    case kpidCharacts:
    {
      if (_incompatFlags)
      {
        AString s ("incompatible: ");
        // we need to show also high 32-bits.
        s += FlagsToString(g_IncompatFlags_Characts,
            Z7_ARRAY_SIZE(g_IncompatFlags_Characts), (UInt32)_incompatFlags);
        prop = s;
      }
      break;
    }
    case kpidMethod:
    {
      AString s;

      if (_compressionType)
      {
        if (_compressionType == 1)
          s += "ZSTD";
        else
        {
          s += "Compression:";
          s.Add_UInt32(_compressionType);
        }
      }
      else if (_needCompression)
        s.Add_OptSpaced("Deflate");

      if (_cryptMethod)
      {
        s.Add_Space_if_NotEmpty();
        if (_cryptMethod == 1)
          s += "AES";
        if (_cryptMethod == 2)
          s += "LUKS";
        else
        {
          s += "Encryption:";
          s.Add_UInt32(_cryptMethod);
        }
      }
      if (!s.IsEmpty())
        prop = s;
      break;
    }

    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc) v |= kpv_ErrorFlags_IsNotArc;
      if (_unsupported) v |= kpv_ErrorFlags_UnsupportedMethod;
      // if (_headerError) v |= kpv_ErrorFlags_HeadersError;
      if (!Stream && v == 0)
        v = kpv_ErrorFlags_HeadersError;
      if (v)
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
    case kpidPackSize: prop = _phySize; break;
    case kpidExtension: prop = (_imgExt ? _imgExt : "img"); break;
  }
  
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


HRESULT CHandler::Open2(IInStream *stream, IArchiveOpenCallback *openCallback)
{
  UInt64 buf64[0x70 / 8];
  RINOK(ReadStream_FALSE(stream, buf64, sizeof(buf64)))
  const void *buf = (const void *)buf64;
  // signature: { 'Q', 'F', 'I', 0xFB }
  if (*(const UInt32 *)buf != Z7_CONV_BE_TO_NATIVE_CONST32(0x514649fb))
    return S_FALSE;
  _version = Get32((const Byte *)(const void *)buf64 + 4);
  if (_version < 1 || _version > 3)
    return S_FALSE;
  
  const UInt64 k_UncompressedSize_MAX = (UInt64)1 << 60;
  const UInt64 k_CompressedSize_MAX   = (UInt64)1 << 60;

  _size = Get64((const Byte *)(const void *)buf64 + 0x18);
  if (_size > k_UncompressedSize_MAX)
    return S_FALSE;
  size_t l1Size;
  UInt32 headerSize;

  if (_version == 1)
  {
    // _mTime = Get32((const Byte *)(const void *)buf64 + 0x14); // is unused in most images
    _clusterBits = ((const Byte *)(const void *)buf64)[0x20];
    _numMidBits  = ((const Byte *)(const void *)buf64)[0x21];
    if (_clusterBits < 9 || _clusterBits > 30)
      return S_FALSE;
    if (_numMidBits < 1 || _numMidBits > 28)
      return S_FALSE;
    _cryptMethod = Get32((const Byte *)(const void *)buf64 + 0x24);
    const unsigned numBits2 = _clusterBits + _numMidBits;
    const UInt64 l1Size64 = (_size + (((UInt64)1 << numBits2) - 1)) >> numBits2;
    if (l1Size64 > ((UInt32)1 << 31))
      return S_FALSE;
    l1Size = (size_t)l1Size64;
    headerSize = 0x30;
  }
  else
  {
    _clusterBits = Get32((const Byte *)(const void *)buf64 + 0x14);
    if (_clusterBits < 9 || _clusterBits > 30)
      return S_FALSE;
    _numMidBits = _clusterBits - 3;
    _cryptMethod = Get32((const Byte *)(const void *)buf64 + 0x20);
    l1Size = Get32((const Byte *)(const void *)buf64 + 0x24);
    headerSize = 0x48;
    if (_version >= 3)
    {
      _incompatFlags = Get64((const Byte *)(const void *)buf64 + 0x48);
      // const UInt64 CompatFlags    = Get64((const Byte *)(const void *)buf64 + 0x50);
      // const UInt64 AutoClearFlags = Get64((const Byte *)(const void *)buf64 + 0x58);
      // const UInt32 RefCountOrder = Get32((const Byte *)(const void *)buf64 + 0x60);
      headerSize = 0x68;
      const UInt32 headerSize2  = Get32((const Byte *)(const void *)buf64 + 0x64);
      if (headerSize2 > (1u << 30))
        return S_FALSE;
      if (headerSize < headerSize2)
          headerSize = headerSize2;
      if (headerSize2 >= 0x68 + 1)
        _compressionType = ((const Byte *)(const void *)buf64)[0x68];
    }

    const UInt64 refOffset = Get64((const Byte *)(const void *)buf64 + 0x30); // must be aligned for cluster
    const UInt32 refClusters = Get32((const Byte *)(const void *)buf64 + 0x38);
    // UInt32 numSnapshots = Get32((const Byte *)(const void *)buf64 + 0x3C);
    // UInt64 snapshotsOffset = Get64((const Byte *)(const void *)buf64 + 0x40); // must be aligned for cluster
    /*
    if (numSnapshots)
      return S_FALSE;
    */
    if (refClusters)
    {
      if (refOffset > k_CompressedSize_MAX)
        return S_FALSE;
      const UInt64 numBytes = (UInt64)refClusters << _clusterBits;
      const UInt64 end = refOffset + numBytes;
      if (end > k_CompressedSize_MAX)
        return S_FALSE;
      /*
      CByteBuffer refs;
      refs.Alloc(numBytes);
      RINOK(InStream_SeekSet(stream, refOffset))
      RINOK(ReadStream_FALSE(stream, refs, numBytes));
      */
      if (_phySize < end)
          _phySize = end;
      /*
      for (size_t i = 0; i < numBytes; i += 2)
      {
        UInt32 v = GetBe16((const Byte *)refs + (size_t)i);
        if (v == 0)
          continue;
      }
      */
    }
  }

  const UInt64 l1Offset = Get64((const Byte *)(const void *)buf64 + 0x28); // must be aligned for cluster ?
  if (l1Offset < headerSize || l1Offset > k_CompressedSize_MAX)
    return S_FALSE;
  if (_phySize < headerSize)
      _phySize = headerSize;

  _isArc = true;
  {
    const UInt64 backOffset = Get64((const Byte *)(const void *)buf64 + 8);
    // UInt32 backSize = Get32((const Byte *)(const void *)buf64 + 0x10);
    if (backOffset)
    {
      _unsupported = true;
      return S_FALSE;
    }
  }

  UInt64 fileSize = 0;
  RINOK(InStream_GetSize_SeekToBegin(stream, fileSize))

  const size_t clusterSize = (size_t)1 << _clusterBits;
  const size_t t1SizeBytes = (size_t)l1Size << 3;
  {
    const UInt64 end = l1Offset + t1SizeBytes;
    if (end > k_CompressedSize_MAX)
      return S_FALSE;
    // we need to use align end for empty qcow files
    // some files has no cluster alignment padding at the end
    // but has sector alignment
    // end = (end + clusterSize - 1) >> _clusterBits << _clusterBits;
    if (_phySize < end)
        _phySize = end;
    if (end > fileSize)
      return S_FALSE;
    if (_phySize < fileSize)
    {
      const UInt64 end2 = (end + 511) & ~(UInt64)511;
      if (end2 == fileSize)
        _phySize = end2;
    }
  }
  CObjArray<UInt64> table64(l1Size);
  {
    // if ((t1SizeBytes >> 3) != l1Size) return S_FALSE;
    RINOK(InStream_SeekSet(stream, l1Offset))
    RINOK(ReadStream_FALSE(stream, table64, t1SizeBytes))
  }

  _compressedFlag = (_version <= 1) ? ((UInt64)1 << 63) : ((UInt64)1 << 62);
  const UInt64 offsetMask = _compressedFlag - 1;
  const size_t midSize = (size_t)1 << (_numMidBits + 3);
  size_t numTables = 0;
  size_t i;

  for (i = 0; i < l1Size; i++)
  {
    const UInt64 v = Get64(table64 + (size_t)i) & offsetMask;
    if (!v)
      continue;
    numTables++;
    const UInt64 end = v + midSize;
    if (end > k_CompressedSize_MAX)
      return S_FALSE;
    if (_phySize < end)
        _phySize = end;
    if (end > fileSize)
      return S_FALSE;
  }

  if (numTables)
  {
    const size_t size = (size_t)numTables << (_numMidBits + 3);
    if (size >> (_numMidBits + 3) != numTables)
      return E_OUTOFMEMORY;
    _table.Alloc(size);
    if (!_table.IsAllocated())
      return E_OUTOFMEMORY;
    if (openCallback)
    {
      const UInt64 totalBytes = size;
      RINOK(openCallback->SetTotal(NULL, &totalBytes))
    }
  }

  _dir.SetSize((unsigned)l1Size);

  UInt32 curTable = 0;

  for (i = 0; i < l1Size; i++)
  {
    Byte *buf2;
    {
      const UInt64 v = Get64(table64 + (size_t)i) & offsetMask;
      if (v == 0)
      {
        _dir[i] = kEmptyDirItem;
        continue;
      }
      _dir[i] = curTable;
      const size_t tableOffset = (size_t)curTable << (_numMidBits + 3);
      buf2 = (Byte *)_table + tableOffset;
      curTable++;
      if (openCallback && (tableOffset & 0xFFFFF) == 0)
      {
        const UInt64 numBytes = tableOffset;
        RINOK(openCallback->SetCompleted(NULL, &numBytes))
      }
      RINOK(InStream_SeekSet(stream, v))
      RINOK(ReadStream_FALSE(stream, buf2, midSize))
    }

    for (size_t k = 0; k < midSize; k += 8)
    {
      const UInt64 v = Get64((const Byte *)buf2 + (size_t)k);
      if (v == 0)
        continue;
      UInt64 offset = v & offsetMask;
      size_t dataSize = clusterSize;
      
      if (v & _compressedFlag)
      {
        if (_version <= 1)
        {
          const unsigned numOffsetBits = 63 - _clusterBits;
          dataSize = ((size_t)(offset >> numOffsetBits) + 1) << 9;
          offset &= ((UInt64)1 << numOffsetBits) - 1;
          dataSize = 0; // why ?
          // offset &= ~(((UInt64)1 << 9) - 1);
        }
        else
        {
          const unsigned numOffsetBits = 62 - (_clusterBits - 8);
          dataSize = ((size_t)(offset >> numOffsetBits) + 1) << 9;
          offset &= ((UInt64)1 << numOffsetBits) - (1 << 9);
        }
        _needCompression = true;
      }
      else
      {
        const UInt32 low = (UInt32)v & 511;
        if (low)
        {
          // version_3 supports zero clusters
          if (_version < 3 || low != 1)
          {
            _unsupported = true;
            return S_FALSE;
          }
        }
      }
      
      const UInt64 end = offset + dataSize;
      if (_phySize < end)
          _phySize = end;
    }
  }

  if (curTable != numTables)
    return E_FAIL;

  if (_cryptMethod)
    _unsupported = true;
  if (_needCompression && _version <= 1) // that case was not implemented
    _unsupported = true;
  if (_compressionType)
    _unsupported = true;

  Stream = stream;
  return S_OK;
}


Z7_COM7F_IMF(CHandler::Close())
{
  _table.Free();
  _dir.Free();
  // _cache.Free();
  // _cacheCompressed.Free();
  _phySize = 0;

  _cacheCluster = (UInt64)(Int64)-1;
  _comprPos = 0;
  _comprSize = 0;

  _needCompression = false;
  _isArc = false;
  _unsupported = false;

  _compressionType = 0;
  _incompatFlags = 0;

  // CHandlerImg:
  Clear_HandlerImg_Vars();
  Stream.Release();
  return S_OK;
}


Z7_COM7F_IMF(CHandler::GetStream(UInt32 /* index */, ISequentialInStream **stream))
{
  COM_TRY_BEGIN
  *stream = NULL;
  if (_unsupported || !Stream)
    return S_FALSE;
  if (_needCompression)
  {
    if (_version <= 1 || _compressionType)
      return S_FALSE;
    _bufInStream.Create_if_Empty();
    _bufOutStream.Create_if_Empty();
    _deflateDecoder.Create_if_Empty();
    _deflateDecoder->Set_NeedFinishInput(true);
    const size_t clusterSize = (size_t)1 << _clusterBits;
    _cache.AllocAtLeast(clusterSize);
    _cacheCompressed.AllocAtLeast(clusterSize * 2);
  }
  CMyComPtr<ISequentialInStream> streamTemp = this;
  RINOK(InitAndSeek())
  *stream = streamTemp.Detach();
  return S_OK;
  COM_TRY_END
}


REGISTER_ARC_I(
  "QCOW", "qcow qcow2 qcow2c", NULL, 0xCA,
  k_Signature,
  0,
  0,
  NULL)

}}

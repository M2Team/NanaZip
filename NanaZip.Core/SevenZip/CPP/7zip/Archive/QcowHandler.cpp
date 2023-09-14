// QcowHandler.cpp

#include "StdAfx.h"

// #include <stdio.h>

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/MyBuffer2.h"

#include "../../Windows/PropVariant.h"

#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#include "../Compress/DeflateDecoder.h"

#include "HandlerCont.h"

#define Get32(p) GetBe32(p)
#define Get64(p) GetBe64(p)

using namespace NWindows;

namespace NArchive {
namespace NQcow {

static const Byte k_Signature[] =  { 'Q', 'F', 'I', 0xFB, 0, 0, 0 };

/*
VA to PA maps:
  high bits (L1) :              : in L1 Table : the reference to L1 Table
  mid bits  (L2) : _numMidBits  : in L2 Table : the reference to cluster
  low bits       : _clusterBits
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
  UInt64 _cacheCluster;
  CByteBuffer _cache;
  CByteBuffer _cacheCompressed;

  UInt64 _comprPos;
  size_t _comprSize;

  UInt64 _phySize;

  CBufInStream *_bufInStreamSpec;
  CMyComPtr<ISequentialInStream> _bufInStream;

  CBufPtrSeqOutStream *_bufOutStreamSpec;
  CMyComPtr<ISequentialOutStream> _bufOutStream;

  NCompress::NDeflate::NDecoder::CCOMCoder *_deflateDecoderSpec;
  CMyComPtr<ICompressCoder> _deflateDecoder;

  bool _needDeflate;
  bool _isArc;
  bool _unsupported;

  UInt32 _version;
  UInt32 _cryptMethod;
  
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
    UInt64 rem = _size - _virtPos;
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
      size_t rem = clusterSize - lowBits;
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
      const UInt32 tabl = _dir[(unsigned)high];
    
      if (tabl != kEmptyDirItem)
      {
        const Byte *buffer = _table + ((size_t)tabl << (_numMidBits + 3));
        const size_t midBits = (size_t)cluster & (((size_t)1 << _numMidBits) - 1);
        const Byte *p = (const Byte *)buffer + (midBits << 3);
        UInt64 v = Get64(p);
        
        if (v != 0)
        {
          if ((v & _compressedFlag) != 0)
          {
            if (_version <= 1)
              return E_FAIL;

            /*
            the example of table record for 12-bit clusters (4KB uncompressed).
             2 bits : isCompressed status
             4 bits : num_sectors_minus1; packSize = (num_sectors_minus1 + 1) * 512;
                      it uses one additional bit over unpacked cluster_bits
            49 bits : offset of 512-sector
             9 bits : offset in 512-sector
            */

            const unsigned numOffsetBits = (62 - (_clusterBits - 9 + 1));
            const UInt64 offset = v & (((UInt64)1 << 62) - 1);
            const size_t dataSize = ((size_t)(offset >> numOffsetBits) + 1) << 9;
            UInt64 sectorOffset = offset & (((UInt64)1 << numOffsetBits) - (1 << 9));
            const UInt64 offset2inCache = sectorOffset - _comprPos;
            
            // _comprPos is aligned for 512-bytes
            // we try to use previous _cacheCompressed that contains compressed data
            // that was read for previous unpacking

            if (sectorOffset >= _comprPos && offset2inCache < _comprSize)
            {
              if (offset2inCache != 0)
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
              RINOK(ReadStream(Stream, _cacheCompressed + _comprSize, &dataSize2))
              _posInArc += dataSize2;
              if (dataSize2 != dataSize3)
                return E_FAIL;
              _comprSize += dataSize2;
            }
            
            const size_t kSectorMask = (1 << 9) - 1;
            const size_t offsetInSector = ((size_t)offset & kSectorMask);
            _bufInStreamSpec->Init(_cacheCompressed + offsetInSector, dataSize - offsetInSector);
            
            _cacheCluster = (UInt64)(Int64)-1;
            if (_cache.Size() < clusterSize)
              return E_FAIL;
            _bufOutStreamSpec->Init(_cache, clusterSize);
            
            // Do we need to use smaller block than clusterSize for last cluster?
            const UInt64 blockSize64 = clusterSize;
            HRESULT res = _deflateDecoder->Code(_bufInStream, _bufOutStream, NULL, &blockSize64, NULL);

            /*
            if (_bufOutStreamSpec->GetPos() != clusterSize)
              memset(_cache + _bufOutStreamSpec->GetPos(), 0, clusterSize - _bufOutStreamSpec->GetPos());
            */

            if (res == S_OK)
              if (!_deflateDecoderSpec->IsFinished()
                  || _bufOutStreamSpec->GetPos() != clusterSize)
                res = S_FALSE;

            RINOK(res)
            _cacheCluster = cluster;
            
            continue;
            /*
            memcpy(data, _cache + lowBits, size);
            break;
            */
          }

          // version 3 support zero clusters
          if (((UInt32)v & 511) != 1)
          {
            v &= (_compressedFlag - 1);
            v += lowBits;
            if (v != _posInArc)
            {
              // printf("\n%12I64x\n", v - _posInArc);
              RINOK(Seek2(v))
            }
            HRESULT res = Stream->Read(data, size, &size);
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
  kpidUnpackVer,
  kpidMethod
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
    case kpidClusterSize: prop = (UInt32)1 << _clusterBits; break;
    case kpidPhySize: if (_phySize != 0) prop = _phySize; break;
    case kpidUnpackVer: prop = _version; break;

    case kpidMethod:
    {
      AString s;

      if (_needDeflate)
        s = "Deflate";

      if (_cryptMethod != 0)
      {
        s.Add_Space_if_NotEmpty();
        if (_cryptMethod == 1)
          s += "AES";
        else
          s.Add_UInt32(_cryptMethod);
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
      if (!Stream && v == 0 && _isArc)
        v = kpv_ErrorFlags_HeadersError;
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
    case kpidPackSize: prop = _phySize; break;
    case kpidExtension: prop = (_imgExt ? _imgExt : "img"); break;
  }
  
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


HRESULT CHandler::Open2(IInStream *stream, IArchiveOpenCallback *openCallback)
{
  const unsigned kHeaderSize = 18 * 4;
  Byte buf[kHeaderSize];
  RINOK(ReadStream_FALSE(stream, buf, kHeaderSize))

  if (memcmp(buf, k_Signature, 4) != 0)
    return S_FALSE;

  _version = Get32(buf + 4);
  if (_version < 1 || _version > 3)
    return S_FALSE;
  
  const UInt64 backOffset = Get64(buf + 8);
  // UInt32 backSize = Get32(buf + 0x10);
  
  UInt64 l1Offset;
  UInt32 l1Size;

  if (_version == 1)
  {
    // _mTime = Get32(buf + 0x14); // is unused im most images
    _size = Get64(buf + 0x18);
    _clusterBits = buf[0x20];
    _numMidBits = buf[0x21];
    if (_clusterBits < 9 || _clusterBits > 30)
      return S_FALSE;
    if (_numMidBits < 1 || _numMidBits > 28)
      return S_FALSE;
    _cryptMethod = Get32(buf + 0x24);
    l1Offset = Get64(buf + 0x28);
    if (l1Offset < 0x30)
      return S_FALSE;
    const unsigned numBits2 = (_clusterBits + _numMidBits);
    const UInt64 l1Size64 = (_size + (((UInt64)1 << numBits2) - 1)) >> numBits2;
    if (l1Size64 > ((UInt32)1 << 31))
      return S_FALSE;
    l1Size = (UInt32)l1Size64;
  }
  else
  {
    _clusterBits = Get32(buf + 0x14);
    if (_clusterBits < 9 || _clusterBits > 30)
      return S_FALSE;
    _numMidBits = _clusterBits - 3;
    _size = Get64(buf + 0x18);
    _cryptMethod = Get32(buf + 0x20);
    l1Size = Get32(buf + 0x24);
    l1Offset = Get64(buf + 0x28); // must be aligned for cluster
    
    const UInt64 refOffset = Get64(buf + 0x30); // must be aligned for cluster
    const UInt32 refClusters = Get32(buf + 0x38);
    
    // UInt32 numSnapshots = Get32(buf + 0x3C);
    // UInt64 snapshotsOffset = Get64(buf + 0x40); // must be aligned for cluster
    /*
    if (numSnapshots != 0)
      return S_FALSE;
    */

    if (refClusters != 0)
    {
      const size_t numBytes = refClusters << _clusterBits;
      /*
      CByteBuffer refs;
      refs.Alloc(numBytes);
      RINOK(InStream_SeekSet(stream, refOffset))
      RINOK(ReadStream_FALSE(stream, refs, numBytes));
      */
      const UInt64 end = refOffset + numBytes;
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

  _isArc = true;

  if (backOffset != 0)
  {
    _unsupported = true;
    return S_FALSE;
  }

  const size_t clusterSize = (size_t)1 << _clusterBits;

  CByteBuffer table;
  {
    const size_t t1SizeBytes = (size_t)l1Size << 3;
    if ((t1SizeBytes >> 3) != l1Size)
      return S_FALSE;
    table.Alloc(t1SizeBytes);
    RINOK(InStream_SeekSet(stream, l1Offset))
    RINOK(ReadStream_FALSE(stream, table, t1SizeBytes))
    
    {
      UInt64 end = l1Offset + t1SizeBytes;
      // we need to uses align end for empty qcow files
      end = (end + clusterSize - 1) >> _clusterBits << _clusterBits;
      if (_phySize < end)
        _phySize = end;
    }
  }

  _compressedFlag = (_version <= 1) ? ((UInt64)1 << 63) : ((UInt64)1 << 62);
  const UInt64 offsetMask = _compressedFlag - 1;

  UInt32 numTables = 0;
  UInt32 i;
  
  for (i = 0; i < l1Size; i++)
  {
    const UInt64 v = Get64((const Byte *)table + (size_t)i * 8) & offsetMask;
    if (v != 0)
      numTables++;
  }

  if (numTables != 0)
  {
    const size_t size = (size_t)numTables << (_numMidBits + 3);
    if (size >> (_numMidBits + 3) != numTables)
      return E_OUTOFMEMORY;
    _table.Alloc(size);
    if (!_table.IsAllocated())
      return E_OUTOFMEMORY;
  }

  _dir.SetSize(l1Size);

  UInt32 curTable = 0;

  if (openCallback)
  {
    const UInt64 totalBytes = (UInt64)numTables << (_numMidBits + 3);
    RINOK(openCallback->SetTotal(NULL, &totalBytes))
  }

  for (i = 0; i < l1Size; i++)
  {
    Byte *buf2;
    const size_t midSize = (size_t)1 << (_numMidBits + 3);
   
    {
      const UInt64 v = Get64((const Byte *)table + (size_t)i * 8) & offsetMask;
      if (v == 0)
      {
        _dir[i] = kEmptyDirItem;
        continue;
      }

      _dir[i] = curTable;
      const size_t tableOffset = ((size_t)curTable << (_numMidBits + 3));
      buf2 = (Byte *)_table + tableOffset;
      curTable++;

      if (openCallback && (tableOffset & 0xFFFFF) == 0)
      {
        const UInt64 numBytes = tableOffset;
        RINOK(openCallback->SetCompleted(NULL, &numBytes))
      }
      
      RINOK(InStream_SeekSet(stream, v))
      RINOK(ReadStream_FALSE(stream, buf2, midSize))

      const UInt64 end = v + midSize;
      if (_phySize < end)
        _phySize = end;
    }

    for (size_t k = 0; k < midSize; k += 8)
    {
      const UInt64 v = Get64((const Byte *)buf2 + (size_t)k);
      if (v == 0)
        continue;
      UInt64 offset = v & offsetMask;
      size_t dataSize = clusterSize;
      
      if ((v & _compressedFlag) != 0)
      {
        if (_version <= 1)
        {
          unsigned numOffsetBits = (63 - _clusterBits);
          dataSize = ((size_t)(offset >> numOffsetBits) + 1) << 9;
          offset &= ((UInt64)1 << numOffsetBits) - 1;
          dataSize = 0;
          // offset >>= 9;
          // offset <<= 9;
        }
        else
        {
          unsigned numOffsetBits = (62 - (_clusterBits - 8));
          dataSize = ((size_t)(offset >> numOffsetBits) + 1) << 9;
          offset &= ((UInt64)1 << numOffsetBits) - 1;
          offset >>= 9;
          offset <<= 9;
        }
        _needDeflate = true;
      }
      else
      {
        UInt32 low = (UInt32)v & 511;
        if (low != 0)
        {
          // version 3 support zero clusters
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

  if (_cryptMethod != 0)
    _unsupported = true;

  if (_needDeflate && _version <= 1) // that case was not implemented
    _unsupported = true;

  Stream = stream;
  return S_OK;
}


Z7_COM7F_IMF(CHandler::Close())
{
  _table.Free();
  _dir.Free();
  _phySize = 0;

  _cacheCluster = (UInt64)(Int64)-1;
  _comprPos = 0;
  _comprSize = 0;
  _needDeflate = false;

  _isArc = false;
  _unsupported = false;

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

  if (_needDeflate)
  {
    if (_version <= 1)
      return S_FALSE;

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

    if (!_deflateDecoder)
    {
      _deflateDecoderSpec = new NCompress::NDeflate::NDecoder::CCOMCoder();
      _deflateDecoder = _deflateDecoderSpec;
      _deflateDecoderSpec->Set_NeedFinishInput(true);
    }
    
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

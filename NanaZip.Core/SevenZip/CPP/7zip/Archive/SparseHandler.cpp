// SparseHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"

#include "../../Windows/PropVariantUtils.h"

#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "HandlerCont.h"

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)

#define G16(_offs_, dest) dest = Get16(p + (_offs_));
#define G32(_offs_, dest) dest = Get32(p + (_offs_));

using namespace NWindows;

namespace NArchive {
namespace NSparse {

// libsparse and simg2img

struct CHeader
{
  // UInt32 magic;          /* 0xed26ff3a */
  // UInt16 major_version;  /* (0x1) - reject images with higher major versions */
  // UInt16 minor_version;  /* (0x0) - allow images with higer minor versions */
  UInt16 file_hdr_sz;    /* 28 bytes for first revision of the file format */
  UInt16 chunk_hdr_sz;   /* 12 bytes for first revision of the file format */
  UInt32 BlockSize;      /* block size in bytes, must be a multiple of 4 (4096) */
  UInt32 NumBlocks;      /* total blocks in the non-sparse output image */
  UInt32 NumChunks;      /* total chunks in the sparse input image */
  // UInt32 image_checksum; /* CRC32 checksum of the original data, counting "don't care" as 0. */

  void Parse(const Byte *p)
  {
    // G16 (4, major_version);
    // G16 (6, minor_version);
    G16 (8, file_hdr_sz)
    G16 (10, chunk_hdr_sz)
    G32 (12, BlockSize)
    G32 (16, NumBlocks)
    G32 (20, NumChunks)
    // G32 (24, image_checksum);
  }
};
 
// #define SPARSE_HEADER_MAGIC 0xed26ff3a

#define CHUNK_TYPE_RAW        0xCAC1
#define CHUNK_TYPE_FILL       0xCAC2
#define CHUNK_TYPE_DONT_CARE  0xCAC3
#define CHUNK_TYPE_CRC32      0xCAC4

#define MY_CHUNK_TYPE_FILL       0
#define MY_CHUNK_TYPE_DONT_CARE  1
#define MY_CHUNK_TYPE_RAW_START 2

static const char * const g_Methods[] =
{
    "RAW"
  , "FILL"
  , "SPARSE" // "DONT_CARE"
  , "CRC32"
};

static const unsigned kFillSize = 4;

struct CChunk
{
  UInt32 VirtBlock;
  Byte Fill [kFillSize];
  UInt64 PhyOffset;
};

static const Byte k_Signature[] = { 0x3a, 0xff, 0x26, 0xed, 1, 0 };


Z7_class_CHandler_final: public CHandlerImg
{
  Z7_IFACE_COM7_IMP(IInArchive_Img)

  Z7_IFACE_COM7_IMP(IInArchiveGetStream)
  Z7_IFACE_COM7_IMP(ISequentialInStream)

  CRecordVector<CChunk> Chunks;
  UInt64 _virtSize_fromChunks;
  unsigned _blockSizeLog;
  UInt32 _chunkIndexPrev;

  UInt64 _packSizeProcessed;
  UInt64 _phySize;
  UInt32 _methodFlags;
  bool _isArc;
  bool _headersError;
  bool _unexpectedEnd;
  // bool _unsupported;
  UInt32 NumChunks; // from header

  HRESULT Seek2(UInt64 offset)
  {
    _posInArc = offset;
    return InStream_SeekSet(Stream, offset);
  }

  void InitSeekPositions()
  {
    /* (_virtPos) and (_posInArc) is used only in Read() (that calls ReadPhy()).
       So we must reset these variables before first call of Read() */
    Reset_VirtPos();
    Reset_PosInArc();
    _chunkIndexPrev = 0;
    _packSizeProcessed = 0;
  }

  // virtual functions
  bool Init_PackSizeProcessed() Z7_override
  {
    _packSizeProcessed = 0;
    return true;
  }
  bool Get_PackSizeProcessed(UInt64 &size) Z7_override
  {
    size = _packSizeProcessed;
    return true;
  }

  HRESULT Open2(IInStream *stream, IArchiveOpenCallback *openCallback) Z7_override;
  HRESULT ReadPhy(UInt64 offset, void *data, UInt32 size, UInt32 &processed);
};



static const Byte kProps[] =
{
  kpidSize,
  kpidPackSize
};

static const Byte kArcProps[] =
{
  kpidClusterSize,
  kpidNumBlocks,
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
    case kpidClusterSize: prop = (UInt32)((UInt32)1 << _blockSizeLog); break;
    case kpidNumBlocks: prop = (UInt32)NumChunks; break;
    case kpidPhySize: if (_phySize != 0) prop = _phySize; break;

    case kpidMethod:
    {
      FLAGS_TO_PROP(g_Methods, _methodFlags, prop);
      break;
    }

    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc)        v |= kpv_ErrorFlags_IsNotArc;
      if (_headersError)  v |= kpv_ErrorFlags_HeadersError;
      if (_unexpectedEnd) v |= kpv_ErrorFlags_UnexpectedEnd;
      // if (_unsupported) v |= kpv_ErrorFlags_UnsupportedMethod;
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


static unsigned GetLogSize(UInt32 size)
{
  unsigned k;
  for (k = 0; k < 32; k++)
    if (((UInt32)1 << k) == size)
      return k;
  return k;
}


HRESULT CHandler::Open2(IInStream *stream, IArchiveOpenCallback *openCallback)
{
  const unsigned kHeaderSize = 28;
  const unsigned kChunkHeaderSize = 12;
  CHeader h;
  {
    Byte buf[kHeaderSize];
    RINOK(ReadStream_FALSE(stream, buf, kHeaderSize))
    if (memcmp(buf, k_Signature, 6) != 0)
      return S_FALSE;
    h.Parse(buf);
  }

  if (h.file_hdr_sz != kHeaderSize ||
      h.chunk_hdr_sz != kChunkHeaderSize)
    return S_FALSE;

  NumChunks = h.NumChunks;

  const unsigned logSize = GetLogSize(h.BlockSize);
  if (logSize < 2 || logSize >= 32)
    return S_FALSE;
  _blockSizeLog = logSize;

  _size = (UInt64)h.NumBlocks << logSize;

  if (h.NumChunks >= (UInt32)(Int32)-2) // it's our limit
    return S_FALSE;

  _isArc = true;
  Chunks.Reserve(h.NumChunks + 1);
  UInt64 offset = kHeaderSize;
  UInt32 virtBlock = 0;
  UInt32 i;

  for (i = 0; i < h.NumChunks; i++)
  {
    {
      const UInt32 mask = ((UInt32)1 << 16) - 1;
      if ((i & mask) == mask && openCallback)
      {
        RINOK(openCallback->SetCompleted(NULL, &offset))
      }
    }
    Byte buf[kChunkHeaderSize];
    {
      size_t processed = kChunkHeaderSize;
      RINOK(ReadStream(stream, buf, &processed))
      if (kChunkHeaderSize != processed)
      {
        offset += kChunkHeaderSize;
        break;
      }
    }
    const UInt32 type = Get32(&buf[0]);
    const UInt32 numBlocks = Get32(&buf[4]);
    UInt32 size = Get32(&buf[8]);

    if (type < CHUNK_TYPE_RAW ||
        type > CHUNK_TYPE_CRC32)
      return S_FALSE;
    if (size < kChunkHeaderSize)
      return S_FALSE;
    CChunk c;
    c.PhyOffset = offset + kChunkHeaderSize;
    c.VirtBlock = virtBlock;
    offset += size;
    size -= kChunkHeaderSize;
    _methodFlags |= ((UInt32)1 << (type - CHUNK_TYPE_RAW));

    if (numBlocks > h.NumBlocks - virtBlock)
      return S_FALSE;
    
    if (type == CHUNK_TYPE_CRC32)
    {
      // crc chunk must be last chunk (i == h.NumChunks -1);
      if (size != kFillSize || numBlocks != 0)
        return S_FALSE;
      {
        size_t processed = kFillSize;
        RINOK(ReadStream(stream, c.Fill, &processed))
        if (kFillSize != processed)
          break;
      }
      continue;
    }
    // else
    {
      if (numBlocks == 0)
        return S_FALSE;
      
      if (type == CHUNK_TYPE_DONT_CARE)
      {
        if (size != 0)
          return S_FALSE;
        c.PhyOffset = MY_CHUNK_TYPE_DONT_CARE;
      }
      else if (type == CHUNK_TYPE_FILL)
      {
        if (size != kFillSize)
          return S_FALSE;
        c.PhyOffset = MY_CHUNK_TYPE_FILL;
        size_t processed = kFillSize;
        RINOK(ReadStream(stream, c.Fill, &processed))
        if (kFillSize != processed)
          break;
      }
      else if (type == CHUNK_TYPE_RAW)
      {
        /* Here we require (size == virtSize).
           Probably original decoder also requires it.
           But maybe size of last chunk can be non-aligned with blockSize ? */
        const UInt32 virtSize = (numBlocks << _blockSizeLog);
        if (size != virtSize || numBlocks != (virtSize >> _blockSizeLog))
          return S_FALSE;
      }
      else
        return S_FALSE;

      virtBlock += numBlocks;
      Chunks.AddInReserved(c);
      if (type == CHUNK_TYPE_RAW)
        RINOK(InStream_SeekSet(stream, offset))
    }
  }

  if (i != h.NumChunks)
    _unexpectedEnd = true;
  else if (virtBlock != h.NumBlocks)
    _headersError = true;

  _phySize = offset;

  {
    CChunk c;
    c.VirtBlock = virtBlock;
    c.PhyOffset = offset;
    Chunks.AddInReserved(c);
  }
  _virtSize_fromChunks = (UInt64)virtBlock << _blockSizeLog;

  Stream = stream;
  return S_OK;
}


Z7_COM7F_IMF(CHandler::Close())
{
  Chunks.Clear();
  _isArc = false;
  _virtSize_fromChunks = 0;
  // _unsupported = false;
  _headersError = false;
  _unexpectedEnd = false;
  _phySize = 0;
  _methodFlags = 0;

  _chunkIndexPrev = 0;
  _packSizeProcessed = 0;

  // CHandlerImg:
  Clear_HandlerImg_Vars();
  Stream.Release();
  return S_OK;
}


Z7_COM7F_IMF(CHandler::GetStream(UInt32 /* index */, ISequentialInStream **stream))
{
  COM_TRY_BEGIN
  *stream = NULL;
  if (Chunks.Size() < 1)
    return S_FALSE;
  if (Chunks.Size() < 2 && _virtSize_fromChunks != 0)
    return S_FALSE;
  // if (_unsupported) return S_FALSE;
  InitSeekPositions();
  CMyComPtr<ISequentialInStream> streamTemp = this;
  *stream = streamTemp.Detach();
  return S_OK;
  COM_TRY_END
}



HRESULT CHandler::ReadPhy(UInt64 offset, void *data, UInt32 size, UInt32 &processed)
{
  processed = 0;
  if (offset > _phySize || offset + size > _phySize)
  {
    // we don't expect these cases, if (_phySize) was set correctly.
    return S_FALSE;
  }
  if (offset != _posInArc)
  {
    const HRESULT res = Seek2(offset);
    if (res != S_OK)
    {
      Reset_PosInArc(); // we don't trust seek_pos in case of error
      return res;
    }
  }
  {
    size_t size2 = size;
    const HRESULT res = ReadStream(Stream, data, &size2);
    processed = (UInt32)size2;
    _packSizeProcessed += size2;
    _posInArc += size2;
    if (res != S_OK)
      Reset_PosInArc(); // we don't trust seek_pos in case of reading error
    return res;
  }
}


Z7_COM7F_IMF(CHandler::Read(void *data, UInt32 size, UInt32 *processedSize))
{
  if (processedSize)
    *processedSize = 0;
  // const unsigned kLimit = (1 << 16) + 1; if (size > kLimit) size = kLimit; // for debug
  if (_virtPos >= _virtSize_fromChunks)
    return S_OK;
  {
    const UInt64 rem = _virtSize_fromChunks - _virtPos;
    if (size > rem)
      size = (UInt32)rem;
    if (size == 0)
      return S_OK;
  }

  UInt32 chunkIndex = _chunkIndexPrev;
  if (chunkIndex + 1 >= Chunks.Size())
    return S_FALSE;
  {
    const UInt32 blockIndex = (UInt32)(_virtPos >> _blockSizeLog);
    if (blockIndex <  Chunks[chunkIndex    ].VirtBlock ||
        blockIndex >= Chunks[chunkIndex + 1].VirtBlock)
    {
      unsigned left = 0, right = Chunks.Size() - 1;
      for (;;)
      {
        const unsigned mid = (unsigned)(((size_t)left + (size_t)right) / 2);
        if (mid == left)
          break;
        if (blockIndex < Chunks[mid].VirtBlock)
          right = mid;
        else
          left = mid;
      }
      chunkIndex = left;
      _chunkIndexPrev = chunkIndex;
    }
  }

  const CChunk &c = Chunks[chunkIndex];
  const UInt64 offset = _virtPos - ((UInt64)c.VirtBlock << _blockSizeLog);
  {
    const UInt32 numBlocks = Chunks[chunkIndex + 1].VirtBlock - c.VirtBlock;
    const UInt64 rem = ((UInt64)numBlocks << _blockSizeLog) - offset;
    if (size > rem)
      size = (UInt32)rem;
  }

  const UInt64 phyOffset = c.PhyOffset;
  
  if (phyOffset >= MY_CHUNK_TYPE_RAW_START)
  {
    UInt32 processed = 0;
    const HRESULT res = ReadPhy(phyOffset + offset, data, size, processed);
    if (processedSize)
      *processedSize = processed;
    _virtPos += processed;
    return res;
  }

  Byte b = 0;
  
  if (phyOffset == MY_CHUNK_TYPE_FILL)
  {
    const Byte b0 = c.Fill [0];
    const Byte b1 = c.Fill [1];
    const Byte b2 = c.Fill [2];
    const Byte b3 = c.Fill [3];
    if (b0 != b1 ||
        b0 != b2 ||
        b0 != b3)
    {
      if (processedSize)
        *processedSize = size;
      _virtPos += size;
      Byte *dest = (Byte *)data;
      while (size >= 4)
      {
        dest[0] = b0;
        dest[1] = b1;
        dest[2] = b2;
        dest[3] = b3;
        dest += 4;
        size -= 4;
      }
      if (size > 0) dest[0] = b0;
      if (size > 1) dest[1] = b1;
      if (size > 2) dest[2] = b2;
      return S_OK;
    }
    b = b0;
  }
  else if (phyOffset != MY_CHUNK_TYPE_DONT_CARE)
    return S_FALSE;
  
  memset(data, b, size);
  _virtPos += size;
  if (processedSize)
    *processedSize = size;
  return S_OK;
}

REGISTER_ARC_I(
  "Sparse", "simg img", NULL, 0xc2,
  k_Signature,
  0,
  0,
  NULL)

}}

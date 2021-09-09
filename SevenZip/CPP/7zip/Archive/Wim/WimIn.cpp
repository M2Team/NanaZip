// Archive/WimIn.cpp

#include "StdAfx.h"

// #define SHOW_DEBUG_INFO

#ifdef SHOW_DEBUG_INFO
#include <stdio.h>
#define PRF(x) x
#else
#define PRF(x)
#endif

#include "../../../../C/CpuArch.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringToInt.h"
#include "../../../Common/UTFConvert.h"

#include "../../Common/LimitedStreams.h"
#include "../../Common/StreamObjects.h"
#include "../../Common/StreamUtils.h"

#include "../../Compress/XpressDecoder.h"

#include "../Common/OutStreamWithSha1.h"

#include "WimIn.h"

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)
#define Get64(p) GetUi64(p)

namespace NArchive {
namespace NWim {

static int inline GetLog(UInt32 num)
{
  for (int i = 0; i < 32; i++)
    if (((UInt32)1 << i) == num)
      return i;
  return -1;
}


CUnpacker::~CUnpacker()
{
  if (lzmsDecoder)
    delete lzmsDecoder;
}


HRESULT CUnpacker::UnpackChunk(
    ISequentialInStream *inStream,
    unsigned method, unsigned chunkSizeBits,
    size_t inSize, size_t outSize,
    ISequentialOutStream *outStream)
{
  if (inSize == outSize)
  {
  }
  else if (method == NMethod::kXPRESS)
  {
  }
  else if (method == NMethod::kLZX)
  {
    if (!lzxDecoder)
    {
      lzxDecoderSpec = new NCompress::NLzx::CDecoder(true);
      lzxDecoder = lzxDecoderSpec;
    }
  }
  else if (method == NMethod::kLZMS)
  {
    if (!lzmsDecoder)
      lzmsDecoder = new NCompress::NLzms::CDecoder();
  }
  else
    return E_NOTIMPL;

  const size_t chunkSize = (size_t)1 << chunkSizeBits;
  
  unpackBuf.EnsureCapacity(chunkSize);
  if (!unpackBuf.Data)
    return E_OUTOFMEMORY;
  
  HRESULT res = S_FALSE;
  size_t unpackedSize = 0;
  
  if (inSize == outSize)
  {
    unpackedSize = outSize;
    res = ReadStream(inStream, unpackBuf.Data, &unpackedSize);
    TotalPacked += unpackedSize;
  }
  else if (inSize < chunkSize)
  {
    packBuf.EnsureCapacity(chunkSize);
    if (!packBuf.Data)
      return E_OUTOFMEMORY;
    
    RINOK(ReadStream_FALSE(inStream, packBuf.Data, inSize));

    TotalPacked += inSize;
    
    if (method == NMethod::kXPRESS)
    {
      res = NCompress::NXpress::Decode(packBuf.Data, inSize, unpackBuf.Data, outSize);
      if (res == S_OK)
        unpackedSize = outSize;
    }
    else if (method == NMethod::kLZX)
    {
      res = lzxDecoderSpec->SetExternalWindow(unpackBuf.Data, chunkSizeBits);
      if (res != S_OK)
        return E_NOTIMPL;
      lzxDecoderSpec->KeepHistoryForNext = false;
      lzxDecoderSpec->SetKeepHistory(false);
      res = lzxDecoderSpec->Code(packBuf.Data, inSize, (UInt32)outSize);
      unpackedSize = lzxDecoderSpec->GetUnpackSize();
      if (res == S_OK && !lzxDecoderSpec->WasBlockFinished())
        res = S_FALSE;
    }
    else
    {
      res = lzmsDecoder->Code(packBuf.Data, inSize, unpackBuf.Data, outSize);
      unpackedSize = lzmsDecoder->GetUnpackSize();;
    }
  }
  
  if (unpackedSize != outSize)
  {
    if (res == S_OK)
      res = S_FALSE;
    
    if (unpackedSize > outSize)
      res = S_FALSE;
    else
      memset(unpackBuf.Data + unpackedSize, 0, outSize - unpackedSize);
  }
  
  if (outStream)
  {
    RINOK(WriteStream(outStream, unpackBuf.Data, outSize));
  }
  
  return res;
}


HRESULT CUnpacker::Unpack2(
    IInStream *inStream,
    const CResource &resource,
    const CHeader &header,
    const CDatabase *db,
    ISequentialOutStream *outStream,
    ICompressProgressInfo *progress)
{
  if (!resource.IsCompressed() && !resource.IsSolid())
  {
    if (!copyCoder)
    {
      copyCoderSpec = new NCompress::CCopyCoder;
      copyCoder = copyCoderSpec;
    }

    CLimitedSequentialInStream *limitedStreamSpec = new CLimitedSequentialInStream();
    CMyComPtr<ISequentialInStream> limitedStream = limitedStreamSpec;
    limitedStreamSpec->SetStream(inStream);
    
    RINOK(inStream->Seek(resource.Offset, STREAM_SEEK_SET, NULL));
    if (resource.PackSize != resource.UnpackSize)
      return S_FALSE;

    limitedStreamSpec->Init(resource.PackSize);
    TotalPacked += resource.PackSize;
    
    HRESULT res = copyCoder->Code(limitedStream, outStream, NULL, NULL, progress);
    
    if (res == S_OK && copyCoderSpec->TotalSize != resource.UnpackSize)
      res = S_FALSE;
    return res;
  }
  
  if (resource.IsSolid())
  {
    if (!db || resource.SolidIndex < 0)
      return E_NOTIMPL;
    if (resource.IsCompressed())
      return E_NOTIMPL;

    const CSolid &ss = db->Solids[resource.SolidIndex];
    
    const unsigned chunkSizeBits = ss.ChunkSizeBits;
    const size_t chunkSize = (size_t)1 << chunkSizeBits;
    
    size_t chunkIndex = 0;
    UInt64 rem = ss.UnpackSize;
    size_t offsetInChunk = 0;
    
    if (resource.IsSolidSmall())
    {
      UInt64 offs = resource.Offset;
      if (offs < ss.SolidOffset)
        return E_NOTIMPL;
      offs -= ss.SolidOffset;
      if (offs > ss.UnpackSize)
        return E_NOTIMPL;
      rem = resource.PackSize;
      if (rem > ss.UnpackSize - offs)
        return E_NOTIMPL;
      chunkIndex = (size_t)(offs >> chunkSizeBits);
      offsetInChunk = (size_t)offs & (chunkSize - 1);
    }
    
    UInt64 packProcessed = 0;
    UInt64 outProcessed = 0;
    
    if (_solidIndex == resource.SolidIndex && _unpackedChunkIndex == chunkIndex)
    {
      size_t cur = chunkSize - offsetInChunk;
      if (cur > rem)
        cur = (size_t)rem;
      RINOK(WriteStream(outStream, unpackBuf.Data + offsetInChunk, cur));
      outProcessed += cur;
      rem -= cur;
      offsetInChunk = 0;
      chunkIndex++;
    }
    
    for (;;)
    {
      if (rem == 0)
        return S_OK;
    
      UInt64 offset = ss.Chunks[chunkIndex];
      UInt64 packSize = ss.GetChunkPackSize(chunkIndex);
      const CResource &rs = db->DataStreams[ss.StreamIndex].Resource;
      RINOK(inStream->Seek(rs.Offset + ss.HeadersSize + offset, STREAM_SEEK_SET, NULL));
      
      size_t cur = chunkSize;
      UInt64 unpackRem = ss.UnpackSize - ((UInt64)chunkIndex << chunkSizeBits);
      if (cur > unpackRem)
        cur = (size_t)unpackRem;
      
      _solidIndex = -1;
      _unpackedChunkIndex = 0;
      
      HRESULT res = UnpackChunk(inStream, ss.Method, chunkSizeBits, (size_t)packSize, cur, NULL);
      
      if (res != S_OK)
      {
        // We ignore data errors in solid stream. SHA will show what files are bad.
        if (res != S_FALSE)
          return res;
      }
      
      _solidIndex = resource.SolidIndex;
      _unpackedChunkIndex = chunkIndex;

      if (cur < offsetInChunk)
        return E_FAIL;
      
      cur -= offsetInChunk;
        
      if (cur > rem)
        cur = (size_t)rem;
      
      RINOK(WriteStream(outStream, unpackBuf.Data + offsetInChunk, cur));
      
      if (progress)
      {
        RINOK(progress->SetRatioInfo(&packProcessed, &outProcessed));
        packProcessed += packSize;
        outProcessed += cur;
      }
      
      rem -= cur;
      offsetInChunk = 0;
      chunkIndex++;
    }
  }


  // ---------- NON Solid ----------

  const UInt64 unpackSize = resource.UnpackSize;
  if (unpackSize == 0)
  {
    if (resource.PackSize == 0)
      return S_OK;
    return S_FALSE;
  }

  if (unpackSize > ((UInt64)1 << 63))
    return E_NOTIMPL;

  const unsigned chunkSizeBits = header.ChunkSizeBits;
  const unsigned entrySizeShifts = (resource.UnpackSize < ((UInt64)1 << 32) ? 2 : 3);

  UInt64 baseOffset = resource.Offset;
  UInt64 packDataSize;
  size_t numChunks;
  {
    const UInt64 numChunks64 = (unpackSize + (((UInt32)1 << chunkSizeBits) - 1)) >> chunkSizeBits;
    const UInt64 sizesBufSize64 = (numChunks64 - 1) << entrySizeShifts;
    if (sizesBufSize64 > resource.PackSize)
      return S_FALSE;
    packDataSize = resource.PackSize - sizesBufSize64;
    const size_t sizesBufSize = (size_t)sizesBufSize64;
    if (sizesBufSize != sizesBufSize64)
      return E_OUTOFMEMORY;
    sizesBuf.AllocAtLeast(sizesBufSize);
    RINOK(inStream->Seek(baseOffset, STREAM_SEEK_SET, NULL));
    RINOK(ReadStream_FALSE(inStream, sizesBuf, sizesBufSize));
    baseOffset += sizesBufSize64;
    numChunks = (size_t)numChunks64;
  }

  _solidIndex = -1;
  _unpackedChunkIndex = 0;

  UInt64 outProcessed = 0;
  UInt64 offset = 0;
  
  for (size_t i = 0; i < numChunks; i++)
  {
    UInt64 nextOffset = packDataSize;
    
    if (i + 1 < numChunks)
    {
      const Byte *p = (const Byte *)sizesBuf + (i << entrySizeShifts);
      nextOffset = (entrySizeShifts == 2) ? Get32(p): Get64(p);
    }
    
    if (nextOffset < offset)
      return S_FALSE;

    UInt64 inSize64 = nextOffset - offset;
    size_t inSize = (size_t)inSize64;
    if (inSize != inSize64)
      return S_FALSE;

    RINOK(inStream->Seek(baseOffset + offset, STREAM_SEEK_SET, NULL));

    if (progress)
    {
      RINOK(progress->SetRatioInfo(&offset, &outProcessed));
    }
    
    size_t outSize = (size_t)1 << chunkSizeBits;
    const UInt64 rem = unpackSize - outProcessed;
    if (outSize > rem)
      outSize = (size_t)rem;

    RINOK(UnpackChunk(inStream, header.GetMethod(), chunkSizeBits, inSize, outSize, outStream));

    outProcessed += outSize;
    offset = nextOffset;
  }
  
  return S_OK;
}


HRESULT CUnpacker::Unpack(IInStream *inStream, const CResource &resource, const CHeader &header, const CDatabase *db,
    ISequentialOutStream *outStream, ICompressProgressInfo *progress, Byte *digest)
{
  COutStreamWithSha1 *shaStreamSpec = NULL;
  CMyComPtr<ISequentialOutStream> shaStream;
  
  // outStream can be NULL, so we use COutStreamWithSha1 even if sha1 is not required
  // if (digest)
  {
    shaStreamSpec = new COutStreamWithSha1();
    shaStream = shaStreamSpec;
    shaStreamSpec->SetStream(outStream);
    shaStreamSpec->Init(digest != NULL);
    outStream = shaStream;
  }
  
  HRESULT res = Unpack2(inStream, resource, header, db, outStream, progress);
  
  if (digest)
    shaStreamSpec->Final(digest);
  
  return res;
}


HRESULT CUnpacker::UnpackData(IInStream *inStream,
    const CResource &resource, const CHeader &header,
    const CDatabase *db,
    CByteBuffer &buf, Byte *digest)
{
  // if (resource.IsSolid()) return E_NOTIMPL;

  UInt64 unpackSize64 = resource.UnpackSize;
  if (db)
    unpackSize64 = db->Get_UnpackSize_of_Resource(resource);

  size_t size = (size_t)unpackSize64;
  if (size != unpackSize64)
    return E_OUTOFMEMORY;

  buf.Alloc(size);

  CBufPtrSeqOutStream *outStreamSpec = new CBufPtrSeqOutStream();
  CMyComPtr<ISequentialOutStream> outStream = outStreamSpec;
  outStreamSpec->Init((Byte *)buf, size);

  return Unpack(inStream, resource, header, db, outStream, NULL, digest);
}


void CResource::Parse(const Byte *p)
{
  Flags = p[7];
  PackSize = Get64(p) & (((UInt64)1 << 56) - 1);
  Offset = Get64(p + 8);
  UnpackSize = Get64(p + 16);
  KeepSolid = false;
  SolidIndex = -1;
}

#define GET_RESOURCE(_p_, res) res.ParseAndUpdatePhySize(_p_, phySize)

static inline void ParseStream(bool oldVersion, const Byte *p, CStreamInfo &s)
{
  s.Resource.Parse(p);
  if (oldVersion)
  {
    s.PartNumber = 1;
    s.Id = Get32(p + 24);
    p += 28;
  }
  else
  {
    s.PartNumber = Get16(p + 24);
    p += 26;
  }
  s.RefCount = Get32(p);
  memcpy(s.Hash, p + 4, kHashSize);
}


#define kLongPath "[LongPath]"

void CDatabase::GetShortName(unsigned index, NWindows::NCOM::CPropVariant &name) const
{
  const CItem &item = Items[index];
  const CImage &image = Images[item.ImageIndex];
  if (item.Parent < 0 && image.NumEmptyRootItems != 0)
  {
    name.Clear();
    return;
  }
  const Byte *meta = image.Meta + item.Offset +
      (IsOldVersion ? kDirRecordSizeOld : kDirRecordSize);
  UInt32 fileNameLen = Get16(meta - 2);
  UInt32 shortLen = Get16(meta - 4) / 2;
  wchar_t *s = name.AllocBstr(shortLen);
  if (fileNameLen != 0)
    meta += fileNameLen + 2;
  for (UInt32 i = 0; i < shortLen; i++)
    s[i] = Get16(meta + i * 2);
  s[shortLen] = 0;
  // empty shortName has no ZERO at the end ?
}


void CDatabase::GetItemName(unsigned index, NWindows::NCOM::CPropVariant &name) const
{
  const CItem &item = Items[index];
  const CImage &image = Images[item.ImageIndex];
  if (item.Parent < 0 && image.NumEmptyRootItems != 0)
  {
    name = image.RootName;
    return;
  }
  const Byte *meta = image.Meta + item.Offset +
      (item.IsAltStream ?
      (IsOldVersion ? 0x10 : 0x24) :
      (IsOldVersion ? kDirRecordSizeOld - 2 : kDirRecordSize - 2));
  UInt32 len = Get16(meta) / 2;
  wchar_t *s = name.AllocBstr(len);
  meta += 2;
  len++;
  for (UInt32 i = 0; i < len; i++)
    s[i] = Get16(meta + i * 2);
}


void CDatabase::GetItemPath(unsigned index1, bool showImageNumber, NWindows::NCOM::CPropVariant &path) const
{
  unsigned size = 0;
  int index = index1;
  int imageIndex = Items[index].ImageIndex;
  const CImage &image = Images[imageIndex];
  
  unsigned newLevel = 0;
  bool needColon = false;

  for (;;)
  {
    const CItem &item = Items[index];
    index = item.Parent;
    if (index >= 0 || image.NumEmptyRootItems == 0)
    {
      const Byte *meta = image.Meta + item.Offset;
      meta += item.IsAltStream ?
          (IsOldVersion ? 0x10 : 0x24) :
          (IsOldVersion ? kDirRecordSizeOld - 2 : kDirRecordSize - 2);
      needColon = item.IsAltStream;
      size += Get16(meta) / 2;
      size += newLevel;
      newLevel = 1;
      if (size >= ((UInt32)1 << 15))
      {
        path = kLongPath;
        return;
      }
    }
    if (index < 0)
      break;
  }

  if (showImageNumber)
  {
    size += image.RootName.Len();
    size += newLevel;
  }
  else if (needColon)
    size++;

  wchar_t *s = path.AllocBstr(size);
  s[size] = 0;
  
  if (showImageNumber)
  {
    MyStringCopy(s, (const wchar_t *)image.RootName);
    if (newLevel)
      s[image.RootName.Len()] = (wchar_t)(needColon ? L':' : WCHAR_PATH_SEPARATOR);
  }
  else if (needColon)
    s[0] = L':';

  index = index1;
  wchar_t separator = 0;
  
  for (;;)
  {
    const CItem &item = Items[index];
    index = item.Parent;
    if (index >= 0 || image.NumEmptyRootItems == 0)
    {
      if (separator != 0)
        s[--size] = separator;
      const Byte *meta = image.Meta + item.Offset;
      meta += (item.IsAltStream) ?
          (IsOldVersion ? 0x10: 0x24) :
          (IsOldVersion ? kDirRecordSizeOld - 2 : kDirRecordSize - 2);
      unsigned len = Get16(meta) / 2;
      size -= len;
      wchar_t *dest = s + size;
      meta += 2;
      for (unsigned i = 0; i < len; i++)
      {
        wchar_t c = Get16(meta + i * 2);
        // 18.06
        if (c == CHAR_PATH_SEPARATOR || c == '/')
          c = '_';
        dest[i] = c;
      }
    }
    if (index < 0)
      return;
    separator = item.IsAltStream ? L':' : WCHAR_PATH_SEPARATOR;
  }
}


// if (ver <= 1.10), root folder contains real items.
// if (ver >= 1.12), root folder contains only one folder with empty name.

HRESULT CDatabase::ParseDirItem(size_t pos, int parent)
{
  const unsigned align = GetDirAlignMask();
  if ((pos & align) != 0)
    return S_FALSE;

  for (unsigned numItems = 0;; numItems++)
  {
    if (OpenCallback && (Items.Size() & 0xFFFF) == 0)
    {
      UInt64 numFiles = Items.Size();
      RINOK(OpenCallback->SetCompleted(&numFiles, NULL));
    }
    
    const size_t rem = DirSize - pos;
    if (pos < DirStartOffset || pos > DirSize || rem < 8)
      return S_FALSE;

    const Byte *p = DirData + pos;

    UInt64 len = Get64(p);
    if (len == 0)
    {
      DirProcessed += 8;
      return S_OK;
    }
    
    if ((len & align) != 0 || rem < len)
      return S_FALSE;
    
    DirProcessed += (size_t)len;
    if (DirProcessed > DirSize)
      return S_FALSE;

    const unsigned dirRecordSize = IsOldVersion ? kDirRecordSizeOld : kDirRecordSize;
    if (len < dirRecordSize)
      return S_FALSE;

    CItem item;
    UInt32 attrib = Get32(p + 8);
    item.IsDir = ((attrib & 0x10) != 0);
    UInt64 subdirOffset = Get64(p + 0x10);

    const UInt32 numAltStreams = Get16(p + dirRecordSize - 6);
    const UInt32 shortNameLen = Get16(p + dirRecordSize - 4);
    const UInt32 fileNameLen = Get16(p + dirRecordSize - 2);
    if ((shortNameLen & 1) != 0 || (fileNameLen & 1) != 0)
      return S_FALSE;
    const UInt32 shortNameLen2 = (shortNameLen == 0 ? shortNameLen : shortNameLen + 2);
    const UInt32 fileNameLen2 = (fileNameLen == 0 ? fileNameLen : fileNameLen + 2);
    if (((dirRecordSize + fileNameLen2 + shortNameLen2 + align) & ~align) > len)
      return S_FALSE;
    
    p += dirRecordSize;
    
    {
      if (*(const UInt16 *)(const void *)(p + fileNameLen) != 0)
        return S_FALSE;
      for (UInt32 j = 0; j < fileNameLen; j += 2)
        if (*(const UInt16 *)(const void *)(p + j) == 0)
          return S_FALSE;
    }

    // PRF(printf("\n%S", p));

    if (shortNameLen != 0)
    {
      // empty shortName has no ZERO at the end ?
      const Byte *p2 = p + fileNameLen2;
      if (*(const UInt16 *)(const void *)(p2 + shortNameLen) != 0)
        return S_FALSE;
      for (UInt32 j = 0; j < shortNameLen; j += 2)
        if (*(const UInt16 *)(const void *)(p2 + j) == 0)
          return S_FALSE;
    }
      
    item.Offset = pos;
    item.Parent = parent;
    item.ImageIndex = Images.Size() - 1;
    
    const unsigned prevIndex = Items.Add(item);

    pos += (size_t)len;

    for (UInt32 i = 0; i < numAltStreams; i++)
    {
      const size_t rem2 = DirSize - pos;
      if (pos < DirStartOffset || pos > DirSize || rem2 < 8)
        return S_FALSE;
      const Byte *p2 = DirData + pos;
      const UInt64 len2 = Get64(p2);
      if ((len2 & align) != 0 || rem2 < len2 || len2 < (IsOldVersion ? 0x18 : 0x28))
        return S_FALSE;
     
      DirProcessed += (size_t)len2;
      if (DirProcessed > DirSize)
        return S_FALSE;

      unsigned extraOffset = 0;
      
      if (IsOldVersion)
        extraOffset = 0x10;
      else
      {
        if (Get64(p2 + 8) != 0)
          return S_FALSE;
        extraOffset = 0x24;
      }
      
      const UInt32 fileNameLen111 = Get16(p2 + extraOffset);
      if ((fileNameLen111 & 1) != 0)
        return S_FALSE;
      /* Probably different versions of ImageX can use different number of
         additional ZEROs. So we don't use exact check. */
      const UInt32 fileNameLen222 = (fileNameLen111 == 0 ? fileNameLen111 : fileNameLen111 + 2);
      if (((extraOffset + 2 + fileNameLen222 + align) & ~align) > len2)
        return S_FALSE;
      
      {
        const Byte *p3 = p2 + extraOffset + 2;
        if (*(const UInt16 *)(const void *)(p3 + fileNameLen111) != 0)
          return S_FALSE;
        for (UInt32 j = 0; j < fileNameLen111; j += 2)
          if (*(const UInt16 *)(const void *)(p3 + j) == 0)
            return S_FALSE;
  
        // PRF(printf("\n  %S", p3));
      }


      /* wim uses alt sreams list, if there is at least one alt stream.
         And alt stream without name is main stream. */

      // Why wimlib writes two alt streams for REPARSE_POINT, with empty second alt stream?
      
      Byte *prevMeta = DirData + item.Offset;

      if (fileNameLen111 == 0 &&
          ((attrib & FILE_ATTRIBUTE_REPARSE_POINT) || !item.IsDir)
          && (IsOldVersion || IsEmptySha(prevMeta + 0x40)))
      {
        if (IsOldVersion)
          memcpy(prevMeta + 0x10, p2 + 8, 4); // It's 32-bit Id
        else if (!IsEmptySha(p2 + 0x10))
        {
          // if (IsEmptySha(prevMeta + 0x40))
            memcpy(prevMeta + 0x40, p2 + 0x10, kHashSize);
          // else HeadersError = true;
        }
      }
      else
      {
        ThereAreAltStreams = true;
        CItem item2;
        item2.Offset = pos;
        item2.IsAltStream = true;
        item2.Parent = prevIndex;
        item2.ImageIndex = Images.Size() - 1;
        Items.Add(item2);
      }

      pos += (size_t)len2;
    }

    if (parent < 0 && numItems == 0 && shortNameLen == 0 && fileNameLen == 0 && item.IsDir)
    {
      const Byte *p2 = DirData + pos;
      if (DirSize - pos >= 8 && Get64(p2) == 0)
      {
        CImage &image = Images.Back();
        image.NumEmptyRootItems = 1;

        if (subdirOffset != 0
            && DirSize - pos >= 16
            && Get64(p2 + 8) != 0
            && pos + 8 < subdirOffset)
        {
          // Longhorn.4093 contains hidden files after empty root folder and before items of next folder. Why?
          // That code shows them. If we want to ignore them, we need to update DirProcessed.
          // DirProcessed += (size_t)(subdirOffset - (pos + 8));
          // printf("\ndirOffset = %5d hiddenOffset = %5d\n", (int)subdirOffset, (int)pos + 8);
          subdirOffset = pos + 8;
          // return S_FALSE;
        }
      }
    }

    if (item.IsDir && subdirOffset != 0)
    {
      RINOK(ParseDirItem((size_t)subdirOffset, prevIndex));
    }
  }
}


HRESULT CDatabase::ParseImageDirs(CByteBuffer &buf, int parent)
{
  DirData = buf;
  DirSize = buf.Size();
  if (DirSize < 8)
    return S_FALSE;
  const Byte *p = DirData;
  size_t pos = 0;
  CImage &image = Images.Back();

  if (IsOldVersion)
  {
    UInt32 numEntries = Get32(p + 4);

    if (numEntries > (1 << 28) ||
        numEntries > (DirSize >> 3))
      return S_FALSE;

    UInt32 sum = 8;
    if (numEntries != 0)
      sum = numEntries * 8;

    image.SecurOffsets.ClearAndReserve(numEntries + 1);
    image.SecurOffsets.AddInReserved(sum);

    for (UInt32 i = 0; i < numEntries; i++)
    {
      const Byte *pp = p + (size_t)i * 8;
      UInt32 len = Get32(pp);
      if (i != 0 && Get32(pp + 4) != 0)
        return S_FALSE;
      if (len > DirSize - sum)
        return S_FALSE;
      sum += len;
      if (sum < len)
        return S_FALSE;
      image.SecurOffsets.AddInReserved(sum);
    }

    pos = sum;

    const size_t align = GetDirAlignMask();
    pos = (pos + align) & ~(size_t)align;
  }
  else
  {
    UInt32 totalLen = Get32(p);
    if (totalLen == 0)
      pos = 8;
    else
    {
      if (totalLen < 8)
        return S_FALSE;
      UInt32 numEntries = Get32(p + 4);
      pos = 8;
      if (totalLen > DirSize || numEntries > ((totalLen - 8) >> 3))
        return S_FALSE;
      UInt32 sum = (UInt32)pos + numEntries * 8;
      image.SecurOffsets.ClearAndReserve(numEntries + 1);
      image.SecurOffsets.AddInReserved(sum);
      
      for (UInt32 i = 0; i < numEntries; i++, pos += 8)
      {
        UInt64 len = Get64(p + pos);
        if (len > totalLen - sum)
          return S_FALSE;
        sum += (UInt32)len;
        image.SecurOffsets.AddInReserved(sum);
      }
      
      pos = sum;
      pos = (pos + 7) & ~(size_t)7;
      if (pos != (((size_t)totalLen + 7) & ~(size_t)7))
        return S_FALSE;
    }
  }
  
  if (pos > DirSize)
    return S_FALSE;
  
  DirStartOffset = DirProcessed = pos;
  image.StartItem = Items.Size();

  RINOK(ParseDirItem(pos, parent));
  
  image.NumItems = Items.Size() - image.StartItem;
  if (DirProcessed == DirSize)
    return S_OK;

  /* Original program writes additional 8 bytes (END_OF_ROOT_FOLDER),
     but the reference to that folder is empty */

  // we can't use DirProcessed - DirStartOffset == 112 check if there is alt stream in root
  if (DirProcessed == DirSize - 8 && Get64(p + DirSize - 8) != 0)
    return S_OK;

  // 18.06: we support cases, when some old dism can capture images
  // where DirProcessed much smaller than DirSize
  HeadersError = true;
  return S_OK;
  // return S_FALSE;
}


HRESULT CHeader::Parse(const Byte *p, UInt64 &phySize)
{
  UInt32 headerSize = Get32(p + 8);
  phySize = headerSize;
  Version = Get32(p + 0x0C);
  Flags = Get32(p + 0x10);
  if (!IsSupported())
    return S_FALSE;
  
  {
    ChunkSize = Get32(p + 0x14);
    ChunkSizeBits = kChunkSizeBits;
    if (ChunkSize != 0)
    {
      int log = GetLog(ChunkSize);
      if (log < 12)
        return S_FALSE;
      ChunkSizeBits = log;
    }
  }

  _IsOldVersion = false;
  _IsNewVersion = false;
  
  if (IsSolidVersion())
    _IsNewVersion = true;
  else
  {
    if (Version < 0x010900)
      return S_FALSE;
    _IsOldVersion = (Version <= 0x010A00);
    // We don't know details about 1.11 version. So we use headerSize to guess exact features.
    if (Version == 0x010B00 && headerSize == 0x60)
      _IsOldVersion = true;
    _IsNewVersion = (Version >= 0x010D00);
  }

  unsigned offset;
  
  if (IsOldVersion())
  {
    if (headerSize != 0x60)
      return S_FALSE;
    memset(Guid, 0, 16);
    offset = 0x18;
    PartNumber = 1;
    NumParts = 1;
  }
  else
  {
    if (headerSize < 0x74)
      return S_FALSE;
    memcpy(Guid, p + 0x18, 16);
    PartNumber = Get16(p + 0x28);
    NumParts = Get16(p + 0x2A);
    if (PartNumber == 0 || PartNumber > NumParts)
      return S_FALSE;
    offset = 0x2C;
    if (IsNewVersion())
    {
      // if (headerSize < 0xD0)
      if (headerSize != 0xD0)
        return S_FALSE;
      NumImages = Get32(p + offset);
      offset += 4;
    }
  }
  
  GET_RESOURCE(p + offset       , OffsetResource);
  GET_RESOURCE(p + offset + 0x18, XmlResource);
  GET_RESOURCE(p + offset + 0x30, MetadataResource);
  BootIndex = 0;
  
  if (IsNewVersion())
  {
    BootIndex = Get32(p + offset + 0x48);
    GET_RESOURCE(p + offset + 0x4C, IntegrityResource);
  }

  return S_OK;
}


const Byte kSignature[kSignatureSize] = { 'M', 'S', 'W', 'I', 'M', 0, 0, 0 };

HRESULT ReadHeader(IInStream *inStream, CHeader &h, UInt64 &phySize)
{
  Byte p[kHeaderSizeMax];
  RINOK(ReadStream_FALSE(inStream, p, kHeaderSizeMax));
  if (memcmp(p, kSignature, kSignatureSize) != 0)
    return S_FALSE;
  return h.Parse(p, phySize);
}


static HRESULT ReadStreams(IInStream *inStream, const CHeader &h, CDatabase &db)
{
  CByteBuffer offsetBuf;
  
  CUnpacker unpacker;
  RINOK(unpacker.UnpackData(inStream, h.OffsetResource, h, NULL, offsetBuf, NULL));
  
  const size_t streamInfoSize = h.IsOldVersion() ? kStreamInfoSize + 2 : kStreamInfoSize;
  {
    const unsigned numItems = (unsigned)(offsetBuf.Size() / streamInfoSize);
    if ((size_t)numItems * streamInfoSize != offsetBuf.Size())
      return S_FALSE;
    const unsigned numItems2 = db.DataStreams.Size() + numItems;
    if (numItems2 < numItems)
      return S_FALSE;
    db.DataStreams.Reserve(numItems2);
  }

  bool keepSolid = false;

  for (size_t i = 0; i < offsetBuf.Size(); i += streamInfoSize)
  {
    CStreamInfo s;
    ParseStream(h.IsOldVersion(), (const Byte *)offsetBuf + i, s);

    PRF(printf("\n"));
    PRF(printf(s.Resource.IsMetadata() ? "### META" : "    DATA"));
    PRF(printf(" %2X", s.Resource.Flags));
    PRF(printf(" %9I64X", s.Resource.Offset));
    PRF(printf(" %9I64X", s.Resource.PackSize));
    PRF(printf(" %9I64X", s.Resource.UnpackSize));
    PRF(printf(" %d", s.RefCount));
    
    if (s.PartNumber != h.PartNumber)
      continue;

    if (s.Resource.IsSolid())
    {
      s.Resource.KeepSolid = keepSolid;
      keepSolid = true;
    }
    else
    {
      s.Resource.KeepSolid = false;
      keepSolid = false;
    }

    if (!s.Resource.IsMetadata())
      db.DataStreams.AddInReserved(s);
    else
    {
      if (s.Resource.IsSolid())
        return E_NOTIMPL;
      if (s.RefCount == 0)
      {
        // some wims have such (deleted?) metadata stream.
        // examples: boot.wim in VistaBeta2, WinPE.wim from WAIK.
        // db.DataStreams.Add(s);
        // we can show these delete images, if we comment "continue" command;
        continue;
      }
      
      if (s.RefCount > 1)
      {
        return S_FALSE;
        // s.RefCount--;
        // db.DataStreams.Add(s);
      }

      db.MetaStreams.Add(s);
    }
  }
  
  PRF(printf("\n"));
  
  return S_OK;
}


HRESULT CDatabase::OpenXml(IInStream *inStream, const CHeader &h, CByteBuffer &xml)
{
  CUnpacker unpacker;
  return unpacker.UnpackData(inStream, h.XmlResource, h, this, xml, NULL);
}

static void SetRootNames(CImage &image, unsigned value)
{
  wchar_t temp[16];
  ConvertUInt32ToString(value, temp);
  image.RootName = temp;
  image.RootNameBuf.Alloc(image.RootName.Len() * 2 + 2);
  Byte *p = image.RootNameBuf;
  unsigned len = image.RootName.Len() + 1;
  for (unsigned k = 0; k < len; k++)
  {
    p[k * 2] = (Byte)temp[k];
    p[k * 2 + 1] = 0;
  }
}


HRESULT CDatabase::Open(IInStream *inStream, const CHeader &h, unsigned numItemsReserve, IArchiveOpenCallback *openCallback)
{
  OpenCallback = openCallback;
  IsOldVersion = h.IsOldVersion();
  IsOldVersion9 = (h.Version == 0x10900);

  RINOK(ReadStreams(inStream, h, *this));

  bool needBootMetadata = !h.MetadataResource.IsEmpty();
  unsigned numNonDeletedImages = 0;

  CUnpacker unpacker;

  FOR_VECTOR (i, MetaStreams)
  {
    const CStreamInfo &si = MetaStreams[i];

    if (h.PartNumber != 1 || si.PartNumber != h.PartNumber)
      continue;

    const int userImage = Images.Size() + GetStartImageIndex();
    CImage &image = Images.AddNew();
    SetRootNames(image, userImage);
    
    CByteBuffer &metadata = image.Meta;
    Byte hash[kHashSize];
    
    RINOK(unpacker.UnpackData(inStream, si.Resource, h, this, metadata, hash));
   
    if (memcmp(hash, si.Hash, kHashSize) != 0 &&
        !(h.IsOldVersion() && IsEmptySha(si.Hash)))
      return S_FALSE;
    
    image.NumEmptyRootItems = 0;
    
    if (Items.IsEmpty())
      Items.ClearAndReserve(numItemsReserve);

    RINOK(ParseImageDirs(metadata, -1));
    
    if (needBootMetadata)
    {
      bool sameRes = (h.MetadataResource.Offset == si.Resource.Offset);
      if (sameRes)
        needBootMetadata = false;
      if (h.IsNewVersion())
      {
        if (si.RefCount == 1)
        {
          numNonDeletedImages++;
          bool isBootIndex = (h.BootIndex == numNonDeletedImages);
          if (sameRes && !isBootIndex)
            return S_FALSE;
          if (isBootIndex && !sameRes)
            return S_FALSE;
        }
      }
    }
  }
  
  if (needBootMetadata)
    return S_FALSE;
  return S_OK;
}


bool CDatabase::ItemHasStream(const CItem &item) const
{
  if (item.ImageIndex < 0)
    return true;
  const Byte *meta = Images[item.ImageIndex].Meta + item.Offset;
  if (IsOldVersion)
  {
    // old wim use same field for file_id and dir_offset;
    if (item.IsDir)
      return false;
    meta += (item.IsAltStream ? 0x8 : 0x10);
    UInt32 id = GetUi32(meta);
    return id != 0;
  }
  meta += (item.IsAltStream ? 0x10 : 0x40);
  return !IsEmptySha(meta);
}


#define RINOZ(x) { int __tt = (x); if (__tt != 0) return __tt; }

static int CompareStreamsByPos(const CStreamInfo *p1, const CStreamInfo *p2, void * /* param */)
{
  RINOZ(MyCompare(p1->PartNumber, p2->PartNumber));
  RINOZ(MyCompare(p1->Resource.Offset, p2->Resource.Offset));
  return MyCompare(p1->Resource.PackSize, p2->Resource.PackSize);
}

static int CompareIDs(const unsigned *p1, const unsigned *p2, void *param)
{
  const CStreamInfo *streams = (const CStreamInfo *)param;
  return MyCompare(streams[*p1].Id, streams[*p2].Id);
}

static int CompareHashRefs(const unsigned *p1, const unsigned *p2, void *param)
{
  const CStreamInfo *streams = (const CStreamInfo *)param;
  return memcmp(streams[*p1].Hash, streams[*p2].Hash, kHashSize);
}

static int FindId(const CStreamInfo *streams, const CUIntVector &sorted, UInt32 id)
{
  unsigned left = 0, right = sorted.Size();
  while (left != right)
  {
    unsigned mid = (left + right) / 2;
    unsigned streamIndex = sorted[mid];
    UInt32 id2 = streams[streamIndex].Id;
    if (id == id2)
      return streamIndex;
    if (id < id2)
      right = mid;
    else
      left = mid + 1;
  }
  return -1;
}

static int FindHash(const CStreamInfo *streams, const CUIntVector &sorted, const Byte *hash)
{
  unsigned left = 0, right = sorted.Size();
  while (left != right)
  {
    unsigned mid = (left + right) / 2;
    unsigned streamIndex = sorted[mid];
    const Byte *hash2 = streams[streamIndex].Hash;
    unsigned i;
    for (i = 0; i < kHashSize; i++)
      if (hash[i] != hash2[i])
        break;
    if (i == kHashSize)
      return streamIndex;
    if (hash[i] < hash2[i])
      right = mid;
    else
      left = mid + 1;
  }
  return -1;
}

static int CompareItems(const unsigned *a1, const unsigned *a2, void *param)
{
  const CRecordVector<CItem> &items = ((CDatabase *)param)->Items;
  const CItem &i1 = items[*a1];
  const CItem &i2 = items[*a2];

  if (i1.IsDir != i2.IsDir)
    return i1.IsDir ? -1 : 1;
  if (i1.IsAltStream != i2.IsAltStream)
    return i1.IsAltStream ? 1 : -1;
  RINOZ(MyCompare(i1.StreamIndex, i2.StreamIndex));
  RINOZ(MyCompare(i1.ImageIndex, i2.ImageIndex));
  return MyCompare(i1.Offset, i2.Offset);
}


HRESULT CDatabase::FillAndCheck(const CObjectVector<CVolume> &volumes)
{
  CUIntVector sortedByHash;
  sortedByHash.Reserve(DataStreams.Size());
  {
    CByteBuffer sizesBuf;

    for (unsigned iii = 0; iii < DataStreams.Size();)
    {
      {
        const CResource &r = DataStreams[iii].Resource;
        if (!r.IsSolid())
        {
          sortedByHash.AddInReserved(iii++);
          continue;
        }
      }

      UInt64 solidRunOffset = 0;
      unsigned k;
      unsigned numSolidsStart = Solids.Size();

      for (k = iii; k < DataStreams.Size(); k++)
      {
        CStreamInfo &si = DataStreams[k];
        CResource &r = si.Resource;

        if (!r.IsSolid())
          break;
        if (!r.KeepSolid && k != iii)
          break;

        if (r.Flags != NResourceFlags::kSolid)
          return S_FALSE;

        if (!r.IsSolidBig())
          continue;

        if (!si.IsEmptyHash())
          return S_FALSE;
        if (si.RefCount != 1)
          return S_FALSE;

        r.SolidIndex = Solids.Size();

        CSolid &ss = Solids.AddNew();
        ss.StreamIndex = k;
        ss.SolidOffset = solidRunOffset;
        {
          const size_t kSolidHeaderSize = 8 + 4 + 4;
          Byte header[kSolidHeaderSize];

          if (si.PartNumber >= volumes.Size())
            return S_FALSE;

          const CVolume &vol = volumes[si.PartNumber];
          IInStream *inStream = vol.Stream;
          RINOK(inStream->Seek(r.Offset, STREAM_SEEK_SET, NULL));
          RINOK(ReadStream_FALSE(inStream, (Byte *)header, kSolidHeaderSize));
          
          ss.UnpackSize = GetUi64(header);

          if (ss.UnpackSize > ((UInt64)1 << 63))
            return S_FALSE;

          solidRunOffset += ss.UnpackSize;
          if (solidRunOffset < ss.UnpackSize)
            return S_FALSE;

          const UInt32 solidChunkSize = GetUi32(header + 8);
          int log = GetLog(solidChunkSize);
          if (log < 8 || log > 31)
            return S_FALSE;
          ss.ChunkSizeBits = log;
          ss.Method = GetUi32(header + 12);
          
          UInt64 numChunks64 = (ss.UnpackSize + (((UInt32)1 << ss.ChunkSizeBits) - 1)) >> ss.ChunkSizeBits;
          UInt64 sizesBufSize64 = 4 * numChunks64;
          ss.HeadersSize = kSolidHeaderSize + sizesBufSize64;
          size_t sizesBufSize = (size_t)sizesBufSize64;
          if (sizesBufSize != sizesBufSize64)
            return E_OUTOFMEMORY;
          sizesBuf.AllocAtLeast(sizesBufSize);
          
          RINOK(ReadStream_FALSE(inStream, sizesBuf, sizesBufSize));
          
          size_t numChunks = (size_t)numChunks64;
          ss.Chunks.Alloc(numChunks + 1);

          UInt64 offset = 0;
          
          size_t c;
          for (c = 0; c < numChunks; c++)
          {
            ss.Chunks[c] = offset;
            UInt32 packSize = GetUi32((const Byte *)sizesBuf + c * 4);
            offset += packSize;
            if (offset < packSize)
              return S_FALSE;
          }
          ss.Chunks[c] = offset;

          if (ss.Chunks[0] != 0)
            return S_FALSE;
          if (ss.HeadersSize + offset != r.PackSize)
            return S_FALSE;
        }
      }
      
      unsigned solidLim = k;

      for (k = iii; k < solidLim; k++)
      {
        CStreamInfo &si = DataStreams[k];
        CResource &r = si.Resource;

        if (!r.IsSolidSmall())
          continue;

        if (si.IsEmptyHash())
          return S_FALSE;

        unsigned solidIndex;
        {
          UInt64 offset = r.Offset;
          for (solidIndex = numSolidsStart;; solidIndex++)
          {
            if (solidIndex == Solids.Size())
              return S_FALSE;
            UInt64 unpackSize = Solids[solidIndex].UnpackSize;
            if (offset < unpackSize)
              break;
            offset -= unpackSize;
          }
        }
        CSolid &ss = Solids[solidIndex];
        if (r.Offset < ss.SolidOffset)
          return S_FALSE;
        UInt64 relat = r.Offset - ss.SolidOffset;
        if (relat > ss.UnpackSize)
          return S_FALSE;
        if (r.PackSize > ss.UnpackSize - relat)
          return S_FALSE;
        r.SolidIndex = solidIndex;
        if (ss.FirstSmallStream < 0)
          ss.FirstSmallStream = k;

        sortedByHash.AddInReserved(k);
        // ss.NumRefs++;
      }
      
      iii = solidLim;
    }
  }

  if (Solids.IsEmpty())
  {
    /* We want to check that streams layout is OK.
       So we need resources sorted by offset.
       Another code can work with non-sorted streams.
       NOTE: all WIM programs probably create wim archives with
         sorted data streams. So it doesn't call Sort() here. */
       
    {
      unsigned i;
      for (i = 1; i < DataStreams.Size(); i++)
      {
        const CStreamInfo &s0 = DataStreams[i - 1];
        const CStreamInfo &s1 = DataStreams[i];
        if (s0.PartNumber < s1.PartNumber) continue;
        if (s0.PartNumber > s1.PartNumber) break;
        if (s0.Resource.Offset < s1.Resource.Offset) continue;
        if (s0.Resource.Offset > s1.Resource.Offset) break;
        if (s0.Resource.PackSize > s1.Resource.PackSize) break;
      }
      
      if (i < DataStreams.Size())
      {
        // return E_FAIL;
        DataStreams.Sort(CompareStreamsByPos, NULL);
      }
    }

    for (unsigned i = 1; i < DataStreams.Size(); i++)
    {
      const CStreamInfo &s0 = DataStreams[i - 1];
      const CStreamInfo &s1 = DataStreams[i];
      if (s0.PartNumber == s1.PartNumber)
        if (s0.Resource.GetEndLimit() > s1.Resource.Offset)
          return S_FALSE;
    }
  }
  
  {
    {
      const CStreamInfo *streams = &DataStreams.Front();

      if (IsOldVersion)
      {
        sortedByHash.Sort(CompareIDs, (void *)streams);
        
        for (unsigned i = 1; i < sortedByHash.Size(); i++)
          if (streams[sortedByHash[i - 1]].Id >=
              streams[sortedByHash[i]].Id)
            return S_FALSE;
      }
      else
      {
        sortedByHash.Sort(CompareHashRefs, (void *)streams);

        if (!sortedByHash.IsEmpty())
        {
          if (IsEmptySha(streams[sortedByHash[0]].Hash))
            HeadersError = true;
          
          for (unsigned i = 1; i < sortedByHash.Size(); i++)
            if (memcmp(
                streams[sortedByHash[i - 1]].Hash,
                streams[sortedByHash[i]].Hash,
                kHashSize) >= 0)
              return S_FALSE;
        }
      }
    }
    
    FOR_VECTOR (i, Items)
    {
      CItem &item = Items[i];
      item.StreamIndex = -1;
      const Byte *hash = Images[item.ImageIndex].Meta + item.Offset;
      if (IsOldVersion)
      {
        if (!item.IsDir)
        {
          hash += (item.IsAltStream ? 0x8 : 0x10);
          UInt32 id = GetUi32(hash);
          if (id != 0)
            item.StreamIndex = FindId(&DataStreams.Front(), sortedByHash, id);
        }
      }
      /*
      else if (item.IsDir)
      {
        // reparse points can have dirs some dir
      }
      */
      else
      {
        hash += (item.IsAltStream ? 0x10 : 0x40);
        if (!IsEmptySha(hash))
        {
          item.StreamIndex = FindHash(&DataStreams.Front(), sortedByHash, hash);
        }
      }
    }
  }
  {
    CUIntVector refCounts;
    refCounts.ClearAndSetSize(DataStreams.Size());
    unsigned i;

    for (i = 0; i < DataStreams.Size(); i++)
    {
      UInt32 startVal = 0;
      // const CStreamInfo &s = DataStreams[i];
      /*
      if (s.Resource.IsMetadata() && s.PartNumber == 1)
        startVal = 1;
      */
      refCounts[i] = startVal;
    }
    
    for (i = 0; i < Items.Size(); i++)
    {
      int streamIndex = Items[i].StreamIndex;
      if (streamIndex >= 0)
        refCounts[streamIndex]++;
    }
    
    for (i = 0; i < DataStreams.Size(); i++)
    {
      const CStreamInfo &s = DataStreams[i];
      if (s.RefCount != refCounts[i]
          && !s.Resource.IsSolidBig())
      {
        /*
        printf("\ni=%5d  si.Ref=%2d  realRefs=%2d size=%8d offset=%8x id=%4d ",
          i, s.RefCount, refCounts[i], (unsigned)s.Resource.UnpackSize, (unsigned)s.Resource.Offset, s.Id);
        */
        RefCountError = true;
      }
      
      if (refCounts[i] == 0)
      {
        const CResource &r = DataStreams[i].Resource;
        if (!r.IsSolidBig() || Solids[r.SolidIndex].FirstSmallStream < 0)
        {
          CItem item;
          item.Offset = 0;
          item.StreamIndex = i;
          item.ImageIndex = -1;
          Items.Add(item);
          ThereAreDeletedStreams = true;
        }
      }
    }
  }

  return S_OK;
}


HRESULT CDatabase::GenerateSortedItems(int imageIndex, bool showImageNumber)
{
  SortedItems.Clear();
  VirtualRoots.Clear();
  IndexOfUserImage = imageIndex;
  NumExcludededItems = 0;
  ExludedItem = -1;

  if (Images.Size() != 1 && imageIndex < 0)
    showImageNumber = true;

  unsigned startItem = 0;
  unsigned endItem = 0;
  
  if (imageIndex < 0)
  {
    endItem = Items.Size();
    if (Images.Size() == 1)
    {
      IndexOfUserImage = 0;
      const CImage &image = Images[0];
      if (!showImageNumber)
        NumExcludededItems = image.NumEmptyRootItems;
    }
  }
  else if ((unsigned)imageIndex < Images.Size())
  {
    const CImage &image = Images[imageIndex];
    startItem = image.StartItem;
    endItem = startItem + image.NumItems;
    if (!showImageNumber)
      NumExcludededItems = image.NumEmptyRootItems;
  }
  
  if (NumExcludededItems != 0)
  {
    ExludedItem = startItem;
    startItem += NumExcludededItems;
  }

  unsigned num = endItem - startItem;
  SortedItems.ClearAndSetSize(num);
  unsigned i;
  for (i = 0; i < num; i++)
    SortedItems[i] = startItem + i;

  SortedItems.Sort(CompareItems, this);
  for (i = 0; i < SortedItems.Size(); i++)
    Items[SortedItems[i]].IndexInSorted = i;

  if (showImageNumber)
    for (i = 0; i < Images.Size(); i++)
    {
      CImage &image = Images[i];
      if (image.NumEmptyRootItems != 0)
        continue;
      image.VirtualRootIndex = VirtualRoots.Size();
      VirtualRoots.Add(i);
    }

  return S_OK;
}


static void IntVector_SetMinusOne_IfNeed(CIntVector &v, unsigned size)
{
  if (v.Size() == size)
    return;
  v.ClearAndSetSize(size);
  int *vals = &v[0];
  for (unsigned i = 0; i < size; i++)
    vals[i] = -1;
}


HRESULT CDatabase::ExtractReparseStreams(const CObjectVector<CVolume> &volumes, IArchiveOpenCallback *openCallback)
{
  ItemToReparse.Clear();
  ReparseItems.Clear();
  
  // we don't know about Reparse field for OLD WIM format
  if (IsOldVersion)
    return S_OK;

  CIntVector streamToReparse;
  CUnpacker unpacker;
  UInt64 totalPackedPrev = 0;

  FOR_VECTOR(indexInSorted, SortedItems)
  {
    // we use sorted items for faster access
    unsigned itemIndex = SortedItems[indexInSorted];
    const CItem &item = Items[itemIndex];
    
    if (!item.HasMetadata() || item.IsAltStream)
      continue;
    
    if (item.ImageIndex < 0)
      continue;
    
    const Byte *metadata = Images[item.ImageIndex].Meta + item.Offset;
    
    const UInt32 attrib = Get32(metadata + 8);
    if ((attrib & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
      continue;
    
    if (item.StreamIndex < 0)
      continue; // it's ERROR
    
    const CStreamInfo &si = DataStreams[item.StreamIndex];
    if (si.Resource.UnpackSize >= (1 << 16))
      continue; // reparse data can not be larger than 64 KB

    IntVector_SetMinusOne_IfNeed(streamToReparse, DataStreams.Size());
    IntVector_SetMinusOne_IfNeed(ItemToReparse, Items.Size());
    
    const unsigned offset = 0x58; // we don't know about Reparse field for OLD WIM format
    UInt32 tag = Get32(metadata + offset);
    int reparseIndex = streamToReparse[item.StreamIndex];
    CByteBuffer buf;

    if (openCallback)
    {
      if ((unpacker.TotalPacked - totalPackedPrev) >= ((UInt32)1 << 16))
      {
        UInt64 numFiles = Items.Size();
        RINOK(openCallback->SetCompleted(&numFiles, &unpacker.TotalPacked));
        totalPackedPrev = unpacker.TotalPacked;
      }
    }

    if (reparseIndex >= 0)
    {
      const CByteBuffer &reparse = ReparseItems[reparseIndex];
      if (tag == Get32(reparse))
      {
        ItemToReparse[itemIndex] = reparseIndex;
        continue;
      }
      buf = reparse;
      // we support that strange and unusual situation with different tags and same reparse data.
    }
    else
    {
      /*
      if (si.PartNumber >= volumes.Size())
        continue;
      */
      const CVolume &vol = volumes[si.PartNumber];
      /*
      if (!vol.Stream)
        continue;
      */
      
      Byte digest[kHashSize];
      HRESULT res = unpacker.UnpackData(vol.Stream, si.Resource, vol.Header, this, buf, digest);

      if (res == S_FALSE)
        continue;

      RINOK(res);
      
      if (memcmp(digest, si.Hash, kHashSize) != 0
        // && !(h.IsOldVersion() && IsEmptySha(si.Hash))
        )
      {
        // setErrorStatus;
        continue;
      }
    }
    
    CByteBuffer &reparse = ReparseItems.AddNew();
    reparse.Alloc(8 + buf.Size());
    Byte *dest = (Byte *)reparse;
    SetUi32(dest, tag);
    SetUi32(dest + 4, (UInt32)buf.Size());
    if (buf.Size() != 0)
      memcpy(dest + 8, buf, buf.Size());
    ItemToReparse[itemIndex] = ReparseItems.Size() - 1;
  }

  return S_OK;
}



static bool ParseNumber64(const AString &s, UInt64 &res)
{
  const char *end;
  if (s.IsPrefixedBy("0x"))
  {
    if (s.Len() == 2)
      return false;
    res = ConvertHexStringToUInt64(s.Ptr(2), &end);
  }
  else
  {
    if (s.IsEmpty())
      return false;
    res = ConvertStringToUInt64(s, &end);
  }
  return *end == 0;
}


static bool ParseNumber32(const AString &s, UInt32 &res)
{
  UInt64 res64;
  if (!ParseNumber64(s, res64) || res64 >= ((UInt64)1 << 32))
    return false;
  res = (UInt32)res64;
  return true;
}


static bool ParseTime(const CXmlItem &item, FILETIME &ft, const char *tag)
{
  int index = item.FindSubTag(tag);
  if (index >= 0)
  {
    const CXmlItem &timeItem = item.SubItems[index];
    UInt32 low = 0, high = 0;
    if (ParseNumber32(timeItem.GetSubStringForTag("LOWPART"), low) &&
        ParseNumber32(timeItem.GetSubStringForTag("HIGHPART"), high))
    {
      ft.dwLowDateTime = low;
      ft.dwHighDateTime = high;
      return true;
    }
  }
  return false;
}


void CImageInfo::Parse(const CXmlItem &item)
{
  CTimeDefined = ParseTime(item, CTime, "CREATIONTIME");
  MTimeDefined = ParseTime(item, MTime, "LASTMODIFICATIONTIME");
  NameDefined = true;
  ConvertUTF8ToUnicode(item.GetSubStringForTag("NAME"), Name);

  ParseNumber64(item.GetSubStringForTag("DIRCOUNT"), DirCount);
  ParseNumber64(item.GetSubStringForTag("FILECOUNT"), FileCount);
  IndexDefined = ParseNumber32(item.GetPropVal("INDEX"), Index);
}

void CWimXml::ToUnicode(UString &s)
{
  size_t size = Data.Size();
  if (size < 2 || (size & 1) != 0 || size > (1 << 24))
    return;
  const Byte *p = Data;
  if (Get16(p) != 0xFEFF)
    return;
  wchar_t *chars = s.GetBuf((unsigned)(size / 2));
  for (size_t i = 2; i < size; i += 2)
  {
    wchar_t c = Get16(p + i);
    if (c == 0)
      break;
    *chars++ = c;
  }
  *chars = 0;
  s.ReleaseBuf_SetLen((unsigned)(chars - (const wchar_t *)s));
}


bool CWimXml::Parse()
{
  IsEncrypted = false;
  AString utf;
  {
    UString s;
    ToUnicode(s);
    // if (!ConvertUnicodeToUTF8(s, utf)) return false;
    ConvertUnicodeToUTF8(s, utf);
  }

  if (!Xml.Parse(utf))
    return false;
  if (Xml.Root.Name != "WIM")
    return false;

  FOR_VECTOR (i, Xml.Root.SubItems)
  {
    const CXmlItem &item = Xml.Root.SubItems[i];
    
    if (item.IsTagged("IMAGE"))
    {
      CImageInfo imageInfo;
      imageInfo.Parse(item);
      if (!imageInfo.IndexDefined)
        return false;

      if (imageInfo.Index != (UInt32)Images.Size() + 1)
      {
        // old wim (1.09) uses zero based image index
        if (imageInfo.Index != (UInt32)Images.Size())
          return false;
      }

      imageInfo.ItemIndexInXml = i;
      Images.Add(imageInfo);
    }

    if (item.IsTagged("ESD"))
    {
      FOR_VECTOR (k, item.SubItems)
      {
        const CXmlItem &item2 = item.SubItems[k];
        if (item2.IsTagged("ENCRYPTED"))
          IsEncrypted = true;
      }
    }
  }

  return true;
}

}}

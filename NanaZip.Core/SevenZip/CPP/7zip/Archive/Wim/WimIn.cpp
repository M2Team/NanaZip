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

// #include "../../Common/LimitedStreams.h"
#include "../../Common/StreamObjects.h"
#include "../../Common/StreamUtils.h"

#include "../../Compress/XpressDecoder.h"

#include "../Common/OutStreamWithSha1.h"

#include "WimIn.h"

// #define Z7_WIM_SHOW_DELETED_IMAGES // for debug : define to show deleted images in some old WIMs

#define Get16(p) GetUi16a(p)  // aligned
#define Get32(p) GetUi32a(p)  // aligned
#define Get64(p) GetUi64(p)   // unaligned

#if defined(Z7_MSC_VER_ORIGINAL)
#define PRINT_UI64(val)  PRF(printf(" %9I64x", (UInt64)(val)));
#else
#define PRINT_UI64(val)  PRF(printf(" %9llx", (unsigned long long)(val)));
#endif

namespace NArchive {
namespace NWim {

// (data) is 4-bytes aligned
#define IsEmptySha(data)  IsEmptySha1_32((const UInt32 *)(const void *)(data))

#if 1 // 0 : for debug
Z7_FORCE_INLINE
static int COMPARE_HASHES(const UInt32 * const a, const UInt32 * const b)
{
  Z7_WIM_SHA1_UI32_COMPARE_LESS_EQUAL_GREATER(a, b,
      return -1; ,
      return  0; ,
      return  1; )
}
#else
#define COMPARE_HASHES(a, b)  memcmp(a, b, kHashSize)
#endif

static bool inline GetLog_val_min_dest(const UInt32 val, unsigned i, unsigned &dest)
{
  UInt32 v = (UInt32)1 << i;
  for (; i < 32; i++)
  {
    if (v == val)
    {
      dest = i;
      return true;
    }
    v += v;
  }
  return false;
}


// (outSize <= (1u << chunkSizeBits))
// if (method == 0), it returns E_NOTIMPL
HRESULT CUnpacker::UnpackChunk(const unsigned method, const unsigned chunkSizeBits,
    const size_t inSize, const size_t outSize,
    ISequentialInStream *inStream, ISequentialOutStream *outStream)
{
  if (method == NMethod::kXPRESS)
  {
    if (chunkSizeBits < 12 || chunkSizeBits > 16)
      return E_NOTIMPL;
  }
  else
  {
    if (chunkSizeBits < 15)
      return E_NOTIMPL;
    if (method == NMethod::kLZX)
    {
      // chunkSizeBits <= 15 : MS
      // chunkSizeBits <= 21 : wimlib
      if (chunkSizeBits > 21)
        return E_NOTIMPL;
    }
    else if (method == NMethod::kLZMS)
    {
      // chunkSizeBits <= 26 : MS
      // chunkSizeBits <= 30 : wimlib
      if (chunkSizeBits > 30)
        return E_NOTIMPL;
    }
    else
      return E_NOTIMPL;
  }

  const size_t chunkSize = (size_t)1 << chunkSizeBits;
  {
    const unsigned
        kAdditionalOutputBufSize = MyMax(NCompress::NLzx::
        kAdditionalOutputBufSize,        NCompress::NXpress::
        kAdditionalOutputBufSize);
    unpackBuf.EnsureCapacity(chunkSize + kAdditionalOutputBufSize);
    if (!unpackBuf.Data)
      return E_OUTOFMEMORY;
  }
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
    const unsigned kAdditionalInputSize = 32;
    packBuf.EnsureCapacity(chunkSize + kAdditionalInputSize);
    if (!packBuf.Data)
      return E_OUTOFMEMORY;
    RINOK(ReadStream_FALSE(inStream, packBuf.Data, inSize))
    memset(packBuf.Data + inSize, 0xff, kAdditionalInputSize);

    TotalPacked += inSize;
    
    if (method == NMethod::kXPRESS)
    {
      res = NCompress::NXpress::Decode_WithExceedWrite(packBuf.Data, inSize, unpackBuf.Data, outSize);
      if (res == S_OK)
        unpackedSize = outSize;
    }
    else if (method == NMethod::kLZX)
    {
      lzxDecoder.Create_if_Empty();
      lzxDecoder->Set_WimMode(true);
      res = lzxDecoder->Set_ExternalWindow_DictBits(unpackBuf.Data, chunkSizeBits);
      if (res != S_OK)
        return E_NOTIMPL;
      lzxDecoder->Set_KeepHistoryForNext(false);
      lzxDecoder->Set_KeepHistory(false);
      res = lzxDecoder->Code_WithExceedReadWrite(packBuf.Data, inSize, (UInt32)outSize);
      unpackedSize = lzxDecoder->GetUnpackSize();
      if (res == S_OK && !lzxDecoder->WasBlockFinished())
        res = S_FALSE;
    }
    else
    {
      lzmsDecoder.Create_if_Empty();
      res = lzmsDecoder->Code(packBuf.Data, inSize, unpackBuf.Data, outSize);
      unpackedSize = lzmsDecoder->GetUnpackSize();
    }
  }
  
  if (unpackedSize != outSize)
  {
    if (res == S_OK)
      res = S_FALSE;
    if (unpackedSize > outSize)
      res = S_FALSE; // is not expected
    else
      memset(unpackBuf.Data + unpackedSize, 0, outSize - unpackedSize);
  }
  
  if (outStream)
    RINOK(WriteStream(outStream, unpackBuf.Data, outSize))
  return res;
}


HRESULT CUnpacker::Unpack2(const CResource &resource,
    const CHeader &header, const CDatabase *db,
    IInStream *inStream, ISequentialOutStream *outStream, ICompressProgressInfo *progress)
{
  if (resource.IsSolid())
  {
    if (resource.Flags != NResourceFlags::kSolid
        || resource.SolidIndex < 0 || !db)
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
        return S_FALSE; // unexpected
      offs -= ss.SolidOffset;
      if (offs > ss.UnpackSize)
        return S_FALSE; // unexpected
      rem = resource.PackSize;
      if (rem > ss.UnpackSize - offs)
        return S_FALSE; // small solid block crosses the end of big solid block
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
      RINOK(WriteStream(outStream, unpackBuf.Data + offsetInChunk, cur))
      outProcessed += cur;
      rem -= cur;
      offsetInChunk = 0;
      chunkIndex++;
    }
    
    for (;;)
    {
      if (rem == 0)
        return S_OK;
    
      const UInt64 offset = ss.Chunks[chunkIndex];
      const size_t packSize = (size_t)(ss.Chunks[(size_t)chunkIndex + 1] - offset);
      const CResource &rs = db->DataStreams[ss.StreamIndex].Resource;
      RINOK(InStream_SeekSet(inStream, rs.Offset + offset))
      
      size_t cur = chunkSize;
      const UInt64 unpackRem = ss.UnpackSize - ((UInt64)chunkIndex << chunkSizeBits);
      if (cur > unpackRem)
        cur = (size_t)unpackRem;
      
      _solidIndex = -1;
      _unpackedChunkIndex = 0;
      
      const HRESULT res = UnpackChunk((unsigned)ss.Method, chunkSizeBits, packSize, cur, inStream, NULL);
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
      
      RINOK(WriteStream(outStream, unpackBuf.Data + offsetInChunk, cur))
      
      if (progress)
      {
        packProcessed += packSize;
        outProcessed += cur;
        RINOK(progress->SetRatioInfo(&packProcessed, &outProcessed))
      }
      rem -= cur;
      offsetInChunk = 0;
      chunkIndex++;
    }
  }

  // ---------- NON-Solid ----------

  if (!resource.IsCompressed())
  {
    if (resource.PackSize != resource.UnpackSize)
      return S_FALSE;
    copyCoder.Create_if_Empty();
    RINOK(InStream_SeekSet(inStream, resource.Offset))
    TotalPacked += resource.PackSize;
#if 1
    HRESULT res = copyCoder.Interface()->Code(inStream, outStream, NULL, &resource.UnpackSize, progress);
#else
    CMyComPtr2_Create<ISequentialInStream, CLimitedSequentialInStream> limitedStream;
    limitedStream->SetStream(inStream);
    limitedStream->Init(resource.PackSize);
    HRESULT res = copyCoder.Interface()->Code(limitedStream, outStream, NULL, NULL, progress);
#endif
    if (res == S_OK && copyCoder->TotalSize != resource.UnpackSize)
      res = S_FALSE;
    return res;
  }
  
  // ---------- NON-Solid Compressed ----------

  const UInt64 unpackSize = resource.UnpackSize;
  if (unpackSize == 0)
  {
    if (resource.PackSize == 0)
      return S_OK;
    return S_FALSE;
  }
  if (unpackSize >= ((UInt64)1 << 63))
    return E_NOTIMPL;

  const unsigned chunkSizeBits = header.ChunkSizeBits;
  const unsigned entrySizeShifts = (unpackSize < ((UInt64)1 << 32) ? 2 : 3);
  // (num_table_entries == num_chunks - 1) because there is no table entry for last chunk.
  const UInt64 tableSize = (unpackSize - 1) >> chunkSizeBits << entrySizeShifts;
  if (tableSize > resource.PackSize)
    return S_FALSE;

  UInt64 outProcessed = 0, packOffset = 0;

  for (;;)
  {
    // ---------- Read Pack Offset Table ----------
    size_t bufSize;
    {
      // if (outProcessed >= unpackSize || outProcessed % ((UInt32)1 << chunkSizeBits)) return E_FAIL;
      const UInt64 remTable = (unpackSize - outProcessed - 1) >> chunkSizeBits << entrySizeShifts;
      bufSize = (size_t)8 << 17; // must be a multiple of 8. Use big value to reduce number of table seeks.
                // (size_t)8 << 0; // for debug : many table seeks.
      if (bufSize >= remTable)
        bufSize = (size_t)remTable;
      if (MemUsage + bufSize + ((UInt64)2 << chunkSizeBits) >= MemUsage_Limit)
        return E_OUTOFMEMORY;
      if (bufSize)
      {
        RINOK(InStream_SeekSet(inStream, resource.Offset +
            (outProcessed >> chunkSizeBits << entrySizeShifts)))
        sizesBuf.AllocAtLeast(bufSize);
        RINOK(ReadStream_FALSE(inStream, sizesBuf, bufSize))
      }
      RINOK(InStream_SeekSet(inStream, resource.Offset + tableSize + packOffset))
    }
    
    // we clear solid tags to disable cached data in unpackBuf
    _solidIndex = -1;
    _unpackedChunkIndex = 0;

    // ---------- Unpack Chunks ----------
    const Byte *p = (const Byte *)sizesBuf;
    for (;;)
    {
      UInt64 nextOffset = resource.PackSize - tableSize;
      size_t outSize;
      {
        const UInt64 rem = unpackSize - outProcessed;
        if (rem == 0)
          return S_OK;
        outSize = (size_t)1 << chunkSizeBits;
        if (outSize >= rem)  // we use >=, because (rem == (1 << chunkSizeBits)) is last chunk also
          outSize = (size_t)rem; // last chunk of file
        else
        {
          if ((size_t)(p - sizesBuf) >= bufSize)
            break; // we need new table records
          if (entrySizeShifts == 2)
            { nextOffset = GetUi32a(p);  p += 4; }
          else
            { nextOffset = GetUi64a(p);  p += 8; }
        }
      }
      if (nextOffset < packOffset)
        return S_FALSE;
      const UInt64 inSize64 = nextOffset - packOffset;
      const size_t inSize = (size_t)inSize64;
      if (inSize != inSize64)
        return S_FALSE;
      if (progress)
        RINOK(progress->SetRatioInfo(&packOffset, &outProcessed))
      // if UnpackChunk() returns S_OK, we don't need additional Seek:
      // RINOK(InStream_SeekSet(inStream, resource.Offset + tableSize + packOffset)) // optional
      packOffset = nextOffset;
      outProcessed += outSize;
      RINOK(UnpackChunk(header.Method, chunkSizeBits, inSize, outSize, inStream, outStream))
    }
  }
}


HRESULT CUnpacker::Unpack(IInStream *inStream, const CResource &resource, const CHeader &header, const CDatabase *db,
    ISequentialOutStream *outStream, ICompressProgressInfo *progress, UInt32 *digest)
{
  CMyComPtr2_Create<ISequentialOutStream, COutStreamWithSha1> shaStream;
  // outStream can be NULL, so we use COutStreamWithSha1 even if sha1 is not required
  shaStream->SetStream(outStream);
  shaStream->Init(digest != NULL);
  const HRESULT res = Unpack2(resource, header, db, inStream, shaStream, progress);
  if (digest)
    shaStream->Final((Byte *)(void *)digest);
  return res;
}


HRESULT CUnpacker::UnpackData(IInStream *inStream,
    const CResource &resource, const CHeader &header,
    const CDatabase *db,
    CByteBuffer &buf, UInt32 *digest)
{
  // if (resource.IsSolid()) return E_NOTIMPL;
  UInt64 unpackSize64 = resource.UnpackSize;
  if (!resource.IsCompressed() && !resource.IsSolid()
      && resource.PackSize != unpackSize64)
    return S_FALSE;
  if (db)
    unpackSize64 = db->Get_UnpackSize_of_Resource(resource);
  const size_t size = (size_t)unpackSize64;
  if (size != unpackSize64)
    return E_OUTOFMEMORY;
  buf.Alloc(size);

  CMyComPtr2_Create<ISequentialOutStream, CBufPtrSeqOutStream> outStream;
  outStream->Init((Byte *)buf, size);
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

#define UPDATE_MEM_USAGE_WITH_RESOURCE(resource) \
  { if (MemUsage_Limit - MemUsage < resource.UnpackSize) return E_OUTOFMEMORY; \
    MemUsage += resource.UnpackSize; }

#define UPDATE_MEM_USAGE(size) \
  { MemUsage += (size); \
    if (MemUsage > MemUsage_Limit) return E_OUTOFMEMORY; }


#define kLongPath "[LONG_PATH]" STRING_PATH_SEPARATOR "[LONG_PATH_ITEM]"

// (data) is aligned for 2-bytes
static bool CheckName_and_Fix_in_Meta(const Byte *data, unsigned numBytes)
{
  const Byte * const start = data;
  numBytes /= 2;
  if (numBytes)
    do
    {
      if (*(const UInt16 *)(const void *)data == 0)
      {
        SetUi16a(start - 2, (UInt16)(data - start))  // we fix name length in meta
        return false;
      }
      data += 2;
    }
    while (--numBytes);
  if (*(const UInt16 *)(const void *)data)
  {
    *(UInt16 *)(void *)data = 0;  // we fix null terminator in meta
    return false;
  }
  return true;
}

// (data) is aligned for 2-bytes
static void AllocateAndCopyName(const Byte *data, const unsigned numBytes, NWindows::NCOM::CPropVariant &name)
{
  const unsigned len = numBytes / 2; // Get_Ui16_String_Size(data, numBytes) / 2;
  wchar_t *s = name.AllocBstr(len);
  for (unsigned i = 0; i < len; i++)
  {
    *s++ = Get16(data);
    data += 2;
  }
  *s = 0;
}

// item.ImageIndex >= 0 && !item.IsAltStream
void CDatabase::GetShortName(const unsigned index, NWindows::NCOM::CPropVariant &name) const
{
  const CItem &item = Items[index];
  const CImage &image = Images[item.ImageIndex];
  if (image.NeedExcludeItemFromPath_parentIndex(item.Parent))
  {
    name.Clear();
    return;
  }
  const Byte *meta = image.Meta + item.Offset +
      (IsOldVersion ? kDirRecordSizeOld : kDirRecordSize);
  const unsigned len = Get16(meta - 4);
  const size_t fileNameLen = Get16(meta - 2);
  if (fileNameLen)
    meta += fileNameLen + 2;
  AllocateAndCopyName(meta, len, name);
}


// Items[index].ImageIndex >= 0
void CDatabase::GetItemName(const unsigned index, NWindows::NCOM::CPropVariant &name) const
{
  const CItem &item = Items[index];
  const CImage &image = Images[item.ImageIndex];
  if (image.NeedExcludeItemFromPath_parentIndex(item.Parent))
  {
    name = image.RootName;
    return;
  }
  const Byte *meta = image.Meta + GetNameOffset(item);
  const unsigned len = Get16(meta - 2);
  AllocateAndCopyName(meta, len, name);
}

// item.ImageIndex >= 0
void CDatabase::GetItemPath(const unsigned index1, const bool showImageNumber, NWindows::NCOM::CPropVariant &path) const
{
  unsigned size = 0;
  int index = (int)index1;
  const int imageIndex = Items[index].ImageIndex;
  const CImage &image = Images[imageIndex];
  
  unsigned newLevel = 0;
  bool needColon = false;

  for (;;)
  {
    const CItem &item = Items[index];
    if (item.DirLevel > (1 << 12))
    {
      path = kLongPath;
      return;
    }
    index = item.Parent;
    if (!image.NeedExcludeItemFromPath_parentIndex(index))
    {
      needColon = item.IsAltStream;
      const Byte *meta = image.Meta + GetNameOffset(item);
      size += Get16(meta - 2) / 2; // Get_Ui16_String_Size(meta, Get16(meta - 2)) / 2;
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

  index = (int)index1;
  wchar_t separator = 0;
  
  for (;;)
  {
    const CItem &item = Items[index];
    index = item.Parent;
    if (!image.NeedExcludeItemFromPath_parentIndex(index))
    {
      if (separator)
        s[--size] = separator;
      const Byte *meta = image.Meta + GetNameOffset(item);
      const size_t len = Get16(meta - 2) / 2; // Get_Ui16_String_Size(meta, Get16(meta - 2)) / 2;
      size -= (unsigned)len;
      wchar_t *dest = s + size;

      for (size_t i = 0; i < len; i++)
      {
        wchar_t c = Get16(meta + i * 2);
        if (c == L'/')
          c = L'_';
        #if WCHAR_PATH_SEPARATOR != L'/'
        else if (c == L'\\')
          c = WCHAR_IN_FILE_NAME_BACKSLASH_REPLACEMENT; // 22.00 : WSL scheme
        #endif
        dest[i] = c;
      }
    }
    if (index < 0)
      return;
    separator = item.IsAltStream ? L':' : WCHAR_PATH_SEPARATOR;
  }
}


static const unsigned k_DirRecord_FieldOffset_of_SubdirOffset = 0x10;
// old wim (IsOld) uses same field for FileId and SubdirOffset:
static const unsigned k_DirRecord_FieldOffset_of_FileId = k_DirRecord_FieldOffset_of_SubdirOffset; // for IsOld wim
static const unsigned k_AltRecord_FieldOffset_of_FileId = 8; // for IsOld wim

// wim 1.10- : root dir contains real items
// wim 1.12+ : root dir contains only one dir with empty name
// (pos <= DirSize) for good archives
HRESULT CDatabase::ParseDirItem(size_t pos, const int parent, const unsigned dirLevel)
{
  // if (dirLevel > (1 << 10)) return S_FALSE;
  CImage &image = Images.Back();
  const size_t align = GetDirAlignMask();
  if (pos & align)
    return S_FALSE;
  const unsigned numShifts = GetDirAlign_numShifts();

  for (unsigned numItems = 0;; numItems++)
  {
    if (OpenCallback && (Items.Size() & 0xFFFF) == 0)
    {
      const UInt64 numFiles = Items.Size();
      RINOK(OpenCallback->SetCompleted(&numFiles, NULL))
    }
    // if (pos & align) throw 1; // optional check
    // (pos & align) == 0
    const size_t rem = DirSize - pos;
    if (pos < DirStartOffset || pos > DirSize || rem < 8 /* || DirSize - DirProcessed < 8 */ )
      return S_FALSE;
    const Byte *p = DirData + pos;
    const UInt64 len64 = Get64(p);
    if (rem < len64 /* || DirSize - DirProcessed < len64 */ )
      return S_FALSE;
    const size_t len = (size_t)len64;
    if (len & align)
      return S_FALSE;

#define CHECK_USED_MAP(numBytes) \
    { size_t numChecks = (numBytes) >> numShifts; \
      Byte *used = _useMap + (pos >> numShifts); \
      do { if (*used) return S_FALSE; \
        *used++ = 1; \
      } while (--numChecks); \
    }
    CHECK_USED_MAP(len < 8 ? 8 : len)
    if (len == 0)
    {
      // DirProcessed += 8;
      return S_OK;
    }
    // DirProcessed += len;
    const size_t dirRecordSize = IsOldVersion ? kDirRecordSizeOld : kDirRecordSize;
    if (len < dirRecordSize)
      return S_FALSE;

    CItem item;
    item.Construct();
    const UInt32 attrib = Get32(p + 8);
    {
      const UInt32 securId = Get32(p + 0xC) + 1;
      if (securId && securId >= image.SecurOffsets.Size())
        HeadersError = true;
    }
    size_t subdirOffset = 0;
    item.IsDir = (attrib & FILE_ATTRIBUTE_DIRECTORY) != 0;
    item.IsDir_NonReparse =
        (attrib & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
        (attrib & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
    if (!IsOldVersion || item.IsDir_NonReparse)
    {
      const UInt64 subdirOffset64 = Get64(p + k_DirRecord_FieldOffset_of_SubdirOffset);
      if (subdirOffset64 > DirSize)
        return S_FALSE;
      subdirOffset = (size_t)subdirOffset64;
      if (!item.IsDir_NonReparse && subdirOffset)
        HeadersError = true;
    }
    p += dirRecordSize;
    const unsigned numAltStreams = Get16(p - 6);
    const size_t shortNameLen = Get16(p - 4);
    size_t fileNameLen = Get16(p - 2);
    if ((shortNameLen & 1) || (fileNameLen & 1))
      return S_FALSE;
    const size_t fileNameLen2 = (fileNameLen == 0 ? fileNameLen : fileNameLen + 2);
    {
      // imagex writes additional 2-bytes NUL character at the end by some reason.
      // dism doesn't write additional bytes.
      const size_t namesSum = fileNameLen2 + (shortNameLen == 0 ? shortNameLen : shortNameLen + 2);
      size_t end = dirRecordSize + namesSum;
      if (end > len)
        return S_FALSE;
#if 1 // 1 : optional strict check of padding data
      // WIMGAPI and wimlib can store extra records after aligned padding.
      // So we test padding data only up to nearest aligned range:
      for (const Byte *p2 = p + namesSum; end & align; end++)
        if (*p2++)
          HeadersError = true; // optional check of padding
#endif
    }
    /* if (shortNameLen == 0 && fileNameLen == 0) { then
          dirRecordSize == 62 or 102, and there are at least 2 bytes of padding
    */
    if (!CheckName_and_Fix_in_Meta(p, (unsigned)fileNameLen)
        || (fileNameLen == 0 && shortNameLen != 0))
    {
      HeadersError = true;
      /* position of shortName depends from fileNameLen in meta record.
         if (fileNameLen == 0 && shortNameLen != 0)
            then fileName and shortName are at same address.
         We clear shortName: */
      *(UInt16 *)(void *)(p - 4) = 0; // clear shortNameLen
      fileNameLen = Get16(p - 2); // reload fileNameLen after file name correction
    }
    else if (shortNameLen)
    {
      Byte * const data = (Byte *)(void *)(p + fileNameLen2);
      size_t i = 0;
      for (;;)
      {
        if (*(const UInt16 *)(const void *)(data + i) == 0)
        {
          // return S_FALSE;
          SetUi16a(p - 4, (UInt16)i)  // we fix shortNameLen in meta
          HeadersError = true;
          break;
        }
        i += 2;
        if (i >= shortNameLen)
        {
          if (*(const UInt16 *)(const void *)(data + i))
          {
            // return S_FALSE;
            *(UInt16 *)(void *)(data + i) = 0;
            HeadersError = true;
          }
          break;
        }
      }
    }
    
    // PRF(printf("\n%S", p));
    UPDATE_MEM_USAGE (sizeof(CItem) * 5 / 4)
    item.Offset = pos;
    item.Parent = parent;
    item.DirLevel = dirLevel;
    item.ImageIndex = (int)Images.Size() - 1;
    const unsigned prevIndex = Items.Add(item);

    pos += len;

    for (unsigned i = 0; i < numAltStreams; i++)
    {
      const size_t rem2 = DirSize - pos;
      if (rem2 < 8)
        return S_FALSE;
      const Byte * const p2 = DirData + pos;
      const UInt64 len2_64 = Get64(p2);
      if (rem2 < len2_64 /* || DirSize - DirProcessed < len2_64 */ )
        return S_FALSE;
      const size_t len2 = (size_t)len2_64;
      if (len2 & align)
        return S_FALSE;
      const size_t extraOffset = (IsOldVersion ? 0x12u : 0x26u);
      if (len2 < extraOffset)
        return S_FALSE;
      // DirProcessed += len2;
      CHECK_USED_MAP(len2)
      if (!IsOldVersion && Get64(p2 + 8)) // reserved field
        HeadersError = true; // unknown feature in reserved field
      size_t fileNameLen111 = Get16(p2 + extraOffset - 2);
      if (fileNameLen111 & 1)
        return S_FALSE;
      /* different number of additional ZEROs are possible as padding.
         if (fileNameLen111 == 0) { then record size is not aligned for 4,
            and there is padding. So we can use that padding for NUL character. }
         And we always use (fileNameLen111 + 2) in check: */
      if (((extraOffset + fileNameLen111 + 2 + align) & ~align) > len2)
        return S_FALSE;
      if (!CheckName_and_Fix_in_Meta(p2 + extraOffset, (unsigned)fileNameLen111))
        HeadersError = true; // return S_FALSE;
      fileNameLen111 = Get16(p2 + extraOffset - 2); // reload fileNameLen111 after correction
      // PRF(printf("\n  %S", p2 + extraOffset));
      /* wim uses alt streams list, if there is at least one alt stream.
         And alt stream without name is main stream. */
      // Why wimlib writes two alt streams for REPARSE_POINT, with empty second alt stream?
      
      Byte *prevMeta = DirData + item.Offset;

      if (fileNameLen111 == 0
          && !item.IsDir_NonReparse
          && (IsOldVersion || IsEmptySha(prevMeta + k_DirRecord_FieldOffset_of_Hash)))
      {
        if (IsOldVersion)
          memcpy(prevMeta + k_DirRecord_FieldOffset_of_FileId,
                       p2 + k_AltRecord_FieldOffset_of_FileId, 8);
          // we use only 32-bit of FileId, but field is 64-bit
        else if (!IsEmptySha(p2 + k_AltRecord_FieldOffset_of_Hash))
        {
          // if (IsEmptySha(prevMeta + k_DirRecord_FieldOffset_of_Hash))
          memcpy(prevMeta + k_DirRecord_FieldOffset_of_Hash,
                       p2 + k_AltRecord_FieldOffset_of_Hash, kHashSize);
          // else HeadersError = true;
        }
      }
      else
      {
        UPDATE_MEM_USAGE (sizeof(CItem) * 5 / 4)
        ThereAreAltStreams = true;
        CItem item2;
        item2.Construct();
        item2.Offset = pos;
        item2.IsAltStream = true;
        item2.Parent = (int)prevIndex;
        item2.DirLevel = dirLevel;
        item2.ImageIndex = (int)Images.Size() - 1;
        Items.Add(item2);
      }

      pos += len2;
    }

    if (parent < 0 && numItems == 0 && item.IsDir)
    {
      // this item is dir and is first item in root_dir
      if (!IsOldVersion || item.IsDir_NonReparse)
      {
        // (numItems == 0 && item.IsDir)
        const Byte *p2 = DirData + pos;
        if (DirSize - pos >= 8 && Get64(p2) == 0)
        {
          // this item is dir and is first and last item in root_dir
          if (fileNameLen == 0)
          {
            // wim 1.12+ : there is additional root directory with single subdirectory and empty name.
            // we set (NumEmptyRootItems = 1) to allow excluding of that directory item from path.
            image.NumEmptyRootItems = 1;
          }
          
          if (DirSize - pos >= 16)
          {
            // there is some space for additional list
            /* imagex and dism decoders probably ignore (subdirOffset) value in root_item,
            and imagex and dism just read next list after the end of root_list.
            imagex and dism encoders: if there are no user items in image,
            image still contains 2 directory lists:
              list_0: root_list with single item: root_item (with empty name)
              list_1: empty_list_1 (just 8 zero bytes)
            but root_item.subdirOffset == 0. So there is no reference to list_1 in root_item.
            
              (nextList_pos < subdirOffset) in Longhorn.4093 wim (wim with 3 images: that includes WinPE)
                and there are additional file records between (nextList_pos) and (subdirOffset) in list.
              
              we support such cases here:
            */
            const size_t nextList_pos = pos + 8;
            if (subdirOffset == 0 /* && Get64(p2 + 8) == 0 */ // it's usual case for empty wim archive
                || (nextList_pos < subdirOffset && Get64(p2 + 8)) // Longhorn.4093 case
                )
            {
              // we write new (subdirOffset) value in meta record of item:
              SetUi64(DirData + item.Offset + k_DirRecord_FieldOffset_of_SubdirOffset, nextList_pos)
              // printf("\ndirOffset = %5d hiddenOffset = %5d\n", (int)subdirOffset, (int)pos + 8);
            }
            // else DirProcessed += subdirOffset - (pos + 8);
          }
        }
      }
    }
  }
}


HRESULT CDatabase::ParseImageDirs(CByteBuffer &buf)
{
  DirData = buf;
  DirSize = buf.Size();
  if (DirSize < 8)
    return S_FALSE;
  size_t pos = 0;
  CImage &image = Images.Back();
  const Byte * const p = DirData;

  if (IsOldVersion)
  {
    const UInt32 numEntries = Get32(p + 4);
    if (numEntries >= (1 << 28) ||
        numEntries > (DirSize >> 3))
      return S_FALSE;
    UInt32 sum = 8;
    if (numEntries)
      sum = numEntries * 8;
    image.SecurOffsets.ClearAndReserve(numEntries + 1);
    image.SecurOffsets.AddInReserved(sum);
    for (UInt32 i = 0; i < numEntries; i++)
    {
      const Byte *pp = p + (size_t)i * 8;
      const UInt32 len = Get32(pp);
      if (i && Get32(pp + 4))
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
    const UInt32 totalLen = Get32(p);
    pos = 8;
    if (totalLen)
    {
      if (totalLen < 8)
        return S_FALSE;
      UInt32 numEntries = Get32(p + 4);
      if (totalLen > DirSize || numEntries > ((totalLen - 8) >> 3))
        return S_FALSE;
      UInt32 sum = (UInt32)pos + numEntries * 8;
      numEntries++;
      image.SecurOffsets.ClearAndReserve(numEntries);
      for (;;)
      {
        image.SecurOffsets.AddInReserved(sum);
        if (--numEntries == 0)
          break;
        const UInt64 len = Get64(p + pos);
        pos += 8;
        if (len > totalLen - sum)
          return S_FALSE;
        sum += (UInt32)len;
      }
      pos = sum;
      pos = (pos + 7) & ~(size_t)7;
      if (pos != (((size_t)totalLen + 7) & ~(size_t)7))
        return S_FALSE;
    }
  }
  
  if (pos > DirSize)
    return S_FALSE;
  DirStartOffset = /* DirProcessed = */ pos;
  image.StartItem = Items.Size();
  {
    MemUsage -= _useMap.Size();
    const size_t useMapSize = (DirSize + GetDirAlignMask()) >> GetDirAlign_numShifts();
    UPDATE_MEM_USAGE(useMapSize)
    _useMap.Alloc(useMapSize);
    memset(_useMap, 0, _useMap.Size());
  }
  {
    const int parent = -1;
    RINOK(ParseDirItem(pos, parent, 0)) // dirLevel = 0
  }
  {
    for (unsigned i = image.StartItem; i < Items.Size(); i++)
    {
      const CItem &item = Items[i];
      if (item.IsDir)
      if (item.IsDir_NonReparse || !IsOldVersion)
      {
        const UInt64 offset = Get64(image.Meta + item.Offset +
            k_DirRecord_FieldOffset_of_SubdirOffset);
        if (offset)
          RINOK(ParseDirItem((size_t)offset, (int)i, item.DirLevel + 1))
      }
    }
  }
  image.NumItems = Items.Size() - image.StartItem;

  if (DirSize & GetDirAlignMask())
    HeadersError = true;
  else
  {
    const size_t start = DirStartOffset >> GetDirAlign_numShifts();
    size_t num = _useMap.Size() - start;
    if (num)
    {
      const Byte *used = _useMap + start;
      do
      {
        if (!*used)
        {
          HeadersError = true;
          break;
        }
        used++;
      }
      while (--num);
    }
  }
  MemUsage -= _useMap.Size();
  _useMap.Free();
  /*
  if (DirProcessed == DirSize)
    return S_OK;
  if (DirProcessed == DirSize - 8 && Get64(p + DirSize - 8) == 0)
    return S_OK;
  // 18.06: we support cases, when some old dism can capture images
  // where DirProcessed much smaller than DirSize
  HeadersError = true;
  */
  return S_OK;
}


// (p) is aligned for 4 bytes
HRESULT CHeader::Parse(const Byte *p, UInt64 &phySize)
{
  const UInt32 headerSize = GetUi32a(p + 8);
  phySize = headerSize;
  {
    const UInt32 flags = GetUi32a(p + 0x10);
    Flags = flags;
    unsigned method = 0;
    if (flags & NHeaderFlags::kCompression)
    {
      const UInt32 mask = flags & NHeaderFlags::kMethodMask;
           if (mask == NHeaderFlags::kXPRESS ||
               mask == NHeaderFlags::kXPRESS2)  method = NMethod::kXPRESS;
      else if (mask == NHeaderFlags::kLZX)      method = NMethod::kLZX;
      else if (mask == NHeaderFlags::kLZMS)     method = NMethod::kLZMS;
      else return S_FALSE;
    }
    Method = method;
  }
  {
    ChunkSize = GetUi32a(p + 0x14);
    ChunkSizeBits = kChunkSizeBits;
    if (ChunkSize)
      if (!GetLog_val_min_dest(ChunkSize, 12, ChunkSizeBits))
        return S_FALSE;
  }

  _isOldVersion = false;
  _isNewVersion = true;
  Version = GetUi32a(p + 0x0C);
  if (!IsSolidVersion())
  {
    if (Version < 0x10900)
      return S_FALSE;
    _isNewVersion = (Version >= 0x10d00);
    // We don't know details about 1.11 version. So we use headerSize to guess exact features.
    if (Version <  0x10b00 || (Version == 0x10b00 && headerSize == 0x60))
      _isOldVersion = true;
  }

  BootIndex = 0;
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
      NumImages = GetUi32a(p + offset);
      offset += 4;
      BootIndex = GetUi32a(p + offset + 0x48);
      GET_RESOURCE(p + offset + 0x4C, IntegrityResource);
    }
  }
  
  GET_RESOURCE(p + offset       , OffsetResource);
  GET_RESOURCE(p + offset + 0x18, XmlResource);
  GET_RESOURCE(p + offset + 0x30, MetadataResource);

  return S_OK;
}


const Byte kSignature[kSignatureSize] = { 'M', 'S', 'W', 'I', 'M', 0, 0, 0 };

HRESULT ReadHeader(IInStream *inStream, CHeader &h, UInt64 &phySize)
{
  UInt64 p[(kHeaderSizeMax + 7) / 8];
  RINOK(ReadStream_FALSE(inStream, p, kHeaderSizeMax))
  if (memcmp(p, kSignature, kSignatureSize))
    return S_FALSE;
  return h.Parse((const Byte *)(const void *)p, phySize);
}

HRESULT CDatabase::OpenXml(IInStream *inStream, const CHeader &h, CByteBuffer &xml)
{
  if (h.XmlResource.UnpackSize >= 1u << 26)
    return E_OUTOFMEMORY;
  RINOK(UpdateMemUsage(h.XmlResource.UnpackSize * 4))
  CUnpacker unpacker;
  unpacker.MemUsage = MemUsage;
  unpacker.MemUsage_Limit = MemUsage_Limit;
  return unpacker.UnpackData(inStream, h.XmlResource, h, this, xml, NULL);
}

static void SetRootNames(CImage &image, const unsigned value /* , bool isDeletedImage */)
{
  char temp[16]; // 32 for deleted
  // char *e =
  ConvertUInt32ToString(value, temp);
  // if (isDeletedImage) MyStringCopy(e, "-DELETED");
  image.RootName = temp;
  unsigned len = image.RootName.Len() + 1;
  image.RootNameBuf.Alloc(len * 2);
  Byte *dest = image.RootNameBuf;
  const char *src = temp;
  do
  {
    SetUi16a(dest, (Byte)*src++)
    dest += 2;
  }
  while (--len);
}


HRESULT CDatabase::Open(IInStream *inStream, const CHeader &h, unsigned numItemsReserve, IArchiveOpenCallback *openCallback)
{
  OpenCallback = openCallback;
  IsOldVersion = h.IsOldVersion();
  IsOldVersion9 = (h.Version == 0x10900);

#ifdef Z7_WIM_SHOW_DELETED_IMAGES
  unsigned numDeletedImages = 0; // in MetaStreams
#endif

  CUnpacker unpacker;
  unpacker.MemUsage = MemUsage;
  unpacker.MemUsage_Limit = MemUsage_Limit;
  // ---------- Read Streams ----------
  {
    CByteBuffer offsetBuf;
    UPDATE_MEM_USAGE_WITH_RESOURCE(h.OffsetResource)
    RINOK(unpacker.UnpackData(inStream, h.OffsetResource, h, NULL, offsetBuf, NULL))
    {
      const size_t streamInfoSize = h.IsOldVersion() ? kStreamInfoSize + 2 : kStreamInfoSize;
      const unsigned numItems = (unsigned)(offsetBuf.Size() / streamInfoSize);
      if ((size_t)numItems * streamInfoSize != offsetBuf.Size())
        return S_FALSE;
      const unsigned numItems2 = DataStreams.Size() + numItems;
      if (numItems2 < numItems)
        return S_FALSE;
      DataStreams.Reserve(numItems2);
    }
    bool keepSolid = false;
    
    HRESULT hres = S_OK;

    for (const Byte *p = offsetBuf; p < offsetBuf + offsetBuf.Size();)
    {
      // (p) is aligned for 2-bytes
      CStreamInfo s;
      s.Resource.Parse(p);
      if (h.IsOldVersion())
      {
        s.PartNumber = 1;
        s.Id = Get32(p + 24);
        p += kStreamInfoSize + 2;
        // (p) is aligned for 4-bytes
      }
      else
      {
        s.PartNumber = Get16(p + 24);
        // s.Id = 0; // optional : unused
        p += kStreamInfoSize;
      }
      // (p) is aligned for 2-bytes
      s.RefCount = GetUi32(p - kHashSize - 4); // it's unaligned for (!h.IsOldVersion())
      memcpy(s.Hash, p - kHashSize, kHashSize);
      
      PRF(printf("\n"));
      PRF(printf("%s", s.Resource.IsMetadata() ? "### META" : "    DATA"));
      PRF(printf(" %2x", (unsigned)s.Resource.Flags));
      PRINT_UI64(s.Resource.Offset)
      PRINT_UI64(s.Resource.PackSize)
      PRINT_UI64(s.Resource.UnpackSize)
      PRF(printf(" %5u", (unsigned)s.RefCount));
      PRF(fflush(stdout);)
      
      if (s.Resource.AreUnknownFlags())
        HeadersError = true;
      
      if (s.PartNumber != h.PartNumber)
        continue; // is it possible in real archive, or it's error in header?

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
      
      UPDATE_MEM_USAGE(sizeof(CStreamInfo))
      if (!s.Resource.IsMetadata())
      {
        DataStreams.AddInReserved(s);
        continue;
      }
      {
        // s.Resource.IsMetadata() == true
        if (s.Resource.IsSolid())
        {
          hres = S_FALSE; // E_NOTIMPL;
          HeadersError = true;
          continue;
        }
        if (s.RefCount == 0)
        {
          // some wims have such (deleted?) metadata stream.
          // examples: boot.wim in VistaBeta2, WinPE.wim from WAIK.
#ifndef Z7_WIM_SHOW_DELETED_IMAGES
          // HeadersError = true; // v26.04: we can show error for that case.
          continue;
#endif
        }
        if (s.RefCount > 1)
        {
          hres = S_FALSE;
          HeadersError = true;
          continue;
          // s.RefCount--;
          // DataStreams.Add(s);
        }
        
        // DOCS: the first part will always contain all metadata resources
        // so we ignore another Meta Streams.
        if (s.PartNumber == 1 /* && h.PartNumber == 1 */)
        {
          if (s.Resource.UnpackSize >= (UInt64)1 << 48) return E_OUTOFMEMORY;
          UPDATE_MEM_USAGE_WITH_RESOURCE(s.Resource)
#ifdef Z7_WIM_SHOW_DELETED_IMAGES
          // we insert non-deleted images before deleted images:
          unsigned insertPos = MetaStreams.Size();
          if (s.RefCount == 0)
            numDeletedImages++;
          else
            insertPos -= numDeletedImages;
          MetaStreams.Insert(insertPos, s);
#else
          MetaStreams.Add(s);
#endif
        }
        else
        {
          HeadersError = true;
          continue;
        }
      }
    }
    RINOK(hres)
    PRF(printf("\n"));
  }
  MemUsage -= h.OffsetResource.UnpackSize;

  bool needBootMetadata = !h.MetadataResource.IsEmpty();
  if (h.PartNumber == 1)
  {
    // ---------- Parse MetaStreams ----------
    // Meta must be stored only in first volume. We ignore another  Meta Streams.
    unsigned numNonDeletedImages = 0;
    if (h.IsNewVersion() && MetaStreams.Size() != h.NumImages
#ifdef Z7_WIM_SHOW_DELETED_IMAGES
        + numDeletedImages
#endif
        )
      HeadersError = true;

    FOR_VECTOR (i, MetaStreams)
    {
      const CStreamInfo &si = MetaStreams[i];
      if (si.PartNumber != h.PartNumber)
        continue; // is not expected case, because we fill MetaStreams[] only for (PartNumber == 1)
      si.Resource.UpdatePhySize(PhySize);
      
      const unsigned userImage = Images.Size() + GetStartImageIndex();
      CImage &image = Images.AddNew();
      SetRootNames(image, userImage /* , i >= MetaStreams.Size() - numDeletedImages */);
      
      CByteBuffer &metadata = image.Meta;
      UInt32 hash[kHashSize / 4];
      
      RINOK(unpacker.UnpackData(inStream, si.Resource, h, this, metadata, hash))
        
      if (COMPARE_HASHES(hash, si.Hash) &&
         !(h.IsOldVersion() && si.IsEmptyHash()))
        return S_FALSE;
        
      image.NumEmptyRootItems = 0;
        
      if (Items.IsEmpty())
        Items.ClearAndReserve(numItemsReserve);
        
      RINOK(ParseImageDirs(metadata))
          
      if (needBootMetadata)
      {
        const bool sameRes = (h.MetadataResource.Offset == si.Resource.Offset);
        if (sameRes)
        {
          if (   h.MetadataResource.UnpackSize != si.Resource.UnpackSize
              || h.MetadataResource.PackSize != si.Resource.PackSize
              || h.MetadataResource.Flags != si.Resource.Flags)
            return S_FALSE;
          needBootMetadata = false;
        }
        if (h.IsNewVersion())
        {
          if (si.RefCount == 1)
          {
            numNonDeletedImages++;
            const bool isBootIndex = (h.BootIndex == numNonDeletedImages);
            if (sameRes != isBootIndex)
              return S_FALSE;
          }
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
    // old wim uses same field for file_id and dir_offset;
    // if (item.IsDir)
    if (item.IsDir_NonReparse)
      return false;
    meta += item.IsAltStream ?
        k_AltRecord_FieldOffset_of_FileId :
        k_DirRecord_FieldOffset_of_FileId;
    const UInt32 id = Get32(meta);
    return id != 0;
  }
  meta += item.GetHashFieldOffset();
  return !IsEmptySha(meta);
}


#define RINOZ(x) { int _tt_ = (x); if (_tt_ != 0) return _tt_; }

static int CompareStreamsByPos(const CStreamInfo *p1, const CStreamInfo *p2, void * /* param */)
{
  RINOZ(MyCompare(p1->PartNumber, p2->PartNumber))
  RINOZ(MyCompare(p1->Resource.Offset, p2->Resource.Offset))
  return MyCompare(p1->Resource.PackSize, p2->Resource.PackSize);
}

static int CompareIDs(const unsigned *p1, const unsigned *p2, void *param)
{
  const CStreamInfo *streams = (const CStreamInfo *)param;
  return MyCompare(streams[*p1].Id, streams[*p2].Id);
}


/* CompareHashRefs() and FindHash() must use same comparison function for hash values.
   So we use COMPARE_HASHES() in these functions */

static int CompareHashRefs(const unsigned *p1, const unsigned *p2, void *param)
{
  const CStreamInfo *streams = (const CStreamInfo *)param;
  return COMPARE_HASHES(streams[*p1].Hash, streams[*p2].Hash);
}

static int FindId(const CStreamInfo *streams, const CUIntVector &sorted, const UInt32 id)
{
  unsigned left = 0, right = sorted.Size();
  while (left != right)
  {
    const unsigned mid = (left + right) / 2;
    const unsigned streamIndex = sorted[mid];
    const UInt32 id2 = streams[streamIndex].Id;
    if (id == id2)
      return (int)streamIndex;
    if (id < id2)
      right = mid;
    else
      left = mid + 1;
  }
  return -1;
}

static int FindHash(const CStreamInfo *streams, const CUIntVector &sorted, const Byte * const hash)
{
  unsigned left = 0, right = sorted.Size();
  while (left != right)
  {
    const unsigned mid = (left + right) / 2;
    const unsigned streamIndex = sorted[mid];
#if 1 // 0 : for debug
    const int comp = COMPARE_HASHES((const UInt32 *)(const void *)hash, streams[streamIndex].Hash);
    if (comp == 0)
      return (int)streamIndex;
    if (comp < 0)
      right = mid;
    else
      left = mid + 1;
#else
    // we can us it only if CompareHashRefs() also uses Z7_WIM_SHA1_UI32_COMPARE_LESS_EQUAL_GREATER
    Z7_WIM_SHA1_UI32_COMPARE_LESS_EQUAL_GREATER(
        (const UInt32 *)(const void *)hash, streams[streamIndex].Hash,
      right = mid; ,
      return (int)streamIndex; ,
      left = mid + 1; )
#endif
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
  if (i1.IsDir_NonReparse != i2.IsDir_NonReparse)
    return i1.IsDir_NonReparse ? -1 : 1;
  if (i1.IsAltStream != i2.IsAltStream)
    return i1.IsAltStream ? 1 : -1;
  RINOZ(MyCompare(i1.StreamIndex, i2.StreamIndex))
  RINOZ(MyCompare(i1.ImageIndex, i2.ImageIndex))
  return MyCompare(i1.Offset, i2.Offset);
}


HRESULT CDatabase::FillAndCheck(const CObjectVector<CVolume> &volumes)
{
  CUIntVector sortedByHash;
  sortedByHash.Reserve(DataStreams.Size());
  {
    for (unsigned iii = 0; iii < DataStreams.Size();)
    {
      if (!DataStreams[iii].Resource.IsSolid())
      {
        sortedByHash.AddInReserved(iii++);
        continue;
      }
      
      // We process all current SolidBig streams inside current solid group.
     
      UInt64 solidRunOffset = 0;
      const unsigned numSolidsStart = Solids.Size(); // start of current solid group
      unsigned k;
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
        if (!si.IsEmptyHash() || si.RefCount != 1)
          return S_FALSE;

        r.SolidIndex = (int)Solids.Size();
        CSolid &ss = Solids.AddNew();
        ss.StreamIndex = k;
        ss.SolidOffset = solidRunOffset;
        {
          const size_t kSolidHeaderSize = 8 + 4 + 4;
          UInt64 header64[kSolidHeaderSize / 8];

          if (si.PartNumber >= volumes.Size())
            return S_FALSE;
          IInStream *inStream = volumes[si.PartNumber].Stream;
          RINOK(InStream_SeekSet(inStream, r.Offset))
          RINOK(ReadStream_FALSE(inStream, header64, kSolidHeaderSize))
          
          ss.UnpackSize = GetUi64(header64);
          if (ss.UnpackSize >= ((UInt64)1 << 63))
            return S_FALSE;
          solidRunOffset += ss.UnpackSize;
          if (solidRunOffset < ss.UnpackSize)
            return S_FALSE;

          const UInt32 solidChunkSize = GetUi32a((const Byte *)(const void *)header64 + 8);
          if (!GetLog_val_min_dest(solidChunkSize, 12, ss.ChunkSizeBits)) // min: 12 for XPRESS, 15 for LZX/LZMS
            return S_FALSE;
          ss.Method = (Int32)GetUi32a((const Byte *)(const void *)header64 + 12);
          
          const UInt64 numChunks64 = (ss.UnpackSize + (((UInt32)1 << ss.ChunkSizeBits) - 1)) >> ss.ChunkSizeBits;
          const UInt64 sizesBufSize64 = 4 * numChunks64;
          UInt64 offset = kSolidHeaderSize + sizesBufSize64;
          if (offset > r.PackSize)
            return S_FALSE;
          const size_t sizesBufSize = (size_t)sizesBufSize64;
          if (sizesBufSize != sizesBufSize64)
            return E_OUTOFMEMORY;
          UPDATE_MEM_USAGE(numChunks64 * sizeof(ss.Chunks[0]) + sizeof(CSolid) / 4 * 5 + 32)
          
          size_t numItems = (size_t)numChunks64 + 1;
          ss.Chunks.Alloc(numItems);
          
          UInt32 *packSizes = (UInt32 *)(void *)
              ((Byte *)(void *)(ss.Chunks + numItems) - sizesBufSize);
          RINOK(ReadStream_FALSE(inStream, packSizes, sizesBufSize))
          UInt64 *chunks = ss.Chunks;
          
          for (;;)
          {
            *chunks++ = offset;
            if (--numItems == 0)
              break;
            const UInt32 packSize = GetUi32a(packSizes);
            packSizes++;
            offset += packSize;
            if (offset < packSize)
              return S_FALSE;
          }
          if (offset != r.PackSize)
            return S_FALSE;
          // if ((void *)chunks != (void *)packSizes) return E_FAIL;
        }
      }
      
      // We process all SolidSmall streams inside latest Solids[] solid group

      for (; iii < k; iii++)
      {
        CStreamInfo &si = DataStreams[iii];
        CResource &r = si.Resource;
        if (!r.IsSolidSmall())
          continue;
        if (si.IsEmptyHash())
          return S_FALSE;
        unsigned left = numSolidsStart;
        unsigned right = Solids.Size();
        for (;;)
        {
          if (left == right)
            return S_FALSE;
          const unsigned mid = (unsigned)(((size_t)left + (size_t)right) / 2);
          CSolid &ss = Solids[mid];
          if (r.Offset < ss.SolidOffset)
          {
            right = mid;
            continue;
          }
          if (r.Offset - ss.SolidOffset < ss.UnpackSize)
          {
            r.SolidIndex = (int)mid;
            if (ss.FirstSmallStream < 0)
              ss.FirstSmallStream = (int)iii;
            break;
          }
          left = mid + 1;
        }
        sortedByHash.AddInReserved(iii);
        // ss.NumRefs++;
      }
    }
  }

  if (Solids.IsEmpty())
  {
    // ---------- NON-SOLID ARCHIVE ----------
    if (sortedByHash.Size() != DataStreams.Size())
      return E_FAIL;
    /* sortedByHash[] contains all indexes that refer to DataStreams[].
       So we can change order of DataStreams[] items here with sorting.
      
       We sort DataStreams[] by [PartNumber, Offset, PackSize] fields.
       And then we check streams for overlapping.
       NOTE: another our code can work with non-sorted streams.
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
      if (s0.PartNumber == s1.PartNumber &&
          s0.Resource.GetEndLimit() > s1.Resource.Offset)
        return S_FALSE;
    }
  }
  
  // ---------- SORTING BY HASH ----------
  {
    const CStreamInfo *streams = DataStreams.ConstData();
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
        if (streams[sortedByHash[0]].IsEmptyHash())
          HeadersError = true;
        for (unsigned i = 1; i < sortedByHash.Size(); i++)
          if (COMPARE_HASHES(
              streams[sortedByHash[i - 1]].Hash,
              streams[sortedByHash[i]].Hash
              ) >= 0)
            return S_FALSE;
      }
    }
  }
  // ---------- FIND index by HASH ----------
  {
    FOR_VECTOR (i, Items)
    {
      CItem &item = Items[i];
      item.StreamIndex = -1;
      const Byte *hash = Images[item.ImageIndex].Meta + item.Offset;
      if (IsOldVersion)
      {
        // if (!item.IsDir)
        if (!item.IsDir_NonReparse)
        {
          hash += item.IsAltStream ?
              k_AltRecord_FieldOffset_of_FileId :
              k_DirRecord_FieldOffset_of_FileId;
          const UInt32 id = Get32(hash);
          if (id)
            item.StreamIndex = FindId(DataStreams.ConstData(), sortedByHash, id);
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
        hash += item.GetHashFieldOffset();
        if (!IsEmptySha(hash))
          item.StreamIndex = FindHash(DataStreams.ConstData(), sortedByHash, hash);
      }
    }
  }
  // ---------- REF COUNTING ----------
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
      const int streamIndex = Items[i].StreamIndex;
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
          // ---------- Add DELETED ITEM ----------
          CItem item;
          item.Construct();
          item.Offset = 0;
          item.StreamIndex = (int)i;
          item.ImageIndex = -1;
          Items.Add(item);
          ThereAreDeletedStreams = true;
        }
      }
    }
  }

  return S_OK;
}


HRESULT CDatabase::GenerateSortedItems(const int imageIndex, bool showImageNumber)
{
  SortedItems.Clear();
  VirtualRoots.Clear();
  IndexOfUserImage = imageIndex;
  NumExcludededItems = 0;
  ExcludedItem = -1;

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
    else
    {
      // (Images.Size() != 1 && imageIndex < 0)
      #if 1 // optional code
      // it's expected that already set (showImageNumber = true) for that case.
      showImageNumber = true;
      #endif
      // showImageNumber = false; // for debug
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
  
  if (NumExcludededItems)
  {
    ExcludedItem = (int)startItem;
    startItem += NumExcludededItems;
  }

  const unsigned num = endItem - startItem;
  SortedItems.ClearAndSetSize(num);
  unsigned i;
  for (i = 0; i < num; i++)
    SortedItems[i] = startItem + i;

  SortedItems.Sort(CompareItems, this);
  for (i = 0; i < SortedItems.Size(); i++)
    Items[SortedItems[i]].IndexInSorted = (int)i;

  if (showImageNumber)
    for (i = 0; i < Images.Size(); i++)
    {
      CImage &image = Images[i];
      if (image.NumEmptyRootItems == 0)
      {
        // 1.10- archives
        image.VirtualRootIndex = (int)VirtualRoots.Size();
        VirtualRoots.Add(i);
      }
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


bool CDatabase::Check_PartNumber_in_Items(unsigned numVolumes) const
{
  // maybe it's better to check all Items[] or all DataStreams[] items instead
  FOR_VECTOR (indexInSorted, SortedItems)
  {
    const unsigned itemIndex = SortedItems[indexInSorted];
    const CItem &item = Items[itemIndex];
    if (item.StreamIndex < 0)
      continue;
    const CStreamInfo &si = DataStreams[item.StreamIndex];
    if (si.PartNumber >= numVolumes)
      return false;
  }
  return true;
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
  unpacker.MemUsage = MemUsage;
  unpacker.MemUsage_Limit = MemUsage_Limit;
  UInt64 totalPackedPrev = 0;

  FOR_VECTOR (indexInSorted, SortedItems)
  {
    // we use sorted items for faster access
    const unsigned itemIndex = SortedItems[indexInSorted];
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
    const UInt32 tag = Get32(metadata + offset);
    const int reparseIndex = streamToReparse[item.StreamIndex];
    CByteBuffer buf;

    if (openCallback && unpacker.TotalPacked - totalPackedPrev >= ((UInt32)1 << 16))
    {
      totalPackedPrev = unpacker.TotalPacked;
      const UInt64 numFiles = Items.Size();
      RINOK(openCallback->SetCompleted(&numFiles, &unpacker.TotalPacked))
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
      if (si.PartNumber >= volumes.Size())
        continue;
      const CVolume &vol = volumes[si.PartNumber];
      /*
      if (!vol.Stream)
        continue;
      */
      
      UInt32 digest[kHashSize / 4];
      HRESULT res = unpacker.UnpackData(vol.Stream, si.Resource, vol.Header, this, buf, digest);

      if (res == S_FALSE)
        continue;

      RINOK(res)
      
      if (COMPARE_HASHES(digest, si.Hash)
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
    SetUi32a(dest, tag)
    SetUi32a(dest + 4, (UInt32)buf.Size())
    if (buf.Size())
      memcpy(dest + 8, buf, buf.Size());
    ItemToReparse[itemIndex] = (int)ReparseItems.Size() - 1;
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
  const CXmlItem *timeItem = item.FindSubTag_GetPtr(tag);
  if (timeItem)
  {
    UInt32 low = 0, high = 0;
    if (ParseNumber32(timeItem->GetSubStringForTag("LOWPART"), low) &&
        ParseNumber32(timeItem->GetSubStringForTag("HIGHPART"), high))
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
  if (size < 2 || (size & 1) || size > (1 << 24))
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
  if (!Xml.Root.Name.IsEqualTo("WIM"))
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

      imageInfo.ItemIndexInXml = (int)i;
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

// Archive/CabIn.cpp

#include "StdAfx.h"

// #include <stdio.h>

#include "../../../../C/CpuArch.h"

#include "../../Common/LimitedStreams.h"
#include "../../Common/StreamUtils.h"

#include "CabIn.h"

#define Get16a(p) GetUi16a(p)
#define Get32a(p) GetUi32a(p)

#if 1
#define Get16(p) GetUi16a(p)
#define Get32(p) GetUi32a(p)
#else
#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)
#endif

namespace NArchive {
namespace NCab {

struct CUnexpectedEndException {};

void CInArchive::Skip(unsigned size)
{
  if (_inBuffer.Skip(size) != size)
    throw CUnexpectedEndException();
}

void CInArchive::Read(Byte *data, const unsigned size)
{
  if (_inBuffer.ReadBytes(data, size) != size)
    throw CUnexpectedEndException();
}

static const size_t k_Name_Size_Limit = 1 << 13;

bool CInArchive::ReadName(AString &s, const UInt32 index)
{
  size_t i;
  for (i = 0; i < k_Name_Size_Limit; i++)
  {
    Byte b;
    if (!_inBuffer.ReadByte(b))
      throw CUnexpectedEndException();
    if (b == 0)
    {
      s.SetFrom((const char *)(const Byte *)_tempBuf, (unsigned)i);
      return true;
    }
    _tempBuf[i] = b;
  }
  
  ErrorInNames = true;
  s = "[LONG_PATH_FILE]";
  if (index != (UInt32)0-1)
  {
    s.Add_Minus();
    s.Add_UInt32(index);
  }
  for (;;)
  {
    Byte b;
    if (!_inBuffer.ReadByte(b))
      throw CUnexpectedEndException();
    if (b == 0)
      return true;
    if (++i >= (1 << 15))
      return false;
  }
}

bool CInArchive::ReadOtherArc(COtherArc &oa)
{
  if (!ReadName(oa.FileName))
    return false;
  AString DiskName;
  return ReadName(DiskName);
}


#if 0
struct CSignatureFinder
{
  Byte *Buf;
  UInt32 Pos;
  UInt32 End;
  const Byte *Signature;
  UInt32 SignatureSize;
  
  UInt32 _headerSize;
  UInt32 _alignSize;
  UInt32 _bufUseCapacity;

  const UInt64 *SearchLimit;
  ISequentialInStream *Stream;
  UInt64 Processed; // Global offset of start of Buf

  UInt32 GetTotalCapacity(UInt32 basicSize, UInt32 headerSize)
  {
    _headerSize = headerSize;
    for (_alignSize = (1 << 5); _alignSize < _headerSize; _alignSize <<= 1);
    _bufUseCapacity = basicSize + _alignSize;
    return _bufUseCapacity + 16;
  }

  /*
  returns:
    S_OK      - signature found (at Pos)
    S_FALSE   - signature not found
  */
  HRESULT Find();
};


HRESULT CSignatureFinder::Find()
{
  for (;;)
  {
    Buf[End] = Signature[0]; // it's for fast search;

    while (End - Pos >= _headerSize)
    {
      const Byte *p = Buf + Pos;
      const Byte b = Signature[0];
      for (;;)
      {
        if (*p == b) { break; }  p++;
        if (*p == b) { break; }  p++;
      }
      Pos = (UInt32)(p - Buf);
      if (End - Pos < _headerSize)
      {
        Pos = End - _headerSize + 1;
        break;
      }
      UInt32 i;
      for (i = 1; i < SignatureSize && p[i] == Signature[i]; i++);
      if (i == SignatureSize)
        return S_OK;
      Pos++;
    }

    if (Pos >= _alignSize)
    {
      const UInt32 num = (Pos & ~(_alignSize - 1));
      Processed += num;
      Pos -= num;
      End -= num;
      memmove(Buf, Buf + num, End);
    }
    UInt32 rem = _bufUseCapacity - End;
    if (SearchLimit)
    {
      if (Processed + Pos > *SearchLimit)
        return S_FALSE;
      const UInt64 rem2 = *SearchLimit - (Processed + End) + _headerSize;
      if (rem > rem2)
        rem = (UInt32)rem2;
    }
    
    UInt32 processedSize;
    if (Processed == 0 && rem == _bufUseCapacity - _headerSize)
      rem -= _alignSize; // to make reads more aligned.
    RINOK(Stream->Read(Buf + End, rem, &processedSize))
    if (processedSize == 0)
      return S_FALSE;
    End += processedSize;
  }
}
#endif


bool CInArcInfo::Parse(const Byte *p)
{
  if (Get32(p + 0x0C) | Get32(p + 0x14))
    return false;
  if (Get16(p + 0x18) != 0x103) // VersionMajor/VersionMinor
    return false;
  Flags = Get16(p + 0x1E);
  if (Flags > 7)
    return false;
  Size = Get32(p + 8);
  if (Size < 36)
    return false;
  FileHeadersOffset = Get32(p + 0x10);
  if (FileHeadersOffset > Size)
    return false;
  NumFolders = Get16(p + 0x1A);
  NumFiles = Get16(p + 0x1C);
  return true;
}
  

HRESULT CInArchive::Open2(CDatabaseEx &db, const UInt64 * /* searchHeaderSizeLimit */)
{
  IsArc = false;
  ErrorInNames = false;
  UnexpectedEnd = false;
  HeaderError = false;

  db.Clear();
  db.StartPosition = 0;
  // RINOK(InStream_GetPos(db.Stream, db.StartPosition))

  CByteBuffer buffer;
  CInArcInfo &ai = db.ArcInfo;
  const UInt64 startInBuf = 0;

  CLimitedSequentialInStream *limitedStreamSpec = NULL;
  CMyComPtr<ISequentialInStream> limitedStream;

  {
    const UInt32 kMainHeaderSize = 32;
    UInt32 header32[kMainHeaderSize / 4];
    const UInt32 kBufSize = 1 << 15;
    RINOK(ReadStream_FALSE(db.Stream, header32, kMainHeaderSize))
    if (memcmp(header32, NHeader::kMarker, NHeader::kMarkerSize) == 0
        && ai.Parse((const Byte *)(const void *)header32))
    {
      limitedStreamSpec = new CLimitedSequentialInStream;
      limitedStream = limitedStreamSpec;
      limitedStreamSpec->SetStream(db.Stream);
      limitedStreamSpec->Init(ai.Size - kMainHeaderSize);
      buffer.Alloc(kBufSize);
      // memcpy(buffer, header32, kMainHeaderSize);
      UInt32 numProcessedBytes;
      RINOK(limitedStream->Read(buffer + kMainHeaderSize, kBufSize - kMainHeaderSize, &numProcessedBytes))
      _inBuffer.SetBuf(buffer, (UInt32)kBufSize, kMainHeaderSize + numProcessedBytes, kMainHeaderSize);
    }
    else
    {
#if 1
      return S_FALSE;
#else
      if (searchHeaderSizeLimit && *searchHeaderSizeLimit == 0)
        return S_FALSE;
      CSignatureFinder finder;

      finder.Stream = db.Stream;
      finder.Signature = NHeader::kMarker;
      finder.SignatureSize = NHeader::kMarkerSize;
      finder.SearchLimit = searchHeaderSizeLimit;

      buffer.Alloc(finder.GetTotalCapacity(kBufSize, kMainHeaderSize));
      finder.Buf = buffer;

      memcpy(buffer, header, kMainHeaderSize);
      finder.Processed = db.StartPosition;
      finder.End = kMainHeaderSize;
      finder.Pos = 0;
  
      for (;;)
      {
        finder.Pos++;
        RINOK(finder.Find())
        if (ai.Parse(finder.Buf + finder.Pos))
          break;
      }
      db.StartPosition = finder.Processed + finder.Pos;
      limitedStreamSpec = new CLimitedSequentialInStream;
      limitedStreamSpec->SetStream(db.Stream);
      limitedStream = limitedStreamSpec;
      const UInt32 remInFinder = finder.End - finder.Pos;
      if (ai.Size <= remInFinder)
      {
        finder.End = finder.Pos + ai.Size;
        limitedStreamSpec->Init(0);
      }
      else
        limitedStreamSpec->Init(ai.Size - remInFinder);
      
      startInBuf = finder.Pos;
      _inBuffer.SetBuf(buffer, (UInt32)kBufSize, finder.End, finder.Pos + kMainHeaderSize);
#endif
    }
  }
  
  IsArc = true;
  _inBuffer.SetStream(limitedStream);
  _tempBuf.Alloc(k_Name_Size_Limit);

  UInt32 p32[16 / 4];
  Byte * const p = (Byte *)(void *)p32;
  const unsigned nextSize = 4 + (ai.ReserveBlockPresent() ? 4 : 0);
  Read(p, nextSize);
  ai.SetID = Get16a(p);
  ai.CabinetNumber = Get16a(p + 2);
  // printf("CabinetNumber=%5u \n", ai.CabinetNumber);
  if (ai.ReserveBlockPresent())
  {
    ai.PerCabinet_AreaSize = Get16a(p + 4);
    ai.PerFolder_AreaSize = p[6];
    ai.PerDataBlock_AreaSize = p[7];
    Skip(ai.PerCabinet_AreaSize);
  }
  if (ai.IsTherePrev() && !ReadOtherArc(ai.PrevArc)) return false;
  if (ai.IsThereNext() && !ReadOtherArc(ai.NextArc)) return false;
  
  unsigned i;
  db.Folders.ClearAndReserve(ai.NumFolders);
  for (i = 0; i < ai.NumFolders; i++)
  {
    Read(p, 8);
    CFolder folder;
    folder.DataStart = Get32a(p);
    folder.NumDataBlocks = Get16a(p + 4);
    folder.MethodMajor = p[6];
    folder.MethodMinor = p[7];
    Skip(ai.PerFolder_AreaSize);
    db.Folders.AddInReserved(folder);
    // printf("folder=%5u DataStart=%8u numBlocks=%8u\n", i, folder.DataStart, folder.NumDataBlocks);
  }
  
  {
    const UInt64 procSize = _inBuffer.GetProcessedSize() - startInBuf;
    if (procSize != ai.FileHeadersOffset)
    {
      if (procSize > ai.FileHeadersOffset)
        return S_FALSE;
      // printf("\n!!! Seek Error !!!!\n");
      // fflush(stdout);
      RINOK(InStream_SeekSet(db.Stream, db.StartPosition + ai.FileHeadersOffset))
      limitedStreamSpec->Init(ai.Size - ai.FileHeadersOffset);
      _inBuffer.Init();
    }
  }

/*
  if (FolderIndex == kContinuedFromPrev || FolderIndex == kContinuedPrevAndNext),
     then there are no (FolderIndex == 0) items in that cab volume.
     But (FolderIndex == kContinuedToNext) and (FolderIndex == numFolders - 1) items are possible in same volume.
  if there are multiple files with (FolderIndex == kContinuedToNext), then they are duplicated
  in next volume with (FolderIndex == kContinuedFromPrev or kContinuedPrevAndNext).
*/
  db.Items.ClearAndReserve(ai.NumFiles);
  for (i = 0; i < ai.NumFiles; i++)
  {
    Read(p, 16);
    CItem &item = db.Items.AddNewInReserved();
    item.Size = Get32a(p);
    item.Offset = Get32a(p + 4);
    item.FolderIndex = Get16a(p + 8);
    const UInt16 pureDate = Get16a(p + 10);
    const UInt16 pureTime = Get16a(p + 12);
    item.Time = (((UInt32)pureDate << 16)) | pureTime;
    item.Attributes = Get16a(p + 14);
    if (!ReadName(item.Name, db.Items.Size() - 1))
      return S_FALSE;
    // printf("  -file=%5u Folder=%5x Offset=%10u size=%10u %s\n", i, item.FolderIndex, item.Offset, item.Size, item.Name.Ptr());
    const unsigned numFolders = db.Folders.Size();
    if (numFolders == 0 || item.GetFolderIndex(numFolders) >= numFolders)
    {
      HeaderError = true;
      return S_FALSE;
    }
  }
  return S_OK;
}


HRESULT CInArchive::Open(CDatabaseEx &db, const UInt64 *searchHeaderSizeLimit)
{
  try
  {
    // for (int y = 0; y < 1000; y++) { Open2(db, searchHeaderSizeLimit);  RINOK(InStream_SeekToBegin(db.Stream)) }
    return Open2(db, searchHeaderSizeLimit);
  }
  catch(const CInBufferException &e) { return e.ErrorCode; }
  catch(CUnexpectedEndException &) { UnexpectedEnd = true; return S_FALSE; }
}



#define RINOZ(x) { int _tt_ = (x); if (_tt_ != 0) return _tt_; }

static int CompareMvItems(const CMvItem *p1, const CMvItem *p2, void *param)
{
  const CMvDatabaseEx &mvDb = *(const CMvDatabaseEx *)param;
  const CDatabaseEx &db1 = mvDb.Volumes[p1->VolumeIndex];
  const CDatabaseEx &db2 = mvDb.Volumes[p2->VolumeIndex];
  const CItem &item1 = db1.Items[p1->ItemIndex];
  const CItem &item2 = db2.Items[p2->ItemIndex];
  const int f1 = mvDb.GetGlobalFolderIndex(*p1);
  const int f2 = mvDb.GetGlobalFolderIndex(*p2);
  RINOZ(MyCompare(f1, f2))
  RINOZ(MyCompare(item1.Offset, item2.Offset))
  RINOZ(MyCompare(item1.Size, item2.Size))
  RINOZ(MyCompare(p1->VolumeIndex, p2->VolumeIndex))
  return MyCompare(p1->ItemIndex, p2->ItemIndex);
}


bool CMvDatabaseEx::AreItemsEqual(unsigned i1, unsigned i2) const
{
  const CMvItem &p1 = Items[i1];
  const CMvItem &p2 = Items[i2];
  const CDatabaseEx &db1 = Volumes[p1.VolumeIndex];
  const CDatabaseEx &db2 = Volumes[p2.VolumeIndex];
  const CItem &item1 = db1.Items[p1.ItemIndex];
  const CItem &item2 = db2.Items[p2.ItemIndex];
  return GetGlobalFolderIndex(p1) == GetGlobalFolderIndex(p2)
      && item1.Offset == item2.Offset
      && item1.Size == item2.Size
      && item1.Name == item2.Name;
}


bool CMvDatabaseEx::Fill_Sort_Shrink_and_Check()
{
{
  Items.Clear();
  FolderStartFileIndex.Clear();
  
  int offset = 0;
  
  FOR_VECTOR (v, Volumes)
  {
    CDatabaseEx &db = Volumes[v];
    if (db.IsTherePrevFolder())
      offset--;
    db.StartGlobalFolderIndex = offset;
    // printf("vol=%5u StartGlobalFolderIndex=%8d\n", v, offset);
    offset += (int)db.Folders.Size();
    if (offset < -1) // overflow
      return false;

    CMvItem mvItem;
    mvItem.VolumeIndex = v;
    FOR_VECTOR (i, db.Items)
    {
      mvItem.ItemIndex = i;
      Items.Add(mvItem);
    }
  }

  if (Items.Size() > 1)
  {
    Items.Sort(CompareMvItems, (void *)this);
    unsigned j = 1;
    unsigned i = 1;
    for (; i < Items.Size(); i++)
      if (!AreItemsEqual(i, i - 1))
        Items[j++] = Items[i];
    Items.DeleteFrom(j);
  }

  FOR_VECTOR (i, Items)
  {
    const int folderIndex = GetGlobalFolderIndex(Items[i]);
    while (folderIndex >= (int)FolderStartFileIndex.Size())
      FolderStartFileIndex.Add(i);
  }
}

  for (unsigned v = 1; v < Volumes.Size(); v++)
  {
    const CDatabaseEx &db1 = Volumes[v];
    if (db1.IsTherePrevFolder())
    {
      const CDatabaseEx &db0 = Volumes[v - 1];
      if (db0.Folders.IsEmpty() || db1.Folders.IsEmpty())
        return false;
      const CFolder &f0 = db0.Folders.Back();
      const CFolder &f1 = db1.Folders.FrontItem();
      if (!f0.Is_Method_EqualTo(f1))
        return false;
    }
  }

  UInt32 prevOffs = 0, prevSize = 0;
  int prevFolder = -2;
  
  FOR_VECTOR (i, Items)
  {
    const CMvItem &mvItem = Items[i];
    const int folderIndex = GetGlobalFolderIndex(mvItem);
    if (folderIndex >= (int)FolderStartFileIndex.Size())
      return false;
    const CItem &item = Volumes[mvItem.VolumeIndex].Items[mvItem.ItemIndex];
    
    if (folderIndex != prevFolder)
      prevFolder = folderIndex;
    else if (item.Offset < (UInt64)prevOffs + prevSize &&
        (item.Offset != prevOffs || item.Size != prevSize))
      return false;
    
    prevOffs = item.Offset;
    prevSize = item.Size;
  }
  
  return true;
}

}}

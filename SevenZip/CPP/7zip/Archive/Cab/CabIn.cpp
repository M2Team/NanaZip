// Archive/CabIn.cpp

#include "StdAfx.h"

// #include <stdio.h>

#include "../../../../C/CpuArch.h"

#include "../../Common/LimitedStreams.h"
#include "../../Common/StreamUtils.h"

#include "CabIn.h"

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)

namespace NArchive {
namespace NCab {

struct CUnexpectedEndException {};

void CInArchive::Skip(unsigned size)
{
  if (_inBuffer.Skip(size) != size)
    throw CUnexpectedEndException();
}

void CInArchive::Read(Byte *data, unsigned size)
{
  if (_inBuffer.ReadBytes(data, size) != size)
    throw CUnexpectedEndException();
}

void CInArchive::ReadName(AString &s)
{
  for (size_t i = 0; i < ((size_t)1 << 13); i++)
  {
    Byte b;
    if (!_inBuffer.ReadByte(b))
      throw CUnexpectedEndException();
    if (b == 0)
    {
      s.SetFrom((const char *)(const Byte *)_tempBuf, (unsigned)i);
      return;
    }
    if (_tempBuf.Size() == i)
      _tempBuf.ChangeSize_KeepData(i * 2, i);
    _tempBuf[i] = b;
  }
  
  for (;;)
  {
    Byte b;
    if (!_inBuffer.ReadByte(b))
      throw CUnexpectedEndException();
    if (b == 0)
      break;
  }
  
  ErrorInNames = true;
  s = "[ERROR-LONG-PATH]";
}

void CInArchive::ReadOtherArc(COtherArc &oa)
{
  ReadName(oa.FileName);
  ReadName(oa.DiskName);
}


struct CSignatureFinder
{
  Byte *Buf;
  UInt32 Pos;
  UInt32 End;
  const Byte *Signature;
  UInt32 SignatureSize;
  
  UInt32 _HeaderSize;
  UInt32 _AlignSize;
  UInt32 _BufUseCapacity;

  ISequentialInStream *Stream;
  UInt64 Processed; // Global offset of start of Buf

  const UInt64 *SearchLimit;

  UInt32 GetTotalCapacity(UInt32 basicSize, UInt32 headerSize)
  {
    _HeaderSize = headerSize;
    for (_AlignSize = (1 << 5); _AlignSize < _HeaderSize; _AlignSize <<= 1);
    _BufUseCapacity = basicSize + _AlignSize;
    return _BufUseCapacity + 16;
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

    while (End - Pos >= _HeaderSize)
    {
      const Byte *p = Buf + Pos;
      Byte b = Signature[0];
      for (;;)
      {
        if (*p == b) { break; }  p++;
        if (*p == b) { break; }  p++;
      }
      Pos = (UInt32)(p - Buf);
      if (End - Pos < _HeaderSize)
      {
        Pos = End - _HeaderSize + 1;
        break;
      }
      UInt32 i;
      for (i = 1; i < SignatureSize && p[i] == Signature[i]; i++);
      if (i == SignatureSize)
        return S_OK;
      Pos++;
    }

    if (Pos >= _AlignSize)
    {
      UInt32 num = (Pos & ~(_AlignSize - 1));
      Processed += num;
      Pos -= num;
      End -= num;
      memmove(Buf, Buf + num, End);
    }
    UInt32 rem = _BufUseCapacity - End;
    if (SearchLimit)
    {
      if (Processed + Pos > *SearchLimit)
        return S_FALSE;
      UInt64 rem2 = *SearchLimit - (Processed + End) + _HeaderSize;
      if (rem > rem2)
        rem = (UInt32)rem2;
    }
    
    UInt32 processedSize;
    if (Processed == 0 && rem == _BufUseCapacity - _HeaderSize)
      rem -= _AlignSize; // to make reads more aligned.
    RINOK(Stream->Read(Buf + End, rem, &processedSize));
    if (processedSize == 0)
      return S_FALSE;
    End += processedSize;
  }
}


bool CInArcInfo::Parse(const Byte *p)
{
  if (Get32(p + 0x0C) != 0 ||
      Get32(p + 0x14) != 0)
    return false;
  Size = Get32(p + 8);
  if (Size < 36)
    return false;
  Flags = Get16(p + 0x1E);
  if (Flags > 7)
    return false;
  FileHeadersOffset = Get32(p + 0x10);
  if (FileHeadersOffset != 0 && FileHeadersOffset > Size)
    return false;
  VersionMinor = p[0x18];
  VersionMajor = p[0x19];
  NumFolders = Get16(p + 0x1A);
  NumFiles = Get16(p + 0x1C);
  return true;
}
  

HRESULT CInArchive::Open2(CDatabaseEx &db, const UInt64 *searchHeaderSizeLimit)
{
  IsArc = false;
  ErrorInNames = false;
  UnexpectedEnd = false;
  HeaderError = false;

  db.Clear();
  RINOK(db.Stream->Seek(0, STREAM_SEEK_CUR, &db.StartPosition));
  // UInt64 temp = db.StartPosition;

  CByteBuffer buffer;
  CInArcInfo &ai = db.ArcInfo;
  UInt64 startInBuf = 0;

  CLimitedSequentialInStream *limitedStreamSpec = NULL;
  CMyComPtr<ISequentialInStream> limitedStream;

  // for (int iii = 0; iii < 10000; iii++)
  {
    // db.StartPosition = temp; RINOK(db.Stream->Seek(db.StartPosition, STREAM_SEEK_SET, NULL));
    
    const UInt32 kMainHeaderSize = 32;
    Byte header[kMainHeaderSize];
    const UInt32 kBufSize = 1 << 15;
    RINOK(ReadStream_FALSE(db.Stream, header, kMainHeaderSize));
    if (memcmp(header, NHeader::kMarker, NHeader::kMarkerSize) == 0 && ai.Parse(header))
    {
      limitedStreamSpec = new CLimitedSequentialInStream;
      limitedStream = limitedStreamSpec;
      limitedStreamSpec->SetStream(db.Stream);
      limitedStreamSpec->Init(ai.Size - NHeader::kMarkerSize);
      buffer.Alloc(kBufSize);
      memcpy(buffer, header, kMainHeaderSize);
      UInt32 numProcessedBytes;
      RINOK(limitedStream->Read(buffer + kMainHeaderSize, kBufSize - kMainHeaderSize, &numProcessedBytes));
      _inBuffer.SetBuf(buffer, (UInt32)kBufSize, kMainHeaderSize + numProcessedBytes, kMainHeaderSize);
    }
    else
    {
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
      finder.Pos = 1;
  
      for (;;)
      {
        RINOK(finder.Find());
        if (ai.Parse(finder.Buf + finder.Pos))
        {
          db.StartPosition = finder.Processed + finder.Pos;
          limitedStreamSpec = new CLimitedSequentialInStream;
          limitedStreamSpec->SetStream(db.Stream);
          limitedStream = limitedStreamSpec;
          UInt32 remInFinder = finder.End - finder.Pos;
          if (ai.Size <= remInFinder)
          {
            limitedStreamSpec->Init(0);
            finder.End = finder.Pos + ai.Size;
          }
          else
            limitedStreamSpec->Init(ai.Size - remInFinder);

          startInBuf = finder.Pos;
          _inBuffer.SetBuf(buffer, (UInt32)kBufSize, finder.End, finder.Pos + kMainHeaderSize);
          break;
        }
        finder.Pos++;
      }
    }
  }
  
  IsArc = true;

  _inBuffer.SetStream(limitedStream);
  if (_tempBuf.Size() == 0)
    _tempBuf.Alloc(1 << 12);

  Byte p[16];
  unsigned nextSize = 4 + (ai.ReserveBlockPresent() ? 4 : 0);
  Read(p, nextSize);
  ai.SetID = Get16(p);
  ai.CabinetNumber = Get16(p + 2);

  if (ai.ReserveBlockPresent())
  {
    ai.PerCabinet_AreaSize = Get16(p + 4);
    ai.PerFolder_AreaSize = p[6];
    ai.PerDataBlock_AreaSize = p[7];
    Skip(ai.PerCabinet_AreaSize);
  }

  if (ai.IsTherePrev()) ReadOtherArc(ai.PrevArc);
  if (ai.IsThereNext()) ReadOtherArc(ai.NextArc);
  
  UInt32 i;
  
  db.Folders.ClearAndReserve(ai.NumFolders);
  
  for (i = 0; i < ai.NumFolders; i++)
  {
    Read(p, 8);
    CFolder folder;
    folder.DataStart = Get32(p);
    folder.NumDataBlocks = Get16(p + 4);
    folder.MethodMajor = p[6];
    folder.MethodMinor = p[7];
    Skip(ai.PerFolder_AreaSize);
    db.Folders.AddInReserved(folder);
  }
  
  // for (int iii = 0; iii < 10000; iii++) {

  if (_inBuffer.GetProcessedSize() - startInBuf != ai.FileHeadersOffset)
  {
    // printf("\n!!! Seek Error !!!!\n");
    // fflush(stdout);
    RINOK(db.Stream->Seek((Int64)(db.StartPosition + ai.FileHeadersOffset), STREAM_SEEK_SET, NULL));
    limitedStreamSpec->Init(ai.Size - ai.FileHeadersOffset);
    _inBuffer.Init();
  }

  db.Items.ClearAndReserve(ai.NumFiles);

  for (i = 0; i < ai.NumFiles; i++)
  {
    Read(p, 16);
    CItem &item = db.Items.AddNewInReserved();
    item.Size = Get32(p);
    item.Offset = Get32(p + 4);
    item.FolderIndex = Get16(p + 8);
    UInt16 pureDate = Get16(p + 10);
    UInt16 pureTime = Get16(p + 12);
    item.Time = (((UInt32)pureDate << 16)) | pureTime;
    item.Attributes = Get16(p + 14);

    ReadName(item.Name);
    
    if (item.GetFolderIndex(db.Folders.Size()) >= (int)db.Folders.Size())
    {
      HeaderError = true;
      return S_FALSE;
    }
  }
  
  // }
  
  return S_OK;
}


HRESULT CInArchive::Open(CDatabaseEx &db, const UInt64 *searchHeaderSizeLimit)
{
  try
  {
    return Open2(db, searchHeaderSizeLimit);
  }
  catch(const CInBufferException &e) { return e.ErrorCode; }
  catch(CUnexpectedEndException &) { UnexpectedEnd = true; return S_FALSE; }
}



#define RINOZ(x) { int __tt = (x); if (__tt != 0) return __tt; }

static int CompareMvItems(const CMvItem *p1, const CMvItem *p2, void *param)
{
  const CMvDatabaseEx &mvDb = *(const CMvDatabaseEx *)param;
  const CDatabaseEx &db1 = mvDb.Volumes[p1->VolumeIndex];
  const CDatabaseEx &db2 = mvDb.Volumes[p2->VolumeIndex];
  const CItem &item1 = db1.Items[p1->ItemIndex];
  const CItem &item2 = db2.Items[p2->ItemIndex];;
  bool isDir1 = item1.IsDir();
  bool isDir2 = item2.IsDir();
  if (isDir1 && !isDir2) return -1;
  if (isDir2 && !isDir1) return 1;
  int f1 = mvDb.GetFolderIndex(p1);
  int f2 = mvDb.GetFolderIndex(p2);
  RINOZ(MyCompare(f1, f2));
  RINOZ(MyCompare(item1.Offset, item2.Offset));
  RINOZ(MyCompare(item1.Size, item2.Size));
  RINOZ(MyCompare(p1->VolumeIndex, p2->VolumeIndex));
  return MyCompare(p1->ItemIndex, p2->ItemIndex);
}


bool CMvDatabaseEx::AreItemsEqual(unsigned i1, unsigned i2)
{
  const CMvItem *p1 = &Items[i1];
  const CMvItem *p2 = &Items[i2];
  const CDatabaseEx &db1 = Volumes[p1->VolumeIndex];
  const CDatabaseEx &db2 = Volumes[p2->VolumeIndex];
  const CItem &item1 = db1.Items[p1->ItemIndex];
  const CItem &item2 = db2.Items[p2->ItemIndex];;
  return GetFolderIndex(p1) == GetFolderIndex(p2)
      && item1.Offset == item2.Offset
      && item1.Size == item2.Size
      && item1.Name == item2.Name;
}


void CMvDatabaseEx::FillSortAndShrink()
{
  Items.Clear();
  StartFolderOfVol.Clear();
  FolderStartFileIndex.Clear();
  
  int offset = 0;
  
  FOR_VECTOR (v, Volumes)
  {
    const CDatabaseEx &db = Volumes[v];
    int curOffset = offset;
    if (db.IsTherePrevFolder())
      curOffset--;
    StartFolderOfVol.Add(curOffset);
    offset += db.GetNumberOfNewFolders();

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
    int folderIndex = GetFolderIndex(&Items[i]);
    while (folderIndex >= (int)FolderStartFileIndex.Size())
      FolderStartFileIndex.Add(i);
  }
}


bool CMvDatabaseEx::Check()
{
  for (unsigned v = 1; v < Volumes.Size(); v++)
  {
    const CDatabaseEx &db1 = Volumes[v];
    if (db1.IsTherePrevFolder())
    {
      const CDatabaseEx &db0 = Volumes[v - 1];
      if (db0.Folders.IsEmpty() || db1.Folders.IsEmpty())
        return false;
      const CFolder &f0 = db0.Folders.Back();
      const CFolder &f1 = db1.Folders.Front();
      if (f0.MethodMajor != f1.MethodMajor ||
          f0.MethodMinor != f1.MethodMinor)
        return false;
    }
  }

  UInt32 beginPos = 0;
  UInt64 endPos = 0;
  int prevFolder = -2;
  
  FOR_VECTOR (i, Items)
  {
    const CMvItem &mvItem = Items[i];
    int fIndex = GetFolderIndex(&mvItem);
    if (fIndex >= (int)FolderStartFileIndex.Size())
      return false;
    const CItem &item = Volumes[mvItem.VolumeIndex].Items[mvItem.ItemIndex];
    if (item.IsDir())
      continue;
    
    int folderIndex = GetFolderIndex(&mvItem);
  
    if (folderIndex != prevFolder)
      prevFolder = folderIndex;
    else if (item.Offset < endPos &&
        (item.Offset != beginPos || item.GetEndOffset() != endPos))
      return false;
    
    beginPos = item.Offset;
    endPos = item.GetEndOffset();
  }
  
  return true;
}

}}

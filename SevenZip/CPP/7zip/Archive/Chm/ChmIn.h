// Archive/ChmIn.h

#ifndef __ARCHIVE_CHM_IN_H
#define __ARCHIVE_CHM_IN_H

#include "../../../Common/MyBuffer.h"
#include "../../../Common/MyString.h"

#include "../../IStream.h"

#include "../../Common/InBuffer.h"

namespace NArchive {
namespace NChm {

struct CItem
{
  UInt64 Section;
  UInt64 Offset;
  UInt64 Size;
  AString Name;

  bool IsFormatRelatedItem() const
  {
    if (Name.Len() < 2)
      return false;
    return Name[0] == ':' && Name[1] == ':';
  }
  
  bool IsUserItem() const
  {
    if (Name.Len() < 2)
      return false;
    return Name[0] == '/';
  }
  
  bool IsDir() const
  {
    if (Name.IsEmpty())
      return false;
    return (Name.Back() == '/');
  }
};


struct CDatabase
{
  UInt64 StartPosition;
  UInt64 ContentOffset;
  CObjectVector<CItem> Items;
  AString NewFormatString;
  bool Help2Format;
  bool NewFormat;
  UInt64 PhySize;

  void UpdatePhySize(UInt64 v) { if (PhySize < v) PhySize = v; }

  int FindItem(const AString &name) const
  {
    FOR_VECTOR (i, Items)
      if (Items[i].Name == name)
        return i;
    return -1;
  }

  void Clear()
  {
    NewFormat = false;
    NewFormatString.Empty();
    Help2Format = false;
    Items.Clear();
    StartPosition = 0;
    PhySize = 0;
  }
};


const UInt32 kBlockSize = 1 << 15;

struct CResetTable
{
  UInt64 UncompressedSize;
  UInt64 CompressedSize;
  // unsigned BlockSizeBits;
  CRecordVector<UInt64> ResetOffsets;
  
  bool GetCompressedSizeOfBlocks(UInt64 blockIndex, UInt32 numBlocks, UInt64 &size) const
  {
    if (blockIndex >= ResetOffsets.Size())
      return false;
    UInt64 startPos = ResetOffsets[(unsigned)blockIndex];
    if (blockIndex + numBlocks >= ResetOffsets.Size())
      size = CompressedSize - startPos;
    else
      size = ResetOffsets[(unsigned)(blockIndex + numBlocks)] - startPos;
    return true;
  }

  bool GetCompressedSizeOfBlock(UInt64 blockIndex, UInt64 &size) const
  {
    return GetCompressedSizeOfBlocks(blockIndex, 1, size);
  }
  
  UInt64 GetNumBlocks(UInt64 size) const
  {
    return (size + kBlockSize - 1) / kBlockSize;
  }
};


struct CLzxInfo
{
  UInt32 Version;
  
  unsigned ResetIntervalBits;
  unsigned WindowSizeBits;
  UInt32 CacheSize;
  
  CResetTable ResetTable;

  unsigned GetNumDictBits() const
  {
    if (Version == 2 || Version == 3)
      return 15 + WindowSizeBits;
    return 0;
  }

  UInt64 GetFolderSize() const { return kBlockSize << ResetIntervalBits; }
  UInt64 GetFolder(UInt64 offset) const { return offset / GetFolderSize(); }
  UInt64 GetFolderPos(UInt64 folderIndex) const { return folderIndex * GetFolderSize(); }
  UInt64 GetBlockIndexFromFolderIndex(UInt64 folderIndex) const { return folderIndex << ResetIntervalBits; }

  bool GetOffsetOfFolder(UInt64 folderIndex, UInt64 &offset) const
  {
    UInt64 blockIndex = GetBlockIndexFromFolderIndex(folderIndex);
    if (blockIndex >= ResetTable.ResetOffsets.Size())
      return false;
    offset = ResetTable.ResetOffsets[(unsigned)blockIndex];
    return true;
  }
  
  bool GetCompressedSizeOfFolder(UInt64 folderIndex, UInt64 &size) const
  {
    UInt64 blockIndex = GetBlockIndexFromFolderIndex(folderIndex);
    return ResetTable.GetCompressedSizeOfBlocks(blockIndex, (UInt32)1 << ResetIntervalBits, size);
  }
};


struct CMethodInfo
{
  Byte Guid[16];
  CByteBuffer ControlData;
  CLzxInfo LzxInfo;
  
  bool IsLzx() const;
  bool IsDes() const;
  AString GetGuidString() const;
  AString GetName() const;
};


struct CSectionInfo
{
  UInt64 Offset;
  UInt64 CompressedSize;
  UInt64 UncompressedSize;

  AString Name;
  CObjectVector<CMethodInfo> Methods;

  bool IsLzx() const;
  UString GetMethodName() const;
};

class CFilesDatabase: public CDatabase
{
public:
  bool LowLevel;
  CUIntVector Indices;
  CObjectVector<CSectionInfo> Sections;

  UInt64 GetFileSize(unsigned fileIndex) const { return Items[Indices[fileIndex]].Size; }
  UInt64 GetFileOffset(unsigned fileIndex) const { return Items[Indices[fileIndex]].Offset; }

  UInt64 GetFolder(unsigned fileIndex) const
  {
    const CItem &item = Items[Indices[fileIndex]];
    if (item.Section < Sections.Size())
    {
      const CSectionInfo &section = Sections[(unsigned)item.Section];
      if (section.IsLzx())
        return section.Methods[0].LzxInfo.GetFolder(item.Offset);
    }
    return 0;
  }

  UInt64 GetLastFolder(unsigned fileIndex) const
  {
    const CItem &item = Items[Indices[fileIndex]];
    if (item.Section < Sections.Size())
    {
      const CSectionInfo &section = Sections[(unsigned)item.Section];
      if (section.IsLzx())
        return section.Methods[0].LzxInfo.GetFolder(item.Offset + item.Size - 1);
    }
    return 0;
  }

  void HighLevelClear()
  {
    LowLevel = true;
    Indices.Clear();
    Sections.Clear();
  }

  void Clear()
  {
    CDatabase::Clear();
    HighLevelClear();
  }
  
  void SetIndices();
  void Sort();
  bool Check();
  bool CheckSectionRefs();
};


class CInArchive
{
  CMyComPtr<ISequentialInStream> m_InStreamRef;
  ::CInBuffer _inBuffer;
  UInt64 _chunkSize;
  bool _help2;

  Byte ReadByte();
  void ReadBytes(Byte *data, UInt32 size);
  void Skip(size_t size);
  UInt16 ReadUInt16();
  UInt32 ReadUInt32();
  UInt64 ReadUInt64();
  UInt64 ReadEncInt();
  void ReadString(unsigned size, AString &s);
  void ReadUString(unsigned size, UString &s);
  void ReadGUID(Byte *g);

  HRESULT ReadChunk(IInStream *inStream, UInt64 pos, UInt64 size);

  HRESULT ReadDirEntry(CDatabase &database);
  HRESULT DecompressStream(IInStream *inStream, const CDatabase &database, const AString &name);

public:
  bool IsArc;
  bool HeadersError;
  bool UnexpectedEnd;
  bool UnsupportedFeature;

  CInArchive(bool help2) { _help2 = help2; }

  HRESULT OpenChm(IInStream *inStream, CDatabase &database);
  HRESULT OpenHelp2(IInStream *inStream, CDatabase &database);
  HRESULT OpenHighLevel(IInStream *inStream, CFilesDatabase &database);
  HRESULT Open2(IInStream *inStream, const UInt64 *searchHeaderSizeLimit, CFilesDatabase &database);
  HRESULT Open(IInStream *inStream, const UInt64 *searchHeaderSizeLimit, CFilesDatabase &database);
};
  
}}
  
#endif

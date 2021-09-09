// WimHandlerOut.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/IntToString.h"
#include "../../../Common/MyBuffer2.h"
#include "../../../Common/StringToInt.h"
#include "../../../Common/UTFConvert.h"
#include "../../../Common/Wildcard.h"

#include "../../../Windows/PropVariant.h"
#include "../../../Windows/TimeUtils.h"

#include "../../Common/LimitedStreams.h"
#include "../../Common/ProgressUtils.h"
#include "../../Common/StreamUtils.h"
#include "../../Common/UniqBlocks.h"

#include "../../Crypto/RandGen.h"
#include "../../Crypto/Sha1Cls.h"

#include "WimHandler.h"

using namespace NWindows;

namespace NArchive {
namespace NWim {

static int AddUniqHash(const CStreamInfo *streams, CUIntVector &sorted, const Byte *h, int streamIndexForInsert)
{
  unsigned left = 0, right = sorted.Size();
  while (left != right)
  {
    unsigned mid = (left + right) / 2;
    unsigned index = sorted[mid];
    const Byte *hash2 = streams[index].Hash;
    
    unsigned i;
    for (i = 0; i < kHashSize; i++)
      if (h[i] != hash2[i])
        break;
    
    if (i == kHashSize)
      return index;
  
    if (h[i] < hash2[i])
      right = mid;
    else
      left = mid + 1;
  }

  if (streamIndexForInsert >= 0)
    sorted.Insert(left, streamIndexForInsert);
 
  return -1;
}


struct CAltStream
{
  int UpdateIndex;
  int HashIndex;
  UInt64 Size;
  UString Name;
  bool Skip;

  CAltStream(): UpdateIndex(-1), HashIndex(-1), Skip(false) {}
};


struct CMetaItem
{
  int UpdateIndex;
  int HashIndex;
  
  UInt64 Size;
  FILETIME CTime;
  FILETIME ATime;
  FILETIME MTime;
  UInt32 Attrib;
  UInt64 FileID;
  UInt64 VolID;

  UString Name;
  UString ShortName;

  int SecurityId;       // -1: means no secutity ID
  bool IsDir;
  bool Skip;
  unsigned NumSkipAltStreams;
  CObjectVector<CAltStream> AltStreams;

  CByteBuffer Reparse;

  unsigned GetNumAltStreams() const { return AltStreams.Size() - NumSkipAltStreams; }
  CMetaItem():
        UpdateIndex(-1)
      , HashIndex(-1)
      , FileID(0)
      , VolID(0)
      , SecurityId(-1)
      , Skip(false)
      , NumSkipAltStreams(0)
      {}
};


static int Compare_HardLink_MetaItems(const CMetaItem &a1, const CMetaItem &a2)
{
  if (a1.VolID < a2.VolID) return -1;
  if (a1.VolID > a2.VolID) return 1;
  if (a1.FileID < a2.FileID) return -1;
  if (a1.FileID > a2.FileID) return 1;
  if (a1.Size < a2.Size) return -1;
  if (a1.Size > a2.Size) return 1;
  return ::CompareFileTime(&a1.MTime, &a2.MTime);
}


static int AddToHardLinkList(const CObjectVector<CMetaItem> &metaItems, unsigned indexOfItem, CUIntVector &indexes)
{
  const CMetaItem &mi = metaItems[indexOfItem];
  unsigned left = 0, right = indexes.Size();
  while (left != right)
  {
    unsigned mid = (left + right) / 2;
    unsigned index = indexes[mid];
    int comp = Compare_HardLink_MetaItems(mi, metaItems[index]);
    if (comp == 0)
      return index;
    if (comp < 0)
      right = mid;
    else
      left = mid + 1;
  }
  indexes.Insert(left, indexOfItem);
  return -1;
}


struct CUpdateItem
{
  unsigned CallbackIndex; // index in callback
  
  int MetaIndex;          // index in in MetaItems[]
  
  int AltStreamIndex;     // index in CMetaItem::AltStreams vector
                          // -1: if not alt stream?
  
  int InArcIndex;         // >= 0, if we use OLD Data
                          //   -1, if we use NEW Data
 
  CUpdateItem(): MetaIndex(-1), AltStreamIndex(-1), InArcIndex(-1) {}
};


struct CDir
{
  int MetaIndex;
  CObjectVector<CDir> Dirs;
  CUIntVector Files; // indexes in MetaItems[]

  CDir(): MetaIndex(-1) {}
  unsigned GetNumDirs() const;
  unsigned GetNumFiles() const;
  UInt64 GetTotalSize(const CObjectVector<CMetaItem> &metaItems) const;
  bool FindDir(const CObjectVector<CMetaItem> &items, const UString &name, unsigned &index);
};

/* imagex counts Junctions as files (not as dirs).
   We suppose that it's not correct */

unsigned CDir::GetNumDirs() const
{
  unsigned num = Dirs.Size();
  FOR_VECTOR (i, Dirs)
    num += Dirs[i].GetNumDirs();
  return num;
}

unsigned CDir::GetNumFiles() const
{
  unsigned num = Files.Size();
  FOR_VECTOR (i, Dirs)
    num += Dirs[i].GetNumFiles();
  return num;
}

UInt64 CDir::GetTotalSize(const CObjectVector<CMetaItem> &metaItems) const
{
  UInt64 sum = 0;
  unsigned i;
  for (i = 0; i < Files.Size(); i++)
    sum += metaItems[Files[i]].Size;
  for (i = 0; i < Dirs.Size(); i++)
    sum += Dirs[i].GetTotalSize(metaItems);
  return sum;
}

bool CDir::FindDir(const CObjectVector<CMetaItem> &items, const UString &name, unsigned &index)
{
  unsigned left = 0, right = Dirs.Size();
  while (left != right)
  {
    unsigned mid = (left + right) / 2;
    int comp = CompareFileNames(name, items[Dirs[mid].MetaIndex].Name);
    if (comp == 0)
    {
      index = mid;
      return true;
    }
    if (comp < 0)
      right = mid;
    else
      left = mid + 1;
  }
  index = left;
  return false;
}


STDMETHODIMP CHandler::GetFileTimeType(UInt32 *type)
{
  *type = NFileTimeType::kWindows;
  return S_OK;
}


HRESULT CHandler::GetOutProperty(IArchiveUpdateCallback *callback, UInt32 callbackIndex, Int32 arcIndex, PROPID propID, PROPVARIANT *value)
{
  if (arcIndex >= 0)
    return GetProperty(arcIndex, propID, value);
  return callback->GetProperty(callbackIndex, propID, value);
}


HRESULT CHandler::GetTime(IArchiveUpdateCallback *callback, UInt32 callbackIndex, Int32 arcIndex, PROPID propID, FILETIME &ft)
{
  ft.dwLowDateTime = ft.dwHighDateTime = 0;
  NCOM::CPropVariant prop;
  RINOK(GetOutProperty(callback, callbackIndex, arcIndex, propID, &prop));
  if (prop.vt == VT_FILETIME)
    ft = prop.filetime;
  else if (prop.vt != VT_EMPTY)
    return E_INVALIDARG;
  return S_OK;
}


static HRESULT GetRootTime(
    IArchiveGetRootProps *callback,
    IArchiveGetRootProps *arcRoot,
    PROPID propID, FILETIME &ft)
{
  NCOM::CPropVariant prop;
  if (callback)
  {
    RINOK(callback->GetRootProp(propID, &prop));
    if (prop.vt == VT_FILETIME)
    {
      ft = prop.filetime;
      return S_OK;
    }
    if (prop.vt != VT_EMPTY)
      return E_INVALIDARG;
  }
  if (arcRoot)
  {
    RINOK(arcRoot->GetRootProp(propID, &prop));
    if (prop.vt == VT_FILETIME)
    {
      ft = prop.filetime;
      return S_OK;
    }
    if (prop.vt != VT_EMPTY)
      return E_INVALIDARG;
  }
  return S_OK;
}

#define Set16(p, d) SetUi16(p, d)
#define Set32(p, d) SetUi32(p, d)
#define Set64(p, d) SetUi64(p, d)

void CResource::WriteTo(Byte *p) const
{
  Set64(p, PackSize);
  p[7] = Flags;
  Set64(p + 8, Offset);
  Set64(p + 16, UnpackSize);
}


void CHeader::WriteTo(Byte *p) const
{
  memcpy(p, kSignature, kSignatureSize);
  Set32(p + 8, kHeaderSizeMax);
  Set32(p + 0xC, Version);
  Set32(p + 0x10, Flags);
  Set32(p + 0x14, ChunkSize);
  memcpy(p + 0x18, Guid, 16);
  Set16(p + 0x28, PartNumber);
  Set16(p + 0x2A, NumParts);
  Set32(p + 0x2C, NumImages);
  OffsetResource.WriteTo(p + 0x30);
  XmlResource.WriteTo(p + 0x48);
  MetadataResource.WriteTo(p + 0x60);
  IntegrityResource.WriteTo(p + 0x7C);
  Set32(p + 0x78, BootIndex);
  memset(p + 0x94, 0, 60);
}


void CStreamInfo::WriteTo(Byte *p) const
{
  Resource.WriteTo(p);
  Set16(p + 0x18, PartNumber);
  Set32(p + 0x1A, RefCount);
  memcpy(p + 0x1E, Hash, kHashSize);
}


class CInStreamWithSha1:
  public ISequentialInStream,
  public CMyUnknownImp
{
  CMyComPtr<ISequentialInStream> _stream;
  UInt64 _size;
  // NCrypto::NSha1::CContext _sha;
  CAlignedBuffer _sha;
  CSha1 *Sha() { return (CSha1 *)(void *)(Byte *)_sha; }
public:
  MY_UNKNOWN_IMP1(IInStream)
  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);

  CInStreamWithSha1(): _sha(sizeof(CSha1)) {}
  void SetStream(ISequentialInStream *stream) { _stream = stream;  }
  void Init()
  {
    _size = 0;
    Sha1_Init(Sha());
  }
  void ReleaseStream() { _stream.Release(); }
  UInt64 GetSize() const { return _size; }
  void Final(Byte *digest) { Sha1_Final(Sha(), digest); }
};

STDMETHODIMP CInStreamWithSha1::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  UInt32 realProcessedSize;
  HRESULT result = _stream->Read(data, size, &realProcessedSize);
  _size += realProcessedSize;
  Sha1_Update(Sha(), (const Byte *)data, realProcessedSize);
  if (processedSize)
    *processedSize = realProcessedSize;
  return result;
}


static void SetFileTimeToMem(Byte *p, const FILETIME &ft)
{
  Set32(p, ft.dwLowDateTime);
  Set32(p + 4, ft.dwHighDateTime);
}

static size_t WriteItem_Dummy(const CMetaItem &item)
{
  if (item.Skip)
    return 0;
  unsigned fileNameLen = item.Name.Len() * 2;
  // we write fileNameLen + 2 + 2 to be same as original WIM.
  unsigned fileNameLen2 = (fileNameLen == 0 ? 0 : fileNameLen + 2);

  unsigned shortNameLen = item.ShortName.Len() * 2;
  unsigned shortNameLen2 = (shortNameLen == 0 ? 2 : shortNameLen + 4);

  size_t totalLen = ((kDirRecordSize + fileNameLen2 + shortNameLen2 + 6) & ~7);
  if (item.GetNumAltStreams() != 0)
  {
    if (!item.IsDir)
    {
      UInt32 curLen = (((0x26 + 0) + 6) & ~7);
      totalLen += curLen;
    }
    FOR_VECTOR (i, item.AltStreams)
    {
      const CAltStream &ss = item.AltStreams[i];
      if (ss.Skip)
        continue;
      fileNameLen = ss.Name.Len() * 2;
      fileNameLen2 = (fileNameLen == 0 ? 0 : fileNameLen + 2 + 2);
      UInt32 curLen = (((0x26 + fileNameLen2) + 6) & ~7);
      totalLen += curLen;
    }
  }
  return totalLen;
}


static size_t WriteItem(const CStreamInfo *streams, const CMetaItem &item, Byte *p)
{
  if (item.Skip)
    return 0;
  unsigned fileNameLen = item.Name.Len() * 2;
  unsigned fileNameLen2 = (fileNameLen == 0 ? 0 : fileNameLen + 2);
  unsigned shortNameLen = item.ShortName.Len() * 2;
  unsigned shortNameLen2 = (shortNameLen == 0 ? 2 : shortNameLen + 4);

  size_t totalLen = ((kDirRecordSize + fileNameLen2 + shortNameLen2 + 6) & ~7);
  
  memset(p, 0, totalLen);
  Set64(p, totalLen);
  Set64(p + 8, item.Attrib);
  Set32(p + 0xC, (Int32)item.SecurityId);
  SetFileTimeToMem(p + 0x28, item.CTime);
  SetFileTimeToMem(p + 0x30, item.ATime);
  SetFileTimeToMem(p + 0x38, item.MTime);
  
  /* WIM format probably doesn't support hard links to symbolic links.
     In these cases it just stores symbolic links (REPARSE TAGS).
     Check it in new versions of WIM software form MS !!!
     We also follow that scheme */

  if (item.Reparse.Size() != 0)
  {
    UInt32 tag = GetUi32(item.Reparse);
    Set32(p + 0x58, tag);
    // Set32(p + 0x5C, 0); // probably it's always ZERO
  }
  else if (item.FileID != 0)
  {
    Set64(p + 0x58, item.FileID);
  }
  
  Set16(p + 0x62, (UInt16)shortNameLen);
  Set16(p + 0x64, (UInt16)fileNameLen);
  unsigned i;
  for (i = 0; i * 2 < fileNameLen; i++)
    Set16(p + kDirRecordSize + i * 2, (UInt16)item.Name[i]);
  for (i = 0; i * 2 < shortNameLen; i++)
    Set16(p + kDirRecordSize + fileNameLen2 + i * 2, (UInt16)item.ShortName[i]);
  
  if (item.GetNumAltStreams() == 0)
  {
    if (item.HashIndex >= 0)
      memcpy(p + 0x40, streams[item.HashIndex].Hash, kHashSize);
  }
  else
  {
    Set16(p + 0x60, (UInt16)(item.GetNumAltStreams() + (item.IsDir ? 0 : 1)));
    p += totalLen;
    
    if (!item.IsDir)
    {
      UInt32 curLen = (((0x26 + 0) + 6) & ~7);
      memset(p, 0, curLen);
      Set64(p, curLen);
      if (item.HashIndex >= 0)
        memcpy(p + 0x10, streams[item.HashIndex].Hash, kHashSize);
      totalLen += curLen;
      p += curLen;
    }
    
    FOR_VECTOR (si, item.AltStreams)
    {
      const CAltStream &ss = item.AltStreams[si];
      if (ss.Skip)
        continue;
      
      fileNameLen = ss.Name.Len() * 2;
      fileNameLen2 = (fileNameLen == 0 ? 0 : fileNameLen + 2 + 2);
      UInt32 curLen = (((0x26 + fileNameLen2) + 6) & ~7);
      memset(p, 0, curLen);
      
      Set64(p, curLen);
      if (ss.HashIndex >= 0)
        memcpy(p + 0x10, streams[ss.HashIndex].Hash, kHashSize);
      Set16(p + 0x24, (UInt16)fileNameLen);
      for (i = 0; i * 2 < fileNameLen; i++)
        Set16(p + 0x26 + i * 2, (UInt16)ss.Name[i]);
      totalLen += curLen;
      p += curLen;
    }
  }
  
  return totalLen;
}


struct CDb
{
  CMetaItem DefaultDirItem;
  const CStreamInfo *Hashes;
  CObjectVector<CMetaItem> MetaItems;
  CRecordVector<CUpdateItem> UpdateItems;
  CUIntVector UpdateIndexes; /* indexes in UpdateItems in order of writing data streams
                                to disk (the order of tree items). */

  size_t WriteTree_Dummy(const CDir &tree) const;
  void WriteTree(const CDir &tree, Byte *dest, size_t &pos)  const;
  void WriteOrderList(const CDir &tree);
};


size_t CDb::WriteTree_Dummy(const CDir &tree) const
{
  unsigned i;
  size_t pos = 0;
  for (i = 0; i < tree.Files.Size(); i++)
    pos += WriteItem_Dummy(MetaItems[tree.Files[i]]);
  for (i = 0; i < tree.Dirs.Size(); i++)
  {
    const CDir &subDir = tree.Dirs[i];
    pos += WriteItem_Dummy(MetaItems[subDir.MetaIndex]);
    pos += WriteTree_Dummy(subDir);
  }
  return pos + 8;
}


void CDb::WriteTree(const CDir &tree, Byte *dest, size_t &pos) const
{
  unsigned i;
  for (i = 0; i < tree.Files.Size(); i++)
    pos += WriteItem(Hashes, MetaItems[tree.Files[i]], dest + pos);

  size_t posStart = pos;
  for (i = 0; i < tree.Dirs.Size(); i++)
    pos += WriteItem_Dummy(MetaItems[tree.Dirs[i].MetaIndex]);

  Set64(dest + pos, 0);

  pos += 8;

  for (i = 0; i < tree.Dirs.Size(); i++)
  {
    const CDir &subDir = tree.Dirs[i];
    const CMetaItem &metaItem = MetaItems[subDir.MetaIndex];
    bool needCreateTree = (metaItem.Reparse.Size() == 0)
        || !subDir.Files.IsEmpty()
        || !subDir.Dirs.IsEmpty();
    size_t len = WriteItem(Hashes, metaItem, dest + posStart);
    posStart += len;
    if (needCreateTree)
    {
      Set64(dest + posStart - len + 0x10, pos); // subdirOffset
      WriteTree(subDir, dest, pos);
    }
  }
}


void CDb::WriteOrderList(const CDir &tree)
{
  if (tree.MetaIndex >= 0)
  {
    const CMetaItem &mi = MetaItems[tree.MetaIndex];
    if (mi.UpdateIndex >= 0)
      UpdateIndexes.Add(mi.UpdateIndex);
    FOR_VECTOR (si, mi.AltStreams)
      UpdateIndexes.Add(mi.AltStreams[si].UpdateIndex);
  }

  unsigned i;
  for (i = 0; i < tree.Files.Size(); i++)
  {
    const CMetaItem &mi = MetaItems[tree.Files[i]];
    UpdateIndexes.Add(mi.UpdateIndex);
    FOR_VECTOR (si, mi.AltStreams)
      UpdateIndexes.Add(mi.AltStreams[si].UpdateIndex);
  }

  for (i = 0; i < tree.Dirs.Size(); i++)
    WriteOrderList(tree.Dirs[i]);
}


static void AddTag_ToString(AString &s, const char *name, const char *value)
{
  s += '<';
  s += name;
  s += '>';
  s += value;
  s += '<';
  s += '/';
  s += name;
  s += '>';
}


static void AddTagUInt64_ToString(AString &s, const char *name, UInt64 value)
{
  char temp[32];
  ConvertUInt64ToString(value, temp);
  AddTag_ToString(s, name, temp);
}


static CXmlItem &AddUniqueTag(CXmlItem &parentItem, const char *name)
{
  int index = parentItem.FindSubTag(name);
  if (index < 0)
  {
    CXmlItem &subItem = parentItem.SubItems.AddNew();
    subItem.IsTag = true;
    subItem.Name = name;
    return subItem;
  }
  CXmlItem &subItem = parentItem.SubItems[index];
  subItem.SubItems.Clear();
  return subItem;
}


static void AddTag_UInt64_2(CXmlItem &item, UInt64 value)
{
  CXmlItem &subItem = item.SubItems.AddNew();
  subItem.IsTag = false;
  char temp[32];
  ConvertUInt64ToString(value, temp);
  subItem.Name = temp;
}


static void AddTag_UInt64(CXmlItem &parentItem, const char *name, UInt64 value)
{
  AddTag_UInt64_2(AddUniqueTag(parentItem, name), value);
}


static void AddTag_Hex(CXmlItem &item, const char *name, UInt32 value)
{
  item.IsTag = true;
  item.Name = name;
  char temp[16];
  temp[0] = '0';
  temp[1] = 'x';
  ConvertUInt32ToHex8Digits(value, temp + 2);
  CXmlItem &subItem = item.SubItems.AddNew();
  subItem.IsTag = false;
  subItem.Name = temp;
}


static void AddTag_Time_2(CXmlItem &item, const FILETIME &ft)
{
  AddTag_Hex(item.SubItems.AddNew(), "HIGHPART", ft.dwHighDateTime);
  AddTag_Hex(item.SubItems.AddNew(), "LOWPART", ft.dwLowDateTime);
}


static void AddTag_Time(CXmlItem &parentItem, const char *name, const FILETIME &ft)
{
  AddTag_Time_2(AddUniqueTag(parentItem, name), ft);
}


static void AddTag_String_IfEmpty(CXmlItem &parentItem, const char *name, const char *value)
{
  int index = parentItem.FindSubTag(name);
  if (index >= 0)
    return;
  CXmlItem &tag = parentItem.SubItems.AddNew();
  tag.IsTag = true;
  tag.Name = name;
  CXmlItem &subItem = tag.SubItems.AddNew();
  subItem.IsTag = false;
  subItem.Name = value;
}


void CHeader::SetDefaultFields(bool useLZX)
{
  Version = k_Version_NonSolid;
  Flags = NHeaderFlags::kReparsePointFixup;
  ChunkSize = 0;
  if (useLZX)
  {
    Flags |= NHeaderFlags::kCompression | NHeaderFlags::kLZX;
    ChunkSize = kChunkSize;
    ChunkSizeBits = kChunkSizeBits;
  }
  MY_RAND_GEN(Guid, 16);
  PartNumber = 1;
  NumParts = 1;
  NumImages = 1;
  BootIndex = 0;
  OffsetResource.Clear();
  XmlResource.Clear();
  MetadataResource.Clear();
  IntegrityResource.Clear();
}


static void AddTrees(CObjectVector<CDir> &trees, CObjectVector<CMetaItem> &metaItems, const CMetaItem &ri, int curTreeIndex)
{
  while (curTreeIndex >= (int)trees.Size())
    trees.AddNew().Dirs.AddNew().MetaIndex = metaItems.Add(ri);
}


#define IS_LETTER_CHAR(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z'))



STDMETHODIMP CHandler::UpdateItems(ISequentialOutStream *outSeqStream, UInt32 numItems, IArchiveUpdateCallback *callback)
{
  COM_TRY_BEGIN

  if (!IsUpdateSupported())
    return E_NOTIMPL;

  bool isUpdate = (_volumes.Size() != 0);
  int defaultImageIndex = _defaultImageNumber - 1;
  bool showImageNumber;

  if (isUpdate)
  {
    showImageNumber = _showImageNumber;
    if (!showImageNumber)
      defaultImageIndex = _db.IndexOfUserImage;
  }
  else
  {
    showImageNumber = (_set_use_ShowImageNumber && _set_showImageNumber);
    if (!showImageNumber)
      defaultImageIndex = 0;
  }

  if (defaultImageIndex >= kNumImagesMaxUpdate)
    return E_NOTIMPL;

  CMyComPtr<IOutStream> outStream;
  RINOK(outSeqStream->QueryInterface(IID_IOutStream, (void **)&outStream));
  if (!outStream)
    return E_NOTIMPL;
  if (!callback)
    return E_FAIL;

  CDb db;
  CObjectVector<CDir> trees;

  CMetaItem ri; // default DIR item
  FILETIME ftCur;
  NTime::GetCurUtcFileTime(ftCur);
  ri.MTime = ri.ATime = ri.CTime = ftCur;
  ri.Attrib = FILE_ATTRIBUTE_DIRECTORY;
  ri.IsDir = true;


  // ---------- Detect changed images ----------

  unsigned i;
  CBoolVector isChangedImage;
  {
    CUIntVector numUnchangedItemsInImage;
    for (i = 0; i < _db.Images.Size(); i++)
    {
      numUnchangedItemsInImage.Add(0);
      isChangedImage.Add(false);
    }
    
    for (i = 0; i < numItems; i++)
    {
      UInt32 indexInArchive;
      Int32 newData, newProps;
      RINOK(callback->GetUpdateItemInfo(i, &newData, &newProps, &indexInArchive));
      if (newProps == 0)
      {
        if (indexInArchive >= _db.SortedItems.Size())
          continue;
        const CItem &item = _db.Items[_db.SortedItems[indexInArchive]];
        if (newData == 0)
        {
          if (item.ImageIndex >= 0)
            numUnchangedItemsInImage[item.ImageIndex]++;
        }
        else
        {
          // oldProps & newData. Current version of 7-Zip doesn't use it
          if (item.ImageIndex >= 0)
            isChangedImage[item.ImageIndex] = true;
        }
      }
      else if (!showImageNumber)
      {
        if (defaultImageIndex >= 0 && defaultImageIndex < (int)isChangedImage.Size())
          isChangedImage[defaultImageIndex] = true;
      }
      else
      {
        NCOM::CPropVariant prop;
        RINOK(callback->GetProperty(i, kpidPath, &prop));
        
        if (prop.vt != VT_BSTR)
          return E_INVALIDARG;
        const wchar_t *path = prop.bstrVal;
        if (!path)
          return E_INVALIDARG;

        const wchar_t *end;
        UInt64 val = ConvertStringToUInt64(path, &end);
        if (end == path)
          return E_INVALIDARG;
        if (val == 0 || val > kNumImagesMaxUpdate)
          return E_INVALIDARG;
        wchar_t c = *end;
        if (c != 0 && c != ':' && c != L'/' && c != WCHAR_PATH_SEPARATOR)
          return E_INVALIDARG;
        unsigned imageIndex = (unsigned)val - 1;
        if (imageIndex < _db.Images.Size())
          isChangedImage[imageIndex] = true;
        if (_defaultImageNumber > 0 && val != (unsigned)_defaultImageNumber)
          return E_INVALIDARG;
      }
    }
    
    for (i = 0; i < _db.Images.Size(); i++)
      if (!isChangedImage[i])
        isChangedImage[i] = _db.GetNumUserItemsInImage(i) != numUnchangedItemsInImage[i];
  }

  if (defaultImageIndex >= 0)
  {
    for (i = 0; i < _db.Images.Size(); i++)
      if ((int)i != defaultImageIndex)
        isChangedImage[i] = false;
  }

  CMyComPtr<IArchiveGetRawProps> getRawProps;
  callback->QueryInterface(IID_IArchiveGetRawProps, (void **)&getRawProps);

  CMyComPtr<IArchiveGetRootProps> getRootProps;
  callback->QueryInterface(IID_IArchiveGetRootProps, (void **)&getRootProps);

  CObjectVector<CUniqBlocks> secureBlocks;

  if (!showImageNumber && (getRootProps || isUpdate) &&
      (
        defaultImageIndex >= (int)isChangedImage.Size()
        || defaultImageIndex < 0 // test it
        || isChangedImage[defaultImageIndex]
      ))
  {
    // Fill Root Item: Metadata and security
    CMetaItem rootItem = ri;
    {
      const void *data = NULL;
      UInt32 dataSize = 0;
      UInt32 propType = 0;
      if (getRootProps)
      {
        RINOK(getRootProps->GetRootRawProp(kpidNtSecure, &data, &dataSize, &propType));
      }
      if (dataSize == 0 && isUpdate)
      {
        RINOK(GetRootRawProp(kpidNtSecure, &data, &dataSize, &propType));
      }
      if (dataSize != 0)
      {
        if (propType != NPropDataType::kRaw)
          return E_FAIL;
        while (defaultImageIndex >= (int)secureBlocks.Size())
          secureBlocks.AddNew();
        CUniqBlocks &secUniqBlocks = secureBlocks[defaultImageIndex];
        rootItem.SecurityId = secUniqBlocks.AddUniq((const Byte *)data, dataSize);
      }
    }
    
    IArchiveGetRootProps *thisGetRoot = isUpdate ? this : NULL;
    
    RINOK(GetRootTime(getRootProps, thisGetRoot, kpidCTime, rootItem.CTime));
    RINOK(GetRootTime(getRootProps, thisGetRoot, kpidATime, rootItem.ATime));
    RINOK(GetRootTime(getRootProps, thisGetRoot, kpidMTime, rootItem.MTime));
    
    {
      NCOM::CPropVariant prop;
      if (getRootProps)
      {
        RINOK(getRootProps->GetRootProp(kpidAttrib, &prop));
        if (prop.vt == VT_UI4)
          rootItem.Attrib = prop.ulVal;
        else if (prop.vt != VT_EMPTY)
          return E_INVALIDARG;
      }
      if (prop.vt == VT_EMPTY && thisGetRoot)
      {
        RINOK(GetRootProp(kpidAttrib, &prop));
        if (prop.vt == VT_UI4)
          rootItem.Attrib = prop.ulVal;
        else if (prop.vt != VT_EMPTY)
          return E_INVALIDARG;
      }
      rootItem.Attrib |= FILE_ATTRIBUTE_DIRECTORY;
    }
    
    AddTrees(trees, db.MetaItems, ri, defaultImageIndex);
    db.MetaItems[trees[defaultImageIndex].Dirs[0].MetaIndex] = rootItem;
  }

  // ---------- Request Metadata for changed items ----------

  UString fileName;
  
  for (i = 0; i < numItems; i++)
  {
    CUpdateItem ui;
    UInt32 indexInArchive;
    Int32 newData, newProps;
    RINOK(callback->GetUpdateItemInfo(i, &newData, &newProps, &indexInArchive));

    if (newData == 0 || newProps == 0)
    {
      if (indexInArchive >= _db.SortedItems.Size())
        continue;
      
      const CItem &item = _db.Items[_db.SortedItems[indexInArchive]];
      
      if (item.ImageIndex >= 0)
      {
        if (!isChangedImage[item.ImageIndex])
        {
          if (newData == 0 && newProps == 0)
            continue;
          return E_FAIL;
        }
      }
      else
      {
        // if deleted item was not renamed, we just skip it
        if (newProps == 0)
          continue;
        if (item.StreamIndex >= 0)
        {
          // we don't support property change for SolidBig streams
          if (_db.DataStreams[item.StreamIndex].Resource.IsSolidBig())
            return E_NOTIMPL;
        }
      }
    
      if (newData == 0)
        ui.InArcIndex = indexInArchive;
    }

    // we set arcIndex only if we must use old props
    Int32 arcIndex = (newProps ? -1 : indexInArchive);

    bool isDir = false;
    {
      NCOM::CPropVariant prop;
      RINOK(GetOutProperty(callback, i, arcIndex, kpidIsDir, &prop));
      if (prop.vt == VT_BOOL)
        isDir = (prop.boolVal != VARIANT_FALSE);
      else if (prop.vt != VT_EMPTY)
        return E_INVALIDARG;
    }

    bool isAltStream = false;
    {
      NCOM::CPropVariant prop;
      RINOK(GetOutProperty(callback, i, arcIndex, kpidIsAltStream, &prop));
      if (prop.vt == VT_BOOL)
        isAltStream = (prop.boolVal != VARIANT_FALSE);
      else if (prop.vt != VT_EMPTY)
        return E_INVALIDARG;
    }

    if (isDir && isAltStream)
      return E_INVALIDARG;

    UInt64 size = 0;
    UInt64 iNode = 0;

    if (!isDir)
    {
      if (!newData)
      {
        NCOM::CPropVariant prop;
        GetProperty(indexInArchive, kpidINode, &prop);
        if (prop.vt == VT_UI8)
          iNode = prop.uhVal.QuadPart;
      }

      NCOM::CPropVariant prop;
      
      if (newData)
      {
        RINOK(callback->GetProperty(i, kpidSize, &prop));
      }
      else
      {
        RINOK(GetProperty(indexInArchive, kpidSize, &prop));
      }
     
      if (prop.vt == VT_UI8)
        size = prop.uhVal.QuadPart;
      else if (prop.vt != VT_EMPTY)
        return E_INVALIDARG;
    }

    {
      NCOM::CPropVariant propPath;
      const wchar_t *path = NULL;
      RINOK(GetOutProperty(callback, i, arcIndex, kpidPath, &propPath));
      if (propPath.vt == VT_BSTR)
        path = propPath.bstrVal;
      else if (propPath.vt != VT_EMPTY)
        return E_INVALIDARG;
    
    if (!path)
      return E_INVALIDARG;

    CDir *curItem = NULL;
    bool isRootImageDir = false;
    fileName.Empty();

    int imageIndex;
    
    if (!showImageNumber)
    {
      imageIndex = defaultImageIndex;
      AddTrees(trees, db.MetaItems, ri, imageIndex);
      curItem = &trees[imageIndex].Dirs[0];
    }
    else
    {
      const wchar_t *end;
      UInt64 val = ConvertStringToUInt64(path, &end);
      if (end == path)
        return E_INVALIDARG;
      if (val == 0 || val > kNumImagesMaxUpdate)
        return E_INVALIDARG;
      
      imageIndex = (int)val - 1;
      if (imageIndex < (int)isChangedImage.Size())
       if (!isChangedImage[imageIndex])
          return E_FAIL;

      AddTrees(trees, db.MetaItems, ri, imageIndex);
      curItem = &trees[imageIndex].Dirs[0];
      wchar_t c = *end;
      
      if (c == 0)
      {
        if (!isDir || isAltStream)
          return E_INVALIDARG;
        ui.MetaIndex = curItem->MetaIndex;
        isRootImageDir = true;
      }
      else if (c == ':')
      {
        if (isDir || !isAltStream)
          return E_INVALIDARG;
        ui.MetaIndex = curItem->MetaIndex;
        CAltStream ss;
        ss.Size = size;
        ss.Name = end + 1;
        ss.UpdateIndex = db.UpdateItems.Size();
        ui.AltStreamIndex = db.MetaItems[ui.MetaIndex].AltStreams.Add(ss);
      }
      else if (c == WCHAR_PATH_SEPARATOR || c == L'/')
      {
        path = end + 1;
        if (*path == 0)
          return E_INVALIDARG;
      }
      else
        return E_INVALIDARG;
    }
      
    if (ui.MetaIndex < 0)
    {
      for (;;)
      {
        wchar_t c = *path++;
        if (c == 0)
          break;
        if (c == WCHAR_PATH_SEPARATOR || c == L'/')
        {
          unsigned indexOfDir;
          if (!curItem->FindDir(db.MetaItems, fileName, indexOfDir))
          {
            CDir &dir = curItem->Dirs.InsertNew(indexOfDir);
            dir.MetaIndex = db.MetaItems.Add(ri);
            db.MetaItems.Back().Name = fileName;
          }
          curItem = &curItem->Dirs[indexOfDir];
          fileName.Empty();
        }
        else
        {
          /*
          #if WCHAR_MAX > 0xffff
          if (c >= 0x10000)
          {
            c -= 0x10000;

            if (c < (1 << 20))
            {
              wchar_t c0 = 0xd800 + ((c >> 10) & 0x3FF);
              fileName += c0;
              c = 0xdc00 + (c & 0x3FF);
            }
            else
              c = '_'; // we change character unsupported by UTF16
          }
          #endif
          */

          fileName += c;
        }
      }

      if (isAltStream)
      {
        int colonPos = fileName.Find(L':');
        if (colonPos < 0)
          return E_INVALIDARG;
        
        // we want to support cases of c::substream, where c: is drive name
        if (colonPos == 1 && fileName[2] == L':' && IS_LETTER_CHAR(fileName[0]))
          colonPos = 2;
        const UString mainName = fileName.Left(colonPos);
        unsigned indexOfDir;
        
        if (mainName.IsEmpty())
          ui.MetaIndex = curItem->MetaIndex;
        else if (curItem->FindDir(db.MetaItems, mainName, indexOfDir))
          ui.MetaIndex = curItem->Dirs[indexOfDir].MetaIndex;
        else
        {
          for (int j = (int)curItem->Files.Size() - 1; j >= 0; j--)
          {
            int metaIndex = curItem->Files[j];
            const CMetaItem &mi = db.MetaItems[metaIndex];
            if (CompareFileNames(mainName, mi.Name) == 0)
            {
              ui.MetaIndex = metaIndex;
              break;
            }
          }
        }
        
        if (ui.MetaIndex >= 0)
        {
          CAltStream ss;
          ss.Size = size;
          ss.Name = fileName.Ptr(colonPos + 1);
          ss.UpdateIndex = db.UpdateItems.Size();
          ui.AltStreamIndex = db.MetaItems[ui.MetaIndex].AltStreams.Add(ss);
        }
      }
    }


    if (ui.MetaIndex < 0 || isRootImageDir)
    {
      if (!isRootImageDir)
      {
        ui.MetaIndex = db.MetaItems.Size();
        db.MetaItems.AddNew();
      }
    
      CMetaItem &mi = db.MetaItems[ui.MetaIndex];
      mi.Size = size;
      mi.IsDir = isDir;
      mi.Name = fileName;
      mi.UpdateIndex = db.UpdateItems.Size();
      {
        NCOM::CPropVariant prop;
        RINOK(GetOutProperty(callback, i, arcIndex, kpidAttrib, &prop));
        if (prop.vt == VT_EMPTY)
          mi.Attrib = 0;
        else if (prop.vt == VT_UI4)
          mi.Attrib = prop.ulVal;
        else
          return E_INVALIDARG;
        if (isDir)
          mi.Attrib |= FILE_ATTRIBUTE_DIRECTORY;
      }
      RINOK(GetTime(callback, i, arcIndex, kpidCTime, mi.CTime));
      RINOK(GetTime(callback, i, arcIndex, kpidATime, mi.ATime));
      RINOK(GetTime(callback, i, arcIndex, kpidMTime, mi.MTime));

      {
        NCOM::CPropVariant prop;
        RINOK(GetOutProperty(callback, i, arcIndex, kpidShortName, &prop));
        if (prop.vt == VT_BSTR)
          mi.ShortName.SetFromBstr(prop.bstrVal);
        else if (prop.vt != VT_EMPTY)
          return E_INVALIDARG;
      }

      while (imageIndex >= (int)secureBlocks.Size())
        secureBlocks.AddNew();
      
      if (!isAltStream && (getRawProps || arcIndex >= 0))
      {
        CUniqBlocks &secUniqBlocks = secureBlocks[imageIndex];
        const void *data;
        UInt32 dataSize;
        UInt32 propType;
        
        data = NULL;
        dataSize = 0;
        propType = 0;
        
        if (arcIndex >= 0)
        {
          GetRawProp(arcIndex, kpidNtSecure, &data, &dataSize, &propType);
        }
        else
        {
          getRawProps->GetRawProp(i, kpidNtSecure, &data, &dataSize, &propType);
        }
        
        if (dataSize != 0)
        {
          if (propType != NPropDataType::kRaw)
            return E_FAIL;
          mi.SecurityId = secUniqBlocks.AddUniq((const Byte *)data, dataSize);
        }

        data = NULL;
        dataSize = 0;
        propType = 0;
        
        if (arcIndex >= 0)
        {
          GetRawProp(arcIndex, kpidNtReparse, &data, &dataSize, &propType);
        }
        else
        {
          getRawProps->GetRawProp(i, kpidNtReparse, &data, &dataSize, &propType);
        }
      
        if (dataSize != 0)
        {
          if (propType != NPropDataType::kRaw)
            return E_FAIL;
          mi.Reparse.CopyFrom((const Byte *)data, dataSize);
        }
      }

      if (!isRootImageDir)
      {
        if (isDir)
        {
          unsigned indexOfDir;
          if (curItem->FindDir(db.MetaItems, fileName, indexOfDir))
            curItem->Dirs[indexOfDir].MetaIndex = ui.MetaIndex;
          else
            curItem->Dirs.InsertNew(indexOfDir).MetaIndex = ui.MetaIndex;
        }
        else
          curItem->Files.Add(ui.MetaIndex);
      }
    }
    
    }
    
    if (iNode != 0 && ui.MetaIndex >= 0 && ui.AltStreamIndex < 0)
      db.MetaItems[ui.MetaIndex].FileID = iNode;

    ui.CallbackIndex = i;
    db.UpdateItems.Add(ui);
  }

  unsigned numNewImages = trees.Size();
  for (i = numNewImages; i < isChangedImage.Size(); i++)
    if (!isChangedImage[i])
      numNewImages = i + 1;

  AddTrees(trees, db.MetaItems, ri, numNewImages - 1);

  for (i = 0; i < trees.Size(); i++)
    if (i >= isChangedImage.Size() || isChangedImage[i])
      db.WriteOrderList(trees[i]);


  UInt64 complexity = 0;

  unsigned numDataStreams = _db.DataStreams.Size();
  CUIntArr streamsRefs(numDataStreams);
  for (i = 0; i < numDataStreams; i++)
    streamsRefs[i] = 0;

  // ---------- Calculate Streams Refs Counts in unchanged images

  for (i = 0; i < _db.Images.Size(); i++)
  {
    if (isChangedImage[i])
      continue;
    complexity += _db.MetaStreams[i].Resource.PackSize;
    const CImage &image = _db.Images[i];
    unsigned endItem = image.StartItem + image.NumItems;
    for (unsigned k = image.StartItem; k < endItem; k++)
    {
      const CItem &item = _db.Items[k];
      if (item.StreamIndex >= 0)
        streamsRefs[(unsigned)item.StreamIndex]++;
    }
  }


  // ---------- Update Streams Refs Counts in changed images

  for (i = 0; i < db.UpdateIndexes.Size(); i++)
  {
    const CUpdateItem &ui = db.UpdateItems[db.UpdateIndexes[i]];
    
    if (ui.InArcIndex >= 0)
    {
      if ((unsigned)ui.InArcIndex >= _db.SortedItems.Size())
        continue;
      const CItem &item = _db.Items[_db.SortedItems[ui.InArcIndex]];
      if (item.StreamIndex >= 0)
        streamsRefs[(unsigned)item.StreamIndex]++;
    }
    else
    {
      const CMetaItem &mi = db.MetaItems[ui.MetaIndex];
      UInt64 size;
      if (ui.AltStreamIndex < 0)
        size = mi.Size;
      else
        size = mi.AltStreams[ui.AltStreamIndex].Size;
      complexity += size;
    }
  }

  // Clear ref counts for SolidBig streams
  
  for (i = 0; i < _db.DataStreams.Size(); i++)
    if (_db.DataStreams[i].Resource.IsSolidBig())
      streamsRefs[i] = 0;

  // Set ref counts for SolidBig streams
  
  for (i = 0; i < _db.DataStreams.Size(); i++)
    if (streamsRefs[i] != 0)
    {
      const CResource &rs = _db.DataStreams[i].Resource;
      if (rs.IsSolidSmall())
        streamsRefs[_db.Solids[rs.SolidIndex].StreamIndex] = 1;
    }

  for (i = 0; i < _db.DataStreams.Size(); i++)
    if (streamsRefs[i] != 0)
    {
      const CResource &rs = _db.DataStreams[i].Resource;
      if (!rs.IsSolidSmall())
        complexity += rs.PackSize;
    }
      
  RINOK(callback->SetTotal(complexity));
  UInt64 totalComplexity = complexity;

  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder;
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(callback, true);

  complexity = 0;

  // bool useResourceCompression = false;
  // use useResourceCompression only if CHeader::Flags compression is also set

  CHeader header;
  header.SetDefaultFields(false);

  if (isUpdate)
  {
    const CHeader &srcHeader = _volumes[1].Header;
    header.Flags = srcHeader.Flags;
    header.Version = srcHeader.Version;
    header.ChunkSize = srcHeader.ChunkSize;
    header.ChunkSizeBits = srcHeader.ChunkSizeBits;
  }

  {
    Byte buf[kHeaderSizeMax];
    header.WriteTo(buf);
    RINOK(WriteStream(outStream, buf, kHeaderSizeMax));
  }

  UInt64 curPos = kHeaderSizeMax;

  CInStreamWithSha1 *inShaStreamSpec = new CInStreamWithSha1;
  CMyComPtr<ISequentialInStream> inShaStream = inShaStreamSpec;

  CLimitedSequentialInStream *inStreamLimitedSpec = NULL;
  CMyComPtr<CLimitedSequentialInStream> inStreamLimited;
  if (_volumes.Size() == 2)
  {
    inStreamLimitedSpec = new CLimitedSequentialInStream;
    inStreamLimited = inStreamLimitedSpec;
    inStreamLimitedSpec->SetStream(_volumes[1].Stream);
  }

  
  CRecordVector<CStreamInfo> streams;
  CUIntVector sortedHashes; // indexes to streams, sorted by SHA1
  
  // ---------- Copy unchanged data streams ----------

  UInt64 solidRunOffset = 0;
  UInt64 curSolidSize = 0;

  for (i = 0; i < _db.DataStreams.Size(); i++)
  {
    const CStreamInfo &siOld = _db.DataStreams[i];
    const CResource &rs = siOld.Resource;
    
    unsigned numRefs = streamsRefs[i];

    if (numRefs == 0)
    {
      if (!rs.IsSolidSmall())
        continue;
      if (streamsRefs[_db.Solids[rs.SolidIndex].StreamIndex] == 0)
        continue;
    }

    lps->InSize = lps->OutSize = complexity;
    RINOK(lps->SetCur());

    int streamIndex = streams.Size();
    CStreamInfo s;
    s.Resource = rs;
    s.PartNumber = 1;
    s.RefCount = numRefs;

    memcpy(s.Hash, siOld.Hash, kHashSize);

    if (rs.IsSolid())
    {
      CSolid &ss = _db.Solids[rs.SolidIndex];
      if (rs.IsSolidSmall())
      {
        UInt64 oldOffset = ss.SolidOffset;
        if (rs.Offset < oldOffset)
          return E_FAIL;
        UInt64 relatOffset = rs.Offset - oldOffset;
        s.Resource.Offset = solidRunOffset + relatOffset;
      }
      else
      {
        // IsSolidBig
        solidRunOffset += curSolidSize;
        curSolidSize = ss.UnpackSize;
      }
    }
    else
    {
      solidRunOffset = 0;
      curSolidSize = 0;
    }
     
    if (!rs.IsSolid() || rs.IsSolidSmall())
    {
      int find = AddUniqHash(&streams.Front(), sortedHashes, siOld.Hash, streamIndex);
      if (find >= 0)
        return E_FAIL; // two streams with same SHA-1
    }
   
    if (!rs.IsSolid() || rs.IsSolidBig())
    {
      RINOK(_volumes[siOld.PartNumber].Stream->Seek(rs.Offset, STREAM_SEEK_SET, NULL));
      inStreamLimitedSpec->Init(rs.PackSize);
      RINOK(copyCoder->Code(inStreamLimited, outStream, NULL, NULL, progress));
      if (copyCoderSpec->TotalSize != rs.PackSize)
        return E_FAIL;
      s.Resource.Offset = curPos;
      curPos += rs.PackSize;
      lps->ProgressOffset += rs.PackSize;
    }

    streams.Add(s);
  }

  
  // ---------- Write new items ----------

  CUIntVector hlIndexes; // sorted indexes for hard link items

  for (i = 0; i < db.UpdateIndexes.Size(); i++)
  {
    lps->InSize = lps->OutSize = complexity;
    RINOK(lps->SetCur());
    const CUpdateItem &ui = db.UpdateItems[db.UpdateIndexes[i]];
    CMetaItem &mi = db.MetaItems[ui.MetaIndex];
    UInt64 size = 0;
    
    if (ui.AltStreamIndex >= 0)
    {
      if (mi.Skip)
        continue;
      size = mi.AltStreams[ui.AltStreamIndex].Size;
    }
    else
    {
      size = mi.Size;
      if (mi.IsDir)
      {
        // we support LINK files here
        if (mi.Reparse.Size() == 0)
          continue;
      }
    }

    if (ui.InArcIndex >= 0)
    {
      // data streams with OLD Data were written already
      // we just need to find HashIndex in hashes.

      if ((unsigned)ui.InArcIndex >= _db.SortedItems.Size())
        return E_FAIL;
      
      const CItem &item = _db.Items[_db.SortedItems[ui.InArcIndex]];
      
      if (item.StreamIndex < 0)
      {
        if (size == 0)
          continue;
        // if (_db.ItemHasStream(item))
        return E_FAIL;
      }

      // We support empty file (size = 0, but with stream and SHA-1) from old archive
      
      const CStreamInfo &siOld = _db.DataStreams[item.StreamIndex];

      int index = AddUniqHash(&streams.Front(), sortedHashes, siOld.Hash, -1);
      // we must have written that stream already
      if (index < 0)
        return E_FAIL;

      if (ui.AltStreamIndex < 0)
        mi.HashIndex = index;
      else
        mi.AltStreams[ui.AltStreamIndex].HashIndex = index;
      
      continue;
    }

    CMyComPtr<ISequentialInStream> fileInStream;
    HRESULT res = callback->GetStream(ui.CallbackIndex, &fileInStream);
    
    if (res == S_FALSE)
    {
      if (ui.AltStreamIndex >= 0)
      {
        mi.NumSkipAltStreams++;
        mi.AltStreams[ui.AltStreamIndex].Skip = true;
      }
      else
        mi.Skip = true;
    }
    else
    {
      RINOK(res);

      int miIndex = -1;
      
      if (!fileInStream)
      {
        if (!mi.IsDir)
          return E_INVALIDARG;
      }
      else if (ui.AltStreamIndex < 0)
      {
        CMyComPtr<IStreamGetProps2> getProps2;
        fileInStream->QueryInterface(IID_IStreamGetProps2, (void **)&getProps2);
        if (getProps2)
        {
          CStreamFileProps props;
          if (getProps2->GetProps2(&props) == S_OK)
          {
            mi.Attrib = props.Attrib;
            mi.CTime = props.CTime;
            mi.ATime = props.ATime;
            mi.MTime = props.MTime;
            mi.FileID = props.FileID_Low;
            if (props.NumLinks <= 1)
              mi.FileID = 0;
            mi.VolID = props.VolID;
            if (mi.FileID != 0)
              miIndex = AddToHardLinkList(db.MetaItems, ui.MetaIndex, hlIndexes);

            if (props.Size != size && props.Size != (UInt64)(Int64)-1)
            {
              Int64 delta = (Int64)props.Size - (Int64)size;
              Int64 newComplexity = totalComplexity + delta;
              if (newComplexity > 0)
              {
                totalComplexity = newComplexity;
                callback->SetTotal(totalComplexity);
              }
              mi.Size = props.Size;
              size = props.Size;
            }
          }
        }
      }
      
      if (miIndex >= 0)
      {
        mi.HashIndex = db.MetaItems[miIndex].HashIndex;
        if (mi.HashIndex >= 0)
          streams[mi.HashIndex].RefCount++;
        // fix for future: maybe we need to check also that real size is equal to size from IStreamGetProps2
      }
      else if (ui.AltStreamIndex < 0 && mi.Reparse.Size() != 0)
      {
        if (mi.Reparse.Size() < 8)
          return E_FAIL;
        NCrypto::NSha1::CContext sha1;
        sha1.Init();
        size_t packSize = mi.Reparse.Size() - 8;
        sha1.Update((const Byte *)mi.Reparse + 8, packSize);
        Byte hash[kHashSize];
        sha1.Final(hash);
        
        int index = AddUniqHash(&streams.Front(), sortedHashes, hash, streams.Size());

        if (index >= 0)
          streams[index].RefCount++;
        else
        {
          index = streams.Size();
          RINOK(WriteStream(outStream, (const Byte *)mi.Reparse + 8, packSize));
          CStreamInfo s;
          s.Resource.PackSize = packSize;
          s.Resource.Offset = curPos;
          s.Resource.UnpackSize = packSize;
          s.Resource.Flags = 0; // check it
          /*
            if (useResourceCompression)
              s.Resource.Flags = NResourceFlags::Compressed;
          */
          s.PartNumber = 1;
          s.RefCount = 1;
          memcpy(s.Hash, hash, kHashSize);
          curPos += packSize;

          streams.Add(s);
        }
        
        mi.HashIndex = index;
      }
      else
      {
        inShaStreamSpec->SetStream(fileInStream);
        fileInStream.Release();
        inShaStreamSpec->Init();
        UInt64 offsetBlockSize = 0;
        /*
        if (useResourceCompression)
        {
          for (UInt64 t = kChunkSize; t < size; t += kChunkSize)
          {
            Byte buf[8];
            SetUi32(buf, (UInt32)t);
            RINOK(WriteStream(outStream, buf, 4));
            offsetBlockSize += 4;
          }
        }
        */
        
        RINOK(copyCoder->Code(inShaStream, outStream, NULL, NULL, progress));
        size = copyCoderSpec->TotalSize;
       
        if (size != 0)
        {
          Byte hash[kHashSize];
          UInt64 packSize = offsetBlockSize + size;
          inShaStreamSpec->Final(hash);

          int index = AddUniqHash(&streams.Front(), sortedHashes, hash, streams.Size());

          if (index >= 0)
          {
            streams[index].RefCount++;
            outStream->Seek(-(Int64)packSize, STREAM_SEEK_CUR, &curPos);
            outStream->SetSize(curPos);
          }
          else
          {
            index = streams.Size();
            CStreamInfo s;
            s.Resource.PackSize = packSize;
            s.Resource.Offset = curPos;
            s.Resource.UnpackSize = size;
            s.Resource.Flags = 0;
            /*
            if (useResourceCompression)
            s.Resource.Flags = NResourceFlags::Compressed;
            */
            s.PartNumber = 1;
            s.RefCount = 1;
            memcpy(s.Hash, hash, kHashSize);
            curPos += packSize;

            streams.Add(s);
          }
          
          if (ui.AltStreamIndex < 0)
            mi.HashIndex = index;
          else
            mi.AltStreams[ui.AltStreamIndex].HashIndex = index;
        }
      }
    }
    fileInStream.Release();
    complexity += size;
    RINOK(callback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK));
  }

  while (secureBlocks.Size() < numNewImages)
    secureBlocks.AddNew();

  
  
  // ---------- Write Images ----------

  for (i = 0; i < numNewImages; i++)
  {
    lps->InSize = lps->OutSize = complexity;
    RINOK(lps->SetCur());
    if (i < isChangedImage.Size() && !isChangedImage[i])
    {
      CStreamInfo s = _db.MetaStreams[i];
      
      RINOK(_volumes[1].Stream->Seek(s.Resource.Offset, STREAM_SEEK_SET, NULL));
      inStreamLimitedSpec->Init(s.Resource.PackSize);
      RINOK(copyCoder->Code(inStreamLimited, outStream, NULL, NULL, progress));
      if (copyCoderSpec->TotalSize != s.Resource.PackSize)
        return E_FAIL;

      s.Resource.Offset = curPos;
      s.PartNumber = 1;
      s.RefCount = 1;
      streams.Add(s);

      if (_bootIndex != 0 && _bootIndex == (UInt32)i + 1)
      {
        header.MetadataResource = s.Resource;
        header.BootIndex = _bootIndex;
      }

      lps->ProgressOffset += s.Resource.PackSize;
      curPos += s.Resource.PackSize;
      // printf("\nWrite old image %x\n", i + 1);
      continue;
    }

    const CDir &tree = trees[i];
    const UInt32 kSecuritySize = 8;
    
    size_t pos = kSecuritySize;

    const CUniqBlocks &secUniqBlocks = secureBlocks[i];
    const CObjectVector<CByteBuffer> &secBufs = secUniqBlocks.Bufs;
    pos += (size_t)secUniqBlocks.GetTotalSizeInBytes();
    pos += secBufs.Size() * 8;
    pos = (pos + 7) & ~(size_t)7;
    
    db.DefaultDirItem = ri;
    pos += db.WriteTree_Dummy(tree);
    
    CByteArr meta(pos);
    
    Set32((Byte *)meta + 4, secBufs.Size()); // num security entries
    pos = kSecuritySize;
    
    if (secBufs.Size() == 0)
    {
      // we can write 0 here only if there is no security data, imageX does it,
      // but some programs expect size = 8
      Set32((Byte *)meta, 8); // size of security data
      // Set32((Byte *)meta, 0);
    }
    else
    {
      unsigned k;
      for (k = 0; k < secBufs.Size(); k++, pos += 8)
      {
        Set64(meta + pos, secBufs[k].Size());
      }
      for (k = 0; k < secBufs.Size(); k++)
      {
        const CByteBuffer &buf = secBufs[k];
        size_t size = buf.Size();
        if (size != 0)
        {
          memcpy(meta + pos, buf, size);
          pos += size;
        }
      }
      while ((pos & 7) != 0)
        meta[pos++] = 0;
      Set32((Byte *)meta, (UInt32)pos); // size of security data
    }
    
    db.Hashes = &streams.Front();
    db.WriteTree(tree, (Byte *)meta, pos);

    {
      NCrypto::NSha1::CContext sha;
      sha.Init();
      sha.Update((const Byte *)meta, pos);

      Byte digest[kHashSize];
      sha.Final(digest);
      
      CStreamInfo s;
      s.Resource.PackSize = pos;
      s.Resource.Offset = curPos;
      s.Resource.UnpackSize = pos;
      s.Resource.Flags = NResourceFlags::kMetadata;
      s.PartNumber = 1;
      s.RefCount = 1;
      memcpy(s.Hash, digest, kHashSize);
      streams.Add(s);

      if (_bootIndex != 0 && _bootIndex == (UInt32)i + 1)
      {
        header.MetadataResource = s.Resource;
        header.BootIndex = _bootIndex;
      }

      RINOK(WriteStream(outStream, (const Byte *)meta, pos));
      meta.Free();
      curPos += pos;
    }
  }

  lps->InSize = lps->OutSize = complexity;
  RINOK(lps->SetCur());

  header.OffsetResource.UnpackSize = header.OffsetResource.PackSize = (UInt64)streams.Size() * kStreamInfoSize;
  header.OffsetResource.Offset = curPos;
  header.OffsetResource.Flags = NResourceFlags::kMetadata;

  
  
  // ---------- Write Streams Info Tables ----------

  for (i = 0; i < streams.Size(); i++)
  {
    Byte buf[kStreamInfoSize];
    streams[i].WriteTo(buf);
    RINOK(WriteStream(outStream, buf, kStreamInfoSize));
    curPos += kStreamInfoSize;
  }

  AString xml ("<WIM>");
  AddTagUInt64_ToString(xml, "TOTALBYTES", curPos);
  for (i = 0; i < trees.Size(); i++)
  {
    CDir &tree = trees[i];

    CXmlItem item;
    if (_xmls.Size() == 1)
    {
      const CWimXml &_oldXml = _xmls[0];
      if (i < _oldXml.Images.Size())
      {
        // int ttt = _oldXml.Images[i].ItemIndexInXml;
        item = _oldXml.Xml.Root.SubItems[_oldXml.Images[i].ItemIndexInXml];
      }
    }
    if (i >= isChangedImage.Size() || isChangedImage[i])
    {
      char temp[16];
      if (item.Name.IsEmpty())
      {
        ConvertUInt32ToString(i + 1, temp);
        item.Name = "IMAGE";
        item.IsTag = true;
        CXmlProp &prop = item.Props.AddNew();
        prop.Name = "INDEX";
        prop.Value = temp;
      }
      
      AddTag_String_IfEmpty(item, "NAME", temp);
      AddTag_UInt64(item, "DIRCOUNT", tree.GetNumDirs() - 1);
      AddTag_UInt64(item, "FILECOUNT", tree.GetNumFiles());
      AddTag_UInt64(item, "TOTALBYTES", tree.GetTotalSize(db.MetaItems));
      
      AddTag_Time(item, "CREATIONTIME", ftCur);
      AddTag_Time(item, "LASTMODIFICATIONTIME", ftCur);
    }

    item.AppendTo(xml);
  }
  xml += "</WIM>";

  size_t xmlSize;
  {
    UString utf16;
    if (!ConvertUTF8ToUnicode(xml, utf16))
      return S_FALSE;
    xmlSize = (utf16.Len() + 1) * 2;

    CByteArr xmlBuf(xmlSize);
    Set16((Byte *)xmlBuf, 0xFEFF);
    for (i = 0; i < (unsigned)utf16.Len(); i++)
      Set16((Byte *)xmlBuf + 2 + i * 2, (UInt16)utf16[i]);
    RINOK(WriteStream(outStream, (const Byte *)xmlBuf, xmlSize));
  }
  
  header.XmlResource.UnpackSize = header.XmlResource.PackSize = xmlSize;
  header.XmlResource.Offset = curPos;
  header.XmlResource.Flags = NResourceFlags::kMetadata;

  outStream->Seek(0, STREAM_SEEK_SET, NULL);
  header.NumImages = trees.Size();
  {
    Byte buf[kHeaderSizeMax];
    header.WriteTo(buf);
    return WriteStream(outStream, buf, kHeaderSizeMax);
  }

  COM_TRY_END
}

}}

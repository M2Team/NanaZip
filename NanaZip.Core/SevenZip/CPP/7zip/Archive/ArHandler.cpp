// ArHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/StringConvert.h"
#include "../../Common/StringToInt.h"

#include "../../Windows/PropVariant.h"
#include "../../Windows/TimeUtils.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"

#include "Common/ItemNameUtils.h"

using namespace NWindows;
using namespace NTime;

namespace NArchive {
namespace NAr {

/*
The end of each file member (including last file in archive) is 2-bytes aligned.
It uses 0xA padding if required.

File Names:

GNU/SVR4 variant (.a static library):
  /       - archive symbol table
  //      - the list of the long filenames, separated by one or more LF characters.
  /N      - the reference to name string in long filenames list
  name/   - the name

Microsoft variant (.lib static library):
  /       - First linker file (archive symbol table)
  /       - Second linker file
  //      - the list of the long filenames, null-terminated. Each string begins
            immediately after the null byte in the previous string.
  /N      - the reference to name string in long filenames list
  name/   - the name

BSD (Mac OS X) variant:
  "__.SYMDEF"         -  archive symbol table
    or
  "__.SYMDEF SORTED"  -  archive symbol table
  #1/N    - the real filename of length N is appended to the file header.
*/

static const unsigned kSignatureLen = 8;
static const Byte kSignature[kSignatureLen] =
  { '!', '<', 'a', 'r', 'c', 'h', '>', 0x0A };

static const unsigned kNameSize = 16;
static const unsigned kTimeSize = 12;
static const unsigned kUserSize = 6;
static const unsigned kModeSize = 8;
static const unsigned kSizeSize = 10;

static const unsigned kHeaderSize = kNameSize + kTimeSize + kUserSize * 2 + kModeSize + kSizeSize + 1 + 1;

enum EType
{
  kType_Ar,
  kType_ALib,
  kType_Deb,
  kType_Lib
};

static const char * const k_TypeExtionsions[] =
{
    "ar"
  , "a"
  , "deb"
  , "lib"
};

enum ESubType
{
  kSubType_None,
  kSubType_BSD
};

/*
struct CHeader
{
  char Name[kNameSize];
  char MTime[kTimeSize];
  char User[kUserSize];
  char Group[kUserSize];
  char Mode[kModeSize];
  char Size[kSizeSize];
  char Quote;
  char NewLine;
};
*/

struct CItem
{
  AString Name;
  UInt64 Size;
  UInt32 MTime;
  UInt32 User;
  UInt32 Group;
  UInt32 Mode;
  
  UInt64 HeaderPos;
  UInt64 HeaderSize;

  int TextFileIndex;
  int SameNameIndex;

  CItem(): TextFileIndex(-1), SameNameIndex(-1) {}
  UInt64 GetDataPos() const { return HeaderPos + HeaderSize; }
};

class CInArchive
{
  CMyComPtr<IInStream> m_Stream;
  
public:
  UInt64 Position;
  ESubType SubType;
  
  HRESULT GetNextItem(CItem &itemInfo, bool &filled);
  HRESULT Open(IInStream *inStream);
  HRESULT SkipData(UInt64 dataSize)
  {
    return m_Stream->Seek((Int64)(dataSize + (dataSize & 1)), STREAM_SEEK_CUR, &Position);
  }
};

HRESULT CInArchive::Open(IInStream *inStream)
{
  SubType = kSubType_None;
  RINOK(InStream_GetPos(inStream, Position))
  char signature[kSignatureLen];
  RINOK(ReadStream_FALSE(inStream, signature, kSignatureLen))
  Position += kSignatureLen;
  if (memcmp(signature, kSignature, kSignatureLen) != 0)
    return S_FALSE;
  m_Stream = inStream;
  return S_OK;
}

static unsigned RemoveTailSpaces(char *dest, const char *s, unsigned size)
{
  memcpy(dest, s, size);
  for (; size != 0; size--)
  {
    if (dest[size - 1] != ' ')
      break;
  }
  dest[size] = 0;
  return size;
}

static bool OctalToNumber32(const char *s, unsigned size, UInt32 &res)
{
  res = 0;
  char sz[32];
  size = RemoveTailSpaces(sz, s, size);
  if (size == 0 || strcmp(sz, "-1") == 0)
    return true; // some items don't contain any numbers
  const char *end;
  UInt64 res64 = ConvertOctStringToUInt64(sz, &end);
  if ((unsigned)(end - sz) != size)
    return false;
  res = (UInt32)res64;
  return (res64 <= 0xFFFFFFFF);
}

static bool DecimalToNumber(const char *s, unsigned size, UInt64 &res)
{
  res = 0;
  char sz[32];
  size = RemoveTailSpaces(sz, s, size);
  if (size == 0 || strcmp(sz, "-1") == 0)
    return true; // some items don't contain any numbers
  const char *end;
  res = ConvertStringToUInt64(sz, &end);
  return ((unsigned)(end - sz) == size);
}

static bool DecimalToNumber32(const char *s, unsigned size, UInt32 &res)
{
  UInt64 res64;
  if (!DecimalToNumber(s, size, res64))
    return false;
  res = (UInt32)res64;
  return (res64 <= 0xFFFFFFFF);
}

#define RIF(x) { if (!(x)) return S_FALSE; }


HRESULT CInArchive::GetNextItem(CItem &item, bool &filled)
{
  filled = false;

  char header[kHeaderSize];
  const char *cur = header;

  {
    size_t processedSize = sizeof(header);
    item.HeaderPos = Position;
    item.HeaderSize = kHeaderSize;
    RINOK(ReadStream(m_Stream, header, &processedSize))
    if (processedSize != sizeof(header))
      return S_OK;
    if (header[kHeaderSize - 2] != 0x60 ||
      header[kHeaderSize - 1] != 0x0A)
      return S_OK;
    for (unsigned i = 0; i < kHeaderSize - 2; i++)
      // if (header[i] < 0x20)
      if (header[i] == 0)
        return S_OK;
    Position += processedSize;
  }

  UInt32 longNameLen = 0;
  if (cur[0] == '#' &&
      cur[1] == '1' &&
      cur[2] == '/' &&
      cur[3] != 0)
  {
    // BSD variant
    RIF(DecimalToNumber32(cur + 3, kNameSize - 3 , longNameLen))
    if (longNameLen >= (1 << 12))
      longNameLen = 0;
  }
  else
  {
    char tempString[kNameSize + 1];
    RemoveTailSpaces(tempString, cur, kNameSize);
    item.Name = tempString;
  }
  cur += kNameSize;

  RIF(DecimalToNumber32(cur, kTimeSize, item.MTime)) cur += kTimeSize;
  RIF(DecimalToNumber32(cur, kUserSize, item.User)) cur += kUserSize;
  RIF(DecimalToNumber32(cur, kUserSize, item.Group)) cur += kUserSize;
  RIF(OctalToNumber32(cur, kModeSize, item.Mode)) cur += kModeSize;
  RIF(DecimalToNumber(cur, kSizeSize, item.Size)) cur += kSizeSize;

  if (longNameLen != 0 && longNameLen <= item.Size)
  {
    SubType = kSubType_BSD;
    size_t processedSize = longNameLen;
    char *s = item.Name.GetBuf(longNameLen);
    HRESULT res = ReadStream(m_Stream, s, &processedSize);
    item.Name.ReleaseBuf_CalcLen(longNameLen);
    RINOK(res)
    if (processedSize != longNameLen)
      return S_OK;
    item.Size -= longNameLen;
    item.HeaderSize += longNameLen;
    Position += processedSize;
  }

  filled = true;
  return S_OK;
}


Z7_CLASS_IMP_CHandler_IInArchive_1(
  IInArchiveGetStream
)
  CObjectVector<CItem> _items;
  CMyComPtr<IInStream> _stream;
  Int32 _mainSubfile;
  UInt64 _phySize;

  EType _type;
  ESubType _subType;
  int _longNames_FileIndex;
  AString _libFiles[2];
  unsigned _numLibFiles;
  AString _errorMessage;
  bool _isArc;

  void UpdateErrorMessage(const char *s);
  
  HRESULT ParseLongNames(IInStream *stream);
  void ChangeDuplicateNames();
  int FindItem(UInt32 offset) const;
  HRESULT AddFunc(UInt32 offset, const Byte *data, size_t size, size_t &pos);
  HRESULT ParseLibSymbols(IInStream *stream, unsigned fileIndex);
};

void CHandler::UpdateErrorMessage(const char *s)
{
  if (!_errorMessage.IsEmpty())
    _errorMessage += '\n';
  _errorMessage += s;
}

static const Byte kArcProps[] =
{
  kpidSubType
};

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidMTime,
  kpidPosixAttrib,
  kpidUserId,
  kpidGroupId
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

HRESULT CHandler::ParseLongNames(IInStream *stream)
{
  unsigned i;
  for (i = 0; i < _items.Size(); i++)
    if (_items[i].Name == "//")
      break;
  if (i == _items.Size())
    return S_OK;

  unsigned fileIndex = i;
  const CItem &item = _items[fileIndex];
  if (item.Size > ((UInt32)1 << 30))
    return S_FALSE;
  RINOK(InStream_SeekSet(stream, item.GetDataPos()))
  const size_t size = (size_t)item.Size;

  CByteArr p(size);
  RINOK(ReadStream_FALSE(stream, p, size))
  
  for (i = 0; i < _items.Size(); i++)
  {
    CItem &item2 = _items[i];
    if (item2.Name[0] != '/')
      continue;
    const char *ptr = item2.Name.Ptr(1);
    const char *end;
    UInt32 pos = ConvertStringToUInt32(ptr, &end);
    if (*end != 0 || end == ptr)
      continue;
    if (pos >= size)
      continue;
    UInt32 start = pos;
    for (;;)
    {
      if (pos >= size)
        return S_FALSE;
      const Byte c = p[pos];
      if (c == 0 || c == 0x0A)
        break;
      pos++;
    }
    item2.Name.SetFrom((const char *)(p + start), (unsigned)(pos - start));
  }
  
  _longNames_FileIndex = (int)fileIndex;
  return S_OK;
}

void CHandler::ChangeDuplicateNames()
{
  unsigned i;
  for (i = 1; i < _items.Size(); i++)
  {
    CItem &item = _items[i];
    if (item.Name[0] == '/')
      continue;
    CItem &prev = _items[i - 1];
    if (item.Name == prev.Name)
    {
      if (prev.SameNameIndex < 0)
        prev.SameNameIndex = 0;
      item.SameNameIndex = prev.SameNameIndex + 1;
    }
  }
  for (i = 0; i < _items.Size(); i++)
  {
    CItem &item = _items[i];
    if (item.SameNameIndex < 0)
      continue;
    char sz[32];
    ConvertUInt32ToString((unsigned)item.SameNameIndex + 1, sz);
    unsigned len = MyStringLen(sz);
    sz[len++] = '.';
    sz[len] = 0;
    item.Name.Insert(0, sz);
  }
}

int CHandler::FindItem(UInt32 offset) const
{
  unsigned left = 0, right = _items.Size();
  while (left != right)
  {
    const unsigned mid = (left + right) / 2;
    const UInt64 midVal = _items[mid].HeaderPos;
    if (offset == midVal)
      return (int)mid;
    if (offset < midVal)
      right = mid;
    else
      left = mid + 1;
  }
  return -1;
}

HRESULT CHandler::AddFunc(UInt32 offset, const Byte *data, size_t size, size_t &pos)
{
  const int fileIndex = FindItem(offset);
  if (fileIndex < (int)0)
    return S_FALSE;

  size_t i = pos;
  do
  {
    if (i >= size)
      return S_FALSE;
  }
  while (data[i++] != 0);
  
  AString &s = _libFiles[_numLibFiles];
  const AString &name = _items[(unsigned)fileIndex].Name;
  s += name;
  if (!name.IsEmpty() && name.Back() == '/')
    s.DeleteBack();
  s += "    ";
  s += (const char *)(data + pos);
  s += (char)0xD;
  s += (char)0xA;
  pos = i;
  return S_OK;
}

static UInt32 Get32(const Byte *p, unsigned be) { if (be) return GetBe32(p); return GetUi32(p); }

HRESULT CHandler::ParseLibSymbols(IInStream *stream, unsigned fileIndex)
{
  CItem &item = _items[fileIndex];
  if (item.Name != "/" &&
      item.Name != "__.SYMDEF"  &&
      item.Name != "__.SYMDEF SORTED")
    return S_OK;
  if (item.Size > ((UInt32)1 << 30) ||
      item.Size < 4)
    return S_OK;
  RINOK(InStream_SeekSet(stream, item.GetDataPos()))
  size_t size = (size_t)item.Size;
  CByteArr p(size);
  RINOK(ReadStream_FALSE(stream, p, size))
 
  size_t pos = 0;

  if (item.Name != "/")
  {
    // "__.SYMDEF" parsing (BSD)
    unsigned be;
    for (be = 0; be < 2; be++)
    {
      const UInt32 tableSize = Get32(p, be);
      pos = 4;
      if (size - pos < tableSize || (tableSize & 7) != 0)
        continue;
      size_t namesStart = pos + tableSize;
      const UInt32 namesSize = Get32(p + namesStart, be);
      namesStart += 4;
      if (namesStart > size || namesStart + namesSize != size)
        continue;
      
      const UInt32 numSymbols = tableSize >> 3;
      UInt32 i;
      for (i = 0; i < numSymbols; i++, pos += 8)
      {
        size_t namePos = Get32(p + pos, be);
        const UInt32 offset = Get32(p + pos + 4, be);
        if (AddFunc(offset, p + namesStart, namesSize, namePos) != S_OK)
          break;
      }
      if (i == numSymbols)
      {
        pos = size;
        _type = kType_ALib;
        _subType = kSubType_BSD;
        break;
      }
    }
    if (be == 2)
      return S_FALSE;
  }
  else if (_numLibFiles == 0)
  {
    // archive symbol table (GNU)
    const UInt32 numSymbols = GetBe32(p);
    pos = 4;
    if (numSymbols > (size - pos) / 4)
      return S_FALSE;
    pos += 4 * numSymbols;
    
    for (UInt32 i = 0; i < numSymbols; i++)
    {
      const UInt32 offset = GetBe32(p + 4 + i * 4);
      RINOK(AddFunc(offset, p, size, pos))
    }
    _type = kType_ALib;
  }
  else
  {
    // Second linker file (Microsoft .lib)
    const UInt32 numMembers = GetUi32(p);
    pos = 4;
    if (numMembers > (size - pos) / 4)
      return S_FALSE;
    pos += 4 * numMembers;
    
    if (size - pos < 4)
      return S_FALSE;
    const UInt32 numSymbols = GetUi32(p + pos);
    pos += 4;
    if (numSymbols > (size - pos) / 2)
      return S_FALSE;
    size_t indexStart = pos;
    pos += 2 * numSymbols;
    
    for (UInt32 i = 0; i < numSymbols; i++)
    {
      // index is 1-based. So 32-bit numSymbols field works as item[0]
      const UInt32 index = GetUi16(p + indexStart + i * 2);
      if (index == 0 || index > numMembers)
        return S_FALSE;
      const UInt32 offset = GetUi32(p + index * 4);
      RINOK(AddFunc(offset, p, size, pos))
    }
    _type = kType_Lib;
  }
  // size can be 2-byte aligned in linux files
  if (pos != size && pos + (pos & 1) != size)
    return S_FALSE;
  item.TextFileIndex = (int)(_numLibFiles++);
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Open(IInStream *stream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback *callback))
{
  COM_TRY_BEGIN
  {
    Close();

    UInt64 fileSize;
    RINOK(InStream_AtBegin_GetSize(stream, fileSize))

    CInArchive arc;
    RINOK(arc.Open(stream))

    if (callback)
    {
      RINOK(callback->SetTotal(NULL, &fileSize))
      const UInt64 numFiles = _items.Size();
      RINOK(callback->SetCompleted(&numFiles, &arc.Position))
    }

    CItem item;
    for (;;)
    {
      bool filled;
      RINOK(arc.GetNextItem(item, filled))
      if (!filled)
        break;
      _items.Add(item);
      arc.SkipData(item.Size);
      if (callback && (_items.Size() & 0xFF) == 0)
      {
        UInt64 numFiles = _items.Size();
        RINOK(callback->SetCompleted(&numFiles, &arc.Position))
      }
    }

    if (_items.IsEmpty())
    {
      // we don't need false empty archives (8-bytes signature only)
      if (arc.Position != fileSize)
        return S_FALSE;
    }

    _isArc = true;

    _subType = arc.SubType;
    
    if (ParseLongNames(stream) != S_OK)
      UpdateErrorMessage("Long file names parsing error");
    if (_longNames_FileIndex >= 0)
      _items.Delete((unsigned)_longNames_FileIndex);

    if (!_items.IsEmpty() && _items[0].Name == "debian-binary")
    {
      _type = kType_Deb;
      _items.DeleteFrontal(1);
      for (unsigned i = 0; i < _items.Size(); i++)
        if (_items[i].Name.IsPrefixedBy("data.tar."))
        {
          if (_mainSubfile < 0)
            _mainSubfile = (int)i;
          else
          {
            _mainSubfile = -1;
            break;
          }
        }
    }
    else
    {
      ChangeDuplicateNames();
      bool error = false;
      for (unsigned li = 0; li < 2 && li < _items.Size(); li++)
        if (ParseLibSymbols(stream, li) != S_OK)
          error = true;
      if (error)
        UpdateErrorMessage("Library symbols information error");
    }

    _stream = stream;
    _phySize = arc.Position;

    /*
    if (fileSize < _phySize)
      UpdateErrorMessage("Unexpected end of archive");
    */
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _isArc = false;
  _phySize = 0;

  _errorMessage.Empty();
  _stream.Release();
  _items.Clear();

  _type = kType_Ar;
  _subType = kSubType_None;
  _mainSubfile = -1;
  _longNames_FileIndex = -1;

  _numLibFiles = 0;
  _libFiles[0].Empty();
  _libFiles[1].Empty();

  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _items.Size();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPhySize: prop = _phySize; break;
    case kpidMainSubfile: if (_mainSubfile >= 0) prop = (UInt32)_mainSubfile; break;
    case kpidExtension: prop = k_TypeExtionsions[(unsigned)_type]; break;
    case kpidShortComment:
    case kpidSubType:
    {
      AString s (k_TypeExtionsions[(unsigned)_type]);
      if (_subType == kSubType_BSD)
        s += ":BSD";
      prop = s;
      break;
    }
    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc) v |= kpv_ErrorFlags_IsNotArc;
      prop = v;
      break;
    }
    case kpidWarning: if (!_errorMessage.IsEmpty()) prop = _errorMessage; break;
    case kpidIsNotArcType: if (_type != kType_Deb) prop = true; break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  const CItem &item = _items[index];
  switch (propID)
  {
    case kpidPath:
      if (item.TextFileIndex >= 0)
        prop = (item.TextFileIndex == 0) ? "1.txt" : "2.txt";
      else
        prop = (const wchar_t *)NItemName::GetOsPath_Remove_TailSlash(MultiByteToUnicodeString(item.Name, CP_OEMCP));
      break;
    case kpidSize:
    case kpidPackSize:
      if (item.TextFileIndex >= 0)
        prop = (UInt64)_libFiles[(unsigned)item.TextFileIndex].Len();
      else
        prop = item.Size;
      break;
    case kpidMTime:
    {
      if (item.MTime != 0)
        PropVariant_SetFrom_UnixTime(prop, item.MTime);
      break;
    }
    case kpidUserId: if (item.User != 0) prop = item.User; break;
    case kpidGroupId: if (item.Group != 0) prop = item.Group; break;
    case kpidPosixAttrib:
      if (item.TextFileIndex < 0)
        prop = item.Mode;
      break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _items.Size();
  if (numItems == 0)
    return S_OK;
  UInt64 totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    const CItem &item = _items[allFilesMode ? i : indices[i]];
    totalSize +=
      (item.TextFileIndex >= 0) ?
        (UInt64)_libFiles[(unsigned)item.TextFileIndex].Len() : item.Size;
  }
  extractCallback->SetTotal(totalSize);

  UInt64 currentTotalSize = 0;
  
  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder();
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  CLimitedSequentialInStream *streamSpec = new CLimitedSequentialInStream;
  CMyComPtr<ISequentialInStream> inStream(streamSpec);
  streamSpec->SetStream(_stream);

  for (i = 0; i < numItems; i++)
  {
    lps->InSize = lps->OutSize = currentTotalSize;
    RINOK(lps->SetCur())
    CMyComPtr<ISequentialOutStream> realOutStream;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    const UInt32 index = allFilesMode ? i : indices[i];
    const CItem &item = _items[index];
    RINOK(extractCallback->GetStream(index, &realOutStream, askMode))
    currentTotalSize += (item.TextFileIndex >= 0) ?
        (UInt64)_libFiles[(unsigned)item.TextFileIndex].Len() : item.Size;
    
    if (!testMode && !realOutStream)
      continue;
    RINOK(extractCallback->PrepareOperation(askMode))
    if (testMode)
    {
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
      continue;
    }
    bool isOk = true;
    if (item.TextFileIndex >= 0)
    {
      const AString &f = _libFiles[(unsigned)item.TextFileIndex];
      if (realOutStream)
        RINOK(WriteStream(realOutStream, f, f.Len()))
    }
    else
    {
      RINOK(InStream_SeekSet(_stream, item.GetDataPos()))
      streamSpec->Init(item.Size);
      RINOK(copyCoder->Code(inStream, realOutStream, NULL, NULL, progress))
      isOk = (copyCoderSpec->TotalSize == item.Size);
    }
    realOutStream.Release();
    RINOK(extractCallback->SetOperationResult(isOk ?
        NExtract::NOperationResult::kOK:
        NExtract::NOperationResult::kDataError))
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetStream(UInt32 index, ISequentialInStream **stream))
{
  COM_TRY_BEGIN
  const CItem &item = _items[index];
  if (item.TextFileIndex >= 0)
  {
    const AString &f = _libFiles[(unsigned)item.TextFileIndex];
    Create_BufInStream_WithNewBuffer((const void *)(const char *)f, f.Len(), stream);
    return S_OK;
  }
  else
    return CreateLimitedInStream(_stream, item.GetDataPos(), item.Size, stream);
  COM_TRY_END
}

REGISTER_ARC_I(
  "Ar", "ar a deb udeb lib", NULL, 0xEC,
  kSignature,
  0,
  0,
  NULL)

}}

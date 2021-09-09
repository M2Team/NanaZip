// Archive/ChmIn.cpp

#include "StdAfx.h"

// #include <stdio.h>

#include "../../../../C/CpuArch.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/UTFConvert.h"

#include "../../Common/LimitedStreams.h"

#include "ChmIn.h"

#define Get32(p) GetUi32(p)
#define Get64(p) GetUi64(p)

namespace NArchive {
namespace NChm {

static const UInt32 kSignature_ITSP = 0x50535449;
static const UInt32 kSignature_PMGL = 0x4C474D50;
static const UInt32 kSignature_LZXC = 0x43585A4C;

static const UInt32 kSignature_IFCM = 0x4D434649;
static const UInt32 kSignature_AOLL = 0x4C4C4F41;
static const UInt32 kSignature_CAOL = 0x4C4F4143;

static const UInt32 kSignature_ITSF = 0x46535449;
static const UInt32 kSignature_ITOL = 0x4C4F5449;
static const UInt32 kSignature_ITLS = 0x534C5449;

struct CEnexpectedEndException {};
struct CHeaderErrorException {};

// define CHM_LOW, if you want to see low level items
// #define CHM_LOW

static const Byte kChmLzxGuid[16]   = { 0x40, 0x89, 0xC2, 0x7F, 0x31, 0x9D, 0xD0, 0x11, 0x9B, 0x27, 0x00, 0xA0, 0xC9, 0x1E, 0x9C, 0x7C };
static const Byte kHelp2LzxGuid[16] = { 0xC6, 0x07, 0x90, 0x0A, 0x76, 0x40, 0xD3, 0x11, 0x87, 0x89, 0x00, 0x00, 0xF8, 0x10, 0x57, 0x54 };
static const Byte kDesGuid[16]      = { 0xA2, 0xE4, 0xF6, 0x67, 0xBF, 0x60, 0xD3, 0x11, 0x85, 0x40, 0x00, 0xC0, 0x4F, 0x58, 0xC3, 0xCF };

static bool inline AreGuidsEqual(const Byte *g1, const Byte *g2)
{
  return memcmp(g1, g2, 16) == 0;
}

static char GetHex(unsigned v)
{
  return (char)((v < 10) ? ('0' + v) : ('A' + (v - 10)));
}

static void PrintByte(Byte b, AString &s)
{
  s += GetHex(b >> 4);
  s += GetHex(b & 0xF);
}

AString CMethodInfo::GetGuidString() const
{
  char s[48];
  RawLeGuidToString_Braced(Guid, s);
  // MyStringUpper_Ascii(s);
  return (AString)s;
}

bool CMethodInfo::IsLzx() const
{
  if (AreGuidsEqual(Guid, kChmLzxGuid))
    return true;
  return AreGuidsEqual(Guid, kHelp2LzxGuid);
}

bool CMethodInfo::IsDes() const
{
  return AreGuidsEqual(Guid, kDesGuid);
}

AString CMethodInfo::GetName() const
{
  AString s;
  if (IsLzx())
  {
    s = "LZX:";
    s.Add_UInt32(LzxInfo.GetNumDictBits());
  }
  else
  {
    if (IsDes())
      s = "DES";
    else
    {
      s = GetGuidString();
      if (ControlData.Size() > 0)
      {
        s += ':';
        for (size_t i = 0; i < ControlData.Size(); i++)
          PrintByte(ControlData[i], s);
      }
    }
  }
  return s;
}

bool CSectionInfo::IsLzx() const
{
  if (Methods.Size() != 1)
    return false;
  return Methods[0].IsLzx();
}

UString CSectionInfo::GetMethodName() const
{
  UString s;
  if (!IsLzx())
  {
    UString temp;
    ConvertUTF8ToUnicode(Name, temp);
      s += temp;
    s += ": ";
  }
  FOR_VECTOR (i, Methods)
  {
    if (i != 0)
      s.Add_Space();
    s += Methods[i].GetName();
  }
  return s;
}

Byte CInArchive::ReadByte()
{
  Byte b;
  if (!_inBuffer.ReadByte(b))
    throw CEnexpectedEndException();
  return b;
}

void CInArchive::Skip(size_t size)
{
  if (_inBuffer.Skip(size) != size)
    throw CEnexpectedEndException();
}

void CInArchive::ReadBytes(Byte *data, UInt32 size)
{
  if (_inBuffer.ReadBytes(data, size) != size)
    throw CEnexpectedEndException();
}

UInt16 CInArchive::ReadUInt16()
{
  Byte b0, b1;
  if (!_inBuffer.ReadByte(b0)) throw CEnexpectedEndException();
  if (!_inBuffer.ReadByte(b1)) throw CEnexpectedEndException();
  return (UInt16)(((UInt16)b1 << 8) | b0);
}

UInt32 CInArchive::ReadUInt32()
{
  Byte p[4];
  ReadBytes(p, 4);
  return Get32(p);
}

UInt64 CInArchive::ReadUInt64()
{
  Byte p[8];
  ReadBytes(p, 8);
  return Get64(p);
}

UInt64 CInArchive::ReadEncInt()
{
  UInt64 val = 0;
  for (int i = 0; i < 9; i++)
  {
    Byte b = ReadByte();
    val |= (b & 0x7F);
    if (b < 0x80)
      return val;
    val <<= 7;
  }
  throw CHeaderErrorException();
}

void CInArchive::ReadGUID(Byte *g)
{
  ReadBytes(g, 16);
}

void CInArchive::ReadString(unsigned size, AString &s)
{
  s.Empty();
  if (size != 0)
  {
    ReadBytes((Byte *)s.GetBuf(size), size);
    s.ReleaseBuf_CalcLen(size);
  }
}

void CInArchive::ReadUString(unsigned size, UString &s)
{
  s.Empty();
  while (size-- != 0)
  {
    wchar_t c = ReadUInt16();
    if (c == 0)
    {
      Skip(2 * size);
      return;
    }
    s += c;
  }
}

HRESULT CInArchive::ReadChunk(IInStream *inStream, UInt64 pos, UInt64 size)
{
  RINOK(inStream->Seek(pos, STREAM_SEEK_SET, NULL));
  CLimitedSequentialInStream *streamSpec = new CLimitedSequentialInStream;
  CMyComPtr<ISequentialInStream> limitedStream(streamSpec);
  streamSpec->SetStream(inStream);
  streamSpec->Init(size);
  m_InStreamRef = limitedStream;
  _inBuffer.SetStream(limitedStream);
  _inBuffer.Init();
  return S_OK;
}

HRESULT CInArchive::ReadDirEntry(CDatabase &database)
{
  CItem item;
  UInt64 nameLen = ReadEncInt();
  if (nameLen == 0 || nameLen > (1 << 13))
    return S_FALSE;
  ReadString((unsigned)nameLen, item.Name);
  item.Section = ReadEncInt();
  item.Offset = ReadEncInt();
  item.Size = ReadEncInt();
  database.Items.Add(item);
  return S_OK;
}

HRESULT CInArchive::OpenChm(IInStream *inStream, CDatabase &database)
{
  UInt32 headerSize = ReadUInt32();
  if (headerSize != 0x60)
    return S_FALSE;
  database.PhySize = headerSize;

  UInt32 unknown1 = ReadUInt32();
  if (unknown1 != 0 && unknown1 != 1) // it's 0 in one .sll file
    return S_FALSE;

  IsArc = true;

  /* UInt32 timeStamp = */ ReadUInt32();
      // Considered as a big-endian DWORD, it appears to contain seconds (MSB) and
      // fractional seconds (second byte).
      // The third and fourth bytes may contain even more fractional bits.
      // The 4 least significant bits in the last byte are constant.
  /* UInt32 lang = */ ReadUInt32();
  Byte g[16];
  ReadGUID(g); // {7C01FD10-7BAA-11D0-9E0C-00A0-C922-E6EC}
  ReadGUID(g); // {7C01FD11-7BAA-11D0-9E0C-00A0-C922-E6EC}
  const unsigned kNumSections = 2;
  UInt64 sectionOffsets[kNumSections];
  UInt64 sectionSizes[kNumSections];
  unsigned i;
  for (i = 0; i < kNumSections; i++)
  {
    sectionOffsets[i] = ReadUInt64();
    sectionSizes[i] = ReadUInt64();
    UInt64 end = sectionOffsets[i] + sectionSizes[i];
    database.UpdatePhySize(end);
  }
  // if (chmVersion == 3)
    database.ContentOffset = ReadUInt64();
  /*
  else
    database.ContentOffset = database.StartPosition + 0x58
  */

  // Section 0
  ReadChunk(inStream, sectionOffsets[0], sectionSizes[0]);
  if (sectionSizes[0] < 0x18)
    return S_FALSE;
  if (ReadUInt32() != 0x01FE)
    return S_FALSE;
  ReadUInt32(); // unknown:  0
  UInt64 fileSize = ReadUInt64();
  database.UpdatePhySize(fileSize);
  ReadUInt32(); // unknown:  0
  ReadUInt32(); // unknown:  0

  // Section 1: The Directory Listing
  ReadChunk(inStream, sectionOffsets[1], sectionSizes[1]);
  if (ReadUInt32() != kSignature_ITSP)
    return S_FALSE;
  if (ReadUInt32() != 1) // version
    return S_FALSE;
  /* UInt32 dirHeaderSize = */ ReadUInt32();
  ReadUInt32(); // 0x0A (unknown)
  UInt32 dirChunkSize = ReadUInt32(); // $1000
  if (dirChunkSize < 32)
    return S_FALSE;
  /* UInt32 density = */ ReadUInt32(); //  "Density" of quickref section, usually 2.
  /* UInt32 depth = */ ReadUInt32(); //  Depth of the index tree: 1 there is no index,
                               // 2 if there is one level of PMGI chunks.

  /* UInt32 chunkNumber = */ ReadUInt32(); //  Chunk number of root index chunk, -1 if there is none
                                     // (though at least one file has 0 despite there being no
                                     // index chunk, probably a bug.)
  /* UInt32 firstPmglChunkNumber = */ ReadUInt32(); // Chunk number of first PMGL (listing) chunk
  /* UInt32 lastPmglChunkNumber = */ ReadUInt32();  // Chunk number of last PMGL (listing) chunk
  ReadUInt32(); // -1 (unknown)
  UInt32 numDirChunks = ReadUInt32(); // Number of directory chunks (total)
  /* UInt32 windowsLangId = */ ReadUInt32();
  ReadGUID(g);  // {5D02926A-212E-11D0-9DF9-00A0C922E6EC}
  ReadUInt32(); // 0x54 (This is the length again)
  ReadUInt32(); // -1 (unknown)
  ReadUInt32(); // -1 (unknown)
  ReadUInt32(); // -1 (unknown)

  for (UInt32 ci = 0; ci < numDirChunks; ci++)
  {
    UInt64 chunkPos = _inBuffer.GetProcessedSize();
    if (ReadUInt32() == kSignature_PMGL)
    {
      // The quickref area is written backwards from the end of the chunk.
      // One quickref entry exists for every n entries in the file, where n
      // is calculated as 1 + (1 << quickref density). So for density = 2, n = 5.

      UInt32 quickrefLength = ReadUInt32(); // Len of free space and/or quickref area at end of directory chunk
      if (quickrefLength > dirChunkSize || quickrefLength < 2)
        return S_FALSE;
      ReadUInt32(); // Always 0
      ReadUInt32(); // Chunk number of previous listing chunk when reading
                    // directory in sequence (-1 if this is the first listing chunk)
      ReadUInt32(); // Chunk number of next  listing chunk when reading
                    // directory in sequence (-1 if this is the last listing chunk)
      unsigned numItems = 0;
      
      for (;;)
      {
        UInt64 offset = _inBuffer.GetProcessedSize() - chunkPos;
        UInt32 offsetLimit = dirChunkSize - quickrefLength;
        if (offset > offsetLimit)
          return S_FALSE;
        if (offset == offsetLimit)
          break;
        RINOK(ReadDirEntry(database));
        numItems++;
      }
      
      Skip(quickrefLength - 2);
      
      unsigned rrr = ReadUInt16();
      if (rrr != numItems)
      {
        // Lazarus 9-26-2 chm contains 0 here.
        if (rrr != 0)
          return S_FALSE;
      }
    }
    else
      Skip(dirChunkSize - 4);
  }
  return S_OK;
}

HRESULT CInArchive::OpenHelp2(IInStream *inStream, CDatabase &database)
{
  if (ReadUInt32() != 1) // version
    return S_FALSE;
  if (ReadUInt32() != 0x28) // Location of header section table
    return S_FALSE;
  UInt32 numHeaderSections = ReadUInt32();
  const unsigned kNumHeaderSectionsMax = 5;
  if (numHeaderSections != kNumHeaderSectionsMax)
    return S_FALSE;

  IsArc = true;

  ReadUInt32(); // Len of post-header table
  Byte g[16];
  ReadGUID(g);  // {0A9007C1-4076-11D3-8789-0000F8105754}

  // header section table
  UInt64 sectionOffsets[kNumHeaderSectionsMax];
  UInt64 sectionSizes[kNumHeaderSectionsMax];
  UInt32 i;
  for (i = 0; i < numHeaderSections; i++)
  {
    sectionOffsets[i] = ReadUInt64();
    sectionSizes[i] = ReadUInt64();
    UInt64 end = sectionOffsets[i] + sectionSizes[i];
    database.UpdatePhySize(end);
  }
  
  // Post-Header
  ReadUInt32(); // 2
  ReadUInt32(); // 0x98: offset to CAOL from beginning of post-header)
  // ----- Directory information
  ReadUInt64(); // Chunk number of top-level AOLI chunk in directory, or -1
  ReadUInt64(); // Chunk number of first AOLL chunk in directory
  ReadUInt64(); // Chunk number of last AOLL chunk in directory
  ReadUInt64(); // 0 (unknown)
  ReadUInt32(); // $2000 (Directory chunk size of directory)
  ReadUInt32(); // Quickref density for main directory, usually 2
  ReadUInt32(); // 0 (unknown)
  ReadUInt32(); // Depth of main directory index tree
                // 1 there is no index, 2 if there is one level of AOLI chunks.
  ReadUInt64(); // 0 (unknown)
  UInt64 numDirEntries = ReadUInt64(); // Number of directory entries
  // ----- Directory Index Information
  ReadUInt64(); // -1 (unknown, probably chunk number of top-level AOLI in directory index)
  ReadUInt64(); // Chunk number of first AOLL chunk in directory index
  ReadUInt64(); // Chunk number of last AOLL chunk in directory index
  ReadUInt64(); // 0 (unknown)
  ReadUInt32(); // $200 (Directory chunk size of directory index)
  ReadUInt32(); // Quickref density for directory index, usually 2
  ReadUInt32(); // 0 (unknown)
  ReadUInt32(); // Depth of directory index index tree.
  ReadUInt64(); // Possibly flags -- sometimes 1, sometimes 0.
  ReadUInt64(); // Number of directory index entries (same as number of AOLL
               // chunks in main directory)
  
  // (The obvious guess for the following two fields, which recur in a number
  // of places, is they are maximum sizes for the directory and directory index.
  // However, I have seen no direct evidence that this is the case.)

  ReadUInt32(); // $100000 (Same as field following chunk size in directory)
  ReadUInt32(); // $20000 (Same as field following chunk size in directory index)

  ReadUInt64(); // 0 (unknown)
  if (ReadUInt32() != kSignature_CAOL)
    return S_FALSE;
  if (ReadUInt32() != 2) // (Most likely a version number)
    return S_FALSE;
  UInt32 caolLength = ReadUInt32(); // $50 (Len of the CAOL section, which includes the ITSF section)
  if (caolLength >= 0x2C)
  {
    /* UInt32 c7 = */ ReadUInt16(); // Unknown.  Remains the same when identical files are built.
              // Does not appear to be a checksum.  Many files have
              // 'HH' (HTML Help?) here, indicating this may be a compiler ID
              //  field.  But at least one ITOL/ITLS compiler does not set this
              // field to a constant value.
    ReadUInt16(); // 0 (Unknown.  Possibly part of 00A4 field)
    ReadUInt32(); // Unknown.  Two values have been seen -- $43ED, and 0.
    ReadUInt32(); // $2000 (Directory chunk size of directory)
    ReadUInt32(); // $200 (Directory chunk size of directory index)
    ReadUInt32(); // $100000 (Same as field following chunk size in directory)
    ReadUInt32(); // $20000 (Same as field following chunk size in directory index)
    ReadUInt32(); // 0 (unknown)
    ReadUInt32(); // 0 (Unknown)
    if (caolLength == 0x2C)
    {
      // fprintf(stdout, "\n !!!NewFormat\n");
      // fflush(stdout);
      database.ContentOffset = 0; // maybe we must add database.StartPosition here?
      database.NewFormat = true;
    }
    else if (caolLength == 0x50)
    {
      ReadUInt32(); // 0 (Unknown)
      if (ReadUInt32() != kSignature_ITSF)
        return S_FALSE;
      if (ReadUInt32() != 4) // $4 (Version number -- CHM uses 3)
        return S_FALSE;
      if (ReadUInt32() != 0x20) // $20 (length of ITSF)
        return S_FALSE;
      UInt32 unknown = ReadUInt32();
      if (unknown != 0 && unknown != 1) // = 0 for some HxW files, 1 in other cases;
        return S_FALSE;
      database.ContentOffset = database.StartPosition + ReadUInt64();
      /* UInt32 timeStamp = */ ReadUInt32();
          // A timestamp of some sort.
          // Considered as a big-endian DWORD, it appears to contain
          // seconds (MSB) and fractional seconds (second byte).
          // The third and fourth bytes may contain even more fractional
          // bits.  The 4 least significant bits in the last byte are constant.
      /* UInt32 lang = */ ReadUInt32(); // BE?
    }
    else
      return S_FALSE;
  }

  // Section 0
  ReadChunk(inStream, database.StartPosition + sectionOffsets[0], sectionSizes[0]);
  if (sectionSizes[0] < 0x18)
    return S_FALSE;
  if (ReadUInt32() != 0x01FE)
    return S_FALSE;
  ReadUInt32(); // unknown:  0
  UInt64 fileSize = ReadUInt64();
  database.UpdatePhySize(fileSize);
  ReadUInt32(); // unknown:  0
  ReadUInt32(); // unknown:  0

  // Section 1: The Directory Listing
  ReadChunk(inStream, database.StartPosition + sectionOffsets[1], sectionSizes[1]);
  if (ReadUInt32() != kSignature_IFCM)
    return S_FALSE;
  if (ReadUInt32() != 1) // (probably a version number)
    return S_FALSE;
  UInt32 dirChunkSize = ReadUInt32(); // $2000
  if (dirChunkSize < 64)
    return S_FALSE;
  ReadUInt32(); // $100000  (unknown)
  ReadUInt32(); // -1 (unknown)
  ReadUInt32(); // -1 (unknown)
  UInt32 numDirChunks = ReadUInt32();
  ReadUInt32(); // 0 (unknown, probably high word of above)

  for (UInt32 ci = 0; ci < numDirChunks; ci++)
  {
    UInt64 chunkPos = _inBuffer.GetProcessedSize();
    if (ReadUInt32() == kSignature_AOLL)
    {
      UInt32 quickrefLength = ReadUInt32(); // Len of quickref area at end of directory chunk
      if (quickrefLength > dirChunkSize || quickrefLength < 2)
        return S_FALSE;
      ReadUInt64(); // Directory chunk number
            // This must match physical position in file, that is
            // the chunk size times the chunk number must be the
            // offset from the end of the directory header.
      ReadUInt64(); // Chunk number of previous listing chunk when reading
                    // directory in sequence (-1 if first listing chunk)
      ReadUInt64(); // Chunk number of next listing chunk when reading
                    // directory in sequence (-1 if last listing chunk)
      ReadUInt64(); // Number of first listing entry in this chunk
      ReadUInt32(); // 1 (unknown -- other values have also been seen here)
      ReadUInt32(); // 0 (unknown)
      
      unsigned numItems = 0;
      for (;;)
      {
        UInt64 offset = _inBuffer.GetProcessedSize() - chunkPos;
        UInt32 offsetLimit = dirChunkSize - quickrefLength;
        if (offset > offsetLimit)
          return S_FALSE;
        if (offset == offsetLimit)
          break;
        if (database.NewFormat)
        {
          UInt16 nameLen = ReadUInt16();
          if (nameLen == 0)
            return S_FALSE;
          UString name;
          ReadUString((unsigned)nameLen, name);
          AString s;
          ConvertUnicodeToUTF8(name, s);
          Byte b = ReadByte();
          s.Add_Space();
          PrintByte(b, s);
          s.Add_Space();
          UInt64 len = ReadEncInt();
          // then number of items ?
          // then length ?
          // then some data (binary encoding?)
          while (len-- != 0)
          {
            b = ReadByte();
            PrintByte(b, s);
          }
          database.NewFormatString += s;
          database.NewFormatString += "\r\n";
        }
        else
        {
          RINOK(ReadDirEntry(database));
        }
        numItems++;
      }
      Skip(quickrefLength - 2);
      if (ReadUInt16() != numItems)
        return S_FALSE;
      if (numItems > numDirEntries)
        return S_FALSE;
      numDirEntries -= numItems;
    }
    else
      Skip(dirChunkSize - 4);
  }
  return numDirEntries == 0 ? S_OK : S_FALSE;
}

HRESULT CInArchive::DecompressStream(IInStream *inStream, const CDatabase &database, const AString &name)
{
  int index = database.FindItem(name);
  if (index < 0)
    return S_FALSE;
  const CItem &item = database.Items[index];
  _chunkSize = item.Size;
  return ReadChunk(inStream, database.ContentOffset + item.Offset, item.Size);
}


#define DATA_SPACE "::DataSpace/"
#define kNameList DATA_SPACE "NameList"
#define kStorage DATA_SPACE "Storage/"
#define kContent "Content"
#define kControlData "ControlData"
#define kSpanInfo "SpanInfo"
#define kTransform "Transform/"
#define kResetTable "/InstanceData/ResetTable"
#define kTransformList "List"

static AString GetSectionPrefix(const AString &name)
{
  AString s (kStorage);
  s += name;
  s += '/';
  return s;
}

#define RINOZ(x) { int __tt = (x); if (__tt != 0) return __tt; }

static int CompareFiles(const unsigned *p1, const unsigned *p2, void *param)
{
  const CObjectVector<CItem> &items = *(const CObjectVector<CItem> *)param;
  const CItem &item1 = items[*p1];
  const CItem &item2 = items[*p2];
  bool isDir1 = item1.IsDir();
  bool isDir2 = item2.IsDir();
  if (isDir1 && !isDir2)
    return -1;
  if (isDir2)
  {
    if (!isDir1)
      return 1;
  }
  else
  {
    RINOZ(MyCompare(item1.Section, item2.Section));
    RINOZ(MyCompare(item1.Offset, item2.Offset));
    RINOZ(MyCompare(item1.Size, item2.Size));
  }
  return MyCompare(*p1, *p2);
}

void CFilesDatabase::SetIndices()
{
  FOR_VECTOR (i, Items)
  {
    const CItem &item = Items[i];
    if (item.IsUserItem() && item.Name.Len() != 1)
      Indices.Add(i);
  }
}

void CFilesDatabase::Sort()
{
  Indices.Sort(CompareFiles, (void *)&Items);
}

bool CFilesDatabase::Check()
{
  UInt64 maxPos = 0;
  UInt64 prevSection = 0;
  FOR_VECTOR (i, Indices)
  {
    const CItem &item = Items[Indices[i]];
    if (item.Section == 0 || item.IsDir())
      continue;
    if (item.Section != prevSection)
    {
      prevSection = item.Section;
      maxPos = 0;
      continue;
    }
    if (item.Offset < maxPos)
      return false;
    maxPos = item.Offset + item.Size;
    if (maxPos < item.Offset)
      return false;
  }
  return true;
}

bool CFilesDatabase::CheckSectionRefs()
{
  FOR_VECTOR (i, Indices)
  {
    const CItem &item = Items[Indices[i]];
    if (item.Section == 0 || item.IsDir())
      continue;
    if (item.Section >= Sections.Size())
      return false;
  }
  return true;
}

static int inline GetLog(UInt32 num)
{
  for (int i = 0; i < 32; i++)
    if (((UInt32)1 << i) == num)
      return i;
  return -1;
}

HRESULT CInArchive::OpenHighLevel(IInStream *inStream, CFilesDatabase &database)
{
  {
    // The NameList file
    RINOK(DecompressStream(inStream, database, (AString)kNameList));
    /* UInt16 length = */ ReadUInt16();
    UInt16 numSections = ReadUInt16();
    for (unsigned i = 0; i < numSections; i++)
    {
      CSectionInfo section;
      UInt16 nameLen = ReadUInt16();
      UString name;
      ReadUString(nameLen, name);
      if (ReadUInt16() != 0)
        return S_FALSE;
      ConvertUnicodeToUTF8(name, section.Name);
      // if (!ConvertUnicodeToUTF8(name, section.Name)) return S_FALSE;
      database.Sections.Add(section);
    }
  }

  unsigned si;
  for (si = 1; si < database.Sections.Size(); si++)
  {
    CSectionInfo &section = database.Sections[si];
    AString sectionPrefix (GetSectionPrefix(section.Name));
    {
      // Content
      int index = database.FindItem(sectionPrefix + kContent);
      if (index < 0)
        return S_FALSE;
      const CItem &item = database.Items[index];
      section.Offset = item.Offset;
      section.CompressedSize = item.Size;
    }
    AString transformPrefix (sectionPrefix + kTransform);
    if (database.Help2Format)
    {
      // Transform List
      RINOK(DecompressStream(inStream, database, transformPrefix + kTransformList));
      if ((_chunkSize & 0xF) != 0)
        return S_FALSE;
      unsigned numGuids = (unsigned)(_chunkSize / 0x10);
      if (numGuids < 1)
        return S_FALSE;
      for (unsigned i = 0; i < numGuids; i++)
      {
        CMethodInfo method;
        ReadGUID(method.Guid);
        section.Methods.Add(method);
      }
    }
    else
    {
      CMethodInfo method;
      memcpy(method.Guid, kChmLzxGuid, 16);
      section.Methods.Add(method);
    }

    {
      // Control Data
      RINOK(DecompressStream(inStream, database, sectionPrefix + kControlData));
      
      FOR_VECTOR (mi, section.Methods)
      {
        CMethodInfo &method = section.Methods[mi];
        UInt32 numDWORDS = ReadUInt32();
        if (method.IsLzx())
        {
          if (numDWORDS < 5)
            return S_FALSE;
          if (ReadUInt32() != kSignature_LZXC)
            return S_FALSE;
          CLzxInfo &li = method.LzxInfo;
          li.Version = ReadUInt32();
          if (li.Version != 2 && li.Version != 3)
            return S_FALSE;
          
          {
            // There is bug in VC6, if we use function call as parameter for inline function
            UInt32 val32 = ReadUInt32();
            int n = GetLog(val32);
            if (n < 0 || n > 16)
              return S_FALSE;
            li.ResetIntervalBits = n;
          }
          
          {
            UInt32 val32 = ReadUInt32();
            int n = GetLog(val32);
            if (n < 0 || n > 16)
              return S_FALSE;
            li.WindowSizeBits = n;
          }

          li.CacheSize = ReadUInt32();
          numDWORDS -= 5;
          while (numDWORDS-- != 0)
            ReadUInt32();
        }
        else
        {
          UInt32 numBytes = numDWORDS * 4;
          method.ControlData.Alloc(numBytes);
          ReadBytes(method.ControlData, numBytes);
        }
      }
    }

    {
      // SpanInfo
      RINOK(DecompressStream(inStream, database, sectionPrefix + kSpanInfo));
      section.UncompressedSize = ReadUInt64();
    }

    // read ResetTable for LZX
    FOR_VECTOR (mi, section.Methods)
    {
      CMethodInfo &method = section.Methods[mi];
      if (method.IsLzx())
      {
        // ResetTable;
        RINOK(DecompressStream(inStream, database, transformPrefix +
            method.GetGuidString() + kResetTable));
        CResetTable &rt = method.LzxInfo.ResetTable;
        
        if (_chunkSize < 4)
        {
          if (_chunkSize != 0)
            return S_FALSE;
          // ResetTable is empty in .chw files
          if (section.UncompressedSize != 0)
            return S_FALSE;
          rt.UncompressedSize = 0;
          rt.CompressedSize = 0;
          // rt.BlockSize = 0;
        }
        else
        {
          UInt32 ver = ReadUInt32(); // 2  unknown (possibly a version number)
          if (ver != 2 && ver != 3)
            return S_FALSE;
          UInt32 numEntries = ReadUInt32();
          const unsigned kEntrySize = 8;
          if (ReadUInt32() != kEntrySize)
            return S_FALSE;
          const unsigned kRtHeaderSize = 4 * 4 + 8 * 3;
          if (ReadUInt32() != kRtHeaderSize)
            return S_FALSE;
          if (kRtHeaderSize + kEntrySize * (UInt64)numEntries != _chunkSize)
            return S_FALSE;

          rt.UncompressedSize = ReadUInt64();
          rt.CompressedSize = ReadUInt64();
          UInt64 blockSize = ReadUInt64();
          if (blockSize != kBlockSize)
            return S_FALSE;
          UInt64 numBlocks = (rt.UncompressedSize + kBlockSize + 1) / kBlockSize;
          if (numEntries != numBlocks &&
              numEntries != numBlocks + 1)
            return S_FALSE;
  
          rt.ResetOffsets.ClearAndReserve(numEntries);
          
          for (UInt32 i = 0; i < numEntries; i++)
          {
            UInt64 v = ReadUInt64();
            if (i != 0 && v < rt.ResetOffsets[i - 1])
              return S_FALSE;
            rt.ResetOffsets.AddInReserved(v);
          }

          if (numEntries != 0)
            if (rt.ResetOffsets[0] != 0)
              return S_FALSE;

          if (numEntries == numBlocks + 1)
          {
            // Lazarus 9-26-2 chm contains additional entty
            if (rt.ResetOffsets.Back() != rt.CompressedSize)
              return S_FALSE;
          }
        }
      }
    }
  }

  database.SetIndices();
  database.Sort();
  return database.Check() ? S_OK : S_FALSE;
}

HRESULT CInArchive::Open2(IInStream *inStream,
    const UInt64 *searchHeaderSizeLimit,
    CFilesDatabase &database)
{
  IsArc = false;
  HeadersError = false;
  UnexpectedEnd = false;
  UnsupportedFeature = false;

  database.Clear();
  database.Help2Format = _help2;
  const UInt32 chmVersion = 3;

  RINOK(inStream->Seek(0, STREAM_SEEK_CUR, &database.StartPosition));

  if (!_inBuffer.Create(1 << 14))
    return E_OUTOFMEMORY;
  _inBuffer.SetStream(inStream);
  _inBuffer.Init();
  
  if (_help2)
  {
    const unsigned kSignatureSize = 8;
    const UInt64 signature = ((UInt64)kSignature_ITLS << 32) | kSignature_ITOL;
    UInt64 limit = 1 << 18;
    
    if (searchHeaderSizeLimit)
      if (limit > *searchHeaderSizeLimit)
        limit = *searchHeaderSizeLimit;

    UInt64 val = 0;
    
    for (;;)
    {
      Byte b;
      if (!_inBuffer.ReadByte(b))
        return S_FALSE;
      val >>= 8;
      val |= ((UInt64)b) << ((kSignatureSize - 1) * 8);
      if (_inBuffer.GetProcessedSize() >= kSignatureSize)
      {
        if (val == signature)
          break;
        if (_inBuffer.GetProcessedSize() > limit)
          return S_FALSE;
      }
    }
    
    database.StartPosition += _inBuffer.GetProcessedSize() - kSignatureSize;
    RINOK(OpenHelp2(inStream, database));
    if (database.NewFormat)
      return S_OK;
  }
  else
  {
    if (ReadUInt32() != kSignature_ITSF)
      return S_FALSE;
    if (ReadUInt32() != chmVersion)
      return S_FALSE;
    RINOK(OpenChm(inStream, database));
  }


  #ifndef CHM_LOW
  
  try
  {
    try
    {
      HRESULT res = OpenHighLevel(inStream, database);
      if (res == S_FALSE)
      {
        UnsupportedFeature = true;
        database.HighLevelClear();
        return S_OK;
      }
      RINOK(res);
      if (!database.CheckSectionRefs())
        HeadersError = true;
      database.LowLevel = false;
    }
    catch(...)
    {
      database.HighLevelClear();
      throw;
    }
  }
  // catch(const CInBufferException &e) { return e.ErrorCode; }
  catch(CEnexpectedEndException &) { UnexpectedEnd = true; }
  catch(CHeaderErrorException &) { HeadersError = true; }
  catch(...) { throw; }

  #endif

  return S_OK;
}

HRESULT CInArchive::Open(IInStream *inStream,
    const UInt64 *searchHeaderSizeLimit,
    CFilesDatabase &database)
{
  try
  {
    try
    {
      HRESULT res = Open2(inStream, searchHeaderSizeLimit, database);
      m_InStreamRef.Release();
      return res;
    }
    catch(...)
    {
      m_InStreamRef.Release();
      throw;
    }
  }
  catch(const CInBufferException &e) { return e.ErrorCode; }
  catch(CEnexpectedEndException &) { UnexpectedEnd = true; }
  catch(CHeaderErrorException &) { HeadersError = true; }
  return S_FALSE;
}

}}

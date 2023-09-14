// Archive/WimIn.h

#ifndef ZIP7_INC_ARCHIVE_WIM_IN_H
#define ZIP7_INC_ARCHIVE_WIM_IN_H

#include "../../../../C/Alloc.h"

#include "../../../Common/MyBuffer.h"
#include "../../../Common/MyXml.h"

#include "../../../Windows/PropVariant.h"

#include "../../Compress/CopyCoder.h"
#include "../../Compress/LzmsDecoder.h"
#include "../../Compress/LzxDecoder.h"

#include "../IArchive.h"

namespace NArchive {
namespace NWim {

/*
WIM versions:
hexVer : headerSize : ver
 : 1.07.01 - 1.08.01 : Longhorn.4001-4015 - another header, no signature, CAB compression
10900 : 60 : 1.09 : Longhorn.4029-4039 (2003)
10A00 : 60 : 1.10 : Longhorn.4083 (2004) image starting from 1
10B00 : ?? : 1.11 : ??
10C00 : 74 : 1.12 : Longhorn.4093 - VistaBeta1.5112 (2005) - (Multi-Part, SHA1)
10D00 : D0 : 1.13 : VistaBeta2 - Win10, (NumImages, BootIndex, IntegrityResource)
00E00 : D0 : 0.14 : LZMS, solid, esd, dism
*/

const unsigned kDirRecordSizeOld = 62;
const unsigned kDirRecordSize = 102;

/*
  There is error in WIM specification about dwReparseTag, dwReparseReserved and liHardLink fields.

  Correct DIRENTRY structure:
  {
    hex offset
     0    UInt64  Len;
     8    UInt32  Attrib;
     C    UInt32  SecurityId;
    
    10    UInt64  SubdirOffset; // = 0 for files

    18    UInt64  unused1; // = 0?
    20    UInt64  unused2; // = 0?
    
    28    UInt64  CTime;
    30    UInt64  ATime;
    38    UInt64  MTime;
    
    40    Byte    Sha1[20];
    
    54    UInt32  Unknown1; // is it 0 always?

       
    union
    {
    58    UInt64  NtNodeId;
        {
    58    UInt32  ReparseTag;
    5C    UInt32  ReparseFlags; // is it 0 always? Check with new imagex.
        }
    }

    60    UInt16  Streams;
    
    62    UInt16  ShortNameLen;
    64    UInt16  FileNameLen;
    
    66    UInt16  Name[];
          UInt16  ShortName[];
  }

  // DIRENTRY for WIM_VERSION <= 1.10
  DIRENTRY_OLD structure:
  {
    hex offset
     0    UInt64  Len;
     8    UInt32  Attrib;
     C    UInt32  SecurityId;

    union
    {
    10    UInt64  SubdirOffset; //

    10    UInt32  OldWimFileId; // used for files in old WIMs
    14    UInt32  OldWimFileId_Reserved; // = 0
    }

    18    UInt64  CTime;
    20    UInt64  ATime;
    28    UInt64  MTime;
    
    30    UInt64  Unknown; // NtNodeId ?

    38    UInt16  Streams;
    3A    UInt16  ShortNameLen;
    3C    UInt16  FileNameLen;
    3E    UInt16  FileName[];
          UInt16  ShortName[];
  }

  ALT_STREAM structure:
  {
    hex offset
     0    UInt64  Len;
     8    UInt64  Unused;
    10    Byte    Sha1[20];
    24    UInt16  FileNameLen;
    26    UInt16  FileName[];
  }

  ALT_STREAM_OLD structure:
  {
    hex offset
     0    UInt64  Len;
     8    UInt64  StreamId; // 32-bit value
    10    UInt16  FileNameLen;
    12    UInt16  FileName[];
  }

  If item is file (not Directory) and there are alternative streams,
  there is additional ALT_STREAM item of main "unnamed" stream in Streams array.

*/


namespace NResourceFlags
{
  // const Byte kFree = 1 << 0;
  const Byte kMetadata = 1 << 1;
  const Byte kCompressed = 1 << 2;
  // const Byte kSpanned = 1 << 3;
  const Byte kSolid = 1 << 4;
}

const UInt64 k_SolidBig_Resource_Marker = (UInt64)1 << 32;

struct CResource
{
  UInt64 PackSize;
  UInt64 Offset;
  UInt64 UnpackSize;
  Byte Flags;
  bool KeepSolid;
  int SolidIndex;

  void Clear()
  {
    PackSize = 0;
    Offset = 0;
    UnpackSize = 0;
    Flags = 0;
    KeepSolid = false;
    SolidIndex = -1;
  }

  UInt64 GetEndLimit() const { return Offset + PackSize; }
  void Parse(const Byte *p);
  void ParseAndUpdatePhySize(const Byte *p, UInt64 &phySize)
  {
    Parse(p);
    UInt64 v = GetEndLimit();
    if (phySize < v)
      phySize = v;
  }

  void WriteTo(Byte *p) const;

  bool IsMetadata() const { return (Flags & NResourceFlags::kMetadata) != 0; }
  bool IsCompressed() const { return (Flags & NResourceFlags::kCompressed) != 0; }
  bool IsSolid() const { return (Flags & NResourceFlags::kSolid) != 0; }
  bool IsSolidBig() const { return IsSolid() && UnpackSize == k_SolidBig_Resource_Marker; }
  bool IsSolidSmall() const { return IsSolid() && UnpackSize == 0; }

  bool IsEmpty() const { return (UnpackSize == 0); }
};


struct CSolid
{
  unsigned StreamIndex;
  // unsigned NumRefs;
  int FirstSmallStream;
  
  UInt64 SolidOffset;
  
  UInt64 UnpackSize;
  int Method;
  unsigned ChunkSizeBits;

  UInt64 HeadersSize;
  // size_t NumChunks;
  CObjArray<UInt64> Chunks; // [NumChunks + 1] (start offset)

  UInt64 GetChunkPackSize(size_t chunkIndex) const { return Chunks[chunkIndex + 1] - Chunks[chunkIndex]; }

  CSolid():
      FirstSmallStream(-1),
      // NumRefs(0),
      Method(-1)
      {}
};


namespace NHeaderFlags
{
  const UInt32 kCompression  = 1 << 1;
  const UInt32 kReadOnly     = 1 << 2;
  const UInt32 kSpanned      = 1 << 3;
  const UInt32 kResourceOnly = 1 << 4;
  const UInt32 kMetadataOnly = 1 << 5;
  const UInt32 kWriteInProgress = 1 << 6;
  const UInt32 kReparsePointFixup = 1 << 7;
  
  const UInt32 kXPRESS       = (UInt32)1 << 17;
  const UInt32 kLZX          = (UInt32)1 << 18;
  const UInt32 kLZMS         = (UInt32)1 << 19;
  const UInt32 kXPRESS2      = (UInt32)1 << 21; // XPRESS with nonstandard chunk size ?

  const UInt32 kMethodMask   = 0xFFFE0000;
}


namespace NMethod
{
  const UInt32 kXPRESS = 1;
  const UInt32 kLZX    = 2;
  const UInt32 kLZMS   = 3;
}


const UInt32 k_Version_NonSolid = 0x10D00;
const UInt32 k_Version_Solid = 0xE00;

const unsigned kHeaderSizeMax = 0xD0;
const unsigned kSignatureSize = 8;
extern const Byte kSignature[kSignatureSize];

const unsigned kChunkSizeBits = 15;
const UInt32 kChunkSize = (UInt32)1 << kChunkSizeBits;


struct CHeader
{
  UInt32 Version;
  UInt32 Flags;
  UInt32 ChunkSize;
  unsigned ChunkSizeBits;
  Byte Guid[16];
  UInt16 PartNumber;
  UInt16 NumParts;
  UInt32 NumImages;
  UInt32 BootIndex;

  bool _isOldVersion; // 1.10-
  bool _isNewVersion; // 1.13+ or 0.14

  CResource OffsetResource;
  CResource XmlResource;
  CResource MetadataResource;
  CResource IntegrityResource;

  void SetDefaultFields(bool useLZX);

  void WriteTo(Byte *p) const;
  HRESULT Parse(const Byte *p, UInt64 &phySize);
  
  bool IsCompressed() const { return (Flags & NHeaderFlags::kCompression) != 0; }
  
  bool IsSupported() const
  {
    return (!IsCompressed()
        || (Flags & NHeaderFlags::kLZX) != 0
        || (Flags & NHeaderFlags::kXPRESS) != 0
        || (Flags & NHeaderFlags::kLZMS) != 0
        || (Flags & NHeaderFlags::kXPRESS2) != 0);
  }
  
  unsigned GetMethod() const
  {
    if (!IsCompressed())
      return 0;
    UInt32 mask = (Flags & NHeaderFlags::kMethodMask);
    if (mask == 0) return 0;
    if (mask == NHeaderFlags::kXPRESS) return NMethod::kXPRESS;
    if (mask == NHeaderFlags::kLZX) return NMethod::kLZX;
    if (mask == NHeaderFlags::kLZMS) return NMethod::kLZMS;
    if (mask == NHeaderFlags::kXPRESS2) return NMethod::kXPRESS;
    return mask;
  }

  bool IsOldVersion() const { return _isOldVersion; }
  bool IsNewVersion() const { return _isNewVersion; }
  bool IsSolidVersion() const { return (Version == k_Version_Solid); }

  bool AreFromOnArchive(const CHeader &h)
  {
    return (memcmp(Guid, h.Guid, sizeof(Guid)) == 0) && (h.NumParts == NumParts);
  }
};


const unsigned kHashSize = 20;

inline bool IsEmptySha(const Byte *data)
{
  for (unsigned i = 0; i < kHashSize; i++)
    if (data[i] != 0)
      return false;
  return true;
}

const unsigned kStreamInfoSize = 24 + 2 + 4 + kHashSize;

struct CStreamInfo
{
  CResource Resource;
  UInt16 PartNumber;      // for NEW WIM format, we set it to 1 for OLD WIM format
  UInt32 RefCount;
  UInt32 Id;              // for OLD WIM format
  Byte Hash[kHashSize];

  bool IsEmptyHash() const { return IsEmptySha(Hash); }
  
  void WriteTo(Byte *p) const;
};


struct CItem
{
  size_t Offset;
  int IndexInSorted;
  int StreamIndex;
  int Parent;
  int ImageIndex; // -1 means that file is unreferenced in Images (deleted item?)
  bool IsDir;
  bool IsAltStream;

  bool HasMetadata() const { return ImageIndex >= 0; }

  CItem():
    IndexInSorted(-1),
    StreamIndex(-1),
    Parent(-1),
    IsDir(false),
    IsAltStream(false)
    {}
};

struct CImage
{
  CByteBuffer Meta;
  CRecordVector<UInt32> SecurOffsets;
  unsigned StartItem;
  unsigned NumItems;
  unsigned NumEmptyRootItems;
  int VirtualRootIndex; // index in CDatabase::VirtualRoots[]
  UString RootName;
  CByteBuffer RootNameBuf;

  CImage(): VirtualRootIndex(-1) {}
};


struct CImageInfo
{
  bool CTimeDefined;
  bool MTimeDefined;
  bool NameDefined;
  bool IndexDefined;
  
  FILETIME CTime;
  FILETIME MTime;
  UString Name;

  UInt64 DirCount;
  UInt64 FileCount;
  UInt32 Index;

  int ItemIndexInXml;

  UInt64 GetTotalFilesAndDirs() const { return DirCount + FileCount; }
  
  CImageInfo(): CTimeDefined(false), MTimeDefined(false), NameDefined(false),
      IndexDefined(false), ItemIndexInXml(-1) {}
  void Parse(const CXmlItem &item);
};


struct CWimXml
{
  CByteBuffer Data;
  CXml Xml;

  UInt16 VolIndex;
  CObjectVector<CImageInfo> Images;

  UString FileName;
  bool IsEncrypted;

  UInt64 GetTotalFilesAndDirs() const
  {
    UInt64 sum = 0;
    FOR_VECTOR (i, Images)
      sum += Images[i].GetTotalFilesAndDirs();
    return sum;
  }

  void ToUnicode(UString &s);
  bool Parse();

  CWimXml(): IsEncrypted(false) {}
};


struct CVolume
{
  CHeader Header;
  CMyComPtr<IInStream> Stream;
};


class CDatabase
{
  Byte *DirData;
  size_t DirSize;
  size_t DirProcessed;
  size_t DirStartOffset;
  IArchiveOpenCallback *OpenCallback;

  HRESULT ParseDirItem(size_t pos, int parent);
  HRESULT ParseImageDirs(CByteBuffer &buf, int parent);

public:
  CRecordVector<CStreamInfo> DataStreams;
  CRecordVector<CStreamInfo> MetaStreams;

  CObjectVector<CSolid> Solids;
  
  CRecordVector<CItem> Items;
  CObjectVector<CByteBuffer> ReparseItems;
  CIntVector ItemToReparse; // from index_in_Items to index_in_ReparseItems
                            // -1 means no reparse;
  
  CObjectVector<CImage> Images;
  
  bool IsOldVersion9;
  bool IsOldVersion;
  bool ThereAreDeletedStreams;
  bool ThereAreAltStreams;
  bool RefCountError;
  bool HeadersError;

  unsigned GetStartImageIndex() const { return IsOldVersion9 ? 0 : 1; }
  unsigned GetDirAlignMask() const { return IsOldVersion9 ? 3 : 7; }
  
  // User Items can contain all images or just one image from all.
  CUIntVector SortedItems;
  int IndexOfUserImage;    // -1 : if more than one images was filled to Sorted Items
  
  unsigned NumExcludededItems;
  int ExludedItem;          // -1 : if there are no exclude items
  CUIntVector VirtualRoots; // we use them for old 1.10 WIM archives

  bool ThereIsError() const { return RefCountError || HeadersError; }

  unsigned GetNumUserItemsInImage(unsigned imageIndex) const
  {
    if (IndexOfUserImage >= 0 && imageIndex != (unsigned)IndexOfUserImage)
      return 0;
    if (imageIndex >= Images.Size())
      return 0;
    return Images[imageIndex].NumItems - NumExcludededItems;
  }

  bool ItemHasStream(const CItem &item) const;

  UInt64 Get_UnpackSize_of_Resource(const CResource &r) const
  {
    if (!r.IsSolid())
      return r.UnpackSize;
    if (r.IsSolidSmall())
      return r.PackSize;
    if (r.IsSolidBig() && r.SolidIndex >= 0)
      return Solids[(unsigned)r.SolidIndex].UnpackSize;
    return 0;
  }

  UInt64 Get_PackSize_of_Resource(unsigned streamIndex) const
  {
    const CResource &r = DataStreams[streamIndex].Resource;
    if (!r.IsSolidSmall())
      return r.PackSize;
    if (r.SolidIndex >= 0)
    {
      const CSolid &ss = Solids[(unsigned)r.SolidIndex];
      if (ss.FirstSmallStream == (int)streamIndex)
        return DataStreams[ss.StreamIndex].Resource.PackSize;
    }
    return 0;
  }

  UInt64 GetUnpackSize() const
  {
    UInt64 res = 0;
    FOR_VECTOR (i, DataStreams)
      res += DataStreams[i].Resource.UnpackSize;
    return res;
  }

  UInt64 GetPackSize() const
  {
    UInt64 res = 0;
    FOR_VECTOR (i, DataStreams)
      res += DataStreams[i].Resource.PackSize;
    return res;
  }

  void Clear()
  {
    DataStreams.Clear();
    MetaStreams.Clear();
    Solids.Clear();
    
    Items.Clear();
    ReparseItems.Clear();
    ItemToReparse.Clear();

    SortedItems.Clear();
    
    Images.Clear();
    VirtualRoots.Clear();

    IsOldVersion = false;
    ThereAreDeletedStreams = false;
    ThereAreAltStreams = false;
    RefCountError = false;
    HeadersError = false;
  }

  CDatabase():
    RefCountError(false),
    HeadersError(false)
    {}

  void GetShortName(unsigned index, NWindows::NCOM::CPropVariant &res) const;
  void GetItemName(unsigned index1, NWindows::NCOM::CPropVariant &res) const;
  void GetItemPath(unsigned index, bool showImageNumber, NWindows::NCOM::CPropVariant &res) const;

  HRESULT OpenXml(IInStream *inStream, const CHeader &h, CByteBuffer &xml);
  HRESULT Open(IInStream *inStream, const CHeader &h, unsigned numItemsReserve, IArchiveOpenCallback *openCallback);
  HRESULT FillAndCheck(const CObjectVector<CVolume> &volumes);

  /*
    imageIndex showImageNumber NumImages
         *        true           *       Show Image_Number
        -1           *          >1       Show Image_Number
        -1        false          1       Don't show Image_Number
         N        false          *       Don't show Image_Number
  */
  HRESULT GenerateSortedItems(int imageIndex, bool showImageNumber);

  HRESULT ExtractReparseStreams(const CObjectVector<CVolume> &volumes, IArchiveOpenCallback *openCallback);
};

HRESULT ReadHeader(IInStream *inStream, CHeader &header, UInt64 &phySize);


struct CMidBuf
{
  Byte *Data;
  size_t _size;

  CMidBuf(): Data(NULL), _size(0) {}

  void EnsureCapacity(size_t size)
  {
    if (size > _size)
    {
      ::MidFree(Data);
      _size = 0;
      Data = (Byte *)::MidAlloc(size);
      if (Data)
        _size = size;
    }
  }

  ~CMidBuf() { ::MidFree(Data); }
};


class CUnpacker
{
  NCompress::CCopyCoder *copyCoderSpec;
  CMyComPtr<ICompressCoder> copyCoder;

  NCompress::NLzx::CDecoder *lzxDecoderSpec;
  CMyComPtr<IUnknown> lzxDecoder;

  NCompress::NLzms::CDecoder *lzmsDecoder;

  CByteBuffer sizesBuf;

  CMidBuf packBuf;
  CMidBuf unpackBuf;

  // solid resource
  int _solidIndex;
  size_t _unpackedChunkIndex;

  HRESULT UnpackChunk(
      ISequentialInStream *inStream,
      unsigned method, unsigned chunkSizeBits,
      size_t inSize, size_t outSize,
      ISequentialOutStream *outStream);

  HRESULT Unpack2(
      IInStream *inStream,
      const CResource &res,
      const CHeader &header,
      const CDatabase *db,
      ISequentialOutStream *outStream,
      ICompressProgressInfo *progress);

public:
  UInt64 TotalPacked;

  CUnpacker():
      lzmsDecoder(NULL),
      _solidIndex(-1),
      _unpackedChunkIndex(0),
      TotalPacked(0)
      {}
  ~CUnpacker();

  HRESULT Unpack(
      IInStream *inStream,
      const CResource &res,
      const CHeader &header,
      const CDatabase *db,
      ISequentialOutStream *outStream,
      ICompressProgressInfo *progress,
      Byte *digest);

  HRESULT UnpackData(IInStream *inStream,
      const CResource &resource, const CHeader &header,
      const CDatabase *db,
      CByteBuffer &buf, Byte *digest);
};

}}
  
#endif

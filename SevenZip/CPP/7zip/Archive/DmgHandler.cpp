// DmgHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/IntToString.h"
#include "../../Common/MyXml.h"
#include "../../Common/UTFConvert.h"

#include "../../Windows/PropVariant.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#include "../Compress/BZip2Decoder.h"
#include "../Compress/CopyCoder.h"
#include "../Compress/LzfseDecoder.h"
#include "../Compress/ZlibDecoder.h"

#include "Common/OutStreamWithCRC.h"

// #define DMG_SHOW_RAW

// #include <stdio.h>
#define PRF(x) // x

#define Get16(p) GetBe16(p)
#define Get32(p) GetBe32(p)
#define Get64(p) GetBe64(p)

Byte *Base64ToBin(Byte *dest, const char *src);

namespace NArchive {
namespace NDmg {


static const UInt32  METHOD_ZERO_0  = 0;
static const UInt32  METHOD_COPY    = 1;
static const UInt32  METHOD_ZERO_2  = 2; // without file CRC calculation
static const UInt32  METHOD_ADC     = 0x80000004;
static const UInt32  METHOD_ZLIB    = 0x80000005;
static const UInt32  METHOD_BZIP2   = 0x80000006;
static const UInt32  METHOD_LZFSE   = 0x80000007;
static const UInt32  METHOD_COMMENT = 0x7FFFFFFE; // is used to comment "+beg" and "+end" in extra field.
static const UInt32  METHOD_END     = 0xFFFFFFFF;


struct CBlock
{
  UInt32 Type;
  UInt64 UnpPos;
  UInt64 UnpSize;
  UInt64 PackPos;
  UInt64 PackSize;
  
  UInt64 GetNextPackOffset() const { return PackPos + PackSize; }
  UInt64 GetNextUnpPos() const { return UnpPos + UnpSize; }

  bool IsZeroMethod() const { return Type == METHOD_ZERO_0 || Type == METHOD_ZERO_2; }
  bool ThereAreDataInBlock() const { return Type != METHOD_COMMENT && Type != METHOD_END; }
};

static const UInt32 kCheckSumType_CRC = 2;

static const size_t kChecksumSize_Max = 0x80;

struct CChecksum
{
  UInt32 Type;
  UInt32 NumBits;
  Byte Data[kChecksumSize_Max];

  bool IsCrc32() const { return Type == kCheckSumType_CRC && NumBits == 32; }
  UInt32 GetCrc32() const { return Get32(Data); }
  void Parse(const Byte *p);
};

void CChecksum::Parse(const Byte *p)
{
  Type = Get32(p);
  NumBits = Get32(p + 4);
  memcpy(Data, p + 8, kChecksumSize_Max);
};

struct CFile
{
  UInt64 Size;
  UInt64 PackSize;
  UInt64 StartPos;
  AString Name;
  CRecordVector<CBlock> Blocks;
  CChecksum Checksum;
  bool FullFileChecksum;

  HRESULT Parse(const Byte *p, UInt32 size);
};

#ifdef DMG_SHOW_RAW
struct CExtraFile
{
  CByteBuffer Data;
  AString Name;
};
#endif


struct CForkPair
{
  UInt64 Offset;
  UInt64 Len;
  
  void Parse(const Byte *p)
  {
    Offset = Get64(p);
    Len = Get64(p + 8);
  }

  bool UpdateTop(UInt64 limit, UInt64 &top)
  {
    if (Offset > limit || Len > limit - Offset)
      return false;
    UInt64 top2 = Offset + Len;
    if (top <= top2)
      top = top2;
    return true;
  }
};


class CHandler:
  public IInArchive,
  public IInArchiveGetStream,
  public CMyUnknownImp
{
  CMyComPtr<IInStream> _inStream;
  CObjectVector<CFile> _files;
  bool _masterCrcError;
  bool _headersError;

  UInt32 _dataStartOffset;
  UInt64 _startPos;
  UInt64 _phySize;

  AString _name;
  
  #ifdef DMG_SHOW_RAW
  CObjectVector<CExtraFile> _extras;
  #endif

  HRESULT ReadData(IInStream *stream, const CForkPair &pair, CByteBuffer &buf);
  bool ParseBlob(const CByteBuffer &data);
  HRESULT Open2(IInStream *stream);
  HRESULT Extract(IInStream *stream);
public:
  MY_UNKNOWN_IMP2(IInArchive, IInArchiveGetStream)
  INTERFACE_IInArchive(;)
  STDMETHOD(GetStream)(UInt32 index, ISequentialInStream **stream);
};

// that limit can be increased, if there are such dmg files
static const size_t kXmlSizeMax = 0xFFFF0000; // 4 GB - 64 KB;

struct CMethods
{
  CRecordVector<UInt32> Types;
  CRecordVector<UInt32> ChecksumTypes;

  void Update(const CFile &file);
  void GetString(AString &s) const;
};

void CMethods::Update(const CFile &file)
{
  ChecksumTypes.AddToUniqueSorted(file.Checksum.Type);
  FOR_VECTOR (i, file.Blocks)
    Types.AddToUniqueSorted(file.Blocks[i].Type);
}

void CMethods::GetString(AString &res) const
{
  res.Empty();

  unsigned i;
  
  for (i = 0; i < Types.Size(); i++)
  {
    const UInt32 type = Types[i];
    if (type == METHOD_COMMENT || type == METHOD_END)
      continue;
    char buf[16];
    const char *s;
    switch (type)
    {
      case METHOD_ZERO_0: s = "Zero0"; break;
      case METHOD_ZERO_2: s = "Zero2"; break;
      case METHOD_COPY:   s = "Copy";  break;
      case METHOD_ADC:    s = "ADC";   break;
      case METHOD_ZLIB:   s = "ZLIB";  break;
      case METHOD_BZIP2:  s = "BZip2"; break;
      case METHOD_LZFSE:  s = "LZFSE"; break;
      default: ConvertUInt32ToString(type, buf); s = buf;
    }
    res.Add_OptSpaced(s);
  }
  
  for (i = 0; i < ChecksumTypes.Size(); i++)
  {
    res.Add_Space_if_NotEmpty();
    UInt32 type = ChecksumTypes[i];
    switch (type)
    {
      case kCheckSumType_CRC: res += "CRC"; break;
      default:
        res += "Check";
        res.Add_UInt32(type);
    }
  }
}

struct CAppleName
{
  bool IsFs;
  const char *Ext;
  const char *AppleName;
};

static const CAppleName k_Names[] =
{
  { true,  "hfs",  "Apple_HFS" },
  { true,  "hfsx", "Apple_HFSX" },
  { true,  "ufs",  "Apple_UFS" },

  // efi_sys partition is FAT32, but it's not main file. So we use (IsFs = false)
  { false,  "efi_sys", "C12A7328-F81F-11D2-BA4B-00A0C93EC93B" },

  { false, "free", "Apple_Free" },
  { false, "ddm",  "DDM" },
  { false, NULL,   "Apple_partition_map" },
  { false, NULL,   " GPT " },
  { false, NULL,   "MBR" },
  { false, NULL,   "Driver" },
  { false, NULL,   "Patches" }
};
  
static const unsigned kNumAppleNames = ARRAY_SIZE(k_Names);

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidPackSize,
  kpidCRC,
  kpidComment,
  kpidMethod
  // kpidOffset
};

IMP_IInArchive_Props

static const Byte kArcProps[] =
{
  kpidMethod,
  kpidNumBlocks,
  kpidComment
};

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidMethod:
    {
      CMethods m;
      FOR_VECTOR (i, _files)
        m.Update(_files[i]);
      AString s;
      m.GetString(s);
      if (!s.IsEmpty())
        prop = s;
      break;
    }
    case kpidNumBlocks:
    {
      UInt64 numBlocks = 0;
      FOR_VECTOR (i, _files)
        numBlocks += _files[i].Blocks.Size();
      prop = numBlocks;
      break;
    }
    case kpidMainSubfile:
    {
      int mainIndex = -1;
      unsigned numFS = 0;
      unsigned numUnknown = 0;
      FOR_VECTOR (i, _files)
      {
        const AString &name = _files[i].Name;
        unsigned n;
        for (n = 0; n < kNumAppleNames; n++)
        {
          const CAppleName &appleName = k_Names[n];
          // if (name.Find(appleName.AppleName) >= 0)
          if (strstr(name, appleName.AppleName))
          {
            if (appleName.IsFs)
            {
              numFS++;
              mainIndex = i;
            }
            break;
          }
        }
        if (n == kNumAppleNames)
        {
          mainIndex = i;
          numUnknown++;
        }
      }
      if (numFS + numUnknown == 1)
        prop = (UInt32)mainIndex;
      break;
    }
    case kpidWarning:
      if (_masterCrcError)
        prop = "Master CRC error";
      break;

    case kpidWarningFlags:
    {
      UInt32 v = 0;
      if (_headersError) v |= kpv_ErrorFlags_HeadersError;
      if (v != 0)
        prop = v;
      break;
    }

    case kpidOffset: prop = _startPos; break;
    case kpidPhySize: prop = _phySize; break;

    case kpidComment:
      if (!_name.IsEmpty() && _name.Len() < 256)
        prop = _name;
      break;

    case kpidName:
      if (!_name.IsEmpty() && _name.Len() < 256)
      {
        prop = _name + ".dmg";
      }
      break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

IMP_IInArchive_ArcProps

HRESULT CFile::Parse(const Byte *p, UInt32 size)
{
  const UInt32 kHeadSize = 0xCC;
  if (size < kHeadSize)
    return S_FALSE;
  if (Get32(p) != 0x6D697368) // "mish" signature
    return S_FALSE;
  if (Get32(p + 4) != 1) // version
    return S_FALSE;
  // UInt64 firstSectorNumber = Get64(p + 8);
  UInt64 numSectors = Get64(p + 0x10);

  StartPos = Get64(p + 0x18);

  // UInt32 decompressedBufRequested = Get32(p + 0x20); // ???
  // UInt32 blocksDescriptor = Get32(p + 0x24); // number starting from -1?
  // char Reserved1[24];
  
  Checksum.Parse(p + 0x40);
  PRF(printf("\n\nChecksum Type = %2d", Checksum.Type));

  UInt32 numBlocks = Get32(p + 0xC8);
  if (numBlocks > ((UInt32)1 << 28))
    return S_FALSE;

  const UInt32 kRecordSize = 40;
  if (numBlocks * kRecordSize + kHeadSize != size)
    return S_FALSE;

  PackSize = 0;
  Size = 0;
  Blocks.ClearAndReserve(numBlocks);
  FullFileChecksum = true;

  p += kHeadSize;
  UInt32 i;
 
  for (i = 0; i < numBlocks; i++, p += kRecordSize)
  {
    CBlock b;
    b.Type = Get32(p);
    b.UnpPos   = Get64(p + 0x08) << 9;
    b.UnpSize  = Get64(p + 0x10) << 9;
    b.PackPos  = Get64(p + 0x18);
    b.PackSize = Get64(p + 0x20);
    
    // b.PackPos can be 0 for some types. So we don't check it
    if (!Blocks.IsEmpty())
      if (b.UnpPos != Blocks.Back().GetNextUnpPos())
        return S_FALSE;
    
    PRF(printf("\nType=%8x  m[1]=%8x  uPos=%8x  uSize=%7x  pPos=%8x  pSize=%7x",
        b.Type, Get32(p + 4), (UInt32)b.UnpPos, (UInt32)b.UnpSize, (UInt32)b.PackPos, (UInt32)b.PackSize));
    
    if (b.Type == METHOD_COMMENT)
      continue;
    if (b.Type == METHOD_END)
      break;
    PackSize += b.PackSize;
  
    if (b.UnpSize != 0)
    {
      if (b.Type == METHOD_ZERO_2)
        FullFileChecksum = false;
      Blocks.AddInReserved(b);
    }
  }
  
  if (i != numBlocks - 1)
    return S_FALSE;
  if (!Blocks.IsEmpty())
    Size = Blocks.Back().GetNextUnpPos();
  if (Size != (numSectors << 9))
    return S_FALSE;
  
  return S_OK;
}

static int FindKeyPair(const CXmlItem &item, const char *key, const char *nextTag)
{
  for (unsigned i = 0; i + 1 < item.SubItems.Size(); i++)
  {
    const CXmlItem &si = item.SubItems[i];
    if (si.IsTagged("key") && si.GetSubString() == key && item.SubItems[i + 1].IsTagged(nextTag))
      return i + 1;
  }
  return -1;
}

static const AString *GetStringFromKeyPair(const CXmlItem &item, const char *key, const char *nextTag)
{
  int index = FindKeyPair(item, key, nextTag);
  if (index >= 0)
    return item.SubItems[index].GetSubStringPtr();
  return NULL;
}

static const unsigned HEADER_SIZE = 0x200;

static const Byte k_Signature[] = { 'k','o','l','y', 0, 0, 0, 4, 0, 0, 2, 0 };

static inline bool IsKoly(const Byte *p)
{
  return memcmp(p, k_Signature, ARRAY_SIZE(k_Signature)) == 0;
  /*
  if (Get32(p) != 0x6B6F6C79) // "koly" signature
    return false;
  if (Get32(p + 4) != 4) // version
    return false;
  if (Get32(p + 8) != HEADER_SIZE)
    return false;
  return true;
  */
}


HRESULT CHandler::ReadData(IInStream *stream, const CForkPair &pair, CByteBuffer &buf)
{
  size_t size = (size_t)pair.Len;
  if (size != pair.Len)
    return E_OUTOFMEMORY;
  buf.Alloc(size);
  RINOK(stream->Seek(_startPos + pair.Offset, STREAM_SEEK_SET, NULL));
  return ReadStream_FALSE(stream, buf, size);
}


bool CHandler::ParseBlob(const CByteBuffer &data)
{
  if (data.Size() < 12)
    return false;
  const Byte *p = (const Byte *)data;
  if (Get32(p) != 0xFADE0CC0)
    return true;
  const UInt32 size = Get32(p + 4);
  if (size != data.Size())
    return false;
  const UInt32 num = Get32(p + 8);
  if (num > (size - 12) / 8)
    return false;
  
  for (UInt32 i = 0; i < num; i++)
  {
    // UInt32 type = Get32(p + i * 8 + 12);
    UInt32 offset = Get32(p + i * 8 + 12 + 4);
    if (size - offset < 8)
      return false;
    const Byte *p2 = (const Byte *)data + offset;
    const UInt32 magic = Get32(p2);
    const UInt32 len = Get32(p2 + 4);
    if (size - offset < len || len < 8)
      return false;

    #ifdef DMG_SHOW_RAW
    CExtraFile &extra = _extras.AddNew();
    extra.Name = "_blob_";
    extra.Data.CopyFrom(p2, len);
    #endif

    if (magic == 0xFADE0C02)
    {
      #ifdef DMG_SHOW_RAW
      extra.Name += "codedir";
      #endif
    
      if (len < 11 * 4)
        return false;
      UInt32 idOffset = Get32(p2 + 0x14);
      if (idOffset >= len)
        return false;
      UInt32 len2 = len - idOffset;
      if (len2 < (1 << 10))
        _name.SetFrom_CalcLen((const char *)(p2 + idOffset), len2);
    }
    #ifdef DMG_SHOW_RAW
    else if (magic == 0xFADE0C01)
      extra.Name += "requirements";
    else if (magic == 0xFADE0B01)
      extra.Name += "signed";
    else
    {
      char temp[16];
      ConvertUInt32ToHex8Digits(magic, temp);
      extra.Name += temp;
    }
    #endif
  }

  return true;
}


HRESULT CHandler::Open2(IInStream *stream)
{
  /*
  - usual dmg contains Koly Header at the end:
  - rare case dmg contains Koly Header at the start.
  */

  _dataStartOffset = 0;
  RINOK(stream->Seek(0, STREAM_SEEK_CUR, &_startPos));

  UInt64 fileSize = 0;
  RINOK(stream->Seek(0, STREAM_SEEK_END, &fileSize));
  RINOK(stream->Seek(_startPos, STREAM_SEEK_SET, NULL));

  Byte buf[HEADER_SIZE];
  RINOK(ReadStream_FALSE(stream, buf, HEADER_SIZE));

  UInt64 headerPos;
  bool startKolyMode = false;

  if (IsKoly(buf))
  {
    // it can be normal koly-at-the-end or koly-at-the-start
    headerPos = _startPos;
    if (_startPos <= (1 << 8))
    {
      // we want to support startKolyMode, even if there is
      // some data before dmg file, like 128 bytes MacBin header
      _dataStartOffset = HEADER_SIZE;
      startKolyMode = true;
    }
  }
  else
  {
    // we check only koly-at-the-end
    headerPos = fileSize;
    if (headerPos < HEADER_SIZE)
      return S_FALSE;
    headerPos -= HEADER_SIZE;
    RINOK(stream->Seek(headerPos, STREAM_SEEK_SET, NULL));
    RINOK(ReadStream_FALSE(stream, buf, HEADER_SIZE));
    if (!IsKoly(buf))
      return S_FALSE;
  }

  // UInt32 flags = Get32(buf + 12);
  // UInt64 runningDataForkOffset = Get64(buf + 0x10);
  
  CForkPair dataForkPair, rsrcPair, xmlPair, blobPair;
  
  dataForkPair.Parse(buf + 0x18);
  rsrcPair.Parse(buf + 0x28);
  xmlPair.Parse(buf + 0xD8);
  blobPair.Parse(buf + 0x128);

  // UInt32 segmentNumber = Get32(buf + 0x38);
  // UInt32 segmentCount = Get32(buf + 0x3C);
  // Byte segmentGUID[16];
  // CChecksum dataForkChecksum;
  // dataForkChecksum.Parse(buf + 0x50);

  UInt64 top = 0;
  UInt64 limit = startKolyMode ? fileSize : headerPos;

  if (!dataForkPair.UpdateTop(limit, top)) return S_FALSE;
  if (!xmlPair.UpdateTop(limit, top)) return S_FALSE;
  if (!rsrcPair.UpdateTop(limit, top)) return S_FALSE;

  /* Some old dmg files contain garbage data in blobPair field.
     So we need to ignore such garbage case;
     And we still need to detect offset of start of archive for "parser" mode. */

  bool useBlob = blobPair.UpdateTop(limit, top);


  if (startKolyMode)
    _phySize = top;
  else
  {
    _phySize = headerPos + HEADER_SIZE;
    _startPos = 0;
    if (top != headerPos)
    {
      /*
      if expected absolute offset is not equal to real header offset,
      2 cases are possible:
        - additional (unknown) headers
        - archive with offset.
       So we try to read XML with absolute offset to select from these two ways.
      */
    CForkPair xmlPair2 = xmlPair;
    const char *sz = "<?xml version";
    const unsigned len = (unsigned)strlen(sz);
    if (xmlPair2.Len > len)
      xmlPair2.Len = len;
    CByteBuffer buf2;
    if (xmlPair2.Len < len
        || ReadData(stream, xmlPair2, buf2) != S_OK
        || memcmp(buf2, sz, len) != 0)
    {
      // if absolute offset is not OK, probably it's archive with offset
      _startPos = headerPos - top;
      _phySize = top + HEADER_SIZE;
    }
    }
  }

  // Byte reserved[0x78]

  if (useBlob && blobPair.Len != 0)
  {
    #ifdef DMG_SHOW_RAW
    CExtraFile &extra = _extras.AddNew();
    extra.Name = "_blob.bin";
    CByteBuffer &blobBuf = extra.Data;
    #else
    CByteBuffer blobBuf;
    #endif
    RINOK(ReadData(stream, blobPair, blobBuf));
    if (!ParseBlob(blobBuf))
      _headersError = true;
  }


  CChecksum masterChecksum;
  masterChecksum.Parse(buf + 0x160);

  // UInt32 imageVariant = Get32(buf + 0x1E8);
  // UInt64 numSectors = Get64(buf + 0x1EC);
  // Byte reserved[0x12]

  const UInt32 RSRC_HEAD_SIZE = 0x100;

  // We don't know the size of the field "offset" in rsrc.
  // We suppose that it uses 24 bits. So we use Rsrc, only if the rsrcLen < (1 << 24).
  bool useRsrc = (rsrcPair.Len > RSRC_HEAD_SIZE && rsrcPair.Len < ((UInt32)1 << 24));
  // useRsrc = false;

  if (useRsrc)
  {
    #ifdef DMG_SHOW_RAW
    CExtraFile &extra = _extras.AddNew();
    extra.Name = "rsrc.bin";
    CByteBuffer &rsrcBuf = extra.Data;
    #else
    CByteBuffer rsrcBuf;
    #endif

    RINOK(ReadData(stream, rsrcPair, rsrcBuf));

    const Byte *p = rsrcBuf;
    UInt32 headSize = Get32(p + 0);
    UInt32 footerOffset = Get32(p + 4);
    UInt32 mainDataSize = Get32(p + 8);
    UInt32 footerSize = Get32(p + 12);
    if (headSize != RSRC_HEAD_SIZE
        || footerOffset >= rsrcPair.Len
        || mainDataSize >= rsrcPair.Len
        || footerOffset < mainDataSize
        || footerOffset != headSize + mainDataSize)
      return S_FALSE;

    const UInt32 footerEnd = footerOffset + footerSize;
    if (footerEnd != rsrcPair.Len)
    {
      // there is rare case dmg example, where there are 4 additional bytes
      UInt64 rem = rsrcPair.Len - footerOffset;
      if (rem < footerSize
          || rem - footerSize != 4
          || Get32(p + footerEnd) != 0)
        return S_FALSE;
    }
  
    if (footerSize < 16)
      return S_FALSE;
    if (memcmp(p, p + footerOffset, 16) != 0)
      return S_FALSE;

    p += footerOffset;

    if ((UInt32)Get16(p + 0x18) != 0x1C)
      return S_FALSE;
    const UInt32 namesOffset = Get16(p + 0x1A);
    if (namesOffset > footerSize)
      return S_FALSE;
    
    UInt32 numItems = (UInt32)Get16(p + 0x1C) + 1;
    if (numItems * 8 + 0x1E > namesOffset)
      return S_FALSE;
    
    for (UInt32 i = 0; i < numItems; i++)
    {
      const Byte *p2 = p + 0x1E + i * 8;
    
      const UInt32 typeId = Get32(p2);
      
      #ifndef DMG_SHOW_RAW
      if (typeId != 0x626C6B78) // blkx
        continue;
      #endif
      
      const UInt32 numFiles = (UInt32)Get16(p2 + 4) + 1;
      const UInt32 offs = Get16(p2 + 6);
      if (0x1C + offs + 12 * numFiles > namesOffset)
        return S_FALSE;

      for (UInt32 k = 0; k < numFiles; k++)
      {
        const Byte *p3 = p + 0x1C + offs + k * 12;
        // UInt32 id = Get16(p3);
        const UInt32 namePos = Get16(p3 + 2);
        // Byte attributes = p3[4]; // = 0x50 for blkx
        // we don't know how many bits we can use. So we use 24 bits only
        UInt32 blockOffset = Get32(p3 + 4);
        blockOffset &= (((UInt32)1 << 24) - 1);
        // UInt32 unknown2 = Get32(p3 + 8); // ???
        if (blockOffset + 4 >= mainDataSize)
          return S_FALSE;
        const Byte *pBlock = rsrcBuf + headSize + blockOffset;
        const UInt32 blockSize = Get32(pBlock);
        if (mainDataSize - (blockOffset + 4) < blockSize)
          return S_FALSE;
        
        AString name;
        
        if (namePos != 0xFFFF)
        {
          UInt32 namesBlockSize = footerSize - namesOffset;
          if (namePos >= namesBlockSize)
            return S_FALSE;
          const Byte *namePtr = p + namesOffset + namePos;
          UInt32 nameLen = *namePtr;
          if (namesBlockSize - namePos <= nameLen)
            return S_FALSE;
          for (UInt32 r = 1; r <= nameLen; r++)
          {
            Byte c = namePtr[r];
            if (c < 0x20 || c >= 0x80)
              break;
            name += (char)c;
          }
        }
        
        if (typeId == 0x626C6B78) // blkx
        {
          CFile &file = _files.AddNew();
          file.Name = name;
          RINOK(file.Parse(pBlock + 4, blockSize));
        }
        
        #ifdef DMG_SHOW_RAW
        {
          AString name2;

          name2.Add_UInt32(i);
          name2 += '_';

          {
            char temp[4 + 1] = { 0 };
            memcpy(temp, p2, 4);
            name2 += temp;
          }
          name2.Trim();
          name2 += '_';
          name2.Add_UInt32(k);
          
          if (!name.IsEmpty())
          {
            name2 += '_';
            name2 += name;
          }
          
          CExtraFile &extra = _extras.AddNew();
          extra.Name = name2;
          extra.Data.CopyFrom(pBlock + 4, blockSize);
        }
        #endif
      }
    }
  }
  else
  {
    if (xmlPair.Len >= kXmlSizeMax || xmlPair.Len == 0)
      return S_FALSE;
    size_t size = (size_t)xmlPair.Len;
    if (size != xmlPair.Len)
      return S_FALSE;

    RINOK(stream->Seek(_startPos + xmlPair.Offset, STREAM_SEEK_SET, NULL));
    
    CXml xml;
    {
      CObjArray<char> xmlStr(size + 1);
      RINOK(ReadStream_FALSE(stream, xmlStr, size));
      xmlStr[size] = 0;
      // if (strlen(xmlStr) != size) return S_FALSE;
      if (!xml.Parse(xmlStr))
        return S_FALSE;

      #ifdef DMG_SHOW_RAW
      CExtraFile &extra = _extras.AddNew();
      extra.Name = "a.xml";
      extra.Data.CopyFrom((const Byte *)(const char *)xmlStr, size);
      #endif
    }
    
    if (xml.Root.Name != "plist")
      return S_FALSE;
    
    int dictIndex = xml.Root.FindSubTag("dict");
    if (dictIndex < 0)
      return S_FALSE;
    
    const CXmlItem &dictItem = xml.Root.SubItems[dictIndex];
    int rfDictIndex = FindKeyPair(dictItem, "resource-fork", "dict");
    if (rfDictIndex < 0)
      return S_FALSE;
    
    const CXmlItem &rfDictItem = dictItem.SubItems[rfDictIndex];
    int arrIndex = FindKeyPair(rfDictItem, "blkx", "array");
    if (arrIndex < 0)
      return S_FALSE;
    
    const CXmlItem &arrItem = rfDictItem.SubItems[arrIndex];
    
    FOR_VECTOR (i, arrItem.SubItems)
    {
      const CXmlItem &item = arrItem.SubItems[i];
      if (!item.IsTagged("dict"))
        continue;
      
      CByteBuffer rawBuf;
      unsigned destLen = 0;
      {
        const AString *dataString = GetStringFromKeyPair(item, "Data", "data");
        if (!dataString)
          return S_FALSE;
        destLen = dataString->Len() / 4 * 3 + 4;
        rawBuf.Alloc(destLen);
        {
          const Byte *endPtr = Base64ToBin(rawBuf, *dataString);
          if (!endPtr)
            return S_FALSE;
          destLen = (unsigned)(endPtr - (const Byte *)rawBuf);
        }
        
        #ifdef DMG_SHOW_RAW
        CExtraFile &extra = _extras.AddNew();
        extra.Name.Add_UInt32(_files.Size());
        extra.Data.CopyFrom(rawBuf, destLen);
        #endif
      }
      CFile &file = _files.AddNew();
      {
        const AString *name = GetStringFromKeyPair(item, "Name", "string");
        if (!name || name->IsEmpty())
          name = GetStringFromKeyPair(item, "CFName", "string");
        if (name)
          file.Name = *name;
      }
      RINOK(file.Parse(rawBuf, destLen));
    }
  }

  if (masterChecksum.IsCrc32())
  {
    UInt32 crc = CRC_INIT_VAL;
    unsigned i;
    for (i = 0; i < _files.Size(); i++)
    {
      const CChecksum &cs = _files[i].Checksum;
      if ((cs.NumBits & 0x7) != 0)
        break;
      UInt32 len = cs.NumBits >> 3;
      if (len > kChecksumSize_Max)
        break;
      crc = CrcUpdate(crc, cs.Data, (size_t)len);
    }
    if (i == _files.Size())
      _masterCrcError = (CRC_GET_DIGEST(crc) != masterChecksum.GetCrc32());
  }

  return S_OK;
}

STDMETHODIMP CHandler::Open(IInStream *stream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback * /* openArchiveCallback */)
{
  COM_TRY_BEGIN
  {
    Close();
    if (Open2(stream) != S_OK)
      return S_FALSE;
    _inStream = stream;
  }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::Close()
{
  _phySize = 0;
  _inStream.Release();
  _files.Clear();
  _masterCrcError = false;
  _headersError = false;
  _name.Empty();
  #ifdef DMG_SHOW_RAW
  _extras.Clear();
  #endif
  return S_OK;
}

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = _files.Size()
    #ifdef DMG_SHOW_RAW
    + _extras.Size()
    #endif
  ;
  return S_OK;
}

#ifdef DMG_SHOW_RAW
#define RAW_PREFIX "raw" STRING_PATH_SEPARATOR
#endif

STDMETHODIMP CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  
  #ifdef DMG_SHOW_RAW
  if (index >= _files.Size())
  {
    const CExtraFile &extra = _extras[index - _files.Size()];
    switch (propID)
    {
      case kpidPath:
        prop = (AString)RAW_PREFIX + extra.Name;
        break;
      case kpidSize:
      case kpidPackSize:
        prop = (UInt64)extra.Data.Size();
        break;
    }
  }
  else
  #endif
  {
    const CFile &item = _files[index];
    switch (propID)
    {
      case kpidSize:  prop = item.Size; break;
      case kpidPackSize:  prop = item.PackSize; break;
      case kpidCRC:
      {
        if (item.Checksum.IsCrc32() && item.FullFileChecksum)
          prop = item.Checksum.GetCrc32();
        break;
      }

      /*
      case kpidOffset:
      {
        prop = item.StartPos;
        break;
      }
      */

      case kpidMethod:
      {
        CMethods m;
        m.Update(item);
        AString s;
        m.GetString(s);
        if (!s.IsEmpty())
          prop = s;
        break;
      }
      
      case kpidPath:
      {
        UString name;
        name.Add_UInt32(index);
        unsigned num = 10;
        unsigned numDigits;
        for (numDigits = 1; num < _files.Size(); numDigits++)
          num *= 10;
        while (name.Len() < numDigits)
          name.InsertAtFront(L'0');

        AString subName;
        int pos1 = item.Name.Find('(');
        if (pos1 >= 0)
        {
          pos1++;
          int pos2 = item.Name.Find(')', pos1);
          if (pos2 >= 0)
          {
            subName.SetFrom(item.Name.Ptr(pos1), pos2 - pos1);
            pos1 = subName.Find(':');
            if (pos1 >= 0)
              subName.DeleteFrom(pos1);
          }
        }
        subName.Trim();
        if (!subName.IsEmpty())
        {
          for (unsigned n = 0; n < kNumAppleNames; n++)
          {
            const CAppleName &appleName = k_Names[n];
            if (appleName.Ext)
            {
              if (subName == appleName.AppleName)
              {
                subName = appleName.Ext;
                break;
              }
            }
          }
          UString name2;
          ConvertUTF8ToUnicode(subName, name2);
          name += '.';
          name += name2;
        }
        else
        {
          UString name2;
          ConvertUTF8ToUnicode(item.Name, name2);
          if (!name2.IsEmpty())
            name += "_";
          name += name2;
        }
        prop = name;
        break;
      }
      
      case kpidComment:
      {
        UString name;
        ConvertUTF8ToUnicode(item.Name, name);
        prop = name;
        break;
      }
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

class CAdcDecoder:
  public ICompressCoder,
  public CMyUnknownImp
{
  CLzOutWindow m_OutWindowStream;
  CInBuffer m_InStream;

  /*
  void ReleaseStreams()
  {
    m_OutWindowStream.ReleaseStream();
    m_InStream.ReleaseStream();
  }
  */

  class CCoderReleaser
  {
    CAdcDecoder *m_Coder;
  public:
    bool NeedFlush;
    CCoderReleaser(CAdcDecoder *coder): m_Coder(coder), NeedFlush(true) {}
    ~CCoderReleaser()
    {
      if (NeedFlush)
        m_Coder->m_OutWindowStream.Flush();
      // m_Coder->ReleaseStreams();
    }
  };
  friend class CCoderReleaser;

public:
  MY_UNKNOWN_IMP

  STDMETHOD(CodeReal)(ISequentialInStream *inStream,
      ISequentialOutStream *outStream, const UInt64 *inSize, const UInt64 *outSize,
      ICompressProgressInfo *progress);

  STDMETHOD(Code)(ISequentialInStream *inStream,
      ISequentialOutStream *outStream, const UInt64 *inSize, const UInt64 *outSize,
      ICompressProgressInfo *progress);
};

STDMETHODIMP CAdcDecoder::CodeReal(ISequentialInStream *inStream,
    ISequentialOutStream *outStream, const UInt64 *inSize, const UInt64 *outSize,
    ICompressProgressInfo *progress)
{
  if (!m_OutWindowStream.Create(1 << 18))
    return E_OUTOFMEMORY;
  if (!m_InStream.Create(1 << 18))
    return E_OUTOFMEMORY;

  m_OutWindowStream.SetStream(outStream);
  m_OutWindowStream.Init(false);
  m_InStream.SetStream(inStream);
  m_InStream.Init();
  
  CCoderReleaser coderReleaser(this);

  const UInt32 kStep = (1 << 20);
  UInt64 nextLimit = kStep;

  UInt64 pos = 0;
  while (pos < *outSize)
  {
    if (pos > nextLimit && progress)
    {
      UInt64 packSize = m_InStream.GetProcessedSize();
      RINOK(progress->SetRatioInfo(&packSize, &pos));
      nextLimit += kStep;
    }
    Byte b;
    if (!m_InStream.ReadByte(b))
      return S_FALSE;
    UInt64 rem = *outSize - pos;
    if (b & 0x80)
    {
      unsigned num = (b & 0x7F) + 1;
      if (num > rem)
        return S_FALSE;
      for (unsigned i = 0; i < num; i++)
      {
        if (!m_InStream.ReadByte(b))
          return S_FALSE;
        m_OutWindowStream.PutByte(b);
      }
      pos += num;
      continue;
    }
    Byte b1;
    if (!m_InStream.ReadByte(b1))
      return S_FALSE;

    UInt32 len, distance;

    if (b & 0x40)
    {
      len = ((UInt32)b & 0x3F) + 4;
      Byte b2;
      if (!m_InStream.ReadByte(b2))
        return S_FALSE;
      distance = ((UInt32)b1 << 8) + b2;
    }
    else
    {
      b &= 0x3F;
      len = ((UInt32)b >> 2) + 3;
      distance = (((UInt32)b & 3) << 8) + b1;
    }

    if (distance >= pos || len > rem)
      return S_FALSE;
    m_OutWindowStream.CopyBlock(distance, len);
    pos += len;
  }
  if (*inSize != m_InStream.GetProcessedSize())
    return S_FALSE;
  coderReleaser.NeedFlush = false;
  return m_OutWindowStream.Flush();
}

STDMETHODIMP CAdcDecoder::Code(ISequentialInStream *inStream,
    ISequentialOutStream *outStream, const UInt64 *inSize, const UInt64 *outSize,
    ICompressProgressInfo *progress)
{
  try { return CodeReal(inStream, outStream, inSize, outSize, progress);}
  catch(const CInBufferException &e) { return e.ErrorCode; }
  catch(const CLzOutWindowException &e) { return e.ErrorCode; }
  catch(...) { return S_FALSE; }
}







STDMETHODIMP CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback)
{
  COM_TRY_BEGIN
  bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _files.Size();
  if (numItems == 0)
    return S_OK;
  UInt64 totalSize = 0;
  UInt32 i;
  
  for (i = 0; i < numItems; i++)
  {
    UInt32 index = (allFilesMode ? i : indices[i]);
    #ifdef DMG_SHOW_RAW
    if (index >= _files.Size())
      totalSize += _extras[index - _files.Size()].Data.Size();
    else
    #endif
      totalSize += _files[index].Size;
  }
  extractCallback->SetTotal(totalSize);

  UInt64 currentPackTotal = 0;
  UInt64 currentUnpTotal = 0;
  UInt64 currentPackSize = 0;
  UInt64 currentUnpSize = 0;

  const UInt32 kZeroBufSize = (1 << 14);
  CByteBuffer zeroBuf(kZeroBufSize);
  memset(zeroBuf, 0, kZeroBufSize);
  
  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder();
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  NCompress::NBZip2::CDecoder *bzip2CoderSpec = new NCompress::NBZip2::CDecoder();
  CMyComPtr<ICompressCoder> bzip2Coder = bzip2CoderSpec;

  NCompress::NZlib::CDecoder *zlibCoderSpec = new NCompress::NZlib::CDecoder();
  CMyComPtr<ICompressCoder> zlibCoder = zlibCoderSpec;

  CAdcDecoder *adcCoderSpec = new CAdcDecoder();
  CMyComPtr<ICompressCoder> adcCoder = adcCoderSpec;

  NCompress::NLzfse::CDecoder *lzfseCoderSpec = new NCompress::NLzfse::CDecoder();
  CMyComPtr<ICompressCoder> lzfseCoder = lzfseCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  CLimitedSequentialInStream *streamSpec = new CLimitedSequentialInStream;
  CMyComPtr<ISequentialInStream> inStream(streamSpec);
  streamSpec->SetStream(_inStream);

  for (i = 0; i < numItems; i++, currentPackTotal += currentPackSize, currentUnpTotal += currentUnpSize)
  {
    lps->InSize = currentPackTotal;
    lps->OutSize = currentUnpTotal;
    currentPackSize = 0;
    currentUnpSize = 0;
    RINOK(lps->SetCur());
    CMyComPtr<ISequentialOutStream> realOutStream;
    Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    UInt32 index = allFilesMode ? i : indices[i];
    RINOK(extractCallback->GetStream(index, &realOutStream, askMode));

    if (!testMode && !realOutStream)
      continue;
    RINOK(extractCallback->PrepareOperation(askMode));


    COutStreamWithCRC *outCrcStreamSpec = new COutStreamWithCRC;
    CMyComPtr<ISequentialOutStream> outCrcStream = outCrcStreamSpec;
    outCrcStreamSpec->SetStream(realOutStream);
    bool needCrc = false;
    outCrcStreamSpec->Init(needCrc);

    CLimitedSequentialOutStream *outStreamSpec = new CLimitedSequentialOutStream;
    CMyComPtr<ISequentialOutStream> outStream(outStreamSpec);
    outStreamSpec->SetStream(outCrcStream);
    
    realOutStream.Release();

    Int32 opRes = NExtract::NOperationResult::kOK;
    #ifdef DMG_SHOW_RAW
    if (index >= _files.Size())
    {
      const CByteBuffer &buf = _extras[index - _files.Size()].Data;
      outStreamSpec->Init(buf.Size());
      RINOK(WriteStream(outStream, buf, buf.Size()));
      currentPackSize = currentUnpSize = buf.Size();
    }
    else
    #endif
    {
      const CFile &item = _files[index];
      currentPackSize = item.PackSize;
      currentUnpSize = item.Size;

      needCrc = item.Checksum.IsCrc32();

      UInt64 unpPos = 0;
      UInt64 packPos = 0;
      {
        FOR_VECTOR (j, item.Blocks)
        {
          lps->InSize = currentPackTotal + packPos;
          lps->OutSize = currentUnpTotal + unpPos;
          RINOK(lps->SetCur());

          const CBlock &block = item.Blocks[j];
          if (!block.ThereAreDataInBlock())
            continue;

          packPos += block.PackSize;
          if (block.UnpPos != unpPos)
          {
            opRes = NExtract::NOperationResult::kDataError;
            break;
          }

          RINOK(_inStream->Seek(_startPos + _dataStartOffset + item.StartPos + block.PackPos, STREAM_SEEK_SET, NULL));
          streamSpec->Init(block.PackSize);
          bool realMethod = true;
          outStreamSpec->Init(block.UnpSize);
          HRESULT res = S_OK;

          outCrcStreamSpec->EnableCalc(needCrc);

          switch (block.Type)
          {
            case METHOD_ZERO_0:
            case METHOD_ZERO_2:
              realMethod = false;
              if (block.PackSize != 0)
                opRes = NExtract::NOperationResult::kUnsupportedMethod;
              outCrcStreamSpec->EnableCalc(block.Type == METHOD_ZERO_0);
              break;

            case METHOD_COPY:
              if (block.UnpSize != block.PackSize)
              {
                opRes = NExtract::NOperationResult::kUnsupportedMethod;
                break;
              }
              res = copyCoder->Code(inStream, outStream, NULL, NULL, progress);
              break;
            
            case METHOD_ADC:
            {
              res = adcCoder->Code(inStream, outStream, &block.PackSize, &block.UnpSize, progress);
              break;
            }
            
            case METHOD_ZLIB:
            {
              res = zlibCoder->Code(inStream, outStream, NULL, NULL, progress);
              if (res == S_OK)
                if (zlibCoderSpec->GetInputProcessedSize() != block.PackSize)
                  opRes = NExtract::NOperationResult::kDataError;
              break;
            }

            case METHOD_BZIP2:
            {
              res = bzip2Coder->Code(inStream, outStream, NULL, NULL, progress);
              if (res == S_OK)
                if (bzip2CoderSpec->GetInputProcessedSize() != block.PackSize)
                  opRes = NExtract::NOperationResult::kDataError;
              break;
            }

            case METHOD_LZFSE:
            {
              res = lzfseCoder->Code(inStream, outStream, &block.PackSize, &block.UnpSize, progress);
              break;
            }
            
            default:
              opRes = NExtract::NOperationResult::kUnsupportedMethod;
              break;
          }

          if (res != S_OK)
          {
            if (res != S_FALSE)
              return res;
            if (opRes == NExtract::NOperationResult::kOK)
              opRes = NExtract::NOperationResult::kDataError;
          }
          
          unpPos += block.UnpSize;
          
          if (!outStreamSpec->IsFinishedOK())
          {
            if (realMethod && opRes == NExtract::NOperationResult::kOK)
              opRes = NExtract::NOperationResult::kDataError;

            while (outStreamSpec->GetRem() != 0)
            {
              UInt64 rem = outStreamSpec->GetRem();
              UInt32 size = (UInt32)MyMin(rem, (UInt64)kZeroBufSize);
              RINOK(WriteStream(outStream, zeroBuf, size));
            }
          }
        }
      }
  
      if (needCrc && opRes == NExtract::NOperationResult::kOK)
      {
        if (outCrcStreamSpec->GetCRC() != item.Checksum.GetCrc32())
          opRes = NExtract::NOperationResult::kCRCError;
      }
    }
    outStream.Release();
    RINOK(extractCallback->SetOperationResult(opRes));
  }

  return S_OK;
  COM_TRY_END
}

struct CChunk
{
  int BlockIndex;
  UInt64 AccessMark;
  CByteBuffer Buf;
};

class CInStream:
  public IInStream,
  public CMyUnknownImp
{
  UInt64 _virtPos;
  int _latestChunk;
  int _latestBlock;
  UInt64 _accessMark;
  CObjectVector<CChunk> _chunks;

  NCompress::NBZip2::CDecoder *bzip2CoderSpec;
  CMyComPtr<ICompressCoder> bzip2Coder;

  NCompress::NZlib::CDecoder *zlibCoderSpec;
  CMyComPtr<ICompressCoder> zlibCoder;

  CAdcDecoder *adcCoderSpec;
  CMyComPtr<ICompressCoder> adcCoder;

  NCompress::NLzfse::CDecoder *lzfseCoderSpec;
  CMyComPtr<ICompressCoder> lzfseCoder;

  CBufPtrSeqOutStream *outStreamSpec;
  CMyComPtr<ISequentialOutStream> outStream;

  CLimitedSequentialInStream *limitedStreamSpec;
  CMyComPtr<ISequentialInStream> inStream;

public:
  CMyComPtr<IInStream> Stream;
  UInt64 Size;
  const CFile *File;
  UInt64 _startPos;

  HRESULT InitAndSeek(UInt64 startPos)
  {
    _startPos = startPos;
    _virtPos = 0;
    _latestChunk = -1;
    _latestBlock = -1;
    _accessMark = 0;

    limitedStreamSpec = new CLimitedSequentialInStream;
    inStream = limitedStreamSpec;
    limitedStreamSpec->SetStream(Stream);

    outStreamSpec = new CBufPtrSeqOutStream;
    outStream = outStreamSpec;
    return S_OK;
  }

  MY_UNKNOWN_IMP1(IInStream)

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);
};


static unsigned FindBlock(const CRecordVector<CBlock> &blocks, UInt64 pos)
{
  unsigned left = 0, right = blocks.Size();
  for (;;)
  {
    unsigned mid = (left + right) / 2;
    if (mid == left)
      return left;
    if (pos < blocks[mid].UnpPos)
      right = mid;
    else
      left = mid;
  }
}

STDMETHODIMP CInStream::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  COM_TRY_BEGIN

  if (processedSize)
    *processedSize = 0;
  if (size == 0)
    return S_OK;
  if (_virtPos >= Size)
    return S_OK; // (Size == _virtPos) ? S_OK: E_FAIL;
  {
    UInt64 rem = Size - _virtPos;
    if (size > rem)
      size = (UInt32)rem;
  }

  if (_latestBlock >= 0)
  {
    const CBlock &block = File->Blocks[_latestBlock];
    if (_virtPos < block.UnpPos || (_virtPos - block.UnpPos) >= block.UnpSize)
      _latestBlock = -1;
  }
  
  if (_latestBlock < 0)
  {
    _latestChunk = -1;
    unsigned blockIndex = FindBlock(File->Blocks, _virtPos);
    const CBlock &block = File->Blocks[blockIndex];
    
    if (!block.IsZeroMethod() && block.Type != METHOD_COPY)
    {
      unsigned i;
      for (i = 0; i < _chunks.Size(); i++)
        if (_chunks[i].BlockIndex == (int)blockIndex)
          break;
      
      if (i != _chunks.Size())
        _latestChunk = i;
      else
      {
        const unsigned kNumChunksMax = 128;
        unsigned chunkIndex;
      
        if (_chunks.Size() != kNumChunksMax)
          chunkIndex = _chunks.Add(CChunk());
        else
        {
          chunkIndex = 0;
          for (i = 0; i < _chunks.Size(); i++)
            if (_chunks[i].AccessMark < _chunks[chunkIndex].AccessMark)
              chunkIndex = i;
        }
        
        CChunk &chunk = _chunks[chunkIndex];
        chunk.BlockIndex = -1;
        chunk.AccessMark = 0;
        
        if (chunk.Buf.Size() < block.UnpSize)
        {
          chunk.Buf.Free();
          if (block.UnpSize > ((UInt32)1 << 31))
            return E_FAIL;
          chunk.Buf.Alloc((size_t)block.UnpSize);
        }
        
        outStreamSpec->Init(chunk.Buf, (size_t)block.UnpSize);
          
        RINOK(Stream->Seek(_startPos + File->StartPos + block.PackPos, STREAM_SEEK_SET, NULL));

        limitedStreamSpec->Init(block.PackSize);
        HRESULT res = S_OK;
        
        switch (block.Type)
        {
          case METHOD_COPY:
            if (block.PackSize != block.UnpSize)
              return E_FAIL;
            res = ReadStream_FAIL(inStream, chunk.Buf, (size_t)block.UnpSize);
            break;
            
          case METHOD_ADC:
            if (!adcCoder)
            {
              adcCoderSpec = new CAdcDecoder();
              adcCoder = adcCoderSpec;
            }
            res = adcCoder->Code(inStream, outStream, &block.PackSize, &block.UnpSize, NULL);
            break;
            
          case METHOD_ZLIB:
            if (!zlibCoder)
            {
              zlibCoderSpec = new NCompress::NZlib::CDecoder();
              zlibCoder = zlibCoderSpec;
            }
            res = zlibCoder->Code(inStream, outStream, NULL, NULL, NULL);
            if (res == S_OK && zlibCoderSpec->GetInputProcessedSize() != block.PackSize)
              res = S_FALSE;
            break;
            
          case METHOD_BZIP2:
            if (!bzip2Coder)
            {
              bzip2CoderSpec = new NCompress::NBZip2::CDecoder();
              bzip2Coder = bzip2CoderSpec;
            }
            res = bzip2Coder->Code(inStream, outStream, NULL, NULL, NULL);
            if (res == S_OK && bzip2CoderSpec->GetInputProcessedSize() != block.PackSize)
              res = S_FALSE;
            break;

          case METHOD_LZFSE:
            if (!lzfseCoder)
            {
              lzfseCoderSpec = new NCompress::NLzfse::CDecoder();
              lzfseCoder = lzfseCoderSpec;
            }
            res = lzfseCoder->Code(inStream, outStream, &block.PackSize, &block.UnpSize, NULL);
            break;
            
          default:
            return E_FAIL;
        }
        
        if (res != S_OK)
          return res;
        if (block.Type != METHOD_COPY && outStreamSpec->GetPos() != block.UnpSize)
          return E_FAIL;
        chunk.BlockIndex = blockIndex;
        _latestChunk = chunkIndex;
      }
      
      _chunks[_latestChunk].AccessMark = _accessMark++;
    }
  
    _latestBlock = blockIndex;
  }

  const CBlock &block = File->Blocks[_latestBlock];
  const UInt64 offset = _virtPos - block.UnpPos;
  const UInt64 rem = block.UnpSize - offset;
  if (size > rem)
    size = (UInt32)rem;
  
  HRESULT res = S_OK;
  
  if (block.Type == METHOD_COPY)
  {
    RINOK(Stream->Seek(_startPos + File->StartPos + block.PackPos + offset, STREAM_SEEK_SET, NULL));
    res = Stream->Read(data, size, &size);
  }
  else if (block.IsZeroMethod())
    memset(data, 0, size);
  else if (size != 0)
    memcpy(data, _chunks[_latestChunk].Buf + (size_t)offset, size);
  
  _virtPos += size;
  if (processedSize)
    *processedSize = size;
  
  return res;
  COM_TRY_END
}
 
STDMETHODIMP CInStream::Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition)
{
  switch (seekOrigin)
  {
    case STREAM_SEEK_SET: break;
    case STREAM_SEEK_CUR: offset += _virtPos; break;
    case STREAM_SEEK_END: offset += Size; break;
    default: return STG_E_INVALIDFUNCTION;
  }
  if (offset < 0)
    return HRESULT_WIN32_ERROR_NEGATIVE_SEEK;
  _virtPos = offset;
  if (newPosition)
    *newPosition = offset;
  return S_OK;
}

STDMETHODIMP CHandler::GetStream(UInt32 index, ISequentialInStream **stream)
{
  COM_TRY_BEGIN
  
  #ifdef DMG_SHOW_RAW
  if (index >= (UInt32)_files.Size())
    return S_FALSE;
  #endif
  
  CInStream *spec = new CInStream;
  CMyComPtr<ISequentialInStream> specStream = spec;
  spec->File = &_files[index];
  const CFile &file = *spec->File;
  
  FOR_VECTOR (i, file.Blocks)
  {
    const CBlock &block = file.Blocks[i];
    switch (block.Type)
    {
      case METHOD_ZERO_0:
      case METHOD_ZERO_2:
      case METHOD_COPY:
      case METHOD_ADC:
      case METHOD_ZLIB:
      case METHOD_BZIP2:
      case METHOD_LZFSE:
      case METHOD_END:
        break;
      default:
        return S_FALSE;
    }
  }
  
  spec->Stream = _inStream;
  spec->Size = spec->File->Size;
  RINOK(spec->InitAndSeek(_startPos + _dataStartOffset));
  *stream = specStream.Detach();
  return S_OK;
  
  COM_TRY_END
}

REGISTER_ARC_I(
  "Dmg", "dmg", 0, 0xE4,
  k_Signature,
  0,
  NArcInfoFlags::kBackwardOpen |
  NArcInfoFlags::kUseGlobalOffset,
  NULL)

}}

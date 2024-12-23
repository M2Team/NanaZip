// XarHandler.cpp

#include "StdAfx.h"

#include "../../../C/Sha256.h"
#include "../../../C/Sha512.h"
#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/MyLinux.h"
#include "../../Common/MyXml.h"
#include "../../Common/StringToInt.h"
#include "../../Common/UTFConvert.h"

#include "../../Windows/PropVariant.h"
#include "../../Windows/TimeUtils.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#include "../Compress/BZip2Decoder.h"
#include "../Compress/CopyCoder.h"
#include "../Compress/ZlibDecoder.h"

#include "Common/OutStreamWithSha1.h"

using namespace NWindows;

#define XAR_SHOW_RAW

#define Get16(p) GetBe16(p)
#define Get32(p) GetBe32(p)
#define Get64(p) GetBe64(p)

namespace NArchive {
namespace NXar {

Z7_CLASS_IMP_NOQIB_1(
  CInStreamWithSha256
  , ISequentialInStream
)
  bool _sha512Mode;
  CMyComPtr<ISequentialInStream> _stream;
  CAlignedBuffer1 _sha256;
  CAlignedBuffer1 _sha512;
  UInt64 _size;

  CSha256 *Sha256() { return (CSha256 *)(void *)(Byte *)_sha256; }
  CSha512 *Sha512() { return (CSha512 *)(void *)(Byte *)_sha512; }
public:
  CInStreamWithSha256():
      _sha256(sizeof(CSha256)),
      _sha512(sizeof(CSha512))
      {}
  void SetStream(ISequentialInStream *stream) { _stream = stream;  }
  void Init(bool sha512Mode)
  {
    _sha512Mode = sha512Mode;
    _size = 0;
    if (sha512Mode)
      Sha512_Init(Sha512(), SHA512_DIGEST_SIZE);
    else
      Sha256_Init(Sha256());
  }
  void ReleaseStream() { _stream.Release(); }
  UInt64 GetSize() const { return _size; }
  void Final256(Byte *digest) { Sha256_Final(Sha256(), digest); }
  void Final512(Byte *digest) { Sha512_Final(Sha512(), digest, SHA512_DIGEST_SIZE); }
};

Z7_COM7F_IMF(CInStreamWithSha256::Read(void *data, UInt32 size, UInt32 *processedSize))
{
  UInt32 realProcessedSize;
  const HRESULT result = _stream->Read(data, size, &realProcessedSize);
  _size += realProcessedSize;
  if (_sha512Mode)
    Sha512_Update(Sha512(), (const Byte *)data, realProcessedSize);
  else
    Sha256_Update(Sha256(), (const Byte *)data, realProcessedSize);
  if (processedSize)
    *processedSize = realProcessedSize;
  return result;
}


Z7_CLASS_IMP_NOQIB_1(
  COutStreamWithSha256
  , ISequentialOutStream
)
  bool _sha512Mode;
  CMyComPtr<ISequentialOutStream> _stream;
  CAlignedBuffer1 _sha256;
  CAlignedBuffer1 _sha512;
  UInt64 _size;

  CSha256 *Sha256() { return (CSha256 *)(void *)(Byte *)_sha256; }
  CSha512 *Sha512() { return (CSha512 *)(void *)(Byte *)_sha512; }
public:
  COutStreamWithSha256():
      _sha256(sizeof(CSha256)),
      _sha512(sizeof(CSha512))
      {}
  void SetStream(ISequentialOutStream *stream) { _stream = stream; }
  void ReleaseStream() { _stream.Release(); }
  void Init(bool sha512Mode)
  {
    _sha512Mode = sha512Mode;
    _size = 0;
    if (sha512Mode)
      Sha512_Init(Sha512(), SHA512_DIGEST_SIZE);
    else
      Sha256_Init(Sha256());
  }
  UInt64 GetSize() const { return _size; }
  void Final256(Byte *digest) { Sha256_Final(Sha256(), digest); }
  void Final512(Byte *digest) { Sha512_Final(Sha512(), digest, SHA512_DIGEST_SIZE); }
};

Z7_COM7F_IMF(COutStreamWithSha256::Write(const void *data, UInt32 size, UInt32 *processedSize))
{
  HRESULT result = S_OK;
  if (_stream)
    result = _stream->Write(data, size, &size);
  // if (_calculate)
  if (_sha512Mode)
    Sha512_Update(Sha512(), (const Byte *)data, size);
  else
    Sha256_Update(Sha256(), (const Byte *)data, size);
  _size += size;
  if (processedSize)
    *processedSize = size;
  return result;
}

// we limit supported xml sizes:
// ((size_t)1 << (sizeof(size_t) / 2 + 28)) - (1u << 14);
static const size_t kXmlSizeMax = ((size_t)1 << 30) - (1u << 14);
static const size_t kXmlPackSizeMax = kXmlSizeMax;

#define XAR_CKSUM_NONE    0
#define XAR_CKSUM_SHA1    1
#define XAR_CKSUM_MD5     2
#define XAR_CKSUM_SHA256  3
#define XAR_CKSUM_SHA512  4
// #define XAR_CKSUM_OTHER   3
// fork version of xar can use (3) as special case,
// where name of hash is stored as string at the end of header
// we do not support such hash still.

static const char * const k_ChecksumNames[] =
{
    "NONE"
  , "SHA1"
  , "MD5"
  , "SHA256"
  , "SHA512"
};

static unsigned GetHashSize(int algo)
{
  if (algo <= XAR_CKSUM_NONE || algo > XAR_CKSUM_SHA512)
    return 0;
  if (algo == XAR_CKSUM_SHA1)
    return SHA1_DIGEST_SIZE;
  return (16u >> XAR_CKSUM_MD5) << algo;
}

#define METHOD_NAME_ZLIB  "zlib"

static int Find_ChecksumId_for_Name(const AString &style)
{
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(k_ChecksumNames); i++)
  {
    // old xars used upper case in "style"
    // new xars use  lower case in "style"
    if (style.IsEqualTo_Ascii_NoCase(k_ChecksumNames[i]))
      return (int)i;
  }
  return -1;
}


struct CCheckSum
{
  int AlgoNumber;
  bool Error;
  CByteBuffer Data;
  AString Style;
  
  CCheckSum(): AlgoNumber(-1), Error(false) {}
  void AddNameToString(AString &s) const;
};

void CCheckSum::AddNameToString(AString &s) const
{
  if (Style.IsEmpty())
    s.Add_OptSpaced("NO-CHECKSUM");
  else
  {
    s.Add_OptSpaced(Style);
    if (Error)
      s += "-ERROR";
  }
}


struct CFile
{
  bool IsDir;
  bool Is_SymLink;
  bool HasData;
  bool Mode_Defined;
  bool INode_Defined;
  bool UserId_Defined;
  bool GroupId_Defined;
  // bool Device_Defined;
  bool Id_Defined;

  int Parent;
  UInt32 Mode;

  UInt64 Size;
  UInt64 PackSize;
  UInt64 Offset;
  UInt64 MTime;
  UInt64 CTime;
  UInt64 ATime;
  UInt64 INode;
  UInt64 UserId;
  UInt64 GroupId;
  // UInt64 Device;

  AString Name;
  AString Method;
  AString User;
  AString Group;
  // AString Id;
  AString Type;
  AString Link;
  // AString LinkType;
  // AString LinkFrom;
  
  UInt64 Id;
  CCheckSum extracted_checksum;
  CCheckSum archived_checksum;

  CFile(int parent):
      IsDir(false),
      Is_SymLink(false),
      HasData(false),
      Mode_Defined(false),
      INode_Defined(false),
      UserId_Defined(false),
      GroupId_Defined(false),
      // Device_Defined(false),
      Id_Defined(false),
      Parent(parent),
      Mode(0),
      Size(0), PackSize(0), Offset(0),
      MTime(0), CTime(0), ATime(0),
      INode(0)
      {}

  bool IsCopyMethod() const
  {
    return Method.IsEmpty() || Method == "octet-stream";
  }

  void UpdateTotalPackSize(UInt64 &totalSize) const
  {
    const UInt64 t = Offset + PackSize;
    if (t >= Offset)
    if (totalSize < t)
        totalSize = t;
  }
};


Z7_CLASS_IMP_CHandler_IInArchive_2(
    IArchiveGetRawProps,
    IInArchiveGetStream
)
  bool _is_pkg;
  bool _toc_CrcError;
  CObjectVector<CFile> _files;
  CMyComPtr<IInStream> _inStream;
  UInt64 _dataStartPos;
  UInt64 _phySize;
  CAlignedBuffer _xmlBuf;
  size_t _xmlLen;
  // UInt64 CreationTime;
  AString CreationTime_String;
  UInt32 _checkSumAlgo;
  Int32 _mainSubfile;

  HRESULT Open2(IInStream *stream);
};


static const Byte kArcProps[] =
{
  kpidSubType,
  // kpidHeadersSize,
  kpidMethod,
  kpidCTime
};

// #define kpidLinkType 250
// #define kpidLinkFrom 251

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidPackSize,
  kpidMTime,
  kpidCTime,
  kpidATime,
  kpidPosixAttrib,
  kpidType,
  kpidUser,
  kpidGroup,
  kpidUserId,
  kpidGroupId,
  kpidINode,
  // kpidDeviceMajor,
  // kpidDeviceMinor,
  kpidSymLink,
  // kpidLinkType,
  // kpidLinkFrom,
  kpidMethod,
  kpidId,
  kpidOffset
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

static bool ParseUInt64(const CXmlItem &item, const char *name, UInt64 &res)
{
  const AString s (item.GetSubStringForTag(name));
  if (s.IsEmpty())
    return false;
  const char *end;
  res = ConvertStringToUInt64(s, &end);
  return *end == 0;
}


#define PARSE_NUM(_num_, _dest_) \
    { const char *end; _dest_ = ConvertStringToUInt32(p, &end); \
    if ((unsigned)(end - p) != _num_) return 0; \
    p += _num_ + 1; }

static UInt64 ParseTime(const CXmlItem &item, const char *name /* , bool z_isRequired */ )
{
  const AString s (item.GetSubStringForTag(name));
  if (s.Len() < 20 /* (z_isRequired ? 20u : 19u) */)
    return 0;
  const char *p = s;
  if (p[ 4] != '-' ||
      p[ 7] != '-' ||
      p[10] != 'T' ||
      p[13] != ':' ||
      p[16] != ':')
    return 0;
  // if (z_isRequired)
  if (p[19] != 'Z')
    return 0;
  UInt32 year, month, day, hour, min, sec;
  PARSE_NUM(4, year)
  PARSE_NUM(2, month)
  PARSE_NUM(2, day)
  PARSE_NUM(2, hour)
  PARSE_NUM(2, min)
  PARSE_NUM(2, sec)
  UInt64 numSecs;
  if (!NTime::GetSecondsSince1601(year, month, day, hour, min, sec, numSecs))
    return 0;
  return numSecs * 10000000;
}


static void ParseChecksum(const CXmlItem &item, const char *name, CCheckSum &checksum)
{
  const CXmlItem *checkItem = item.FindSubTag_GetPtr(name);
  if (!checkItem)
    return; // false;
  checksum.Style = checkItem->GetPropVal("style");
  const AString s (checkItem->GetSubString());
  if ((s.Len() & 1) == 0 && s.Len() <= (2u << 7)) // 1024-bit max
  {
    const size_t size = s.Len() / 2;
    CByteBuffer temp(size);
    if ((size_t)(ParseHexString(s, temp) - temp) == size)
    {
      checksum.Data = temp;
      const int index = Find_ChecksumId_for_Name(checksum.Style);
      if (index >= 0 && checksum.Data.Size() == GetHashSize(index))
      {
        checksum.AlgoNumber = index;
        return;
      }
    }
  }
  checksum.Error = true;
}


static bool AddItem(const CXmlItem &item, CObjectVector<CFile> &files, int parent, int level)
{
  if (!item.IsTag)
    return true;
  if (level >= 1024)
    return false;
  if (item.Name == "file")
  {
    CFile file(parent);
    parent = (int)files.Size();
    {
      const AString id = item.GetPropVal("id");
      const char *end;
      file.Id = ConvertStringToUInt64(id, &end);
      if (*end == 0)
        file.Id_Defined = true;
    }
    file.Name = item.GetSubStringForTag("name");
    z7_xml_DecodeString(file.Name);
    {
      const CXmlItem *typeItem = item.FindSubTag_GetPtr("type");
      if (typeItem)
      {
        file.Type = typeItem->GetSubString();
        // file.LinkFrom = typeItem->GetPropVal("link");
        if (file.Type == "directory")
          file.IsDir = true;
        else
        {
          // file.IsDir = false;
          /*
          else if (file.Type == "file")
          {}
          else if (file.Type == "hardlink")
          {}
          else
          */
          if (file.Type == "symlink")
            file.Is_SymLink = true;
          // file.IsDir = false;
        }
      }
    }
    {
      const CXmlItem *linkItem = item.FindSubTag_GetPtr("link");
      if (linkItem)
      {
        // file.LinkType = linkItem->GetPropVal("type");
        file.Link = linkItem->GetSubString();
        z7_xml_DecodeString(file.Link);
      }
    }

    const CXmlItem *dataItem = item.FindSubTag_GetPtr("data");
    if (dataItem && !file.IsDir)
    {
      file.HasData = true;
      if (!ParseUInt64(*dataItem, "size", file.Size))
        return false;
      if (!ParseUInt64(*dataItem, "length", file.PackSize))
        return false;
      if (!ParseUInt64(*dataItem, "offset", file.Offset))
        return false;
      ParseChecksum(*dataItem, "extracted-checksum", file.extracted_checksum);
      ParseChecksum(*dataItem, "archived-checksum",  file.archived_checksum);
      const CXmlItem *encodingItem = dataItem->FindSubTag_GetPtr("encoding");
      if (encodingItem)
      {
        AString s (encodingItem->GetPropVal("style"));
        if (!s.IsEmpty())
        {
          const AString appl ("application/");
          if (s.IsPrefixedBy(appl))
          {
            s.DeleteFrontal(appl.Len());
            const AString xx ("x-");
            if (s.IsPrefixedBy(xx))
            {
              s.DeleteFrontal(xx.Len());
              if (s == "gzip")
                s = METHOD_NAME_ZLIB;
            }
          }
          file.Method = s;
        }
      }
    }

    file.INode_Defined = ParseUInt64(item, "inode", file.INode);
    file.UserId_Defined = ParseUInt64(item, "uid", file.UserId);
    file.GroupId_Defined = ParseUInt64(item, "gid", file.GroupId);
    // file.Device_Defined = ParseUInt64(item, "deviceno", file.Device);
    file.MTime = ParseTime(item, "mtime"); // z_IsRequied = true
    file.CTime = ParseTime(item, "ctime");
    file.ATime = ParseTime(item, "atime");
    {
      const AString s (item.GetSubStringForTag("mode"));
      if (s[0] == '0')
      {
        const char *end;
        file.Mode = ConvertOctStringToUInt32(s, &end);
        file.Mode_Defined = (*end == 0);
      }
    }
    file.User = item.GetSubStringForTag("user");
    file.Group = item.GetSubStringForTag("group");

    files.Add(file);
  }

  FOR_VECTOR (i, item.SubItems)
    if (!AddItem(item.SubItems[i], files, parent, level + 1))
      return false;
  return true;
}



struct CInStreamWithHash
{
  CMyComPtr2_Create<ISequentialInStream, CInStreamWithSha1> inStreamSha1;
  CMyComPtr2_Create<ISequentialInStream, CInStreamWithSha256> inStreamSha256;
  CMyComPtr2_Create<ISequentialInStream, CLimitedSequentialInStream> inStreamLim;

  void SetStreamAndInit(ISequentialInStream *stream, int algo);
  bool CheckHash(int algo, const Byte *digest_from_arc) const;
};


void CInStreamWithHash::SetStreamAndInit(ISequentialInStream *stream, int algo)
{
  if (algo == XAR_CKSUM_SHA1)
  {
    inStreamSha1->SetStream(stream);
    inStreamSha1->Init();
    stream = inStreamSha1;
  }
  else if (algo == XAR_CKSUM_SHA256
        || algo == XAR_CKSUM_SHA512)
  {
    inStreamSha256->SetStream(stream);
    inStreamSha256->Init(algo == XAR_CKSUM_SHA512);
    stream = inStreamSha256;
  }
  inStreamLim->SetStream(stream);
}

bool CInStreamWithHash::CheckHash(int algo, const Byte *digest_from_arc) const
{
  if (algo == XAR_CKSUM_SHA1)
  {
    Byte digest[SHA1_DIGEST_SIZE];
    inStreamSha1->Final(digest);
    if (memcmp(digest, digest_from_arc, sizeof(digest)) != 0)
      return false;
  }
  else if (algo == XAR_CKSUM_SHA256)
  {
    Byte digest[SHA256_DIGEST_SIZE];
    inStreamSha256->Final256(digest);
    if (memcmp(digest, digest_from_arc, sizeof(digest)) != 0)
      return false;
  }
  else if (algo == XAR_CKSUM_SHA512)
  {
    Byte digest[SHA512_DIGEST_SIZE];
    inStreamSha256->Final512(digest);
    if (memcmp(digest, digest_from_arc, sizeof(digest)) != 0)
      return false;
  }
  return true;
}


HRESULT CHandler::Open2(IInStream *stream)
{
  const unsigned kHeaderSize = 28;
  UInt32 buf32[kHeaderSize / sizeof(UInt32)];
  RINOK(ReadStream_FALSE(stream, buf32, kHeaderSize))
  const unsigned headerSize = Get16((const Byte *)(const void *)buf32 + 4);
  // xar library now writes 1 to version field.
  // some old xars could have version == 0 ?
  // specification allows (headerSize != 28),
  // but we don't expect big value in (headerSize).
  // so we restrict (headerSize) with 64 bytes to reduce false open.
  const unsigned kHeaderSize_MAX = 64;
  if (Get32(buf32) != 0x78617221  // signature: "xar!"
      || headerSize < kHeaderSize
      || headerSize > kHeaderSize_MAX
      || Get16((const Byte *)(const void *)buf32 + 6) > 1 // version
      )
    return S_FALSE;
  _checkSumAlgo = Get32(buf32 + 6);
  const UInt64 packSize = Get64(buf32 + 2);
  const UInt64 unpackSize = Get64(buf32 + 4);
  if (packSize >= kXmlPackSizeMax ||
      unpackSize >= kXmlSizeMax)
    return S_FALSE;
  /* some xar archives can have padding bytes at offset 28,
     or checksum algorithm name at offset 28 (in xar fork, if cksum_alg==3)
     But we didn't see such xar archives.
  */
  if (headerSize != kHeaderSize)
  {
    RINOK(InStream_SeekSet(stream, headerSize))
  }
  _dataStartPos = headerSize + packSize;
  _phySize = _dataStartPos;

  _xmlBuf.Alloc((size_t)unpackSize + 1);
  if (!_xmlBuf.IsAllocated())
    return E_OUTOFMEMORY;
  _xmlLen = (size_t)unpackSize;
  
  CInStreamWithHash hashStream;
  {
    CMyComPtr2_Create<ICompressCoder, NCompress::NZlib::CDecoder> zlibCoder;
    hashStream.SetStreamAndInit(stream, (int)(unsigned)_checkSumAlgo);
    hashStream.inStreamLim->Init(packSize);
    CMyComPtr2_Create<ISequentialOutStream, CBufPtrSeqOutStream> outStreamLim;
    outStreamLim->Init(_xmlBuf, (size_t)unpackSize);
    RINOK(zlibCoder.Interface()->Code(hashStream.inStreamLim, outStreamLim, NULL, &unpackSize, NULL))
    if (outStreamLim->GetPos() != (size_t)unpackSize)
      return S_FALSE;
  }
  _xmlBuf[(size_t)unpackSize] = 0;
  if (strlen((const char *)(const Byte *)_xmlBuf) != (size_t)unpackSize)
    return S_FALSE;
  CXml xml;
  if (!xml.Parse((const char *)(const Byte *)_xmlBuf))
    return S_FALSE;
  
  if (!xml.Root.IsTagged("xar") || xml.Root.SubItems.Size() != 1)
    return S_FALSE;
  const CXmlItem &toc = xml.Root.SubItems[0];
  if (!toc.IsTagged("toc"))
    return S_FALSE;

  // CreationTime = ParseTime(toc, "creation-time", false); // z_IsRequied
  CreationTime_String = toc.GetSubStringForTag("creation-time");
  {
    // we suppose that offset of checksum is always 0;
    // but [TOC].xml contains exact offset value in <checksum> block.
    const UInt64 offset = 0;
    const unsigned hashSize = GetHashSize((int)(unsigned)_checkSumAlgo);
    if (hashSize)
    {
      /*
      const CXmlItem *csItem = toc.FindSubTag_GetPtr("checksum");
      if (csItem)
      {
        const int checkSumAlgo2 = Find_ChecksumId_for_Name(csItem->GetPropVal("style"));
        UInt64 csSize, csOffset;
        if (ParseUInt64(*csItem, "size", csSize) &&
            ParseUInt64(*csItem, "offset", csOffset)  &&
            csSize == hashSize &&
            (unsigned)checkSumAlgo2 == _checkSumAlgo)
          offset = csOffset;
      }
      */
      CByteBuffer digest_from_arc(hashSize);
      RINOK(InStream_SeekSet(stream, _dataStartPos + offset))
      RINOK(ReadStream_FALSE(stream, digest_from_arc, hashSize))
      if (!hashStream.CheckHash((int)(unsigned)_checkSumAlgo, digest_from_arc))
        _toc_CrcError = true;
    }
  }
    
  if (!AddItem(toc, _files,
      -1, // parent
      0)) // level
    return S_FALSE;

  UInt64 totalPackSize = 0;
  unsigned numMainFiles = 0;
  
  FOR_VECTOR (i, _files)
  {
    const CFile &file = _files[i];
    file.UpdateTotalPackSize(totalPackSize);
    if (file.Parent == -1)
    {
      if (file.Name == "Payload" || file.Name == "Content")
      {
        _mainSubfile = (Int32)(int)i;
        numMainFiles++;
      }
      else if (file.Name == "PackageInfo")
        _is_pkg = true;
    }
  }

  if (numMainFiles > 1)
    _mainSubfile = -1;

  const UInt64 k_PhySizeLim = (UInt64)1 << 62;
  _phySize = (totalPackSize > k_PhySizeLim - _dataStartPos) ?
      k_PhySizeLim :
      _dataStartPos + totalPackSize;

  return S_OK;
}

Z7_COM7F_IMF(CHandler::Open(IInStream *stream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback * /* openArchiveCallback */))
{
  COM_TRY_BEGIN
  {
    Close();
    RINOK(Open2(stream))
    _inStream = stream;
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _phySize = 0;
  _dataStartPos = 0;
  _inStream.Release();
  _files.Clear();
  _xmlLen = 0;
  _xmlBuf.Free();
  _mainSubfile = -1;
  _is_pkg = false;
  _toc_CrcError = false;
  _checkSumAlgo = 0;
  // CreationTime = 0;
  CreationTime_String.Empty();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _files.Size()
#ifdef XAR_SHOW_RAW
    + 1
#endif
  ;
  return S_OK;
}

static void TimeToProp(UInt64 t, NCOM::CPropVariant &prop)
{
  if (t != 0)
  {
    FILETIME ft;
    ft.dwLowDateTime = (UInt32)(t);
    ft.dwHighDateTime = (UInt32)(t >> 32);
    prop = ft;
  }
}

static void Utf8StringToProp(const AString &s, NCOM::CPropVariant &prop)
{
  if (!s.IsEmpty())
  {
    UString us;
    ConvertUTF8ToUnicode(s, us);
    prop = us;
  }
}

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    // case kpidHeadersSize: prop = _dataStartPos; break;
    case kpidPhySize: prop = _phySize; break;
    case kpidMainSubfile: if (_mainSubfile >= 0) prop = (UInt32)_mainSubfile; break;
    case kpidSubType: if (_is_pkg) prop = "pkg"; break;
    case kpidExtension: prop = _is_pkg ? "pkg" : "xar"; break;
    case kpidCTime:
    {
      // it's local time. We can transfer it to UTC time, if we use FILETIME.
      // TimeToProp(CreationTime, prop); break;
      if (!CreationTime_String.IsEmpty())
        prop = CreationTime_String;
      break;
    }
    case kpidMethod:
    {
      AString s;
      if (_checkSumAlgo < Z7_ARRAY_SIZE(k_ChecksumNames))
        s = k_ChecksumNames[_checkSumAlgo];
      else
      {
        s += "Checksum";
        s.Add_UInt32(_checkSumAlgo);
      }
      prop = s;
      break;
    }
    case kpidWarningFlags:
    {
      UInt32 v = 0;
      if (_toc_CrcError) v |= kpv_ErrorFlags_CrcError;
      prop = v;
      break;
    }
    case kpidINode: prop = true; break;
    case kpidIsTree: prop = true; break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


/*
inline UInt32 MY_dev_major(UInt64 dev)
{
  return ((UInt32)(dev >> 8) & (UInt32)0xfff) | ((UInt32)(dev >> 32) & ~(UInt32)0xfff);
}
inline UInt32 MY_dev_minor(UInt64 dev)
{
  return ((UInt32)(dev) & 0xff) | ((UInt32)(dev >> 12) & ~(UInt32)0xff);
}
*/

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  
#ifdef XAR_SHOW_RAW
  if (index >= _files.Size())
  {
    switch (propID)
    {
      case kpidName:
      case kpidPath:
        prop = "[TOC].xml"; break;
      case kpidSize:
      case kpidPackSize: prop = (UInt64)_xmlLen; break;
    }
  }
  else
#endif
  {
    const CFile &item = _files[index];
    switch (propID)
    {
      case kpidPath:
      {
        AString path;
        unsigned cur = index;
        for (;;)
        {
          const CFile &item2 = _files[cur];
          if (!path.IsEmpty())
            path.InsertAtFront(CHAR_PATH_SEPARATOR);
// #define XAR_EMPTY_NAME_REPLACEMENT "[]"
          if (item2.Name.IsEmpty())
          {
            AString s('[');
            s.Add_UInt32(cur);
            s.Add_Char(']');
            path.Insert(0, s);
          }
          else
            path.Insert(0, item2.Name);
          if (item2.Parent < 0)
            break;
          cur = (unsigned)item2.Parent;
        }
        Utf8StringToProp(path, prop);
        break;
      }

      case kpidName:
      {
        if (item.Name.IsEmpty())
        {
          AString s('[');
          s.Add_UInt32(index);
          s.Add_Char(']');
          prop = s;
        }
        else
          Utf8StringToProp(item.Name, prop);
        break;
      }

      case kpidIsDir: prop = item.IsDir; break;
      
      case kpidSize:     if (item.HasData && !item.IsDir) prop = item.Size; break;
      case kpidPackSize: if (item.HasData && !item.IsDir) prop = item.PackSize; break;

      case kpidMethod:
      {
        if (item.HasData)
        {
          AString s = item.Method;
          item.extracted_checksum.AddNameToString(s);
          item.archived_checksum.AddNameToString(s);
          Utf8StringToProp(s, prop);
        }
        break;
      }
        
      case kpidMTime: TimeToProp(item.MTime, prop); break;
      case kpidCTime: TimeToProp(item.CTime, prop); break;
      case kpidATime: TimeToProp(item.ATime, prop); break;
      
      case kpidPosixAttrib:
        if (item.Mode_Defined)
        {
          UInt32 mode = item.Mode;
          if ((mode & MY_LIN_S_IFMT) == 0)
            mode |= (
                item.Is_SymLink ? MY_LIN_S_IFLNK :
                item.IsDir      ? MY_LIN_S_IFDIR :
                                  MY_LIN_S_IFREG);
          prop = mode;
        }
        break;
      
      case kpidType:  Utf8StringToProp(item.Type, prop); break;
      case kpidUser:  Utf8StringToProp(item.User, prop); break;
      case kpidGroup: Utf8StringToProp(item.Group, prop); break;
      case kpidSymLink: if (item.Is_SymLink) Utf8StringToProp(item.Link, prop); break;
      
      case kpidUserId:  if (item.UserId_Defined)  prop = item.UserId;   break;
      case kpidGroupId: if (item.GroupId_Defined) prop = item.GroupId;  break;
      case kpidINode:   if (item.INode_Defined)   prop = item.INode;    break;
      case kpidId:      if (item.Id_Defined)      prop = item.Id;       break;
      // Utf8StringToProp(item.Id, prop);
      /*
      case kpidDeviceMajor: if (item.Device_Defined) prop = (UInt32)MY_dev_major(item.Device);  break;
      case kpidDeviceMinor: if (item.Device_Defined) prop = (UInt32)MY_dev_minor(item.Device);  break;
      case kpidLinkType:
        if (!item.LinkType.IsEmpty())
          Utf8StringToProp(item.LinkType, prop);
        break;
      case kpidLinkFrom:
        if (!item.LinkFrom.IsEmpty())
          Utf8StringToProp(item.LinkFrom, prop);
        break;
      */
      case kpidOffset:
        if (item.HasData)
          prop = _dataStartPos + item.Offset;
        break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


// for debug:
// #define Z7_XAR_SHOW_CHECKSUM_PACK

#ifdef Z7_XAR_SHOW_CHECKSUM_PACK
enum
{
  kpidChecksumPack = kpidUserDefined
};
#endif

static const Byte kRawProps[] =
{
  kpidChecksum
#ifdef Z7_XAR_SHOW_CHECKSUM_PACK
  , kpidCRC // instead of kpidUserDefined / kpidCRC
#endif
};

Z7_COM7F_IMF(CHandler::GetNumRawProps(UInt32 *numProps))
{
  *numProps = Z7_ARRAY_SIZE(kRawProps);
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetRawPropInfo(UInt32 index, BSTR *name, PROPID *propID))
{
  *propID = kRawProps[index];
  *name = NULL;

#ifdef Z7_XAR_SHOW_CHECKSUM_PACK
  if (index != 0)
  {
    *propID = kpidChecksumPack;
    *name = NWindows::NCOM::AllocBstrFromAscii("archived-checksum");
  }
#endif
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetParent(UInt32 index, UInt32 *parent, UInt32 *parentType))
{
  *parentType = NParentType::kDir;
  *parent = (UInt32)(Int32)-1;
#ifdef XAR_SHOW_RAW
  if (index >= _files.Size())
    return S_OK;
#endif
  {
    const CFile &item = _files[index];
    *parent = (UInt32)(Int32)item.Parent;
  }
  return S_OK;
}


Z7_COM7F_IMF(CHandler::GetRawProp(UInt32 index, PROPID propID, const void **data, UInt32 *dataSize, UInt32 *propType))
{
  *data = NULL;
  *dataSize = 0;
  *propType = 0;

  // COM_TRY_BEGIN
  NCOM::CPropVariant prop;

  if (propID == kpidChecksum)
  {
#ifdef XAR_SHOW_RAW
    if (index >= _files.Size())
    {
      // case kpidPath: prop = "[TOC].xml"; break;
    }
    else
#endif
    {
      const CFile &item = _files[index];
      const size_t size = item.extracted_checksum.Data.Size();
      if (size != 0)
      {
        *dataSize = (UInt32)size;
        *propType = NPropDataType::kRaw;
        *data = item.extracted_checksum.Data;
      }
    }
  }

#ifdef Z7_XAR_SHOW_CHECKSUM_PACK
  if (propID == kpidChecksumPack)
  {
#ifdef XAR_SHOW_RAW
    if (index >= _files.Size())
    {
      // we can show digest check sum here
    }
    else
#endif
    {
      const CFile &item = _files[index];
      const size_t size = (UInt32)item.archived_checksum.Data.Size();
      if (size != 0)
      {
        *dataSize = (UInt32)size;
        *propType = NPropDataType::kRaw;
        *data = item.archived_checksum.Data;
      }
    }
  }
#endif
  return S_OK;
}




Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _files.Size()
#ifdef XAR_SHOW_RAW
    + 1
#endif
    ;
  if (numItems == 0)
    return S_OK;
  UInt64 totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    const UInt32 index = allFilesMode ? i : indices[i];
#ifdef XAR_SHOW_RAW
    if (index >= _files.Size())
      totalSize += _xmlLen;
    else
#endif
      totalSize += _files[index].Size;
  }
  RINOK(extractCallback->SetTotal(totalSize))

  CMyComPtr2_Create<ICompressProgressInfo, CLocalProgress> lps;
  lps->Init(extractCallback, false);
  CInStreamWithHash inHashStream;
  CMyComPtr2_Create<ISequentialOutStream, COutStreamWithSha1> outStreamSha1;
  CMyComPtr2_Create<ISequentialOutStream, COutStreamWithSha256> outStreamSha256;
  CMyComPtr2_Create<ISequentialOutStream, CLimitedSequentialOutStream> outStreamLim;
  CMyComPtr2_Create<ICompressCoder, NCompress::CCopyCoder> copyCoder;
  CMyComPtr2_Create<ICompressCoder, NCompress::NZlib::CDecoder> zlibCoder;
  CMyComPtr2_Create<ICompressCoder, NCompress::NBZip2::CDecoder> bzip2Coder;
  bzip2Coder->FinishMode = true;
  
  UInt64 cur_PackSize, cur_UnpSize;

  for (i = 0;; i++,
      lps->InSize += cur_PackSize,
      lps->OutSize += cur_UnpSize)
  {
    cur_PackSize = 0;
    cur_UnpSize = 0;
    RINOK(lps->SetCur())
    if (i >= numItems)
      break;

    CMyComPtr<ISequentialOutStream> realOutStream;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    const UInt32 index = allFilesMode ? i : indices[i];
    RINOK(extractCallback->GetStream(index, &realOutStream, askMode))
    
    if (index < _files.Size())
    {
      const CFile &item = _files[index];
      if (item.IsDir)
      {
        RINOK(extractCallback->PrepareOperation(askMode))
        realOutStream.Release();
        RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
        continue;
      }
    }

    if (!testMode && !realOutStream)
      continue;
    RINOK(extractCallback->PrepareOperation(askMode))

    Int32 opRes = NExtract::NOperationResult::kOK;

#ifdef XAR_SHOW_RAW
    if (index >= _files.Size())
    {
      cur_PackSize = cur_UnpSize = _xmlLen;
      if (realOutStream)
        RINOK(WriteStream(realOutStream, _xmlBuf, _xmlLen))
      realOutStream.Release();
    }
    else
#endif
    {
      const CFile &item = _files[index];
      if (!item.HasData)
        realOutStream.Release();
      else
      {
        cur_PackSize = item.PackSize;
        cur_UnpSize = item.Size;
        
        RINOK(InStream_SeekSet(_inStream, _dataStartPos + item.Offset))

        inHashStream.SetStreamAndInit(_inStream, item.archived_checksum.AlgoNumber);
        inHashStream.inStreamLim->Init(item.PackSize);

        const int checksum_method = item.extracted_checksum.AlgoNumber;
        if (checksum_method == XAR_CKSUM_SHA1)
        {
          outStreamLim->SetStream(outStreamSha1);
          outStreamSha1->SetStream(realOutStream);
          outStreamSha1->Init();
        }
        else if (checksum_method == XAR_CKSUM_SHA256
              || checksum_method == XAR_CKSUM_SHA512)
        {
          outStreamLim->SetStream(outStreamSha256);
          outStreamSha256->SetStream(realOutStream);
          outStreamSha256->Init(checksum_method == XAR_CKSUM_SHA512);
        }
        else
          outStreamLim->SetStream(realOutStream);

        realOutStream.Release();

        // outStreamSha1->Init(item.Sha1IsDefined);

        outStreamLim->Init(item.Size);
        HRESULT res = S_OK;
        
        ICompressCoder *coder = NULL;
        if (item.IsCopyMethod())
        {
          if (item.PackSize == item.Size)
            coder = copyCoder;
          else
            opRes = NExtract::NOperationResult::kUnsupportedMethod;
        }
        else if (item.Method == METHOD_NAME_ZLIB)
          coder = zlibCoder;
        else if (item.Method == "bzip2")
          coder = bzip2Coder;
        else
          opRes = NExtract::NOperationResult::kUnsupportedMethod;
        
        if (coder)
          res = coder->Code(inHashStream.inStreamLim, outStreamLim, NULL, &item.Size, lps);
        
        if (res != S_OK)
        {
          if (!outStreamLim->IsFinishedOK())
            opRes = NExtract::NOperationResult::kDataError;
          else if (res != S_FALSE)
            return res;
          if (opRes == NExtract::NOperationResult::kOK)
            opRes = NExtract::NOperationResult::kDataError;
        }

        if (opRes == NExtract::NOperationResult::kOK)
        {
          if (outStreamLim->IsFinishedOK())
          {
            if (checksum_method == XAR_CKSUM_SHA1)
            {
              Byte digest[SHA1_DIGEST_SIZE];
              outStreamSha1->Final(digest);
              if (memcmp(digest, item.extracted_checksum.Data, SHA1_DIGEST_SIZE) != 0)
                opRes = NExtract::NOperationResult::kCRCError;
            }
            else if (checksum_method == XAR_CKSUM_SHA256)
            {
              Byte digest[SHA256_DIGEST_SIZE];
              outStreamSha256->Final256(digest);
              if (memcmp(digest, item.extracted_checksum.Data, sizeof(digest)) != 0)
                opRes = NExtract::NOperationResult::kCRCError;
            }
            else if (checksum_method == XAR_CKSUM_SHA512)
            {
              Byte digest[SHA512_DIGEST_SIZE];
              outStreamSha256->Final512(digest);
              if (memcmp(digest, item.extracted_checksum.Data, sizeof(digest)) != 0)
                opRes = NExtract::NOperationResult::kCRCError;
            }
            if (opRes == NExtract::NOperationResult::kOK)
              if (!inHashStream.CheckHash(
                    item.archived_checksum.AlgoNumber,
                    item.archived_checksum.Data))
                opRes = NExtract::NOperationResult::kCRCError;
          }
          else
            opRes = NExtract::NOperationResult::kDataError;
        }
        if (checksum_method == XAR_CKSUM_SHA1)
          outStreamSha1->ReleaseStream();
        else if (checksum_method == XAR_CKSUM_SHA256)
          outStreamSha256->ReleaseStream();
      }
      outStreamLim->ReleaseStream();
    }
    RINOK(extractCallback->SetOperationResult(opRes))
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetStream(UInt32 index, ISequentialInStream **stream))
{
  *stream = NULL;
  COM_TRY_BEGIN
#ifdef XAR_SHOW_RAW
  if (index >= _files.Size())
  {
    Create_BufInStream_WithNewBuffer(_xmlBuf, _xmlLen, stream);
    return S_OK;
  }
  else
#endif
  {
    const CFile &item = _files[index];
    if (item.HasData && item.IsCopyMethod() && item.PackSize == item.Size)
      return CreateLimitedInStream(_inStream, _dataStartPos + item.Offset, item.Size, stream);
  }
  return S_FALSE;
  COM_TRY_END
}

// 0x1c == 28 is expected header size value for most archives.
// but we want to support another (rare case) headers sizes.
// so we must reduce signature to 4 or 5 bytes.
static const Byte k_Signature[] =
//  { 'x', 'a', 'r', '!', 0, 0x1C, 0 };
    { 'x', 'a', 'r', '!', 0 };

REGISTER_ARC_I(
  "Xar", "xar pkg xip", NULL, 0xE1,
  k_Signature,
  0,
  0,
  NULL)

}}

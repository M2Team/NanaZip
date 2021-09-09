// XarHandler.cpp

#include "StdAfx.h"

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

static const size_t kXmlSizeMax = ((size_t )1 << 30) - (1 << 14);
static const size_t kXmlPackSizeMax = kXmlSizeMax;

/*
#define XAR_CKSUM_NONE  0
#define XAR_CKSUM_SHA1  1
#define XAR_CKSUM_MD5   2

static const char * const k_ChecksumAlgos[] =
{
    "None"
  , "SHA-1"
  , "MD5"
};
*/

#define METHOD_NAME_ZLIB "zlib"


struct CFile
{
  AString Name;
  AString Method;
  UInt64 Size;
  UInt64 PackSize;
  UInt64 Offset;
  
  UInt64 CTime;
  UInt64 MTime;
  UInt64 ATime;
  UInt32 Mode;

  AString User;
  AString Group;
  
  bool IsDir;
  bool HasData;
  bool ModeDefined;
  bool Sha1IsDefined;
  // bool packSha1IsDefined;

  Byte Sha1[SHA1_DIGEST_SIZE];
  // Byte packSha1[SHA1_DIGEST_SIZE];

  int Parent;

  CFile():
      Size(0), PackSize(0), Offset(0),
      CTime(0), MTime(0), ATime(0), Mode(0),
      IsDir(false), HasData(false), ModeDefined(false), Sha1IsDefined(false),
      /* packSha1IsDefined(false), */
      Parent(-1)
      {}

  bool IsCopyMethod() const
  {
    return Method.IsEmpty() || Method == "octet-stream";
  }

  void UpdateTotalPackSize(UInt64 &totalSize) const
  {
    UInt64 t = Offset + PackSize;
    if (totalSize < t)
      totalSize = t;
  }
};

class CHandler:
  public IInArchive,
  public IInArchiveGetStream,
  public CMyUnknownImp
{
  UInt64 _dataStartPos;
  CMyComPtr<IInStream> _inStream;
  CByteArr _xml;
  size_t _xmlLen;
  CObjectVector<CFile> _files;
  // UInt32 _checkSumAlgo;
  UInt64 _phySize;
  Int32 _mainSubfile;
  bool _is_pkg;

  HRESULT Open2(IInStream *stream);
  HRESULT Extract(IInStream *stream);
public:
  MY_UNKNOWN_IMP2(IInArchive, IInArchiveGetStream)
  INTERFACE_IInArchive(;)
  STDMETHOD(GetStream)(UInt32 index, ISequentialInStream **stream);
};

static const Byte kArcProps[] =
{
  kpidSubType,
  kpidHeadersSize
};

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidPackSize,
  kpidMTime,
  kpidCTime,
  kpidATime,
  kpidPosixAttrib,
  kpidUser,
  kpidGroup,
  kpidMethod
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

#define PARSE_NUM(_num_, _dest_) \
    { const char *end; _dest_ = ConvertStringToUInt32(p, &end); \
    if ((unsigned)(end - p) != _num_) return 0; p += _num_ + 1; }

static bool ParseUInt64(const CXmlItem &item, const char *name, UInt64 &res)
{
  const AString s (item.GetSubStringForTag(name));
  if (s.IsEmpty())
    return false;
  const char *end;
  res = ConvertStringToUInt64(s, &end);
  return *end == 0;
}

static UInt64 ParseTime(const CXmlItem &item, const char *name)
{
  const AString s (item.GetSubStringForTag(name));
  if (s.Len() < 20)
    return 0;
  const char *p = s;
  if (p[ 4] != '-' || p[ 7] != '-' || p[10] != 'T' ||
      p[13] != ':' || p[16] != ':' || p[19] != 'Z')
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

static int HexToByte(unsigned char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

static bool ParseSha1(const CXmlItem &item, const char *name, Byte *digest)
{
  int index = item.FindSubTag(name);
  if (index < 0)
    return false;
  const CXmlItem &checkItem = item.SubItems[index];
  const AString style (checkItem.GetPropVal("style"));
  if (style == "SHA1")
  {
    const AString s (checkItem.GetSubString());
    if (s.Len() != SHA1_DIGEST_SIZE * 2)
      return false;
    for (unsigned i = 0; i < s.Len(); i += 2)
    {
      int b0 = HexToByte(s[i]);
      int b1 = HexToByte(s[i + 1]);
      if (b0 < 0 || b1 < 0)
        return false;
      digest[i / 2] = (Byte)((b0 << 4) | b1);
    }
    return true;
  }
  return false;
}

static bool AddItem(const CXmlItem &item, CObjectVector<CFile> &files, int parent)
{
  if (!item.IsTag)
    return true;
  if (item.Name == "file")
  {
    CFile file;
    file.Parent = parent;
    parent = files.Size();
    file.Name = item.GetSubStringForTag("name");
    const AString type (item.GetSubStringForTag("type"));
    if (type == "directory")
      file.IsDir = true;
    else if (type == "file")
      file.IsDir = false;
    else
      return false;

    int dataIndex = item.FindSubTag("data");
    if (dataIndex >= 0 && !file.IsDir)
    {
      file.HasData = true;
      const CXmlItem &dataItem = item.SubItems[dataIndex];
      if (!ParseUInt64(dataItem, "size", file.Size))
        return false;
      if (!ParseUInt64(dataItem, "length", file.PackSize))
        return false;
      if (!ParseUInt64(dataItem, "offset", file.Offset))
        return false;
      file.Sha1IsDefined = ParseSha1(dataItem, "extracted-checksum", file.Sha1);
      // file.packSha1IsDefined = ParseSha1(dataItem, "archived-checksum",  file.packSha1);
      int encodingIndex = dataItem.FindSubTag("encoding");
      if (encodingIndex >= 0)
      {
        const CXmlItem &encodingItem = dataItem.SubItems[encodingIndex];
        if (encodingItem.IsTag)
        {
          AString s (encodingItem.GetPropVal("style"));
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
    }

    file.CTime = ParseTime(item, "ctime");
    file.MTime = ParseTime(item, "mtime");
    file.ATime = ParseTime(item, "atime");

    {
      const AString s (item.GetSubStringForTag("mode"));
      if (s[0] == '0')
      {
        const char *end;
        file.Mode = ConvertOctStringToUInt32(s, &end);
        file.ModeDefined = (*end == 0);
      }
    }

    file.User = item.GetSubStringForTag("user");
    file.Group = item.GetSubStringForTag("group");

    files.Add(file);
  }
  FOR_VECTOR (i, item.SubItems)
    if (!AddItem(item.SubItems[i], files, parent))
      return false;
  return true;
}

HRESULT CHandler::Open2(IInStream *stream)
{
  const UInt32 kHeaderSize = 0x1C;
  Byte buf[kHeaderSize];
  RINOK(ReadStream_FALSE(stream, buf, kHeaderSize));

  UInt32 size = Get16(buf + 4);
  // UInt32 ver = Get16(buf + 6); // == 1
  if (Get32(buf) != 0x78617221 || size != kHeaderSize)
    return S_FALSE;

  UInt64 packSize = Get64(buf + 8);
  UInt64 unpackSize = Get64(buf + 0x10);

  // _checkSumAlgo = Get32(buf + 0x18);

  if (packSize >= kXmlPackSizeMax ||
      unpackSize >= kXmlSizeMax)
    return S_FALSE;

  _dataStartPos = kHeaderSize + packSize;
  _phySize = _dataStartPos;

  _xml.Alloc((size_t)unpackSize + 1);
  _xmlLen = (size_t)unpackSize;
  
  NCompress::NZlib::CDecoder *zlibCoderSpec = new NCompress::NZlib::CDecoder();
  CMyComPtr<ICompressCoder> zlibCoder = zlibCoderSpec;

  CLimitedSequentialInStream *inStreamLimSpec = new CLimitedSequentialInStream;
  CMyComPtr<ISequentialInStream> inStreamLim(inStreamLimSpec);
  inStreamLimSpec->SetStream(stream);
  inStreamLimSpec->Init(packSize);

  CBufPtrSeqOutStream *outStreamLimSpec = new CBufPtrSeqOutStream;
  CMyComPtr<ISequentialOutStream> outStreamLim(outStreamLimSpec);
  outStreamLimSpec->Init(_xml, (size_t)unpackSize);

  RINOK(zlibCoder->Code(inStreamLim, outStreamLim, NULL, NULL, NULL));

  if (outStreamLimSpec->GetPos() != (size_t)unpackSize)
    return S_FALSE;

  _xml[(size_t)unpackSize] = 0;
  if (strlen((const char *)(const Byte *)_xml) != unpackSize) return S_FALSE;

  CXml xml;
  if (!xml.Parse((const char *)(const Byte *)_xml))
    return S_FALSE;
  
  if (!xml.Root.IsTagged("xar") || xml.Root.SubItems.Size() != 1)
    return S_FALSE;
  const CXmlItem &toc = xml.Root.SubItems[0];
  if (!toc.IsTagged("toc"))
    return S_FALSE;
  if (!AddItem(toc, _files, -1))
    return S_FALSE;

  UInt64 totalPackSize = 0;
  unsigned numMainFiles = 0;
  
  FOR_VECTOR (i, _files)
  {
    const CFile &file = _files[i];
    file.UpdateTotalPackSize(totalPackSize);
    if (file.Name == "Payload" || file.Name == "Content")
    {
      _mainSubfile = i;
      numMainFiles++;
    }
    else if (file.Name == "PackageInfo")
      _is_pkg = true;
  }

  if (numMainFiles > 1)
    _mainSubfile = -1;
  
  _phySize = _dataStartPos + totalPackSize;

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
  _xmlLen = 0;
  _xml.Free();
  _mainSubfile = -1;
  _is_pkg = false;
  return S_OK;
}

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
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

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidHeadersSize: prop = _dataStartPos; break;
    case kpidPhySize: prop = _phySize; break;
    case kpidMainSubfile: if (_mainSubfile >= 0) prop = (UInt32)_mainSubfile; break;
    case kpidSubType: if (_is_pkg) prop = "pkg"; break;
    case kpidExtension: prop = _is_pkg ? "pkg" : "xar"; break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  
  #ifdef XAR_SHOW_RAW
  if (index == _files.Size())
  {
    switch (propID)
    {
      case kpidPath: prop = "[TOC].xml"; break;
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
      case kpidMethod: Utf8StringToProp(item.Method, prop); break;

      case kpidPath:
      {
        AString path;
        int cur = index;
        do
        {
          const CFile &item2 = _files[cur];
          if (!path.IsEmpty())
            path.InsertAtFront(CHAR_PATH_SEPARATOR);
          if (item2.Name.IsEmpty())
            path.Insert(0, "unknown");
          else
            path.Insert(0, item2.Name);
          cur = item2.Parent;
        }
        while (cur >= 0);

        Utf8StringToProp(path, prop);
        break;
      }
      
      case kpidIsDir: prop = item.IsDir; break;
      case kpidSize: if (!item.IsDir) prop = item.Size; break;
      case kpidPackSize: if (!item.IsDir) prop = item.PackSize; break;
      
      case kpidMTime: TimeToProp(item.MTime, prop); break;
      case kpidCTime: TimeToProp(item.CTime, prop); break;
      case kpidATime: TimeToProp(item.ATime, prop); break;
      case kpidPosixAttrib:
        if (item.ModeDefined)
        {
          UInt32 mode = item.Mode;
          if ((mode & MY_LIN_S_IFMT) == 0)
            mode |= (item.IsDir ? MY_LIN_S_IFDIR : MY_LIN_S_IFREG);
          prop = mode;
        }
        break;
      case kpidUser: Utf8StringToProp(item.User, prop); break;
      case kpidGroup: Utf8StringToProp(item.Group, prop); break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
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
    #ifdef XAR_SHOW_RAW
    if (index == _files.Size())
      totalSize += _xmlLen;
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

  NCompress::NZlib::CDecoder *zlibCoderSpec = new NCompress::NZlib::CDecoder();
  CMyComPtr<ICompressCoder> zlibCoder = zlibCoderSpec;
  
  NCompress::NBZip2::CDecoder *bzip2CoderSpec = new NCompress::NBZip2::CDecoder();
  CMyComPtr<ICompressCoder> bzip2Coder = bzip2CoderSpec;

  NCompress::NDeflate::NDecoder::CCOMCoder *deflateCoderSpec = new NCompress::NDeflate::NDecoder::CCOMCoder();
  CMyComPtr<ICompressCoder> deflateCoder = deflateCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  CLimitedSequentialInStream *inStreamSpec = new CLimitedSequentialInStream;
  CMyComPtr<ISequentialInStream> inStream(inStreamSpec);
  inStreamSpec->SetStream(_inStream);

  
  CLimitedSequentialOutStream *outStreamLimSpec = new CLimitedSequentialOutStream;
  CMyComPtr<ISequentialOutStream> outStream(outStreamLimSpec);

  COutStreamWithSha1 *outStreamSha1Spec = new COutStreamWithSha1;
  {
    CMyComPtr<ISequentialOutStream> outStreamSha1(outStreamSha1Spec);
    outStreamLimSpec->SetStream(outStreamSha1);
  }

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
    
    if (index < _files.Size())
    {
      const CFile &item = _files[index];
      if (item.IsDir)
      {
        RINOK(extractCallback->PrepareOperation(askMode));
        RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK));
        continue;
      }
    }

    if (!testMode && !realOutStream)
      continue;
    RINOK(extractCallback->PrepareOperation(askMode));

    outStreamSha1Spec->SetStream(realOutStream);
    realOutStream.Release();

    Int32 opRes = NExtract::NOperationResult::kOK;
    #ifdef XAR_SHOW_RAW
    if (index == _files.Size())
    {
      outStreamSha1Spec->Init(false);
      outStreamLimSpec->Init(_xmlLen);
      RINOK(WriteStream(outStream, _xml, _xmlLen));
      currentPackSize = currentUnpSize = _xmlLen;
    }
    else
    #endif
    {
      const CFile &item = _files[index];
      if (item.HasData)
      {
        currentPackSize = item.PackSize;
        currentUnpSize = item.Size;
        
        RINOK(_inStream->Seek(_dataStartPos + item.Offset, STREAM_SEEK_SET, NULL));
        inStreamSpec->Init(item.PackSize);
        outStreamSha1Spec->Init(item.Sha1IsDefined);
        outStreamLimSpec->Init(item.Size);
        HRESULT res = S_OK;
        
        ICompressCoder *coder = NULL;
        if (item.IsCopyMethod())
          if (item.PackSize == item.Size)
            coder = copyCoder;
          else
            opRes = NExtract::NOperationResult::kUnsupportedMethod;
        else if (item.Method == METHOD_NAME_ZLIB)
          coder = zlibCoder;
        else if (item.Method == "bzip2")
          coder = bzip2Coder;
        else
          opRes = NExtract::NOperationResult::kUnsupportedMethod;
        
        if (coder)
          res = coder->Code(inStream, outStream, NULL, NULL, progress);
        
        if (res != S_OK)
        {
          if (!outStreamLimSpec->IsFinishedOK())
            opRes = NExtract::NOperationResult::kDataError;
          else if (res != S_FALSE)
            return res;
          if (opRes == NExtract::NOperationResult::kOK)
            opRes = NExtract::NOperationResult::kDataError;
        }

        if (opRes == NExtract::NOperationResult::kOK)
        {
          if (outStreamLimSpec->IsFinishedOK() &&
              outStreamSha1Spec->GetSize() == item.Size)
          {
            if (!outStreamLimSpec->IsFinishedOK())
            {
              opRes = NExtract::NOperationResult::kDataError;
            }
            else if (item.Sha1IsDefined)
            {
              Byte digest[SHA1_DIGEST_SIZE];
              outStreamSha1Spec->Final(digest);
              if (memcmp(digest, item.Sha1, SHA1_DIGEST_SIZE) != 0)
                opRes = NExtract::NOperationResult::kCRCError;
            }
          }
          else
            opRes = NExtract::NOperationResult::kDataError;
        }
      }
    }
    outStreamSha1Spec->ReleaseStream();
    RINOK(extractCallback->SetOperationResult(opRes));
  }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::GetStream(UInt32 index, ISequentialInStream **stream)
{
  *stream = NULL;
  COM_TRY_BEGIN
  #ifdef XAR_SHOW_RAW
  if (index == _files.Size())
  {
    Create_BufInStream_WithNewBuffer(_xml, _xmlLen, stream);
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

static const Byte k_Signature[] = { 'x', 'a', 'r', '!', 0, 0x1C };

REGISTER_ARC_I(
  "Xar", "xar pkg xip", 0, 0xE1,
  k_Signature,
  0,
  0,
  NULL)

}}

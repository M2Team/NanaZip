// CpioHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/MyLinux.h"
#include "../../Common/StringConvert.h"
#include "../../Common/StringToInt.h"
#include "../../Common/UTFConvert.h"

#include "../../Windows/PropVariant.h"
#include "../../Windows/TimeUtils.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"

#include "Common/ItemNameUtils.h"

using namespace NWindows;

namespace NArchive {
namespace NCpio {

static const Byte kMagicBin0 = 0xC7;
static const Byte kMagicBin1 = 0x71;

// #define MAGIC_ASCII { '0', '7', '0', '7', '0' }

static const Byte kMagicHex    = '1'; // New ASCII Format
static const Byte kMagicHexCrc = '2'; // New CRC Format
static const Byte kMagicOct    = '7'; // Portable ASCII Format

static const char * const kName_TRAILER = "TRAILER!!!";

static const unsigned k_BinRecord_Size = 2 + 8 * 2 + 2 * 4;
static const unsigned k_OctRecord_Size = 6 + 8 * 6 + 2 * 11;
static const unsigned k_HexRecord_Size = 6 + 13 * 8;

static const unsigned k_RecordSize_Max = k_HexRecord_Size;

  /*
  struct CBinRecord
  {
    unsigned short c_magic;
    short c_dev;
    unsigned short c_ino;
    unsigned short c_mode;
    unsigned short c_uid;
    unsigned short c_gid;
    unsigned short c_nlink;
    short c_rdev;
    unsigned short c_mtimes[2];
    unsigned short c_namesize;
    unsigned short c_filesizes[2];
  };

  struct CHexRecord
  {
    char Magic[6];
    char inode[8];
    char Mode[8];
    char UID[8];
    char GID[8];
    char nlink[8];
    char mtime[8];
    char Size[8]; // must be 0 for FIFOs and directories
    char DevMajor[8];
    char DevMinor[8];
    char RDevMajor[8];  //only valid for chr and blk special files
    char RDevMinor[8];  //only valid for chr and blk special files
    char NameSize[8]; // count includes terminating NUL in pathname
    char ChkSum[8];  // 0 for "new" portable format; for CRC format the sum of all the bytes in the file
  };
*/

enum EType
{
  k_Type_BinLe,
  k_Type_BinBe,
  k_Type_Oct,
  k_Type_Hex,
  k_Type_HexCrc
};

static const char * const k_Types[] =
{
    "Binary LE"
  , "Binary BE"
  , "Portable ASCII"
  , "New ASCII"
  , "New CRC"
};

struct CItem
{
  AString Name;
  UInt32 inode;
  UInt32 Mode;
  UInt32 UID;
  UInt32 GID;
  UInt64 Size;
  UInt32 MTime;

  UInt32 NumLinks;
  UInt32 DevMajor;
  UInt32 DevMinor;
  UInt32 RDevMajor;
  UInt32 RDevMinor;
  UInt32 ChkSum;

  UInt32 Align;
  EType Type;

  UInt32 HeaderSize;
  UInt64 HeaderPos;

  bool IsBin() const { return Type == k_Type_BinLe || Type == k_Type_BinBe; }
  bool IsCrcFormat() const { return Type == k_Type_HexCrc; }
  bool IsDir() const { return MY_LIN_S_ISDIR(Mode); }
  bool IsTrailer() const { return strcmp(Name, kName_TRAILER) == 0; }
  UInt64 GetDataPosition() const { return HeaderPos + HeaderSize; }
};

enum EErrorType
{
  k_ErrorType_OK,
  k_ErrorType_Corrupted,
  k_ErrorType_UnexpectedEnd
};

struct CInArchive
{
  ISequentialInStream *Stream;
  UInt64 Processed;
  
  HRESULT Read(void *data, size_t *size);
  HRESULT GetNextItem(CItem &item, EErrorType &errorType);
};

HRESULT CInArchive::Read(void *data, size_t *size)
{
  HRESULT res = ReadStream(Stream, data, size);
  Processed += *size;
  return res;
}

static bool ReadHex(const Byte *p, UInt32 &resVal)
{
  char sz[16];
  memcpy(sz, p, 8);
  sz[8] = 0;
  const char *end;
  resVal = ConvertHexStringToUInt32(sz, &end);
  return (unsigned)(end - sz) == 8;
}

static bool ReadOct6(const Byte *p, UInt32 &resVal)
{
  char sz[16];
  memcpy(sz, p, 6);
  sz[6] = 0;
  const char *end;
  resVal = ConvertOctStringToUInt32(sz, &end);
  return (unsigned)(end - sz) == 6;
}

static bool ReadOct11(const Byte *p, UInt64 &resVal)
{
  char sz[16];
  memcpy(sz, p, 11);
  sz[11] = 0;
  const char *end;
  resVal = ConvertOctStringToUInt64(sz, &end);
  return (unsigned)(end - sz) == 11;
}


#define READ_HEX(y) { if (!ReadHex(p2, y)) return S_OK; p2 += 8; }
#define READ_OCT_6(y) { if (!ReadOct6(p2, y)) return S_OK; p2 += 6; }
#define READ_OCT_11(y) { if (!ReadOct11(p2, y)) return S_OK; p2 += 11; }

static UInt32 GetAlignedSize(UInt32 size, UInt32 align)
{
  while ((size & (align - 1)) != 0)
    size++;
  return size;
}

static UInt16 Get16(const Byte *p, bool be) { if (be) return GetBe16(p); return GetUi16(p); }
static UInt32 Get32(const Byte *p, bool be) { return ((UInt32)Get16(p, be) << 16) + Get16(p + 2, be); }

#define G16(offs, v) v = Get16(p + (offs), be)
#define G32(offs, v) v = Get32(p + (offs), be)

static const unsigned kNameSizeMax = 1 << 12;

API_FUNC_static_IsArc IsArc_Cpio(const Byte *p, size_t size)
{
  if (size < k_BinRecord_Size)
    return k_IsArc_Res_NEED_MORE;

  UInt32 nameSize;
  UInt32 numLinks;
  if (p[0] == '0')
  {
    if (p[1] != '7' ||
        p[2] != '0' ||
        p[3] != '7' ||
        p[4] != '0')
      return k_IsArc_Res_NO;
    if (p[5] == '7')
    {
      if (size < k_OctRecord_Size)
        return k_IsArc_Res_NEED_MORE;
      for (unsigned i = 6; i < k_OctRecord_Size; i++)
      {
        char c = p[i];
        if (c < '0' || c > '7')
          return k_IsArc_Res_NO;
      }
      ReadOct6(p + 6 * 6, numLinks);
      ReadOct6(p + 8 * 6 + 11, nameSize);
    }
    else if (p[5] == '1' || p[5] == '2')
    {
      if (size < k_HexRecord_Size)
        return k_IsArc_Res_NEED_MORE;
      for (unsigned i = 6; i < k_HexRecord_Size; i++)
      {
        char c = p[i];
        if ((c < '0' || c > '9') &&
            (c < 'A' || c > 'F') &&
            (c < 'a' || c > 'f'))
          return k_IsArc_Res_NO;
      }
      ReadHex(p + 6 +  4 * 8, numLinks);
      ReadHex(p + 6 + 11 * 8, nameSize);
    }
    else
      return k_IsArc_Res_NO;
  }
  else
  {
    UInt32 rDevMinor;
    if (p[0] == kMagicBin0 && p[1] == kMagicBin1)
    {
      numLinks = GetUi16(p + 12);
      rDevMinor = GetUi16(p + 14);
      nameSize = GetUi16(p + 20);
    }
    else if (p[0] == kMagicBin1 && p[1] == kMagicBin0)
    {
      numLinks = GetBe16(p + 12);
      rDevMinor = GetBe16(p + 14);
      nameSize = GetBe16(p + 20);
    }
    else
      return k_IsArc_Res_NO;

    if (rDevMinor != 0)
      return k_IsArc_Res_NO;
    if (nameSize > (1 << 8))
      return k_IsArc_Res_NO;
  }
  // 20.03: some cpio files have (numLinks == 0).
  // if (numLinks == 0) return k_IsArc_Res_NO;
  if (numLinks >= (1 << 10))
    return k_IsArc_Res_NO;
  if (nameSize == 0 || nameSize > kNameSizeMax)
    return k_IsArc_Res_NO;
  return k_IsArc_Res_YES;
}
}

#define READ_STREAM(_dest_, _size_) \
  { size_t processed = (_size_); RINOK(Read(_dest_, &processed)); \
if (processed != (_size_)) { errorType = k_ErrorType_UnexpectedEnd; return S_OK; } }

HRESULT CInArchive::GetNextItem(CItem &item, EErrorType &errorType)
{
  errorType = k_ErrorType_Corrupted;

  Byte p[k_RecordSize_Max];

  READ_STREAM(p, k_BinRecord_Size)

  UInt32 nameSize;

  if (p[0] != '0')
  {
    bool be;
         if (p[0] == kMagicBin0 && p[1] == kMagicBin1) { be = false; item.Type = k_Type_BinLe; }
    else if (p[0] == kMagicBin1 && p[1] == kMagicBin0) { be = true;  item.Type = k_Type_BinBe; }
    else return S_FALSE;

    item.Align = 2;
    item.DevMajor = 0;
    item.RDevMajor =0;
    item.ChkSum = 0;

    G16(2, item.DevMinor);
    G16(4, item.inode);
    G16(6, item.Mode);
    G16(8, item.UID);
    G16(10, item.GID);
    G16(12, item.NumLinks);
    G16(14, item.RDevMinor);
    G32(16, item.MTime);
    G16(20, nameSize);
    G32(22, item.Size);

    /*
    if (item.RDevMinor != 0)
      return S_FALSE;
    */

    item.HeaderSize = GetAlignedSize(nameSize + k_BinRecord_Size, item.Align);
    nameSize = item.HeaderSize - k_BinRecord_Size;
  }
  else
  {
    if (p[1] != '7' ||
        p[2] != '0' ||
        p[3] != '7' ||
        p[4] != '0')
      return S_FALSE;
    if (p[5] == kMagicOct)
    {
      item.Type = k_Type_Oct;
      READ_STREAM(p + k_BinRecord_Size, k_OctRecord_Size - k_BinRecord_Size)
      item.Align = 1;
      item.DevMajor = 0;
      item.RDevMajor = 0;

      const Byte *p2 = p + 6;
      READ_OCT_6(item.DevMinor);
      READ_OCT_6(item.inode);
      READ_OCT_6(item.Mode);
      READ_OCT_6(item.UID);
      READ_OCT_6(item.GID);
      READ_OCT_6(item.NumLinks);
      READ_OCT_6(item.RDevMinor);
      {
        UInt64 mTime64;
        READ_OCT_11(mTime64);
        item.MTime = 0;
        if (mTime64 < (UInt32)(Int32)-1)
          item.MTime = (UInt32)mTime64;
      }
      READ_OCT_6(nameSize);
      READ_OCT_11(item.Size);  // ?????
      item.HeaderSize = GetAlignedSize(nameSize + k_OctRecord_Size, item.Align);
      nameSize = item.HeaderSize - k_OctRecord_Size;
    }
    else
    {
      if (p[5] == kMagicHex)
        item.Type = k_Type_Hex;
      else if (p[5] == kMagicHexCrc)
        item.Type = k_Type_HexCrc;
      else
        return S_FALSE;

      READ_STREAM(p + k_BinRecord_Size, k_HexRecord_Size - k_BinRecord_Size)

      item.Align = 4;

      const Byte *p2 = p + 6;
      READ_HEX(item.inode);
      READ_HEX(item.Mode);
      READ_HEX(item.UID);
      READ_HEX(item.GID);
      READ_HEX(item.NumLinks);
      READ_HEX(item.MTime);
      {
        UInt32 size32;
        READ_HEX(size32);
        item.Size = size32;
      }
      READ_HEX(item.DevMajor);
      READ_HEX(item.DevMinor);
      READ_HEX(item.RDevMajor);
      READ_HEX(item.RDevMinor);
      READ_HEX(nameSize);
      READ_HEX(item.ChkSum);
      if (nameSize >= kNameSizeMax)
        return S_OK;
      item.HeaderSize = GetAlignedSize(nameSize + k_HexRecord_Size, item.Align);
      nameSize = item.HeaderSize - k_HexRecord_Size;
    }
  }
  if (nameSize > kNameSizeMax)
    return S_FALSE;
  if (nameSize == 0 || nameSize >= kNameSizeMax)
    return S_OK;
  char *s = item.Name.GetBuf(nameSize);
  size_t processedSize = nameSize;
  RINOK(Read(s, &processedSize));
  item.Name.ReleaseBuf_CalcLen(nameSize);
  if (processedSize != nameSize)
  {
    errorType = k_ErrorType_UnexpectedEnd;
    return S_OK;
  }
  errorType = k_ErrorType_OK;
  return S_OK;
}

class CHandler:
  public IInArchive,
  public IInArchiveGetStream,
  public CMyUnknownImp
{
  CObjectVector<CItem> _items;
  CMyComPtr<IInStream> _stream;
  UInt64 _phySize;
  EType _Type;
  EErrorType _error;
  bool _isArc;
public:
  MY_UNKNOWN_IMP2(IInArchive, IInArchiveGetStream)
  INTERFACE_IInArchive(;)
  STDMETHOD(GetStream)(UInt32 index, ISequentialInStream **stream);
};

static const Byte kArcProps[] =
{
  kpidSubType
};

static const Byte kProps[] =
{
  kpidPath,
  kpidIsDir,
  kpidSize,
  kpidMTime,
  kpidPosixAttrib,
  kpidLinks
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidSubType: prop = k_Types[(unsigned)_Type]; break;
    case kpidPhySize: prop = _phySize; break;
    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc)
        v |= kpv_ErrorFlags_IsNotArc;
      switch (_error)
      {
        case k_ErrorType_UnexpectedEnd: v |= kpv_ErrorFlags_UnexpectedEnd; break;
        case k_ErrorType_Corrupted: v |= kpv_ErrorFlags_HeadersError; break;
        case k_ErrorType_OK:
        default:
          break;
      }
      prop = v;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


STDMETHODIMP CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *callback)
{
  COM_TRY_BEGIN
  {
    Close();
    
    UInt64 endPos = 0;

    RINOK(stream->Seek(0, STREAM_SEEK_END, &endPos));
    RINOK(stream->Seek(0, STREAM_SEEK_SET, NULL));
    if (callback)
    {
      RINOK(callback->SetTotal(NULL, &endPos));
    }

    _items.Clear();
    CInArchive arc;

    arc.Stream = stream;
    arc.Processed = 0;

    for (;;)
    {
      CItem item;
      item.HeaderPos = arc.Processed;
      HRESULT result = arc.GetNextItem(item, _error);
      if (result == S_FALSE)
        return S_FALSE;
      if (result != S_OK)
        return S_FALSE;
      if (_error != k_ErrorType_OK)
      {
        if (_error == k_ErrorType_Corrupted)
          arc.Processed = item.HeaderPos;
        break;
      }
      if (_items.IsEmpty())
        _Type = item.Type;
      else if (_items.Back().Type != item.Type)
      {
        _error = k_ErrorType_Corrupted;
        arc.Processed = item.HeaderPos;
        break;
      }
      if (item.IsTrailer())
        break;
      
      _items.Add(item);

      {
        // archive.SkipDataRecords(item.Size, item.Align);
        UInt64 dataSize = item.Size;
        UInt32 align = item.Align;
        while ((dataSize & (align - 1)) != 0)
          dataSize++;

        // _error = k_ErrorType_UnexpectedEnd; break;

        arc.Processed += dataSize;
        if (arc.Processed > endPos)
        {
          _error = k_ErrorType_UnexpectedEnd;
          break;
        }

        UInt64 newPostion;
        RINOK(stream->Seek(dataSize, STREAM_SEEK_CUR, &newPostion));
        if (arc.Processed != newPostion)
          return E_FAIL;
      }

      if (callback && (_items.Size() & 0xFF) == 0)
      {
        UInt64 numFiles = _items.Size();
        RINOK(callback->SetCompleted(&numFiles, &item.HeaderPos));
      }
    }
    _phySize = arc.Processed;
    if (_error != k_ErrorType_OK)
    {
      if (_items.Size() == 0)
        return S_FALSE;
      if (_items.Size() == 1 && _items[0].IsBin())
      {
        // probably it's false detected archive. So we return error
        return S_FALSE;
      }
    }
    else
    {
      // Read tailing zeros.
      // Most of cpio files use 512-bytes aligned zeros
      // rare case: 4K/8K aligment is possible also
      const unsigned kTailSize_MAX = 1 << 9;
      Byte buf[kTailSize_MAX];
      
      unsigned pos = (unsigned)arc.Processed & (kTailSize_MAX - 1);
      if (pos != 0) // use this check to support 512 bytes alignment only
      for (;;)
      {
        unsigned rem = kTailSize_MAX - pos;
        size_t processed = rem;
        RINOK(ReadStream(stream, buf + pos, &processed));
        if (processed != rem)
          break;
        
        for (; pos < kTailSize_MAX && buf[pos] == 0; pos++)
        {}
        if (pos != kTailSize_MAX)
          break;
        _phySize += processed;
        pos = 0;

        //       use break to support 512   bytes alignment zero tail
        // don't use break to support 512*n bytes alignment zero tail
        break;
      }
    }
    
    _isArc = true;
    _stream = stream;
  }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::Close()
{
  _items.Clear();
  _stream.Release();
  _phySize = 0;
  _Type = k_Type_BinLe;
  _isArc = false;
  _error = k_ErrorType_OK;
  return S_OK;
}

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = _items.Size();
  return S_OK;
}

STDMETHODIMP CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  const CItem &item = _items[index];

  switch (propID)
  {
    case kpidPath:
    {
      UString res;
      bool needConvert = true;
      #ifdef _WIN32
      // if (
      ConvertUTF8ToUnicode(item.Name, res);
      // )
        needConvert = false;
      #endif
      if (needConvert)
        res = MultiByteToUnicodeString(item.Name, CP_OEMCP);
      prop = NItemName::GetOsPath(res);
      break;
    }
    case kpidIsDir: prop = item.IsDir(); break;
    case kpidSize:
    case kpidPackSize:
      prop = (UInt64)item.Size;
      break;
    case kpidMTime:
    {
      if (item.MTime != 0)
      {
        FILETIME utc;
        NTime::UnixTimeToFileTime(item.MTime, utc);
        prop = utc;
      }
      break;
    }
    case kpidPosixAttrib: prop = item.Mode; break;
    case kpidLinks: prop = item.NumLinks; break;
    /*
    case kpidinode:  prop = item.inode; break;
    case kpidiChkSum:  prop = item.ChkSum; break;
    */
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

class COutStreamWithSum:
  public ISequentialOutStream,
  public CMyUnknownImp
{
  CMyComPtr<ISequentialOutStream> _stream;
  UInt64 _size;
  UInt32 _crc;
  bool _calculate;
public:
  MY_UNKNOWN_IMP
  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
  void SetStream(ISequentialOutStream *stream) { _stream = stream; }
  void ReleaseStream() { _stream.Release(); }
  void Init(bool calculate = true)
  {
    _size = 0;
    _calculate = calculate;
    _crc = 0;
  }
  void EnableCalc(bool calculate) { _calculate = calculate; }
  void InitCRC() { _crc = 0; }
  UInt64 GetSize() const { return _size; }
  UInt32 GetCRC() const { return _crc; }
};

STDMETHODIMP COutStreamWithSum::Write(const void *data, UInt32 size, UInt32 *processedSize)
{
  HRESULT result = S_OK;
  if (_stream)
    result = _stream->Write(data, size, &size);
  if (_calculate)
  {
    UInt32 crc = 0;
    for (UInt32 i = 0; i < size; i++)
      crc += (UInt32)(((const Byte *)data)[i]);
    _crc += crc;
  }
  if (processedSize)
    *processedSize = size;
  return result;
}

STDMETHODIMP CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback)
{
  COM_TRY_BEGIN
  bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _items.Size();
  if (numItems == 0)
    return S_OK;
  UInt64 totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
    totalSize += _items[allFilesMode ? i : indices[i]].Size;
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

  COutStreamWithSum *outStreamSumSpec = new COutStreamWithSum;
  CMyComPtr<ISequentialOutStream> outStreamSum(outStreamSumSpec);

  for (i = 0; i < numItems; i++)
  {
    lps->InSize = lps->OutSize = currentTotalSize;
    RINOK(lps->SetCur());
    CMyComPtr<ISequentialOutStream> outStream;
    Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    Int32 index = allFilesMode ? i : indices[i];
    const CItem &item = _items[index];
    RINOK(extractCallback->GetStream(index, &outStream, askMode));
    currentTotalSize += item.Size;
    if (item.IsDir())
    {
      RINOK(extractCallback->PrepareOperation(askMode));
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK));
      continue;
    }
    if (!testMode && !outStream)
      continue;
    outStreamSumSpec->Init(item.IsCrcFormat());
    outStreamSumSpec->SetStream(outStream);
    outStream.Release();

    RINOK(extractCallback->PrepareOperation(askMode));
    RINOK(_stream->Seek(item.GetDataPosition(), STREAM_SEEK_SET, NULL));
    streamSpec->Init(item.Size);
    RINOK(copyCoder->Code(inStream, outStreamSum, NULL, NULL, progress));
    outStreamSumSpec->ReleaseStream();
    Int32 res = NExtract::NOperationResult::kDataError;
    if (copyCoderSpec->TotalSize == item.Size)
    {
      res = NExtract::NOperationResult::kOK;
      if (item.IsCrcFormat() && item.ChkSum != outStreamSumSpec->GetCRC())
        res = NExtract::NOperationResult::kCRCError;
    }
    RINOK(extractCallback->SetOperationResult(res));
  }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::GetStream(UInt32 index, ISequentialInStream **stream)
{
  COM_TRY_BEGIN
  const CItem &item = _items[index];
  return CreateLimitedInStream(_stream, item.GetDataPosition(), item.Size, stream);
  COM_TRY_END
}

static const Byte k_Signature[] = {
    5, '0', '7', '0', '7', '0',
    2, kMagicBin0, kMagicBin1,
    2, kMagicBin1, kMagicBin0 };

REGISTER_ARC_I(
  "Cpio", "cpio", 0, 0xED,
  k_Signature,
  0,
  NArcInfoFlags::kMultiSignature,
  IsArc_Cpio)

}}

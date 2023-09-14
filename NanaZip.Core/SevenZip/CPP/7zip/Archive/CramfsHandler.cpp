// CramfsHandler.cpp

#include "StdAfx.h"

#include "../../../C/7zCrc.h"
#include "../../../C/Alloc.h"
#include "../../../C/CpuArch.h"
#include "../../../C/LzmaDec.h"

#include "../../Common/ComTry.h"
#include "../../Common/MyLinux.h"
#include "../../Common/StringConvert.h"

#include "../../Windows/PropVariantUtils.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"
#include "../Compress/ZlibDecoder.h"

namespace NArchive {
namespace NCramfs {

static const Byte kSignature[] =
  { 'C','o','m','p','r','e','s','s','e','d',' ','R','O','M','F','S' };

static const UInt32 kArcSizeMax = (256 + 16) << 20;
static const UInt32 kNumFilesMax = (1 << 19);
static const unsigned kNumDirLevelsMax = (1 << 8);

static const UInt32 kHeaderSize = 0x40;
static const unsigned kHeaderNameSize = 16;
static const UInt32 kNodeSize = 12;

static const UInt32 kFlag_FsVer2 = (1 << 0);

static const unsigned k_Flags_BlockSize_Shift = 11;
static const unsigned k_Flags_BlockSize_Mask = 7;
static const unsigned k_Flags_Method_Shift = 14;
static const unsigned k_Flags_Method_Mask = 3;

/*
  There is possible collision in flags:
    - Original CramFS writes 0 in method field. But it uses ZLIB.
    - Modified CramFS writes 0 in method field for "NONE" compression?
  How to solve that collision?
*/

#define k_Flags_Method_NONE 0
#define k_Flags_Method_ZLIB 1
#define k_Flags_Method_LZMA 2

static const char * const k_Methods[] =
{
    "Copy"
  , "ZLIB"
  , "LZMA"
  , "Unknown"
};

static const CUInt32PCharPair k_Flags[] =
{
  { 0, "Ver2" },
  { 1, "SortedDirs" },
  { 8, "Holes" },
  { 9, "WrongSignature" },
  { 10, "ShiftedRootOffset" }
};

static const unsigned kBlockSizeLog = 12;

/*
struct CNode
{
  UInt16 Mode;
  UInt16 Uid;
  UInt32 Size;
  Byte Gid;
  UInt32 NameLen;
  UInt32 Offset;

  void Parse(const Byte *p)
  {
    Mode = GetUi16(p);
    Uid = GetUi16(p + 2);
    Size = Get32(p + 4) & 0xFFFFFF;
    Gid = p[7];
    NameLen = p[8] & 0x3F;
    Offset = Get32(p + 8) >> 6;
  }
};
*/

#define Get32(p) (be ? GetBe32(p) : GetUi32(p))

static UInt32 GetMode(const Byte *p, bool be) { return be ? GetBe16(p) : GetUi16(p); }
static bool IsDir(const Byte *p, bool be) { return MY_LIN_S_ISDIR(GetMode(p, be)); }

static UInt32 GetSize(const Byte *p, bool be)
{
  if (be)
    return GetBe32(p + 4) >> 8;
  else
    return GetUi32(p + 4) & 0xFFFFFF;
}

static UInt32 GetNameLen(const Byte *p, bool be)
{
  if (be)
    return (p[8] & 0xFC);
  else
    return ((UInt32)p[8] & (UInt32)0x3F) << 2;
}

static UInt32 GetOffset(const Byte *p, bool be)
{
  if (be)
    return (GetBe32(p + 8) & 0x03FFFFFF) << 2;
  else
    return GetUi32(p + 8) >> 6 << 2;
}

struct CItem
{
  UInt32 Offset;
  int Parent;
};

struct CHeader
{
  bool be;
  UInt32 Size;
  UInt32 Flags;
  // UInt32 Future;
  UInt32 Crc;
  // UInt32 Edition;
  UInt32 NumBlocks;
  UInt32 NumFiles;
  char Name[kHeaderNameSize];

  bool Parse(const Byte *p)
  {
    if (memcmp(p + 16, kSignature, Z7_ARRAY_SIZE(kSignature)) != 0)
      return false;
    switch (GetUi32(p))
    {
      case 0x28CD3D45: be = false; break;
      case 0x453DCD28: be = true; break;
      default: return false;
    }
    Size = Get32(p + 4);
    Flags = Get32(p + 8);
    // Future = Get32(p + 0xC);
    Crc = Get32(p + 0x20);
    // Edition = Get32(p + 0x24);
    NumBlocks = Get32(p + 0x28);
    NumFiles = Get32(p + 0x2C);
    memcpy(Name, p + 0x30, kHeaderNameSize);
    return true;
  }

  bool IsVer2() const { return (Flags & kFlag_FsVer2) != 0; }
  unsigned GetBlockSizeShift() const { return (unsigned)(Flags >> k_Flags_BlockSize_Shift) & k_Flags_BlockSize_Mask; }
  unsigned GetMethod() const { return (unsigned)(Flags >> k_Flags_Method_Shift) & k_Flags_Method_Mask; }
};



Z7_CLASS_IMP_CHandler_IInArchive_1(
  IInArchiveGetStream
)
  CRecordVector<CItem> _items;
  CMyComPtr<IInStream> _stream;
  Byte *_data;
  UInt32 _size;
  UInt32 _headersSize;

  UInt32 _errorFlags;
  bool _isArc;

  CHeader _h;
  UInt32 _phySize;

  unsigned _method;
  unsigned _blockSizeLog;

  // Current file

  NCompress::NZlib::CDecoder *_zlibDecoderSpec;
  CMyComPtr<ICompressCoder> _zlibDecoder;

  CBufInStream *_inStreamSpec;
  CMyComPtr<ISequentialInStream> _inStream;

  CBufPtrSeqOutStream *_outStreamSpec;
  CMyComPtr<ISequentialOutStream> _outStream;

  UInt32 _curBlocksOffset;
  UInt32 _curNumBlocks;

  HRESULT OpenDir(int parent, UInt32 baseOffsetBase, unsigned level);
  HRESULT Open2(IInStream *inStream);
  AString GetPath(unsigned index) const;
  bool GetPackSize(unsigned index, UInt32 &res) const;
  void Free();

  UInt32 GetNumBlocks(UInt32 size) const
  {
    return (size + ((UInt32)1 << _blockSizeLog) - 1) >> _blockSizeLog;
  }

  void UpdatePhySize(UInt32 s)
  {
    if (_phySize < s)
      _phySize = s;
  }

public:
  CHandler(): _data(NULL) {}
  ~CHandler() { Free(); }
  HRESULT ReadBlock(UInt64 blockIndex, Byte *dest, size_t blockSize);
};

static const Byte kProps[] =
{
  kpidPath,
  kpidIsDir,
  kpidSize,
  kpidPackSize,
  kpidPosixAttrib
  // kpidOffset
};

static const Byte kArcProps[] =
{
  kpidVolumeName,
  kpidBigEndian,
  kpidCharacts,
  kpidClusterSize,
  kpidMethod,
  kpidHeadersSize,
  kpidNumSubFiles,
  kpidNumBlocks
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

HRESULT CHandler::OpenDir(int parent, UInt32 baseOffset, unsigned level)
{
  const Byte *p = _data + baseOffset;
  bool be = _h.be;
  if (!IsDir(p, be))
    return S_OK;
  UInt32 offset = GetOffset(p, be);
  UInt32 size = GetSize(p, be);
  if (offset == 0 && size == 0)
    return S_OK;
  UInt32 end = offset + size;
  if (offset < kHeaderSize || end > _size || level > kNumDirLevelsMax)
    return S_FALSE;
  UpdatePhySize(end);
  if (end > _headersSize)
    _headersSize = end;

  unsigned startIndex = _items.Size();
  
  while (size != 0)
  {
    if (size < kNodeSize || (UInt32)_items.Size() >= kNumFilesMax)
      return S_FALSE;
    CItem item;
    item.Parent = parent;
    item.Offset = offset;
    _items.Add(item);
    UInt32 nodeLen = kNodeSize + GetNameLen(_data + offset, be);
    if (size < nodeLen)
      return S_FALSE;
    offset += nodeLen;
    size -= nodeLen;
  }

  unsigned endIndex = _items.Size();
  for (unsigned i = startIndex; i < endIndex; i++)
  {
    RINOK(OpenDir((int)i, _items[i].Offset, level + 1))
  }
  return S_OK;
}

HRESULT CHandler::Open2(IInStream *inStream)
{
  Byte buf[kHeaderSize];
  RINOK(ReadStream_FALSE(inStream, buf, kHeaderSize))
  if (!_h.Parse(buf))
    return S_FALSE;
  _method = k_Flags_Method_ZLIB;
  _blockSizeLog = kBlockSizeLog;
  _phySize = kHeaderSize;
  if (_h.IsVer2())
  {
    _method = _h.GetMethod();
    // FIT IT. Now we don't know correct way to work with collision in method field.
    if (_method == k_Flags_Method_NONE)
      _method = k_Flags_Method_ZLIB;
    _blockSizeLog = kBlockSizeLog + _h.GetBlockSizeShift();
    if (_h.Size < kHeaderSize || _h.Size > kArcSizeMax || _h.NumFiles > kNumFilesMax)
      return S_FALSE;
    _phySize = _h.Size;
  }
  else
  {
    UInt64 size;
    RINOK(InStream_GetSize_SeekToEnd(inStream, size))
    if (size > kArcSizeMax)
      size = kArcSizeMax;
    _h.Size = (UInt32)size;
    RINOK(InStream_SeekSet(inStream, kHeaderSize))
  }
  _data = (Byte *)MidAlloc(_h.Size);
  if (!_data)
    return E_OUTOFMEMORY;
  memcpy(_data, buf, kHeaderSize);
  size_t processed = _h.Size - kHeaderSize;
  RINOK(ReadStream(inStream, _data + kHeaderSize, &processed))
  if (processed < kNodeSize)
    return S_FALSE;
  _size = kHeaderSize + (UInt32)processed;
  if (_h.IsVer2())
  {
    if (_size != _h.Size)
      _errorFlags = kpv_ErrorFlags_UnexpectedEnd;
    else
    {
      SetUi32(_data + 0x20, 0)
      if (CrcCalc(_data, _h.Size) != _h.Crc)
      {
        _errorFlags = kpv_ErrorFlags_HeadersError;
        // _errorMessage = "CRC error";
      }
    }
    if (_h.NumFiles >= 1)
      _items.ClearAndReserve(_h.NumFiles - 1);
  }
  
  RINOK(OpenDir(-1, kHeaderSize, 0))
  
  if (!_h.IsVer2())
  {
    FOR_VECTOR (i, _items)
    {
      const CItem &item = _items[i];
      const Byte *p = _data + item.Offset;
      bool be = _h.be;
      if (IsDir(p, be))
        continue;
      UInt32 offset = GetOffset(p, be);
      if (offset < kHeaderSize)
        continue;
      UInt32 numBlocks = GetNumBlocks(GetSize(p, be));
      if (numBlocks == 0)
        continue;
      UInt32 start = offset + numBlocks * 4;
      if (start > _size)
        continue;
      UInt32 end = Get32(_data + start - 4);
      if (end >= start)
        UpdatePhySize(end);
    }

    // Read tailing zeros. Most cramfs archives use 4096-bytes aligned zeros
    const UInt32 kTailSize_MAX = 1 << 12;
    UInt32 endPos = (_phySize + kTailSize_MAX - 1) & ~(kTailSize_MAX - 1);
    if (endPos > _size)
      endPos = _size;
    UInt32 pos;
    for (pos = _phySize; pos < endPos && _data[pos] == 0; pos++);
    if (pos == endPos)
      _phySize = endPos;
  }
  return S_OK;
}

AString CHandler::GetPath(unsigned index) const
{
  unsigned len = 0;
  unsigned indexMem = index;
  for (;;)
  {
    const CItem &item = _items[index];
    const Byte *p = _data + item.Offset;
    unsigned size = GetNameLen(p, _h.be);
    p += kNodeSize;
    unsigned i;
    for (i = 0; i < size && p[i]; i++);
    len += i + 1;
    index = (unsigned)item.Parent;
    if (item.Parent < 0)
      break;
  }
  len--;

  AString path;
  char *dest = path.GetBuf_SetEnd(len) + len;
  index = indexMem;
  for (;;)
  {
    const CItem &item = _items[index];
    const Byte *p = _data + item.Offset;
    unsigned size = GetNameLen(p, _h.be);
    p += kNodeSize;
    unsigned i;
    for (i = 0; i < size && p[i]; i++);
    dest -= i;
    memcpy(dest, p, i);
    index = (unsigned)item.Parent;
    if (item.Parent < 0)
      break;
    *(--dest) = CHAR_PATH_SEPARATOR;
  }
  return path;
}

bool CHandler::GetPackSize(unsigned index, UInt32 &res) const
{
  res = 0;
  const CItem &item = _items[index];
  const Byte *p = _data + item.Offset;
  const bool be = _h.be;
  const UInt32 offset = GetOffset(p, be);
  if (offset < kHeaderSize)
    return false;
  const UInt32 numBlocks = GetNumBlocks(GetSize(p, be));
  if (numBlocks == 0)
    return true;
  const UInt32 start = offset + numBlocks * 4;
  if (start > _size)
    return false;
  const UInt32 end = Get32(_data + start - 4);
  if (end < start)
    return false;
  res = end - start;
  return true;
}

Z7_COM7F_IMF(CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback * /* callback */))
{
  COM_TRY_BEGIN
  {
    Close();
    RINOK(Open2(stream))
    _isArc = true;
    _stream = stream;
  }
  return S_OK;
  COM_TRY_END
}

void CHandler::Free()
{
  MidFree(_data);
  _data = NULL;
}

Z7_COM7F_IMF(CHandler::Close())
{
  _isArc = false;
  _phySize = 0;
  _errorFlags = 0;
  _headersSize = 0;
  _items.Clear();
  _stream.Release();
  Free();
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
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidVolumeName:
    {
      char dest[kHeaderNameSize + 4];
      memcpy(dest, _h.Name, kHeaderNameSize);
      dest[kHeaderNameSize] = 0;
      prop = dest;
      break;
    }
    case kpidBigEndian: prop = _h.be; break;
    case kpidCharacts: FLAGS_TO_PROP(k_Flags, _h.Flags, prop); break;
    case kpidMethod: prop = k_Methods[_method]; break;
    case kpidClusterSize: prop = (UInt32)1 << _blockSizeLog; break;
    case kpidNumBlocks: if (_h.IsVer2()) prop = _h.NumBlocks; break;
    case kpidNumSubFiles: if (_h.IsVer2()) prop = _h.NumFiles; break;
    case kpidPhySize: prop = _phySize; break;
    case kpidHeadersSize: prop = _headersSize; break;
    case kpidErrorFlags:
    {
      UInt32 v = _errorFlags;
      if (!_isArc)
        v |= kpv_ErrorFlags_IsNotArc;
      prop = v;
      break;
    }
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
  const Byte *p = _data + item.Offset;
  bool be = _h.be;
  bool isDir = IsDir(p, be);
  switch (propID)
  {
    case kpidPath: prop = MultiByteToUnicodeString(GetPath(index), CP_OEMCP); break;
    case kpidIsDir: prop = isDir; break;
    // case kpidOffset: prop = (UInt32)GetOffset(p, be); break;
    case kpidSize: if (!isDir) prop = GetSize(p, be); break;
    case kpidPackSize:
      if (!isDir)
      {
        UInt32 size;
        if (GetPackSize(index, size))
          prop = size;
      }
      break;
    case kpidPosixAttrib: prop = (UInt32)GetMode(p, be); break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

class CCramfsInStream: public CCachedInStream
{
  HRESULT ReadBlock(UInt64 blockIndex, Byte *dest, size_t blockSize) Z7_override;
public:
  CHandler *Handler;
};

HRESULT CCramfsInStream::ReadBlock(UInt64 blockIndex, Byte *dest, size_t blockSize)
{
  return Handler->ReadBlock(blockIndex, dest, blockSize);
}

HRESULT CHandler::ReadBlock(UInt64 blockIndex, Byte *dest, size_t blockSize)
{
  if (_method == k_Flags_Method_ZLIB)
  {
    if (!_zlibDecoder)
    {
      _zlibDecoderSpec = new NCompress::NZlib::CDecoder();
      _zlibDecoder = _zlibDecoderSpec;
    }
  }
  else
  {
    if (_method != k_Flags_Method_LZMA)
    {
      // probably we must support no-compression archives here.
      return E_NOTIMPL;
    }
  }

  const bool be = _h.be;
  const Byte *p2 = _data + (_curBlocksOffset + (UInt32)blockIndex * 4);
  const UInt32 start = (blockIndex == 0 ? _curBlocksOffset + _curNumBlocks * 4: Get32(p2 - 4));
  const UInt32 end = Get32(p2);
  if (end < start || end > _size)
    return S_FALSE;
  const UInt32 inSize = end - start;

  if (_method == k_Flags_Method_LZMA)
  {
    const unsigned kLzmaHeaderSize = LZMA_PROPS_SIZE + 4;
    if (inSize < kLzmaHeaderSize)
      return S_FALSE;
    const Byte *p = _data + start;
    UInt32 destSize32 = GetUi32(p + LZMA_PROPS_SIZE);
    if (destSize32 > blockSize)
      return S_FALSE;
    SizeT destLen = destSize32;
    SizeT srcLen = inSize - kLzmaHeaderSize;
    ELzmaStatus status;
    SRes res = LzmaDecode(dest, &destLen, p + kLzmaHeaderSize, &srcLen,
        p, LZMA_PROPS_SIZE, LZMA_FINISH_END, &status, &g_Alloc);
    if (res != SZ_OK
        || (status != LZMA_STATUS_FINISHED_WITH_MARK &&
            status != LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK)
        || destLen != destSize32
        || srcLen != inSize - kLzmaHeaderSize)
      return S_FALSE;
    return S_OK;
  }

  if (!_inStream)
  {
    _inStreamSpec = new CBufInStream();
    _inStream = _inStreamSpec;
  }
  if (!_outStream)
  {
    _outStreamSpec = new CBufPtrSeqOutStream();
    _outStream = _outStreamSpec;
  }
  _inStreamSpec->Init(_data + start, inSize);
  _outStreamSpec->Init(dest, blockSize);
  RINOK(_zlibDecoder->Code(_inStream, _outStream, NULL, NULL, NULL))
  return (inSize == _zlibDecoderSpec->GetInputProcessedSize() &&
      _outStreamSpec->GetPos() == blockSize) ? S_OK : S_FALSE;
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
  bool be = _h.be;
  UInt64 totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    const Byte *p = _data + _items[allFilesMode ? i : indices[i]].Offset;
    if (!IsDir(p, be))
      totalSize += GetSize(p, be);
  }
  extractCallback->SetTotal(totalSize);

  UInt64 totalPackSize;
  totalSize = totalPackSize = 0;
  
  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder();
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  for (i = 0; i < numItems; i++)
  {
    lps->InSize = totalPackSize;
    lps->OutSize = totalSize;
    RINOK(lps->SetCur())
    CMyComPtr<ISequentialOutStream> outStream;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    const UInt32 index = allFilesMode ? i : indices[i];
    const CItem &item = _items[index];
    RINOK(extractCallback->GetStream(index, &outStream, askMode))
    const Byte *p = _data + item.Offset;

    if (IsDir(p, be))
    {
      RINOK(extractCallback->PrepareOperation(askMode))
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
      continue;
    }
    UInt32 curSize = GetSize(p, be);
    totalSize += curSize;
    UInt32 packSize;
    if (GetPackSize(index, packSize))
      totalPackSize += packSize;

    if (!testMode && !outStream)
      continue;
    RINOK(extractCallback->PrepareOperation(askMode))

    UInt32 offset = GetOffset(p, be);
    if (offset < kHeaderSize)
      curSize = 0;

    int res = NExtract::NOperationResult::kDataError;
    {
      CMyComPtr<ISequentialInStream> inSeqStream;
      HRESULT hres = GetStream(index, &inSeqStream);
      if (hres == E_OUTOFMEMORY)
        return E_OUTOFMEMORY;
      if (hres == S_FALSE || !inSeqStream)
        res = NExtract::NOperationResult::kUnsupportedMethod;
      else
      {
        RINOK(hres)
        {
          hres = copyCoder->Code(inSeqStream, outStream, NULL, NULL, progress);
          if (hres == S_OK)
          {
            if (copyCoderSpec->TotalSize == curSize)
              res = NExtract::NOperationResult::kOK;
          }
          else if (hres == E_NOTIMPL)
            res = NExtract::NOperationResult::kUnsupportedMethod;
          else if (hres != S_FALSE)
            return hres;
        }
      }
    }
    RINOK(extractCallback->SetOperationResult(res))
  }

  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetStream(UInt32 index, ISequentialInStream **stream))
{
  COM_TRY_BEGIN

  const CItem &item = _items[index];
  const Byte *p = _data + item.Offset;

  bool be = _h.be;
  if (IsDir(p, be))
    return E_FAIL;

  UInt32 size = GetSize(p, be);
  UInt32 numBlocks = GetNumBlocks(size);
  UInt32 offset = GetOffset(p, be);
  if (offset < kHeaderSize)
  {
    if (offset != 0)
      return S_FALSE;
    CBufInStream *streamSpec = new CBufInStream;
    CMyComPtr<IInStream> streamTemp = streamSpec;
    streamSpec->Init(NULL, 0);
    *stream = streamTemp.Detach();
    return S_OK;
  }

  if (offset + numBlocks * 4 > _size)
    return S_FALSE;
  UInt32 prev = offset;
  for (UInt32 i = 0; i < numBlocks; i++)
  {
    UInt32 next = Get32(_data + offset + i * 4);
    if (next < prev || next > _size)
      return S_FALSE;
    prev = next;
  }

  CCramfsInStream *streamSpec = new CCramfsInStream;
  CMyComPtr<IInStream> streamTemp = streamSpec;
  _curNumBlocks = numBlocks;
  _curBlocksOffset = offset;
  streamSpec->Handler = this;
  if (!streamSpec->Alloc(_blockSizeLog, 21 - _blockSizeLog))
    return E_OUTOFMEMORY;
  streamSpec->Init(size);
  *stream = streamTemp.Detach();

  return S_OK;
  COM_TRY_END
}

REGISTER_ARC_I(
  "CramFS", "cramfs", NULL, 0xD3,
  kSignature,
  16,
  0,
  NULL)

}}

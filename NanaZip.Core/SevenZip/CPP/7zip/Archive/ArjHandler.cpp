// ArjHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/StringConvert.h"

#include "../../Windows/PropVariant.h"
#include "../../Windows/PropVariantUtils.h"
#include "../../Windows/TimeUtils.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"
#include "../Compress/LzhDecoder.h"

#include "Common/ItemNameUtils.h"
#include "Common/OutStreamWithCRC.h"

namespace NCompress {
namespace NArj {
namespace NDecoder {

static const unsigned kMatchMinLen = 3;

static const UInt32 kWindowSize = 1 << 15; // must be >= (1 << 14)


Z7_CLASS_IMP_NOQIB_1(
  CCoder
  , ICompressCoder
)
  CLzOutWindow _outWindow;
  NBitm::CDecoder<CInBuffer> _inBitStream;

  class CCoderReleaser
  {
    CCoder *_coder;
  public:
    CCoderReleaser(CCoder *coder): _coder(coder) {}
    void Disable() { _coder = NULL; }
    ~CCoderReleaser() { if (_coder) _coder->_outWindow.Flush(); }
  };
  friend class CCoderReleaser;

  HRESULT CodeReal(UInt64 outSize, ICompressProgressInfo *progress);
public:
  bool FinishMode;

  CCoder(): FinishMode(false) {}
  UInt64 GetInputProcessedSize() const { return _inBitStream.GetProcessedSize(); }
};


HRESULT CCoder::CodeReal(UInt64 rem, ICompressProgressInfo *progress)
{
  const UInt32 kStep = 1 << 20;
  UInt64 next = 0;
  if (rem > kStep && progress)
    next = rem - kStep;

  while (rem != 0)
  {
    if (rem <= next)
    {
      if (_inBitStream.ExtraBitsWereRead())
        return S_FALSE;

      UInt64 packSize = _inBitStream.GetProcessedSize();
      UInt64 pos = _outWindow.GetProcessedSize();
      RINOK(progress->SetRatioInfo(&packSize, &pos))
      next = 0;
      if (rem > kStep)
        next = rem - kStep;
    }

    UInt32 len;
    
    {
      const unsigned kNumBits = 7 + 7;
      UInt32 val = _inBitStream.GetValue(kNumBits);
      
      if ((val & (1 << (kNumBits - 1))) == 0)
      {
        _outWindow.PutByte((Byte)(val >> 5));
        _inBitStream.MovePos(1 + 8);
        rem--;
        continue;
      }

      UInt32 mask = 1 << (kNumBits - 2);
      unsigned w;

      for (w = 1; w < 7; w++, mask >>= 1)
        if ((val & mask) == 0)
          break;
      
      unsigned readBits = (w != 7 ? 1 : 0);
      readBits += w + w;
      len = (1 << w) - 1 + kMatchMinLen - 1 +
          (((val >> (kNumBits - readBits)) & ((1 << w) - 1)));
      _inBitStream.MovePos(readBits);
    }
    
    {
      const unsigned kNumBits = 4 + 13;
      UInt32 val = _inBitStream.GetValue(kNumBits);

      unsigned readBits = 1;
      unsigned w;
     
           if ((val & ((UInt32)1 << 16)) == 0) w = 9;
      else if ((val & ((UInt32)1 << 15)) == 0) w = 10;
      else if ((val & ((UInt32)1 << 14)) == 0) w = 11;
      else if ((val & ((UInt32)1 << 13)) == 0) w = 12;
      else { w = 13; readBits = 0; }

      readBits += w + w - 9;

      UInt32 dist = ((UInt32)1 << w) - (1 << 9) +
          (((val >> (kNumBits - readBits)) & ((1 << w) - 1)));
      _inBitStream.MovePos(readBits);

      if (len > rem)
        len = (UInt32)rem;

      if (!_outWindow.CopyBlock(dist, len))
        return S_FALSE;
      rem -= len;
    }
  }

  if (FinishMode)
  {
    if (_inBitStream.ReadAlignBits() != 0)
      return S_FALSE;
  }

  if (_inBitStream.ExtraBitsWereRead())
    return S_FALSE;

  return S_OK;
}



Z7_COM7F_IMF(CCoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 *outSize, ICompressProgressInfo *progress))
{
  try
  {
    if (!outSize)
      return E_INVALIDARG;
    
    if (!_outWindow.Create(kWindowSize))
      return E_OUTOFMEMORY;
    if (!_inBitStream.Create(1 << 17))
      return E_OUTOFMEMORY;
    
    _outWindow.SetStream(outStream);
    _outWindow.Init(false);
    _inBitStream.SetStream(inStream);
    _inBitStream.Init();
    
    CCoderReleaser coderReleaser(this);
    HRESULT res;
    {
      res = CodeReal(*outSize, progress);
      if (res != S_OK)
        return res;
    }
    
    coderReleaser.Disable();
    return _outWindow.Flush();
  }
  catch(const CInBufferException &e) { return e.ErrorCode; }
  catch(const CLzOutWindowException &e) { return e.ErrorCode; }
  catch(...) { return S_FALSE; }
}

}}}




using namespace NWindows;

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)

namespace NArchive {
namespace NArj {

static const unsigned kBlockSizeMin = 30;
static const unsigned kBlockSizeMax = 2600;

static const Byte kSig0 = 0x60;
static const Byte kSig1 = 0xEA;

namespace NCompressionMethod
{
  enum
  {
    kStored = 0,
    kCompressed1a = 1,
    kCompressed1b = 2,
    kCompressed1c = 3,
    kCompressed2 = 4,
    kNoDataNoCRC = 8,
    kNoData = 9
  };
}

namespace NFileType
{
  enum
  {
    kBinary = 0,
    k7BitText,
    kArchiveHeader,
    kDirectory,
    kVolumeLablel,
    kChapterLabel
  };
}

namespace NFlags
{
  const Byte kGarbled  = 1 << 0;
  // const Byte kAnsiPage = 1 << 1; // or (OLD_SECURED_FLAG) obsolete
  const Byte kVolume   = 1 << 2;
  const Byte kExtFile  = 1 << 3;
  // const Byte kPathSym  = 1 << 4;
  // const Byte kBackup   = 1 << 5; // obsolete
  // const Byte kSecured  = 1 << 6;
  // const Byte kDualName = 1 << 7;
}

namespace NHostOS
{
  enum EEnum
  {
    kMSDOS = 0,  // MS-DOS, OS/2, Win32, pkarj 2.50 (FAT / VFAT / FAT32)
    kPRIMOS,
    kUnix,
    kAMIGA,
    kMac,
    kOS_2,
    kAPPLE_GS,
    kAtari_ST,
    kNext,
    kVAX_VMS,
    kWIN95
  };
}

static const char * const kHostOS[] =
{
    "MSDOS"
  , "PRIMOS"
  , "UNIX"
  , "AMIGA"
  , "MAC"
  , "OS/2"
  , "APPLE GS"
  , "ATARI ST"
  , "NEXT"
  , "VAX VMS"
  , "WIN95"
};

struct CArcHeader
{
  // Byte ArchiverVersion;
  // Byte ExtractVersion;
  Byte HostOS;
  // Byte Flags;
  // Byte SecuryVersion;
  // Byte FileType;
  // Byte Reserved;
  UInt32 CTime;
  UInt32 MTime;
  UInt32 ArchiveSize;
  // UInt32 SecurPos;
  // UInt16 FilespecPosInFilename;
  UInt16 SecurSize;
  // Byte EncryptionVersion;
  // Byte LastChapter;
  AString Name;
  AString Comment;
  
  HRESULT Parse(const Byte *p, unsigned size);
};

API_FUNC_static_IsArc IsArc_Arj(const Byte *p, size_t size)
{
  if (size < kBlockSizeMin + 4)
    return k_IsArc_Res_NEED_MORE;
  if (p[0] != kSig0 || p[1] != kSig1)
    return k_IsArc_Res_NO;
  UInt32 blockSize = Get16(p + 2);
  if (blockSize < kBlockSizeMin ||
      blockSize > kBlockSizeMax)
    return k_IsArc_Res_NO;

  p += 4;
  size -= 4;

  Byte headerSize = p[0];
  if (headerSize < kBlockSizeMin ||
      headerSize > blockSize ||
      p[6] != NFileType::kArchiveHeader ||
      p[28] > 8) // EncryptionVersion
    return k_IsArc_Res_NO;

  if (blockSize + 4 <= size)
    if (Get32(p + blockSize) != CrcCalc(p, blockSize))
      return k_IsArc_Res_NO;

  return k_IsArc_Res_YES;
}
}

static HRESULT ReadString(const Byte *p, unsigned &size, AString &res)
{
  unsigned num = size;
  for (unsigned i = 0; i < num;)
  {
    if (p[i++] == 0)
    {
      size = i;
      res = (const char *)p;
      return S_OK;
    }
  }
  return S_FALSE;
}

HRESULT CArcHeader::Parse(const Byte *p, unsigned size)
{
  Byte headerSize = p[0];
  if (headerSize < kBlockSizeMin || headerSize > size)
    return S_FALSE;
  // ArchiverVersion = p[1];
  // ExtractVersion = p[2];
  HostOS = p[3];
  // Flags = p[4];
  // SecuryVersion = p[5];
  if (p[6] != NFileType::kArchiveHeader)
    return S_FALSE;
  // Reserved = p[7];
  CTime = Get32(p + 8);
  MTime = Get32(p + 12);
  ArchiveSize = Get32(p + 16); // it can be zero. (currently used only for secured archives)
  // SecurPos = Get32(p + 20);
  // UInt16 filespecPositionInFilename = Get16(p + 24);
  SecurSize = Get16(p + 26);
  // EncryptionVersion = p[28];
  // LastChapter = p[29];
  unsigned pos = headerSize;
  unsigned size1 = size - pos;
  RINOK(ReadString(p + pos, size1, Name))
  pos += size1;
  size1 = size - pos;
  RINOK(ReadString(p + pos, size1, Comment))
  pos += size1;
  return S_OK;
}


struct CExtendedInfo
{
  UInt64 Size;
  bool CrcError;
  
  void Clear()
  {
    Size = 0;
    CrcError = false;
  }
  void ParseToPropVar(NCOM::CPropVariant &prop) const
  {
    if (Size != 0)
    {
       AString s;
       s += "Extended:";
       s.Add_UInt32((UInt32)Size);
       if (CrcError)
         s += ":CRC_ERROR";
       prop = s;
    }
  }
};


struct CItem
{
  AString Name;
  AString Comment;

  UInt32 MTime;
  UInt32 PackSize;
  UInt32 Size;
  UInt32 FileCRC;
  UInt32 SplitPos;

  Byte Version;
  Byte ExtractVersion;
  Byte HostOS;
  Byte Flags;
  Byte Method;
  Byte FileType;

  // UInt16 FilespecPosInFilename;
  UInt16 FileAccessMode;
  // Byte FirstChapter;
  // Byte LastChapter;
  
  UInt64 DataPosition;

  CExtendedInfo ExtendedInfo;
  
  bool IsEncrypted() const { return (Flags & NFlags::kGarbled) != 0; }
  bool IsDir() const { return (FileType == NFileType::kDirectory); }
  bool IsSplitAfter() const { return (Flags & NFlags::kVolume) != 0; }
  bool IsSplitBefore() const { return (Flags & NFlags::kExtFile) != 0; }
  UInt32 GetWinAttrib() const
  {
    UInt32 atrrib = 0;
    switch (HostOS)
    {
      case NHostOS::kMSDOS:
      case NHostOS::kWIN95:
        atrrib = FileAccessMode;
        break;
    }
    if (IsDir())
      atrrib |= FILE_ATTRIBUTE_DIRECTORY;
    return atrrib;
  }

  HRESULT Parse(const Byte *p, unsigned size);
};

HRESULT CItem::Parse(const Byte *p, unsigned size)
{
  Byte headerSize = p[0];
  if (headerSize < kBlockSizeMin || headerSize > size)
    return S_FALSE;
  Version = p[1];
  ExtractVersion = p[2];
  HostOS = p[3];
  Flags = p[4];
  Method = p[5];
  FileType = p[6];
  // Reserved = p[7];
  MTime = Get32(p + 8);
  PackSize = Get32(p + 12);
  Size = Get32(p + 16);
  FileCRC = Get32(p + 20);
  // FilespecPosInFilename = Get16(p + 24);
  FileAccessMode = Get16(p + 26);
  // FirstChapter = p[28];
  // FirstChapter = p[29];

  SplitPos = 0;
  if (IsSplitBefore() && headerSize >= 34)
    SplitPos = Get32(p + 30);

  unsigned pos = headerSize;
  unsigned size1 = size - pos;
  RINOK(ReadString(p + pos, size1, Name))
  pos += size1;
  size1 = size - pos;
  RINOK(ReadString(p + pos, size1, Comment))
  pos += size1;

  return S_OK;
}

enum EErrorType
{
  k_ErrorType_OK,
  k_ErrorType_Corrupted,
  k_ErrorType_UnexpectedEnd
};

class CArc
{
public:
  UInt64 Processed;
  EErrorType Error;
  bool IsArc;
  IInStream *Stream;
  IArchiveOpenCallback *Callback;
  UInt64 NumFiles;
  CArcHeader Header;

  CExtendedInfo ExtendedInfo;

  HRESULT Open();
  HRESULT GetNextItem(CItem &item, bool &filled);
  void Close()
  {
    IsArc = false;
    Error = k_ErrorType_OK;
    ExtendedInfo.Clear();
  }
private:
  unsigned _blockSize;
  CByteBuffer _block;

  HRESULT ReadBlock(bool &filled, CExtendedInfo *extendedInfo);
  HRESULT SkipExtendedHeaders(CExtendedInfo &extendedInfo);
  HRESULT Read(void *data, size_t *size);
};

HRESULT CArc::Read(void *data, size_t *size)
{
  HRESULT res = ReadStream(Stream, data, size);
  Processed += *size;
  return res;
}

#define READ_STREAM(_dest_, _size_) \
  { size_t _processed_ = (_size_); RINOK(Read(_dest_, &_processed_)); \
  if (_processed_ != (_size_)) { Error = k_ErrorType_UnexpectedEnd; return S_OK; } }

HRESULT CArc::ReadBlock(bool &filled, CExtendedInfo *extendedInfo)
{
  Error = k_ErrorType_OK;
  filled = false;
  Byte buf[4];
  const unsigned signSize = extendedInfo ? 0 : 2;
  READ_STREAM(buf, signSize + 2)
  if (!extendedInfo)
    if (buf[0] != kSig0 || buf[1] != kSig1)
    {
      Error = k_ErrorType_Corrupted;
      return S_OK;
    }
  _blockSize = Get16(buf + signSize);
  if (_blockSize == 0) // end of archive
    return S_OK;

  if (!extendedInfo)
    if (_blockSize < kBlockSizeMin || _blockSize > kBlockSizeMax)
    {
      Error = k_ErrorType_Corrupted;
      return S_OK;
    }

  const size_t readSize = _blockSize + 4;
  if (readSize > _block.Size())
  {
    // extended data size is limited by (64 KB)
    // _blockSize is less than 64 KB
    const size_t upSize = (_blockSize > kBlockSizeMax ? (1 << 16) : kBlockSizeMax);
    _block.Alloc(upSize + 4);
  }

  if (extendedInfo)
    extendedInfo->Size += _blockSize;

  READ_STREAM(_block, readSize)
  if (Get32(_block + _blockSize) != CrcCalc(_block, _blockSize))
  {
    if (extendedInfo)
      extendedInfo->CrcError = true;
    else
    {
      Error = k_ErrorType_Corrupted;
      return S_OK;
    }
  }
  filled = true;
  return S_OK;
}

HRESULT CArc::SkipExtendedHeaders(CExtendedInfo &extendedInfo)
{
  extendedInfo.Clear();
  for (UInt32 i = 0;; i++)
  {
    bool filled;
    RINOK(ReadBlock(filled, &extendedInfo))
    if (!filled)
      return S_OK;
    if (Callback && (i & 0xFF) == 0)
      RINOK(Callback->SetCompleted(&NumFiles, &Processed))
  }
}

HRESULT CArc::Open()
{
  bool filled;
  RINOK(ReadBlock(filled, NULL)) // (extendedInfo = NULL)
  if (!filled)
    return S_FALSE;
  RINOK(Header.Parse(_block, _blockSize))
  IsArc = true;
  return SkipExtendedHeaders(ExtendedInfo);
}

HRESULT CArc::GetNextItem(CItem &item, bool &filled)
{
  RINOK(ReadBlock(filled, NULL)) // (extendedInfo = NULL)
  if (!filled)
    return S_OK;
  filled = false;
  if (item.Parse(_block, _blockSize) != S_OK)
  {
    Error = k_ErrorType_Corrupted;
    return S_OK;
  }
  /*
  UInt32 extraData;
  if ((header.Flags & NFlags::kExtFile) != 0)
    extraData = GetUi32(_block + pos);
  */

  RINOK(SkipExtendedHeaders(item.ExtendedInfo))
  filled = true;
  return S_OK;
}


Z7_CLASS_IMP_CHandler_IInArchive_0

  CObjectVector<CItem> _items;
  CMyComPtr<IInStream> _stream;
  UInt64 _phySize;
  CArc _arc;

  HRESULT Open2(IInStream *inStream, IArchiveOpenCallback *callback);
};

static const Byte kArcProps[] =
{
  kpidName,
  kpidCTime,
  kpidMTime,
  kpidHostOS,
  kpidComment,
  kpidCharacts
};

static const Byte kProps[] =
{
  kpidPath,
  kpidIsDir,
  kpidSize,
  kpidPosition,
  kpidPackSize,
  kpidMTime,
  kpidAttrib,
  kpidEncrypted,
  kpidCRC,
  kpidMethod,
  kpidHostOS,
  kpidComment,
  kpidCharacts
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

static void SetTime(UInt32 dosTime, NCOM::CPropVariant &prop)
{
  if (dosTime == 0)
    return;
  PropVariant_SetFrom_DosTime(prop, dosTime);
}

static void SetHostOS(Byte hostOS, NCOM::CPropVariant &prop)
{
  TYPE_TO_PROP(kHostOS, hostOS, prop);
}

static void SetUnicodeString(const AString &s, NCOM::CPropVariant &prop)
{
  if (!s.IsEmpty())
    prop = MultiByteToUnicodeString(s, CP_OEMCP);
}
 
Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPhySize: prop = _phySize; break;
    case kpidName: SetUnicodeString(_arc.Header.Name, prop); break;
    case kpidCTime: SetTime(_arc.Header.CTime, prop); break;
    case kpidMTime: SetTime(_arc.Header.MTime, prop); break;
    case kpidHostOS: SetHostOS(_arc.Header.HostOS, prop); break;
    case kpidComment: SetUnicodeString(_arc.Header.Comment, prop); break;
    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_arc.IsArc) v |= kpv_ErrorFlags_IsNotArc;
      switch (_arc.Error)
      {
        case k_ErrorType_UnexpectedEnd: v |= kpv_ErrorFlags_UnexpectedEnd; break;
        case k_ErrorType_Corrupted: v |= kpv_ErrorFlags_HeadersError; break;
        case k_ErrorType_OK:
        // default:
          break;
      }
      prop = v;
      break;
    }
    case kpidCharacts: _arc.ExtendedInfo.ParseToPropVar(prop); break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _items.Size();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  const CItem &item = _items[index];
  switch (propID)
  {
    case kpidPath:  prop = NItemName::GetOsPath(MultiByteToUnicodeString(item.Name, CP_OEMCP)); break;
    case kpidIsDir:  prop = item.IsDir(); break;
    case kpidSize:  prop = item.Size; break;
    case kpidPackSize:  prop = item.PackSize; break;
    case kpidPosition:  if (item.IsSplitBefore() || item.IsSplitAfter()) prop = (UInt64)item.SplitPos; break;
    case kpidAttrib:  prop = item.GetWinAttrib(); break;
    case kpidEncrypted:  prop = item.IsEncrypted(); break;
    case kpidCRC:  prop = item.FileCRC; break;
    case kpidMethod:  prop = item.Method; break;
    case kpidHostOS:  SetHostOS(item.HostOS, prop); break;
    case kpidMTime:  SetTime(item.MTime, prop); break;
    case kpidComment:  SetUnicodeString(item.Comment, prop); break;
    case kpidCharacts: item.ExtendedInfo.ParseToPropVar(prop); break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

HRESULT CHandler::Open2(IInStream *inStream, IArchiveOpenCallback *callback)
{
  Close();
  
  UInt64 endPos;
  RINOK(InStream_AtBegin_GetSize(inStream, endPos))
  
  _arc.Stream = inStream;
  _arc.Callback = callback;
  _arc.NumFiles = 0;
  _arc.Processed = 0;

  RINOK(_arc.Open())

  _phySize = _arc.Processed;
  if (_arc.Header.ArchiveSize != 0)
    _phySize = (UInt64)_arc.Header.ArchiveSize + _arc.Header.SecurSize;

  for (;;)
  {
    CItem item;
    bool filled;

    _arc.Error = k_ErrorType_OK;
    RINOK(_arc.GetNextItem(item, filled))

    if (_arc.Error != k_ErrorType_OK)
      break;
    
    if (!filled)
    {
      if (_arc.Error == k_ErrorType_OK)
        if (_arc.Header.ArchiveSize == 0)
          _phySize = _arc.Processed;
      break;
    }
    item.DataPosition = _arc.Processed;
    _items.Add(item);
    
    UInt64 pos = item.DataPosition + item.PackSize;
    if (_arc.Header.ArchiveSize == 0)
      _phySize = pos;
    if (pos > endPos)
    {
      _arc.Error = k_ErrorType_UnexpectedEnd;
      break;
    }

    RINOK(InStream_SeekSet(inStream, pos))
    _arc.NumFiles = _items.Size();
    _arc.Processed = pos;
    
    if (callback && (_items.Size() & 0xFF) == 0)
    {
      RINOK(callback->SetCompleted(&_arc.NumFiles, &_arc.Processed))
    }
  }
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Open(IInStream *inStream,
    const UInt64 * /* maxCheckStartPosition */, IArchiveOpenCallback *callback))
{
  COM_TRY_BEGIN
  HRESULT res;
  {
    res = Open2(inStream, callback);
    if (res == S_OK)
    {
      _stream = inStream;
      return S_OK;
    }
  }
  return res;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _arc.Close();
  _phySize = 0;
  _items.Clear();
  _stream.Release();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  UInt64 totalUnpacked = 0, totalPacked = 0;
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _items.Size();
  if (numItems == 0)
    return S_OK;
  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    const CItem &item = _items[allFilesMode ? i : indices[i]];
    totalUnpacked += item.Size;
    // totalPacked += item.PackSize;
  }
  extractCallback->SetTotal(totalUnpacked);

  totalUnpacked = totalPacked = 0;
  UInt64 curUnpacked, curPacked;
  
  NCompress::NLzh::NDecoder::CCoder *lzhDecoderSpec = NULL;
  CMyComPtr<ICompressCoder> lzhDecoder;

  NCompress::NArj::NDecoder::CCoder *arjDecoderSpec = NULL;
  CMyComPtr<ICompressCoder> arjDecoder;

  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder();
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  CLimitedSequentialInStream *inStreamSpec = new CLimitedSequentialInStream;
  CMyComPtr<ISequentialInStream> inStream(inStreamSpec);
  inStreamSpec->SetStream(_stream);

  for (i = 0; i < numItems; i++, totalUnpacked += curUnpacked, totalPacked += curPacked)
  {
    lps->InSize = totalPacked;
    lps->OutSize = totalUnpacked;
    RINOK(lps->SetCur())

    curUnpacked = curPacked = 0;

    CMyComPtr<ISequentialOutStream> realOutStream;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    const UInt32 index = allFilesMode ? i : indices[i];
    const CItem &item = _items[index];
    RINOK(extractCallback->GetStream(index, &realOutStream, askMode))

    if (item.IsDir())
    {
      // if (!testMode)
      {
        RINOK(extractCallback->PrepareOperation(askMode))
        RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
      }
      continue;
    }

    if (!testMode && !realOutStream)
      continue;

    RINOK(extractCallback->PrepareOperation(askMode))
    curUnpacked = item.Size;
    curPacked = item.PackSize;

    {
      COutStreamWithCRC *outStreamSpec = new COutStreamWithCRC;
      CMyComPtr<ISequentialOutStream> outStream(outStreamSpec);
      outStreamSpec->SetStream(realOutStream);
      realOutStream.Release();
      outStreamSpec->Init();
  
      inStreamSpec->Init(item.PackSize);
      
      RINOK(InStream_SeekSet(_stream, item.DataPosition))

      HRESULT result = S_OK;
      Int32 opRes = NExtract::NOperationResult::kOK;

      if (item.IsEncrypted())
        opRes = NExtract::NOperationResult::kUnsupportedMethod;
      else
      {
        switch (item.Method)
        {
          case NCompressionMethod::kStored:
          {
            result = copyCoder->Code(inStream, outStream, NULL, NULL, progress);
            if (result == S_OK && copyCoderSpec->TotalSize != item.PackSize)
              result = S_FALSE;
            break;
          }
          case NCompressionMethod::kCompressed1a:
          case NCompressionMethod::kCompressed1b:
          case NCompressionMethod::kCompressed1c:
          {
            if (!lzhDecoder)
            {
              lzhDecoderSpec = new NCompress::NLzh::NDecoder::CCoder;
              lzhDecoder = lzhDecoderSpec;
            }
            lzhDecoderSpec->FinishMode = true;
            const UInt32 kHistorySize = 26624;
            lzhDecoderSpec->SetDictSize(kHistorySize);
            result = lzhDecoder->Code(inStream, outStream, NULL, &curUnpacked, progress);
            if (result == S_OK && lzhDecoderSpec->GetInputProcessedSize() != item.PackSize)
              result = S_FALSE;
            break;
          }
          case NCompressionMethod::kCompressed2:
          {
            if (!arjDecoder)
            {
              arjDecoderSpec = new NCompress::NArj::NDecoder::CCoder;
              arjDecoder = arjDecoderSpec;
            }
            arjDecoderSpec->FinishMode = true;
            result = arjDecoder->Code(inStream, outStream, NULL, &curUnpacked, progress);
            if (result == S_OK && arjDecoderSpec->GetInputProcessedSize() != item.PackSize)
              result = S_FALSE;
            break;
          }
          default:
            opRes = NExtract::NOperationResult::kUnsupportedMethod;
        }
      }
      
      if (opRes == NExtract::NOperationResult::kOK)
      {
        if (result == S_FALSE)
          opRes = NExtract::NOperationResult::kDataError;
        else
        {
          RINOK(result)
          opRes = (outStreamSpec->GetCRC() == item.FileCRC) ?
              NExtract::NOperationResult::kOK:
              NExtract::NOperationResult::kCRCError;
        }
      }
      
      outStream.Release();
      RINOK(extractCallback->SetOperationResult(opRes))
    }
  }
  
  return S_OK;
  COM_TRY_END
}

static const Byte k_Signature[] = { kSig0, kSig1 };

REGISTER_ARC_I(
  "Arj", "arj", NULL, 4,
  k_Signature,
  0,
  0,
  IsArc_Arj)

}}

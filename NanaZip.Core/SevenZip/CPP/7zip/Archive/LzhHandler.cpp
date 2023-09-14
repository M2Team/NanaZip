// LzhHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/MyBuffer.h"
#include "../../Common/StringConvert.h"

#include "../../Windows/PropVariant.h"
#include "../../Windows/PropVariantUtils.h"
#include "../../Windows/TimeUtils.h"

#include "../ICoder.h"

#include "../Common/LimitedStreams.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"
#include "../Compress/LzhDecoder.h"

#include "IArchive.h"

#include "Common/ItemNameUtils.h"

using namespace NWindows;
using namespace NTime;

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)


// CRC-16 (-IBM, -ANSI). The poly is 0x8005 (x^16 + x^15 + x^2 + 1)

static const UInt16 kCrc16Poly = 0xA001;

static UInt16 g_LzhCrc16Table[256];

#define CRC16_UPDATE_BYTE(crc, b) (g_LzhCrc16Table[((crc) ^ (b)) & 0xFF] ^ ((crc) >> 8))

UInt32 LzhCrc16Update(UInt32 crc, const void *data, size_t size);
UInt32 LzhCrc16Update(UInt32 crc, const void *data, size_t size)
{
  const Byte *p = (const Byte *)data;
  const Byte *pEnd = p + size;
  for (; p != pEnd; p++)
    crc = CRC16_UPDATE_BYTE(crc, *p);
  return crc;
}

static struct CLzhCrc16TableInit
{
  CLzhCrc16TableInit()
  {
    for (UInt32 i = 0; i < 256; i++)
    {
      UInt32 r = i;
      for (unsigned j = 0; j < 8; j++)
        r = (r >> 1) ^ (kCrc16Poly & ((UInt32)0 - (r & 1)));
      g_LzhCrc16Table[i] = (UInt16)r;
    }
  }
} g_LzhCrc16TableInit;


namespace NArchive {
namespace NLzh{

const unsigned kMethodIdSize = 5;

const Byte kExtIdFileName = 0x01;
const Byte kExtIdDirName  = 0x02;
const Byte kExtIdUnixTime = 0x54;

struct CExtension
{
  Byte Type;
  CByteBuffer Data;
  
  AString GetString() const
  {
    AString s;
    s.SetFrom_CalcLen((const char *)(const Byte *)Data, (unsigned)Data.Size());
    return s;
  }
};

const UInt32 kBasicPartSize = 22;

API_FUNC_static_IsArc IsArc_Lzh(const Byte *p, size_t size)
{
  if (size < 2 + kBasicPartSize)
    return k_IsArc_Res_NEED_MORE;
  if (p[2] != '-' || p[3] != 'l'  || p[4] != 'h' || p[6] != '-')
    return k_IsArc_Res_NO;
  Byte n = p[5];
  if (n != 'd')
    if (n < '0' || n > '7')
      return k_IsArc_Res_NO;
  return k_IsArc_Res_YES;
}
}

struct CItem
{
  AString Name;
  Byte Method[kMethodIdSize];
  Byte Attributes;
  Byte Level;
  Byte OsId;
  UInt32 PackSize;
  UInt32 Size;
  UInt32 ModifiedTime;
  UInt16 CRC;
  CObjectVector<CExtension> Extensions;

  bool IsValidMethod() const  { return (Method[0] == '-' && Method[1] == 'l' && Method[4] == '-'); }
  bool IsLhMethod() const  {return (IsValidMethod() && Method[2] == 'h'); }
  bool IsDir() const {return (IsLhMethod() && Method[3] == 'd'); }

  bool IsCopyMethod() const
  {
    return (IsLhMethod() && Method[3] == '0') ||
      (IsValidMethod() && Method[2] == 'z' && Method[3] == '4');
  }
  
  bool IsLh1GroupMethod() const
  {
    if (!IsLhMethod())
      return false;
    switch (Method[3])
    {
      case '1':
        return true;
    }
    return false;
  }
  
  bool IsLh4GroupMethod() const
  {
    if (!IsLhMethod())
      return false;
    switch (Method[3])
    {
      case '4':
      case '5':
      case '6':
      case '7':
        return true;
    }
    return false;
  }
  
  unsigned GetNumDictBits() const
  {
    if (!IsLhMethod())
      return 0;
    switch (Method[3])
    {
      case '1': return 12;
      case '2': return 13;
      case '3': return 13;
      case '4': return 12;
      case '5': return 13;
      case '6': return 15;
      case '7': return 16;
    }
    return 0;
  }

  int FindExt(Byte type) const
  {
    FOR_VECTOR (i, Extensions)
      if (Extensions[i].Type == type)
        return (int)i;
    return -1;
  }
  
  bool GetUnixTime(UInt32 &value) const
  {
    value = 0;
    int index = FindExt(kExtIdUnixTime);
    if (index < 0
        || Extensions[index].Data.Size() < 4)
    {
      if (Level == 2)
      {
        value = ModifiedTime;
        return true;
      }
      return false;
    }
    const Byte *data = (const Byte *)(Extensions[index].Data);
    value = GetUi32(data);
    return true;
  }

  AString GetDirName() const
  {
    int index = FindExt(kExtIdDirName);
    if (index < 0)
      return AString();
    return Extensions[index].GetString();
  }

  AString GetFileName() const
  {
    int index = FindExt(kExtIdFileName);
    if (index < 0)
      return Name;
    return Extensions[index].GetString();
  }

  AString GetName() const
  {
    AString s (GetDirName());
    const char kDirSeparator = '\\';
    // check kDirSeparator in Linux
    s.Replace((char)(unsigned char)0xFF, kDirSeparator);
    if (!s.IsEmpty() && s.Back() != kDirSeparator)
      s += kDirSeparator;
    s += GetFileName();
    return s;
  }
};

static const Byte *ReadUInt16(const Byte *p, UInt16 &v)
{
  v = Get16(p);
  return p + 2;
}

static const Byte *ReadString(const Byte *p, size_t size, AString &s)
{
  s.Empty();
  for (size_t i = 0; i < size; i++)
  {
    const Byte c = p[i];
    if (c == 0)
      break;
    s += (char)c;
  }
  return p + size;
}

static Byte CalcSum(const Byte *data, size_t size)
{
  Byte sum = 0;
  for (size_t i = 0; i < size; i++)
    sum = (Byte)(sum + data[i]);
  return sum;
}

static HRESULT GetNextItem(ISequentialInStream *stream, bool &filled, CItem &item)
{
  filled = false;

  size_t processedSize = 2;
  Byte startHeader[2];
  RINOK(ReadStream(stream, startHeader, &processedSize))
  if (processedSize == 0)
    return S_OK;
  if (processedSize == 1)
    return (startHeader[0] == 0) ? S_OK: S_FALSE;
  if (startHeader[0] == 0 && startHeader[1] == 0)
    return S_OK;

  Byte header[256];
  processedSize = kBasicPartSize;
  RINOK(ReadStream(stream, header, &processedSize))
  if (processedSize != kBasicPartSize)
    return (startHeader[0] == 0) ? S_OK: S_FALSE;

  const Byte *p = header;
  memcpy(item.Method, p, kMethodIdSize);
  if (!item.IsValidMethod())
    return S_OK;
  p += kMethodIdSize;
  item.PackSize = Get32(p);
  item.Size = Get32(p + 4);
  item.ModifiedTime = Get32(p + 8);
  item.Attributes = p[12];
  item.Level = p[13];
  p += 14;
  if (item.Level > 2)
    return S_FALSE;
  UInt32 headerSize;
  if (item.Level < 2)
  {
    headerSize = startHeader[0];
    if (headerSize < kBasicPartSize)
      return S_FALSE;
    RINOK(ReadStream_FALSE(stream, header + kBasicPartSize, headerSize - kBasicPartSize))
    if (startHeader[1] != CalcSum(header, headerSize))
      return S_FALSE;
    const size_t nameLength = *p++;
    if ((size_t)(p - header) + nameLength + 2 > headerSize)
      return S_FALSE;
    p = ReadString(p, nameLength, item.Name);
  }
  else
    headerSize = startHeader[0] | ((UInt32)startHeader[1] << 8);
  p = ReadUInt16(p, item.CRC);
  if (item.Level != 0)
  {
    if (item.Level == 2)
    {
      RINOK(ReadStream_FALSE(stream, header + kBasicPartSize, 2))
    }
    if ((size_t)(p - header) + 3 > headerSize)
      return S_FALSE;
    item.OsId = *p++;
    UInt16 nextSize;
    p = ReadUInt16(p, nextSize);
    while (nextSize != 0)
    {
      if (nextSize < 3)
        return S_FALSE;
      if (item.Level == 1)
      {
        if (item.PackSize < nextSize)
          return S_FALSE;
        item.PackSize -= nextSize;
      }
      if (item.Extensions.Size() >= (1 << 8))
        return S_FALSE;
      CExtension ext;
      RINOK(ReadStream_FALSE(stream, &ext.Type, 1))
      nextSize = (UInt16)(nextSize - 3);
      ext.Data.Alloc(nextSize);
      RINOK(ReadStream_FALSE(stream, (Byte *)ext.Data, nextSize))
      item.Extensions.Add(ext);
      Byte hdr2[2];
      RINOK(ReadStream_FALSE(stream, hdr2, 2))
      ReadUInt16(hdr2, nextSize);
    }
  }
  filled = true;
  return S_OK;
}


static const CUInt32PCharPair g_OsPairs[] =
{
  {   0, "MS-DOS" },
  { 'M', "MS-DOS" },
  { '2', "OS/2" },
  { '9', "OS9" },
  { 'K', "OS/68K" },
  { '3', "OS/386" },
  { 'H', "HUMAN" },
  { 'U', "UNIX" },
  { 'C', "CP/M" },
  { 'F', "FLEX" },
  { 'm', "Mac" },
  { 'R', "Runser" },
  { 'T', "TownsOS" },
  { 'X', "XOSK" },
  { 'w', "Windows 95" },
  { 'W', "Windows NT" },
  { 'J', "Java VM" }
};


static const Byte kProps[] =
{
  kpidPath,
  kpidIsDir,
  kpidSize,
  kpidPackSize,
  kpidMTime,
  // kpidAttrib,
  kpidCRC,
  kpidMethod,
  kpidHostOS
};


Z7_CLASS_IMP_NOQIB_1(
  COutStreamWithCRC
  , ISequentialOutStream
)
  UInt32 _crc;
  CMyComPtr<ISequentialOutStream> _stream;
public:
  void Init(ISequentialOutStream *stream)
  {
    _stream = stream;
    _crc = 0;
  }
  void ReleaseStream() { _stream.Release(); }
  UInt32 GetCRC() const { return _crc; }
};

Z7_COM7F_IMF(COutStreamWithCRC::Write(const void *data, UInt32 size, UInt32 *processedSize))
{
  HRESULT res = S_OK;
  if (_stream)
    res = _stream->Write(data, size, &size);
  _crc = LzhCrc16Update(_crc, data, size);
  if (processedSize)
    *processedSize = size;
  return res;
}


struct CItemEx: public CItem
{
  UInt64 DataPosition;
};


Z7_CLASS_IMP_CHandler_IInArchive_0

  CObjectVector<CItemEx> _items;
  CMyComPtr<IInStream> _stream;
  UInt64 _phySize;
  UInt32 _errorFlags;
  bool _isArc;
public:
  CHandler();
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps_NO_Table

CHandler::CHandler() {}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _items.Size();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPhySize: prop = _phySize; break;
   
    case kpidErrorFlags:
      UInt32 v = _errorFlags;
      if (!_isArc) v |= kpv_ErrorFlags_IsNotArc;
      prop = v;
      break;
  }
  prop.Detach(value);
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  const CItemEx &item = _items[index];
  switch (propID)
  {
    case kpidPath:
    {
      UString s = NItemName::WinPathToOsPath(MultiByteToUnicodeString(item.GetName(), CP_OEMCP));
      if (!s.IsEmpty())
      {
        if (s.Back() == WCHAR_PATH_SEPARATOR)
          s.DeleteBack();
        prop = s;
      }
      break;
    }
    case kpidIsDir:  prop = item.IsDir(); break;
    case kpidSize:   prop = item.Size; break;
    case kpidPackSize:  prop = item.PackSize; break;
    case kpidCRC:  prop = (UInt32)item.CRC; break;
    case kpidHostOS:  PAIR_TO_PROP(g_OsPairs, item.OsId, prop); break;
    case kpidMTime:
    {
      UInt32 unixTime;
      if (item.GetUnixTime(unixTime))
        PropVariant_SetFrom_UnixTime(prop, unixTime);
      else
        PropVariant_SetFrom_DosTime(prop, item.ModifiedTime);
      break;
    }
    // case kpidAttrib:  prop = (UInt32)item.Attributes; break;
    case kpidMethod:
    {
      char method2[kMethodIdSize + 1];
      method2[kMethodIdSize] = 0;
      memcpy(method2, item.Method, kMethodIdSize);
      prop = method2;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Open(IInStream *stream,
    const UInt64 * /* maxCheckStartPosition */, IArchiveOpenCallback *callback))
{
  COM_TRY_BEGIN
  Close();
  try
  {
    _items.Clear();

    UInt64 endPos;
    bool needSetTotal = true;

    RINOK(InStream_AtBegin_GetSize(stream, endPos))

    for (;;)
    {
      CItemEx item;
      bool filled;
      const HRESULT res = GetNextItem(stream, filled, item);
      RINOK(InStream_GetPos(stream, item.DataPosition))
      if (res == S_FALSE)
      {
        _errorFlags = kpv_ErrorFlags_HeadersError;
        break;
      }

      if (res != S_OK)
        return S_FALSE;
      _phySize = item.DataPosition;
      if (!filled)
        break;
      _items.Add(item);

      _isArc = true;

      UInt64 newPostion;
      RINOK(stream->Seek(item.PackSize, STREAM_SEEK_CUR, &newPostion))
      if (newPostion > endPos)
      {
        _phySize = endPos;
        _errorFlags = kpv_ErrorFlags_UnexpectedEnd;
        break;
      }
      _phySize = newPostion;
      if (callback)
      {
        if (needSetTotal)
        {
          RINOK(callback->SetTotal(NULL, &endPos))
          needSetTotal = false;
        }
        if (_items.Size() % 100 == 0)
        {
          UInt64 numFiles = _items.Size();
          UInt64 numBytes = item.DataPosition;
          RINOK(callback->SetCompleted(&numFiles, &numBytes))
        }
      }
    }
    if (_items.IsEmpty())
      return S_FALSE;

    _stream = stream;
  }
  catch(...)
  {
    return S_FALSE;
  }
  COM_TRY_END
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Close())
{
  _isArc = false;
  _phySize = 0;
  _errorFlags = 0;
  _items.Clear();
  _stream.Release();
  return S_OK;
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
  UInt64 totalUnPacked = 0 /* , totalPacked = 0 */;
  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    const CItemEx &item = _items[allFilesMode ? i : indices[i]];
    totalUnPacked += item.Size;
    // totalPacked += item.PackSize;
  }
  RINOK(extractCallback->SetTotal(totalUnPacked))

  UInt64 currentItemUnPacked, currentItemPacked;
  
  NCompress::NLzh::NDecoder::CCoder *lzhDecoderSpec = NULL;
  CMyComPtr<ICompressCoder> lzhDecoder;
  // CMyComPtr<ICompressCoder> lzh1Decoder;
  // CMyComPtr<ICompressCoder> arj2Decoder;

  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder();
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  CLimitedSequentialInStream *streamSpec = new CLimitedSequentialInStream;
  CMyComPtr<ISequentialInStream> inStream(streamSpec);
  streamSpec->SetStream(_stream);

  for (i = 0;; i++,
      lps->OutSize += currentItemUnPacked,
      lps->InSize += currentItemPacked)
  {
    currentItemUnPacked = 0;
    currentItemPacked = 0;

    RINOK(lps->SetCur())

    if (i >= numItems)
      break;

    CMyComPtr<ISequentialOutStream> realOutStream;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    const UInt32 index = allFilesMode ? i : indices[i];
    const CItemEx &item = _items[index];
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
    currentItemUnPacked = item.Size;
    currentItemPacked = item.PackSize;

    {
      COutStreamWithCRC *outStreamSpec = new COutStreamWithCRC;
      CMyComPtr<ISequentialOutStream> outStream(outStreamSpec);
      outStreamSpec->Init(realOutStream);
      realOutStream.Release();
      
      RINOK(InStream_SeekSet(_stream, item.DataPosition))

      streamSpec->Init(item.PackSize);

      HRESULT res = S_OK;
      Int32 opRes = NExtract::NOperationResult::kOK;

      if (item.IsCopyMethod())
      {
        res = copyCoder->Code(inStream, outStream, NULL, NULL, progress);
        if (res == S_OK && copyCoderSpec->TotalSize != item.PackSize)
          res = S_FALSE;
      }
      else if (item.IsLh4GroupMethod())
      {
        if (!lzhDecoder)
        {
          lzhDecoderSpec = new NCompress::NLzh::NDecoder::CCoder;
          lzhDecoder = lzhDecoderSpec;
        }
        lzhDecoderSpec->FinishMode = true;
        lzhDecoderSpec->SetDictSize(1 << item.GetNumDictBits());
        res = lzhDecoder->Code(inStream, outStream, NULL, &currentItemUnPacked, progress);
        if (res == S_OK && lzhDecoderSpec->GetInputProcessedSize() != item.PackSize)
          res = S_FALSE;
      }
      /*
      else if (item.IsLh1GroupMethod())
      {
        if (!lzh1Decoder)
        {
          lzh1DecoderSpec = new NCompress::NLzh1::NDecoder::CCoder;
          lzh1Decoder = lzh1DecoderSpec;
        }
        lzh1DecoderSpec->SetDictionary(item.GetNumDictBits());
        res = lzh1Decoder->Code(inStream, outStream, NULL, &currentItemUnPacked, progress);
      }
      */
      else
        opRes = NExtract::NOperationResult::kUnsupportedMethod;

      if (opRes == NExtract::NOperationResult::kOK)
      {
        if (res == S_FALSE)
          opRes = NExtract::NOperationResult::kDataError;
        else
        {
          RINOK(res)
          if (outStreamSpec->GetCRC() != item.CRC)
            opRes = NExtract::NOperationResult::kCRCError;
        }
      }
      outStream.Release();
      RINOK(extractCallback->SetOperationResult(opRes))
    }
  }
  return S_OK;
  COM_TRY_END
}

static const Byte k_Signature[] = { '-', 'l', 'h' };

REGISTER_ARC_I(
  "Lzh", "lzh lha", NULL, 6,
  k_Signature,
  2,
  0,
  IsArc_Lzh)

}}

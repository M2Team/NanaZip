// Rar5Handler.cpp

#include "StdAfx.h"

#include "../../../../C/7zCrc.h"
#include "../../../../C/CpuArch.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/IntToString.h"
#include "../../../Common/MyBuffer2.h"
#include "../../../Common/UTFConvert.h"

#include "../../../Windows/PropVariantUtils.h"
#include "../../../Windows/TimeUtils.h"

#include "../../IPassword.h"

#include "../../Common/FilterCoder.h"
#include "../../Common/LimitedStreams.h"
#include "../../Common/MethodProps.h"
#include "../../Common/ProgressUtils.h"
#include "../../Common/RegisterArc.h"
#include "../../Common/StreamObjects.h"
#include "../../Common/StreamUtils.h"

#include "../../Common/RegisterCodec.h"

#include "../../Compress/CopyCoder.h"

#include "../../Crypto/Rar5Aes.h"

#include "../../Archive/Common/FindSignature.h"
#include "../../Archive/Common/ItemNameUtils.h"
#include "../../Archive/Common/HandlerOut.h"

#include "../../Archive/HandlerCont.h"

#include "../../Archive/Rar/RarVol.h"
#include "Rar5Handler.h"

using namespace NWindows;

#define Get32(p) GetUi32(p)

namespace NArchive {
namespace NRar5 {

static const unsigned kMarkerSize = 8;

static const Byte kMarker[kMarkerSize] =
  { 0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x01, 0 };

// Comment length is limited to 256 KB in rar-encoder.
// So we use same limitation
static const size_t kCommentSize_Max = (size_t)1 << 18;


static const char * const kHostOS[] =
{
    "Windows"
  , "Unix"
};


static const char * const k_ArcFlags[] =
{
    "Volume"
  , "VolumeField"
  , "Solid"
  , "Recovery"
  , "Lock" // 4
};


static const char * const k_FileFlags[] =
{
    "Dir"
  , "UnixTime"
  , "CRC"
  , "UnknownSize"
};


static const char * const g_ExtraTypes[] =
{
    "0"
  , "Crypto"
  , "Hash"
  , "Time"
  , "Version"
  , "Link"
  , "UnixOwner"
  , "Subdata"
};


static const char * const g_LinkTypes[] =
{
    "0"
  , "UnixSymLink"
  , "WinSymLink"
  , "WinJunction"
  , "HardLink"
  , "FileCopy"
};


static const char g_ExtraTimeFlags[] = { 'u', 'M', 'C', 'A', 'n' };


static
Z7_NO_INLINE
unsigned ReadVarInt(const Byte *p, size_t maxSize, UInt64 *val_ptr)
{
  if (maxSize > 10)
      maxSize = 10;
  UInt64 val = 0;
  unsigned i;
  for (i = 0; i < maxSize;)
  {
    const unsigned b = p[i];
    val |= (UInt64)(b & 0x7F) << (7 * i);
    i++;
    if ((b & 0x80) == 0)
    {
      *val_ptr = val;
      return i;
    }
  }
  *val_ptr = 0;
#if 1
  return 0; // 7zip-unrar : strict check of error
#else
  return i; // original-unrar : ignore error
#endif
}


#define PARSE_VAR_INT(p, size, dest) \
{ const unsigned num_ = ReadVarInt(p, size, &dest);  \
  if (num_ == 0) return false; \
  p += num_; \
  size -= num_; \
}


bool CLinkInfo::Parse(const Byte *p, unsigned size)
{
  const Byte *pStart = p;
  UInt64 len;
  PARSE_VAR_INT(p, size, Type)
  PARSE_VAR_INT(p, size, Flags)
  PARSE_VAR_INT(p, size, len)
  if (size != len)
    return false;
  NameLen = (unsigned)len;
  NameOffset = (unsigned)(size_t)(p - pStart);
  return true;
}


static void AddHex64(AString &s, UInt64 v)
{
  char sz[32];
  sz[0] = '0';
  sz[1] = 'x';
  ConvertUInt64ToHex(v, sz + 2);
  s += sz;
}


static void PrintType(AString &s, const char * const table[], unsigned num, UInt64 val)
{
  char sz[32];
  const char *p = NULL;
  if (val < num)
    p = table[(unsigned)val];
  if (!p)
  {
    ConvertUInt64ToString(val, sz);
    p = sz;
  }
  s += p;
}


int CItem::FindExtra(unsigned extraID, unsigned &recordDataSize) const
{
  recordDataSize = 0;
  size_t offset = 0;

  for (;;)
  {
    size_t rem = Extra.Size() - offset;
    if (rem == 0)
      return -1;
    
    {
      UInt64 size;
      const unsigned num = ReadVarInt(Extra + offset, rem, &size);
      if (num == 0)
        return -1;
      offset += num;
      rem -= num;
      if (size > rem)
        return -1;
      rem = (size_t)size;
    }
    {
      UInt64 id;
      const unsigned num = ReadVarInt(Extra + offset, rem, &id);
      if (num == 0)
        return -1;
      offset += num;
      rem -= num;

      // There was BUG in RAR 5.21- : it stored (size-1) instead of (size)
      // for Subdata record in Service header.
      // That record always was last in bad archives, so we can fix that case.
      if (id == NExtraID::kSubdata
          && RecordType == NHeaderType::kService
          && rem + 1 == Extra.Size() - offset)
        rem++;

      if (id == extraID)
      {
        recordDataSize = (unsigned)rem;
        return (int)offset;
      }

      offset += rem;
    }
  }
}


void CItem::PrintInfo(AString &s) const
{
  size_t offset = 0;

  for (;;)
  {
    size_t rem = Extra.Size() - offset;
    if (rem == 0)
      return;
    
    {
      UInt64 size;
      unsigned num = ReadVarInt(Extra + offset, rem, &size);
      if (num == 0)
        return;
      offset += num;
      rem -= num;
      if (size > rem)
        break;
      rem = (size_t)size;
    }
    {
      UInt64 id;
      {
        unsigned num = ReadVarInt(Extra + offset, rem, &id);
        if (num == 0)
          break;
        offset += num;
        rem -= num;
      }

      // There was BUG in RAR 5.21- : it stored (size-1) instead of (size)
      // for Subdata record in Service header.
      // That record always was last in bad archives, so we can fix that case.
      if (id == NExtraID::kSubdata
          && RecordType == NHeaderType::kService
          && rem + 1 == Extra.Size() - offset)
        rem++;

      s.Add_Space_if_NotEmpty();
      PrintType(s, g_ExtraTypes, Z7_ARRAY_SIZE(g_ExtraTypes), id);

      if (id == NExtraID::kTime)
      {
        const Byte *p = Extra + offset;
        UInt64 flags;
        const unsigned num = ReadVarInt(p, rem, &flags);
        if (num != 0)
        {
          s.Add_Colon();
          for (unsigned i = 0; i < Z7_ARRAY_SIZE(g_ExtraTimeFlags); i++)
            if ((flags & ((UInt64)1 << i)) != 0)
              s.Add_Char(g_ExtraTimeFlags[i]);
          flags &= ~(((UInt64)1 << Z7_ARRAY_SIZE(g_ExtraTimeFlags)) - 1);
          if (flags != 0)
          {
            s.Add_Char('_');
            AddHex64(s, flags);
          }
        }
      }
      else if (id == NExtraID::kLink)
      {
        CLinkInfo linkInfo;
        if (linkInfo.Parse(Extra + offset, (unsigned)rem))
        {
          s.Add_Colon();
          PrintType(s, g_LinkTypes, Z7_ARRAY_SIZE(g_LinkTypes), linkInfo.Type);
          UInt64 flags = linkInfo.Flags;
          if (flags != 0)
          {
            s.Add_Colon();
            if (flags & NLinkFlags::kTargetIsDir)
            {
              s.Add_Char('D');
              flags &= ~((UInt64)NLinkFlags::kTargetIsDir);
            }
            if (flags != 0)
            {
              s.Add_Char('_');
              AddHex64(s, flags);
            }
          }
        }
      }

      offset += rem;
    }
  }

  s.Add_OptSpaced("ERROR");
}


bool CCryptoInfo::Parse(const Byte *p, size_t size)
{
  Algo = 0;
  Flags = 0;
  Cnt = 0;
  PARSE_VAR_INT(p, size, Algo)
  PARSE_VAR_INT(p, size, Flags)
  if (size > 0)
    Cnt = p[0];
  if (size != 1 + 16 + 16 + (unsigned)(IsThereCheck() ? 12 : 0))
    return false;
  return true;
}


bool CItem::FindExtra_Version(UInt64 &version) const
{
  unsigned size;
  const int offset = FindExtra(NExtraID::kVersion, size);
  if (offset < 0)
    return false;
  const Byte *p = Extra + (unsigned)offset;

  UInt64 flags;
  PARSE_VAR_INT(p, size, flags)
  PARSE_VAR_INT(p, size, version)
  return size == 0;
}

bool CItem::FindExtra_Link(CLinkInfo &link) const
{
  unsigned size;
  const int offset = FindExtra(NExtraID::kLink, size);
  if (offset < 0)
    return false;
  if (!link.Parse(Extra + (unsigned)offset, size))
    return false;
  link.NameOffset += (unsigned)offset;
  return true;
}

bool CItem::Is_CopyLink() const
{
  CLinkInfo link;
  return FindExtra_Link(link) && link.Type == NLinkType::kFileCopy;
}

bool CItem::Is_HardLink() const
{
  CLinkInfo link;
  return FindExtra_Link(link) && link.Type == NLinkType::kHardLink;
}

bool CItem::Is_CopyLink_or_HardLink() const
{
  CLinkInfo link;
  return FindExtra_Link(link) && (link.Type == NLinkType::kFileCopy || link.Type == NLinkType::kHardLink);
}

void CItem::Link_to_Prop(unsigned linkType, NWindows::NCOM::CPropVariant &prop) const
{
  CLinkInfo link;
  if (!FindExtra_Link(link))
    return;

  if (link.Type != linkType)
  {
    if (linkType != NLinkType::kUnixSymLink)
      return;
    switch ((unsigned)link.Type)
    {
      case NLinkType::kUnixSymLink:
      case NLinkType::kWinSymLink:
      case NLinkType::kWinJunction:
        break;
      default: return;
    }
  }

  AString s;
  s.SetFrom_CalcLen((const char *)(Extra + link.NameOffset), link.NameLen);

  UString unicode;
  ConvertUTF8ToUnicode(s, unicode);
  prop = NItemName::GetOsPath(unicode);
}

bool CItem::GetAltStreamName(AString &name) const
{
  name.Empty();
  unsigned size;
  const int offset = FindExtra(NExtraID::kSubdata, size);
  if (offset < 0)
    return false;
  name.SetFrom_CalcLen((const char *)(Extra + (unsigned)offset), size);
  return true;
}


class CHash
{
  bool _calcCRC;
  UInt32 _crc;
  int _blakeOffset;
  CAlignedBuffer1 _buf;
  // CBlake2sp _blake;
  CBlake2sp *BlakeObj() { return (CBlake2sp *)(void *)(Byte *)_buf; }
public:
  CHash():
    _buf(sizeof(CBlake2sp))
    {}

  void Init_NoCalc()
  {
    _calcCRC = false;
    _crc = CRC_INIT_VAL;
    _blakeOffset = -1;
  }

  void Init(const CItem &item);
  void Update(const void *data, size_t size);
  UInt32 GetCRC() const { return CRC_GET_DIGEST(_crc); }

  bool Check(const CItem &item, NCrypto::NRar5::CDecoder *cryptoDecoder);
};

void CHash::Init(const CItem &item)
{
  _crc = CRC_INIT_VAL;
  _calcCRC = item.Has_CRC();
  _blakeOffset = item.FindExtra_Blake();
  if (_blakeOffset >= 0)
    Blake2sp_Init(BlakeObj());
}

void CHash::Update(const void *data, size_t size)
{
  if (_calcCRC)
    _crc = CrcUpdate(_crc, data, size);
  if (_blakeOffset >= 0)
    Blake2sp_Update(BlakeObj(), (const Byte *)data, size);
}

bool CHash::Check(const CItem &item, NCrypto::NRar5::CDecoder *cryptoDecoder)
{
  if (_calcCRC)
  {
    UInt32 crc = GetCRC();
    if (cryptoDecoder)
      crc = cryptoDecoder->Hmac_Convert_Crc32(crc);
    if (crc != item.CRC)
      return false;
  }
  if (_blakeOffset >= 0)
  {
    UInt32 digest[Z7_BLAKE2S_DIGEST_SIZE / sizeof(UInt32)];
    Blake2sp_Final(BlakeObj(), (Byte *)(void *)digest);
    if (cryptoDecoder)
      cryptoDecoder->Hmac_Convert_32Bytes((Byte *)(void *)digest);
    if (memcmp(digest, item.Extra + (unsigned)_blakeOffset, Z7_BLAKE2S_DIGEST_SIZE) != 0)
      return false;
  }
  return true;
}


Z7_CLASS_IMP_NOQIB_1(
  COutStreamWithHash
  , ISequentialOutStream
)
  bool _size_Defined;
  ISequentialOutStream *_stream;
  UInt64 _pos;
  UInt64 _size;
  Byte *_destBuf;
public:
  CHash _hash;

  COutStreamWithHash(): _destBuf(NULL) {}

  void SetStream(ISequentialOutStream *stream) { _stream = stream; }
  void Init(const CItem &item, Byte *destBuf, bool needChecksumCheck)
  {
    _size_Defined = false;
    _size = 0;
    _destBuf = NULL;
    if (!item.Is_UnknownSize())
    {
      _size_Defined = true;
      _size = item.Size;
      _destBuf = destBuf;
    }
    _pos = 0;
    if (needChecksumCheck)
      _hash.Init(item);
    else
      _hash.Init_NoCalc();
  }
  UInt64 GetPos() const { return _pos; }
};


Z7_COM7F_IMF(COutStreamWithHash::Write(const void *data, UInt32 size, UInt32 *processedSize))
{
  HRESULT result = S_OK;
  if (_size_Defined)
  {
    const UInt64 rem = _size - _pos;
    if (size > rem)
      size = (UInt32)rem;
  }
  if (_stream)
    result = _stream->Write(data, size, &size);
  if (_destBuf)
    memcpy(_destBuf + (size_t)_pos, data, size);
  _hash.Update(data, size);
  _pos += size;
  if (processedSize)
    *processedSize = size;
  return result;
}





class CInArchive
{
  CAlignedBuffer _buf;
  size_t _bufSize;
  size_t _bufPos;
  ISequentialInStream *_stream;

  CMyComPtr2<ICompressFilter, NCrypto::NRar5::CDecoder> m_CryptoDecoder;

  Z7_CLASS_NO_COPY(CInArchive)

  HRESULT ReadStream_Check(void *data, size_t size);

public:
  bool m_CryptoMode;

  bool WrongPassword;
  bool IsArc;
  bool UnexpectedEnd;

  UInt64 StreamStartPosition;
  UInt64 Position;
    
  size_t Get_Buf_RemainSize() const { return _bufSize - _bufPos; }
  bool Is_Buf_Finished() const { return _bufPos == _bufSize; }
  const Byte *Get_Buf_Data() const { return _buf + _bufPos; }
  void Move_BufPos(size_t num) { _bufPos += num; }
  bool ReadVar(UInt64 &val);

  struct CHeader
  {
    UInt64 Type;
    UInt64 Flags;
    size_t ExtraSize;
    UInt64 DataSize;
  };

  CInArchive() {}

  HRESULT ReadBlockHeader(CHeader &h);
  bool ReadFileHeader(const CHeader &header, CItem &item);
  void AddToSeekValue(UInt64 addValue)
  {
    Position += addValue;
  }

  HRESULT Open(IInStream *inStream, const UInt64 *searchHeaderSizeLimit, ICryptoGetTextPassword *getTextPassword,
      CInArcInfo &info);
};
  

static HRESULT MySetPassword(ICryptoGetTextPassword *getTextPassword, NCrypto::NRar5::CDecoder *cryptoDecoder)
{
  CMyComBSTR_Wipe password;
  RINOK(getTextPassword->CryptoGetTextPassword(&password))
  AString_Wipe utf8;
  const unsigned kPasswordLen_MAX = 127;
  UString_Wipe unicode;
  unicode.SetFromBstr(password);
  if (unicode.Len() > kPasswordLen_MAX)
    unicode.DeleteFrom(kPasswordLen_MAX);
  ConvertUnicodeToUTF8(unicode, utf8);
  cryptoDecoder->SetPassword((const Byte *)(const char *)utf8, utf8.Len());
  return S_OK;
}


bool CInArchive::ReadVar(UInt64 &val)
{
  const unsigned offset = ReadVarInt(Get_Buf_Data(), Get_Buf_RemainSize(), &val);
  Move_BufPos(offset);
  return (offset != 0);
}


HRESULT CInArchive::ReadStream_Check(void *data, size_t size)
{
  size_t size2 = size;
  RINOK(ReadStream(_stream, data, &size2))
  if (size2 == size)
    return S_OK;
  UnexpectedEnd = true;
  return S_FALSE;
}


HRESULT CInArchive::ReadBlockHeader(CHeader &h)
{
  h.Type = 0;
  h.Flags = 0;
  h.ExtraSize = 0;
  h.DataSize = 0;

  Byte buf[AES_BLOCK_SIZE];
  unsigned filled;
  
  if (m_CryptoMode)
  {
    _buf.AllocAtLeast(1 << 12); // at least (AES_BLOCK_SIZE * 2)
    if (!(Byte *)_buf)
      return E_OUTOFMEMORY;
    RINOK(ReadStream_Check(_buf, AES_BLOCK_SIZE * 2))
    memcpy(m_CryptoDecoder->_iv, _buf, AES_BLOCK_SIZE);
    RINOK(m_CryptoDecoder->Init())
    // we call RAR5_AES_Filter with:
    //   data_ptr  == aligned_ptr + 16
    //   data_size == 16
    if (m_CryptoDecoder->Filter(_buf + AES_BLOCK_SIZE, AES_BLOCK_SIZE) != AES_BLOCK_SIZE)
      return E_FAIL;
    memcpy(buf, _buf + AES_BLOCK_SIZE, AES_BLOCK_SIZE);
    filled = AES_BLOCK_SIZE;
  }
  else
  {
    const unsigned kStartSize = 4 + 3;
    RINOK(ReadStream_Check(buf, kStartSize))
    filled = kStartSize;
  }
  
  {
    UInt64 val;
    unsigned offset = ReadVarInt(buf + 4, 3, &val);
    if (offset == 0)
      return S_FALSE;
    size_t size = (size_t)val;
    if (size < 2)
      return S_FALSE;
    offset += 4;
    _bufPos = offset;
    size += offset;
    _bufSize = size;
    if (m_CryptoMode)
      size = (size + AES_BLOCK_SIZE - 1) & ~(size_t)(AES_BLOCK_SIZE - 1);
    _buf.AllocAtLeast(size);
    if (!(Byte *)_buf)
      return E_OUTOFMEMORY;
    memcpy(_buf, buf, filled);
    const size_t rem = size - filled;
    // if (m_CryptoMode), we add AES_BLOCK_SIZE here, because _iv is not included to size.
    AddToSeekValue(size + (m_CryptoMode ? AES_BLOCK_SIZE : 0));
    RINOK(ReadStream_Check(_buf + filled, rem))
    if (m_CryptoMode)
    {
      // we call RAR5_AES_Filter with:
      //   data_ptr  == aligned_ptr + 16
      //   (rem) can be big
      if (m_CryptoDecoder->Filter(_buf + filled, (UInt32)rem) != rem)
        return E_FAIL;
#if 1
      // optional 7zip-unrar check : remainder must contain zeros.
      const size_t pad = size - _bufSize;
      const Byte *p = _buf + _bufSize;
      for (size_t i = 0; i < pad; i++)
        if (p[i])
          return S_FALSE;
#endif
    }
  }

  if (CrcCalc(_buf + 4, _bufSize - 4) != Get32(buf))
    return S_FALSE;

  if (!ReadVar(h.Type)) return S_FALSE;
  if (!ReadVar(h.Flags)) return S_FALSE;

  if (h.Flags & NHeaderFlags::kExtra)
  {
    UInt64 extraSize;
    if (!ReadVar(extraSize))
      return S_FALSE;
    if (extraSize >= (1u << 21))
      return S_FALSE;
    h.ExtraSize = (size_t)extraSize;
  }
  
  if (h.Flags & NHeaderFlags::kData)
  {
    if (!ReadVar(h.DataSize))
      return S_FALSE;
  }
  
  if (h.ExtraSize > Get_Buf_RemainSize())
    return S_FALSE;
  return S_OK;
}


bool CInArcInfo::CLocator::Parse(const Byte *p, size_t size)
{
  Flags = 0;
  QuickOpen = 0;
  Recovery = 0;

  PARSE_VAR_INT(p, size, Flags)

  if (Is_QuickOpen())
  {
    PARSE_VAR_INT(p, size, QuickOpen)
  }
  if (Is_Recovery())
  {
    PARSE_VAR_INT(p, size, Recovery)
  }
#if 0
  // another records are possible in future rar formats.
  if (size != 0)
    return false;
#endif
  return true;
}


bool CInArcInfo::CMetadata::Parse(const Byte *p, size_t size)
{
  PARSE_VAR_INT(p, size, Flags)
  if (Flags & NMetadataFlags::kArcName)
  {
    UInt64 nameLen;
    PARSE_VAR_INT(p, size, nameLen)
    if (nameLen > size)
      return false;
    ArcName.SetFrom_CalcLen((const char *)(const void *)p, (unsigned)nameLen);
    p += (size_t)nameLen;
    size -= (size_t)nameLen;
  }
  if (Flags & NMetadataFlags::kCTime)
  {
    if ((Flags & NMetadataFlags::kUnixTime) &&
        (Flags & NMetadataFlags::kNanoSec) == 0)
    {
      if (size < 4)
        return false;
      CTime = GetUi32(p);
      p += 4;
      size -= 4;
    }
    else
    {
      if (size < 8)
        return false;
      CTime = GetUi64(p);
      p += 8;
      size -= 8;
    }
  }
#if 0
  // another records are possible in future rar formats.
  if (size != 0)
    return false;
#endif
  return true;
}


bool CInArcInfo::ParseExtra(const Byte *p, size_t size)
{
  for (;;)
  {
    if (size == 0)
      return true;
    UInt64 recSize64, id;
    PARSE_VAR_INT(p, size, recSize64)
    if (recSize64 > size)
      return false;
    size_t recSize = (size_t)recSize64;
    size -= recSize;
    // READ_VAR_INT(p, recSize, recSize)
    {
      const unsigned num = ReadVarInt(p, recSize, &id);
      if (num == 0)
        return false;
      p += num;
      recSize -= num;
    }
    if (id == kArcExtraRecordType_Metadata)
    {
      Metadata_Defined = true;
      if (!Metadata.Parse(p, recSize))
        Metadata_Error = true;
    }
    else if (id == kArcExtraRecordType_Locator)
    {
      Locator_Defined = true;
      if (!Locator.Parse(p, recSize))
        Locator_Error = true;
    }
    else
      UnknownExtraRecord = true;
    p += recSize;
  }
}



HRESULT CInArchive::Open(IInStream *stream, const UInt64 *searchHeaderSizeLimit, ICryptoGetTextPassword *getTextPassword,
    CInArcInfo &info)
{
  m_CryptoMode = false;
  
  WrongPassword = false;
  IsArc = false;
  UnexpectedEnd = false;

  Position = StreamStartPosition;

  UInt64 arcStartPos = StreamStartPosition;
  {
    Byte marker[kMarkerSize];
    RINOK(ReadStream_FALSE(stream, marker, kMarkerSize))
    if (memcmp(marker, kMarker, kMarkerSize) == 0)
      Position += kMarkerSize;
    else
    {
      if (searchHeaderSizeLimit && *searchHeaderSizeLimit == 0)
        return S_FALSE;
      RINOK(InStream_SeekSet(stream, StreamStartPosition))
      RINOK(FindSignatureInStream(stream, kMarker, kMarkerSize,
          searchHeaderSizeLimit, arcStartPos))
      arcStartPos += StreamStartPosition;
      Position = arcStartPos + kMarkerSize;
      RINOK(InStream_SeekSet(stream, Position))
    }
  }

  info.StartPos = arcStartPos;
  _stream = stream;

  CHeader h;
  RINOK(ReadBlockHeader(h))
  info.IsEncrypted = false;
  
  if (h.Type == NHeaderType::kArcEncrypt)
  {
    info.IsEncrypted = true;
    IsArc = true;
    if (!getTextPassword)
      return E_NOTIMPL;
    m_CryptoMode = true;
    m_CryptoDecoder.Create_if_Empty();
    RINOK(m_CryptoDecoder->SetDecoderProps(
        Get_Buf_Data(), (unsigned)Get_Buf_RemainSize(), false, false))
    RINOK(MySetPassword(getTextPassword, m_CryptoDecoder.ClsPtr()))
    if (!m_CryptoDecoder->CalcKey_and_CheckPassword())
    {
      WrongPassword = True;
      return S_FALSE;
    }
    RINOK(ReadBlockHeader(h))
  }

  if (h.Type != NHeaderType::kArc)
    return S_FALSE;

  IsArc = true;
  info.VolNumber = 0;
  
  if (!ReadVar(info.Flags))
    return S_FALSE;
  
  if (info.Flags & NArcFlags::kVolNumber)
    if (!ReadVar(info.VolNumber))
      return S_FALSE;
  
  if (h.ExtraSize != Get_Buf_RemainSize())
    return S_FALSE;
  if (h.ExtraSize)
  {
    if (!info.ParseExtra(Get_Buf_Data(), h.ExtraSize))
      info.Extra_Error = true;
  }
  return S_OK;
}


bool CInArchive::ReadFileHeader(const CHeader &header, CItem &item)
{
  item.CommonFlags = (UInt32)header.Flags;
  item.PackSize = header.DataSize;
  item.UnixMTime = 0;
  item.CRC = 0;

  {
    UInt64 flags64;
    if (!ReadVar(flags64)) return false;
    item.Flags = (UInt32)flags64;
  }

  if (!ReadVar(item.Size)) return false;
  
  {
    UInt64 attrib;
    if (!ReadVar(attrib)) return false;
    item.Attrib = (UInt32)attrib;
  }
  if (item.Has_UnixMTime())
  {
    if (Get_Buf_RemainSize() < 4)
      return false;
    item.UnixMTime = Get32(Get_Buf_Data());
    Move_BufPos(4);
  }
  if (item.Has_CRC())
  {
    if (Get_Buf_RemainSize() < 4)
      return false;
    item.CRC = Get32(Get_Buf_Data());
    Move_BufPos(4);
  }
  {
    UInt64 method;
    if (!ReadVar(method)) return false;
    item.Method = (UInt32)method;
  }

  if (!ReadVar(item.HostOS)) return false;

  {
    UInt64 len;
    if (!ReadVar(len)) return false;
    if (len > Get_Buf_RemainSize())
      return false;
    item.Name.SetFrom_CalcLen((const char *)Get_Buf_Data(), (unsigned)len);
    Move_BufPos((size_t)len);
  }
  
  item.Extra.Free();
  const size_t extraSize = header.ExtraSize;
  if (extraSize != 0)
  {
    if (Get_Buf_RemainSize() < extraSize)
      return false;
    item.Extra.Alloc(extraSize);
    memcpy(item.Extra, Get_Buf_Data(), extraSize);
    Move_BufPos(extraSize);
  }
  
  return Is_Buf_Finished();
}



struct CLinkFile
{
  unsigned Index;
  unsigned NumLinks; // the number of links to Data
  CByteBuffer Data;
  HRESULT Res;
  bool crcOK;

  CLinkFile(): Index(0), NumLinks(0), Res(S_OK), crcOK(true) {}
};


struct CUnpacker
{
  CMyComPtr2<ICompressCoder, NCompress::CCopyCoder> copyCoder;
  CMyComPtr<ICompressCoder> LzCoders[2];
  bool SolidAllowed;
  bool NeedCrc;
  CFilterCoder *filterStreamSpec;
  CMyComPtr<ISequentialInStream> filterStream;
  CMyComPtr2<ICompressFilter, NCrypto::NRar5::CDecoder> cryptoDecoder;
  CMyComPtr<ICryptoGetTextPassword> getTextPassword;
  CMyComPtr2<ISequentialOutStream, COutStreamWithHash> outStream;

  CByteBuffer _tempBuf;
  CLinkFile *linkFile;

  CUnpacker(): linkFile(NULL) { SolidAllowed = false; NeedCrc = true; }

  HRESULT Create(DECL_EXTERNAL_CODECS_LOC_VARS
      const CItem &item, bool isSolid, bool &wrongPassword);

  HRESULT Code(const CItem &item, const CItem &lastItem, UInt64 packSize,
      ISequentialInStream *inStream, ISequentialOutStream *outStream, ICompressProgressInfo *progress,
      bool &isCrcOK);

  HRESULT DecodeToBuf(DECL_EXTERNAL_CODECS_LOC_VARS
      const CItem &item, UInt64 packSize, ISequentialInStream *inStream, CByteBuffer &buffer);
};


static const unsigned kLzMethodMax = 5;

HRESULT CUnpacker::Create(DECL_EXTERNAL_CODECS_LOC_VARS
    const CItem &item, bool isSolid, bool &wrongPassword)
{
  wrongPassword = false;

  if (item.Get_AlgoVersion_RawBits() > 1)
    return E_NOTIMPL;

  outStream.Create_if_Empty();

  const unsigned method = item.Get_Method();

  if (method == 0)
    copyCoder.Create_if_Empty();
  else
  {
    if (method > kLzMethodMax)
      return E_NOTIMPL;
    /*
    if (item.IsSplitBefore())
      return S_FALSE;
    */
    const unsigned lzIndex = item.IsService() ? 1 : 0;
    CMyComPtr<ICompressCoder> &lzCoder = LzCoders[lzIndex];
    if (!lzCoder)
    {
      const UInt32 methodID = 0x40305;
      RINOK(CreateCoder_Id(EXTERNAL_CODECS_LOC_VARS methodID, false, lzCoder))
      if (!lzCoder)
        return E_NOTIMPL;
    }

    CMyComPtr<ICompressSetDecoderProperties2> csdp;
    RINOK(lzCoder.QueryInterface(IID_ICompressSetDecoderProperties2, &csdp))
    if (!csdp)
      return E_NOTIMPL;
    const unsigned ver = item.Get_AlgoVersion_HuffRev();
    if (ver > 1)
      return E_NOTIMPL;
    const Byte props[2] =
    {
      (Byte)item.Get_DictSize_Main(),
      (Byte)((item.Get_DictSize_Frac() << 3) + (ver << 1) + (isSolid ? 1 : 0))
    };
    RINOK(csdp->SetDecoderProperties2(props, 2))
  }

  unsigned cryptoSize = 0;
  const int cryptoOffset = item.FindExtra(NExtraID::kCrypto, cryptoSize);

  if (cryptoOffset >= 0)
  {
    if (!filterStream)
    {
      filterStreamSpec = new CFilterCoder(false);
      filterStream = filterStreamSpec;
    }

    cryptoDecoder.Create_if_Empty();

    RINOK(cryptoDecoder->SetDecoderProps(item.Extra + (unsigned)cryptoOffset, cryptoSize, true, item.IsService()))

    if (!getTextPassword)
    {
      wrongPassword = True;
      return E_NOTIMPL;
    }

    RINOK(MySetPassword(getTextPassword, cryptoDecoder.ClsPtr()))
      
    if (!cryptoDecoder->CalcKey_and_CheckPassword())
      wrongPassword = True;
  }

  return S_OK;
}


HRESULT CUnpacker::Code(const CItem &item, const CItem &lastItem, UInt64 packSize,
    ISequentialInStream *volsInStream, ISequentialOutStream *realOutStream, ICompressProgressInfo *progress,
    bool &isCrcOK)
{
  isCrcOK = true;

  const unsigned method = item.Get_Method();
  if (method > kLzMethodMax)
    return E_NOTIMPL;

  const bool needBuf = (linkFile && linkFile->NumLinks != 0);

  if (needBuf && !lastItem.Is_UnknownSize())
  {
    const size_t dataSize = (size_t)lastItem.Size;
    if (dataSize != lastItem.Size)
      return E_NOTIMPL;
    linkFile->Data.Alloc(dataSize);
  }

  bool isCryptoMode = false;
  ISequentialInStream *inStream;

  if (item.IsEncrypted())
  {
    filterStreamSpec->Filter = cryptoDecoder;
    filterStreamSpec->SetInStream(volsInStream);
    filterStreamSpec->SetOutStreamSize(NULL);
    inStream = filterStream;
    isCryptoMode = true;
  }
  else
    inStream = volsInStream;

  ICompressCoder *commonCoder = (method == 0) ?
      copyCoder.Interface() :
      LzCoders[item.IsService() ? 1 : 0].Interface();

  outStream->SetStream(realOutStream);
  outStream->Init(lastItem, (needBuf ? (Byte *)linkFile->Data : NULL), NeedCrc);

  HRESULT res = S_OK;
  if (packSize != 0 || lastItem.Is_UnknownSize() || lastItem.Size != 0)
  {
    res = commonCoder->Code(inStream, outStream, &packSize,
      lastItem.Is_UnknownSize() ? NULL : &lastItem.Size, progress);
    if (!item.IsService())
      SolidAllowed = true;
  }
  else
  {
    // res = res;
  }

  if (isCryptoMode)
    filterStreamSpec->ReleaseInStream();

  const UInt64 processedSize = outStream->GetPos();
  if (res == S_OK && !lastItem.Is_UnknownSize() && processedSize != lastItem.Size)
    res = S_FALSE;

  // if (res == S_OK)
  {
    unsigned cryptoSize = 0;
    const int cryptoOffset = lastItem.FindExtra(NExtraID::kCrypto, cryptoSize);
    NCrypto::NRar5::CDecoder *crypto = NULL;
    if (cryptoOffset >= 0)
    {
      CCryptoInfo cryptoInfo;
      if (cryptoInfo.Parse(lastItem.Extra + (unsigned)cryptoOffset, cryptoSize))
        if (cryptoInfo.UseMAC())
          crypto = cryptoDecoder.ClsPtr();
    }
    if (NeedCrc)
      isCrcOK =  outStream->_hash.Check(lastItem, crypto);
  }

  if (linkFile)
  {
    linkFile->Res = res;
    linkFile->crcOK = isCrcOK;
    if (needBuf
        && !lastItem.Is_UnknownSize()
        && processedSize != lastItem.Size)
      linkFile->Data.ChangeSize_KeepData((size_t)processedSize, (size_t)processedSize);
  }

  return res;
}


HRESULT CUnpacker::DecodeToBuf(DECL_EXTERNAL_CODECS_LOC_VARS
    const CItem &item, UInt64 packSize,
    ISequentialInStream *inStream,
    CByteBuffer &buffer)
{
  CMyComPtr2_Create<ISequentialOutStream, CBufPtrSeqOutStream> out;
  _tempBuf.AllocAtLeast((size_t)item.Size);
  out->Init(_tempBuf, (size_t)item.Size);

  bool wrongPassword;

  if (item.IsSolid())
    return E_NOTIMPL;

  HRESULT res = Create(EXTERNAL_CODECS_LOC_VARS item, item.IsSolid(), wrongPassword);
  
  if (res == S_OK)
  {
    if (wrongPassword)
      return S_FALSE;

    CMyComPtr2_Create<ISequentialInStream, CLimitedSequentialInStream> limitedStream;
    limitedStream->SetStream(inStream);
    limitedStream->Init(packSize);

    bool crcOK = true;
    res = Code(item, item, packSize, limitedStream, out, NULL, crcOK);
    if (res == S_OK)
    {
      if (!crcOK || out->GetPos() != item.Size)
        res = S_FALSE;
      else
        buffer.CopyFrom(_tempBuf, (size_t)item.Size);
    }
  }
  
  return res;
}


struct CTempBuf
{
  CByteBuffer _buf;
  size_t _offset;
  bool _isOK;

  void Clear()
  {
    _offset = 0;
    _isOK = true;
  }

  CTempBuf() { Clear(); }

  HRESULT Decode(DECL_EXTERNAL_CODECS_LOC_VARS
      const CItem &item,
      ISequentialInStream *inStream,
      CUnpacker &unpacker,
      CByteBuffer &destBuf);
};


HRESULT CTempBuf::Decode(DECL_EXTERNAL_CODECS_LOC_VARS
    const CItem &item,
    ISequentialInStream *inStream,
    CUnpacker &unpacker,
    CByteBuffer &destBuf)
{
  const size_t kPackSize_Max = (1 << 24);
  if (item.Size > (1 << 24)
      || item.Size == 0
      || item.PackSize >= kPackSize_Max)
  {
    Clear();
    return S_OK;
  }

  if (item.IsSplit() /* && _isOK */)
  {
    size_t packSize = (size_t)item.PackSize;
    if (packSize > kPackSize_Max - _offset)
      return S_OK;
    size_t newSize = _offset + packSize;
    if (newSize > _buf.Size())
      _buf.ChangeSize_KeepData(newSize, _offset);
    
    Byte *data = (Byte *)_buf + _offset;
    RINOK(ReadStream_FALSE(inStream, data, packSize))
    
    _offset += packSize;
    
    if (item.IsSplitAfter())
    {
      CHash hash;
      hash.Init(item);
      hash.Update(data, packSize);
      _isOK = hash.Check(item, NULL); // RAR5 doesn't use HMAC for packed part
    }
  }
  
  if (_isOK)
  {
    if (!item.IsSplitAfter())
    {
      if (_offset == 0)
      {
        RINOK(unpacker.DecodeToBuf(EXTERNAL_CODECS_LOC_VARS
            item, item.PackSize, inStream, destBuf))
      }
      else
      {
        CMyComPtr2_Create<ISequentialInStream, CBufInStream> bufInStream;
        bufInStream->Init(_buf, _offset);
        RINOK(unpacker.DecodeToBuf(EXTERNAL_CODECS_LOC_VARS
            item, _offset, bufInStream, destBuf))
      }
    }
  }

  return S_OK;
}



static const Byte kProps[] =
{
  kpidPath,
  kpidIsDir,
  kpidSize,
  kpidPackSize,
  kpidMTime,
  kpidCTime,
  kpidATime,
  kpidAttrib,
  // kpidPosixAttrib, // for debug

  kpidIsAltStream,
  kpidEncrypted,
  kpidSolid,
  kpidSplitBefore,
  kpidSplitAfter,
  kpidCRC,
  kpidHostOS,
  kpidMethod,
  kpidCharacts,
  kpidSymLink,
  kpidHardLink,
  kpidCopyLink,

  kpidVolumeIndex
};


static const Byte kArcProps[] =
{
  kpidTotalPhySize,
  kpidCharacts,
  kpidEncrypted,
  kpidSolid,
  kpidNumBlocks,
  kpidMethod,
  kpidIsVolume,
  kpidVolumeIndex,
  kpidNumVolumes,
  kpidName,
  kpidCTime,
  kpidComment
};


IMP_IInArchive_Props
IMP_IInArchive_ArcProps


UInt64 CHandler::GetPackSize(unsigned refIndex) const
{
  UInt64 size = 0;
  unsigned index = _refs[refIndex].Item;
  for (;;)
  {
    const CItem &item = _items[index];
    size += item.PackSize;
    if (item.NextItem < 0)
      return size;
    index = (unsigned)item.NextItem;
  }
}

static char *PrintDictSize(char *s, UInt64 w)
{
  char                               c = 'K'; w >>= 10;
  if ((w & ((1 << 10) - 1)) == 0)  { c = 'M'; w >>= 10;
  if ((w & ((1 << 10) - 1)) == 0)  { c = 'G'; w >>= 10; }}
  s = ConvertUInt64ToString(w, s);
  *s++ = c;
  *s = 0;
  return s;
}


Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN

  NCOM::CPropVariant prop;

  const CInArcInfo *arcInfo = NULL;
  if (!_arcs.IsEmpty())
    arcInfo = &_arcs[0].Info;

  switch (propID)
  {
    case kpidVolumeIndex: if (arcInfo && arcInfo->IsVolume()) prop = arcInfo->GetVolIndex(); break;
    case kpidSolid: if (arcInfo) prop = arcInfo->IsSolid(); break;
    case kpidCharacts:
    {
      AString s;
      if (arcInfo)
      {
        s = FlagsToString(k_ArcFlags, Z7_ARRAY_SIZE(k_ArcFlags), (UInt32)arcInfo->Flags);
        if (arcInfo->Extra_Error)
          s.Add_OptSpaced("Extra-ERROR");
        if (arcInfo->UnsupportedFeature)
          s.Add_OptSpaced("unsupported-feature");
        if (arcInfo->Metadata_Defined)
        {
          s.Add_OptSpaced("Metadata");
          if (arcInfo->Metadata_Error)
            s += "-ERROR";
          else
          {
            if (arcInfo->Metadata.Flags & NMetadataFlags::kArcName)
              s.Add_OptSpaced("arc-name");
            if (arcInfo->Metadata.Flags & NMetadataFlags::kCTime)
            {
              s.Add_OptSpaced("ctime-");
              s +=
                (arcInfo->Metadata.Flags & NMetadataFlags::kUnixTime) ?
                (arcInfo->Metadata.Flags & NMetadataFlags::kNanoSec) ?
                    "1ns" : "1s" : "win";
            }
          }
        }
        if (arcInfo->Locator_Defined)
        {
          s.Add_OptSpaced("Locator");
          if (arcInfo->Locator_Error)
            s += "-ERROR";
          else
          {
            if (arcInfo->Locator.Is_QuickOpen())
            {
              s.Add_OptSpaced("QuickOpen:");
              s.Add_UInt64(arcInfo->Locator.QuickOpen);
            }
            if (arcInfo->Locator.Is_Recovery())
            {
              s.Add_OptSpaced("Recovery:");
              s.Add_UInt64(arcInfo->Locator.Recovery);
            }
          }
        }
        if (arcInfo->UnknownExtraRecord)
          s.Add_OptSpaced("Unknown-Extra-Record");

      }
      if (_comment_WasUsedInArc)
      {
        s.Add_OptSpaced("Comment");
        // s.Add_UInt32((UInt32)_comment.Size());
      }
      //
      if (_acls.Size() != 0)
      {
        s.Add_OptSpaced("ACL");
        // s.Add_UInt32(_acls.Size());
      }
      if (!s.IsEmpty())
        prop = s;
      break;
    }
    case kpidEncrypted: if (arcInfo) prop = arcInfo->IsEncrypted; break; // it's for encrypted names.
    case kpidIsVolume: if (arcInfo) prop = arcInfo->IsVolume(); break;
    case kpidNumVolumes: prop = (UInt32)_arcs.Size(); break;
    case kpidOffset: if (arcInfo && arcInfo->StartPos != 0) prop = arcInfo->StartPos; break;

    case kpidTotalPhySize:
    {
      if (_arcs.Size() > 1)
      {
        UInt64 sum = 0;
        FOR_VECTOR (v, _arcs)
          sum += _arcs[v].Info.GetPhySize();
        prop = sum;
      }
      break;
    }

    case kpidPhySize:
    {
      if (arcInfo)
        prop = arcInfo->GetPhySize();
      break;
    }

    case kpidName:
      if (arcInfo)
      if (!arcInfo->Metadata_Error
          && !arcInfo->Metadata.ArcName.IsEmpty())
      {
        UString s;
        if (ConvertUTF8ToUnicode(arcInfo->Metadata.ArcName, s))
          prop = s;
      }
      break;

    case kpidCTime:
      if (arcInfo)
      if (!arcInfo->Metadata_Error
          && (arcInfo->Metadata.Flags & NMetadataFlags::kCTime))
      {
        const UInt64 ct = arcInfo->Metadata.CTime;
        if (arcInfo->Metadata.Flags & NMetadataFlags::kUnixTime)
        {
          if (arcInfo->Metadata.Flags & NMetadataFlags::kNanoSec)
          {
            const UInt64 sec = ct / 1000000000;
            const UInt64 ns  = ct % 1000000000;
            UInt64 wt = NTime::UnixTime64_To_FileTime64((Int64)sec);
            wt += ns / 100;
            const unsigned ns100 = (unsigned)(ns % 100);
            FILETIME ft;
            ft.dwLowDateTime = (DWORD)(UInt32)wt;
            ft.dwHighDateTime = (DWORD)(UInt32)(wt >> 32);
            prop.SetAsTimeFrom_FT_Prec_Ns100(ft, k_PropVar_TimePrec_1ns, ns100);
          }
          else
          {
            const UInt64 wt = NTime::UnixTime64_To_FileTime64((Int64)ct);
            prop.SetAsTimeFrom_Ft64_Prec(wt, k_PropVar_TimePrec_Unix);
          }
        }
        else
          prop.SetAsTimeFrom_Ft64_Prec(ct, k_PropVar_TimePrec_100ns);
      }
      break;

    case kpidComment:
    {
      // if (!_arcs.IsEmpty())
      {
        // const CArc &arc = _arcs[0];
        const CByteBuffer &cmt = _comment;
        if (cmt.Size() != 0 /* && cmt.Size() < (1 << 16) */)
        {
          AString s;
          s.SetFrom_CalcLen((const char *)(const Byte *)cmt, (unsigned)cmt.Size());
          UString unicode;
          ConvertUTF8ToUnicode(s, unicode);
          prop = unicode;
        }
      }
      break;
    }

    case kpidNumBlocks:
    {
      prop = (UInt32)_numBlocks;
      break;
    }

    case kpidMethod:
    {
      AString s;

      UInt64 algo = _algo_Mask;
      for (unsigned v = 0; algo != 0; v++, algo >>= 1)
      {
        if ((algo & 1) == 0)
          continue;
        s.Add_OptSpaced("v");
        s.Add_UInt32(v + 6);
        if (v < Z7_ARRAY_SIZE(_methodMasks))
        {
          const UInt64 dict = _dictMaxSizes[v];
          if (dict)
          {
            char temp[24];
            temp[0] = ':';
            PrintDictSize(temp + 1, dict);
            s += temp;
          }
          unsigned method = _methodMasks[v];
          for (unsigned m = 0; method; m++, method >>= 1)
          {
            if ((method & 1) == 0)
              continue;
            s += ":m";
            s.Add_UInt32(m);
          }
        }
      }
      if (_rar5comapt_mask & 2)
      {
        s += ":c";
        if (_rar5comapt_mask & 1)
          s.Add_Char('n');
      }
      prop = s;
      break;
    }
    
    case kpidError:
    {
      if (/* &_missingVol || */ !_missingVolName.IsEmpty())
      {
        UString s ("Missing volume : ");
        s += _missingVolName;
        prop = s;
      }
      break;
    }

    case kpidErrorFlags:
    {
      UInt32 v = _errorFlags;
      if (!_isArc)
        v |= kpv_ErrorFlags_IsNotArc;
      if (_error_in_ACL)
        v |= kpv_ErrorFlags_HeadersError;
      if (_split_Error)
        v |= kpv_ErrorFlags_HeadersError;
      prop = v;
      break;
    }

    /*
    case kpidWarningFlags:
    {
      if (_warningFlags != 0)
        prop = _warningFlags;
      break;
    }
    */

    case kpidExtension:
      if (_arcs.Size() == 1)
      {
        if (arcInfo->IsVolume())
        {
          AString s ("part");
          UInt32 v = (UInt32)arcInfo->GetVolIndex() + 1;
          if (v < 10)
            s.Add_Char('0');
          s.Add_UInt32(v);
          s += ".rar";
          prop = s;
        }
      }
      break;

    case kpidIsAltStream: prop = true; break;
  }

  prop.Detach(value);
  return S_OK;
  
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = (UInt32)_refs.Size();
  return S_OK;
}


static const Byte kRawProps[] =
{
  kpidChecksum,
  kpidNtSecure
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
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetParent(UInt32 index, UInt32 *parent, UInt32 *parentType))
{
  *parentType = NParentType::kDir;
  *parent = (UInt32)(Int32)-1;

  if (index >= _refs.Size())
    return S_OK;

  const CRefItem &ref = _refs[index];
  const CItem &item = _items[ref.Item];

  if (item.Is_STM() && ref.Parent >= 0)
  {
    *parent = (UInt32)ref.Parent;
    *parentType = NParentType::kAltStream;
  }

  return S_OK;
}


Z7_COM7F_IMF(CHandler::GetRawProp(UInt32 index, PROPID propID, const void **data, UInt32 *dataSize, UInt32 *propType))
{
  *data = NULL;
  *dataSize = 0;
  *propType = 0;

  if (index >= _refs.Size())
    return E_INVALIDARG;

  const CItem &item = _items[_refs[index].Item];

  if (propID == kpidNtSecure)
  {
    if (item.ACL >= 0)
    {
      const CByteBuffer &buf = _acls[item.ACL];
      *dataSize = (UInt32)buf.Size();
      *propType = NPropDataType::kRaw;
      *data = (const Byte *)buf;
    }
    return S_OK;
  }
  
  if (propID == kpidChecksum)
  {
    const int hashRecOffset = item.FindExtra_Blake();
    if (hashRecOffset >= 0)
    {
      *dataSize = Z7_BLAKE2S_DIGEST_SIZE;
      *propType = NPropDataType::kRaw;
      *data = item.Extra + (unsigned)hashRecOffset;
    }
    /*
    else if (item.Has_CRC() && item.IsEncrypted())
    {
      *dataSize = 4;
      *propType = NPropDataType::kRaw;
      *data = &item->CRC; // we must show same value for big/little endian here
    }
    */
    return S_OK;
  }
  
  return S_OK;
}


static void TimeRecordToProp(const CItem &item, unsigned stampIndex, NCOM::CPropVariant &prop)
{
  unsigned size;
  const int offset = item.FindExtra(NExtraID::kTime, size);
  if (offset < 0)
    return;

  const Byte *p = item.Extra + (unsigned)offset;
  UInt64 flags;
  // PARSE_VAR_INT(p, size, flags)
  {
    const unsigned num = ReadVarInt(p, size, &flags);
    if (num == 0)
      return;
    p += num;
    size -= num;
  }

  if ((flags & (NTimeRecord::NFlags::kMTime << stampIndex)) == 0)
    return;
  
  unsigned numStamps = 0;
  unsigned curStamp = 0;

  for (unsigned i = 0; i < 3; i++)
    if ((flags & (NTimeRecord::NFlags::kMTime << i)) != 0)
    {
      if (i == stampIndex)
        curStamp = numStamps;
      numStamps++;
    }

  FILETIME ft;

  unsigned timePrec = 0;
  unsigned ns100 = 0;

  if ((flags & NTimeRecord::NFlags::kUnixTime) != 0)
  {
    curStamp *= 4;
    if (curStamp + 4 > size)
      return;
    p += curStamp;
    UInt64 val = NTime::UnixTime_To_FileTime64(Get32(p));
    numStamps *= 4;
    timePrec = k_PropVar_TimePrec_Unix;
    if ((flags & NTimeRecord::NFlags::kUnixNs) != 0 && numStamps * 2 <= size)
    {
      const UInt32 ns = Get32(p + numStamps) & 0x3FFFFFFF;
      if (ns < 1000000000)
      {
        val += ns / 100;
        ns100 = (unsigned)(ns % 100);
        timePrec = k_PropVar_TimePrec_1ns;
      }
    }
    ft.dwLowDateTime = (DWORD)val;
    ft.dwHighDateTime = (DWORD)(val >> 32);
  }
  else
  {
    curStamp *= 8;
    if (curStamp + 8 > size)
      return;
    p += curStamp;
    ft.dwLowDateTime = Get32(p);
    ft.dwHighDateTime = Get32(p + 4);
  }
  
  prop.SetAsTimeFrom_FT_Prec_Ns100(ft, timePrec, ns100);
}


Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  
  NCOM::CPropVariant prop;
  const CRefItem &ref = _refs[index];
  const CItem &item = _items[ref.Item];
  const CItem &lastItem = _items[ref.Last];

  switch (propID)
  {
    case kpidPath:
    {
      UString unicodeName;
      
      if (item.Is_STM())
      {
        AString s;
        if (ref.Parent >= 0)
        {
          const CItem &mainItem = _items[_refs[ref.Parent].Item];
          s = mainItem.Name;
        }

        AString name;
        item.GetAltStreamName(name);
        if (name[0] != ':')
          s.Add_Colon();
        s += name;
        ConvertUTF8ToUnicode(s, unicodeName);
      }
      else
      {
        ConvertUTF8ToUnicode(item.Name, unicodeName);

        if (item.Version_Defined)
        {
          char temp[32];
          // temp[0] = ';';
          // ConvertUInt64ToString(item.Version, temp + 1);
          // unicodeName += temp;
          ConvertUInt64ToString(item.Version, temp);
          UString s2 ("[VER]" STRING_PATH_SEPARATOR);
          s2 += temp;
          s2.Add_PathSepar();
          unicodeName.Insert(0, s2);
        }
      }
      
      NItemName::ReplaceToOsSlashes_Remove_TailSlash(unicodeName);
      prop = unicodeName;

      break;
    }
    
    case kpidIsDir: prop = item.IsDir(); break;
    case kpidSize: if (!lastItem.Is_UnknownSize()) prop = lastItem.Size; break;
    case kpidPackSize: prop = GetPackSize((unsigned)index); break;
    
    case kpidMTime:
    {
      TimeRecordToProp(item, NTimeRecord::k_Index_MTime, prop);
      if (prop.vt == VT_EMPTY && item.Has_UnixMTime())
        PropVariant_SetFrom_UnixTime(prop, item.UnixMTime);
      if (prop.vt == VT_EMPTY && ref.Parent >= 0)
      {
        const CItem &baseItem = _items[_refs[ref.Parent].Item];
        TimeRecordToProp(baseItem, NTimeRecord::k_Index_MTime, prop);
        if (prop.vt == VT_EMPTY && baseItem.Has_UnixMTime())
          PropVariant_SetFrom_UnixTime(prop, baseItem.UnixMTime);
      }
      break;
    }
    case kpidCTime: TimeRecordToProp(item, NTimeRecord::k_Index_CTime, prop); break;
    case kpidATime: TimeRecordToProp(item, NTimeRecord::k_Index_ATime, prop); break;

    case kpidName:
    {
      if (item.Is_STM())
      {
        AString name;
        item.GetAltStreamName(name);
        if (name[0] == ':')
        {
          name.DeleteFrontal(1);
          UString unicodeName;
          ConvertUTF8ToUnicode(name, unicodeName);
          prop = unicodeName;
        }
      }
      break;
    }

    case kpidIsAltStream: prop = item.Is_STM(); break;

    case kpidSymLink: item.Link_to_Prop(NLinkType::kUnixSymLink, prop); break;
    case kpidHardLink: item.Link_to_Prop(NLinkType::kHardLink, prop); break;
    case kpidCopyLink: item.Link_to_Prop(NLinkType::kFileCopy, prop); break;

    case kpidAttrib: prop = item.GetWinAttrib(); break;
    case kpidPosixAttrib:
      if (item.HostOS == kHost_Unix)
        prop = (UInt32)item.Attrib;
      break;

    case kpidEncrypted: prop = item.IsEncrypted(); break;
    case kpidSolid: prop = item.IsSolid(); break;

    case kpidSplitBefore: prop = item.IsSplitBefore(); break;
    case kpidSplitAfter: prop = lastItem.IsSplitAfter(); break;

    case kpidVolumeIndex:
    {
      if (item.VolIndex < _arcs.Size())
      {
        const CInArcInfo &arcInfo = _arcs[item.VolIndex].Info;
        if (arcInfo.IsVolume())
          prop = (UInt64)arcInfo.GetVolIndex();
      }
      break;
    }

    case kpidCRC:
    {
      const CItem *item2 = (lastItem.IsSplitAfter() ? &item : &lastItem);
      // we don't want to show crc for encrypted file here,
      // because crc is also encrrypted.
      if (item2->Has_CRC() && !item2->IsEncrypted())
        prop = item2->CRC;
      break;
    }

    case kpidMethod:
    {
      char temp[128];
      const unsigned algo = item.Get_AlgoVersion_RawBits();
      char *s = temp;
      // if (algo != 0)
      {
        *s++ = 'v';
        s = ConvertUInt32ToString((UInt32)algo + 6, s);
        if (item.Is_Rar5_Compat())
          *s++ = 'c';
        *s++ = ':';
      }
      {
        const unsigned m = item.Get_Method();
        *s++ = 'm';
        *s++ = (char)(m + '0');
        if (!item.IsDir())
        {
          *s++ = ':';
          const unsigned dictMain = item.Get_DictSize_Main();
          const unsigned frac = item.Get_DictSize_Frac();
          /*
          if (frac == 0 && algo == 0)
            s = ConvertUInt32ToString(dictMain + 17, s);
          else
          */
          s = PrintDictSize(s, (UInt64)(32 + frac) << (12 + dictMain));
          if (item.Is_Rar5_Compat())
          {
            *s++ = ':';
            *s++ = 'c';
          }
        }
      }
      unsigned cryptoSize = 0;
      const int cryptoOffset = item.FindExtra(NExtraID::kCrypto, cryptoSize);
      if (cryptoOffset >= 0)
      {
        *s++ = ' ';
        CCryptoInfo cryptoInfo;
        const bool isOK = cryptoInfo.Parse(item.Extra + (unsigned)cryptoOffset, cryptoSize);
        if (cryptoInfo.Algo == 0)
          s = MyStpCpy(s, "AES");
        else
        {
          s = MyStpCpy(s, "Crypto_");
          s = ConvertUInt64ToString(cryptoInfo.Algo, s);
        }
        if (isOK)
        {
          *s++ = ':';
          s = ConvertUInt32ToString(cryptoInfo.Cnt, s);
          *s++ = ':';
          s = ConvertUInt64ToString(cryptoInfo.Flags, s);
        }
      }
      *s = 0;
      prop = temp;
      break;
    }
    
    case kpidCharacts:
    {
      AString s;

      if (item.ACL >= 0)
        s.Add_OptSpaced("ACL");

      const UInt32 flags = item.Flags;
      if (flags != 0)
      {
        const AString s2 = FlagsToString(k_FileFlags, Z7_ARRAY_SIZE(k_FileFlags), flags);
        if (!s2.IsEmpty())
          s.Add_OptSpaced(s2);
      }

      item.PrintInfo(s);
    
      if (!s.IsEmpty())
        prop = s;
      break;
    }


    case kpidHostOS:
      if (item.HostOS < Z7_ARRAY_SIZE(kHostOS))
        prop = kHostOS[(size_t)item.HostOS];
      else
        prop = (UInt64)item.HostOS;
      break;
  }
  
  prop.Detach(value);
  return S_OK;
  
  COM_TRY_END
}



// ---------- Copy Links ----------

static int CompareItemsPaths(const CHandler &handler, unsigned p1, unsigned p2, const AString *name1)
{
  const CItem &item1 = handler._items[handler._refs[p1].Item];
  const CItem &item2 = handler._items[handler._refs[p2].Item];
  
  if (item1.Version_Defined)
  {
    if (!item2.Version_Defined)
      return -1;
    const int res = MyCompare(item1.Version, item2.Version);
    if (res != 0)
      return res;
  }
  else if (item2.Version_Defined)
    return 1;

  if (!name1)
    name1 = &item1.Name;
  return strcmp(*name1, item2.Name);
}

static int CompareItemsPaths2(const CHandler &handler, unsigned p1, unsigned p2, const AString *name1)
{
  const int res = CompareItemsPaths(handler, p1, p2, name1);
  if (res != 0)
    return res;
  return MyCompare(p1, p2);
}

static int CompareItemsPaths_Sort(const unsigned *p1, const unsigned *p2, void *param)
{
  return CompareItemsPaths2(*(const CHandler *)param, *p1, *p2, NULL);
}

static int FindLink(const CHandler &handler, const CUIntVector &sorted,
    const AString &s, unsigned index)
{
  unsigned left = 0, right = sorted.Size();
  for (;;)
  {
    if (left == right)
    {
      if (left > 0)
      {
        const unsigned refIndex = sorted[left - 1];
        if (CompareItemsPaths(handler, index, refIndex, &s) == 0)
          return (int)refIndex;
      }
      if (right < sorted.Size())
      {
        const unsigned refIndex = sorted[right];
        if (CompareItemsPaths(handler, index, refIndex, &s) == 0)
          return (int)refIndex;
      }
      return -1;
    }

    const unsigned mid = (left + right) / 2;
    const unsigned refIndex = sorted[mid];
    const int compare = CompareItemsPaths2(handler, index, refIndex, &s);
    if (compare == 0)
      return (int)refIndex;
    if (compare < 0)
      right = mid;
    else
      left = mid + 1;
  }
}

void CHandler::FillLinks()
{
  unsigned i;

  bool need_FillLinks = false;

  for (i = 0; i < _refs.Size(); i++)
  {
    const CItem &item = _items[_refs[i].Item];
    if (!item.IsDir()
        && !item.IsService()
        && item.NeedUse_as_CopyLink())
      need_FillLinks = true;

    if (!item.IsSolid())
      _numBlocks++;

    const unsigned algo = item.Get_AlgoVersion_RawBits();
    _algo_Mask |= (UInt64)1 << algo;
    _rar5comapt_mask |= 1u << item.Get_Rar5_CompatBit();
    if (!item.IsDir() && algo < Z7_ARRAY_SIZE(_methodMasks))
    {
      _methodMasks[algo] |= 1u << (item.Get_Method());
      UInt64 d = 32 + item.Get_DictSize_Frac();
      d <<= (12 + item.Get_DictSize_Main());
      if (_dictMaxSizes[algo] < d)
          _dictMaxSizes[algo] = d;
    }
  }

  if (!need_FillLinks)
    return;
  
  CUIntVector sorted;
  for (i = 0; i < _refs.Size(); i++)
  {
    const CItem &item = _items[_refs[i].Item];
    if (!item.IsDir() && !item.IsService())
      sorted.Add(i);
  }
  
  if (sorted.IsEmpty())
    return;
  
  sorted.Sort(CompareItemsPaths_Sort, (void *)this);
  
  AString link;
  
  for (i = 0; i < _refs.Size(); i++)
  {
    CRefItem &ref = _refs[i];
    const CItem &item = _items[ref.Item];
    if (item.IsDir() || item.IsService() || item.PackSize != 0)
      continue;
    CLinkInfo linkInfo;
    if (!item.FindExtra_Link(linkInfo) || linkInfo.Type != NLinkType::kFileCopy)
      continue;
    link.SetFrom_CalcLen((const char *)(item.Extra + linkInfo.NameOffset), linkInfo.NameLen);
    const int linkIndex = FindLink(*this, sorted, link, i);
    if (linkIndex < 0)
      continue;
    if ((unsigned)linkIndex >= i)
      continue; // we don't support forward links that can lead to loops
    const CRefItem &linkRef = _refs[linkIndex];
    const CItem &linkItem = _items[linkRef.Item];
    if (linkItem.Size == item.Size)
    {
      if (linkRef.Link >= 0)
        ref.Link = linkRef.Link;
      else if (!linkItem.NeedUse_as_CopyLink())
        ref.Link = linkIndex;
    }
  }
}



HRESULT CHandler::Open2(IInStream *stream,
    const UInt64 *maxCheckStartPosition,
    IArchiveOpenCallback *openCallback)
{
  CMyComPtr<IArchiveOpenVolumeCallback> openVolumeCallback;
  // CMyComPtr<ICryptoGetTextPassword> getTextPassword;
  NRar::CVolumeName seqName;
  CTempBuf tempBuf;
  CUnpacker unpacker;
  
  if (openCallback)
  {
    openCallback->QueryInterface(IID_IArchiveOpenVolumeCallback, (void **)&openVolumeCallback);
    openCallback->QueryInterface(IID_ICryptoGetTextPassword, (void **)&unpacker.getTextPassword);
  }
  // unpacker.getTextPassword = getTextPassword;
  
  CInArchive arch;
  int prevSplitFile = -1;
  int prevMainFile = -1;
  UInt64 totalBytes = 0;
  UInt64 curBytes = 0;
  bool nextVol_is_Required = false;
  
  for (;;)
  {
    CMyComPtr<IInStream> inStream;
    
    if (_arcs.IsEmpty())
      inStream = stream;
    else
    {
      if (!openVolumeCallback)
        break;
      if (_arcs.Size() == 1)
      {
        UString baseName;
        {
          NCOM::CPropVariant prop;
          RINOK(openVolumeCallback->GetProperty(kpidName, &prop))
          if (prop.vt != VT_BSTR)
            break;
          baseName = prop.bstrVal;
        }
        if (!seqName.InitName(baseName))
          break;
      }
      const UString volName = seqName.GetNextName();
      const HRESULT result = openVolumeCallback->GetStream(volName, &inStream);
      if (result != S_OK && result != S_FALSE)
        return result;
      if (!inStream || result != S_OK)
      {
        if (nextVol_is_Required)
          _missingVolName = volName;
        break;
      }
    }
    
    UInt64 endPos = 0;
    RINOK(InStream_GetPos_GetSize(inStream, arch.StreamStartPosition, endPos))
    
    if (openCallback)
    {
      totalBytes += endPos;
      RINOK(openCallback->SetTotal(NULL, &totalBytes))
    }
    
    CInArcInfo arcInfo_Open;
    {
      const HRESULT res = arch.Open(inStream, maxCheckStartPosition, unpacker.getTextPassword, arcInfo_Open);
      if (arch.IsArc && arch.UnexpectedEnd)
        _errorFlags |= kpv_ErrorFlags_UnexpectedEnd;
      if (_arcs.IsEmpty())
        _isArc = arch.IsArc;
      if (res != S_OK)
      {
        if (res != S_FALSE)
          return res;
        if (_arcs.IsEmpty())
          return res;
        break;
      }
    }
    
    CArc &arc = _arcs.AddNew();
    CInArcInfo &arcInfo = arc.Info;
    arcInfo = arcInfo_Open;
    arc.Stream = inStream;
    
    CItem item;
    
    for (;;)
    {
      item.Clear();
      
      arcInfo.EndPos = arch.Position;
      if (arch.Position > endPos)
      {
        _errorFlags |= kpv_ErrorFlags_UnexpectedEnd;
        break;
      }
      RINOK(InStream_SeekSet(inStream, arch.Position))
      
      {
        CInArchive::CHeader h;
        const HRESULT res = arch.ReadBlockHeader(h);
        if (res != S_OK)
        {
          if (res != S_FALSE)
            return res;
          if (arch.UnexpectedEnd)
          {
            _errorFlags |= kpv_ErrorFlags_UnexpectedEnd;
            if (arcInfo.EndPos < arch.Position)
                arcInfo.EndPos = arch.Position;
            if (arcInfo.EndPos < endPos)
                arcInfo.EndPos = endPos;
          }
          else
            _errorFlags |= kpv_ErrorFlags_HeadersError;
          break;
        }
        
        if (h.Type == NHeaderType::kEndOfArc)
        {
          arcInfo.EndPos = arch.Position;
          arcInfo.EndOfArchive_was_Read = true;
          if (!arch.ReadVar(arcInfo.EndFlags))
            _errorFlags |= kpv_ErrorFlags_HeadersError;
          if (!arch.Is_Buf_Finished() || h.ExtraSize || h.DataSize)
            arcInfo.UnsupportedFeature = true;
          if (arcInfo.IsVolume())
          {
            // for multivolume archives RAR can add ZERO bytes at the end for alignment.
            // We must skip these bytes to prevent phySize warning.
            RINOK(InStream_SeekSet(inStream, arcInfo.EndPos))
            bool areThereNonZeros;
            UInt64 numZeros;
            const UInt64 maxSize = 1 << 12;
            RINOK(ReadZeroTail(inStream, areThereNonZeros, numZeros, maxSize))
            if (!areThereNonZeros && numZeros != 0 && numZeros <= maxSize)
              arcInfo.EndPos += numZeros;
          }
          break;
        }
        
        if (h.Type != NHeaderType::kFile &&
            h.Type != NHeaderType::kService)
        {
          _errorFlags |= kpv_ErrorFlags_UnsupportedFeature;
          break;
        }
        
        item.RecordType = (Byte)h.Type;
        if (!arch.ReadFileHeader(h, item))
        {
          _errorFlags |= kpv_ErrorFlags_HeadersError;
          break;
        }
        item.DataPos = arch.Position;
      }
      
      bool isOk_packSize = true;
      {
        arcInfo.EndPos = arch.Position;
        if (arch.Position + item.PackSize < arch.Position)
        {
          isOk_packSize = false;
          _errorFlags |= kpv_ErrorFlags_HeadersError;
          if (arcInfo.EndPos < endPos)
              arcInfo.EndPos = endPos;
        }
        else
        {
          arch.AddToSeekValue(item.PackSize); // Position points to next header;
          arcInfo.EndPos = arch.Position;
        }
      }

      bool needAdd = true;

      if (!_comment_WasUsedInArc
          && _comment.Size() == 0
          && item.Is_CMT())
      {
        _comment_WasUsedInArc = true;
        if (   item.PackSize <= kCommentSize_Max
            && item.PackSize == item.Size
            && item.PackSize != 0
            && item.Get_Method() == 0
            && !item.IsSplit())
        {
          RINOK(unpacker.DecodeToBuf(EXTERNAL_CODECS_VARS item, item.PackSize, inStream, _comment))
          needAdd = false;
          // item.RecordType = (Byte)NHeaderType::kFile; // for debug
        }
      }

      CRefItem ref;
      ref.Item = _items.Size();
      ref.Last = ref.Item;
      ref.Parent = -1;
      ref.Link = -1;
      
      if (needAdd)
      {
        if (item.IsService())
        {
          if (item.Is_STM())
          {
            if (prevMainFile >= 0)
              ref.Parent = prevMainFile;
          }
          else
          {
            needAdd = false;
            if (item.Is_ACL())
            {
              _acl_Used = true;
              if (item.IsEncrypted() && !arch.m_CryptoMode)
                _error_in_ACL = true;
              else if (item.IsSolid()
                  || prevMainFile < 0
                  || item.Size >= (1 << 24)
                  || item.Size == 0)
                _error_in_ACL = true;
              if (prevMainFile >= 0 && item.Size < (1 << 24) && item.Size != 0)
              {
                CItem &mainItem = _items[_refs[prevMainFile].Item];
                
                if (mainItem.ACL < 0)
                {
                  CByteBuffer acl;
                  const HRESULT res = tempBuf.Decode(EXTERNAL_CODECS_VARS item, inStream, unpacker, acl);
                  if (!item.IsSplitAfter())
                    tempBuf.Clear();
                  if (res != S_OK)
                  {
                    tempBuf.Clear();
                    if (res != S_FALSE && res != E_NOTIMPL)
                      return res;
                    _error_in_ACL = true;
                  }
                  else if (acl.Size() != 0)
                  {
                    if (_acls.IsEmpty() || acl != _acls.Back())
                      _acls.Add(acl);
                    mainItem.ACL = (int)_acls.Size() - 1;
                  }
                }
              }
            }
          }
        } // item.IsService()
        
        if (needAdd)
        {
          if (item.IsSplitBefore())
          {
            if (prevSplitFile >= 0)
            {
              CRefItem &ref2 = _refs[prevSplitFile];
              CItem &prevItem = _items[ref2.Last];
              if (item.IsNextForItem(prevItem))
              {
                ref2.Last = _items.Size();
                prevItem.NextItem = (int)ref2.Last;
                needAdd = false;
              }
            }
            else
              _split_Error = true;
          }
        }
        
        if (needAdd)
        {
          if (item.IsSplitAfter())
            prevSplitFile = (int)_refs.Size();
          if (!item.IsService())
            prevMainFile = (int)_refs.Size();
        }
      }
      
      {
        UInt64 version;
        if (item.FindExtra_Version(version))
        {
          item.Version_Defined = true;
          item.Version = version;
        }
      }
      
      item.VolIndex = _arcs.Size() - 1;
      _items.Add(item);
      if (needAdd)
        _refs.Add(ref);
      
      if (openCallback && (_items.Size() & 0xFF) == 0)
      {
        const UInt64 numFiles = _refs.Size(); // _items.Size()
        const UInt64 numBytes = curBytes + item.DataPos;
        RINOK(openCallback->SetCompleted(&numFiles, &numBytes))
      }

      if (!isOk_packSize)
        break;
    }
      
    curBytes += endPos;

    nextVol_is_Required = false;

    if (!arcInfo.IsVolume())
      break;

    if (arcInfo.EndOfArchive_was_Read)
    {
      if (!arcInfo.AreMoreVolumes())
        break;
      nextVol_is_Required = true;
    }
  }

  FillLinks();
  return S_OK;
}



Z7_COM7F_IMF(CHandler::Open(IInStream *stream,
    const UInt64 *maxCheckStartPosition,
    IArchiveOpenCallback *openCallback))
{
  COM_TRY_BEGIN
  Close();
  return Open2(stream, maxCheckStartPosition, openCallback);
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  COM_TRY_BEGIN
  _missingVolName.Empty();
  _errorFlags = 0;
  // _warningFlags = 0;
  _isArc = false;
  _comment_WasUsedInArc = false;
  _acl_Used = false;
  _error_in_ACL = false;
  _split_Error = false;
  _numBlocks = 0;
  _rar5comapt_mask = 0;
  _algo_Mask = 0; // (UInt64)0u - 1;
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(_methodMasks); i++)
  {
    _methodMasks[i] = 0;
    _dictMaxSizes[i] = 0;
  }

  _refs.Clear();
  _items.Clear();
  _arcs.Clear();
  _acls.Clear();
  _comment.Free();
  return S_OK;
  COM_TRY_END
}


Z7_CLASS_IMP_NOQIB_1(
  CVolsInStream
  , ISequentialInStream
)
  UInt64 _rem;
  ISequentialInStream *_stream;
  const CObjectVector<CArc> *_arcs;
  const CObjectVector<CItem> *_items;
  int _itemIndex;
public:
  bool CrcIsOK;
private:
  CHash _hash;
public:
  void Init(const CObjectVector<CArc> *arcs,
      const CObjectVector<CItem> *items,
      unsigned itemIndex)
  {
    _arcs = arcs;
    _items = items;
    _itemIndex = (int)itemIndex;
    _stream = NULL;
    CrcIsOK = true;
  }
};

Z7_COM7F_IMF(CVolsInStream::Read(void *data, UInt32 size, UInt32 *processedSize))
{
  if (processedSize)
    *processedSize = 0;
  UInt32 realProcessedSize = 0;

  while (size != 0)
  {
    if (!_stream)
    {
      if (_itemIndex < 0)
        break;
      const CItem &item = (*_items)[_itemIndex];
      IInStream *s = (*_arcs)[item.VolIndex].Stream;
      RINOK(InStream_SeekSet(s, item.GetDataPosition()))
      _stream = s;
      if (CrcIsOK && item.IsSplitAfter())
        _hash.Init(item);
      else
        _hash.Init_NoCalc();
      _rem = item.PackSize;
    }
    {
      UInt32 cur = size;
      if (cur > _rem)
        cur = (UInt32)_rem;
      const UInt32 num = cur;
      HRESULT res = _stream->Read(data, cur, &cur);
      _hash.Update(data, cur);
      realProcessedSize += cur;
      if (processedSize)
        *processedSize = realProcessedSize;
      data = (Byte *)data + cur;
      size -= cur;
      _rem -= cur;
      if (_rem == 0)
      {
        const CItem &item = (*_items)[_itemIndex];
        _itemIndex = item.NextItem;
        if (!_hash.Check(item, NULL)) // RAR doesn't use MAC here
          CrcIsOK = false;
        _stream = NULL;
      }
      if (res != S_OK)
        return res;
      if (realProcessedSize != 0)
        return S_OK;
      if (cur == 0 && num != 0)
        return S_OK;
    }
  }
  
  return S_OK;
}


static int FindLinkBuf(CObjectVector<CLinkFile> &linkFiles, unsigned index)
{
  unsigned left = 0, right = linkFiles.Size();
  for (;;)
  {
    if (left == right)
      return -1;
    const unsigned mid = (left + right) / 2;
    const unsigned linkIndex = linkFiles[mid].Index;
    if (index == linkIndex)
      return (int)mid;
    if (index < linkIndex)
      right = mid;
    else
      left = mid + 1;
  }
}


static inline int DecoderRes_to_OpRes(HRESULT res, bool crcOK)
{
  if (res == E_NOTIMPL)
    return NExtract::NOperationResult::kUnsupportedMethod;
  // if (res == S_FALSE)
  if (res != S_OK)
    return NExtract::NOperationResult::kDataError;
  return crcOK ?
    NExtract::NOperationResult::kOK :
    NExtract::NOperationResult::kCRCError;
}


static HRESULT CopyData_with_Progress(const Byte *data, size_t size,
    ISequentialOutStream *outStream, ICompressProgressInfo *progress)
{
  UInt64 pos64 = 0;
  while (size)
  {
    const UInt32 kStepSize = (UInt32)1 << 24;
    UInt32 cur = kStepSize;
    if (cur > size)
      cur = (UInt32)size;
    RINOK(outStream->Write(data, cur, &cur))
    if (cur == 0)
      return E_FAIL;
    size -= cur;
    data += cur;
    pos64 += cur;
    if (progress)
    {
      RINOK(progress->SetRatioInfo(&pos64, &pos64))
    }
  }
  return S_OK;
}


Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = (UInt32)_refs.Size();
  if (numItems == 0)
    return S_OK;
  
  CByteArr extractStatuses(_refs.Size());
  memset(extractStatuses, 0, _refs.Size());

  // we don't want to use temp buffer for big link files.
  const size_t k_CopyLinkFile_MaxSize = (size_t)1 << (28 + sizeof(size_t) / 2);

  const Byte kStatus_Extract = 1 << 0;
  const Byte kStatus_Skip = 1 << 1;
  const Byte kStatus_Link = 1 << 2;

  /*
    In original RAR:
    1) service streams are not allowed to be solid,
        and solid flag must be ignored for service streams.
    2) If RAR creates new solid block and first file in solid block is Link file,
         then it can clear solid flag for Link file and
         clear solid flag for first non-Link file after Link file.
  */

  CObjectVector<CLinkFile> linkFiles;

  {
    UInt64 total = 0;
    bool isThereUndefinedSize = false;
    bool thereAreLinks = false;
    {
      unsigned solidLimit = 0;
      for (UInt32 t = 0; t < numItems; t++)
      {
        const unsigned index = (unsigned)(allFilesMode ? t : indices[t]);
        const CRefItem &ref = _refs[index];
        const CItem &item = _items[ref.Item];
        const CItem &lastItem = _items[ref.Last];
        
        extractStatuses[index] |= kStatus_Extract;

        if (!lastItem.Is_UnknownSize())
          total += lastItem.Size;
        else
          isThereUndefinedSize = true;
        
        if (ref.Link >= 0)
        {
          // 18.06 fixed: we use links for Test mode too
          // if (!testMode)
          {
            if ((unsigned)ref.Link < index)
            {
              const CRefItem &linkRef = _refs[(unsigned)ref.Link];
              const CItem &linkItem = _items[linkRef.Item];
              if (linkItem.IsSolid())
              if (testMode || linkItem.Size <= k_CopyLinkFile_MaxSize)
              {
                if (extractStatuses[(unsigned)ref.Link] == 0)
                {
                  const CItem &lastLinkItem = _items[linkRef.Last];
                  if (!lastLinkItem.Is_UnknownSize())
                    total += lastLinkItem.Size;
                  else
                    isThereUndefinedSize = true;
                }
                extractStatuses[(unsigned)ref.Link] |= kStatus_Link;
                thereAreLinks = true;
              }
            }
          }
          continue;
        }
        
        if (item.IsService())
          continue;
        
        if (item.IsSolid())
        {
          unsigned j = index;
          
          while (j > solidLimit)
          {
            j--;
            const CRefItem &ref2 = _refs[j];
            const CItem &item2 = _items[ref2.Item];
            if (!item2.IsService())
            {
              if (extractStatuses[j] == 0)
              {
                const CItem &lastItem2 = _items[ref2.Last];
                if (!lastItem2.Is_UnknownSize())
                  total += lastItem2.Size;
                else
                  isThereUndefinedSize = true;
              }
              extractStatuses[j] |= kStatus_Skip;
              if (!item2.IsSolid())
                break;
            }
          }
        }
        
        solidLimit = index + 1;
      }
    }

    if (thereAreLinks)
    {
      unsigned solidLimit = 0;

      FOR_VECTOR (i, _refs)
      {
        if ((extractStatuses[i] & kStatus_Link) == 0)
          continue;

        // We use CLinkFile for testMode too.
        // So we can show errors for copy files.
        // if (!testMode)
        {
          CLinkFile &linkFile = linkFiles.AddNew();
          linkFile.Index = i;
        }

        const CItem &item = _items[_refs[i].Item];
        /*
        if (item.IsService())
          continue;
        */
        
        if (item.IsSolid())
        {
          unsigned j = i;
          
          while (j > solidLimit)
          {
            j--;
            const CRefItem &ref2 = _refs[j];
            const CItem &item2 = _items[ref2.Item];
            if (!item2.IsService())
            {
              if (extractStatuses[j] != 0)
                break;
              extractStatuses[j] = kStatus_Skip;
              {
                const CItem &lastItem2 = _items[ref2.Last];
                if (!lastItem2.Is_UnknownSize())
                  total += lastItem2.Size;
                else
                  isThereUndefinedSize = true;
              }
              if (!item2.IsSolid())
                break;
            }
          }
        }
        
        solidLimit = i + 1;
      }

      if (!testMode)
      for (UInt32 t = 0; t < numItems; t++)
      {
        const unsigned index = (unsigned)(allFilesMode ? t : indices[t]);
        const CRefItem &ref = _refs[index];
       
        const int linkIndex = ref.Link;
        if (linkIndex < 0 || (unsigned)linkIndex >= index)
          continue;
        const CItem &linkItem = _items[_refs[(unsigned)linkIndex].Item];
        if (!linkItem.IsSolid() || linkItem.Size > k_CopyLinkFile_MaxSize)
          continue;
        const int bufIndex = FindLinkBuf(linkFiles, (unsigned)linkIndex);
        if (bufIndex < 0)
          return E_FAIL;
        linkFiles[bufIndex].NumLinks++;
      }
    }
    
    if (total != 0 || !isThereUndefinedSize)
    {
      RINOK(extractCallback->SetTotal(total))
    }
  }


  
  // ---------- MEMORY REQUEST ----------
  {
    UInt64 dictMaxSize = 0;
    for (UInt32 i = 0; i < _refs.Size(); i++)
    {
      if (extractStatuses[i] == 0)
        continue;
      const CRefItem &ref = _refs[i];
      const CItem &item = _items[ref.Item];
/*
      if (!item.IsDir() && !item.IsService() && item.NeedUse_as_CopyLink())
      {
      }
*/
      const unsigned algo = item.Get_AlgoVersion_RawBits();
      if (!item.IsDir() && algo < Z7_ARRAY_SIZE(_methodMasks))
      {
        const UInt64 d = item.Get_DictSize64();
        if (dictMaxSize < d)
            dictMaxSize = d;
      }
    }
    // we use callback, if dict exceeds (1 GB), because
    // client code can set low limit (1 GB) for allowed memory usage.
    const UInt64 k_MemLimit_for_Callback = (UInt64)1 << 30;
    if (dictMaxSize > (_memUsage_WasSet ?
        _memUsage_Decompress : k_MemLimit_for_Callback))
    {
      {
        CMyComPtr<IArchiveRequestMemoryUseCallback> requestMem;
        extractCallback->QueryInterface(IID_IArchiveRequestMemoryUseCallback, (void **)&requestMem);
        if (!requestMem)
        {
          if (_memUsage_WasSet)
            return E_OUTOFMEMORY;
        }
        else
        {
          UInt64 allowedSize = _memUsage_WasSet ?
              _memUsage_Decompress :
              (UInt64)1 << 32; // 4 GB is default allowed limit for RAR7
          
          const UInt32 flags = (_memUsage_WasSet ?
                NRequestMemoryUseFlags::k_AllowedSize_WasForced |
                NRequestMemoryUseFlags::k_MLimit_Exceeded :
            (dictMaxSize > allowedSize) ?
                NRequestMemoryUseFlags::k_DefaultLimit_Exceeded:
                0)
             |  NRequestMemoryUseFlags::k_SkipArc_IsExpected
             // |  NRequestMemoryUseFlags::k_NoErrorMessage // for debug
             ;

          // we set "Allow" for default case, if requestMem doesn't process anything.
          UInt32 answerFlags =
              (_memUsage_WasSet && dictMaxSize > allowedSize) ?
                NRequestMemoryAnswerFlags::k_Limit_Exceeded
              | NRequestMemoryAnswerFlags::k_SkipArc
              : NRequestMemoryAnswerFlags::k_Allow;

          RINOK(requestMem->RequestMemoryUse(
              flags,
              NEventIndexType::kNoIndex,
              // NEventIndexType::kInArcIndex, // for debug
              0,    // index
              NULL, // path
              dictMaxSize, &allowedSize, &answerFlags))
          if ( (answerFlags & NRequestMemoryAnswerFlags::k_Allow) == 0
            || (answerFlags & NRequestMemoryAnswerFlags::k_Stop)
            || (answerFlags & NRequestMemoryAnswerFlags::k_SkipArc)
            )
          {
            return E_OUTOFMEMORY;
          }
/*
          if ((answerFlags & NRequestMemoryAnswerFlags::k_AskForBigFile) == 0 &&
              (answerFlags & NRequestMemoryAnswerFlags::k_ReportForBigFile) == 0)
          {
            // requestMem.Release();
          }
*/
        }
      }
    }
  }



  // ---------- UNPACK ----------

  UInt64 totalUnpacked = 0;
  UInt64 totalPacked = 0;
  UInt64 curUnpackSize;
  UInt64 curPackSize;

  CUnpacker unpacker;
  unpacker.NeedCrc = _needChecksumCheck;
  CMyComPtr2_Create<ISequentialInStream, CVolsInStream> volsInStream;
  CMyComPtr2_Create<ICompressProgressInfo, CLocalProgress> lps;
  lps->Init(extractCallback, false);

/*
  bool prevSolidWasSkipped = false;
  UInt64 solidDictSize_Skip = 0;
*/

  for (unsigned i = 0;; i++,
      totalUnpacked += curUnpackSize,
      totalPacked += curPackSize)
  {
    lps->InSize = totalPacked;
    lps->OutSize = totalUnpacked;
    RINOK(lps->SetCur())
    {
      const unsigned num = _refs.Size();
      if (i >= num)
        break;
      for (;;)
      {
        if (extractStatuses[i] != 0)
          break;
        i++;
        if (i >= num)
          break;
      }
      if (i >= num)
        break;
    }
    curUnpackSize = 0;
    curPackSize = 0;
    
    // isExtract means that we don't skip that item. So we need read data.
    const bool isExtract = ((extractStatuses[i] & kStatus_Extract) != 0);
    Int32 askMode =
        isExtract ? (testMode ?
          NExtract::NAskMode::kTest :
          NExtract::NAskMode::kExtract) :
          NExtract::NAskMode::kSkip;

    unpacker.linkFile = NULL;

    // if (!testMode)
    if ((extractStatuses[i] & kStatus_Link) != 0)
    {
      const int bufIndex = FindLinkBuf(linkFiles, i);
      if (bufIndex < 0)
        return E_FAIL;
      unpacker.linkFile = &linkFiles[bufIndex];
    }

    const unsigned index = i;
    const CRefItem *ref = &_refs[index];
    const CItem *item = &_items[ref->Item];
    const CItem &lastItem = _items[ref->Last];

    curUnpackSize = 0;
    if (!lastItem.Is_UnknownSize())
      curUnpackSize = lastItem.Size;

    curPackSize = GetPackSize(index);

    bool isSolid = false;
    if (!item->IsService())
    {
      if (item->IsSolid())
        isSolid = unpacker.SolidAllowed;
      unpacker.SolidAllowed = isSolid;
    }


    // ----- request mem -----
/*
    // link files are complicated cases. (ref->Link >= 0)
    // link file can refer to non-solid file that can have big dictionary
    // link file can refer to solid files that requres buffer
    if (!item->IsDir() && requestMem && ref->Link < 0)
    {
      bool needSkip = false;
      if (isSolid)
        needSkip = prevSolidWasSkipped;
      else
      {
        // isSolid == false
        const unsigned algo = item->Get_AlgoVersion_RawBits();
        // const unsigned m = item.Get_Method();
        if (algo < Z7_ARRAY_SIZE(_methodMasks))
        {
          solidDictSize_Skip = item->Get_DictSize64();
          if (solidDictSize_Skip > allowedSize)
            needSkip = true;
        }
      }
      if (needSkip)
      {
        UInt32 answerFlags = 0;
        UInt64 allowedSize_File = allowedSize;
        RINOK(requestMem->RequestMemoryUse(
                  NRequestMemoryUseFlags::k_Limit_Exceeded |
                  NRequestMemoryUseFlags::k_IsReport,
              NEventIndexType::kInArcIndex,
              index,
              NULL, // path
              solidDictSize_Skip, &allowedSize_File, &answerFlags))
        if (!item->IsService())
          prevSolidWasSkipped = true;
        continue;
      }
    }
    if (!item->IsService() && item->IsDir())
      prevSolidWasSkipped = false;
*/
    
    CMyComPtr<ISequentialOutStream> realOutStream;
    RINOK(extractCallback->GetStream((UInt32)index, &realOutStream, askMode))

    if (item->IsDir())
    {
      RINOK(extractCallback->PrepareOperation(askMode))
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
      continue;
    }

    const int index2 = ref->Link;

    int bufIndex = -1;

    if (index2 >= 0)
    {
      const CRefItem &ref2 = _refs[index2];
      const CItem &item2 = _items[ref2.Item];
      const CItem &lastItem2 = _items[ref2.Last];
      if (!item2.IsSolid())
      {
        item = &item2;
        ref = &ref2;
        if (!lastItem2.Is_UnknownSize())
          curUnpackSize = lastItem2.Size;
        else
          curUnpackSize = 0;
        curPackSize = GetPackSize((unsigned)index2);
      }
      else
      {
        if ((unsigned)index2 < index)
          bufIndex = FindLinkBuf(linkFiles, (unsigned)index2);
      }
    }

    bool needCallback = true;

    if (!realOutStream)
    {
      if (testMode)
      {
        if (item->NeedUse_as_CopyLink_or_HardLink())
        {
          Int32 opRes = NExtract::NOperationResult::kOK;
          if (bufIndex >= 0)
          {
            const CLinkFile &linkFile = linkFiles[bufIndex];
            opRes = DecoderRes_to_OpRes(linkFile.Res, linkFile.crcOK);
          }

          RINOK(extractCallback->PrepareOperation(askMode))
          RINOK(extractCallback->SetOperationResult(opRes))
          continue;
        }
      }
      else
      {
        if (item->IsService())
          continue;

        needCallback = false;

        if (!item->NeedUse_as_HardLink())
        if (index2 < 0)

        for (unsigned n = i + 1; n < _refs.Size(); n++)
        {
          const CItem &nextItem = _items[_refs[n].Item];
          if (nextItem.IsService())
            continue;
          if (!nextItem.IsSolid())
            break;
          if (extractStatuses[i] != 0)
          {
            needCallback = true;
            break;
          }
        }
        
        askMode = NExtract::NAskMode::kSkip;
      }
    }

    if (needCallback)
    {
      RINOK(extractCallback->PrepareOperation(askMode))
    }

    if (bufIndex >= 0)
    {
      CLinkFile &linkFile = linkFiles[bufIndex];
     
      if (isExtract)
      {
        if (linkFile.NumLinks == 0)
          return E_FAIL;
       
        if (needCallback)
        if (realOutStream)
        {
          RINOK(CopyData_with_Progress(linkFile.Data, linkFile.Data.Size(), realOutStream, lps))
        }
      
        if (--linkFile.NumLinks == 0)
          linkFile.Data.Free();
      }
      
      if (needCallback)
      {
        RINOK(extractCallback->SetOperationResult(DecoderRes_to_OpRes(linkFile.Res, linkFile.crcOK)))
      }
      continue;
    }

    if (!needCallback)
      continue;
    
    if (item->NeedUse_as_CopyLink())
    {
      const int opRes = realOutStream ?
          NExtract::NOperationResult::kUnsupportedMethod:
          NExtract::NOperationResult::kOK;
      realOutStream.Release();
      RINOK(extractCallback->SetOperationResult(opRes))
      continue;
    }

    volsInStream->Init(&_arcs, &_items, ref->Item);

    const UInt64 packSize = curPackSize;

    if (item->IsEncrypted())
      if (!unpacker.getTextPassword)
        extractCallback->QueryInterface(IID_ICryptoGetTextPassword, (void **)&unpacker.getTextPassword);

    bool wrongPassword;
    HRESULT result = unpacker.Create(EXTERNAL_CODECS_VARS *item, isSolid, wrongPassword);

    if (wrongPassword)
    {
      realOutStream.Release();
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kWrongPassword))
      continue;
    }
        
    bool crcOK = true;
    if (result == S_OK)
      result = unpacker.Code(*item, _items[ref->Last], packSize, volsInStream, realOutStream, lps, crcOK);
    realOutStream.Release();
    if (!volsInStream->CrcIsOK)
      crcOK = false;

    int opRes = crcOK ?
        NExtract::NOperationResult::kOK:
        NExtract::NOperationResult::kCRCError;

    if (result != S_OK)
    {
      if (result == S_FALSE)
        opRes = NExtract::NOperationResult::kDataError;
      else if (result == E_NOTIMPL)
        opRes = NExtract::NOperationResult::kUnsupportedMethod;
      else
        return result;
    }

    RINOK(extractCallback->SetOperationResult(opRes))
  }

  {
    FOR_VECTOR (k, linkFiles)
      if (linkFiles[k].NumLinks != 0)
        return E_FAIL;
  }

  return S_OK;
  COM_TRY_END
}


CHandler::CHandler()
{
  InitDefaults();
}

void CHandler::InitDefaults()
{
  _needChecksumCheck = true;
  _memUsage_WasSet = false;
  _memUsage_Decompress = (UInt64)1 << 32;
}

Z7_COM7F_IMF(CHandler::SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps))
{
  InitDefaults();

  for (UInt32 i = 0; i < numProps; i++)
  {
    UString name = names[i];
    name.MakeLower_Ascii();
    if (name.IsEmpty())
      return E_INVALIDARG;

    const PROPVARIANT &prop = values[i];

    if (name.IsPrefixedBy_Ascii_NoCase("mt"))
    {
    }
    else if (name.IsPrefixedBy_Ascii_NoCase("memx"))
    {
      size_t memAvail;
      if (!NWindows::NSystem::GetRamSize(memAvail))
        memAvail = (size_t)sizeof(size_t) << 28;
      UInt64 v;
      if (!ParseSizeString(name.Ptr(4), prop, memAvail, v))
        return E_INVALIDARG;
      _memUsage_Decompress = v;
      _memUsage_WasSet = true;
    }
    else if (name.IsPrefixedBy_Ascii_NoCase("crc"))
    {
      name.Delete(0, 3);
      UInt32 crcSize = 1;
      RINOK(ParsePropToUInt32(name, prop, crcSize))
      _needChecksumCheck = (crcSize != 0);
    }
    else
    {
      return E_INVALIDARG;
    }
  }
  return S_OK;
}


IMPL_ISetCompressCodecsInfo

REGISTER_ARC_I(
  "Rar5", "rar r00", NULL, 0xCC,
  kMarker,
  0,
  NArcInfoFlags::kFindSignature,
  NULL)

}}


Z7_CLASS_IMP_COM_2(
  CBlake2spHasher
  , IHasher
  , ICompressSetCoderProperties
)
  CAlignedBuffer1 _buf;
  // CBlake2sp _blake;
  #define Z7_BLACK2S_ALIGN_OBJECT_OFFSET 0
  CBlake2sp *Obj() { return (CBlake2sp *)(void *)((Byte *)_buf + Z7_BLACK2S_ALIGN_OBJECT_OFFSET); }
public:
  Byte _mtDummy[1 << 7];  // it's public to eliminate clang warning: unused private field
  CBlake2spHasher():
    _buf(sizeof(CBlake2sp) + Z7_BLACK2S_ALIGN_OBJECT_OFFSET)
  {
    Blake2sp_SetFunction(Obj(), 0);
    Blake2sp_InitState(Obj());
  }
};

Z7_COM7F_IMF2(void, CBlake2spHasher::Init())
{
  Blake2sp_InitState(Obj());
}

Z7_COM7F_IMF2(void, CBlake2spHasher::Update(const void *data, UInt32 size))
{
#if 1
  Blake2sp_Update(Obj(), (const Byte *)data, (size_t)size);
#else
  // for debug:
  for (;;)
  {
    if (size == 0)
      return;
    UInt32 size2 = (size * 0x85EBCA87) % size / 800;
    // UInt32 size2 = size / 2;
    if (size2 == 0)
      size2 = 1;
    Blake2sp_Update(Obj(), (const Byte *)data, size2);
    data = (const void *)((const Byte *)data + size2);
    size -= size2;
  }
#endif
}

Z7_COM7F_IMF2(void, CBlake2spHasher::Final(Byte *digest))
{
  Blake2sp_Final(Obj(), digest);
}

Z7_COM7F_IMF(CBlake2spHasher::SetCoderProperties(const PROPID *propIDs, const PROPVARIANT *coderProps, UInt32 numProps))
{
  unsigned algo = 0;
  for (UInt32 i = 0; i < numProps; i++)
  {
    if (propIDs[i] == NCoderPropID::kDefaultProp)
    {
      const PROPVARIANT &prop = coderProps[i];
      if (prop.vt != VT_UI4)
        return E_INVALIDARG;
      /*
      if (prop.ulVal > Z7_BLAKE2S_ALGO_MAX)
        return E_NOTIMPL;
      */
      algo = (unsigned)prop.ulVal;
    }
  }
  if (!Blake2sp_SetFunction(Obj(), algo))
    return E_NOTIMPL;
  return S_OK;
}

REGISTER_HASHER(CBlake2spHasher, 0x202, "BLAKE2sp", Z7_BLAKE2S_DIGEST_SIZE)

static struct CBlake2sp_Prepare { CBlake2sp_Prepare() { z7_Black2sp_Prepare(); } } g_Blake2sp_Prepare;

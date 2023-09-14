/* PpmdHandler.cpp -- PPMd format handler
2020 : Igor Pavlov : Public domain
This code is based on:
  PPMd var.H (2001) / var.I (2002): Dmitry Shkarin : Public domain
  Carryless rangecoder (1999): Dmitry Subbotin : Public domain */

#include "StdAfx.h"

#include "../../../C/CpuArch.h"
#include "../../../C/Alloc.h"
#include "../../../C/Ppmd7.h"
#include "../../../C/Ppmd8.h"

#include "../../Common/ComTry.h"
#include "../../Common/StringConvert.h"

#include "../../Windows/PropVariant.h"
#include "../../Windows/TimeUtils.h"

#include "../Common/CWrappers.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

using namespace NWindows;

namespace NArchive {
namespace NPpmd {

static const UInt32 kBufSize = (1 << 20);

struct CBuf
{
  Byte *Buf;
  
  CBuf(): Buf(NULL) {}
  ~CBuf() { ::MidFree(Buf); }
  bool Alloc()
  {
    if (!Buf)
      Buf = (Byte *)::MidAlloc(kBufSize);
    return (Buf != NULL);
  }
};

static const UInt32 kHeaderSize = 16;
static const UInt32 kSignature = 0x84ACAF8F;
static const unsigned kNewHeaderVer = 8;

struct CItem
{
  UInt32 Attrib;
  UInt32 Time;
  AString Name;
  
  unsigned Order;
  unsigned MemInMB;
  unsigned Ver;
  unsigned Restor;

  HRESULT ReadHeader(ISequentialInStream *s, UInt32 &headerSize);
  bool IsSupported() const
  {
    return (Ver == 7 && Order >= PPMD7_MIN_ORDER)
        || (Ver == 8 && Order >= PPMD8_MIN_ORDER && Restor < PPMD8_RESTORE_METHOD_UNSUPPPORTED);
  }
};

HRESULT CItem::ReadHeader(ISequentialInStream *s, UInt32 &headerSize)
{
  Byte h[kHeaderSize];
  RINOK(ReadStream_FALSE(s, h, kHeaderSize))
  if (GetUi32(h) != kSignature)
    return S_FALSE;
  Attrib = GetUi32(h + 4);
  Time = GetUi32(h + 12);
  unsigned info = GetUi16(h + 8);
  Order = (info & 0xF) + 1;
  MemInMB = ((info >> 4) & 0xFF) + 1;
  Ver = info >> 12;

  if (Ver < 6 || Ver > 11) return S_FALSE;
 
  UInt32 nameLen = GetUi16(h + 10);
  Restor = nameLen >> 14;
  if (Restor > 2)
    return S_FALSE;
  if (Ver >= kNewHeaderVer)
    nameLen &= 0x3FFF;
  if (nameLen > (1 << 9))
    return S_FALSE;
  char *name = Name.GetBuf(nameLen);
  HRESULT res = ReadStream_FALSE(s, name, nameLen);
  Name.ReleaseBuf_CalcLen(nameLen);
  headerSize = kHeaderSize + nameLen;
  return res;
}


Z7_CLASS_IMP_CHandler_IInArchive_1(
  IArchiveOpenSeq
)
  CItem _item;
  UInt32 _headerSize;
  bool _packSize_Defined;
  UInt64 _packSize;
  CMyComPtr<ISequentialInStream> _stream;

  void GetVersion(NCOM::CPropVariant &prop);
};

static const Byte kProps[] =
{
  kpidPath,
  kpidMTime,
  kpidAttrib,
  kpidMethod
};

static const Byte kArcProps[] =
{
  kpidMethod
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

void CHandler::GetVersion(NCOM::CPropVariant &prop)
{
  AString s ("PPMd");
  s += (char)('A' + _item.Ver);
  s += ":o";
  s.Add_UInt32(_item.Order);
  s += ":mem";
  s.Add_UInt32(_item.MemInMB);
  s += 'm';
  if (_item.Ver >= kNewHeaderVer && _item.Restor != 0)
  {
    s += ":r";
    s.Add_UInt32(_item.Restor);
  }
  prop = s;
}

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPhySize: if (_packSize_Defined) prop = _packSize; break;
    case kpidMethod: GetVersion(prop); break;
  }
  prop.Detach(value);
  return S_OK;
}


Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = 1;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 /* index */, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPath: prop = MultiByteToUnicodeString(_item.Name, CP_ACP); break;
    case kpidMTime:
    {
      // time can be in Unix format ???
      FILETIME utc;
      if (NTime::DosTime_To_FileTime(_item.Time, utc))
        prop = utc;
      break;
    }
    case kpidAttrib: prop = _item.Attrib; break;
    case kpidPackSize: if (_packSize_Defined) prop = _packSize; break;
    case kpidMethod: GetVersion(prop); break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *))
{
  return OpenSeq(stream);
}

Z7_COM7F_IMF(CHandler::OpenSeq(ISequentialInStream *stream))
{
  COM_TRY_BEGIN
  HRESULT res;
  try
  {
    Close();
    res = _item.ReadHeader(stream, _headerSize);
  }
  catch(...) { res = S_FALSE; }
  if (res == S_OK)
    _stream = stream;
  else
    Close();
  return res;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _packSize = 0;
  _packSize_Defined = false;
  _stream.Release();
  return S_OK;
}



struct CPpmdCpp
{
  unsigned Ver;
  CPpmd7 _ppmd7;
  CPpmd8 _ppmd8;
  
  CPpmdCpp(unsigned version)
  {
    Ver = version;
    Ppmd7_Construct(&_ppmd7);
    Ppmd8_Construct(&_ppmd8);
  }

  ~CPpmdCpp()
  {
    Ppmd7_Free(&_ppmd7, &g_BigAlloc);
    Ppmd8_Free(&_ppmd8, &g_BigAlloc);
  }

  bool Alloc(UInt32 memInMB)
  {
    memInMB <<= 20;
    if (Ver == 7)
      return Ppmd7_Alloc(&_ppmd7, memInMB, &g_BigAlloc) != 0;
    return Ppmd8_Alloc(&_ppmd8, memInMB, &g_BigAlloc) != 0;
  }

  void Init(unsigned order, unsigned restor)
  {
    if (Ver == 7)
      Ppmd7_Init(&_ppmd7, order);
    else
      Ppmd8_Init(&_ppmd8, order, restor);
  }
    
  bool InitRc(CByteInBufWrap *inStream)
  {
    if (Ver == 7)
    {
      _ppmd7.rc.dec.Stream = &inStream->vt;
      return (Ppmd7a_RangeDec_Init(&_ppmd7.rc.dec) != 0);
    }
    else
    {
      _ppmd8.Stream.In = &inStream->vt;
      return Ppmd8_Init_RangeDec(&_ppmd8) != 0;
    }
  }

  bool IsFinishedOK()
  {
    if (Ver == 7)
      return Ppmd7z_RangeDec_IsFinishedOK(&_ppmd7.rc.dec);
    return Ppmd8_RangeDec_IsFinishedOK(&_ppmd8);
  }
};


Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  if (numItems == 0)
    return S_OK;
  if (numItems != (UInt32)(Int32)-1 && (numItems != 1 || indices[0] != 0))
    return E_INVALIDARG;

  // extractCallback->SetTotal(_packSize);
  UInt64 currentTotalPacked = 0;
  RINOK(extractCallback->SetCompleted(&currentTotalPacked))
  CMyComPtr<ISequentialOutStream> realOutStream;
  const Int32 askMode = testMode ?
      NExtract::NAskMode::kTest :
      NExtract::NAskMode::kExtract;
  RINOK(extractCallback->GetStream(0, &realOutStream, askMode))
  if (!testMode && !realOutStream)
    return S_OK;

  extractCallback->PrepareOperation(askMode);

  CByteInBufWrap inBuf;
  if (!inBuf.Alloc(1 << 20))
    return E_OUTOFMEMORY;
  inBuf.Stream = _stream;

  CBuf outBuf;
  if (!outBuf.Alloc())
    return E_OUTOFMEMORY;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, true);

  CPpmdCpp ppmd(_item.Ver);
  if (!ppmd.Alloc(_item.MemInMB))
    return E_OUTOFMEMORY;
  
  Int32 opRes = NExtract::NOperationResult::kUnsupportedMethod;

  if (_item.IsSupported())
  {
    opRes = NExtract::NOperationResult::kDataError;
    
    ppmd.Init(_item.Order, _item.Restor);
    inBuf.Init();
    UInt64 outSize = 0;
    
    if (ppmd.InitRc(&inBuf) && !inBuf.Extra && inBuf.Res == S_OK)
    for (;;)
    {
      lps->InSize = _packSize = inBuf.GetProcessed();
      lps->OutSize = outSize;
      RINOK(lps->SetCur())

      size_t i;
      int sym = 0;

      Byte *buf = outBuf.Buf;
      if (ppmd.Ver == 7)
      {
        for (i = 0; i < kBufSize; i++)
        {
          sym = Ppmd7a_DecodeSymbol(&ppmd._ppmd7);
          if (inBuf.Extra || sym < 0)
            break;
          buf[i] = (Byte)sym;
        }
      }
      else
      {
        for (i = 0; i < kBufSize; i++)
        {
          sym = Ppmd8_DecodeSymbol(&ppmd._ppmd8);
          if (inBuf.Extra || sym < 0)
            break;
          buf[i] = (Byte)sym;
        }
      }

      outSize += i;
      _packSize = _headerSize + inBuf.GetProcessed();
      _packSize_Defined = true;
      if (realOutStream)
      {
        RINOK(WriteStream(realOutStream, outBuf.Buf, i))
      }

      if (inBuf.Extra)
      {
        opRes = NExtract::NOperationResult::kUnexpectedEnd;
        break;
      }

      if (sym < 0)
      {
        if (sym == -1 && ppmd.IsFinishedOK())
          opRes = NExtract::NOperationResult::kOK;
        break;
      }
    }
    
    RINOK(inBuf.Res)
  }
  
  realOutStream.Release();
  return extractCallback->SetOperationResult(opRes);
}


static const Byte k_Signature[] = { 0x8F, 0xAF, 0xAC, 0x84 };

REGISTER_ARC_I(
  "Ppmd", "pmd", NULL, 0xD,
  k_Signature,
  0,
  0,
  NULL)

}}

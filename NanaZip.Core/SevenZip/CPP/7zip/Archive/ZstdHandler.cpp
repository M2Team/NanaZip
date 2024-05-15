// ZstdHandler.cpp

#include "StdAfx.h"

// #define Z7_USE_ZSTD_ORIG_DECODER
// #define Z7_USE_ZSTD_COMPRESSION

#include "../../Common/ComTry.h"

#include "../Common/MethodProps.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"
#include "../Compress/ZstdDecoder.h"
#ifdef Z7_USE_ZSTD_ORIG_DECODER
#include "../Compress/Zstd2Decoder.h"
#endif

#ifdef Z7_USE_ZSTD_COMPRESSION
#include "../Compress/ZstdEncoder.h"
#include "../Compress/ZstdEncoderProps.h"
#include "Common/HandlerOut.h"
#endif

#include "Common/DummyOutStream.h"

#include "../../../C/CpuArch.h"

using namespace NWindows;

namespace NArchive {
namespace NZstd {

#define DESCRIPTOR_Get_DictionaryId_Flag(d)   ((d) & 3)
#define DESCRIPTOR_FLAG_CHECKSUM              (1 << 2)
#define DESCRIPTOR_FLAG_RESERVED              (1 << 3)
#define DESCRIPTOR_FLAG_UNUSED                (1 << 4)
#define DESCRIPTOR_FLAG_SINGLE                (1 << 5)
#define DESCRIPTOR_Get_ContentSize_Flag3(d)   ((d) >> 5)
#define DESCRIPTOR_Is_ContentSize_Defined(d)  (((d) & 0xe0) != 0)

struct CFrameHeader
{
  Byte Descriptor;
  Byte WindowDescriptor;
  UInt32 DictionaryId;
  UInt64 ContentSize;

  /* by zstd specification:
      the decoder must check that (Is_Reserved() == false)
      the decoder must ignore Unused_bit */
  bool Is_Reserved() const                { return (Descriptor & DESCRIPTOR_FLAG_RESERVED) != 0; }
  bool Is_Checksum() const                { return (Descriptor & DESCRIPTOR_FLAG_CHECKSUM) != 0; }
  bool Is_SingleSegment() const           { return (Descriptor & DESCRIPTOR_FLAG_SINGLE) != 0; }
  bool Is_ContentSize_Defined() const     { return DESCRIPTOR_Is_ContentSize_Defined(Descriptor); }
  unsigned Get_DictionaryId_Flag() const  { return DESCRIPTOR_Get_DictionaryId_Flag(Descriptor); }
  unsigned Get_ContentSize_Flag3() const  { return DESCRIPTOR_Get_ContentSize_Flag3(Descriptor); }
  
  const Byte *Parse(const Byte *p, int size)
  {
    if ((unsigned)size < 2)
      return NULL;
    Descriptor = *p++;
    size--;
    {
      Byte w = 0;
      if (!Is_SingleSegment())
      {
        w = *p++;
        size--;
      }
      WindowDescriptor = w;
    }
    {
      unsigned n = Get_DictionaryId_Flag();
      UInt32 d = 0;
      if (n)
      {
        n = (unsigned)1 << (n - 1);
        if ((size -= (int)n) < 0)
          return NULL;
        d = GetUi32(p) & ((UInt32)(Int32)-1 >> (32 - 8u * n));
        p += n;
      }
      DictionaryId = d;
    }
    {
      unsigned n = Get_ContentSize_Flag3();
      UInt64 v = 0;
      if (n)
      {
        n >>= 1;
        if (n == 1)
          v = 256;
        n = (unsigned)1 << n;
        if ((size -= (int)n) < 0)
          return NULL;
        v += GetUi64(p) & ((UInt64)(Int64)-1 >> (64 - 8u * n));
        p += n;
      }
      ContentSize = v;
    }
    return p;
  }
};



class CHandler Z7_final:
  public IInArchive,
  public IArchiveOpenSeq,
  public ISetProperties,
#ifdef Z7_USE_ZSTD_COMPRESSION
  public IOutArchive,
#endif
  public CMyUnknownImp
{
  Z7_COM_QI_BEGIN2(IInArchive)
  Z7_COM_QI_ENTRY(IArchiveOpenSeq)
  Z7_COM_QI_ENTRY(ISetProperties)
#ifdef Z7_USE_ZSTD_COMPRESSION
  Z7_COM_QI_ENTRY(IOutArchive)
#endif
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE
  
  Z7_IFACE_COM7_IMP(IInArchive)
  Z7_IFACE_COM7_IMP(IArchiveOpenSeq)
  Z7_IFACE_COM7_IMP(ISetProperties)
#ifdef Z7_USE_ZSTD_COMPRESSION
  Z7_IFACE_COM7_IMP(IOutArchive)
#endif

  bool _isArc;
  bool _needSeekToStart;
  // bool _dataAfterEnd;
  // bool _needMoreInput;
  bool _unsupportedBlock;

  bool _wasParsed;
  bool _phySize_Decoded_Defined;
  bool _unpackSize_Defined; // decoded
  bool _decoded_Info_Defined;
  
  bool _parseMode;
  bool _disableHash;
  // bool _smallMode;

  UInt64 _phySize;
  UInt64 _phySize_Decoded;
  UInt64 _unpackSize;

  CZstdDecInfo _parsed_Info;
  CZstdDecInfo _decoded_Info;

  CMyComPtr<IInStream> _stream;
  CMyComPtr<ISequentialInStream> _seqStream;

#ifdef Z7_USE_ZSTD_COMPRESSION
  CSingleMethodProps _props;
#endif

public:
  CHandler():
    _parseMode(false),
    _disableHash(false)
    // _smallMode(false)
    {}
};


static const Byte kProps[] =
{
  kpidSize,
  kpidPackSize
};

static const Byte kArcProps[] =
{
  kpidNumStreams,
  kpidNumBlocks,
  kpidMethod,
  // kpidChecksum
  kpidCRC
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps


// static const unsigned kBlockType_Raw = 0;
static const unsigned kBlockType_RLE = 1;
// static const unsigned kBlockType_Compressed = 2;
static const unsigned kBlockType_Reserved = 3;
/*
static const char * const kNames[] =
{
    "RAW"
  , "RLE"
  , "Compressed"
  , "Reserved"
};
*/

static void Add_UInt64(AString &s, const char *name, UInt64 v)
{
  s.Add_OptSpaced(name);
  s.Add_Colon();
  s.Add_UInt64(v);
}


static void PrintSize(AString &s, UInt64 w)
{
  char c = 0;
       if ((w & ((1 << 30) - 1)) == 0)  { c = 'G'; w >>= 30; }
  else if ((w & ((1 << 20) - 1)) == 0)  { c = 'M'; w >>= 20; }
  else if ((w & ((1 << 10) - 1)) == 0)  { c = 'K'; w >>= 10; }
  s.Add_UInt64(w);
  if (c)
  {
    s.Add_Char(c);
    s += "iB";
  }
}


Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  NCOM::CPropVariant prop;

  CZstdDecInfo *p = NULL;
  if (_wasParsed || !_decoded_Info_Defined)
    p = &_parsed_Info;
  else if (_decoded_Info_Defined)
    p = &_decoded_Info;

  switch (propID)
  {
    case kpidPhySize:
      if (_wasParsed)
        prop = _phySize;
      else if (_phySize_Decoded_Defined)
        prop = _phySize_Decoded;
      break;

    case kpidUnpackSize:
      if (_unpackSize_Defined)
        prop = _unpackSize;
      break;
    
    case kpidNumStreams:
      if (p)
      if (_wasParsed || _decoded_Info_Defined)
        prop = p->num_DataFrames;
      break;
     
    case kpidNumBlocks:
      if (p)
      if (_wasParsed || _decoded_Info_Defined)
        prop = p->num_Blocks;
      break;
    
    // case kpidChecksum:
    case kpidCRC:
      if (p)
        if (p->checksum_Defined && p->num_DataFrames == 1)
          prop = p->checksum; // it's checksum from last frame
      break;

    case kpidMethod:
    {
      AString s;
      s.Add_OptSpaced(p == &_decoded_Info ?
          "decoded:" : _wasParsed ?
          "parsed:" :
          "header-open-only:");
      
      if (p->dictionaryId != 0)
      {
        if (p->are_DictionaryId_Different)
          s.Add_OptSpaced("different-dictionary-IDs");
        s.Add_OptSpaced("dictionary-ID:");
        s.Add_UInt32(p->dictionaryId);
      }
      /*
      if (ContentSize_Defined)
      {
        s.Add_OptSpaced("ContentSize=");
        s.Add_UInt64(ContentSize_Total);
      }
      */
      // if (p->are_Checksums)
      if (p->descriptor_OR & DESCRIPTOR_FLAG_CHECKSUM)
        s.Add_OptSpaced("XXH64");
      if (p->descriptor_NOT_OR & DESCRIPTOR_FLAG_CHECKSUM)
        s.Add_OptSpaced("NO-XXH64");

      if (p->descriptor_OR & DESCRIPTOR_FLAG_UNUSED)
        s.Add_OptSpaced("unused_bit");

      if (p->descriptor_OR & DESCRIPTOR_FLAG_SINGLE)
        s.Add_OptSpaced("single-segments");

      if (p->descriptor_NOT_OR & DESCRIPTOR_FLAG_SINGLE)
      {
        // Add_UInt64(s, "wnd-descriptors", p->num_WindowDescriptors);
        s.Add_OptSpaced("wnd-desc-log-MAX:");
        // WindowDescriptor_MAX = 16 << 3; // for debug
        const unsigned e = p->windowDescriptor_MAX >> 3;
        s.Add_UInt32(e + 10);
        const unsigned m = p->windowDescriptor_MAX & 7;
        if (m != 0)
        {
          s.Add_Dot();
          s.Add_UInt32(m);
        }
      }
      
      if (DESCRIPTOR_Is_ContentSize_Defined(p->descriptor_OR) ||
          (p->descriptor_NOT_OR & DESCRIPTOR_FLAG_SINGLE))
      /*
      if (p->are_ContentSize_Known ||
          p->are_WindowDescriptors)
      */
      {
        s.Add_OptSpaced("wnd-MAX:");
        PrintSize(s, p->windowSize_MAX);
        if (p->windowSize_MAX != p->windowSize_Allocate_MAX)
        {
          s.Add_OptSpaced("wnd-use-MAX:");
          PrintSize(s, p->windowSize_Allocate_MAX);
        }
      }

      if (p->num_DataFrames != 1)
        Add_UInt64(s, "data-frames", p->num_DataFrames);
      if (p->num_SkipFrames != 0)
      {
        Add_UInt64(s, "skip-frames", p->num_SkipFrames);
        Add_UInt64(s, "skip-frames-size-total", p->skipFrames_Size);
      }

      if (p->are_ContentSize_Unknown)
        s.Add_OptSpaced("unknown-content-size");

      if (DESCRIPTOR_Is_ContentSize_Defined(p->descriptor_OR))
      {
        Add_UInt64(s, "content-size-frame-max", p->contentSize_MAX);
        Add_UInt64(s, "content-size-total", p->contentSize_Total);
      }
     
      /*
      for (unsigned i = 0; i < 4; i++)
      {
        const UInt64 n = p->num_Blocks_forType[i];
        if (n)
        {
          s.Add_OptSpaced(kNames[i]);
          s += "-blocks:";
          s.Add_UInt64(n);

          s.Add_OptSpaced(kNames[i]);
          s += "-block-bytes:";
          s.Add_UInt64(p->num_BlockBytes_forType[i]);
        }
      }
      */
      prop = s;
      break;
    }

    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc)            v |= kpv_ErrorFlags_IsNotArc;
      // if (_needMoreInput)     v |= kpv_ErrorFlags_UnexpectedEnd;
      // if (_dataAfterEnd)      v |= kpv_ErrorFlags_DataAfterEnd;
      if (_unsupportedBlock)  v |= kpv_ErrorFlags_UnsupportedMethod;
      /*
      if (_parsed_Info.numBlocks_forType[kBlockType_Reserved])
        v |= kpv_ErrorFlags_UnsupportedMethod;
      */
      prop = v;
      break;
    }

    default: break;
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
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPackSize:
      if (_wasParsed)
        prop = _phySize;
      else if (_phySize_Decoded_Defined)
        prop = _phySize_Decoded;
      break;

    case kpidSize:
      if (_wasParsed && !_parsed_Info.are_ContentSize_Unknown)
        prop = _parsed_Info.contentSize_Total;
      else if (_unpackSize_Defined)
        prop = _unpackSize;
      break;

    default: break;
  }
  prop.Detach(value);
  return S_OK;
}

static const unsigned kSignatureSize = 4;
static const Byte k_Signature[kSignatureSize] = { 0x28, 0xb5, 0x2f, 0xfd } ;

static const UInt32 kDataFrameSignature32    = 0xfd2fb528;
static const UInt32 kSkipFrameSignature      = 0x184d2a50;
static const UInt32 kSkipFrameSignature_Mask = 0xfffffff0;

/*
API_FUNC_static_IsArc IsArc_Zstd(const Byte *p, size_t size)
{
  if (size < kSignatureSize)
    return k_IsArc_Res_NEED_MORE;
  if (memcmp(p, k_Signature, kSignatureSize) != 0)
  {
    const UInt32 v = GetUi32(p);
    if ((v & kSkipFrameSignature_Mask) != kSkipFrameSignature)
      return k_IsArc_Res_NO;
    return k_IsArc_Res_YES;
  }
  p += 4;
  // return k_IsArc_Res_YES;
}
}
*/

// kBufSize must be >= (ZSTD_FRAMEHEADERSIZE_MAX = 18)
// we use big buffer for fast parsing of worst case small blocks.
static const unsigned kBufSize =
     1 << 9;
     // 1 << 14; // fastest in real file

struct CStreamBuffer
{
  unsigned pos;
  unsigned lim;
  IInStream *Stream;
  UInt64 StreamOffset;
  Byte buf[kBufSize];

  CStreamBuffer():
      pos(0),
      lim(0),
      StreamOffset(0)
      {}
  unsigned Avail() const { return lim - pos; }
  const Byte *GetPtr() const { return &buf[pos]; }
  UInt64 GetCurOffset() const { return StreamOffset - Avail(); }
  void SkipInBuf(UInt32 size) { pos += size; }
  HRESULT Skip(UInt32 size);
  HRESULT Read(unsigned num);
};

HRESULT CStreamBuffer::Skip(UInt32 size)
{
  unsigned rem = lim - pos;
  if (rem != 0)
  {
    if (rem > size)
      rem = size;
    pos += rem;
    size -= rem;
    if (pos != lim)
      return S_OK;
  }
  if (size == 0)
    return S_OK;
  return Stream->Seek(size, STREAM_SEEK_CUR, &StreamOffset);
}

HRESULT CStreamBuffer::Read(unsigned num)
{
  if (lim - pos >= num)
    return S_OK;
  if (pos != 0)
  {
    lim -= pos;
    memmove(buf, buf + pos, lim);
    pos = 0;
  }
  size_t processed = kBufSize - ((unsigned)StreamOffset & (kBufSize - 1));
  const unsigned avail = kBufSize - lim;
  num -= lim;
  if (avail < processed || processed < num)
    processed = avail;
  const HRESULT res = ReadStream(Stream, buf + lim, &processed);
  StreamOffset += processed;
  lim += (unsigned)processed;
  return res;
}


static const unsigned k_ZSTD_FRAMEHEADERSIZE_MAX = 4 + 14;

Z7_COM7F_IMF(CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *callback))
{
  COM_TRY_BEGIN
  Close();

  CZstdDecInfo *p = &_parsed_Info;
  // p->are_ContentSize_Unknown = False;
  CStreamBuffer sb;
  sb.Stream = stream;
  
  for (;;)
  {
    RINOK(sb.Read(k_ZSTD_FRAMEHEADERSIZE_MAX))
    if (sb.Avail() < kSignatureSize)
      break;

    if (callback && (ZstdDecInfo_GET_NUM_FRAMES(p) & 0xFFF) == 2)
    {
      const UInt64 numBytes = sb.GetCurOffset();
      RINOK(callback->SetCompleted(NULL, &numBytes))
    }

    const UInt32 v = GetUi32(sb.GetPtr());
    if (v != kDataFrameSignature32)
    {
      if ((v & kSkipFrameSignature_Mask) != kSkipFrameSignature)
        break;
      _phySize = sb.GetCurOffset() + 8;
      p->num_SkipFrames++;
      sb.SkipInBuf(4);
      if (sb.Avail() < 4)
        break;
      const UInt32 size = GetUi32(sb.GetPtr());
      p->skipFrames_Size += size;
      sb.SkipInBuf(4);
      _phySize = sb.GetCurOffset() + size;
      RINOK(sb.Skip(size))
      continue;
    }

    p->num_DataFrames++;
    // _numStreams_Defined = true;
    sb.SkipInBuf(4);
    CFrameHeader fh;
    {
      const Byte *data = fh.Parse(sb.GetPtr(), (int)sb.Avail());
      if (!data)
      {
        // _needMoreInput = true;
        // we set size for one byte more to show that stream was truncated
        _phySize = sb.StreamOffset + 1;
        break;
      }
      if (fh.Is_Reserved())
      {
        // we don't want false detection
        if (ZstdDecInfo_GET_NUM_FRAMES(p) == 1)
          return S_FALSE;
        // _phySize = sb.GetCurOffset();
        break;
      }
      sb.SkipInBuf((unsigned)(data - sb.GetPtr()));
    }

    p->descriptor_OR     = (Byte)(p->descriptor_OR     |  fh.Descriptor);
    p->descriptor_NOT_OR = (Byte)(p->descriptor_NOT_OR | ~fh.Descriptor);

    // _numBlocks_Defined = true;
    // if (fh.Get_DictionaryId_Flag())
    // p->dictionaryId_Cur = fh.DictionaryId;
    if (fh.DictionaryId != 0)
    {
      if (p->dictionaryId == 0)
        p->dictionaryId = fh.DictionaryId;
      else if (p->dictionaryId != fh.DictionaryId)
        p->are_DictionaryId_Different = True;
    }

    UInt32 blockSizeAllowedMax = (UInt32)1 << 17;
    {
      UInt64 winSize = fh.ContentSize;
      UInt64 winSize_forAllocate = fh.ContentSize;
      if (!fh.Is_SingleSegment())
      {
        if (p->windowDescriptor_MAX < fh.WindowDescriptor)
            p->windowDescriptor_MAX = fh.WindowDescriptor;
        const unsigned e = (fh.WindowDescriptor >> 3);
        const unsigned m = (fh.WindowDescriptor & 7);
        winSize = (UInt64)(8 + m) << (e + 10 - 3);
        if (!fh.Is_ContentSize_Defined()
            || fh.DictionaryId != 0
            || winSize_forAllocate > winSize)
          winSize_forAllocate = winSize;
        // p->are_WindowDescriptors = true;
      }
      else
      {
        // p->are_SingleSegments = True;
      }
      if (blockSizeAllowedMax > winSize)
          blockSizeAllowedMax = (UInt32)winSize;
      if (p->windowSize_MAX < winSize)
          p->windowSize_MAX = winSize;
      if (p->windowSize_Allocate_MAX < winSize_forAllocate)
          p->windowSize_Allocate_MAX = winSize_forAllocate;
    }

    if (fh.Is_ContentSize_Defined())
    {
      // p->are_ContentSize_Known = True;
      p->contentSize_Total += fh.ContentSize;
      if (p->contentSize_MAX < fh.ContentSize)
          p->contentSize_MAX = fh.ContentSize;
    }
    else
    {
      p->are_ContentSize_Unknown = True;
    }

    p->checksum_Defined = false;

    // p->numBlocks_forType[3] += 99; // for debug

    if (!_parseMode)
    {
      if (ZstdDecInfo_GET_NUM_FRAMES(p) == 1)
        break;
    }

    _wasParsed = true;
     
    bool blocksWereParsed = false;

    for (;;)
    {
      if (callback && (p->num_Blocks & 0xFFF) == 2)
      {
        // Sleep(10);
        const UInt64 numBytes = sb.GetCurOffset();
        RINOK(callback->SetCompleted(NULL, &numBytes))
      }
      _phySize = sb.GetCurOffset() + 3;
      RINOK(sb.Read(3))
      if (sb.Avail() < 3)
      {
        // _needMoreInput = true;
        // return S_FALSE;
        break; // change it
      }
      const unsigned pos = sb.pos;
      sb.pos = pos + 3;
      UInt32 b = 0;
      b += (UInt32)sb.buf[pos];
      b += (UInt32)sb.buf[pos + 1] << (8 * 1);
      b += (UInt32)sb.buf[pos + 2] << (8 * 2);
      p->num_Blocks++;
      const unsigned blockType = (b >> 1) & 3;
      UInt32 size = b >> 3;
      // p->num_Blocks_forType[blockType]++;
      // p->num_BlockBytes_forType[blockType] += size;
      if (size > blockSizeAllowedMax
          || blockType == kBlockType_Reserved)
      {
        _unsupportedBlock = true;
        if (ZstdDecInfo_GET_NUM_FRAMES(p) == 1 && p->num_Blocks == 1)
          return S_FALSE;
        break;
      }
      if (blockType == kBlockType_RLE)
        size = 1;
      _phySize = sb.GetCurOffset() + size;
      RINOK(sb.Skip(size))
      if (b & 1)
      {
        // it's last block
        blocksWereParsed = true;
        break;
      }
    }

    if (!blocksWereParsed)
      break;

    if (fh.Is_Checksum())
    {
      _phySize = sb.GetCurOffset() + 4;
      RINOK(sb.Read(4))
      if (sb.Avail() < 4)
        break;
      p->checksum_Defined = true;
      // if (p->num_DataFrames == 1)
      p->checksum = GetUi32(sb.GetPtr());
      sb.SkipInBuf(4);
    }
  }
  
  if (ZstdDecInfo_GET_NUM_FRAMES(p) == 0)
    return S_FALSE;

  _needSeekToStart = true;
  // } // _parseMode
  _isArc = true;
  _stream = stream;
  _seqStream = stream;

  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::OpenSeq(ISequentialInStream *stream))
{
  Close();
  _isArc = true;
  _seqStream = stream;
  return S_OK;
}


Z7_COM7F_IMF(CHandler::Close())
{
  _isArc = false;
  _needSeekToStart = false;
  // _dataAfterEnd = false;
  // _needMoreInput = false;
  _unsupportedBlock = false;

  _wasParsed = false;
  _phySize_Decoded_Defined = false;
  _unpackSize_Defined = false;
  _decoded_Info_Defined = false;

  ZstdDecInfo_CLEAR(&_parsed_Info)
  ZstdDecInfo_CLEAR(&_decoded_Info)

  _phySize = 0;
  _phySize_Decoded = 0;
  _unpackSize = 0;

  _seqStream.Release();
  _stream.Release();
  return S_OK;
}


Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
  Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  if (numItems == 0)
    return S_OK;
  if (numItems != (UInt32)(Int32)-1 && (numItems != 1 || indices[0] != 0))
    return E_INVALIDARG;
  if (_wasParsed)
  {
    RINOK(extractCallback->SetTotal(_phySize))
  }

  Int32 opRes;
  {
    CMyComPtr<ISequentialOutStream> realOutStream;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    RINOK(extractCallback->GetStream(0, &realOutStream, askMode))
    if (!testMode && !realOutStream)
      return S_OK;
      
    extractCallback->PrepareOperation(askMode);
    
    if (_needSeekToStart)
    {
      if (!_stream)
        return E_FAIL;
      RINOK(_stream->Seek(0, STREAM_SEEK_SET, NULL))
    }
    else
      _needSeekToStart = true;
    
    CMyComPtr2_Create<ICompressProgressInfo, CLocalProgress> lps;
    lps->Init(extractCallback, true);
    
#ifdef Z7_USE_ZSTD_ORIG_DECODER
    CMyComPtr2_Create<ICompressCoder, NCompress::NZstd2::CDecoder> decoder;
#else
    CMyComPtr2_Create<ICompressCoder, NCompress::NZstd::CDecoder> decoder;
#endif

    CMyComPtr2_Create<ISequentialOutStream, CDummyOutStream> outStreamSpec;
    outStreamSpec->SetStream(realOutStream);
    outStreamSpec->Init();
    // realOutStream.Release();
    
    decoder->FinishMode = true;
#ifndef Z7_USE_ZSTD_ORIG_DECODER
    decoder->DisableHash = _disableHash;
#endif
    
    // _dataAfterEnd = false;
    // _needMoreInput = false;
    const HRESULT hres = decoder.Interface()->Code(_seqStream, outStreamSpec, NULL, NULL, lps);
    /*
    {
      UInt64 t1 = decoder->GetInputProcessedSize();
      // for debug
      const UInt32 kTempSize = 64;
      Byte buf[kTempSize];
      UInt32 processedSize = 0;
      RINOK(decoder->ReadUnusedFromInBuf(buf, kTempSize, &processedSize))
      processedSize -= processedSize;
      UInt64 t2 = decoder->GetInputProcessedSize();
      t2 = t2;
      t1 = t1;
    }
    */
    const UInt64 outSize = outStreamSpec->GetSize();
  // }

    // if (hres == E_ABORT) return hres;
    opRes = NExtract::NOperationResult::kDataError;
    
    if (hres == E_OUTOFMEMORY)
    {
      return hres;
      // opRes = NExtract::NOperationResult::kMemError;
    }
    else if (hres == S_OK || hres == S_FALSE)
    {
#ifndef Z7_USE_ZSTD_ORIG_DECODER
      _decoded_Info_Defined = true;
      _decoded_Info = decoder->_state.info;
      // NumDataFrames_Decoded = decoder->_state.info.num_DataFrames;
      // NumSkipFrames_Decoded = decoder->_state.info.num_SkipFrames;
      const UInt64 inSize = decoder->_inProcessed;
#else
      const UInt64 inSize = decoder->GetInputProcessedSize();
#endif
      _phySize_Decoded = inSize;
      _phySize_Decoded_Defined = true;
      
      _unpackSize_Defined = true;
      _unpackSize = outSize;

      // RINOK(
      lps.Interface()->SetRatioInfo(&inSize, &outSize);
      
#ifdef Z7_USE_ZSTD_ORIG_DECODER
      if (hres == S_OK)
        opRes = NExtract::NOperationResult::kOK;
#else
      if (decoder->ResInfo.decode_SRes == SZ_ERROR_CRC)
      {
        opRes = NExtract::NOperationResult::kCRCError;
      }
      else if (decoder->ResInfo.decode_SRes == SZ_ERROR_NO_ARCHIVE)
      {
        _isArc = false;
        opRes = NExtract::NOperationResult::kIsNotArc;
      }
      else if (decoder->ResInfo.decode_SRes == SZ_ERROR_INPUT_EOF)
        opRes = NExtract::NOperationResult::kUnexpectedEnd;
      else
      {
        if (hres == S_OK && decoder->ResInfo.decode_SRes == SZ_OK)
          opRes = NExtract::NOperationResult::kOK;
        if (decoder->ResInfo.extraSize)
        {
          // if (inSize == 0) _isArc = false;
          opRes = NExtract::NOperationResult::kDataAfterEnd;
        }
        /*
        if (decoder->ResInfo.unexpededEnd)
          opRes = NExtract::NOperationResult::kUnexpectedEnd;
        */
      }
#endif
    }
    else if (hres == E_NOTIMPL)
    {
      opRes = NExtract::NOperationResult::kUnsupportedMethod;
    }
    else
      return hres;
  }

  return extractCallback->SetOperationResult(opRes);

  COM_TRY_END
}



Z7_COM7F_IMF(CHandler::SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps))
{
  // return _props.SetProperties(names, values, numProps);
  // _smallMode = false;
  _disableHash = false;
  _parseMode = false;
  // _parseMode = true; // for debug
#ifdef Z7_USE_ZSTD_COMPRESSION
  _props.Init();
#endif

  for (UInt32 i = 0; i < numProps; i++)
  {
    UString name = names[i];
    const PROPVARIANT &value = values[i];
    
    if (name.IsEqualTo("parse"))
    {
      bool parseMode = true;
      RINOK(PROPVARIANT_to_bool(value, parseMode))
        _parseMode = parseMode;
      continue;
    }
    if (name.IsPrefixedBy_Ascii_NoCase("crc"))
    {
      name.Delete(0, 3);
      UInt32 crcSize = 4;
      RINOK(ParsePropToUInt32(name, value, crcSize))
      if (crcSize == 0)
        _disableHash = true;
      else if (crcSize == 4)
        _disableHash = false;
      else
        return E_INVALIDARG;
      continue;
    }
#ifdef Z7_USE_ZSTD_COMPRESSION
    /*
    if (name.IsEqualTo("small"))
    {
      bool smallMode = true;
      RINOK(PROPVARIANT_to_bool(value, smallMode))
      _smallMode = smallMode;
      continue;
    }
    */
    RINOK(_props.SetProperty(names[i], value))
#endif
  }
  return S_OK;
}




#ifdef Z7_USE_ZSTD_COMPRESSION

Z7_COM7F_IMF(CHandler::GetFileTimeType(UInt32 *timeType))
{
  *timeType = GET_FileTimeType_NotDefined_for_GetFileTimeType;
  // *timeType = NFileTimeType::kUnix;
  return S_OK;
}


Z7_COM7F_IMF(CHandler::UpdateItems(ISequentialOutStream *outStream, UInt32 numItems,
    IArchiveUpdateCallback *updateCallback))
{
  COM_TRY_BEGIN

  if (numItems != 1)
    return E_INVALIDARG;
  {
    CMyComPtr<IStreamSetRestriction> setRestriction;
    outStream->QueryInterface(IID_IStreamSetRestriction, (void **)&setRestriction);
    if (setRestriction)
      RINOK(setRestriction->SetRestriction(0, 0))
  }
  Int32 newData, newProps;
  UInt32 indexInArchive;
  if (!updateCallback)
    return E_FAIL;
  RINOK(updateCallback->GetUpdateItemInfo(0, &newData, &newProps, &indexInArchive))
 
  if (IntToBool(newProps))
  {
    {
      NCOM::CPropVariant prop;
      RINOK(updateCallback->GetProperty(0, kpidIsDir, &prop))
      if (prop.vt != VT_EMPTY)
        if (prop.vt != VT_BOOL || prop.boolVal != VARIANT_FALSE)
          return E_INVALIDARG;
    }
  }

  if (IntToBool(newData))
  {
    UInt64 size;
    {
      NCOM::CPropVariant prop;
      RINOK(updateCallback->GetProperty(0, kpidSize, &prop))
      if (prop.vt != VT_UI8)
        return E_INVALIDARG;
      size = prop.uhVal.QuadPart;
    }

    if (!_props.MethodName.IsEmpty()
        && !_props.MethodName.IsEqualTo_Ascii_NoCase("zstd"))
      return E_INVALIDARG;

    {
      CMyComPtr<ISequentialInStream> fileInStream;
      RINOK(updateCallback->GetStream(0, &fileInStream))
      if (!fileInStream)
        return S_FALSE;
      {
        CMyComPtr<IStreamGetSize> streamGetSize;
        fileInStream.QueryInterface(IID_IStreamGetSize, &streamGetSize);
        if (streamGetSize)
        {
          UInt64 size2;
          if (streamGetSize->GetSize(&size2) == S_OK)
            size = size2;
        }
      }
      RINOK(updateCallback->SetTotal(size))

      CMethodProps props2 = _props;

#ifndef Z7_ST
      /*
      CSingleMethodProps (_props)
      derives from
      CMethodProps (props2)
      So we transfer additional variable (num Threads) to CMethodProps list of properties
      */

      UInt32 numThreads = _props._numThreads;

      if (numThreads > Z7_ZSTDMT_NBWORKERS_MAX)
          numThreads = Z7_ZSTDMT_NBWORKERS_MAX;

      if (_props.FindProp(NCoderPropID::kNumThreads) < 0)
      {
        if (!_props._numThreads_WasForced
            && numThreads >= 1
            && _props._memUsage_WasSet)
        {
          NCompress::NZstd::CEncoderProps zstdProps;
          RINOK(zstdProps.SetFromMethodProps(_props))
          ZstdEncProps_NormalizeFull(&zstdProps.EncProps);
          numThreads = ZstdEncProps_GetNumThreads_for_MemUsageLimit(
              &zstdProps.EncProps, _props._memUsage_Compress, numThreads);
        }
        props2.AddProp_NumThreads(numThreads);
      }

#endif // Z7_ST

      CMyComPtr2_Create<ICompressProgressInfo, CLocalProgress> lps;
      lps->Init(updateCallback, true);
      {
        CMyComPtr2_Create<ICompressCoder, NCompress::NZstd::CEncoder> encoder;
        // size = 1 << 24; // for debug
        RINOK(props2.SetCoderProps(encoder.ClsPtr(), size != (UInt64)(Int64)-1 ? &size : NULL))
        // encoderSpec->_props.SmallFileOpt = _smallMode;
        // we must set kExpectedDataSize just before Code().
        encoder->SrcSizeHint64 = size;
        /*
        CMyComPtr<ICompressSetCoderPropertiesOpt> optProps;
        _compressEncoder->QueryInterface(IID_ICompressSetCoderPropertiesOpt, (void**)&optProps);
        if (optProps)
        {
          PROPID propID = NCoderPropID::kExpectedDataSize;
          NWindows::NCOM::CPropVariant prop = (UInt64)size;
          // RINOK(optProps->SetCoderPropertiesOpt(&propID, &prop, 1))
          RINOK(encoderSpec->SetCoderPropertiesOpt(&propID, &prop, 1))
        }
        */
        RINOK(encoder.Interface()->Code(fileInStream, outStream, NULL, NULL, lps))
      }
    }
    return updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK);
  }

  if (indexInArchive != 0)
    return E_INVALIDARG;

  CMyComPtr2_Create<ICompressProgressInfo, CLocalProgress> lps;
  lps->Init(updateCallback, true);

  CMyComPtr<IArchiveUpdateCallbackFile> opCallback;
  updateCallback->QueryInterface(IID_IArchiveUpdateCallbackFile, (void **)&opCallback);
  if (opCallback)
  {
    RINOK(opCallback->ReportOperation(
        NEventIndexType::kInArcIndex, 0,
        NUpdateNotifyOp::kReplicate))
  }

  if (_stream)
    RINOK(_stream->Seek(0, STREAM_SEEK_SET, NULL))

  return NCompress::CopyStream(_stream, outStream, lps);

  COM_TRY_END
}
#endif



#ifndef Z7_USE_ZSTD_COMPRESSION
#undef  IMP_CreateArcOut
#define IMP_CreateArcOut
#undef  CreateArcOut
#define CreateArcOut NULL
#endif

#ifdef Z7_USE_ZSTD_COMPRESSION
REGISTER_ARC_IO(
  "zstd2", "zst tzst", "* .tar", 0xe + 1,
  k_Signature, 0
  , NArcInfoFlags::kKeepName
  , 0
  , NULL)
#else
REGISTER_ARC_IO(
  "zstd", "zst tzst", "* .tar", 0xe,
  k_Signature, 0
  , NArcInfoFlags::kKeepName
  , 0
  , NULL)
#endif

}}

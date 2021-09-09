// XzHandler.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"

#include "../../Common/ComTry.h"
#include "../../Common/Defs.h"
#include "../../Common/IntToString.h"
#include "../../Common/MyBuffer.h"
#include "../../Common/StringToInt.h"

#include "../../Windows/PropVariant.h"
#include "../../Windows/System.h"

#include "../Common/CWrappers.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"
#include "../Compress/XzDecoder.h"
#include "../Compress/XzEncoder.h"

#include "IArchive.h"

#include "Common/HandlerOut.h"

using namespace NWindows;

namespace NArchive {
namespace NXz {

#define k_LZMA2_Name "LZMA2"


struct CBlockInfo
{
  unsigned StreamFlags;
  UInt64 PackPos;
  UInt64 PackSize; // pure value from Index record, it doesn't include pad zeros
  UInt64 UnpackPos;
};


class CHandler:
  public IInArchive,
  public IArchiveOpenSeq,
  public IInArchiveGetStream,
  public ISetProperties,

  #ifndef EXTRACT_ONLY
  public IOutArchive,
  #endif

  public CMyUnknownImp,

  #ifndef EXTRACT_ONLY
    public CMultiMethodProps
  #else
    public CCommonMethodProps
  #endif
{
  CXzStatInfo _stat;    // it's stat from backward parsing
  CXzStatInfo _stat2;   // it's data from forward parsing, if the decoder was called
  SRes _stat2_decode_SRes;
  bool _stat_defined;
  bool _stat2_defined;

  const CXzStatInfo *GetStat() const
  {
    if (_stat_defined) return &_stat;
    if (_stat2_defined) return &_stat2;
    return NULL;
  }
  
  bool _isArc;
  bool _needSeekToStart;
  bool _firstBlockWasRead;

  AString _methodsString;


  #ifndef EXTRACT_ONLY

  UInt32 _filterId;
  UInt64 _numSolidBytes;

  void InitXz()
  {
    _filterId = 0;
    _numSolidBytes = XZ_PROPS__BLOCK_SIZE__AUTO;
  }

  #endif


  void Init()
  {
    #ifndef EXTRACT_ONLY
      InitXz();
      CMultiMethodProps::Init();
    #else
      CCommonMethodProps::InitCommon();
    #endif
  }
  
  HRESULT SetProperty(const wchar_t *name, const PROPVARIANT &value);

  HRESULT Open2(IInStream *inStream, /* UInt32 flags, */ IArchiveOpenCallback *callback);

  HRESULT Decode(NCompress::NXz::CDecoder &decoder,
      ISequentialInStream *seqInStream,
      ISequentialOutStream *outStream,
      ICompressProgressInfo *progress)
  {
    #ifndef _7ZIP_ST
    decoder._numThreads = _numThreads;
    #endif
    decoder._memUsage = _memUsage;

    HRESULT hres = decoder.Decode(seqInStream, outStream,
        NULL, // *outSizeLimit
        true, // finishStream
        progress);
    
    if (decoder.MainDecodeSRes_wasUsed
        && decoder.MainDecodeSRes != SZ_ERROR_MEM
        && decoder.MainDecodeSRes != SZ_ERROR_UNSUPPORTED)
    {
      // if (!_stat2_defined)
      {
        _stat2_decode_SRes = decoder.MainDecodeSRes;
        _stat2 = decoder.Stat;
        _stat2_defined = true;
      }
    }

    return hres;
  }

public:
  MY_QUERYINTERFACE_BEGIN2(IInArchive)
  MY_QUERYINTERFACE_ENTRY(IArchiveOpenSeq)
  MY_QUERYINTERFACE_ENTRY(IInArchiveGetStream)
  MY_QUERYINTERFACE_ENTRY(ISetProperties)
  #ifndef EXTRACT_ONLY
  MY_QUERYINTERFACE_ENTRY(IOutArchive)
  #endif
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE

  INTERFACE_IInArchive(;)
  STDMETHOD(OpenSeq)(ISequentialInStream *stream);
  STDMETHOD(GetStream)(UInt32 index, ISequentialInStream **stream);
  STDMETHOD(SetProperties)(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps);

  #ifndef EXTRACT_ONLY
  INTERFACE_IOutArchive(;)
  #endif

  CBlockInfo *_blocks;
  size_t _blocksArraySize;
  UInt64 _maxBlocksSize;
  CMyComPtr<IInStream> _stream;
  CMyComPtr<ISequentialInStream> _seqStream;

  CXzBlock _firstBlock;

  CHandler();
  ~CHandler();

  HRESULT SeekToPackPos(UInt64 pos)
  {
    return _stream->Seek((Int64)pos, STREAM_SEEK_SET, NULL);
  }
};


CHandler::CHandler():
    _blocks(NULL),
    _blocksArraySize(0)
{
  #ifndef EXTRACT_ONLY
  InitXz();
  #endif
}

CHandler::~CHandler()
{
  MyFree(_blocks);
}


static const Byte kProps[] =
{
  kpidSize,
  kpidPackSize,
  kpidMethod
};

static const Byte kArcProps[] =
{
  kpidMethod,
  kpidNumStreams,
  kpidNumBlocks,
  kpidClusterSize,
  kpidCharacts
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

static inline char GetHex(unsigned value)
{
  return (char)((value < 10) ? ('0' + value) : ('A' + (value - 10)));
}

static inline void AddHexToString(AString &s, Byte value)
{
  s += GetHex(value >> 4);
  s += GetHex(value & 0xF);
}

static void Lzma2PropToString(AString &s, unsigned prop)
{
  char c = 0;
  UInt32 size;
  if ((prop & 1) == 0)
    size = prop / 2 + 12;
  else
  {
    c = 'k';
    size = (UInt32)(2 | (prop & 1)) << (prop / 2 + 1);
    if (prop > 17)
    {
      size >>= 10;
      c = 'm';
    }
  }
  s.Add_UInt32(size);
  if (c != 0)
    s += c;
}

struct CMethodNamePair
{
  UInt32 Id;
  const char *Name;
};

static const CMethodNamePair g_NamePairs[] =
{
  { XZ_ID_Subblock, "SB" },
  { XZ_ID_Delta, "Delta" },
  { XZ_ID_X86, "BCJ" },
  { XZ_ID_PPC, "PPC" },
  { XZ_ID_IA64, "IA64" },
  { XZ_ID_ARM, "ARM" },
  { XZ_ID_ARMT, "ARMT" },
  { XZ_ID_SPARC, "SPARC" },
  { XZ_ID_LZMA2, "LZMA2" }
};

static void AddMethodString(AString &s, const CXzFilter &f)
{
  const char *p = NULL;
  for (unsigned i = 0; i < ARRAY_SIZE(g_NamePairs); i++)
    if (g_NamePairs[i].Id == f.id)
    {
      p = g_NamePairs[i].Name;
      break;
    }
  char temp[32];
  if (!p)
  {
    ::ConvertUInt64ToString(f.id, temp);
    p = temp;
  }

  s += p;

  if (f.propsSize > 0)
  {
    s += ':';
    if (f.id == XZ_ID_LZMA2 && f.propsSize == 1)
      Lzma2PropToString(s, f.props[0]);
    else if (f.id == XZ_ID_Delta && f.propsSize == 1)
      s.Add_UInt32((UInt32)f.props[0] + 1);
    else
    {
      s += '[';
      for (UInt32 bi = 0; bi < f.propsSize; bi++)
        AddHexToString(s, f.props[bi]);
      s += ']';
    }
  }
}

static const char * const kChecks[] =
{
    "NoCheck"
  , "CRC32"
  , NULL
  , NULL
  , "CRC64"
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , "SHA256"
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
};

static void AddCheckString(AString &s, const CXzs &xzs)
{
  size_t i;
  UInt32 mask = 0;
  for (i = 0; i < xzs.num; i++)
    mask |= ((UInt32)1 << XzFlags_GetCheckType(xzs.streams[i].flags));
  for (i = 0; i <= XZ_CHECK_MASK; i++)
    if (((mask >> i) & 1) != 0)
    {
      s.Add_Space_if_NotEmpty();
      if (kChecks[i])
        s += kChecks[i];
      else
      {
        s += "Check-";
        s.Add_UInt32((UInt32)i);
      }
    }
}

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;

  const CXzStatInfo *stat = GetStat();
  
  switch (propID)
  {
    case kpidPhySize: if (stat) prop = stat->InSize; break;
    case kpidNumStreams: if (stat && stat->NumStreams_Defined) prop = stat->NumStreams; break;
    case kpidNumBlocks: if (stat && stat->NumBlocks_Defined) prop = stat->NumBlocks; break;
    case kpidUnpackSize: if (stat && stat->UnpackSize_Defined) prop = stat->OutSize; break;
    case kpidClusterSize: if (_stat_defined && _stat.NumBlocks_Defined && stat->NumBlocks > 1) prop = _maxBlocksSize; break;
    case kpidCharacts:
      if (_firstBlockWasRead)
      {
        AString s;
        if (XzBlock_HasPackSize(&_firstBlock))
          s.Add_OptSpaced("BlockPackSize");
        if (XzBlock_HasUnpackSize(&_firstBlock))
          s.Add_OptSpaced("BlockUnpackSize");
        if (!s.IsEmpty())
          prop = s;
      }
      break;
        

    case kpidMethod: if (!_methodsString.IsEmpty()) prop = _methodsString; break;
    case kpidErrorFlags:
    {
      UInt32 v = 0;
      SRes sres = _stat2_decode_SRes;
      if (!_isArc)                      v |= kpv_ErrorFlags_IsNotArc;
      if (sres == SZ_ERROR_INPUT_EOF)   v |= kpv_ErrorFlags_UnexpectedEnd;
      if (_stat2_defined && _stat2.DataAfterEnd) v |= kpv_ErrorFlags_DataAfterEnd;
      if (sres == SZ_ERROR_ARCHIVE)     v |= kpv_ErrorFlags_HeadersError;
      if (sres == SZ_ERROR_UNSUPPORTED) v |= kpv_ErrorFlags_UnsupportedMethod;
      if (sres == SZ_ERROR_DATA)        v |= kpv_ErrorFlags_DataError;
      if (sres == SZ_ERROR_CRC)         v |= kpv_ErrorFlags_CrcError;
      if (v != 0)
        prop = v;
      break;
    }

    case kpidMainSubfile:
    {
      // debug only, comment it:
      // if (_blocks) prop = (UInt32)0;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = 1;
  return S_OK;
}

STDMETHODIMP CHandler::GetProperty(UInt32, PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  const CXzStatInfo *stat = GetStat();
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidSize: if (stat && stat->UnpackSize_Defined) prop = stat->OutSize; break;
    case kpidPackSize: if (stat) prop = stat->InSize; break;
    case kpidMethod: if (!_methodsString.IsEmpty()) prop = _methodsString; break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


struct COpenCallbackWrap
{
  ICompressProgress vt;
  IArchiveOpenCallback *OpenCallback;
  HRESULT Res;
  
  // new clang shows "non-POD" warning for offsetof(), if we use constructor instead of Init()
  void Init(IArchiveOpenCallback *progress);
};

static SRes OpenCallbackProgress(const ICompressProgress *pp, UInt64 inSize, UInt64 /* outSize */)
{
  COpenCallbackWrap *p = CONTAINER_FROM_VTBL(pp, COpenCallbackWrap, vt);
  if (p->OpenCallback)
    p->Res = p->OpenCallback->SetCompleted(NULL, &inSize);
  return HRESULT_To_SRes(p->Res, SZ_ERROR_PROGRESS);
}

void COpenCallbackWrap::Init(IArchiveOpenCallback *callback)
{
  vt.Progress = OpenCallbackProgress;
  OpenCallback = callback;
  Res = SZ_OK;
}


struct CXzsCPP
{
  CXzs p;
  CXzsCPP() { Xzs_Construct(&p); }
  ~CXzsCPP() { Xzs_Free(&p, &g_Alloc); }
};

#define kInputBufSize ((size_t)1 << 10)

struct CLookToRead2_CPP: public CLookToRead2
{
  CLookToRead2_CPP()
  {
    buf = NULL;
    LookToRead2_CreateVTable(this,
        True // Lookahead ?
        );
  }
  void Alloc(size_t allocSize)
  {
    buf = (Byte *)MyAlloc(allocSize);
    if (buf)
      this->bufSize = allocSize;
  }
  ~CLookToRead2_CPP()
  {
    MyFree(buf);
  }
};


static HRESULT SRes_to_Open_HRESULT(SRes res)
{
  switch (res)
  {
    case SZ_OK: return S_OK;
    case SZ_ERROR_MEM: return E_OUTOFMEMORY;
    case SZ_ERROR_PROGRESS: return E_ABORT;
    /*
    case SZ_ERROR_UNSUPPORTED:
    case SZ_ERROR_CRC:
    case SZ_ERROR_DATA:
    case SZ_ERROR_ARCHIVE:
    case SZ_ERROR_NO_ARCHIVE:
      return S_FALSE;
    */
  }
  return S_FALSE;
}



HRESULT CHandler::Open2(IInStream *inStream, /* UInt32 flags, */ IArchiveOpenCallback *callback)
{
  _needSeekToStart = true;

  {
    CXzStreamFlags st;
    CSeqInStreamWrap inStreamWrap;
    
    inStreamWrap.Init(inStream);

    SRes res = Xz_ReadHeader(&st, &inStreamWrap.vt);
    
    if (inStreamWrap.Res != S_OK)
      return inStreamWrap.Res;
    if (res != SZ_OK)
      return SRes_to_Open_HRESULT(res);

    {
      CXzBlock block;
      BoolInt isIndex;
      UInt32 headerSizeRes;
    
      SRes res2 = XzBlock_ReadHeader(&block, &inStreamWrap.vt, &isIndex, &headerSizeRes);
      
      if (inStreamWrap.Res != S_OK)
        return inStreamWrap.Res;
      
      if (res2 != SZ_OK)
      {
        if (res2 == SZ_ERROR_INPUT_EOF)
        {
          _stat2_decode_SRes = res2;
          _stream = inStream;
          _seqStream = inStream;
          _isArc = true;
          return S_OK;
        }

        if (res2 == SZ_ERROR_ARCHIVE)
          return S_FALSE;
      }
      else if (!isIndex)
      {
        _firstBlockWasRead = true;
        _firstBlock = block;

        unsigned numFilters = XzBlock_GetNumFilters(&block);
        for (unsigned i = 0; i < numFilters; i++)
        {
          _methodsString.Add_Space_if_NotEmpty();
          AddMethodString(_methodsString, block.filters[i]);
        }
      }
    }
  }

  RINOK(inStream->Seek(0, STREAM_SEEK_END, &_stat.InSize));
  if (callback)
  {
    RINOK(callback->SetTotal(NULL, &_stat.InSize));
  }

  CSeekInStreamWrap inStreamImp;
  
  inStreamImp.Init(inStream);

  CLookToRead2_CPP lookStream;

  lookStream.Alloc(kInputBufSize);
  
  if (!lookStream.buf)
    return E_OUTOFMEMORY;

  lookStream.realStream = &inStreamImp.vt;
  LookToRead2_Init(&lookStream);

  COpenCallbackWrap openWrap;
  openWrap.Init(callback);

  CXzsCPP xzs;
  Int64 startPosition;
  SRes res = Xzs_ReadBackward(&xzs.p, &lookStream.vt, &startPosition, &openWrap.vt, &g_Alloc);
  if (res == SZ_ERROR_PROGRESS)
    return (openWrap.Res == S_OK) ? E_FAIL : openWrap.Res;
  /*
  if (res == SZ_ERROR_NO_ARCHIVE && xzs.p.num > 0)
    res = SZ_OK;
  */
  if (res == SZ_OK && startPosition == 0)
  {
    _stat_defined = true;

    _stat.OutSize = Xzs_GetUnpackSize(&xzs.p);
    _stat.UnpackSize_Defined = true;

    _stat.NumStreams = xzs.p.num;
    _stat.NumStreams_Defined = true;
    
    _stat.NumBlocks = Xzs_GetNumBlocks(&xzs.p);
    _stat.NumBlocks_Defined = true;

    AddCheckString(_methodsString, xzs.p);

    const size_t numBlocks = (size_t)_stat.NumBlocks + 1;
    const size_t bytesAlloc = numBlocks * sizeof(CBlockInfo);
    
    if (bytesAlloc / sizeof(CBlockInfo) == _stat.NumBlocks + 1)
    {
      _blocks = (CBlockInfo *)MyAlloc(bytesAlloc);
      if (_blocks)
      {
        unsigned blockIndex = 0;
        UInt64 unpackPos = 0;
        
        for (size_t si = xzs.p.num; si != 0;)
        {
          si--;
          const CXzStream &str = xzs.p.streams[si];
          UInt64 packPos = str.startOffset + XZ_STREAM_HEADER_SIZE;
          
          for (size_t bi = 0; bi < str.numBlocks; bi++)
          {
            const CXzBlockSizes &bs = str.blocks[bi];
            const UInt64 packSizeAligned = bs.totalSize + ((0 - (unsigned)bs.totalSize) & 3);
            
            if (bs.unpackSize != 0)
            {
              if (blockIndex >= _stat.NumBlocks)
                return E_FAIL;

              CBlockInfo &block = _blocks[blockIndex++];
              block.StreamFlags = str.flags;
              block.PackSize = bs.totalSize; // packSizeAligned;
              block.PackPos = packPos;
              block.UnpackPos = unpackPos;
            }
            packPos += packSizeAligned;
            unpackPos += bs.unpackSize;
            if (_maxBlocksSize < bs.unpackSize)
              _maxBlocksSize = bs.unpackSize;
          }
        }
    
        /*
        if (blockIndex != _stat.NumBlocks)
        {
          // there are Empty blocks;
        }
        */
        if (_stat.OutSize != unpackPos)
          return E_FAIL;
        CBlockInfo &block = _blocks[blockIndex++];
        block.StreamFlags = 0;
        block.PackSize = 0;
        block.PackPos = 0;
        block.UnpackPos = unpackPos;
        _blocksArraySize = blockIndex;
      }
    }
  }
  else
  {
    res = SZ_OK;
  }

  RINOK(SRes_to_Open_HRESULT(res));

  _stream = inStream;
  _seqStream = inStream;
  _isArc = true;
  return S_OK;
}



STDMETHODIMP CHandler::Open(IInStream *inStream, const UInt64 *, IArchiveOpenCallback *callback)
{
  COM_TRY_BEGIN
  {
    Close();
    return Open2(inStream, callback);
  }
  COM_TRY_END
}

STDMETHODIMP CHandler::OpenSeq(ISequentialInStream *stream)
{
  Close();
  _seqStream = stream;
  _isArc = true;
  _needSeekToStart = false;
  return S_OK;
}

STDMETHODIMP CHandler::Close()
{
  XzStatInfo_Clear(&_stat);
  XzStatInfo_Clear(&_stat2);
  _stat_defined = false;
  _stat2_defined = false;
  _stat2_decode_SRes = SZ_OK;

  _isArc = false;
  _needSeekToStart = false;
  _firstBlockWasRead = false;

   _methodsString.Empty();
  _stream.Release();
  _seqStream.Release();

  MyFree(_blocks);
  _blocks = NULL;
  _blocksArraySize = 0;
  _maxBlocksSize = 0;

  return S_OK;
}


struct CXzUnpackerCPP2
{
  Byte *InBuf;
  // Byte *OutBuf;
  CXzUnpacker p;
  
  CXzUnpackerCPP2();
  ~CXzUnpackerCPP2();
};

CXzUnpackerCPP2::CXzUnpackerCPP2(): InBuf(NULL)
  // , OutBuf(NULL)
{
  XzUnpacker_Construct(&p, &g_Alloc);
}

CXzUnpackerCPP2::~CXzUnpackerCPP2()
{
  XzUnpacker_Free(&p);
  MidFree(InBuf);
  // MidFree(OutBuf);
}


class CInStream:
  public IInStream,
  public CMyUnknownImp
{
public:
  UInt64 _virtPos;
  UInt64 Size;
  UInt64 _cacheStartPos;
  size_t _cacheSize;
  CByteBuffer _cache;
  // UInt64 _startPos;
  CXzUnpackerCPP2 xz;

  void InitAndSeek()
  {
    _virtPos = 0;
    _cacheStartPos = 0;
    _cacheSize = 0;
    // _startPos = startPos;
  }

  CHandler *_handlerSpec;
  CMyComPtr<IUnknown> _handler;

  MY_UNKNOWN_IMP1(IInStream)

  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);

  ~CInStream();
};


CInStream::~CInStream()
{
  // _cache.Free();
}


static size_t FindBlock(const CBlockInfo *blocks, size_t numBlocks, UInt64 pos)
{
  size_t left = 0, right = numBlocks;
  for (;;)
  {
    size_t mid = (left + right) / 2;
    if (mid == left)
      return left;
    if (pos < blocks[mid].UnpackPos)
      right = mid;
    else
      left = mid;
  }
}



static HRESULT DecodeBlock(CXzUnpackerCPP2 &xzu,
    ISequentialInStream *seqInStream,
    unsigned streamFlags,
    UInt64 packSize, // pure size from Index record, it doesn't include pad zeros
    size_t unpackSize, Byte *dest
    // , ICompressProgressInfo *progress
    )
{
  const size_t kInBufSize = (size_t)1 << 16;

  XzUnpacker_Init(&xzu.p);

  if (!xzu.InBuf)
  {
    xzu.InBuf = (Byte *)MidAlloc(kInBufSize);
    if (!xzu.InBuf)
      return E_OUTOFMEMORY;
  }
  
  xzu.p.streamFlags = (UInt16)streamFlags;
  XzUnpacker_PrepareToRandomBlockDecoding(&xzu.p);

  XzUnpacker_SetOutBuf(&xzu.p, dest, unpackSize);

  const UInt64 packSizeAligned = packSize + ((0 - (unsigned)packSize) & 3);
  UInt64 packRem = packSizeAligned;

  UInt32 inSize = 0;
  SizeT inPos = 0;
  SizeT outPos = 0;

  HRESULT readRes = S_OK;

  for (;;)
  {
    if (inPos == inSize && readRes == S_OK)
    {
      inPos = 0;
      inSize = 0;
      UInt32 rem = kInBufSize;
      if (rem > packRem)
        rem = (UInt32)packRem;
      if (rem != 0)
        readRes = seqInStream->Read(xzu.InBuf, rem, &inSize);
    }

    SizeT inLen = inSize - inPos;
    SizeT outLen = unpackSize - outPos;
    
    ECoderStatus status;

    SRes res = XzUnpacker_Code(&xzu.p,
        // dest + outPos,
        NULL,
        &outLen,
        xzu.InBuf + inPos, &inLen,
        (inLen == 0), // srcFinished
        CODER_FINISH_END, &status);

    // return E_OUTOFMEMORY;
    // res = SZ_ERROR_CRC;

    if (res != SZ_OK)
    {
      if (res == SZ_ERROR_CRC)
        return S_FALSE;
      return SResToHRESULT(res);
    }

    inPos += inLen;
    outPos += outLen;

    packRem -= inLen;
  
    BoolInt blockFinished = XzUnpacker_IsBlockFinished(&xzu.p);

    if ((inLen == 0 && outLen == 0) || blockFinished)
    {
      if (packRem != 0 || !blockFinished || unpackSize != outPos)
        return S_FALSE;
      if (XzUnpacker_GetPackSizeForIndex(&xzu.p) != packSize)
        return S_FALSE;
      return S_OK;
    }
  }
}


STDMETHODIMP CInStream::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  COM_TRY_BEGIN

  if (processedSize)
    *processedSize = 0;
  if (size == 0)
    return S_OK;

  {
    if (_virtPos >= Size)
      return S_OK; // (Size == _virtPos) ? S_OK: E_FAIL;
    {
      UInt64 rem = Size - _virtPos;
      if (size > rem)
        size = (UInt32)rem;
    }
  }

  if (size == 0)
    return S_OK;

  if (_virtPos < _cacheStartPos || _virtPos >= _cacheStartPos + _cacheSize)
  {
    size_t bi = FindBlock(_handlerSpec->_blocks, _handlerSpec->_blocksArraySize, _virtPos);
    const CBlockInfo &block = _handlerSpec->_blocks[bi];
    const UInt64 unpackSize = _handlerSpec->_blocks[bi + 1].UnpackPos - block.UnpackPos;
    if (_cache.Size() < unpackSize)
      return E_FAIL;

    _cacheSize = 0;

    RINOK(_handlerSpec->SeekToPackPos(block.PackPos));
    RINOK(DecodeBlock(xz, _handlerSpec->_seqStream, block.StreamFlags, block.PackSize,
        (size_t)unpackSize, _cache));
    _cacheStartPos = block.UnpackPos;
    _cacheSize = (size_t)unpackSize;
  }

  {
    size_t offset = (size_t)(_virtPos - _cacheStartPos);
    size_t rem = _cacheSize - offset;
    if (size > rem)
      size = (UInt32)rem;
    memcpy(data, _cache + offset, size);
    _virtPos += size;
    if (processedSize)
      *processedSize = size;
    return S_OK;
  }

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
  _virtPos = (UInt64)offset;
  if (newPosition)
    *newPosition = (UInt64)offset;
  return S_OK;
}



static const UInt64 kMaxBlockSize_for_GetStream = (UInt64)1 << 40;

STDMETHODIMP CHandler::GetStream(UInt32 index, ISequentialInStream **stream)
{
  COM_TRY_BEGIN

  *stream = NULL;

  if (index != 0)
    return E_INVALIDARG;

  if (!_stat.UnpackSize_Defined
      || _maxBlocksSize == 0 // 18.02
      || _maxBlocksSize > kMaxBlockSize_for_GetStream
      || _maxBlocksSize != (size_t)_maxBlocksSize)
    return S_FALSE;

  UInt64 memSize;
  if (!NSystem::GetRamSize(memSize))
    memSize = (UInt64)(sizeof(size_t)) << 28;
  {
    if (_maxBlocksSize > memSize / 4)
      return S_FALSE;
  }

  CInStream *spec = new CInStream;
  CMyComPtr<ISequentialInStream> specStream = spec;
  spec->_cache.Alloc((size_t)_maxBlocksSize);
  spec->_handlerSpec = this;
  spec->_handler = (IInArchive *)this;
  spec->Size = _stat.OutSize;
  spec->InitAndSeek();

  *stream = specStream.Detach();
  return S_OK;
  
  COM_TRY_END
}


static Int32 Get_Extract_OperationResult(const NCompress::NXz::CDecoder &decoder)
{
  Int32 opRes;
  SRes sres = decoder.MainDecodeSRes;
  if (sres == SZ_ERROR_NO_ARCHIVE) // (!IsArc)
    opRes = NExtract::NOperationResult::kIsNotArc;
  else if (sres == SZ_ERROR_INPUT_EOF) // (UnexpectedEnd)
    opRes = NExtract::NOperationResult::kUnexpectedEnd;
  else if (decoder.Stat.DataAfterEnd)
    opRes = NExtract::NOperationResult::kDataAfterEnd;
  else if (sres == SZ_ERROR_CRC) // (CrcError)
    opRes = NExtract::NOperationResult::kCRCError;
  else if (sres == SZ_ERROR_UNSUPPORTED) // (Unsupported)
    opRes = NExtract::NOperationResult::kUnsupportedMethod;
  else if (sres == SZ_ERROR_ARCHIVE) //  (HeadersError)
    opRes = NExtract::NOperationResult::kDataError;
  else if (sres == SZ_ERROR_DATA)  // (DataError)
    opRes = NExtract::NOperationResult::kDataError;
  else if (sres != SZ_OK)
    opRes = NExtract::NOperationResult::kDataError;
  else
    opRes = NExtract::NOperationResult::kOK;
  return opRes;
}




STDMETHODIMP CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback)
{
  COM_TRY_BEGIN
  if (numItems == 0)
    return S_OK;
  if (numItems != (UInt32)(Int32)-1 && (numItems != 1 || indices[0] != 0))
    return E_INVALIDARG;

  const CXzStatInfo *stat = GetStat();

  if (stat)
    extractCallback->SetTotal(stat->InSize);

  UInt64 currentTotalPacked = 0;
  RINOK(extractCallback->SetCompleted(&currentTotalPacked));
  CMyComPtr<ISequentialOutStream> realOutStream;
  Int32 askMode = testMode ?
      NExtract::NAskMode::kTest :
      NExtract::NAskMode::kExtract;
  
  RINOK(extractCallback->GetStream(0, &realOutStream, askMode));
  
  if (!testMode && !realOutStream)
    return S_OK;

  extractCallback->PrepareOperation(askMode);

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> lpsRef = lps;
  lps->Init(extractCallback, true);

  if (_needSeekToStart)
  {
    if (!_stream)
      return E_FAIL;
    RINOK(_stream->Seek(0, STREAM_SEEK_SET, NULL));
  }
  else
    _needSeekToStart = true;


  NCompress::NXz::CDecoder decoder;

  HRESULT hres = Decode(decoder, _seqStream, realOutStream, lpsRef);

  if (!decoder.MainDecodeSRes_wasUsed)
    return hres == S_OK ? E_FAIL : hres;

  Int32 opRes = Get_Extract_OperationResult(decoder);
  if (opRes == NExtract::NOperationResult::kOK
      && hres != S_OK)
    opRes = NExtract::NOperationResult::kDataError;

  realOutStream.Release();
  return extractCallback->SetOperationResult(opRes);
  COM_TRY_END
}



#ifndef EXTRACT_ONLY

STDMETHODIMP CHandler::GetFileTimeType(UInt32 *timeType)
{
  *timeType = NFileTimeType::kUnix;
  return S_OK;
}


STDMETHODIMP CHandler::UpdateItems(ISequentialOutStream *outStream, UInt32 numItems,
    IArchiveUpdateCallback *updateCallback)
{
  COM_TRY_BEGIN

  if (numItems == 0)
  {
    CSeqOutStreamWrap seqOutStream;
    seqOutStream.Init(outStream);
    SRes res = Xz_EncodeEmpty(&seqOutStream.vt);
    return SResToHRESULT(res);
  }
  
  if (numItems != 1)
    return E_INVALIDARG;

  Int32 newData, newProps;
  UInt32 indexInArchive;
  if (!updateCallback)
    return E_FAIL;
  RINOK(updateCallback->GetUpdateItemInfo(0, &newData, &newProps, &indexInArchive));

  if (IntToBool(newProps))
  {
    {
      NCOM::CPropVariant prop;
      RINOK(updateCallback->GetProperty(0, kpidIsDir, &prop));
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
      RINOK(updateCallback->GetProperty(0, kpidSize, &prop));
      if (prop.vt != VT_UI8)
        return E_INVALIDARG;
      size = prop.uhVal.QuadPart;
      RINOK(updateCallback->SetTotal(size));
    }

    NCompress::NXz::CEncoder *encoderSpec = new NCompress::NXz::CEncoder;
    CMyComPtr<ICompressCoder> encoder = encoderSpec;

    CXzProps &xzProps = encoderSpec->xzProps;
    CLzma2EncProps &lzma2Props = xzProps.lzma2Props;

    lzma2Props.lzmaProps.level = GetLevel();

    xzProps.reduceSize = size;
    /*
    {
      NCOM::CPropVariant prop = (UInt64)size;
      RINOK(encoderSpec->SetCoderProp(NCoderPropID::kReduceSize, prop));
    }
    */

    #ifndef _7ZIP_ST
    xzProps.numTotalThreads = (int)_numThreads;
    #endif

    xzProps.blockSize = _numSolidBytes;
    if (_numSolidBytes == XZ_PROPS__BLOCK_SIZE__SOLID)
    {
      xzProps.lzma2Props.blockSize = LZMA2_ENC_PROPS__BLOCK_SIZE__SOLID;
    }

    RINOK(encoderSpec->SetCheckSize(_crcSize));

    {
      CXzFilterProps &filter = xzProps.filterProps;
      
      if (_filterId == XZ_ID_Delta)
      {
        bool deltaDefined = false;
        FOR_VECTOR (j, _filterMethod.Props)
        {
          const CProp &prop = _filterMethod.Props[j];
          if (prop.Id == NCoderPropID::kDefaultProp && prop.Value.vt == VT_UI4)
          {
            UInt32 delta = (UInt32)prop.Value.ulVal;
            if (delta < 1 || delta > 256)
              return E_INVALIDARG;
            filter.delta = delta;
            deltaDefined = true;
          }
          else
            return E_INVALIDARG;
        }
        if (!deltaDefined)
          return E_INVALIDARG;
      }
      filter.id = _filterId;
    }

    FOR_VECTOR (i, _methods)
    {
      COneMethodInfo &m = _methods[i];

      FOR_VECTOR (j, m.Props)
      {
        const CProp &prop = m.Props[j];
        RINOK(encoderSpec->SetCoderProp(prop.Id, prop.Value));
      }
    }

    CMyComPtr<ISequentialInStream> fileInStream;
    RINOK(updateCallback->GetStream(0, &fileInStream));

    CLocalProgress *lps = new CLocalProgress;
    CMyComPtr<ICompressProgressInfo> progress = lps;
    lps->Init(updateCallback, true);

    RINOK(encoderSpec->Code(fileInStream, outStream, NULL, NULL, progress));

    return updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK);
  }

  if (indexInArchive != 0)
    return E_INVALIDARG;

  CMyComPtr<IArchiveUpdateCallbackFile> opCallback;
  updateCallback->QueryInterface(IID_IArchiveUpdateCallbackFile, (void **)&opCallback);
  if (opCallback)
  {
    RINOK(opCallback->ReportOperation(NEventIndexType::kInArcIndex, 0, NUpdateNotifyOp::kReplicate))
  }

  if (_stream)
  {
    const CXzStatInfo *stat = GetStat();
    if (stat)
      RINOK(updateCallback->SetTotal(stat->InSize));
    RINOK(_stream->Seek(0, STREAM_SEEK_SET, NULL));
  }

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(updateCallback, true);

  return NCompress::CopyStream(_stream, outStream, progress);

  COM_TRY_END
}

#endif


HRESULT CHandler::SetProperty(const wchar_t *nameSpec, const PROPVARIANT &value)
{
  UString name = nameSpec;
  name.MakeLower_Ascii();
  if (name.IsEmpty())
    return E_INVALIDARG;
  
  #ifndef EXTRACT_ONLY

  if (name[0] == L's')
  {
    const wchar_t *s = name.Ptr(1);
    if (*s == 0)
    {
      bool useStr = false;
      bool isSolid;
      switch (value.vt)
      {
        case VT_EMPTY: isSolid = true; break;
        case VT_BOOL: isSolid = (value.boolVal != VARIANT_FALSE); break;
        case VT_BSTR:
          if (!StringToBool(value.bstrVal, isSolid))
            useStr = true;
          break;
        default: return E_INVALIDARG;
      }
      if (!useStr)
      {
        _numSolidBytes = (isSolid ? XZ_PROPS__BLOCK_SIZE__SOLID : XZ_PROPS__BLOCK_SIZE__AUTO);
        return S_OK;
      }
    }
    return ParseSizeString(s, value,
        0, // percentsBase
        _numSolidBytes) ? S_OK: E_INVALIDARG;
  }

  return CMultiMethodProps::SetProperty(name, value);

  #else

  {
    HRESULT hres;
    if (SetCommonProperty(name, value, hres))
      return hres;
  }

  return E_INVALIDARG;
  
  #endif
}



STDMETHODIMP CHandler::SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps)
{
  COM_TRY_BEGIN

  Init();

  for (UInt32 i = 0; i < numProps; i++)
  {
    RINOK(SetProperty(names[i], values[i]));
  }

  #ifndef EXTRACT_ONLY

  if (!_filterMethod.MethodName.IsEmpty())
  {
    unsigned k;
    for (k = 0; k < ARRAY_SIZE(g_NamePairs); k++)
    {
      const CMethodNamePair &pair = g_NamePairs[k];
      if (StringsAreEqualNoCase_Ascii(_filterMethod.MethodName, pair.Name))
      {
        _filterId = pair.Id;
        break;
      }
    }
    if (k == ARRAY_SIZE(g_NamePairs))
      return E_INVALIDARG;
  }

  _methods.DeleteFrontal(GetNumEmptyMethods());
  if (_methods.Size() > 1)
    return E_INVALIDARG;
  if (_methods.Size() == 1)
  {
    AString &methodName = _methods[0].MethodName;
    if (methodName.IsEmpty())
      methodName = k_LZMA2_Name;
    else if (
        !methodName.IsEqualTo_Ascii_NoCase(k_LZMA2_Name)
        && !methodName.IsEqualTo_Ascii_NoCase("xz"))
      return E_INVALIDARG;
  }
  
  #endif

  return S_OK;

  COM_TRY_END
}


REGISTER_ARC_IO(
  "xz", "xz txz", "* .tar", 0xC,
  XZ_SIG,
  0,
  NArcInfoFlags::kKeepName,
  NULL)

}}

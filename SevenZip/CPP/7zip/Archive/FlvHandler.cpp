// FlvHandler.cpp

#include "StdAfx.h"

// #include <stdio.h>

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/MyBuffer.h"
#include "../../Common/MyString.h"

#include "../../Windows/PropVariant.h"

#include "../Common/InBuffer.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamObjects.h"
#include "../Common/StreamUtils.h"

#define GetBe24(p) ( \
    ((UInt32)((const Byte *)(p))[0] << 16) | \
    ((UInt32)((const Byte *)(p))[1] <<  8) | \
             ((const Byte *)(p))[2] )

// #define Get16(p) GetBe16(p)
#define Get24(p) GetBe24(p)
#define Get32(p) GetBe32(p)

namespace NArchive {
namespace NFlv {

// static const UInt32 kFileSizeMax = (UInt32)1 << 30;
static const UInt32 kNumChunksMax = (UInt32)1 << 23;

static const UInt32 kTagHeaderSize = 11;

static const Byte kFlag_Video = 1;
static const Byte kFlag_Audio = 4;

static const Byte kType_Audio = 8;
static const Byte kType_Video = 9;
static const Byte kType_Meta = 18;
static const unsigned kNumTypes = 19;

struct CItem
{
  CByteBuffer Data;
  Byte Type;
};

struct CItem2
{
  Byte Type;
  Byte SubType;
  Byte Props;
  bool SameSubTypes;
  unsigned NumChunks;
  size_t Size;

  CReferenceBuf *BufSpec;
  CMyComPtr<IUnknown> RefBuf;

  bool IsAudio() const { return Type == kType_Audio; }
};

class CHandler:
  public IInArchive,
  public IInArchiveGetStream,
  public CMyUnknownImp
{
  CMyComPtr<IInStream> _stream;
  CObjectVector<CItem2> _items2;
  CByteBuffer _metadata;
  bool _isRaw;
  UInt64 _phySize;

  HRESULT Open2(IInStream *stream, IArchiveOpenCallback *callback);
  // AString GetComment();
public:
  MY_UNKNOWN_IMP2(IInArchive, IInArchiveGetStream)
  INTERFACE_IInArchive(;)
  STDMETHOD(GetStream)(UInt32 index, ISequentialInStream **stream);
};

static const Byte kProps[] =
{
  kpidSize,
  kpidNumBlocks,
  kpidComment
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps_NO_Table

static const char * const g_AudioTypes[16] =
{
    "pcm"
  , "adpcm"
  , "mp3"
  , "pcm_le"
  , "nellymoser16"
  , "nellymoser8"
  , "nellymoser"
  , "g711a"
  , "g711m"
  , "audio9"
  , "aac"
  , "speex"
  , "audio12"
  , "audio13"
  , "mp3"
  , "audio15"
};

static const char * const g_VideoTypes[16] =
{
    "video0"
  , "jpeg"
  , "h263"
  , "screen"
  , "vp6"
  , "vp6alpha"
  , "screen2"
  , "avc"
  , "video8"
  , "video9"
  , "video10"
  , "video11"
  , "video12"
  , "video13"
  , "video14"
  , "video15"
};

static const char * const g_Rates[4] =
{
    "5.5 kHz"
  , "11 kHz"
  , "22 kHz"
  , "44 kHz"
};

STDMETHODIMP CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
{
  NWindows::NCOM::CPropVariant prop;
  const CItem2 &item = _items2[index];
  switch (propID)
  {
    case kpidExtension:
      prop = _isRaw ?
        (item.IsAudio() ? g_AudioTypes[item.SubType] : g_VideoTypes[item.SubType]) :
        (item.IsAudio() ? "audio.flv" : "video.flv");
      break;
    case kpidSize:
    case kpidPackSize:
      prop = (UInt64)item.Size;
      break;
    case kpidNumBlocks: prop = (UInt32)item.NumChunks; break;
    case kpidComment:
    {
      char sz[64];
      char *s = MyStpCpy(sz, (item.IsAudio() ? g_AudioTypes[item.SubType] : g_VideoTypes[item.SubType]) );
      if (item.IsAudio())
      {
        *s++ = ' ';
        s = MyStpCpy(s, g_Rates[(item.Props >> 2) & 3]);
        s = MyStpCpy(s, (item.Props & 2) ? " 16-bit" : " 8-bit");
        s = MyStpCpy(s, (item.Props & 1) ? " stereo" : " mono");
      }
      prop = sz;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
}

/*
AString CHandler::GetComment()
{
  const Byte *p = _metadata;
  size_t size = _metadata.Size();
  AString res;
  if (size > 0)
  {
    p++;
    size--;
    for (;;)
    {
      if (size < 2)
        break;
      int len = Get16(p);
      p += 2;
      size -= 2;
      if (len == 0 || (size_t)len > size)
        break;
      {
        AString temp;
        temp.SetFrom_CalcLen((const char *)p, len);
        if (!res.IsEmpty())
          res += '\n';
        res += temp;
      }
      p += len;
      size -= len;
      if (size < 1)
        break;
      Byte type = *p++;
      size--;
      bool ok = false;
      switch (type)
      {
        case 0:
        {
          if (size < 8)
            break;
          ok = true;
          Byte reverse[8];
          for (int i = 0; i < 8; i++)
          {
            bool little_endian = 1;
            if (little_endian)
              reverse[i] = p[7 - i];
            else
              reverse[i] = p[i];
          }
          double d = *(double *)reverse;
          char temp[32];
          sprintf(temp, " = %.3f", d);
          res += temp;
          p += 8;
          size -= 8;
          break;
        }
        case 8:
        {
          if (size < 4)
            break;
          ok = true;
          // UInt32 numItems = Get32(p);
          p += 4;
          size -= 4;
          break;
        }
      }
      if (!ok)
        break;
    }
  }
  return res;
}
*/

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  // COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    // case kpidComment: prop = GetComment(); break;
    case kpidPhySize: prop = (UInt64)_phySize; break;
    case kpidIsNotArcType: prop = true; break;
  }
  prop.Detach(value);
  return S_OK;
  // COM_TRY_END
}

HRESULT CHandler::Open2(IInStream *stream, IArchiveOpenCallback *callback)
{
  const UInt32 kHeaderSize = 13;
  Byte header[kHeaderSize];
  RINOK(ReadStream_FALSE(stream, header, kHeaderSize));
  if (header[0] != 'F' ||
      header[1] != 'L' ||
      header[2] != 'V' ||
      header[3] != 1 ||
      (header[4] & 0xFA) != 0)
    return S_FALSE;
  UInt64 offset = Get32(header + 5);
  if (offset != 9 || Get32(header + 9) != 0)
    return S_FALSE;
  offset = kHeaderSize;
 
  CInBuffer inBuf;
  if (!inBuf.Create(1 << 15))
    return E_OUTOFMEMORY;
  inBuf.SetStream(stream);

  CObjectVector<CItem> items;
  int lasts[kNumTypes];
  unsigned i;
  for (i = 0; i < kNumTypes; i++)
    lasts[i] = -1;

  _phySize = offset;
  for (;;)
  {
    Byte buf[kTagHeaderSize];
    CItem item;
    if (inBuf.ReadBytes(buf, kTagHeaderSize) != kTagHeaderSize)
      break;
    item.Type = buf[0];
    UInt32 size = Get24(buf + 1);
    if (size < 1)
      break;
    // item.Time = Get24(buf + 4);
    // item.Time |= (UInt32)buf[7] << 24;
    if (Get24(buf + 8) != 0) // streamID
      break;

    UInt32 curSize = kTagHeaderSize + size + 4;
    item.Data.Alloc(curSize);
    memcpy(item.Data, buf, kTagHeaderSize);
    if (inBuf.ReadBytes(item.Data + kTagHeaderSize, size) != size)
      break;
    if (inBuf.ReadBytes(item.Data + kTagHeaderSize + size, 4) != 4)
      break;

    if (Get32(item.Data + kTagHeaderSize + size) != kTagHeaderSize + size)
      break;

    offset += curSize;
    
    // printf("\noffset = %6X type = %2d time = %6d size = %6d", (UInt32)offset, item.Type, item.Time, item.Size);

    if (item.Type == kType_Meta)
    {
      // _metadata = item.Buf;
    }
    else
    {
      if (item.Type != kType_Audio && item.Type != kType_Video)
        break;
      if (items.Size() >= kNumChunksMax)
        return S_FALSE;
      Byte firstByte = item.Data[kTagHeaderSize];
      Byte subType, props;
      if (item.Type == kType_Audio)
      {
        subType = (Byte)(firstByte >> 4);
        props = (Byte)(firstByte & 0xF);
      }
      else
      {
        subType = (Byte)(firstByte & 0xF);
        props = (Byte)(firstByte >> 4);
      }
      int last = lasts[item.Type];
      if (last < 0)
      {
        CItem2 item2;
        item2.RefBuf = item2.BufSpec = new CReferenceBuf;
        item2.Size = curSize;
        item2.Type = item.Type;
        item2.SubType = subType;
        item2.Props = props;
        item2.NumChunks = 1;
        item2.SameSubTypes = true;
        lasts[item.Type] = _items2.Add(item2);
      }
      else
      {
        CItem2 &item2 = _items2[last];
        if (subType != item2.SubType)
          item2.SameSubTypes = false;
        item2.Size += curSize;
        item2.NumChunks++;
      }
      items.Add(item);
    }
    _phySize = offset;
    if (callback && (items.Size() & 0xFF) == 0)
    {
      RINOK(callback->SetCompleted(NULL, &offset))
    }
  }
  if (items.IsEmpty())
    return S_FALSE;

  _isRaw = (_items2.Size() == 1);
  for (i = 0; i < _items2.Size(); i++)
  {
    CItem2 &item2 = _items2[i];
    CByteBuffer &itemBuf = item2.BufSpec->Buf;
    if (_isRaw)
    {
      if (!item2.SameSubTypes)
        return S_FALSE;
      itemBuf.Alloc((size_t)item2.Size - (size_t)(kTagHeaderSize + 4 + 1) * item2.NumChunks);
      item2.Size = 0;
    }
    else
    {
      itemBuf.Alloc(kHeaderSize + (size_t)item2.Size);
      memcpy(itemBuf, header, kHeaderSize);
      itemBuf[4] = item2.IsAudio() ? kFlag_Audio : kFlag_Video;
      item2.Size = kHeaderSize;
    }
  }

  for (i = 0; i < items.Size(); i++)
  {
    const CItem &item = items[i];
    CItem2 &item2 = _items2[lasts[item.Type]];
    size_t size = item.Data.Size();
    const Byte *src = item.Data;
    if (_isRaw)
    {
      src += kTagHeaderSize + 1;
      size -= (kTagHeaderSize + 4 + 1);
    }
    if (size != 0)
    {
      memcpy(item2.BufSpec->Buf + item2.Size, src, size);
      item2.Size += size;
    }
  }
  return S_OK;
}

STDMETHODIMP CHandler::Open(IInStream *inStream, const UInt64 *, IArchiveOpenCallback *callback)
{
  COM_TRY_BEGIN
  Close();
  HRESULT res;
  try
  {
    res = Open2(inStream, callback);
    if (res == S_OK)
      _stream = inStream;
  }
  catch(...) { res = S_FALSE; }
  if (res != S_OK)
  {
    Close();
    return S_FALSE;
  }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::Close()
{
  _phySize = 0;
  _stream.Release();
  _items2.Clear();
  // _metadata.SetCapacity(0);
  return S_OK;
}

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = _items2.Size();
  return S_OK;
}

STDMETHODIMP CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback)
{
  COM_TRY_BEGIN
  bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _items2.Size();
  if (numItems == 0)
    return S_OK;
  UInt64 totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
    totalSize += _items2[allFilesMode ? i : indices[i]].Size;
  extractCallback->SetTotal(totalSize);

  totalSize = 0;
  
  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  for (i = 0; i < numItems; i++)
  {
    lps->InSize = lps->OutSize = totalSize;
    RINOK(lps->SetCur());
    CMyComPtr<ISequentialOutStream> outStream;
    Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    UInt32 index = allFilesMode ? i : indices[i];
    const CItem2 &item = _items2[index];
    RINOK(extractCallback->GetStream(index, &outStream, askMode));
    totalSize += item.Size;
    if (!testMode && !outStream)
      continue;
    RINOK(extractCallback->PrepareOperation(askMode));
    if (outStream)
    {
      RINOK(WriteStream(outStream, item.BufSpec->Buf, item.BufSpec->Buf.Size()));
    }
    RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK));
  }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::GetStream(UInt32 index, ISequentialInStream **stream)
{
  COM_TRY_BEGIN
  *stream = 0;
  CBufInStream *streamSpec = new CBufInStream;
  CMyComPtr<ISequentialInStream> streamTemp = streamSpec;
  streamSpec->Init(_items2[index].BufSpec);
  *stream = streamTemp.Detach();
  return S_OK;
  COM_TRY_END
}

static const Byte k_Signature[] = { 'F', 'L', 'V', 1, };

REGISTER_ARC_I(
  "FLV", "flv", 0, 0xD6,
  k_Signature,
  0,
  0,
  NULL)

}}

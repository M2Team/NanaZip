// SplitHandler.cpp

#include "StdAfx.h"

#include "../../Common/ComTry.h"
#include "../../Common/MyString.h"

#include "../../Windows/PropVariant.h"

#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"

#include "../Compress/CopyCoder.h"

#include "Common/MultiStream.h"

using namespace NWindows;

namespace NArchive {
namespace NSplit {

static const Byte kProps[] =
{
  kpidPath,
  kpidSize
};

static const Byte kArcProps[] =
{
  kpidNumVolumes,
  kpidTotalPhySize
};

class CHandler:
  public IInArchive,
  public IInArchiveGetStream,
  public CMyUnknownImp
{
  CObjectVector<CMyComPtr<IInStream> > _streams;
  CRecordVector<UInt64> _sizes;
  UString _subName;
  UInt64 _totalSize;

  HRESULT Open2(IInStream *stream, IArchiveOpenCallback *callback);
public:
  MY_UNKNOWN_IMP2(IInArchive, IInArchiveGetStream)
  INTERFACE_IInArchive(;)
  STDMETHOD(GetStream)(UInt32 index, ISequentialInStream **stream);
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidMainSubfile: prop = (UInt32)0; break;
    case kpidPhySize: if (!_sizes.IsEmpty()) prop = _sizes[0]; break;
    case kpidTotalPhySize: prop = _totalSize; break;
    case kpidNumVolumes: prop = (UInt32)_streams.Size(); break;
  }
  prop.Detach(value);
  return S_OK;
}

struct CSeqName
{
  UString _unchangedPart;
  UString _changedPart;
  bool _splitStyle;
  
  bool GetNextName(UString &s)
  {
    {
      unsigned i = _changedPart.Len();
      for (;;)
      {
        wchar_t c = _changedPart[--i];
        
        if (_splitStyle)
        {
          if (c == 'z')
          {
            _changedPart.ReplaceOneCharAtPos(i, L'a');
            if (i == 0)
              return false;
            continue;
          }
          else if (c == 'Z')
          {
            _changedPart.ReplaceOneCharAtPos(i, L'A');
            if (i == 0)
              return false;
            continue;
          }
        }
        else
        {
          if (c == '9')
          {
            _changedPart.ReplaceOneCharAtPos(i, L'0');
            if (i == 0)
            {
              _changedPart.InsertAtFront(L'1');
              break;
            }
            continue;
          }
        }

        c++;
        _changedPart.ReplaceOneCharAtPos(i, c);
        break;
      }
    }
    
    s = _unchangedPart + _changedPart;
    return true;
  }
};

HRESULT CHandler::Open2(IInStream *stream, IArchiveOpenCallback *callback)
{
  Close();
  if (!callback)
    return S_FALSE;

  CMyComPtr<IArchiveOpenVolumeCallback> volumeCallback;
  callback->QueryInterface(IID_IArchiveOpenVolumeCallback, (void **)&volumeCallback);
  if (!volumeCallback)
    return S_FALSE;
  
  UString name;
  {
    NCOM::CPropVariant prop;
    RINOK(volumeCallback->GetProperty(kpidName, &prop));
    if (prop.vt != VT_BSTR)
      return S_FALSE;
    name = prop.bstrVal;
  }
  
  int dotPos = name.ReverseFind_Dot();
  const UString prefix = name.Left((unsigned)(dotPos + 1));
  const UString ext = name.Ptr((unsigned)(dotPos + 1));
  UString ext2 = ext;
  ext2.MakeLower_Ascii();
  
  CSeqName seqName;
  
  unsigned numLetters = 2;
  bool splitStyle = false;
  
  if (ext2.Len() >= 2 && StringsAreEqual_Ascii(ext2.RightPtr(2), "aa"))
  {
    splitStyle = true;
    while (numLetters < ext2.Len())
    {
      if (ext2[ext2.Len() - numLetters - 1] != 'a')
        break;
      numLetters++;
    }
  }
  else if (ext.Len() >= 2 && StringsAreEqual_Ascii(ext2.RightPtr(2), "01"))
  {
    while (numLetters < ext2.Len())
    {
      if (ext2[ext2.Len() - numLetters - 1] != '0')
        break;
      numLetters++;
    }
    if (numLetters != ext.Len())
      return S_FALSE;
  }
  else
    return S_FALSE;
  
  seqName._unchangedPart = prefix + ext.Left(ext2.Len() - numLetters);
  seqName._changedPart = ext.RightPtr(numLetters);
  seqName._splitStyle = splitStyle;
  
  if (prefix.Len() < 1)
    _subName = "file";
  else
    _subName.SetFrom(prefix, prefix.Len() - 1);
  
  UInt64 size;
  {
    /*
    NCOM::CPropVariant prop;
    RINOK(volumeCallback->GetProperty(kpidSize, &prop));
    if (prop.vt != VT_UI8)
      return E_INVALIDARG;
    size = prop.uhVal.QuadPart;
    */
    RINOK(stream->Seek(0, STREAM_SEEK_END, &size));
    RINOK(stream->Seek(0, STREAM_SEEK_SET, NULL));
  }
  
  _totalSize += size;
  _sizes.Add(size);
  _streams.Add(stream);
  
  {
    const UInt64 numFiles = _streams.Size();
    RINOK(callback->SetCompleted(&numFiles, NULL));
  }
  
  for (;;)
  {
    UString fullName;
    if (!seqName.GetNextName(fullName))
      break;
    CMyComPtr<IInStream> nextStream;
    HRESULT result = volumeCallback->GetStream(fullName, &nextStream);
    if (result == S_FALSE)
      break;
    if (result != S_OK)
      return result;
    if (!nextStream)
      break;
    {
      /*
      NCOM::CPropVariant prop;
      RINOK(volumeCallback->GetProperty(kpidSize, &prop));
      if (prop.vt != VT_UI8)
        return E_INVALIDARG;
      size = prop.uhVal.QuadPart;
      */
      RINOK(nextStream->Seek(0, STREAM_SEEK_END, &size));
      RINOK(nextStream->Seek(0, STREAM_SEEK_SET, NULL));
    }
    _totalSize += size;
    _sizes.Add(size);
    _streams.Add(nextStream);
    {
      const UInt64 numFiles = _streams.Size();
      RINOK(callback->SetCompleted(&numFiles, NULL));
    }
  }

  if (_streams.Size() == 1)
  {
    if (splitStyle)
      return S_FALSE;
  }
  return S_OK;
}

STDMETHODIMP CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *callback)
{
  COM_TRY_BEGIN
  HRESULT res = Open2(stream, callback);
  if (res != S_OK)
    Close();
  return res;
  COM_TRY_END
}

STDMETHODIMP CHandler::Close()
{
  _totalSize = 0;
  _subName.Empty();
  _streams.Clear();
  _sizes.Clear();
  return S_OK;
}

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = _streams.IsEmpty() ? 0 : 1;
  return S_OK;
}

STDMETHODIMP CHandler::GetProperty(UInt32 /* index */, PROPID propID, PROPVARIANT *value)
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPath: prop = _subName; break;
    case kpidSize:
    case kpidPackSize:
      prop = _totalSize;
      break;
  }
  prop.Detach(value);
  return S_OK;
}

STDMETHODIMP CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback)
{
  COM_TRY_BEGIN
  if (numItems == 0)
    return S_OK;
  if (numItems != (UInt32)(Int32)-1 && (numItems != 1 || indices[0] != 0))
    return E_INVALIDARG;

  UInt64 currentTotalSize = 0;
  RINOK(extractCallback->SetTotal(_totalSize));
  CMyComPtr<ISequentialOutStream> outStream;
  Int32 askMode = testMode ?
      NExtract::NAskMode::kTest :
      NExtract::NAskMode::kExtract;
  RINOK(extractCallback->GetStream(0, &outStream, askMode));
  if (!testMode && !outStream)
    return S_OK;
  RINOK(extractCallback->PrepareOperation(askMode));
  
  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder;
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  FOR_VECTOR (i, _streams)
  {
    lps->InSize = lps->OutSize = currentTotalSize;
    RINOK(lps->SetCur());
    IInStream *inStream = _streams[i];
    RINOK(inStream->Seek(0, STREAM_SEEK_SET, NULL));
    RINOK(copyCoder->Code(inStream, outStream, NULL, NULL, progress));
    currentTotalSize += copyCoderSpec->TotalSize;
  }
  outStream.Release();
  return extractCallback->SetOperationResult(NExtract::NOperationResult::kOK);
  COM_TRY_END
}

STDMETHODIMP CHandler::GetStream(UInt32 index, ISequentialInStream **stream)
{
  COM_TRY_BEGIN
  if (index != 0)
    return E_INVALIDARG;
  *stream = 0;
  CMultiStream *streamSpec = new CMultiStream;
  CMyComPtr<ISequentialInStream> streamTemp = streamSpec;
  FOR_VECTOR (i, _streams)
  {
    CMultiStream::CSubStreamInfo subStreamInfo;
    subStreamInfo.Stream = _streams[i];
    subStreamInfo.Size = _sizes[i];
    streamSpec->Streams.Add(subStreamInfo);
  }
  streamSpec->Init();
  *stream = streamTemp.Detach();
  return S_OK;
  COM_TRY_END
}

REGISTER_ARC_I_NO_SIG(
  "Split", "001", 0, 0xEA,
  0,
  0,
  NULL)

}}

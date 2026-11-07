// SplitHandler.cpp

#include "StdAfx.h"

#include "../../Common/ComTry.h"

#include "../../Windows/PropVariant.h"

#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

#include "../Compress/CopyCoder.h"

#include "Common/MultiStream.h"

using namespace NWindows;

namespace NArchive {
namespace NSplit {

Z7_class_CHandler_final:
  public IInArchive,
  public IInArchiveGetStream,
  public IInStream,
  public CMyUnknownImp
{
  Z7_IFACES_IMP_UNK_4(
      IInArchive,
      IInArchiveGetStream,
      ISequentialInStream,
      IInStream)

  unsigned _volIndex; // index of vol in last access for faster processing
  UInt64 _pos;
  UInt64 _totalSize;

  struct CVol
  {
    CMyComPtr<IInStream> Stream;
    UInt64 Size;
    UInt64 GlobalOffset;
    UInt64 LocalPos;
  };

  CObjectVector<CVol> Vols;
  UString _subName;

  HRESULT Open2(IInStream *stream, IArchiveOpenCallback *callback);
};

static const Byte kArcProps[] =
{
  kpidNumVolumes,
  kpidTotalPhySize
};

static const Byte kProps[] =
{
  kpidPath,
  kpidSize
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidMainSubfile: prop = (UInt32)0; break;
    case kpidPhySize: if (!Vols.IsEmpty()) prop = Vols[0].Size; break;
    case kpidTotalPhySize: prop = _totalSize; break;
    case kpidNumVolumes: prop = (UInt32)Vols.Size(); break;
    default: break;
  }
  prop.Detach(value);
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 /* index */, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPath: prop = _subName; break;
    case kpidSize:
    case kpidPackSize:
      prop = _totalSize;
      break;
    default: break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


HRESULT CHandler::Open2(IInStream *stream, IArchiveOpenCallback *callback)
{
  Close();
  if (!callback)
    return S_FALSE;
  Z7_DECL_CMyComPtr_QI_FROM(IArchiveOpenVolumeCallback, volumeCallback, callback)
  if (!volumeCallback)
    return S_FALSE;
  
  CSplitVolName seq;
  {
    NCOM::CPropVariant prop;
    RINOK(volumeCallback->GetProperty(kpidName, &prop))
    if (prop.vt != VT_BSTR)
      return S_FALSE;
    seq.Name = prop.bstrVal;
  }
  if (!seq.ParseName())
    return S_FALSE;
  seq.Generate_SubName(_subName);
  {
    CMyComPtr<IInStream> nextStream;
    do
    {
      UInt64 size;
      RINOK(InStream_AtBegin_GetSize(stream, size))
      CVol &v = Vols.AddNew();
      v.Stream = stream;
      v.Size = size;
      v.GlobalOffset = _totalSize;
      v.LocalPos = 0;
      _totalSize += size;
      if (_totalSize < size)
        return S_FALSE;
      stream = NULL; // we don' want (stream) to point to (nextStream) object
      {
        const UInt64 numFiles = Vols.Size();
        RINOK(callback->SetCompleted(&numFiles, NULL))
      }
      if (!seq.GetNextName())
        break;
      seq.GenerateName();
      nextStream.Release();
      HRESULT result = volumeCallback->GetStream(seq.Name, &nextStream);
#ifndef Z7_SFX
      if (result == S_FALSE && !nextStream && seq.IsComplicated)
      {
        seq.AddPostfix_2Chars();
        seq.GenerateName();
        result = volumeCallback->GetStream(seq.Name, &nextStream);
      }
#endif
      if (result != S_OK && result != S_FALSE)
        return result;
      stream = nextStream;
    }
    while (stream);
  }

#ifndef Z7_SFX
  if (Vols.Size() == 1) // for single volume case, we allow only 7zip_style files with dot : "*.001"
    if (seq.ChangedPos == 0 || seq.IsLatinPostfix())
      return S_FALSE;
#endif
  return S_OK;
}


Z7_COM7F_IMF(CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *callback))
{
  COM_TRY_BEGIN
  const HRESULT res = Open2(stream, callback);
  if (res != S_OK)
    Close();
  return res;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _volIndex = 0;
  _pos = 0;
  _totalSize = 0;
  _subName.Empty();
  Vols.Clear();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = Vols.IsEmpty() ? 0 : 1;
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

  RINOK(extractCallback->SetTotal(_totalSize))
  int opRes;
  {
    CMyComPtr<ISequentialOutStream> outStream;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    RINOK(extractCallback->GetStream(0, &outStream, askMode))
    if (!testMode && !outStream)
      return S_OK;
    RINOK(extractCallback->PrepareOperation(askMode))
        
    NCompress::CCopyCoderBase copyCoder;
    CMyComPtr2_Create<ICompressProgressInfo, CLocalProgress> lps;
    lps->Init(extractCallback, false);
      
    opRes = NExtract::NOperationResult::kOK;
    FOR_VECTOR(i, Vols)
    {
      CVol &v = Vols[i];
      v.LocalPos = (UInt64)0-1;
      IInStream *inStream = v.Stream;
      RINOK(InStream_SeekToBegin(inStream))
      RINOK(copyCoder.Code(inStream, outStream, NULL, lps))
      if (copyCoder.TotalSize != v.Size)
        opRes = NExtract::NOperationResult::kDataError;
      CLocalProgress *lps2 = lps.ClsPtr();
      lps2->InSize += copyCoder.TotalSize;
      lps2->OutSize = lps2->InSize;
      RINOK(lps2->SetCur())
    }
  }
  return extractCallback->SetOperationResult(opRes);
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::Read(void *data, UInt32 size, UInt32 *processedSize))
{
  if (processedSize)
    *processedSize = 0;
  if (size == 0)
    return S_OK;
  if (_pos >= _totalSize)
    return S_OK;

  unsigned left = 0, mid = _volIndex, right = Vols.Size();
  for (;;)
  {
    CVol &v = Vols[mid];
    if (_pos < v.GlobalOffset)
      right = mid;
    else
    {
      const UInt64 localPos = _pos - v.GlobalOffset;
      if (localPos < v.Size)
      {
        _volIndex = mid;
        if (localPos != v.LocalPos)
          RINOK(v.Stream->Seek((Int64)localPos, STREAM_SEEK_SET, &v.LocalPos))
        {
          const UInt64 rem = v.Size - localPos;
          if (size > rem)
            size = (UInt32)rem;
        }
        UInt32 sizeProc = 0;
        const HRESULT result = v.Stream->Read(data, size, &sizeProc);
        size = sizeProc;
        _pos += size;
        if (result == S_OK)
          v.LocalPos += size;
        else
          v.LocalPos = (UInt64)0-1;
        if (processedSize)
          *processedSize = size;
        return result;
      }
      left = mid + 1;
    }
    mid = (left + right) / 2;
  }
}
  
Z7_COM7F_IMF(CHandler::Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition))
{
  switch (seekOrigin)
  {
    case STREAM_SEEK_SET: break;
    case STREAM_SEEK_CUR: offset += _pos; break;
    case STREAM_SEEK_END: offset += _totalSize; break;
    default: return STG_E_INVALIDFUNCTION;
  }
  if (offset < 0)
    return HRESULT_WIN32_ERROR_NEGATIVE_SEEK;
  _pos = (UInt64)offset;
  if (newPosition)
    *newPosition = (UInt64)offset;
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetStream(UInt32 index, ISequentialInStream **stream))
{
  COM_TRY_BEGIN
  if (index != 0)
    return E_INVALIDARG;
  *stream = NULL;
  _volIndex = 0;
  _pos = 0;
  /*
  FOR_VECTOR (i, Vols)
  {
    CVol &v = Vols[i];
    v.LocalPos = 0;
    RINOK(InStream_SeekToBegin(v.Stream))
  }
  */
  CMyComPtr<ISequentialInStream> streamTemp = this;
  *stream = streamTemp.Detach();
  return S_OK;
  COM_TRY_END
}

REGISTER_ARC_I_NO_SIG(
  "Split", "001", NULL, 0xEA,
  0,
  0,
  NULL)

}}

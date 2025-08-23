﻿// UdfHandler.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"

#include "../../../Windows/PropVariant.h"
#include "../../../Windows/TimeUtils.h"

#include "../../Common/LimitedStreams.h"
#include "../../Common/ProgressUtils.h"
#include "../../Common/RegisterArc.h"
#include "../../Common/StreamObjects.h"

#include "../../Compress/CopyCoder.h"

#include "UdfHandler.h"

namespace NArchive {
namespace NUdf {

static void UdfTimeToFileTime(const CTime &t, NWindows::NCOM::CPropVariant &prop)
{
  UInt64 numSecs;
  const Byte *d = t.Data;
  if (!NWindows::NTime::GetSecondsSince1601(t.GetYear(), d[4], d[5], d[6], d[7], d[8], numSecs))
    return;
  if (t.IsLocal())
    numSecs = (UInt64)((Int64)numSecs - (Int64)((Int32)t.GetMinutesOffset() * 60));
  const UInt32 m0 = d[9];
  const UInt32 m1 = d[10];
  const UInt32 m2 = d[11];
  unsigned numDigits = 0;
  UInt64 v = numSecs * 10000000;
  if (m0 < 100 && m1 < 100 && m2 < 100)
  {
    v += m0 * 100000 + m1 * 1000 + m2 * 10;
    numDigits = 6;
  }
  prop.SetAsTimeFrom_Ft64_Prec(v, k_PropVar_TimePrec_Base + numDigits);
}

static const Byte kProps[] =
{
  kpidPath,
  kpidIsDir,
  kpidSize,
  kpidPackSize,
  kpidMTime,
  kpidATime,
  kpidCTime,
  kpidChangeTime,
  // kpidUserId,
  // kpidGroupId,
  // kpidPosixAttrib,
  kpidLinks
};

static const Byte kArcProps[] =
{
  kpidUnpackVer,
  kpidClusterSize,
  kpidSectorSize,
  kpidCTime,
  kpidMTime,
  kpidComment
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPhySize: prop = _archive.PhySize; break;

    case kpidUnpackVer:
    {
      if (_archive.LogVols.Size() == 1)
      {
        UString s;
        const CLogVol &vol = _archive.LogVols[0];
        vol.DomainId.AddUdfVersionTo(s);
        if (!s.IsEmpty())
          prop = s;
      }
      break;
    }
    case kpidComment:
    {
      UString comment = _archive.GetComment();
      if (!comment.IsEmpty())
        prop = comment;
      break;
    }

    case kpidClusterSize:
      if (_archive.LogVols.Size() > 0)
      {
        UInt32 blockSize = _archive.LogVols[0].BlockSize;
        unsigned i;
        for (i = 1; i < _archive.LogVols.Size(); i++)
          if (_archive.LogVols[i].BlockSize != blockSize)
            break;
        if (i == _archive.LogVols.Size())
          prop = blockSize;
      }
      break;

    case kpidSectorSize: prop = ((UInt32)1 << _archive.SecLogSize); break;

    case kpidCTime:
      if (_archive.LogVols.Size() == 1)
      {
        const CLogVol &vol = _archive.LogVols[0];
        if (vol.FileSets.Size() >= 1)
          UdfTimeToFileTime(vol.FileSets[0].RecordingTime, prop);
      }
      break;
    case kpidMTime:
      if (_archive.PrimeVols.Size() == 1)
      {
        const CPrimeVol &pv = _archive.PrimeVols[0];
        UdfTimeToFileTime(pv.RecordingTime, prop);
      }
      break;

    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_archive.IsArc) v |= kpv_ErrorFlags_IsNotArc;
      if (_archive.Unsupported) v |= kpv_ErrorFlags_UnsupportedFeature;
      if (_archive.UnexpectedEnd) v |= kpv_ErrorFlags_UnexpectedEnd;
      if (_archive.NoEndAnchor) v |= kpv_ErrorFlags_HeadersError;
      prop = v;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

class CProgressImp Z7_final: public CProgressVirt
{
  CMyComPtr<IArchiveOpenCallback> _callback;
  UInt64 _numFiles;
  UInt64 _numBytes;
public:
  HRESULT SetTotal(UInt64 numBytes) Z7_override;
  HRESULT SetCompleted(UInt64 numFiles, UInt64 numBytes) Z7_override;
  HRESULT SetCompleted() Z7_override;
  CProgressImp(IArchiveOpenCallback *callback): _callback(callback), _numFiles(0), _numBytes(0) {}
};

HRESULT CProgressImp::SetTotal(UInt64 numBytes)
{
  if (_callback)
    return _callback->SetTotal(NULL, &numBytes);
  return S_OK;
}

HRESULT CProgressImp::SetCompleted(UInt64 numFiles, UInt64 numBytes)
{
  _numFiles = numFiles;
  _numBytes = numBytes;
  return SetCompleted();
}

HRESULT CProgressImp::SetCompleted()
{
  if (_callback)
    return _callback->SetCompleted(&_numFiles, &_numBytes);
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *callback))
{
  COM_TRY_BEGIN
  {
    Close();
    CProgressImp progressImp(callback);
    RINOK(_archive.Open(stream, &progressImp))
    bool showVolName = (_archive.LogVols.Size() > 1);
    FOR_VECTOR (volIndex, _archive.LogVols)
    {
      const CLogVol &vol = _archive.LogVols[volIndex];
      bool showFileSetName = (vol.FileSets.Size() > 1);
      // showFileSetName = true; // for debug
      FOR_VECTOR (fsIndex, vol.FileSets)
      {
        const CFileSet &fs = vol.FileSets[fsIndex];
        for (unsigned i = ((showVolName || showFileSetName) ? 0 : 1); i < fs.Refs.Size(); i++)
        {
          CRef2 ref2;
          ref2.Vol = volIndex;
          ref2.Fs = fsIndex;
          ref2.Ref = i;
          _refs2.Add(ref2);
        }
      }
    }
    _inStream = stream;
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _inStream.Release();
  _archive.Clear();
  _refs2.Clear();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _refs2.Size();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  {
    const CRef2 &ref2 = _refs2[index];
    const CLogVol &vol = _archive.LogVols[ref2.Vol];
    const CRef &ref = vol.FileSets[ref2.Fs].Refs[ref2.Ref];
    const CFile &file = _archive.Files[ref.FileIndex];
    const CItem &item = _archive.Items[file.ItemIndex];
    switch (propID)
    {
      case kpidPath:  prop = _archive.GetItemPath(ref2.Vol, ref2.Fs, ref2.Ref,
            _archive.LogVols.Size() > 1, vol.FileSets.Size() > 1); break;
      case kpidIsDir:  prop = item.IsDir(); break;
      case kpidSize:      if (!item.IsDir()) prop = (UInt64)item.Size; break;
      case kpidPackSize:  if (!item.IsDir()) prop = (UInt64)item.NumLogBlockRecorded * vol.BlockSize; break;
      case kpidMTime:  UdfTimeToFileTime(item.MTime, prop); break;
      case kpidATime:  UdfTimeToFileTime(item.ATime, prop); break;
      case kpidCTime:
        if (item.IsExtended)
            UdfTimeToFileTime(item.CreateTime, prop);
        break;
      case kpidChangeTime:  UdfTimeToFileTime(item.AttribTime, prop); break;
      // case kpidUserId: prop = item.Uid; break;
      // case kpidGroupId: prop = item.Gid; break;
      // case kpidPosixAttrib: prop = (UInt32)item.Permissions; break;
      case kpidLinks: prop = (UInt32)item.FileLinkCount; break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetStream(UInt32 index, ISequentialInStream **stream))
{
  *stream = NULL;

  const CRef2 &ref2 = _refs2[index];
  const CLogVol &vol = _archive.LogVols[ref2.Vol];
  const CRef &ref = vol.FileSets[ref2.Fs].Refs[ref2.Ref];
  const CFile &file = _archive.Files[ref.FileIndex];
  const CItem &item = _archive.Items[file.ItemIndex];
  UInt64 size = item.Size;

  if (!item.IsRecAndAlloc() || !item.CheckChunkSizes() || ! _archive.CheckItemExtents(ref2.Vol, item))
    return E_NOTIMPL;

  if (item.IsInline)
  {
    Create_BufInStream_WithNewBuffer(item.InlineData, stream);
    return S_OK;
  }

  CExtentsStream *extentStreamSpec = new CExtentsStream();
  CMyComPtr<ISequentialInStream> extentStream = extentStreamSpec;
  
  extentStreamSpec->Stream = _inStream;

  UInt64 virtOffset = 0;
  FOR_VECTOR (extentIndex, item.Extents)
  {
    const CMyExtent &extent = item.Extents[extentIndex];
    UInt32 len = extent.GetLen();
    if (len == 0)
      continue;
    if (size < len)
      return S_FALSE;
      
    const unsigned partitionIndex = vol.PartitionMaps[extent.PartitionRef].PartitionIndex;
    UInt32 logBlockNumber = extent.Pos;
    const CPartition &partition = _archive.Partitions[partitionIndex];
    UInt64 offset = ((UInt64)partition.Pos << _archive.SecLogSize) +
      (UInt64)logBlockNumber * vol.BlockSize;
      
    CSeekExtent se;
    se.Phy = offset;
    se.Virt = virtOffset;
    virtOffset += len;
    extentStreamSpec->Extents.Add(se);

    size -= len;
  }
  if (size != 0)
    return S_FALSE;
  CSeekExtent se;
  se.Phy = 0;
  se.Virt = virtOffset;
  extentStreamSpec->Extents.Add(se);
  extentStreamSpec->Init();
  *stream = extentStream.Detach();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _refs2.Size();
  if (numItems == 0)
    return S_OK;
  UInt64 totalSize = 0;
  UInt32 i;

  for (i = 0; i < numItems; i++)
  {
    const UInt32 index = (allFilesMode ? i : indices[i]);
    const CRef2 &ref2 = _refs2[index];
    const CRef &ref = _archive.LogVols[ref2.Vol].FileSets[ref2.Fs].Refs[ref2.Ref];
    const CFile &file = _archive.Files[ref.FileIndex];
    const CItem &item = _archive.Items[file.ItemIndex];
    if (!item.IsDir())
      totalSize += item.Size;
  }
  RINOK(extractCallback->SetTotal(totalSize))

  UInt64 currentTotalSize = 0;
  
  CMyComPtr2_Create<ICompressProgressInfo, CLocalProgress> lps;
  lps->Init(extractCallback, false);
  CMyComPtr2_Create<ICompressCoder, NCompress::CCopyCoder> copyCoder;
  CMyComPtr2_Create<ISequentialOutStream, CLimitedSequentialOutStream> outStream;

  for (i = 0;; i++)
  {
    lps->InSize = lps->OutSize = currentTotalSize;
    RINOK(lps->SetCur())
    if (i >= numItems)
      break;
    CMyComPtr<ISequentialOutStream> realOutStream;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    const UInt32 index = allFilesMode ? i : indices[i];
    
    RINOK(extractCallback->GetStream(index, &realOutStream, askMode))

    const CRef2 &ref2 = _refs2[index];
    const CRef &ref = _archive.LogVols[ref2.Vol].FileSets[ref2.Fs].Refs[ref2.Ref];
    const CFile &file = _archive.Files[ref.FileIndex];
    const CItem &item = _archive.Items[file.ItemIndex];

    if (item.IsDir())
    {
      RINOK(extractCallback->PrepareOperation(askMode))
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
      continue;
    }
    currentTotalSize += item.Size;

    if (!testMode && !realOutStream)
      continue;

    RINOK(extractCallback->PrepareOperation(askMode))
    outStream->SetStream(realOutStream);
    realOutStream.Release();
    outStream->Init(item.Size);
    Int32 opRes;
    {
      CMyComPtr<ISequentialInStream> udfInStream;
      const HRESULT res = GetStream(index, &udfInStream);
      if (res == E_NOTIMPL)
        opRes = NExtract::NOperationResult::kUnsupportedMethod;
      else if (res != S_OK)
        opRes = NExtract::NOperationResult::kDataError;
      else
      {
        RINOK(copyCoder.Interface()->Code(udfInStream, outStream, NULL, NULL, lps))
        opRes = outStream->IsFinishedOK() ?
            NExtract::NOperationResult::kOK:
            NExtract::NOperationResult::kDataError;
      }
    }
    outStream->ReleaseStream();
    RINOK(extractCallback->SetOperationResult(opRes))
  }
  return S_OK;
  COM_TRY_END
}

static const UInt32 kIsoStartPos = 0x8000;

//  5, { 0, 'N', 'S', 'R', '0' },

static const Byte k_Signature[] =
{
  8,    0, 'B', 'E', 'A', '0', '1', 1, 0,
  6,    1, 'C', 'D', '0', '0', '1'
};

REGISTER_ARC_I(
  "Udf", "udf iso img", NULL, 0xE0,
  k_Signature,
  kIsoStartPos,
  NArcInfoFlags::kMultiSignature |
  NArcInfoFlags::kStartOpen,
  IsArc_Udf)

}}

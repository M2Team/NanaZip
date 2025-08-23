﻿// IsoHandler.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/MyLinux.h"
#include "../../../Common/StringConvert.h"

#include "../../Common/LimitedStreams.h"
#include "../../Common/ProgressUtils.h"
#include "../../Common/StreamUtils.h"

#include "../../Compress/CopyCoder.h"

#include "../Common/ItemNameUtils.h"

#include "IsoHandler.h"

using namespace NWindows;
using namespace NTime;

namespace NArchive {
namespace NIso {

static const Byte kProps[] =
{
  kpidPath,
  kpidIsDir,
  kpidSize,
  kpidPackSize,
  kpidMTime,
  // kpidCTime,
  // kpidATime,
  kpidPosixAttrib,
  // kpidUserId,
  // kpidGroupId,
  // kpidLinks,
  kpidSymLink
};

static const Byte kArcProps[] =
{
  kpidComment,
  kpidCTime,
  kpidMTime,
  // kpidHeadersSize
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

Z7_COM7F_IMF(CHandler::Open(IInStream *stream,
    const UInt64 * /* maxCheckStartPosition */,
    IArchiveOpenCallback * /* openArchiveCallback */))
{
  COM_TRY_BEGIN
  Close();
  {
    RINOK(_archive.Open(stream))
    _stream = stream;
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Close())
{
  _archive.Clear();
  _stream.Release();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _archive.Refs.Size() + _archive.BootEntries.Size();
  return S_OK;
}

static void AddString(AString &s, const char *name, const Byte *p, unsigned size)
{
  unsigned i;
  for (i = 0; i < size && p[i]; i++);
  for (; i > 0 && p[i - 1] == ' '; i--);
  if (i != 0)
  {
    AString d;
    d.SetFrom((const char *)p, i);
    s += name;
    s += ": ";
    s += d;
    s.Add_LF();
  }
}

static void AddProp_Size64(AString &s, const char *name, UInt64 size)
{
  s += name;
  s += ": ";
  s.Add_UInt64(size);
  s.Add_LF();
}

#define ADD_STRING(n, v) AddString(s, n, vol. v, sizeof(vol. v))

static void AddErrorMessage(AString &s, const char *message)
{
  if (!s.IsEmpty())
    s += ". ";
  s += message;
}

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  if (_stream)
  {
  const CVolumeDescriptor &vol = _archive.VolDescs[_archive.MainVolDescIndex];
  switch (propID)
  {
    case kpidComment:
    {
      AString s;
      ADD_STRING("System", SystemId);
      ADD_STRING("Volume", VolumeId);
      ADD_STRING("VolumeSet", VolumeSetId);
      ADD_STRING("Publisher", PublisherId);
      ADD_STRING("Preparer", DataPreparerId);
      ADD_STRING("Application", ApplicationId);
      ADD_STRING("Copyright", CopyrightFileId);
      ADD_STRING("Abstract", AbstractFileId);
      ADD_STRING("Bib", BibFileId);
      // ADD_STRING("EscapeSequence", EscapeSequence);
      AddProp_Size64(s, "VolumeSpaceSize", vol.Get_VolumeSpaceSize_inBytes());
      AddProp_Size64(s, "VolumeSetSize", vol.VolumeSetSize);
      AddProp_Size64(s, "VolumeSequenceNumber", vol.VolumeSequenceNumber);
      
      prop = s;
      break;
    }
    case kpidCTime: { vol.CTime.GetFileTime(prop); break; }
    case kpidMTime: { vol.MTime.GetFileTime(prop); break; }
  }
  }

  switch (propID)
  {
    case kpidPhySize: prop = _archive.PhySize; break;
    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_archive.IsArc) v |= kpv_ErrorFlags_IsNotArc;
      if (_archive.UnexpectedEnd) v |= kpv_ErrorFlags_UnexpectedEnd;
      if (_archive.HeadersError) v |= kpv_ErrorFlags_HeadersError;
      prop = v;
      break;
    }
    
    case kpidError:
    {
      AString s;
      if (_archive.IncorrectBigEndian)
        AddErrorMessage(s, "Incorrect big-endian headers");
      if (_archive.SelfLinkedDirs)
        AddErrorMessage(s, "Self-linked directory");
      if (_archive.TooDeepDirs)
        AddErrorMessage(s, "Too deep directory levels");
      if (!s.IsEmpty())
        prop = s;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  if (index >= (UInt32)_archive.Refs.Size())
  {
    index -= _archive.Refs.Size();
    const CBootInitialEntry &be = _archive.BootEntries[index];
    switch (propID)
    {
      case kpidPath:
      {
        AString s ("[BOOT]" STRING_PATH_SEPARATOR);
        if (_archive.BootEntries.Size() != 1)
        {
          s.Add_UInt32(index + 1);
          s.Add_Minus();
        }
        s += be.GetName();
        prop = s;
        break;
      }
      case kpidIsDir: prop = false; break;
      case kpidSize:
      case kpidPackSize:
        prop = (UInt64)_archive.GetBootItemSize(index);
        break;
    }
  }
  else
  {
    const CRef &ref = _archive.Refs[index];
    const CDir &item = ref.Dir->_subItems[ref.Index];
    switch (propID)
    {
      case kpidPath:
        // if (item.FileId.GetCapacity() >= 0)
        {
          UString s;
          if (_archive.IsJoliet())
            item.GetPathU(s);
          else
            s = MultiByteToUnicodeString(item.GetPath(_archive.IsSusp, _archive.SuspSkipSize), CP_OEMCP);

          if (s.Len() >= 2 && s[s.Len() - 2] == ';' && s.Back() == '1')
            s.DeleteFrom(s.Len() - 2);
          
          if (!s.IsEmpty() && s.Back() == L'.')
            s.DeleteBack();

          NItemName::ReplaceToOsSlashes_Remove_TailSlash(s);
          prop = s;
        }
        break;

      case kpidSymLink:
        if (_archive.IsSusp)
        {
          UInt32 mode;
          if (item.GetPx(_archive.SuspSkipSize, k_Px_Mode, mode))
          {
            if (MY_LIN_S_ISLNK(mode))
            {
              AString s8;
              if (item.GetSymLink(_archive.SuspSkipSize, s8))
              {
                UString s = MultiByteToUnicodeString(s8, CP_OEMCP);
                prop = s;
              }
            }
          }
        }
        break;


      case kpidPosixAttrib:
      /*
      case kpidLinks:
      case kpidUserId:
      case kpidGroupId:
      */
      {
        if (_archive.IsSusp)
        {
          UInt32 t = 0;
          switch (propID)
          {
            case kpidPosixAttrib: t = k_Px_Mode; break;
            /*
            case kpidLinks: t = k_Px_Links; break;
            case kpidUserId: t = k_Px_User; break;
            case kpidGroupId: t = k_Px_Group; break;
            */
          }
          UInt32 v;
          if (item.GetPx(_archive.SuspSkipSize, t, v))
            prop = v;
        }
        break;
      }
      
      case kpidIsDir: prop = item.IsDir(); break;
      case kpidSize:
      case kpidPackSize:
        if (!item.IsDir())
          prop = (UInt64)ref.TotalSize;
        break;

      case kpidMTime:
      // case kpidCTime:
      // case kpidATime:
      {
        // if
        item.DateTime.GetFileTime(prop);
        /*
        else
        {
          UInt32 t = 0;
          switch (propID)
          {
            case kpidMTime: t = k_Tf_MTime; break;
            case kpidCTime: t = k_Tf_CTime; break;
            case kpidATime: t = k_Tf_ATime; break;
          }
          CRecordingDateTime dt;
          if (item.GetTf(_archive.SuspSkipSize, t, dt))
          {
            FILETIME utc;
            if (dt.GetFileTime(utc))
              prop = utc;
          }
        }
        */
        break;
      }
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _archive.Refs.Size();
  if (numItems == 0)
    return S_OK;
  UInt64 totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    UInt32 index = (allFilesMode ? i : indices[i]);
    if (index < (UInt32)_archive.Refs.Size())
    {
      const CRef &ref = _archive.Refs[index];
      const CDir &item = ref.Dir->_subItems[ref.Index];
      if (!item.IsDir())
        totalSize += ref.TotalSize;
    }
    else
      totalSize += _archive.GetBootItemSize(index - _archive.Refs.Size());
  }
  RINOK(extractCallback->SetTotal(totalSize))

  UInt64 currentTotalSize = 0;
  UInt64 currentItemSize;
  
  CMyComPtr2_Create<ICompressProgressInfo, CLocalProgress> lps;
  lps->Init(extractCallback, false);
  CMyComPtr2_Create<ICompressCoder, NCompress::CCopyCoder> copyCoder;
  CMyComPtr2_Create<ISequentialInStream, CLimitedSequentialInStream> inStream;
  inStream->SetStream(_stream);

  for (i = 0;; i++, currentTotalSize += currentItemSize)
  {
    lps->InSize = lps->OutSize = currentTotalSize;
    RINOK(lps->SetCur())
    if (i >= numItems)
      break;
    currentItemSize = 0;
    Int32 opRes = NExtract::NOperationResult::kOK;
  {
    CMyComPtr<ISequentialOutStream> realOutStream;
    const Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    const UInt32 index = allFilesMode ? i : indices[i];
    
    RINOK(extractCallback->GetStream(index, &realOutStream, askMode))

    UInt64 blockIndex;
    if (index < (UInt32)_archive.Refs.Size())
    {
      const CRef &ref = _archive.Refs[index];
      const CDir &item = ref.Dir->_subItems[ref.Index];
      if (item.IsDir())
      {
        RINOK(extractCallback->PrepareOperation(askMode))
        RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
        continue;
      }
      currentItemSize = ref.TotalSize;
      blockIndex = item.ExtentLocation;
    }
    else
    {
      unsigned bootIndex = index - _archive.Refs.Size();
      const CBootInitialEntry &be = _archive.BootEntries[bootIndex];
      currentItemSize = _archive.GetBootItemSize(bootIndex);
      blockIndex = be.LoadRBA;
    }
   

    if (!testMode && !realOutStream)
      continue;

    RINOK(extractCallback->PrepareOperation(askMode))

    if (index < (UInt32)_archive.Refs.Size())
    {
      const CRef &ref = _archive.Refs[index];
      UInt64 offset = 0;
      for (UInt32 e = 0; e < ref.NumExtents; e++)
      {
        const CDir &item2 = ref.Dir->_subItems[ref.Index + e];
        if (item2.Size == 0)
          continue;
        lps->InSize = lps->OutSize = currentTotalSize + offset;
        RINOK(InStream_SeekSet(_stream, (UInt64)item2.ExtentLocation * kBlockSize))
        inStream->Init(item2.Size);
        RINOK(copyCoder.Interface()->Code(inStream, realOutStream, NULL, NULL, lps))
        if (copyCoder->TotalSize != item2.Size)
        {
          opRes = NExtract::NOperationResult::kDataError;
          break;
        }
        offset += item2.Size;
      }
    }
    else
    {
      RINOK(InStream_SeekSet(_stream, (UInt64)blockIndex * kBlockSize))
      inStream->Init(currentItemSize);
      RINOK(copyCoder.Interface()->Code(inStream, realOutStream, NULL, NULL, lps))
      if (copyCoder->TotalSize != currentItemSize)
        opRes = NExtract::NOperationResult::kDataError;
    }
    // realOutStream.Release();
  }
    RINOK(extractCallback->SetOperationResult(opRes))
  }
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CHandler::GetStream(UInt32 index, ISequentialInStream **stream))
{
  COM_TRY_BEGIN
  *stream = NULL;
  UInt64 blockIndex;
  UInt64 currentItemSize;
  
  if (index < _archive.Refs.Size())
  {
    const CRef &ref = _archive.Refs[index];
    const CDir &item = ref.Dir->_subItems[ref.Index];
    if (item.IsDir())
      return S_FALSE;

    if (ref.NumExtents > 1)
    {
      CExtentsStream *extentStreamSpec = new CExtentsStream();
      CMyComPtr<ISequentialInStream> extentStream = extentStreamSpec;
      
      extentStreamSpec->Stream = _stream;
      
      UInt64 virtOffset = 0;
      for (UInt32 i = 0; i < ref.NumExtents; i++)
      {
        const CDir &item2 = ref.Dir->_subItems[ref.Index + i];
        if (item2.Size == 0)
          continue;
        CSeekExtent se;
        se.Phy = (UInt64)item2.ExtentLocation * kBlockSize;
        se.Virt = virtOffset;
        extentStreamSpec->Extents.Add(se);
        virtOffset += item2.Size;
      }
      if (virtOffset != ref.TotalSize)
        return S_FALSE;
      CSeekExtent se;
      se.Phy = 0;
      se.Virt = virtOffset;
      extentStreamSpec->Extents.Add(se);
      extentStreamSpec->Init();
      *stream = extentStream.Detach();
      return S_OK;
    }
    
    currentItemSize = item.Size;
    blockIndex = item.ExtentLocation;
  }
  else
  {
    unsigned bootIndex = index - _archive.Refs.Size();
    const CBootInitialEntry &be = _archive.BootEntries[bootIndex];
    currentItemSize = _archive.GetBootItemSize(bootIndex);
    blockIndex = be.LoadRBA;
  }
  
  return CreateLimitedInStream(_stream, (UInt64)blockIndex * kBlockSize, currentItemSize, stream);
  COM_TRY_END
}

}}

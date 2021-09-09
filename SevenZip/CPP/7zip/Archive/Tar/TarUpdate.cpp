// TarUpdate.cpp

#include "StdAfx.h"

#include "../../../Windows/TimeUtils.h"

#include "../../Common/LimitedStreams.h"
#include "../../Common/ProgressUtils.h"

#include "../../Compress/CopyCoder.h"

#include "TarOut.h"
#include "TarUpdate.h"

namespace NArchive {
namespace NTar {

HRESULT UpdateArchive(IInStream *inStream, ISequentialOutStream *outStream,
    const CObjectVector<NArchive::NTar::CItemEx> &inputItems,
    const CObjectVector<CUpdateItem> &updateItems,
    UINT codePage, unsigned utfFlags,
    IArchiveUpdateCallback *updateCallback)
{
  COutArchive outArchive;
  outArchive.Create(outStream);
  outArchive.Pos = 0;

  CMyComPtr<IOutStream> outSeekStream;
  outStream->QueryInterface(IID_IOutStream, (void **)&outSeekStream);

  CMyComPtr<IArchiveUpdateCallbackFile> opCallback;
  updateCallback->QueryInterface(IID_IArchiveUpdateCallbackFile, (void **)&opCallback);

  UInt64 complexity = 0;

  unsigned i;
  for (i = 0; i < updateItems.Size(); i++)
  {
    const CUpdateItem &ui = updateItems[i];
    if (ui.NewData)
      complexity += ui.Size;
    else
      complexity += inputItems[(unsigned)ui.IndexInArc].GetFullSize();
  }

  RINOK(updateCallback->SetTotal(complexity));

  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder;
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(updateCallback, true);

  CLimitedSequentialInStream *streamSpec = new CLimitedSequentialInStream;
  CMyComPtr<CLimitedSequentialInStream> inStreamLimited(streamSpec);
  streamSpec->SetStream(inStream);

  complexity = 0;

  for (i = 0; i < updateItems.Size(); i++)
  {
    lps->InSize = lps->OutSize = complexity;
    RINOK(lps->SetCur());

    const CUpdateItem &ui = updateItems[i];
    CItem item;
  
    if (ui.NewProps)
    {
      item.Mode = ui.Mode;
      item.Name = ui.Name;
      item.User = ui.User;
      item.Group = ui.Group;
      
      if (ui.IsDir)
      {
        item.LinkFlag = NFileHeader::NLinkFlag::kDirectory;
        item.PackSize = 0;
      }
      else
      {
        item.LinkFlag = NFileHeader::NLinkFlag::kNormal;
        item.PackSize = ui.Size;
      }
      
      item.MTime = ui.MTime;
      item.DeviceMajorDefined = false;
      item.DeviceMinorDefined = false;
      item.UID = 0;
      item.GID = 0;
      memcpy(item.Magic, NFileHeader::NMagic::kUsTar_00, 8);
    }
    else
      item = inputItems[(unsigned)ui.IndexInArc];

    AString symLink;
    if (ui.NewData || ui.NewProps)
    {
      RINOK(GetPropString(updateCallback, ui.IndexInClient, kpidSymLink, symLink, codePage, utfFlags, true));
      if (!symLink.IsEmpty())
      {
        item.LinkFlag = NFileHeader::NLinkFlag::kSymLink;
        item.LinkName = symLink;
      }
    }

    if (ui.NewData)
    {
      item.SparseBlocks.Clear();
      item.PackSize = ui.Size;
      item.Size = ui.Size;
      if (ui.Size == (UInt64)(Int64)-1)
        return E_INVALIDARG;

      CMyComPtr<ISequentialInStream> fileInStream;

      bool needWrite = true;
      
      if (!symLink.IsEmpty())
      {
        item.PackSize = 0;
        item.Size = 0;
      }
      else
      {
        HRESULT res = updateCallback->GetStream(ui.IndexInClient, &fileInStream);
        
        if (res == S_FALSE)
          needWrite = false;
        else
        {
          RINOK(res);
          
          if (fileInStream)
          {
            CMyComPtr<IStreamGetProps> getProps;
            fileInStream->QueryInterface(IID_IStreamGetProps, (void **)&getProps);
            if (getProps)
            {
              FILETIME mTime;
              UInt64 size2;
              if (getProps->GetProps(&size2, NULL, NULL, &mTime, NULL) == S_OK)
              {
                item.PackSize = size2;
                item.Size = size2;
                item.MTime = NWindows::NTime::FileTimeToUnixTime64(mTime);;
              }
            }
          }
          else
          {
            item.PackSize = 0;
            item.Size = 0;
          }

          {
            AString hardLink;
            RINOK(GetPropString(updateCallback, ui.IndexInClient, kpidHardLink, hardLink, codePage, utfFlags, true));
            if (!hardLink.IsEmpty())
            {
              item.LinkFlag = NFileHeader::NLinkFlag::kHardLink;
              item.LinkName = hardLink;
              item.PackSize = 0;
              item.Size = 0;
              fileInStream.Release();
            }
          }
        }
      }

      if (needWrite)
      {
        UInt64 fileHeaderStartPos = outArchive.Pos;
        RINOK(outArchive.WriteHeader(item));
        if (fileInStream)
        {
          RINOK(copyCoder->Code(fileInStream, outStream, NULL, NULL, progress));
          outArchive.Pos += copyCoderSpec->TotalSize;
          if (copyCoderSpec->TotalSize != item.PackSize)
          {
            if (!outSeekStream)
              return E_FAIL;
            UInt64 backOffset = outArchive.Pos - fileHeaderStartPos;
            RINOK(outSeekStream->Seek(-(Int64)backOffset, STREAM_SEEK_CUR, NULL));
            outArchive.Pos = fileHeaderStartPos;
            item.PackSize = copyCoderSpec->TotalSize;
            RINOK(outArchive.WriteHeader(item));
            RINOK(outSeekStream->Seek((Int64)item.PackSize, STREAM_SEEK_CUR, NULL));
            outArchive.Pos += item.PackSize;
          }
          RINOK(outArchive.FillDataResidual(item.PackSize));
        }
      }
      
      complexity += item.PackSize;
      RINOK(updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK));
    }
    else
    {
      const CItemEx &existItem = inputItems[(unsigned)ui.IndexInArc];
      UInt64 size;
      
      if (ui.NewProps)
      {
        // memcpy(item.Magic, NFileHeader::NMagic::kEmpty, 8);
        
        if (!symLink.IsEmpty())
        {
          item.PackSize = 0;
          item.Size = 0;
        }
        else
        {
          if (ui.IsDir == existItem.IsDir())
            item.LinkFlag = existItem.LinkFlag;
        
          item.SparseBlocks = existItem.SparseBlocks;
          item.Size = existItem.Size;
          item.PackSize = existItem.PackSize;
        }
        
        item.DeviceMajorDefined = existItem.DeviceMajorDefined;
        item.DeviceMinorDefined = existItem.DeviceMinorDefined;
        item.DeviceMajor = existItem.DeviceMajor;
        item.DeviceMinor = existItem.DeviceMinor;
        item.UID = existItem.UID;
        item.GID = existItem.GID;
        
        RINOK(outArchive.WriteHeader(item));
        RINOK(inStream->Seek((Int64)existItem.GetDataPosition(), STREAM_SEEK_SET, NULL));
        size = existItem.PackSize;
      }
      else
      {
        RINOK(inStream->Seek((Int64)existItem.HeaderPos, STREAM_SEEK_SET, NULL));
        size = existItem.GetFullSize();
      }
      
      streamSpec->Init(size);

      if (opCallback)
      {
        RINOK(opCallback->ReportOperation(
            NEventIndexType::kInArcIndex, (UInt32)ui.IndexInArc,
            NUpdateNotifyOp::kReplicate))
      }

      RINOK(copyCoder->Code(inStreamLimited, outStream, NULL, NULL, progress));
      if (copyCoderSpec->TotalSize != size)
        return E_FAIL;
      outArchive.Pos += size;
      RINOK(outArchive.FillDataResidual(existItem.PackSize));
      complexity += size;
    }
  }
  
  lps->InSize = lps->OutSize = complexity;
  RINOK(lps->SetCur());
  return outArchive.WriteFinishHeader();
}

}}

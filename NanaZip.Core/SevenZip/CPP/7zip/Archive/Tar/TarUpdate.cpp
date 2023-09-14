// TarUpdate.cpp

#include "StdAfx.h"

// #include <stdio.h>

#include "../../../Windows/TimeUtils.h"

#include "../../Common/LimitedStreams.h"
#include "../../Common/ProgressUtils.h"
#include "../../Common/StreamUtils.h"

#include "../../Compress/CopyCoder.h"

#include "TarOut.h"
#include "TarUpdate.h"

namespace NArchive {
namespace NTar {

static void FILETIME_To_PaxTime(const FILETIME &ft, CPaxTime &pt)
{
  UInt32 ns;
  pt.Sec = NWindows::NTime::FileTime_To_UnixTime64_and_Quantums(ft, ns);
  pt.Ns = ns * 100;
  pt.NumDigits = 7;
}


HRESULT Prop_To_PaxTime(const NWindows::NCOM::CPropVariant &prop, CPaxTime &pt)
{
  pt.Clear();
  if (prop.vt == VT_EMPTY)
  {
    // pt.Sec = 0;
    return S_OK;
  }
  if (prop.vt != VT_FILETIME)
    return E_INVALIDARG;
  {
    UInt32 ns;
    pt.Sec = NWindows::NTime::FileTime_To_UnixTime64_and_Quantums(prop.filetime, ns);
    ns *= 100;
    pt.NumDigits = 7;
    const unsigned prec = prop.wReserved1;
    if (prec >= k_PropVar_TimePrec_Base)
    {
      pt.NumDigits = (int)(prec - k_PropVar_TimePrec_Base);
      if (prop.wReserved2 < 100)
        ns += prop.wReserved2;
    }
    pt.Ns = ns;
    return S_OK;
  }
}


static HRESULT GetTime(IStreamGetProp *getProp, UInt32 pid, CPaxTime &pt)
{
  pt.Clear();
  NWindows::NCOM::CPropVariant prop;
  RINOK(getProp->GetProperty(pid, &prop))
  return Prop_To_PaxTime(prop, pt);
}


static HRESULT GetUser(IStreamGetProp *getProp,
    UInt32 pidName, UInt32 pidId, AString &name, UInt32 &id,
    UINT codePage, unsigned utfFlags)
{
  // printf("\nGetUser\n");
  // we keep  old values, if both   GetProperty() return VT_EMPTY
  // we clear old values, if any of GetProperty() returns non-VT_EMPTY;
  bool isSet = false;
  {
    NWindows::NCOM::CPropVariant prop;
    RINOK(getProp->GetProperty(pidId, &prop))
    if (prop.vt == VT_UI4)
    {
      isSet = true;
      id = prop.ulVal;
      name.Empty();
    }
    else if (prop.vt != VT_EMPTY)
      return E_INVALIDARG;
  }
  {
    NWindows::NCOM::CPropVariant prop;
    RINOK(getProp->GetProperty(pidName, &prop))
    if (prop.vt == VT_BSTR)
    {
      const UString s = prop.bstrVal;
      Get_AString_From_UString(s, name, codePage, utfFlags);
      // printf("\ngetProp->GetProperty(pidName, &prop) : %s" , name.Ptr());
      if (!isSet)
        id = 0;
    }
    else if (prop.vt == VT_UI4)
    {
      id = prop.ulVal;
      name.Empty();
    }
    else if (prop.vt != VT_EMPTY)
      return E_INVALIDARG;
  }
  return S_OK;
}


/*
static HRESULT GetDevice(IStreamGetProp *getProp,
    UInt32 &majo, UInt32 &mino, bool &majo_defined, bool &mino_defined)
{
  NWindows::NCOM::CPropVariant prop;
  RINOK(getProp->GetProperty(kpidDevice, &prop));
  if (prop.vt == VT_EMPTY)
    return S_OK;
  if (prop.vt != VT_UI8)
    return E_INVALIDARG;
  {
    printf("\nTarUpdate.cpp :: GetDevice()\n");
    const UInt64 v = prop.uhVal.QuadPart;
    majo = MY_dev_major(v);
    mino = MY_dev_minor(v);
    majo_defined = true;
    mino_defined = true;
  }
  return S_OK;
}
*/

static HRESULT GetDevice(IStreamGetProp *getProp,
    UInt32 pid, UInt32 &id, bool &defined)
{
  defined = false;
  NWindows::NCOM::CPropVariant prop;
  RINOK(getProp->GetProperty(pid, &prop))
  if (prop.vt == VT_EMPTY)
    return S_OK;
  if (prop.vt == VT_UI4)
  {
    id = prop.ulVal;
    defined = true;
    return S_OK;
  }
  return E_INVALIDARG;
}


HRESULT UpdateArchive(IInStream *inStream, ISequentialOutStream *outStream,
    const CObjectVector<NArchive::NTar::CItemEx> &inputItems,
    const CObjectVector<CUpdateItem> &updateItems,
    const CUpdateOptions &options,
    IArchiveUpdateCallback *updateCallback)
{
  COutArchive outArchive;
  outArchive.Create(outStream);
  outArchive.Pos = 0;
  outArchive.IsPosixMode = options.PosixMode;
  outArchive.TimeOptions = options.TimeOptions;

  Z7_DECL_CMyComPtr_QI_FROM(IOutStream, outSeekStream, outStream)
  Z7_DECL_CMyComPtr_QI_FROM(IStreamSetRestriction, setRestriction, outStream)
  Z7_DECL_CMyComPtr_QI_FROM(IArchiveUpdateCallbackFile, opCallback, outStream)

  if (outSeekStream)
  {
    /*
    // for debug
    Byte buf[1 << 14];
    memset (buf, 0, sizeof(buf));
    RINOK(outStream->Write(buf, sizeof(buf), NULL));
    */
    // we need real outArchive.Pos, if outSeekStream->SetSize() will be used.
    RINOK(outSeekStream->Seek(0, STREAM_SEEK_CUR, &outArchive.Pos))
  }
  if (setRestriction)
    RINOK(setRestriction->SetRestriction(0, 0))

  UInt64 complexity = 0;

  unsigned i;
  for (i = 0; i < updateItems.Size(); i++)
  {
    const CUpdateItem &ui = updateItems[i];
    if (ui.NewData)
      complexity += ui.Size;
    else
      complexity += inputItems[(unsigned)ui.IndexInArc].Get_FullSize_Aligned();
  }

  RINOK(updateCallback->SetTotal(complexity))

  NCompress::CCopyCoder *copyCoderSpec = new NCompress::CCopyCoder;
  CMyComPtr<ICompressCoder> copyCoder = copyCoderSpec;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(updateCallback, true);

  CLimitedSequentialInStream *streamSpec = new CLimitedSequentialInStream;
  CMyComPtr<ISequentialInStream> inStreamLimited(streamSpec);
  streamSpec->SetStream(inStream);

  complexity = 0;

  // const int kNumReduceDigits = -1; // for debug

  for (i = 0;; i++)
  {
    lps->InSize = lps->OutSize = complexity;
    RINOK(lps->SetCur())

    if (i == updateItems.Size())
    {
      if (outSeekStream && setRestriction)
        RINOK(setRestriction->SetRestriction(0, 0))
      return outArchive.WriteFinishHeader();
    }

    const CUpdateItem &ui = updateItems[i];
    CItem item;
  
    if (ui.NewProps)
    {
      item.SetMagic_Posix(options.PosixMode);
      item.Name = ui.Name;
      item.User = ui.User;
      item.Group = ui.Group;
      item.UID = ui.UID;
      item.GID = ui.GID;
      item.DeviceMajor = ui.DeviceMajor;
      item.DeviceMinor = ui.DeviceMinor;
      item.DeviceMajor_Defined = ui.DeviceMajor_Defined;
      item.DeviceMinor_Defined = ui.DeviceMinor_Defined;
      
      if (ui.IsDir)
      {
        item.LinkFlag = NFileHeader::NLinkFlag::kDirectory;
        item.PackSize = 0;
      }
      else
      {
        item.PackSize = ui.Size;
        item.Set_LinkFlag_for_File(ui.Mode);
      }

      // 22.00
      item.Mode = ui.Mode & ~(UInt32)MY_LIN_S_IFMT;
      item.PaxTimes = ui.PaxTimes;
      // item.PaxTimes.ReducePrecison(kNumReduceDigits); // for debug
      item.MTime = ui.PaxTimes.MTime.GetSec();
    }
    else
      item = inputItems[(unsigned)ui.IndexInArc];

    AString symLink;
    if (ui.NewData || ui.NewProps)
    {
      RINOK(GetPropString(updateCallback, ui.IndexInClient, kpidSymLink, symLink,
          options.CodePage, options.UtfFlags, true))
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
        const HRESULT res = updateCallback->GetStream(ui.IndexInClient, &fileInStream);
        
        if (res == S_FALSE)
          needWrite = false;
        else
        {
          RINOK(res)
          
          if (!fileInStream)
          {
            item.PackSize = 0;
            item.Size = 0;
          }
          else
          {
            Z7_DECL_CMyComPtr_QI_FROM(IStreamGetProp, getProp, fileInStream)
            if (getProp)
            {
              if (options.Write_MTime.Val) RINOK(GetTime(getProp, kpidMTime, item.PaxTimes.MTime))
              if (options.Write_ATime.Val) RINOK(GetTime(getProp, kpidATime, item.PaxTimes.ATime))
              if (options.Write_CTime.Val) RINOK(GetTime(getProp, kpidCTime, item.PaxTimes.CTime))

              if (options.PosixMode)
              {
                /*
                RINOK(GetDevice(getProp, item.DeviceMajor, item.DeviceMinor,
                    item.DeviceMajor_Defined, item.DeviceMinor_Defined));
                */
                bool defined = false;
                UInt32 val = 0;
                RINOK(GetDevice(getProp, kpidDeviceMajor, val, defined))
                if (defined)
                {
                  item.DeviceMajor = val;
                  item.DeviceMajor_Defined = true;
                  item.DeviceMinor = 0;
                  item.DeviceMinor_Defined = false;
                  RINOK(GetDevice(getProp, kpidDeviceMinor, item.DeviceMinor, item.DeviceMinor_Defined))
                }
              }

              RINOK(GetUser(getProp, kpidUser,  kpidUserId,  item.User,  item.UID, options.CodePage, options.UtfFlags))
              RINOK(GetUser(getProp, kpidGroup, kpidGroupId, item.Group, item.GID, options.CodePage, options.UtfFlags))

              {
                NWindows::NCOM::CPropVariant prop;
                RINOK(getProp->GetProperty(kpidPosixAttrib, &prop))
                if (prop.vt == VT_EMPTY)
                  item.Mode =
                    MY_LIN_S_IRWXO
                  | MY_LIN_S_IRWXG
                  | MY_LIN_S_IRWXU
                  | (ui.IsDir ? MY_LIN_S_IFDIR : MY_LIN_S_IFREG);
                else if (prop.vt != VT_UI4)
                  return E_INVALIDARG;
                else
                  item.Mode = prop.ulVal;
                // 21.07 : we clear high file type bits as GNU TAR.
                item.Set_LinkFlag_for_File(item.Mode);
                item.Mode &= ~(UInt32)MY_LIN_S_IFMT;
              }

              {
                NWindows::NCOM::CPropVariant prop;
                RINOK(getProp->GetProperty(kpidSize, &prop))
                if (prop.vt != VT_UI8)
                  return E_INVALIDARG;
                const UInt64 size = prop.uhVal.QuadPart;
                item.PackSize = size;
                item.Size = size;
              }
              /*
              printf("\nNum digits = %d %d\n",
                  (int)item.PaxTimes.MTime.NumDigits,
                  (int)item.PaxTimes.MTime.Ns);
              */
            }
            else
            {
              Z7_DECL_CMyComPtr_QI_FROM(IStreamGetProps, getProps, fileInStream)
              if (getProps)
              {
                FILETIME mTime, aTime, cTime;
                UInt64 size2;
                if (getProps->GetProps(&size2,
                    options.Write_CTime.Val ? &cTime : NULL,
                    options.Write_ATime.Val ? &aTime : NULL,
                    options.Write_MTime.Val ? &mTime : NULL,
                    NULL) == S_OK)
                {
                  item.PackSize = size2;
                  item.Size = size2;
                  if (options.Write_MTime.Val) FILETIME_To_PaxTime(mTime, item.PaxTimes.MTime);
                  if (options.Write_ATime.Val) FILETIME_To_PaxTime(aTime, item.PaxTimes.ATime);
                  if (options.Write_CTime.Val) FILETIME_To_PaxTime(cTime, item.PaxTimes.CTime);
                }
              }
            }
          }

          {
            // we must request kpidHardLink after updateCallback->GetStream()
            AString hardLink;
            RINOK(GetPropString(updateCallback, ui.IndexInClient, kpidHardLink, hardLink,
                options.CodePage, options.UtfFlags, true))
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

      // item.PaxTimes.ReducePrecison(kNumReduceDigits); // for debug

      if (ui.NewProps)
        item.MTime = item.PaxTimes.MTime.GetSec();

      if (needWrite)
      {
        const UInt64 headerPos = outArchive.Pos;
        // item.PackSize = ((UInt64)1 << 33); // for debug

        if (outSeekStream && setRestriction)
          RINOK(setRestriction->SetRestriction(outArchive.Pos, (UInt64)(Int64)-1))

        RINOK(outArchive.WriteHeader(item))
        if (fileInStream)
        {
          for (unsigned numPasses = 0;; numPasses++)
          {
            /* we support 2 attempts to write header:
                pass-0: main pass:
                pass-1: additional pass, if size_of_file and size_of_header are changed */
            if (numPasses >= 2)
            {
              // opRes = NArchive::NUpdate::NOperationResult::kError_FileChanged;
              // break;
              return E_FAIL;
            }
            
            const UInt64 dataPos = outArchive.Pos;
            RINOK(copyCoder->Code(fileInStream, outStream, NULL, NULL, progress))
            outArchive.Pos += copyCoderSpec->TotalSize;
            RINOK(outArchive.Write_AfterDataResidual(copyCoderSpec->TotalSize))
            
            // if (numPasses >= 10) // for debug
            if (copyCoderSpec->TotalSize == item.PackSize)
              break;
            
            if (opCallback)
            {
              RINOK(opCallback->ReportOperation(
                  NEventIndexType::kOutArcIndex, (UInt32)ui.IndexInClient,
                  NUpdateNotifyOp::kInFileChanged))
            }
            
            if (!outSeekStream)
              return E_FAIL;
            const UInt64 nextPos = outArchive.Pos;
            RINOK(outSeekStream->Seek(-(Int64)(nextPos - headerPos), STREAM_SEEK_CUR, NULL))
            outArchive.Pos = headerPos;
            item.PackSize = copyCoderSpec->TotalSize;
            
            RINOK(outArchive.WriteHeader(item))
            
            // if (numPasses >= 10) // for debug
            if (outArchive.Pos == dataPos)
            {
              const UInt64 alignedSize = nextPos - dataPos;
              if (alignedSize != 0)
              {
                RINOK(outSeekStream->Seek((Int64)alignedSize, STREAM_SEEK_CUR, NULL))
                outArchive.Pos += alignedSize;
              }
              break;
            }
            
            // size of header was changed.
            // we remove data after header and try new attempt, if required
            Z7_DECL_CMyComPtr_QI_FROM(IInStream, fileSeekStream, fileInStream)
            if (!fileSeekStream)
              return E_FAIL;
            RINOK(InStream_SeekToBegin(fileSeekStream))
            RINOK(outSeekStream->SetSize(outArchive.Pos))
            if (item.PackSize == 0)
              break;
          }
        }
      }
      
      complexity += item.PackSize;
      fileInStream.Release();
      RINOK(updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK))
    }
    else
    {
      // (ui.NewData == false)

      if (opCallback)
      {
        RINOK(opCallback->ReportOperation(
            NEventIndexType::kInArcIndex, (UInt32)ui.IndexInArc,
            NUpdateNotifyOp::kReplicate))
      }

      const CItemEx &existItem = inputItems[(unsigned)ui.IndexInArc];
      UInt64 size, pos;
      
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
        
        item.DeviceMajor_Defined = existItem.DeviceMajor_Defined;
        item.DeviceMinor_Defined = existItem.DeviceMinor_Defined;
        item.DeviceMajor = existItem.DeviceMajor;
        item.DeviceMinor = existItem.DeviceMinor;
        item.UID = existItem.UID;
        item.GID = existItem.GID;
        
        RINOK(outArchive.WriteHeader(item))
        size = existItem.Get_PackSize_Aligned();
        pos = existItem.Get_DataPos();
      }
      else
      {
        size = existItem.Get_FullSize_Aligned();
        pos = existItem.HeaderPos;
      }

      if (size != 0)
      {
        RINOK(InStream_SeekSet(inStream, pos))
        streamSpec->Init(size);
        if (outSeekStream && setRestriction)
          RINOK(setRestriction->SetRestriction(0, 0))
        // 22.00 : we copy Residual data from old archive to new archive instead of zeroing
        RINOK(copyCoder->Code(inStreamLimited, outStream, NULL, NULL, progress))
        if (copyCoderSpec->TotalSize != size)
          return E_FAIL;
        outArchive.Pos += size;
        // RINOK(outArchive.Write_AfterDataResidual(existItem.PackSize));
        complexity += size;
      }
    }
  }
}

}}

// ZipUpdate.cpp

#include "StdAfx.h"

#include "../../../../C/Alloc.h"

#include "../../../Common/AutoPtr.h"
#include "../../../Common/Defs.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/TimeUtils.h"
#include "../../../Windows/Thread.h"

#include "../../Common/CreateCoder.h"
#include "../../Common/LimitedStreams.h"
#include "../../Common/OutMemStream.h"
#include "../../Common/ProgressUtils.h"
#ifndef _7ZIP_ST
#include "../../Common/ProgressMt.h"
#endif
#include "../../Common/StreamUtils.h"

#include "../../Compress/CopyCoder.h"

#include "ZipAddCommon.h"
#include "ZipOut.h"
#include "ZipUpdate.h"

using namespace NWindows;
using namespace NSynchronization;

namespace NArchive {
namespace NZip {

static const Byte kHostOS =
  #ifdef _WIN32
  NFileHeader::NHostOS::kFAT;
  #else
  NFileHeader::NHostOS::kUnix;
  #endif

static const Byte kMadeByHostOS = kHostOS;

// 18.06: now we always write zero to high byte of ExtractVersion field.
// Previous versions of p7zip wrote (NFileHeader::NHostOS::kUnix) there, that is not correct
static const Byte kExtractHostOS = 0;

static const Byte kMethodForDirectory = NFileHeader::NCompressionMethod::kStore;


static void AddAesExtra(CItem &item, Byte aesKeyMode, UInt16 method)
{
  CWzAesExtra wzAesField;
  wzAesField.Strength = aesKeyMode;
  wzAesField.Method = method;
  item.Method = NFileHeader::NCompressionMethod::kWzAES;
  item.Crc = 0;
  CExtraSubBlock sb;
  wzAesField.SetSubBlock(sb);
  item.LocalExtra.SubBlocks.Add(sb);
  item.CentralExtra.SubBlocks.Add(sb);
}


static void Copy_From_UpdateItem_To_ItemOut(const CUpdateItem &ui, CItemOut &item)
{
  item.Name = ui.Name;
  item.Name_Utf = ui.Name_Utf;
  item.Comment = ui.Comment;
  item.SetUtf8(ui.IsUtf8);
  // item.SetFlag_AltStream(ui.IsAltStream);
  // item.ExternalAttrib = ui.Attrib;
  item.Time = ui.Time;
  item.Ntfs_MTime = ui.Ntfs_MTime;
  item.Ntfs_ATime = ui.Ntfs_ATime;
  item.Ntfs_CTime = ui.Ntfs_CTime;
  item.NtfsTimeIsDefined = ui.NtfsTimeIsDefined;
}

static void SetFileHeader(
    const CCompressionMethodMode &options,
    const CUpdateItem &ui,
    bool useDescriptor,
    CItemOut &item)
{
  item.Size = ui.Size;
  const bool isDir = ui.IsDir;

  item.ClearFlags();

  if (ui.NewProps)
  {
    Copy_From_UpdateItem_To_ItemOut(ui, item);
    // item.SetFlag_AltStream(ui.IsAltStream);
    item.ExternalAttrib = ui.Attrib;
  }
  /*
  else
    isDir = item.IsDir();
  */

  item.MadeByVersion.HostOS = kMadeByHostOS;
  item.MadeByVersion.Version = NFileHeader::NCompressionMethod::kMadeByProgramVersion;
  
  item.ExtractVersion.HostOS = kExtractHostOS;

  item.InternalAttrib = 0; // test it
  item.SetEncrypted(!isDir && options.PasswordIsDefined);
  item.SetDescriptorMode(useDescriptor);

  if (isDir)
  {
    item.ExtractVersion.Version = NFileHeader::NCompressionMethod::kExtractVersion_Dir;
    item.Method = kMethodForDirectory;
    item.PackSize = 0;
    item.Size = 0;
    item.Crc = 0;
  }

  item.LocalExtra.Clear();
  item.CentralExtra.Clear();

  if (isDir)
  {
    item.ExtractVersion.Version = NFileHeader::NCompressionMethod::kExtractVersion_Dir;
    item.Method = kMethodForDirectory;
    item.PackSize = 0;
    item.Size = 0;
    item.Crc = 0;
  }
  else if (options.IsRealAesMode())
    AddAesExtra(item, options.AesKeyMode, (Byte)(options.MethodSequence.IsEmpty() ? 8 : options.MethodSequence[0]));
}


// we call SetItemInfoFromCompressingResult() after SetFileHeader()

static void SetItemInfoFromCompressingResult(const CCompressingResult &compressingResult,
    bool isAesMode, Byte aesKeyMode, CItem &item)
{
  item.ExtractVersion.Version = compressingResult.ExtractVersion;
  item.Method = compressingResult.Method;
  if (compressingResult.Method == NFileHeader::NCompressionMethod::kLZMA && compressingResult.LzmaEos)
    item.Flags |= NFileHeader::NFlags::kLzmaEOS;
  item.Crc = compressingResult.CRC;
  item.Size = compressingResult.UnpackSize;
  item.PackSize = compressingResult.PackSize;

  item.LocalExtra.Clear();
  item.CentralExtra.Clear();

  if (isAesMode)
    AddAesExtra(item, aesKeyMode, compressingResult.Method);
}


#ifndef _7ZIP_ST

struct CMtSem
{
  NWindows::NSynchronization::CSemaphore Semaphore;
  NWindows::NSynchronization::CCriticalSection CS;
  CIntVector Indexes;
  int Head;

  void ReleaseItem(unsigned index)
  {
    {
      CCriticalSectionLock lock(CS);
      Indexes[index] = Head;
      Head = (int)index;
    }
    Semaphore.Release();
  }

  int GetFreeItem()
  {
    int i;
    {
      CCriticalSectionLock lock(CS);
      i = Head;
      Head = Indexes[(unsigned)i];
    }
    return i;
  }
};

static THREAD_FUNC_DECL CoderThread(void *threadCoderInfo);

struct CThreadInfo
{
  DECL_EXTERNAL_CODECS_LOC_VARS2;

  NWindows::CThread Thread;
  NWindows::NSynchronization::CAutoResetEvent CompressEvent;
  CMtSem *MtSem;
  unsigned ThreadIndex;

  bool ExitThread;

  CMtCompressProgress *ProgressSpec;
  CMyComPtr<ICompressProgressInfo> Progress;

  COutMemStream *OutStreamSpec;
  CMyComPtr<IOutStream> OutStream;
  CMyComPtr<ISequentialInStream> InStream;

  CAddCommon Coder;
  HRESULT Result;
  CCompressingResult CompressingResult;

  bool InSeqMode;
  bool OutSeqMode;
  bool IsFree;
  UInt32 UpdateIndex;
  UInt32 FileTime;
  UInt64 ExpectedDataSize;

  CThreadInfo():
      ExitThread(false),
      ProgressSpec(NULL),
      OutStreamSpec(NULL),
      InSeqMode(false),
      OutSeqMode(false),
      FileTime(0),
      ExpectedDataSize((UInt64)(Int64)-1)
  {}

  void SetOptions(const CCompressionMethodMode &options)
  {
    Coder.SetOptions(options);
  }
  
  HRESULT CreateEvents()
  {
    WRes wres = CompressEvent.CreateIfNotCreated_Reset();
    return HRESULT_FROM_WIN32(wres);
  }
  
  HRESULT CreateThread()
  {
    WRes wres = Thread.Create(CoderThread, this);
    return HRESULT_FROM_WIN32(wres);
  }

  void WaitAndCode();

  void StopWait_Close()
  {
    ExitThread = true;
    if (OutStreamSpec)
      OutStreamSpec->StopWriting(E_ABORT);
    if (CompressEvent.IsCreated())
      CompressEvent.Set();
    Thread.Wait_Close();
  }
};

void CThreadInfo::WaitAndCode()
{
  for (;;)
  {
    CompressEvent.Lock();
    if (ExitThread)
      return;

    Result = Coder.Compress(
        EXTERNAL_CODECS_LOC_VARS
        InStream, OutStream,
        InSeqMode, OutSeqMode, FileTime, ExpectedDataSize,
        Progress, CompressingResult);
    
    if (Result == S_OK && Progress)
      Result = Progress->SetRatioInfo(&CompressingResult.UnpackSize, &CompressingResult.PackSize);
    
    MtSem->ReleaseItem(ThreadIndex);
  }
}

static THREAD_FUNC_DECL CoderThread(void *threadCoderInfo)
{
  ((CThreadInfo *)threadCoderInfo)->WaitAndCode();
  return 0;
}

class CThreads
{
public:
  CObjectVector<CThreadInfo> Threads;
  ~CThreads()
  {
    FOR_VECTOR (i, Threads)
      Threads[i].StopWait_Close();
  }
};

struct CMemBlocks2: public CMemLockBlocks
{
  bool Skip;
  bool InSeqMode;
  bool PreDescriptorMode;
  bool Finished;
  CCompressingResult CompressingResult;
  
  CMemBlocks2(): Skip(false), InSeqMode(false), PreDescriptorMode(false), Finished(false),
    CompressingResult() {}
};

class CMemRefs
{
public:
  CMemBlockManagerMt *Manager;
  CObjectVector<CMemBlocks2> Refs;
  CMemRefs(CMemBlockManagerMt *manager): Manager(manager) {} ;
  ~CMemRefs()
  {
    FOR_VECTOR (i, Refs)
      Refs[i].FreeOpt(Manager);
  }
};

class CMtProgressMixer2:
  public ICompressProgressInfo,
  public CMyUnknownImp
{
  UInt64 ProgressOffset;
  UInt64 InSizes[2];
  UInt64 OutSizes[2];
  CMyComPtr<IProgress> Progress;
  CMyComPtr<ICompressProgressInfo> RatioProgress;
  bool _inSizeIsMain;
public:
  NWindows::NSynchronization::CCriticalSection CriticalSection;
  MY_UNKNOWN_IMP
  void Create(IProgress *progress, bool inSizeIsMain);
  void SetProgressOffset(UInt64 progressOffset);
  void SetProgressOffset_NoLock(UInt64 progressOffset);
  HRESULT SetRatioInfo(unsigned index, const UInt64 *inSize, const UInt64 *outSize);
  STDMETHOD(SetRatioInfo)(const UInt64 *inSize, const UInt64 *outSize);
};

void CMtProgressMixer2::Create(IProgress *progress, bool inSizeIsMain)
{
  Progress = progress;
  Progress.QueryInterface(IID_ICompressProgressInfo, &RatioProgress);
  _inSizeIsMain = inSizeIsMain;
  ProgressOffset = InSizes[0] = InSizes[1] = OutSizes[0] = OutSizes[1] = 0;
}

void CMtProgressMixer2::SetProgressOffset_NoLock(UInt64 progressOffset)
{
  InSizes[1] = OutSizes[1] = 0;
  ProgressOffset = progressOffset;
}

void CMtProgressMixer2::SetProgressOffset(UInt64 progressOffset)
{
  CriticalSection.Enter();
  SetProgressOffset_NoLock(progressOffset);
  CriticalSection.Leave();
}

HRESULT CMtProgressMixer2::SetRatioInfo(unsigned index, const UInt64 *inSize, const UInt64 *outSize)
{
  NWindows::NSynchronization::CCriticalSectionLock lock(CriticalSection);
  if (index == 0 && RatioProgress)
  {
    RINOK(RatioProgress->SetRatioInfo(inSize, outSize));
  }
  if (inSize)
    InSizes[index] = *inSize;
  if (outSize)
    OutSizes[index] = *outSize;
  UInt64 v = ProgressOffset + (_inSizeIsMain  ?
      (InSizes[0] + InSizes[1]) :
      (OutSizes[0] + OutSizes[1]));
  return Progress->SetCompleted(&v);
}

STDMETHODIMP CMtProgressMixer2::SetRatioInfo(const UInt64 *inSize, const UInt64 *outSize)
{
  return SetRatioInfo(0, inSize, outSize);
}

class CMtProgressMixer:
  public ICompressProgressInfo,
  public CMyUnknownImp
{
public:
  CMtProgressMixer2 *Mixer2;
  CMyComPtr<ICompressProgressInfo> RatioProgress;
  void Create(IProgress *progress, bool inSizeIsMain);
  MY_UNKNOWN_IMP
  STDMETHOD(SetRatioInfo)(const UInt64 *inSize, const UInt64 *outSize);
};

void CMtProgressMixer::Create(IProgress *progress, bool inSizeIsMain)
{
  Mixer2 = new CMtProgressMixer2;
  RatioProgress = Mixer2;
  Mixer2->Create(progress, inSizeIsMain);
}

STDMETHODIMP CMtProgressMixer::SetRatioInfo(const UInt64 *inSize, const UInt64 *outSize)
{
  return Mixer2->SetRatioInfo(1, inSize, outSize);
}


#endif

static HRESULT UpdateItemOldData(
    COutArchive &archive,
    CInArchive *inArchive,
    const CItemEx &itemEx,
    const CUpdateItem &ui,
    CItemOut &item,
    /* bool izZip64, */
    ICompressProgressInfo *progress,
    IArchiveUpdateCallbackFile *opCallback,
    UInt64 &complexity)
{
  if (opCallback)
  {
    RINOK(opCallback->ReportOperation(
        NEventIndexType::kInArcIndex, (UInt32)ui.IndexInArc,
        NUpdateNotifyOp::kReplicate))
  }

  UInt64 rangeSize;

  if (ui.NewProps)
  {
    if (item.HasDescriptor())
      return E_NOTIMPL;
    
    // we keep ExternalAttrib and some another properties from old archive
    // item.ExternalAttrib = ui.Attrib;
    // if we don't change Comment, we keep Comment from OldProperties
    Copy_From_UpdateItem_To_ItemOut(ui, item);
    // item.SetFlag_AltStream(ui.IsAltStream);

    item.CentralExtra.RemoveUnknownSubBlocks();
    item.LocalExtra.RemoveUnknownSubBlocks();

    archive.WriteLocalHeader(item);
    rangeSize = item.GetPackSizeWithDescriptor();
  }
  else
  {
    item.LocalHeaderPos = archive.GetCurPos();
    rangeSize = itemEx.GetLocalFullSize();
  }

  CMyComPtr<ISequentialInStream> packStream;

  RINOK(inArchive->GetItemStream(itemEx, ui.NewProps, packStream));
  if (!packStream)
    return E_NOTIMPL;

  complexity += rangeSize;

  CMyComPtr<ISequentialOutStream> outStream;
  archive.CreateStreamForCopying(outStream);
  HRESULT res = NCompress::CopyStream_ExactSize(packStream, outStream, rangeSize, progress);
  archive.MoveCurPos(rangeSize);
  return res;
}


static void WriteDirHeader(COutArchive &archive, const CCompressionMethodMode *options,
    const CUpdateItem &ui, CItemOut &item)
{
  SetFileHeader(*options, ui, false, item);
  archive.WriteLocalHeader(item);
}


static inline bool IsZero_FILETIME(const FILETIME &ft)
{
  return (ft.dwHighDateTime == 0 && ft.dwLowDateTime == 0);
}

static void UpdatePropsFromStream(CUpdateItem &item, ISequentialInStream *fileInStream,
    IArchiveUpdateCallback *updateCallback, UInt64 &totalComplexity)
{
  CMyComPtr<IStreamGetProps> getProps;
  fileInStream->QueryInterface(IID_IStreamGetProps, (void **)&getProps);
  if (!getProps)
    return;

  FILETIME cTime, aTime, mTime;
  UInt64 size;
  UInt32 attrib;
  if (getProps->GetProps(&size, &cTime, &aTime, &mTime, &attrib) != S_OK)
    return;
  
  if (size != item.Size && size != (UInt64)(Int64)-1)
  {
    const Int64 newComplexity = (Int64)totalComplexity + ((Int64)size - (Int64)item.Size);
    if (newComplexity > 0)
    {
      totalComplexity = (UInt64)newComplexity;
      updateCallback->SetTotal(totalComplexity);
    }
    item.Size = size;
  }
  
  if (!IsZero_FILETIME(mTime))
  {
    item.Ntfs_MTime = mTime;
    FILETIME loc = { 0, 0 };
    if (FileTimeToLocalFileTime(&mTime, &loc))
    {
      item.Time = 0;
      NTime::FileTimeToDosTime(loc, item.Time);
    }
  }

  if (!IsZero_FILETIME(cTime)) item.Ntfs_CTime = cTime;
  if (!IsZero_FILETIME(aTime)) item.Ntfs_ATime = aTime;

  item.Attrib = attrib;
}


static HRESULT Update2St(
    DECL_EXTERNAL_CODECS_LOC_VARS
    COutArchive &archive,
    CInArchive *inArchive,
    const CObjectVector<CItemEx> &inputItems,
    CObjectVector<CUpdateItem> &updateItems,
    const CCompressionMethodMode *options, bool outSeqMode,
    const CByteBuffer *comment,
    IArchiveUpdateCallback *updateCallback,
    UInt64 &totalComplexity,
    IArchiveUpdateCallbackFile *opCallback)
{
  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(updateCallback, true);

  CAddCommon compressor;
  compressor.SetOptions(*options);
  
  CObjectVector<CItemOut> items;
  UInt64 unpackSizeTotal = 0, packSizeTotal = 0;

  FOR_VECTOR (itemIndex, updateItems)
  {
    lps->InSize = unpackSizeTotal;
    lps->OutSize = packSizeTotal;
    RINOK(lps->SetCur());
    CUpdateItem &ui = updateItems[itemIndex];
    CItemEx itemEx;
    CItemOut item;

    if (!ui.NewProps || !ui.NewData)
    {
      // Note: for (ui.NewProps && !ui.NewData) it copies Props from old archive,
      // But we will rewrite all important properties later. But we can keep some properties like Comment
      itemEx = inputItems[(unsigned)ui.IndexInArc];
      if (inArchive->ReadLocalItemAfterCdItemFull(itemEx) != S_OK)
        return E_NOTIMPL;
      (CItem &)item = itemEx;
    }

    if (ui.NewData)
    {
      // bool isDir = ((ui.NewProps) ? ui.IsDir : item.IsDir());
      bool isDir = ui.IsDir;
      if (isDir)
      {
        WriteDirHeader(archive, options, ui, item);
      }
      else
      {
        CMyComPtr<ISequentialInStream> fileInStream;
        HRESULT res = updateCallback->GetStream(ui.IndexInClient, &fileInStream);
        if (res == S_FALSE)
        {
          lps->ProgressOffset += ui.Size;
          RINOK(updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK));
          continue;
        }
        RINOK(res);
        if (!fileInStream)
          return E_INVALIDARG;

        bool inSeqMode = false;
        if (!inSeqMode)
        {
          CMyComPtr<IInStream> inStream2;
          fileInStream->QueryInterface(IID_IInStream, (void **)&inStream2);
          inSeqMode = (inStream2 == NULL);
        }
        // seqMode = true; // to test seqMode

        UpdatePropsFromStream(ui, fileInStream, updateCallback, totalComplexity);

        CCompressingResult compressingResult;
        
        RINOK(compressor.Set_Pre_CompressionResult(
            inSeqMode, outSeqMode,
            ui.Size,
            compressingResult));

        SetFileHeader(*options, ui, compressingResult.DescriptorMode, item);

        // file Size can be 64-bit !!!

        SetItemInfoFromCompressingResult(compressingResult, options->IsRealAesMode(), options->AesKeyMode, item);

        archive.WriteLocalHeader(item);

        CMyComPtr<IOutStream> outStream;
        archive.CreateStreamForCompressing(outStream);
        
        RINOK(compressor.Compress(
            EXTERNAL_CODECS_LOC_VARS
            fileInStream, outStream,
            inSeqMode, outSeqMode,
            ui.Time, ui.Size,
            progress, compressingResult));
        
        if (item.HasDescriptor() != compressingResult.DescriptorMode)
          return E_FAIL;

        SetItemInfoFromCompressingResult(compressingResult, options->IsRealAesMode(), options->AesKeyMode, item);

        archive.WriteLocalHeader_Replace(item);

        RINOK(updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK));
        unpackSizeTotal += item.Size;
        packSizeTotal += item.PackSize;
      }
    }
    else
    {
      UInt64 complexity = 0;
      lps->SendRatio = false;

      RINOK(UpdateItemOldData(archive, inArchive, itemEx, ui, item, progress, opCallback, complexity));

      lps->SendRatio = true;
      lps->ProgressOffset += complexity;
    }
  
    items.Add(item);
    lps->ProgressOffset += kLocalHeaderSize;
  }

  lps->InSize = unpackSizeTotal;
  lps->OutSize = packSizeTotal;
  RINOK(lps->SetCur());

  archive.WriteCentralDir(items, comment);

  lps->ProgressOffset += kCentralHeaderSize * updateItems.Size() + 1;
  return lps->SetCur();
}


static HRESULT Update2(
    DECL_EXTERNAL_CODECS_LOC_VARS
    COutArchive &archive,
    CInArchive *inArchive,
    const CObjectVector<CItemEx> &inputItems,
    CObjectVector<CUpdateItem> &updateItems,
    const CCompressionMethodMode &options, bool outSeqMode,
    const CByteBuffer *comment,
    IArchiveUpdateCallback *updateCallback)
{
  CMyComPtr<IArchiveUpdateCallbackFile> opCallback;
  updateCallback->QueryInterface(IID_IArchiveUpdateCallbackFile, (void **)&opCallback);

  bool unknownComplexity = false;
  UInt64 complexity = 0;
  UInt64 numFilesToCompress = 0;
  UInt64 numBytesToCompress = 0;
 
  unsigned i;
  
  for (i = 0; i < updateItems.Size(); i++)
  {
    const CUpdateItem &ui = updateItems[i];
    if (ui.NewData)
    {
      if (ui.Size == (UInt64)(Int64)-1)
        unknownComplexity = true;
      else
        complexity += ui.Size;
      numBytesToCompress += ui.Size;
      numFilesToCompress++;
      /*
      if (ui.Commented)
        complexity += ui.CommentRange.Size;
      */
    }
    else
    {
      CItemEx inputItem = inputItems[(unsigned)ui.IndexInArc];
      if (inArchive->ReadLocalItemAfterCdItemFull(inputItem) != S_OK)
        return E_NOTIMPL;
      complexity += inputItem.GetLocalFullSize();
      // complexity += inputItem.GetCentralExtraPlusCommentSize();
    }
    complexity += kLocalHeaderSize;
    complexity += kCentralHeaderSize;
  }

  if (comment)
    complexity += comment->Size();
  complexity++; // end of central
  
  if (!unknownComplexity)
    updateCallback->SetTotal(complexity);

  UInt64 totalComplexity = complexity;

  CCompressionMethodMode options2 = options;

  if (options2._methods.IsEmpty())
  {
    // we need method item, if default method was used
    options2._methods.AddNew();
  }

  CAddCommon compressor;
  compressor.SetOptions(options2);
  
  complexity = 0;
  
  const Byte method = options.MethodSequence.Front();

  COneMethodInfo *oneMethodMain = NULL;
  if (!options2._methods.IsEmpty())
    oneMethodMain = &options2._methods[0];

  {
    FOR_VECTOR (mi, options2._methods)
    {
      options2.SetGlobalLevelTo(options2._methods[mi]);
    }
  }

  if (oneMethodMain)
  {
    // appnote recommends to use EOS marker for LZMA.
    if (method == NFileHeader::NCompressionMethod::kLZMA)
      oneMethodMain->AddProp_EndMarker_if_NotFound(true);
  }


  #ifndef _7ZIP_ST

  UInt32 numThreads = options._numThreads;

  {
    const UInt32 kNumMaxThreads =
      #ifdef _WIN32
        64; // _WIN32 supports only 64 threads in one group. So no need for more threads here
      #else
        128;
      #endif
    if (numThreads > kNumMaxThreads)
      numThreads = kNumMaxThreads;
  }
  /*
  if (numThreads > MAXIMUM_WAIT_OBJECTS) // is 64 in Windows
    numThreads = MAXIMUM_WAIT_OBJECTS;
  */
  if (numThreads < 1)
    numThreads = 1;

  const size_t kMemPerThread = (size_t)1 << 25;
  const size_t kBlockSize = 1 << 16;

  bool mtMode = (numThreads > 1);

  if (numFilesToCompress <= 1)
    mtMode = false;

  // mtMode = true; // debug: to test mtMode

  if (!mtMode)
  {
    FOR_VECTOR (mi, options2._methods)
    {
      COneMethodInfo &onem = options2._methods[mi];

      if (onem.FindProp(NCoderPropID::kNumThreads) < 0)
      {
        // fixed for 9.31. bzip2 default is just one thread.
        onem.AddProp_NumThreads(numThreads);
      }
    }
  }
  else
  {
    if (method == NFileHeader::NCompressionMethod::kStore && !options.PasswordIsDefined)
      numThreads = 1;
    
    if (oneMethodMain)
    {

    if (method == NFileHeader::NCompressionMethod::kBZip2)
    {
      bool fixedNumber;
      UInt32 numBZip2Threads = oneMethodMain->Get_BZip2_NumThreads(fixedNumber);
      if (!fixedNumber)
      {
        const UInt64 averageSize = numBytesToCompress / numFilesToCompress;
        const UInt32 blockSize = oneMethodMain->Get_BZip2_BlockSize();
        const UInt64 averageNumberOfBlocks = averageSize / blockSize + 1;
        numBZip2Threads = 64;
        if (numBZip2Threads > averageNumberOfBlocks)
          numBZip2Threads = (UInt32)averageNumberOfBlocks;
        if (numBZip2Threads > numThreads)
          numBZip2Threads = numThreads;
        oneMethodMain->AddProp_NumThreads(numBZip2Threads);
      }
      numThreads /= numBZip2Threads;
    }
    else if (method == NFileHeader::NCompressionMethod::kXz)
    {
      UInt32 numLzmaThreads = 1;
      int numXzThreads = oneMethodMain->Get_Xz_NumThreads(numLzmaThreads);
      if (numXzThreads < 0)
      {
        const UInt64 averageSize = numBytesToCompress / numFilesToCompress;
        const UInt64 blockSize = oneMethodMain->Get_Xz_BlockSize();
        UInt64 averageNumberOfBlocks = 1;
        if (blockSize != (UInt64)(Int64)-1)
          averageNumberOfBlocks = averageSize / blockSize + 1;
        UInt32 t = 256;
        if (t > averageNumberOfBlocks)
          t = (UInt32)averageNumberOfBlocks;
        t *= numLzmaThreads;
        if (t > numThreads)
          t = numThreads;
        oneMethodMain->AddProp_NumThreads(t);
        numXzThreads = (int)t;
      }
      numThreads /= (unsigned)numXzThreads;
    }
    else if (method == NFileHeader::NCompressionMethod::kLZMA)
    {
      // we suppose that default LZMA is 2 thread. So we don't change it
      UInt32 numLZMAThreads = oneMethodMain->Get_Lzma_NumThreads();
      numThreads /= numLZMAThreads;
    }
    }

    if (numThreads > numFilesToCompress)
      numThreads = (UInt32)numFilesToCompress;
    if (numThreads <= 1)
      mtMode = false;
  }

  // mtMode = true; // to test mtMode for seqMode

  if (!mtMode)
  #endif
    return Update2St(
        EXTERNAL_CODECS_LOC_VARS
        archive, inArchive,
        inputItems, updateItems, &options2, outSeqMode, comment, updateCallback, totalComplexity, opCallback);


  #ifndef _7ZIP_ST

  CObjectVector<CItemOut> items;

  CMtProgressMixer *mtProgressMixerSpec = new CMtProgressMixer;
  CMyComPtr<ICompressProgressInfo> progress = mtProgressMixerSpec;
  mtProgressMixerSpec->Create(updateCallback, true);

  CMtCompressProgressMixer mtCompressProgressMixer;
  mtCompressProgressMixer.Init(numThreads, mtProgressMixerSpec->RatioProgress);

  CMemBlockManagerMt memManager(kBlockSize);
  CMemRefs refs(&memManager);

  CMtSem mtSem;
  CThreads threads;
  mtSem.Head = -1;
  mtSem.Indexes.ClearAndSetSize(numThreads);
  {
    WRes wres = mtSem.Semaphore.Create(0, numThreads);
    if (wres != 0)
      return HRESULT_FROM_WIN32(wres);
  }

  CUIntVector threadIndices;  // list threads in order of updateItems

  {
    RINOK(memManager.AllocateSpaceAlways((size_t)numThreads * (kMemPerThread / kBlockSize)));
    for (i = 0; i < updateItems.Size(); i++)
      refs.Refs.Add(CMemBlocks2());

    for (i = 0; i < numThreads; i++)
    {
      threads.Threads.AddNew();
      // mtSem.Indexes[i] = -1; // actually we don't use these values
    }

    for (i = 0; i < numThreads; i++)
    {
      CThreadInfo &threadInfo = threads.Threads[i];
      threadInfo.SetOptions(options2);                ;
      #ifdef EXTERNAL_CODECS
      threadInfo.__externalCodecs = __externalCodecs;
      #endif
      RINOK(threadInfo.CreateEvents());
      threadInfo.OutStreamSpec = new COutMemStream(&memManager);
      RINOK(threadInfo.OutStreamSpec->CreateEvents(SYNC_WFMO(&memManager.Synchro)));
      threadInfo.OutStream = threadInfo.OutStreamSpec;
      threadInfo.IsFree = true;
      threadInfo.ProgressSpec = new CMtCompressProgress();
      threadInfo.Progress = threadInfo.ProgressSpec;
      threadInfo.ProgressSpec->Init(&mtCompressProgressMixer, i);
      threadInfo.InSeqMode = false;
      threadInfo.OutSeqMode = false;
      threadInfo.FileTime = 0;
      threadInfo.ExpectedDataSize = (UInt64)(Int64)-1;
      threadInfo.ThreadIndex = i;
      threadInfo.MtSem = &mtSem;
      RINOK(threadInfo.CreateThread());
    }
  }

  unsigned mtItemIndex = 0;
  unsigned itemIndex = 0;
  int lastRealStreamItemIndex = -1;

  
  while (itemIndex < updateItems.Size())
  {
    if (threadIndices.Size() < numThreads && mtItemIndex < updateItems.Size())
    {
      // we start ahead the threads for compressing
      // also we set refs.Refs[itemIndex].SeqMode that is used later
      // don't move that code block
      
      CUpdateItem &ui = updateItems[mtItemIndex++];
      if (!ui.NewData)
        continue;
      CItemEx itemEx;
      CItemOut item;
      
      if (ui.NewProps)
      {
        if (ui.IsDir)
          continue;
      }
      else
      {
        itemEx = inputItems[(unsigned)ui.IndexInArc];
        if (inArchive->ReadLocalItemAfterCdItemFull(itemEx) != S_OK)
          return E_NOTIMPL;
        (CItem &)item = itemEx;
        if (item.IsDir() != ui.IsDir)
          return E_NOTIMPL;
        if (ui.IsDir)
          continue;
      }
      
      CMyComPtr<ISequentialInStream> fileInStream;
      
      CMemBlocks2 &memRef2 = refs.Refs[mtItemIndex - 1];

      {
        NWindows::NSynchronization::CCriticalSectionLock lock(mtProgressMixerSpec->Mixer2->CriticalSection);
        HRESULT res = updateCallback->GetStream(ui.IndexInClient, &fileInStream);
        if (res == S_FALSE)
        {
          complexity += ui.Size;
          complexity += kLocalHeaderSize;
          mtProgressMixerSpec->Mixer2->SetProgressOffset_NoLock(complexity);
          RINOK(updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK));
          memRef2.Skip = true;
          continue;
        }
        RINOK(res);
        if (!fileInStream)
          return E_INVALIDARG;
        UpdatePropsFromStream(ui, fileInStream, updateCallback, totalComplexity);
        RINOK(updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK));
      }

      UInt32 k;
      for (k = 0; k < numThreads; k++)
        if (threads.Threads[k].IsFree)
          break;

      if (k == numThreads)
        return E_FAIL;
      {
        {
          CThreadInfo &threadInfo = threads.Threads[k];
          threadInfo.IsFree = false;
          threadInfo.InStream = fileInStream;

          bool inSeqMode = false;

          if (!inSeqMode)
          {
            CMyComPtr<IInStream> inStream2;
            fileInStream->QueryInterface(IID_IInStream, (void **)&inStream2);
            inSeqMode = (inStream2 == NULL);
          }
          memRef2.InSeqMode = inSeqMode;

          // !!!!! we must release ref before sending event
          // BUG was here in v4.43 and v4.44. It could change ref counter in two threads in same time
          fileInStream.Release();

          threadInfo.OutStreamSpec->Init();
          threadInfo.ProgressSpec->Reinit();
          
          threadInfo.UpdateIndex = mtItemIndex - 1;
          threadInfo.InSeqMode = inSeqMode;
          threadInfo.OutSeqMode = outSeqMode;
          threadInfo.FileTime = ui.Time; // FileTime is used for ZipCrypto only in seqMode
          threadInfo.ExpectedDataSize = ui.Size;

          threadInfo.CompressEvent.Set();
          
          threadIndices.Add(k);
        }
      }
 
      continue;
    }
    
    if (refs.Refs[itemIndex].Skip)
    {
      itemIndex++;
      continue;
    }

    const CUpdateItem &ui = updateItems[itemIndex];

    CItemEx itemEx;
    CItemOut item;
    
    if (!ui.NewProps || !ui.NewData)
    {
      itemEx = inputItems[(unsigned)ui.IndexInArc];
      if (inArchive->ReadLocalItemAfterCdItemFull(itemEx) != S_OK)
        return E_NOTIMPL;
      (CItem &)item = itemEx;
    }

    if (ui.NewData)
    {
      // bool isDir = ((ui.NewProps) ? ui.IsDir : item.IsDir());
      bool isDir = ui.IsDir;
      
      if (isDir)
      {
        WriteDirHeader(archive, &options, ui, item);
      }
      else
      {
        CMemBlocks2 &memRef = refs.Refs[itemIndex];
        
        if (memRef.Finished)
        {
          if (lastRealStreamItemIndex < (int)itemIndex)
            lastRealStreamItemIndex = (int)itemIndex;

          SetFileHeader(options, ui, memRef.CompressingResult.DescriptorMode, item);

          // the BUG was fixed in 9.26:
          // SetItemInfoFromCompressingResult must be after SetFileHeader
          // to write correct Size.

          SetItemInfoFromCompressingResult(memRef.CompressingResult,
              options.IsRealAesMode(), options.AesKeyMode, item);
          archive.WriteLocalHeader(item);
          // RINOK(updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK));
          CMyComPtr<ISequentialOutStream> outStream;
          archive.CreateStreamForCopying(outStream);
          memRef.WriteToStream(memManager.GetBlockSize(), outStream);
          archive.MoveCurPos(item.PackSize);
          memRef.FreeOpt(&memManager);
        }
        else
        {
          // current file was not finished

          if (lastRealStreamItemIndex < (int)itemIndex)
          {
            // LocalHeader was not written for current itemIndex still

            lastRealStreamItemIndex = (int)itemIndex;

            // thread was started before for that item already, and memRef.SeqMode was set

            CCompressingResult compressingResult;
            RINOK(compressor.Set_Pre_CompressionResult(
                memRef.InSeqMode, outSeqMode,
                ui.Size,
                compressingResult));

            memRef.PreDescriptorMode = compressingResult.DescriptorMode;
            SetFileHeader(options, ui, compressingResult.DescriptorMode, item);

            SetItemInfoFromCompressingResult(compressingResult, options.IsRealAesMode(), options.AesKeyMode, item);

            // file Size can be 64-bit !!!
            archive.WriteLocalHeader(item);
          }

          {
            CThreadInfo &thread = threads.Threads[threadIndices.Front()];
            if (!thread.OutStreamSpec->WasUnlockEventSent())
            {
              CMyComPtr<IOutStream> outStream;
              archive.CreateStreamForCompressing(outStream);
              thread.OutStreamSpec->SetOutStream(outStream);
              thread.OutStreamSpec->SetRealStreamMode();
            }
          }

          WRes wres = mtSem.Semaphore.Lock();
          if (wres != 0)
            return HRESULT_FROM_WIN32(wres);

          int ti = mtSem.GetFreeItem();
          if (ti < 0)
            return E_FAIL;

          CThreadInfo &threadInfo = threads.Threads[(unsigned)ti];
          threadInfo.InStream.Release();
          threadInfo.IsFree = true;
          RINOK(threadInfo.Result);

          unsigned t = 0;

          for (;;)
          {
            if (t == threadIndices.Size())
              return E_FAIL;
            if (threadIndices[t] == (unsigned)ti)
              break;
            t++;
          }
          threadIndices.Delete(t);
          
          if (t == 0)
          {
            // if thread for current file was finished.
            if (threadInfo.UpdateIndex != itemIndex)
              return E_FAIL;

            if (memRef.PreDescriptorMode != threadInfo.CompressingResult.DescriptorMode)
              return E_FAIL;

            RINOK(threadInfo.OutStreamSpec->WriteToRealStream());
            threadInfo.OutStreamSpec->ReleaseOutStream();
            SetFileHeader(options, ui, threadInfo.CompressingResult.DescriptorMode, item);
            SetItemInfoFromCompressingResult(threadInfo.CompressingResult,
                options.IsRealAesMode(), options.AesKeyMode, item);

            archive.WriteLocalHeader_Replace(item);
          }
          else
          {
            // it's not current file. So we must store information in array
            CMemBlocks2 &memRef2 = refs.Refs[threadInfo.UpdateIndex];
            threadInfo.OutStreamSpec->DetachData(memRef2);
            memRef2.CompressingResult = threadInfo.CompressingResult;
            // memRef2.SeqMode = threadInfo.SeqMode; // it was set before
            memRef2.Finished = true;
            continue;
          }
        }
      }
    }
    else
    {
      RINOK(UpdateItemOldData(archive, inArchive, itemEx, ui, item, progress, opCallback, complexity));
    }
 
    items.Add(item);
    complexity += kLocalHeaderSize;
    mtProgressMixerSpec->Mixer2->SetProgressOffset(complexity);
    itemIndex++;
  }
  
  RINOK(mtCompressProgressMixer.SetRatioInfo(0, NULL, NULL));

  archive.WriteCentralDir(items, comment);
  
  complexity += kCentralHeaderSize * updateItems.Size() + 1;
  mtProgressMixerSpec->Mixer2->SetProgressOffset(complexity);
  return mtCompressProgressMixer.SetRatioInfo(0, NULL, NULL);

  #endif
}


static const size_t kCacheBlockSize = (1 << 20);
static const size_t kCacheSize = (kCacheBlockSize << 2);
static const size_t kCacheMask = (kCacheSize - 1);

class CCacheOutStream:
  public IOutStream,
  public CMyUnknownImp
{
  CMyComPtr<IOutStream> _stream;
  CMyComPtr<ISequentialOutStream> _seqStream;
  Byte *_cache;
  UInt64 _virtPos;
  UInt64 _virtSize;
  UInt64 _phyPos;
  UInt64 _phySize; // <= _virtSize
  UInt64 _cachedPos; // (_cachedPos + _cachedSize) <= _virtSize
  size_t _cachedSize;

  HRESULT MyWrite(size_t size);
  HRESULT MyWriteBlock()
  {
    return MyWrite(kCacheBlockSize - ((size_t)_cachedPos & (kCacheBlockSize - 1)));
  }
  HRESULT FlushCache();
public:
  CCacheOutStream(): _cache(NULL) {}
  ~CCacheOutStream();
  bool Allocate();
  HRESULT Init(ISequentialOutStream *seqStream, IOutStream *stream);
  
  MY_UNKNOWN_IMP

  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition);
  STDMETHOD(SetSize)(UInt64 newSize);
};

bool CCacheOutStream::Allocate()
{
  if (!_cache)
    _cache = (Byte *)::MidAlloc(kCacheSize);
  return (_cache != NULL);
}

HRESULT CCacheOutStream::Init(ISequentialOutStream *seqStream, IOutStream *stream)
{
  _virtPos = 0;
  _phyPos = 0;
  _virtSize = 0;
  _seqStream = seqStream;
  _stream = stream;
  if (_stream)
  {
    RINOK(_stream->Seek(0, STREAM_SEEK_CUR, &_virtPos));
    RINOK(_stream->Seek(0, STREAM_SEEK_END, &_virtSize));
    RINOK(_stream->Seek((Int64)_virtPos, STREAM_SEEK_SET, &_virtPos));
  }
  _phyPos = _virtPos;
  _phySize = _virtSize;
  _cachedPos = 0;
  _cachedSize = 0;
  return S_OK;
}

HRESULT CCacheOutStream::MyWrite(size_t size)
{
  while (size != 0 && _cachedSize != 0)
  {
    if (_phyPos != _cachedPos)
    {
      if (!_stream)
        return E_FAIL;
      RINOK(_stream->Seek((Int64)_cachedPos, STREAM_SEEK_SET, &_phyPos));
    }
    size_t pos = (size_t)_cachedPos & kCacheMask;
    size_t curSize = MyMin(kCacheSize - pos, _cachedSize);
    curSize = MyMin(curSize, size);
    RINOK(WriteStream(_seqStream, _cache + pos, curSize));
    _phyPos += curSize;
    if (_phySize < _phyPos)
      _phySize = _phyPos;
    _cachedPos += curSize;
    _cachedSize -= curSize;
    size -= curSize;
  }
  return S_OK;
}

HRESULT CCacheOutStream::FlushCache()
{
  return MyWrite(_cachedSize);
}

CCacheOutStream::~CCacheOutStream()
{
  FlushCache();
  if (_stream)
  {
    if (_virtSize != _phySize)
      _stream->SetSize(_virtSize);
    if (_virtPos != _phyPos)
      _stream->Seek((Int64)_virtPos, STREAM_SEEK_SET, NULL);
  }
  ::MidFree(_cache);
}

STDMETHODIMP CCacheOutStream::Write(const void *data, UInt32 size, UInt32 *processedSize)
{
  if (processedSize)
    *processedSize = 0;
  if (size == 0)
    return S_OK;

  UInt64 zerosStart = _virtPos;
  if (_cachedSize != 0)
  {
    if (_virtPos < _cachedPos)
    {
      RINOK(FlushCache());
    }
    else
    {
      UInt64 cachedEnd = _cachedPos + _cachedSize;
      if (cachedEnd < _virtPos)
      {
        if (cachedEnd < _phySize)
        {
          RINOK(FlushCache());
        }
        else
          zerosStart = cachedEnd;
      }
    }
  }

  if (_cachedSize == 0 && _phySize < _virtPos)
    _cachedPos = zerosStart = _phySize;

  if (zerosStart != _virtPos)
  {
    // write zeros to [cachedEnd ... _virtPos)
    
    for (;;)
    {
      UInt64 cachedEnd = _cachedPos + _cachedSize;
      size_t endPos = (size_t)cachedEnd & kCacheMask;
      size_t curSize = kCacheSize - endPos;
      if (curSize > _virtPos - cachedEnd)
        curSize = (size_t)(_virtPos - cachedEnd);
      if (curSize == 0)
        break;
      while (curSize > (kCacheSize - _cachedSize))
      {
        RINOK(MyWriteBlock());
      }
      memset(_cache + endPos, 0, curSize);
      _cachedSize += curSize;
    }
  }

  if (_cachedSize == 0)
    _cachedPos = _virtPos;

  size_t pos = (size_t)_virtPos & kCacheMask;
  size = (UInt32)MyMin((size_t)size, kCacheSize - pos);
  UInt64 cachedEnd = _cachedPos + _cachedSize;
  if (_virtPos != cachedEnd) // _virtPos < cachedEnd
    size = (UInt32)MyMin((size_t)size, (size_t)(cachedEnd - _virtPos));
  else
  {
    // _virtPos == cachedEnd
    if (_cachedSize == kCacheSize)
    {
      RINOK(MyWriteBlock());
    }
    size_t startPos = (size_t)_cachedPos & kCacheMask;
    if (startPos > pos)
      size = (UInt32)MyMin((size_t)size, (size_t)(startPos - pos));
    _cachedSize += size;
  }
  memcpy(_cache + pos, data, size);
  if (processedSize)
    *processedSize = size;
  _virtPos += size;
  if (_virtSize < _virtPos)
    _virtSize = _virtPos;
  return S_OK;
}

STDMETHODIMP CCacheOutStream::Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition)
{
  switch (seekOrigin)
  {
    case STREAM_SEEK_SET: break;
    case STREAM_SEEK_CUR: offset += _virtPos; break;
    case STREAM_SEEK_END: offset += _virtSize; break;
    default: return STG_E_INVALIDFUNCTION;
  }
  if (offset < 0)
    return HRESULT_WIN32_ERROR_NEGATIVE_SEEK;
  _virtPos = (UInt64)offset;
  if (newPosition)
    *newPosition = (UInt64)offset;
  return S_OK;
}

STDMETHODIMP CCacheOutStream::SetSize(UInt64 newSize)
{
  _virtSize = newSize;
  if (newSize < _phySize)
  {
    if (!_stream)
      return E_NOTIMPL;
    RINOK(_stream->SetSize(newSize));
    _phySize = newSize;
  }
  if (newSize <= _cachedPos)
  {
    _cachedSize = 0;
    _cachedPos = newSize;
  }
  if (newSize < _cachedPos + _cachedSize)
    _cachedSize = (size_t)(newSize - _cachedPos);
  return S_OK;
}


HRESULT Update(
    DECL_EXTERNAL_CODECS_LOC_VARS
    const CObjectVector<CItemEx> &inputItems,
    CObjectVector<CUpdateItem> &updateItems,
    ISequentialOutStream *seqOutStream,
    CInArchive *inArchive, bool removeSfx,
    const CCompressionMethodMode &compressionMethodMode,
    IArchiveUpdateCallback *updateCallback)
{
  if (inArchive)
  {
    if (!inArchive->CanUpdate())
      return E_NOTIMPL;
  }


  CMyComPtr<IOutStream> outStream;
  bool outSeqMode;
  {
    CMyComPtr<IOutStream> outStreamReal;
    seqOutStream->QueryInterface(IID_IOutStream, (void **)&outStreamReal);
    if (!outStreamReal)
    {
      // return E_NOTIMPL;
    }

    if (inArchive)
    {
      if (!inArchive->IsMultiVol && inArchive->ArcInfo.Base > 0 && !removeSfx)
      {
        IInStream *baseStream = inArchive->GetBaseStream();
        RINOK(baseStream->Seek(0, STREAM_SEEK_SET, NULL));
        RINOK(NCompress::CopyStream_ExactSize(baseStream, seqOutStream, (UInt64)inArchive->ArcInfo.Base, NULL));
      }
    }

    CCacheOutStream *cacheStream = new CCacheOutStream();
    outStream = cacheStream;
    if (!cacheStream->Allocate())
      return E_OUTOFMEMORY;
    RINOK(cacheStream->Init(seqOutStream, outStreamReal));
    outSeqMode = (outStreamReal == NULL);
  }

  COutArchive outArchive;
  RINOK(outArchive.Create(outStream));

  if (inArchive)
  {
    if (!inArchive->IsMultiVol && (Int64)inArchive->ArcInfo.MarkerPos2 > inArchive->ArcInfo.Base)
    {
      IInStream *baseStream = inArchive->GetBaseStream();
      RINOK(baseStream->Seek(inArchive->ArcInfo.Base, STREAM_SEEK_SET, NULL));
      const UInt64 embStubSize = (UInt64)((Int64)inArchive->ArcInfo.MarkerPos2 - inArchive->ArcInfo.Base);
      RINOK(NCompress::CopyStream_ExactSize(baseStream, outStream, embStubSize, NULL));
      outArchive.MoveCurPos(embStubSize);
    }
  }

  return Update2(
      EXTERNAL_CODECS_LOC_VARS
      outArchive, inArchive,
      inputItems, updateItems,
      compressionMethodMode, outSeqMode,
      inArchive ? &inArchive->ArcInfo.Comment : NULL,
      updateCallback);
}

}}

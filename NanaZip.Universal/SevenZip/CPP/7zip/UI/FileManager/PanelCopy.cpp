/// PanelCopy.cpp

#include "StdAfx.h"

#include "../Common/ZipRegistry.h"

#include "../GUI/HashGUI.h"

#include "FSFolder.h"
#include "ExtractCallback.h"
#include "LangUtils.h"
#include "Panel.h"
#include "UpdateCallback100.h"

#include "resource.h"


class CPanelCopyThread: public CProgressThreadVirt
{
  bool ResultsWereShown;
  bool NeedShowRes;

  HRESULT ProcessVirt() Z7_override;
  virtual void ProcessWasFinished_GuiVirt() Z7_override;
public:
  const CCopyToOptions *options;
  const UStringVector *CopyFrom_Paths;
  CMyComPtr<IFolderOperations> FolderOperations;
  CRecordVector<UInt32> Indices;
  CExtractCallbackImp *ExtractCallbackSpec;
  CMyComPtr<IFolderOperationsExtractCallback> ExtractCallback;

  CHashBundle Hash;
  // UString FirstFilePath;

  // HRESULT Result2;

  void ShowFinalResults(HWND hwnd);

  CPanelCopyThread():
    ResultsWereShown(false),
    NeedShowRes(false),
    CopyFrom_Paths(NULL)
    // , Result2(E_FAIL)
    {}
};

void CPanelCopyThread::ShowFinalResults(HWND hwnd)
{
  if (NeedShowRes)
  if (!ResultsWereShown)
  {
    ResultsWereShown = true;
    ShowHashResults(Hash, hwnd);
  }
}

void CPanelCopyThread::ProcessWasFinished_GuiVirt()
{
  ShowFinalResults(*this);
}

HRESULT CPanelCopyThread::ProcessVirt()
{
  /*
  CMyComPtr<IFolderSetReplaceAltStreamCharsMode> iReplace;
  FolderOperations.QueryInterface(IID_IFolderSetReplaceAltStreamCharsMode, &iReplace);
  if (iReplace)
  {
    RINOK(iReplace->SetReplaceAltStreamCharsMode(ReplaceAltStreamChars ? 1 : 0));
  }
  */

  HRESULT result2;

  if (FolderOperations)
  {
    {
      CMyComPtr<IFolderSetZoneIdMode> setZoneMode;
      FolderOperations.QueryInterface(IID_IFolderSetZoneIdMode, &setZoneMode);
      if (setZoneMode)
      {
        RINOK(setZoneMode->SetZoneIdMode(options->ZoneIdMode))
      }
    }
    {
      CMyComPtr<IFolderSetZoneIdFile> setZoneFile;
      FolderOperations.QueryInterface(IID_IFolderSetZoneIdFile, &setZoneFile);
      if (setZoneFile)
      {
        RINOK(setZoneFile->SetZoneIdFile(options->ZoneBuf, (UInt32)options->ZoneBuf.Size()))
      }
    }
  }

  if (CopyFrom_Paths)
  {
    result2 = NFsFolder::CopyFileSystemItems(
        *CopyFrom_Paths,
        us2fs(options->folder),
        options->moveMode,
        (IFolderOperationsExtractCallback *)ExtractCallbackSpec);
  }
  else if (options->testMode)
  {
    CMyComPtr<IArchiveFolder> archiveFolder;
    FolderOperations.QueryInterface(IID_IArchiveFolder, &archiveFolder);
    if (!archiveFolder)
      return E_NOTIMPL;
    CMyComPtr<IFolderArchiveExtractCallback> extractCallback2;
    RINOK(ExtractCallback.QueryInterface(IID_IFolderArchiveExtractCallback, &extractCallback2))
    NExtract::NPathMode::EEnum pathMode =
        NExtract::NPathMode::kCurPaths;
        // NExtract::NPathMode::kFullPathnames;
    result2 = archiveFolder->Extract(Indices.ConstData(), Indices.Size(),
        BoolToInt(options->includeAltStreams),
        BoolToInt(options->replaceAltStreamChars),
        pathMode, NExtract::NOverwriteMode::kAsk,
        options->folder, BoolToInt(true), extractCallback2);
  }
  else
    result2 = FolderOperations->CopyTo(
      BoolToInt(options->moveMode),
      Indices.ConstData(), Indices.Size(),
      BoolToInt(options->includeAltStreams),
      BoolToInt(options->replaceAltStreamChars),
      options->folder, ExtractCallback);

  if (result2 == S_OK && !ExtractCallbackSpec->ThereAreMessageErrors)
  {
    if (!options->hashMethods.IsEmpty())
      NeedShowRes = true;
    else if (options->testMode)
    {
      CProgressMessageBoxPair &pair = GetMessagePair(false); // GetMessagePair(ExtractCallbackSpec->Hash.NumErrors != 0);
      AddHashBundleRes(pair.Message, Hash);
    }
  }

  return result2;
}


/*
#ifdef Z7_EXTERNAL_CODECS

static void ThrowException_if_Error(HRESULT res)
{
  if (res != S_OK)
    throw CSystemException(res);
}

#endif
*/

void CPanel::Get_ZoneId_Stream_from_ParentFolders(CByteBuffer &buf)
{
  // we suppose that ZoneId of top parent has priority over ZoneId from childs.
  FOR_VECTOR (i, _parentFolders)
  {
    // _parentFolders[0] = is top level archive
    // _parentFolders[1 ... ].isVirtual == true is possible
    //           if extracted size meets size conditions derived from g_RAM_Size.
    const CFolderLink &fl = _parentFolders[i];
    if (fl.IsVirtual)
    {
      if (fl.ZoneBuf.Size() != 0)
      {
        buf = fl.ZoneBuf;
        return;
      }
    }
    else if (!fl.FilePath.IsEmpty())
    {
      ReadZoneFile_Of_BaseFile(fl.FilePath, buf);
      if (buf.Size() != 0)
        return;
    }
  }
}

HRESULT CPanel::CopyTo(CCopyToOptions &options,
    const CRecordVector<UInt32> &indices,
    UStringVector *messages,
    bool &usePassword, UString &password,
    const UStringVector *filePaths)
{
  if (options.NeedRegistryZone && !options.testMode)
  {
    CContextMenuInfo ci;
    ci.Load();
    if (ci.WriteZone != (UInt32)(Int32)-1)
      options.ZoneIdMode = (NExtract::NZoneIdMode::EEnum)(int)(Int32)ci.WriteZone;
  }

  if (options.ZoneBuf.Size() == 0
      && options.ZoneIdMode != NExtract::NZoneIdMode::kNone)
    Get_ZoneId_Stream_from_ParentFolders(options.ZoneBuf);

  if (IsHashFolder())
  {
    if (!options.testMode)
      return E_NOTIMPL;
  }

  if (!filePaths)
  if (!_folderOperations)
  {
    const UString errorMessage = LangString(IDS_OPERATION_IS_NOT_SUPPORTED);
    if (options.showErrorMessages)
      MessageBox_Error(errorMessage);
    else if (messages)
      messages->Add(errorMessage);
    return E_FAIL;
  }

  HRESULT res = S_OK;

  {
  /*
  #ifdef Z7_EXTERNAL_CODECS
  CExternalCodecs g_ExternalCodecs;
  #endif
  */
  /* extracter.Hash uses g_ExternalCodecs
     extracter must be declared after g_ExternalCodecs for correct destructor order !!! */

  CPanelCopyThread extracter;

  extracter.ExtractCallbackSpec = new CExtractCallbackImp;
  extracter.ExtractCallback = extracter.ExtractCallbackSpec;
  extracter.ExtractCallbackSpec->Src_Is_IO_FS_Folder =
      IsFSFolder() || IsAltStreamsFolder();
      // options.src_Is_IO_FS_Folder;
  extracter.options = &options;
  extracter.ExtractCallbackSpec->ProgressDialog = &extracter;
  extracter.CompressingMode = false;

  extracter.ExtractCallbackSpec->StreamMode = options.streamMode;


  if (indices.Size() == 1)
  {
    extracter.Hash.FirstFileName = GetItemRelPath(indices[0]);
    extracter.Hash.MainName = extracter.Hash.FirstFileName;
  }

  if (options.VirtFileSystemSpec)
  {
    extracter.ExtractCallbackSpec->VirtFileSystem = options.VirtFileSystemSpec;
    extracter.ExtractCallbackSpec->VirtFileSystemSpec = options.VirtFileSystemSpec;
  }
  extracter.ExtractCallbackSpec->ProcessAltStreams = options.includeAltStreams;

  if (!options.hashMethods.IsEmpty())
  {
    /* this code is used when we call CRC calculation for files in side archive
       But new code uses global codecs so we don't need to call LoadGlobalCodecs again */

    /*
    #ifdef Z7_EXTERNAL_CODECS
    ThrowException_if_Error(LoadGlobalCodecs());
    #endif
    */

    extracter.Hash.SetMethods(EXTERNAL_CODECS_VARS_G options.hashMethods);
    extracter.ExtractCallbackSpec->SetHashMethods(&extracter.Hash);
  }
  else if (options.testMode)
  {
    extracter.ExtractCallbackSpec->SetHashCalc(&extracter.Hash);
  }

  // extracter.Hash.Init();

  UString title;
  {
    UInt32 titleID = IDS_COPYING;
    if (options.moveMode)
      titleID = IDS_MOVING;
    else if (!options.hashMethods.IsEmpty() && options.streamMode)
    {
      titleID = IDS_CHECKSUM_CALCULATING;
      if (options.hashMethods.Size() == 1)
      {
        const UString &s = options.hashMethods[0];
        if (!s.IsEqualTo("*"))
          title = s;
      }
    }
    else if (options.testMode)
      titleID = IDS_PROGRESS_TESTING;

    if (title.IsEmpty())
      title = LangString(titleID);
  }

  // **************** NanaZip Modification Start ****************
  // const UString progressWindowTitle ("7-Zip"); // LangString(IDS_APP_TITLE);
  const UString progressWindowTitle ("NanaZip"); // LangString(IDS_APP_TITLE);
  // **************** NanaZip Modification End ****************

  extracter.MainWindow = GetParent();
  extracter.MainTitle = progressWindowTitle;
  extracter.MainAddTitle = title + L' ';

  extracter.ExtractCallbackSpec->OverwriteMode = NExtract::NOverwriteMode::kAsk;
  extracter.ExtractCallbackSpec->Init();

  extracter.CopyFrom_Paths = filePaths;
  if (!filePaths)
  {
    extracter.Indices = indices;
    extracter.FolderOperations = _folderOperations;
  }

  extracter.ExtractCallbackSpec->PasswordIsDefined = usePassword;
  extracter.ExtractCallbackSpec->Password = password;

  RINOK(extracter.Create(title, GetParent()))


  if (messages)
    *messages = extracter.Sync.Messages;

  // res = extracter.Result2;
  res = extracter.Result;

  if (res == S_OK && extracter.ExtractCallbackSpec->IsOK())
  {
    usePassword = extracter.ExtractCallbackSpec->PasswordIsDefined;
    password = extracter.ExtractCallbackSpec->Password;
  }

  extracter.ShowFinalResults(_window);

  }

  RefreshTitleAlways();
  return res;
}


struct CThreadUpdate
{
  CMyComPtr<IFolderOperations> FolderOperations;
  UString FolderPrefix;
  UStringVector FileNames;
  CRecordVector<const wchar_t *> FileNamePointers;
  CProgressDialog ProgressDialog;
  CMyComPtr<IFolderArchiveUpdateCallback> UpdateCallback;
  CUpdateCallback100Imp *UpdateCallbackSpec;
  HRESULT Result;
  bool MoveMode;

  void Process()
  {
    try
    {
      CProgressCloser closer(ProgressDialog);
      Result = FolderOperations->CopyFrom(
        MoveMode,
        FolderPrefix,
        FileNamePointers.ConstData(),
        FileNamePointers.Size(),
        UpdateCallback);
    }
    catch(...) { Result = E_FAIL; }
  }
  static THREAD_FUNC_DECL MyThreadFunction(void *param)
  {
    ((CThreadUpdate *)param)->Process();
    return 0;
  }
};


HRESULT CPanel::CopyFrom(bool moveMode, const UString &folderPrefix, const UStringVector &filePaths,
    bool showErrorMessages, UStringVector *messages)
{
  if (IsHashFolder())
  {
    if (moveMode)
      return E_NOTIMPL;
  }
  // CDisableNotify disableNotify(*this);

  HRESULT res;
  if (!_folderOperations)
    res = E_NOINTERFACE;
  else
  {
  CThreadUpdate updater;
  updater.MoveMode = moveMode;
  updater.UpdateCallbackSpec = new CUpdateCallback100Imp;
  updater.UpdateCallback = updater.UpdateCallbackSpec;
  updater.UpdateCallbackSpec->Init();

  updater.UpdateCallbackSpec->ProgressDialog = &updater.ProgressDialog;

  const UString title = LangString(IDS_COPYING);
  // **************** NanaZip Modification Start ****************
  // const UString progressWindowTitle ("7-Zip"); // LangString(IDS_APP_TITLE);
  const UString progressWindowTitle ("NanaZip"); // LangString(IDS_APP_TITLE);
  // **************** NanaZip Modification End ****************

  updater.ProgressDialog.MainWindow = GetParent();
  updater.ProgressDialog.MainTitle = progressWindowTitle;
  updater.ProgressDialog.MainAddTitle = title + L' ';

  {
    if (!_parentFolders.IsEmpty())
    {
      const CFolderLink &fl = _parentFolders.Back();
      updater.UpdateCallbackSpec->PasswordIsDefined = fl.UsePassword;
      updater.UpdateCallbackSpec->Password = fl.Password;
    }
  }

  updater.FolderOperations = _folderOperations;
  updater.FolderPrefix = folderPrefix;
  updater.FileNames.ClearAndReserve(filePaths.Size());
  unsigned i;
  for (i = 0; i < filePaths.Size(); i++)
    updater.FileNames.AddInReserved(filePaths[i]);
  updater.FileNamePointers.ClearAndReserve(updater.FileNames.Size());
  for (i = 0; i < updater.FileNames.Size(); i++)
    updater.FileNamePointers.AddInReserved(updater.FileNames[i]);

  {
    NWindows::CThread thread;
    const WRes wres = thread.Create(CThreadUpdate::MyThreadFunction, &updater);
    if (wres != 0)
      return HRESULT_FROM_WIN32(wres);
    updater.ProgressDialog.Create(title, thread, GetParent());
  }

  if (messages)
    *messages = updater.ProgressDialog.Sync.Messages;

  res = updater.Result;
  }

  if (res == E_NOINTERFACE)
  {
    const UString errorMessage = LangString(IDS_OPERATION_IS_NOT_SUPPORTED);
    if (showErrorMessages)
      MessageBox_Error(errorMessage);
    else if (messages)
      messages->Add(errorMessage);
    return E_ABORT;
  }

  RefreshTitleAlways();
  return res;
}

void CPanel::CopyFromNoAsk(bool moveMode, const UStringVector &filePaths)
{
  CDisableTimerProcessing disableTimerProcessing(*this);

  CSelectedState srcSelState;
  SaveSelectedState(srcSelState);

  CDisableNotify disableNotify(*this);

  const HRESULT result = CopyFrom(moveMode, L"", filePaths, true, NULL);

  if (result != S_OK)
  {
    disableNotify.Restore();
    // For Password:
    SetFocusToList();
    if (result != E_ABORT)
      MessageBox_Error_HRESULT(result);
    return;
  }

  RefreshListCtrl(srcSelState);

  disableNotify.Restore();
  SetFocusToList();
}

// App.cpp

#include "StdAfx.h"

#include "resource.h"
#include "OverwriteDialogRes.h"

#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariantConv.h"

/*
#include "Windows/COM.h"
#include "Windows/Error.h"
#include "Windows/FileDir.h"

#include "Windows/PropVariant.h"
#include "Windows/Thread.h"
*/

#include "App.h"
#include "CopyDialog.h"
#include "ExtractCallback.h"
#include "FormatUtils.h"
#include "IFolder.h"
#include "LangUtils.h"
#include "MyLoadMenu.h"
#include "RegistryUtils.h"
#include "ViewSettings.h"

#include "PropertyNameRes.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;
using namespace NFind;
using namespace NName;

extern DWORD g_ComCtl32Version;
extern HINSTANCE g_hInstance;

#define kTempDirPrefix FTEXT("7zE")

void CPanelCallbackImp::OnTab()
{
  if (g_App.NumPanels != 1)
    _app->Panels[1 - _index].SetFocusToList();
  _app->RefreshTitle();
}

void CPanelCallbackImp::SetFocusToPath(unsigned index)
{
  int newPanelIndex = index;
  if (g_App.NumPanels == 1)
    newPanelIndex = g_App.LastFocusedPanel;
  _app->RefreshTitle();
  _app->Panels[newPanelIndex]._headerComboBox.SetFocus();
  _app->Panels[newPanelIndex]._headerComboBox.ShowDropDown();
}


void CPanelCallbackImp::OnCopy(bool move, bool copyToSame) { _app->OnCopy(move, copyToSame, _index); }
void CPanelCallbackImp::OnSetSameFolder() { _app->OnSetSameFolder(_index); }
void CPanelCallbackImp::OnSetSubFolder()  { _app->OnSetSubFolder(_index); }
void CPanelCallbackImp::PanelWasFocused() { _app->SetFocusedPanel(_index); _app->RefreshTitlePanel(_index); }
void CPanelCallbackImp::DragBegin() { _app->DragBegin(_index); }
void CPanelCallbackImp::DragEnd() { _app->DragEnd(); }
void CPanelCallbackImp::RefreshTitle(bool always) { _app->RefreshTitlePanel(_index, always); }

void CApp::ReloadLang()
{
  LangString(IDS_N_SELECTED_ITEMS, LangString_N_SELECTED_ITEMS);
}

void CApp::SetListSettings()
{
  CFmSettings st;
  st.Load();

  ShowSystemMenu = st.ShowSystemMenu;

  DWORD extendedStyle = LVS_EX_HEADERDRAGDROP;
  if (st.FullRow)
    extendedStyle |= LVS_EX_FULLROWSELECT;
  if (st.ShowGrid)
    extendedStyle |= LVS_EX_GRIDLINES;
  
  if (st.SingleClick)
  {
    extendedStyle |= LVS_EX_ONECLICKACTIVATE | LVS_EX_TRACKSELECT;
    /*
    if (ReadUnderline())
      extendedStyle |= LVS_EX_UNDERLINEHOT;
    */
  }

  for (unsigned i = 0; i < kNumPanelsMax; i++)
  {
    CPanel &panel = Panels[i];
    panel._mySelectMode = st.AlternativeSelection;
    panel._showDots = st.ShowDots;
    panel._showRealFileIcons = st.ShowRealFileIcons;
    panel._exStyle = extendedStyle;

    DWORD style = (DWORD)panel._listView.GetStyle();
    if (st.AlternativeSelection)
      style |= LVS_SINGLESEL;
    else
      style &= ~LVS_SINGLESEL;
    panel._listView.SetStyle(style);
    panel.SetExtendedStyle();
  }
}

#ifndef ILC_COLOR32
#define ILC_COLOR32 0x0020
#endif

HRESULT CApp::CreateOnePanel(int panelIndex, const UString &mainPath, const UString &arcFormat,
    bool needOpenArc,
    COpenResult &openRes)
{
  if (Panels[panelIndex].PanelCreated)
    return S_OK;
  
  m_PanelCallbackImp[panelIndex].Init(this, panelIndex);
  
  UString path;
  if (mainPath.IsEmpty())
  {
    if (!::ReadPanelPath(panelIndex, path))
      path.Empty();
  }
  else
    path = mainPath;
  
  int id = 1000 + 100 * panelIndex;

  return Panels[panelIndex].Create(_window, _window,
      id, path, arcFormat, &m_PanelCallbackImp[panelIndex], &AppState,
      needOpenArc,
      openRes);
}


static void CreateToolbar(HWND parent,
    NControl::CImageList &imageList,
    NControl::CToolBar &toolBar,
    bool largeButtons)
{
  toolBar.Attach(::CreateWindowEx(0, TOOLBARCLASSNAME, NULL, 0
      | WS_CHILD
      | WS_VISIBLE
      | TBSTYLE_FLAT
      | TBSTYLE_TOOLTIPS
      | TBSTYLE_WRAPABLE
      // | TBSTYLE_AUTOSIZE
      // | CCS_NORESIZE
      #ifdef UNDER_CE
      | CCS_NODIVIDER
      | CCS_NOPARENTALIGN
      #endif
      ,0,0,0,0, parent, NULL, g_hInstance, NULL));

  // TB_BUTTONSTRUCTSIZE message, which is required for
  // backward compatibility.
  toolBar.ButtonStructSize();

  imageList.Create(
      largeButtons ? 48: 24,
      largeButtons ? 36: 24,
      ILC_MASK | ILC_COLOR32, 0, 0);
  toolBar.SetImageList(0, imageList);
}


struct CButtonInfo
{
  int CommandID;
  UINT BitmapResID;
  UINT Bitmap2ResID;
  UINT StringResID;

  UString GetText() const { return LangString(StringResID); }
};

static const CButtonInfo g_StandardButtons[] =
{
  { IDM_COPY_TO,    IDB_COPY,   IDB_COPY2,   IDS_BUTTON_COPY },
  { IDM_MOVE_TO,    IDB_MOVE,   IDB_MOVE2,   IDS_BUTTON_MOVE },
  { IDM_DELETE,     IDB_DELETE, IDB_DELETE2, IDS_BUTTON_DELETE } ,
  { IDM_PROPERTIES, IDB_INFO,   IDB_INFO2,   IDS_BUTTON_INFO }
};

static const CButtonInfo g_ArchiveButtons[] =
{
  { kMenuCmdID_Toolbar_Add,     IDB_ADD,     IDB_ADD2,     IDS_ADD },
  { kMenuCmdID_Toolbar_Extract, IDB_EXTRACT, IDB_EXTRACT2, IDS_EXTRACT },
  { kMenuCmdID_Toolbar_Test,    IDB_TEST,    IDB_TEST2,    IDS_TEST }
};

static bool SetButtonText(int commandID, const CButtonInfo *buttons, unsigned numButtons, UString &s)
{
  for (unsigned i = 0; i < numButtons; i++)
  {
    const CButtonInfo &b = buttons[i];
    if (b.CommandID == commandID)
    {
      s = b.GetText();
      return true;
    }
  }
  return false;
}

static void SetButtonText(int commandID, UString &s)
{
  if (SetButtonText(commandID, g_StandardButtons, ARRAY_SIZE(g_StandardButtons), s))
    return;
  SetButtonText(commandID, g_ArchiveButtons, ARRAY_SIZE(g_ArchiveButtons), s);
}

static void AddButton(
    NControl::CImageList &imageList,
    NControl::CToolBar &toolBar,
    const CButtonInfo &butInfo, bool showText, bool large)
{
  TBBUTTON but;
  but.iBitmap = 0;
  but.idCommand = butInfo.CommandID;
  but.fsState = TBSTATE_ENABLED;
  but.fsStyle = TBSTYLE_BUTTON;
  but.dwData = 0;

  UString s = butInfo.GetText();
  but.iString = 0;
  if (showText)
    but.iString = (INT_PTR)(LPCWSTR)s;

  but.iBitmap = imageList.GetImageCount();
  HBITMAP b = ::LoadBitmap(g_hInstance,
      large ?
      MAKEINTRESOURCE(butInfo.BitmapResID):
      MAKEINTRESOURCE(butInfo.Bitmap2ResID));
  if (b != 0)
  {
    imageList.AddMasked(b, RGB(255, 0, 255));
    ::DeleteObject(b);
  }
  #ifdef _UNICODE
  toolBar.AddButton(1, &but);
  #else
  toolBar.AddButtonW(1, &but);
  #endif
}

void CApp::ReloadToolbars()
{
  _buttonsImageList.Destroy();
  _toolBar.Destroy();


  if (ShowArchiveToolbar || ShowStandardToolbar)
  {
    CreateToolbar(_window, _buttonsImageList, _toolBar, LargeButtons);
    unsigned i;
    if (ShowArchiveToolbar)
      for (i = 0; i < ARRAY_SIZE(g_ArchiveButtons); i++)
        AddButton(_buttonsImageList, _toolBar, g_ArchiveButtons[i], ShowButtonsLables, LargeButtons);
    if (ShowStandardToolbar)
      for (i = 0; i < ARRAY_SIZE(g_StandardButtons); i++)
        AddButton(_buttonsImageList, _toolBar, g_StandardButtons[i], ShowButtonsLables, LargeButtons);

    _toolBar.AutoSize();
  }
}

void CApp::SaveToolbarChanges()
{
  SaveToolbar();
  ReloadToolbars();
  MoveSubWindows();
}


HRESULT CApp::Create(HWND hwnd, const UString &mainPath, const UString &arcFormat, int xSizes[2], bool needOpenArc, COpenResult &openRes)
{
  _window.Attach(hwnd);

  #ifdef UNDER_CE
  _commandBar.Create(g_hInstance, hwnd, 1);
  #endif

  MyLoadMenu();
  
  #ifdef UNDER_CE
  _commandBar.AutoSize();
  #endif

  ReadToolbar();
  ReloadToolbars();

  unsigned i;
  for (i = 0; i < kNumPanelsMax; i++)
    Panels[i].PanelCreated = false;

  AppState.Read();
  
  SetListSettings();

  if (LastFocusedPanel >= kNumPanelsMax)
    LastFocusedPanel = 0;
  // ShowDeletedFiles = Read_ShowDeleted();

  CListMode listMode;
  listMode.Read();
  
  for (i = 0; i < kNumPanelsMax; i++)
  {
    CPanel &panel = Panels[i];
    panel._ListViewMode = listMode.Panels[i];
    panel._xSize = xSizes[i];
    panel._flatModeForArc = ReadFlatView(i);
  }
  
  for (i = 0; i < kNumPanelsMax; i++)
  {
    unsigned panelIndex = i;
    if (needOpenArc && LastFocusedPanel == 1)
      panelIndex = 1 - i;

    bool isMainPanel = (panelIndex == LastFocusedPanel);

    if (NumPanels > 1 || isMainPanel)
    {
      if (NumPanels == 1)
        Panels[panelIndex]._xSize = xSizes[0] + xSizes[1];
      
      COpenResult openRes2;
      UString path;
      if (isMainPanel)
        path = mainPath;
      
      RINOK(CreateOnePanel(panelIndex, path, arcFormat,
          isMainPanel && needOpenArc,
          *(isMainPanel ? &openRes : &openRes2)));
      
      if (isMainPanel)
      {
        if (needOpenArc && !openRes.ArchiveIsOpened)
          return S_OK;
      }
    }
  }
  
  SetFocusedPanel(LastFocusedPanel);
  Panels[LastFocusedPanel].SetFocusToList();
  return S_OK;
}


HRESULT CApp::SwitchOnOffOnePanel()
{
  if (NumPanels == 1)
  {
    NumPanels++;
    COpenResult openRes;
    RINOK(CreateOnePanel(1 - LastFocusedPanel, UString(), UString(),
        false, // needOpenArc
        openRes));
    Panels[1 - LastFocusedPanel].Enable(true);
    Panels[1 - LastFocusedPanel].Show(SW_SHOWNORMAL);
  }
  else
  {
    NumPanels--;
    Panels[1 - LastFocusedPanel].Enable(false);
    Panels[1 - LastFocusedPanel].Show(SW_HIDE);
  }
  MoveSubWindows();
  return S_OK;
}

void CApp::Save()
{
  AppState.Save();
  CListMode listMode;
  
  for (unsigned i = 0; i < kNumPanelsMax; i++)
  {
    const CPanel &panel = Panels[i];
    UString path;
    if (panel._parentFolders.IsEmpty())
      path = panel._currentFolderPrefix;
    else
      path = panel._parentFolders[0].ParentFolderPath;
      // GetFolderPath(panel._parentFolders[0].ParentFolder);
    SavePanelPath(i, path);
    listMode.Panels[i] = panel.GetListViewMode();
    SaveFlatView(i, panel._flatModeForArc);
  }
  
  listMode.Save();
  // Save_ShowDeleted(ShowDeletedFiles);
}

void CApp::Release()
{
  // It's for unloading COM dll's: don't change it.
  for (unsigned i = 0; i < kNumPanelsMax; i++)
    Panels[i].Release();
}

// reduces path to part that exists on disk (or root prefix of path)
// output path is normalized (with WCHAR_PATH_SEPARATOR)
static void Reduce_Path_To_RealFileSystemPath(UString &path)
{
  unsigned prefixSize = GetRootPrefixSize(path);

  while (!path.IsEmpty())
  {
    if (NFind::DoesDirExist_FollowLink(us2fs(path)))
    {
      NName::NormalizeDirPathPrefix(path);
      break;
    }
    int pos = path.ReverseFind_PathSepar();
    if (pos < 0)
    {
      path.Empty();
      break;
    }
    path.DeleteFrom((unsigned)(pos + 1));
    if ((unsigned)pos + 1 == prefixSize)
      break;
    path.DeleteFrom((unsigned)pos);
  }
}

// returns: true, if such dir exists or is root
/*
static bool CheckFolderPath(const UString &path)
{
  UString pathReduced = path;
  Reduce_Path_To_RealFileSystemPath(pathReduced);
  return (pathReduced == path);
}
*/

extern UString ConvertSizeToString(UInt64 value);

static void AddSizeValue(UString &s, UInt64 size)
{
  s += MyFormatNew(IDS_FILE_SIZE, ConvertSizeToString(size));
}

static void AddValuePair1(UString &s, UINT resourceID, UInt64 size)
{
  AddLangString(s, resourceID);
  s += ": ";
  AddSizeValue(s, size);
  s.Add_LF();
}

void AddValuePair2(UString &s, UINT resourceID, UInt64 num, UInt64 size);
void AddValuePair2(UString &s, UINT resourceID, UInt64 num, UInt64 size)
{
  if (num == 0)
    return;
  AddLangString(s, resourceID);
  s += ": ";
  s += ConvertSizeToString(num);

  if (size != (UInt64)(Int64)-1)
  {
    s += "    ( ";
    AddSizeValue(s, size);
    s += " )";
  }
  s.Add_LF();
}

static void AddPropValueToSum(IFolderFolder *folder, int index, PROPID propID, UInt64 &sum)
{
  if (sum == (UInt64)(Int64)-1)
    return;
  NCOM::CPropVariant prop;
  folder->GetProperty(index, propID, &prop);
  UInt64 val = 0;
  if (ConvertPropVariantToUInt64(prop, val))
    sum += val;
  else
    sum = (UInt64)(Int64)-1;
}

UString CPanel::GetItemsInfoString(const CRecordVector<UInt32> &indices)
{
  UString info;
  UInt64 numDirs, numFiles, filesSize, foldersSize;
  numDirs = numFiles = filesSize = foldersSize = 0;
  
  unsigned i;
  for (i = 0; i < indices.Size(); i++)
  {
    int index = indices[i];
    if (IsItem_Folder(index))
    {
      AddPropValueToSum(_folder, index, kpidSize, foldersSize);
      numDirs++;
    }
    else
    {
      AddPropValueToSum(_folder, index, kpidSize, filesSize);
      numFiles++;
    }
  }

  AddValuePair2(info, IDS_PROP_FOLDERS, numDirs, foldersSize);
  AddValuePair2(info, IDS_PROP_FILES, numFiles, filesSize);
  int numDefined = ((foldersSize != (UInt64)(Int64)-1) && foldersSize != 0) ? 1: 0;
  numDefined += ((filesSize != (UInt64)(Int64)-1) && filesSize != 0) ? 1: 0;
  if (numDefined == 2)
    AddValuePair1(info, IDS_PROP_SIZE, filesSize + foldersSize);
  
  info.Add_LF();
  info += _currentFolderPrefix;
  
  for (i = 0; i < indices.Size() && (int)i < (int)kCopyDialog_NumInfoLines - 6; i++)
  {
    info.Add_LF();
    info += "  ";
    int index = indices[i];
    info += GetItemRelPath(index);
    if (IsItem_Folder(index))
      info.Add_PathSepar();
  }
  if (i != indices.Size())
  {
    info.Add_LF();
    info += "  ...";
  }
  return info;
}

bool IsCorrectFsName(const UString &name);



/* Returns true, if path is path that can be used as path for File System functions
*/

/*
static bool IsFsPath(const FString &path)
{
  if (!IsAbsolutePath(path))
    return false;
  unsigned prefixSize = GetRootPrefixSize(path);
}
*/

void CApp::OnCopy(bool move, bool copyToSame, int srcPanelIndex)
{
  unsigned destPanelIndex = (NumPanels <= 1) ? srcPanelIndex : (1 - srcPanelIndex);
  CPanel &srcPanel = Panels[srcPanelIndex];
  CPanel &destPanel = Panels[destPanelIndex];

  CPanel::CDisableTimerProcessing disableTimerProcessing1(destPanel);
  CPanel::CDisableTimerProcessing disableTimerProcessing2(srcPanel);

  if (move)
  {
    if (!srcPanel.CheckBeforeUpdate(IDS_MOVE))
      return;
  }
  else if (!srcPanel.DoesItSupportOperations())
  {
    srcPanel.MessageBox_Error_UnsupportOperation();
    return;
  }

  CRecordVector<UInt32> indices;
  UString destPath;
  bool useDestPanel = false;

  {
    if (copyToSame)
    {
      int focusedItem = srcPanel._listView.GetFocusedItem();
      if (focusedItem < 0)
        return;
      int realIndex = srcPanel.GetRealItemIndex(focusedItem);
      if (realIndex == kParentIndex)
        return;
      indices.Add(realIndex);
      destPath = srcPanel.GetItemName(realIndex);
    }
    else
    {
      srcPanel.GetOperatedIndicesSmart(indices);
      if (indices.Size() == 0)
        return;
      destPath = destPanel.GetFsPath();
      if (NumPanels == 1)
        Reduce_Path_To_RealFileSystemPath(destPath);
    }
  }
  
  UStringVector copyFolders;
  ReadCopyHistory(copyFolders);
  
  bool useFullItemPaths = srcPanel.Is_IO_FS_Folder(); // maybe we need flat also here ??

  {
    CCopyDialog copyDialog;

    copyDialog.Strings = copyFolders;
    copyDialog.Value = destPath;
    LangString(move ? IDS_MOVE : IDS_COPY, copyDialog.Title);
    LangString(move ? IDS_MOVE_TO : IDS_COPY_TO, copyDialog.Static);
    copyDialog.Info = srcPanel.GetItemsInfoString(indices);

    if (copyDialog.Create(srcPanel.GetParent()) != IDOK)
      return;

    destPath = copyDialog.Value;
  }

  {
    if (destPath.IsEmpty())
    {
      srcPanel.MessageBox_Error_UnsupportOperation();
      return;
    }

    UString correctName;
    if (!srcPanel.CorrectFsPath(destPath, correctName))
    {
      srcPanel.MessageBox_Error_HRESULT(E_INVALIDARG);
      return;
    }

    if (IsAbsolutePath(destPath))
      destPath.Empty();
    else
      destPath = srcPanel.GetFsPath();
    destPath += correctName;

    #if defined(_WIN32) && !defined(UNDER_CE)
    if (destPath.Len() > 0 && destPath[0] == '\\')
      if (destPath.Len() == 1 || destPath[1] != '\\')
      {
        srcPanel.MessageBox_Error_UnsupportOperation();
        return;
      }
    #endif

    bool possibleToUseDestPanel = false;

    if (CompareFileNames(destPath, destPanel.GetFsPath()) == 0)
    {
      if (NumPanels == 1 || CompareFileNames(destPath, srcPanel.GetFsPath()) == 0)
      {
        srcPanel.MessageBox_Error(L"Cannot copy files onto itself");
        return;
      }

      if (destPanel.DoesItSupportOperations())
        possibleToUseDestPanel = true;
    }

    bool destIsFsPath = false;

    if (possibleToUseDestPanel)
    {
      if (destPanel.IsFSFolder() || destPanel.IsAltStreamsFolder())
        destIsFsPath = true;
      else if (destPanel.IsFSDrivesFolder() || destPanel.IsRootFolder())
      {
        srcPanel.MessageBox_Error_UnsupportOperation();
        return;
      }
    }
    else
    {
      if (IsAltPathPrefix(us2fs(destPath)))
      {
        // we allow alt streams dest only to alt stream folder in second panel
        srcPanel.MessageBox_Error_UnsupportOperation();
        return;
        /*
        FString basePath = us2fs(destPath);
        basePath.DeleteBack();
        if (!DoesFileOrDirExist(basePath))
        {
          srcPanel.MessageBoxError2Lines(basePath, ERROR_FILE_NOT_FOUND); // GetLastError()
          return;
        }
        destIsFsPath = true;
        */
      }
      else
      {
        if (indices.Size() == 1 &&
          !destPath.IsEmpty() && !IS_PATH_SEPAR(destPath.Back()))
        {
          int pos = destPath.ReverseFind_PathSepar();
          if (pos < 0)
          {
            srcPanel.MessageBox_Error_UnsupportOperation();
            return;
          }
          {
            /*
            #ifdef _WIN32
            UString name = destPath.Ptr(pos + 1);
            if (name.Find(L':') >= 0)
            {
              srcPanel.MessageBox_Error_UnsupportOperation();
              return;
            }
            #endif
            */
            UString prefix = destPath.Left(pos + 1);
            if (!CreateComplexDir(us2fs(prefix)))
            {
              DWORD lastError = ::GetLastError();
              srcPanel.MessageBox_Error_2Lines_Message_HRESULT(prefix, lastError);
              return;
            }
          }
          // bool isFolder = srcPanael.IsItem_Folder(indices[0]);
        }
        else
        {
          NName::NormalizeDirPathPrefix(destPath);
          if (!CreateComplexDir(us2fs(destPath)))
          {
            DWORD lastError = ::GetLastError();
            srcPanel.MessageBox_Error_2Lines_Message_HRESULT(destPath, lastError);
            return;
          }
        }
        destIsFsPath = true;
      }
    }

    if (!destIsFsPath)
      useDestPanel = true;

    AddUniqueStringToHeadOfList(copyFolders, destPath);
    while (copyFolders.Size() > 20)
      copyFolders.DeleteBack();
    SaveCopyHistory(copyFolders);
  }

  bool useSrcPanel = !useDestPanel || !srcPanel.Is_IO_FS_Folder();

  bool useTemp = useSrcPanel && useDestPanel;
  if (useTemp && NumPanels == 1)
  {
    srcPanel.MessageBox_Error_UnsupportOperation();
    return;
  }
  
  CTempDir tempDirectory;
  FString tempDirPrefix;
  if (useTemp)
  {
    tempDirectory.Create(kTempDirPrefix);
    tempDirPrefix = tempDirectory.GetPath();
    NFile::NName::NormalizeDirPathPrefix(tempDirPrefix);
  }

  CSelectedState srcSelState;
  CSelectedState destSelState;
  srcPanel.SaveSelectedState(srcSelState);
  destPanel.SaveSelectedState(destSelState);

  CPanel::CDisableNotify disableNotify1(destPanel);
  CPanel::CDisableNotify disableNotify2(srcPanel);

  HRESULT result = S_OK;
  
  if (useSrcPanel)
  {
    CCopyToOptions options;
    options.folder = useTemp ? fs2us(tempDirPrefix) : destPath;
    options.moveMode = move;
    options.includeAltStreams = true;
    options.replaceAltStreamChars = false;
    options.showErrorMessages = true;

    result = srcPanel.CopyTo(options, indices, NULL);
  }
  
  if (result == S_OK && useDestPanel)
  {
    UStringVector filePaths;
    UString folderPrefix;
    
    if (useTemp)
      folderPrefix = fs2us(tempDirPrefix);
    else
      folderPrefix = srcPanel.GetFsPath();
    
    filePaths.ClearAndReserve(indices.Size());
    
    FOR_VECTOR (i, indices)
    {
      UInt32 index = indices[i];
      UString s;
      if (useFullItemPaths)
        s = srcPanel.GetItemRelPath2(index);
      else
        s = srcPanel.GetItemName_for_Copy(index);
      filePaths.AddInReserved(s);
    }
    
    result = destPanel.CopyFrom(move, folderPrefix, filePaths, true, 0);
  }
  
  if (result != S_OK)
  {
    // disableNotify1.Restore();
    // disableNotify2.Restore();
    // For Password:
    // srcPanel.SetFocusToList();
    // srcPanel.InvalidateList(NULL, true);

    if (result != E_ABORT)
      srcPanel.MessageBox_Error_HRESULT(result);
    // return;
  }

  RefreshTitleAlways();
  
  if (copyToSame || move)
  {
    srcPanel.RefreshListCtrl(srcSelState);
  }
  
  if (!copyToSame)
  {
    destPanel.RefreshListCtrl(destSelState);
    srcPanel.KillSelection();
  }

  disableNotify1.Restore();
  disableNotify2.Restore();
  srcPanel.SetFocusToList();
}

void CApp::OnSetSameFolder(int srcPanelIndex)
{
  if (NumPanels <= 1)
    return;
  const CPanel &srcPanel = Panels[srcPanelIndex];
  CPanel &destPanel = Panels[1 - srcPanelIndex];
  destPanel.BindToPathAndRefresh(srcPanel._currentFolderPrefix);
}

void CApp::OnSetSubFolder(int srcPanelIndex)
{
  if (NumPanels <= 1)
    return;
  const CPanel &srcPanel = Panels[srcPanelIndex];
  CPanel &destPanel = Panels[1 - srcPanelIndex];

  int focusedItem = srcPanel._listView.GetFocusedItem();
  if (focusedItem < 0)
    return;
  int realIndex = srcPanel.GetRealItemIndex(focusedItem);
  if (!srcPanel.IsItem_Folder(realIndex))
    return;

  // destPanel.BindToFolder(srcPanel._currentFolderPrefix + srcPanel.GetItemName(realIndex) + WCHAR_PATH_SEPARATOR);

  CMyComPtr<IFolderFolder> newFolder;
  if (realIndex == kParentIndex)
  {
    if (srcPanel._folder->BindToParentFolder(&newFolder) != S_OK)
      return;
    if (!newFolder)
    {
      {
        const UString parentPrefix = srcPanel.GetParentDirPrefix();
        COpenResult openRes;
        destPanel.BindToPath(parentPrefix, UString(), openRes);
      }
      destPanel.RefreshListCtrl();
      return;
    }
  }
  else
  {
    if (srcPanel._folder->BindToFolder(realIndex, &newFolder) != S_OK)
      return;
  }

  if (!newFolder)
    return;

  destPanel.CloseOpenFolders();
  destPanel.SetNewFolder(newFolder);
  destPanel.RefreshListCtrl();
}

/*
int CApp::GetFocusedPanelIndex() const
{
  return LastFocusedPanel;
  HWND hwnd = ::GetFocus();
  for (;;)
  {
    if (hwnd == 0)
      return 0;
    for (unsigned i = 0; i < kNumPanelsMax; i++)
    {
      if (PanelsCreated[i] &&
          ((HWND)Panels[i] == hwnd || Panels[i]._listView == hwnd))
        return i;
    }
    hwnd = GetParent(hwnd);
  }
}
*/

static UString g_ToolTipBuffer;
static CSysString g_ToolTipBufferSys;

void CApp::OnNotify(int /* ctrlID */, LPNMHDR pnmh)
{
  {
    if (pnmh->code == TTN_GETDISPINFO)
    {
      LPNMTTDISPINFO info = (LPNMTTDISPINFO)pnmh;
      info->hinst = 0;
      g_ToolTipBuffer.Empty();
      SetButtonText((int)info->hdr.idFrom, g_ToolTipBuffer);
      g_ToolTipBufferSys = GetSystemString(g_ToolTipBuffer);
      info->lpszText = g_ToolTipBufferSys.Ptr_non_const();
      return;
    }
    #ifndef _UNICODE
    if (pnmh->code == TTN_GETDISPINFOW)
    {
      LPNMTTDISPINFOW info = (LPNMTTDISPINFOW)pnmh;
      info->hinst = 0;
      g_ToolTipBuffer.Empty();
      SetButtonText((int)info->hdr.idFrom, g_ToolTipBuffer);
      info->lpszText = g_ToolTipBuffer.Ptr_non_const();
      return;
    }
    #endif
  }
}

void CApp::RefreshTitle(bool always)
{
  UString path = GetFocusedPanel()._currentFolderPrefix;
  if (path.IsEmpty())
    path = "7-Zip"; // LangString(IDS_APP_TITLE);
  if (!always && path == PrevTitle)
    return;
  PrevTitle = path;
  NWindows::MySetWindowText(_window, path);
}

void CApp::RefreshTitlePanel(unsigned panelIndex, bool always)
{
  if (panelIndex != GetFocusedPanelIndex())
    return;
  RefreshTitle(always);
}

static void AddUniqueStringToHead(UStringVector &list, const UString &s)
{
  for (unsigned i = 0; i < list.Size();)
    if (s.IsEqualTo_NoCase(list[i]))
      list.Delete(i);
    else
      i++;
  list.Insert(0, s);
}


void CFolderHistory::Normalize()
{
  const unsigned kMaxSize = 100;
  if (Strings.Size() > kMaxSize)
    Strings.DeleteFrom(kMaxSize);
}

void CFolderHistory::AddString(const UString &s)
{
  NSynchronization::CCriticalSectionLock lock(_criticalSection);
  AddUniqueStringToHead(Strings, s);
  Normalize();
}

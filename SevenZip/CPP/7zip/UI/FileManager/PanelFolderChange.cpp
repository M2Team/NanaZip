// PanelFolderChange.cpp

#include "StdAfx.h"

#include "../../../Common/StringConvert.h"
#include "../../../Common/Wildcard.h"

#include "../../../Windows/FileName.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/PropVariant.h"

#include "../../PropID.h"

#ifdef UNDER_CE
#include "FSFolder.h"
#else
#include "FSDrives.h"
#endif
#include "LangUtils.h"
#include "ListViewDialog.h"
#include "Panel.h"
#include "RootFolder.h"
#include "ViewSettings.h"

#include "resource.h"

using namespace NWindows;
using namespace NFile;
using namespace NFind;

void CPanel::ReleaseFolder()
{
  DeleteListItems();

  _folder.Release();

  _folderCompare.Release();
  _folderGetItemName.Release();
  _folderRawProps.Release();
  _folderAltStreams.Release();
  _folderOperations.Release();
  
  _thereAreDeletedItems = false;
}

void CPanel::SetNewFolder(IFolderFolder *newFolder)
{
  ReleaseFolder();
  _folder = newFolder;
  if (_folder)
  {
    _folder.QueryInterface(IID_IFolderCompare, &_folderCompare);
    _folder.QueryInterface(IID_IFolderGetItemName, &_folderGetItemName);
    _folder.QueryInterface(IID_IArchiveGetRawProps, &_folderRawProps);
    _folder.QueryInterface(IID_IFolderAltStreams, &_folderAltStreams);
    _folder.QueryInterface(IID_IFolderOperations, &_folderOperations);
  }
}

void CPanel::SetToRootFolder()
{
  ReleaseFolder();
  _library.Free();
  
  CRootFolder *rootFolderSpec = new CRootFolder;
  SetNewFolder(rootFolderSpec);
  rootFolderSpec->Init();
}


static bool DoesNameContainWildcard_SkipRoot(const UString &path)
{
  return DoesNameContainWildcard(path.Ptr(NName::GetRootPrefixSize(path)));
}

HRESULT CPanel::BindToPath(const UString &fullPath, const UString &arcFormat, COpenResult &openRes)
{
  UString path = fullPath;
  #ifdef _WIN32
  path.Replace(L'/', WCHAR_PATH_SEPARATOR);
  #endif

  openRes.ArchiveIsOpened = false;
  openRes.Encrypted = false;
  
  CDisableTimerProcessing disableTimerProcessing(*this);
  CDisableNotify disableNotify(*this);

  for (; !_parentFolders.IsEmpty(); CloseOneLevel())
  {
    // ---------- we try to use open archive ----------

    const CFolderLink &link = _parentFolders.Back();
    const UString &virtPath = link.VirtualPath;
    if (!path.IsPrefixedBy(virtPath))
      continue;
    UString relatPath = path.Ptr(virtPath.Len());
    if (!relatPath.IsEmpty())
    {
      if (!IS_PATH_SEPAR(relatPath[0]))
        continue;
      else
        relatPath.Delete(0);
    }
    
    UString relatPath2 = relatPath;
    if (!relatPath2.IsEmpty() && !IS_PATH_SEPAR(relatPath2.Back()))
      relatPath2.Add_PathSepar();

    for (;;)
    {
      const UString foldPath = GetFolderPath(_folder);
      if (relatPath2 == foldPath)
        break;
      if (relatPath.IsPrefixedBy(foldPath))
      {
        path = relatPath.Ptr(foldPath.Len());
        break;
      }
      CMyComPtr<IFolderFolder> newFolder;
      if (_folder->BindToParentFolder(&newFolder) != S_OK)
        throw 20140918;
      if (!newFolder) // we exit from loop above if (relatPath.IsPrefixedBy(empty path for root folder)
        throw 20140918;
      SetNewFolder(newFolder);
    }
    break;
  }

  if (_parentFolders.IsEmpty())
  {
    // ---------- we open file or folder from file system ----------

    CloseOpenFolders();
    UString sysPath = path;
    
    unsigned prefixSize = NName::GetRootPrefixSize(sysPath);
    if (prefixSize == 0 || sysPath[prefixSize] == 0)
      sysPath.Empty();
    
    #if defined(_WIN32) && !defined(UNDER_CE)
    if (!sysPath.IsEmpty() && sysPath.Back() == ':' &&
      (sysPath.Len() != 2 || !NName::IsDrivePath2(sysPath)))
    {
      UString baseFile = sysPath;
      baseFile.DeleteBack();
      if (NFind::DoesFileOrDirExist(us2fs(baseFile)))
        sysPath.Empty();
    }
    #endif
    
    CFileInfo fileInfo;
    
    while (!sysPath.IsEmpty())
    {
      if (fileInfo.Find(us2fs(sysPath)))
        break;
      int pos = sysPath.ReverseFind_PathSepar();
      if (pos < 0)
        sysPath.Empty();
      else
      {
        /*
        if (reducedParts.Size() > 0 || pos < (int)sysPath.Len() - 1)
          reducedParts.Add(sysPath.Ptr(pos + 1));
        */
        #if defined(_WIN32) && !defined(UNDER_CE)
        if (pos == 2 && NName::IsDrivePath2(sysPath) && sysPath.Len() > 3)
          pos++;
        #endif

        sysPath.DeleteFrom((unsigned)pos);
      }
    }
    
    SetToRootFolder();

    CMyComPtr<IFolderFolder> newFolder;
  
    if (sysPath.IsEmpty())
    {
      _folder->BindToFolder(path, &newFolder);
    }
    else if (fileInfo.IsDir())
    {
      #ifdef _WIN32
      if (DoesNameContainWildcard_SkipRoot(sysPath))
      {
        FString dirPrefix, fileName;
        NDir::GetFullPathAndSplit(us2fs(sysPath), dirPrefix, fileName);
        if (DoesNameContainWildcard_SkipRoot(fs2us(dirPrefix)))
          return E_INVALIDARG;
        sysPath = fs2us(dirPrefix + fileInfo.Name);
      }
      #endif

      NName::NormalizeDirPathPrefix(sysPath);
      _folder->BindToFolder(sysPath, &newFolder);
    }
    else
    {
      FString dirPrefix, fileName;
      
      NDir::GetFullPathAndSplit(us2fs(sysPath), dirPrefix, fileName);

      HRESULT res = S_OK;
      
      #ifdef _WIN32
      if (DoesNameContainWildcard_SkipRoot(fs2us(dirPrefix)))
        return E_INVALIDARG;

      if (DoesNameContainWildcard(fs2us(fileName)))
        res = S_FALSE;
      else
      #endif
      {
        CTempFileInfo tfi;
        tfi.RelPath = fs2us(fileName);
        tfi.FolderPath = dirPrefix;
        tfi.FilePath = us2fs(sysPath);
        res = OpenAsArc(NULL, tfi, sysPath, arcFormat, openRes);
      }
      
      if (res == S_FALSE)
        _folder->BindToFolder(fs2us(dirPrefix), &newFolder);
      else
      {
        RINOK(res);
        openRes.ArchiveIsOpened = true;
        _parentFolders.Back().ParentFolderPath = fs2us(dirPrefix);
        path.DeleteFrontal(sysPath.Len());
        if (!path.IsEmpty() && IS_PATH_SEPAR(path[0]))
          path.Delete(0);
      }
    }
    
    if (newFolder)
    {
      SetNewFolder(newFolder);
      // LoadFullPath();
      return S_OK;
    }
  }
  
  {
    // ---------- we open folder remPath in archive and sub archives ----------

    for (unsigned curPos = 0; curPos != path.Len();)
    {
      UString s = path.Ptr(curPos);
      int slashPos = NName::FindSepar(s);
      unsigned skipLen = s.Len();
      if (slashPos >= 0)
      {
        s.DeleteFrom((unsigned)slashPos);
        skipLen = slashPos + 1;
      }

      CMyComPtr<IFolderFolder> newFolder;
      _folder->BindToFolder(s, &newFolder);
      if (newFolder)
        curPos += skipLen;
      else if (_folderAltStreams)
      {
        int pos = s.Find(L':');
        if (pos >= 0)
        {
          UString baseName = s;
          baseName.DeleteFrom((unsigned)pos);
          if (_folderAltStreams->BindToAltStreams(baseName, &newFolder) == S_OK && newFolder)
            curPos += pos + 1;
        }
      }
      
      if (!newFolder)
        break;

      SetNewFolder(newFolder);
    }
  }

  return S_OK;
}

HRESULT CPanel::BindToPathAndRefresh(const UString &path)
{
  CDisableTimerProcessing disableTimerProcessing(*this);
  CDisableNotify disableNotify(*this);
  COpenResult openRes;
  UString s = path;
  
  #ifdef _WIN32
    if (!s.IsEmpty() && s[0] == '\"' && s.Back() == '\"')
    {
      s.DeleteBack();
      s.Delete(0);
    }
  #endif

  HRESULT res = BindToPath(s, UString(), openRes);
  RefreshListCtrl();
  return res;
}

void CPanel::SetBookmark(unsigned index)
{
  _appState->FastFolders.SetString(index, _currentFolderPrefix);
}

void CPanel::OpenBookmark(unsigned index)
{
  BindToPathAndRefresh(_appState->FastFolders.GetString(index));
}

UString GetFolderPath(IFolderFolder *folder)
{
  {
    NCOM::CPropVariant prop;
    if (folder->GetFolderProperty(kpidPath, &prop) == S_OK)
      if (prop.vt == VT_BSTR)
        return (wchar_t *)prop.bstrVal;
  }
  return UString();
}

void CPanel::LoadFullPath()
{
  _currentFolderPrefix.Empty();
  FOR_VECTOR (i, _parentFolders)
  {
    const CFolderLink &folderLink = _parentFolders[i];
    _currentFolderPrefix += folderLink.ParentFolderPath;
        // GetFolderPath(folderLink.ParentFolder);
    _currentFolderPrefix += folderLink.RelPath;
    _currentFolderPrefix.Add_PathSepar();
  }
  if (_folder)
    _currentFolderPrefix += GetFolderPath(_folder);
}

static int GetRealIconIndex(CFSTR path, DWORD attributes)
{
  int index = -1;
  if (GetRealIconIndex(path, attributes, index) != 0)
    return index;
  return -1;
}

void CPanel::LoadFullPathAndShow()
{
  LoadFullPath();
  _appState->FolderHistory.AddString(_currentFolderPrefix);

  _headerComboBox.SetText(_currentFolderPrefix);

  #ifndef UNDER_CE

  COMBOBOXEXITEM item;
  item.mask = 0;

  UString path = _currentFolderPrefix;
  if (path.Len() >
      #ifdef _WIN32
      3
      #else
      1
      #endif
      && IS_PATH_SEPAR(path.Back()))
    path.DeleteBack();

  DWORD attrib = FILE_ATTRIBUTE_DIRECTORY;

  // GetRealIconIndex is slow for direct DVD/UDF path. So we use dummy path
  if (path.IsPrefixedBy(L"\\\\.\\"))
    path = "_TestFolder_";
  else
  {
    CFileInfo fi;
    if (fi.Find(us2fs(path)))
      attrib = fi.Attrib;
  }
  item.iImage = GetRealIconIndex(us2fs(path), attrib);

  if (item.iImage >= 0)
  {
    item.iSelectedImage = item.iImage;
    item.mask |= (CBEIF_IMAGE | CBEIF_SELECTEDIMAGE);
  }
  item.iItem = -1;
  _headerComboBox.SetItem(&item);
  
  #endif

  RefreshTitle();
}

#ifndef UNDER_CE
LRESULT CPanel::OnNotifyComboBoxEnter(const UString &s)
{
  if (BindToPathAndRefresh(GetUnicodeString(s)) == S_OK)
  {
    PostMsg(kSetFocusToListView);
    return TRUE;
  }
  return FALSE;
}

bool CPanel::OnNotifyComboBoxEndEdit(PNMCBEENDEDITW info, LRESULT &result)
{
  if (info->iWhy == CBENF_ESCAPE)
  {
    _headerComboBox.SetText(_currentFolderPrefix);
    PostMsg(kSetFocusToListView);
    result = FALSE;
    return true;
  }

  /*
  if (info->iWhy == CBENF_DROPDOWN)
  {
    result = FALSE;
    return true;
  }
  */

  if (info->iWhy == CBENF_RETURN)
  {
    // When we use Edit control and press Enter.
    UString s;
    _headerComboBox.GetText(s);
    result = OnNotifyComboBoxEnter(s);
    return true;
  }
  return false;
}
#endif

#ifndef _UNICODE
bool CPanel::OnNotifyComboBoxEndEdit(PNMCBEENDEDIT info, LRESULT &result)
{
  if (info->iWhy == CBENF_ESCAPE)
  {
    _headerComboBox.SetText(_currentFolderPrefix);
    PostMsg(kSetFocusToListView);
    result = FALSE;
    return true;
  }
  /*
  if (info->iWhy == CBENF_DROPDOWN)
  {
    result = FALSE;
    return true;
  }
  */

  if (info->iWhy == CBENF_RETURN)
  {
    UString s;
    _headerComboBox.GetText(s);
    // GetUnicodeString(info->szText)
    result = OnNotifyComboBoxEnter(s);
    return true;
  }
  return false;
}
#endif

void CPanel::AddComboBoxItem(const UString &name, int iconIndex, int indent, bool addToList)
{
  #ifdef UNDER_CE

  UString s;
  iconIndex = iconIndex;
  for (int i = 0; i < indent; i++)
    s += "  ";
  _headerComboBox.AddString(s + name);
  
  #else
  
  COMBOBOXEXITEMW item;
  item.mask = CBEIF_TEXT | CBEIF_INDENT;
  item.iSelectedImage = item.iImage = iconIndex;
  if (iconIndex >= 0)
    item.mask |= (CBEIF_IMAGE | CBEIF_SELECTEDIMAGE);
  item.iItem = -1;
  item.iIndent = indent;
  item.pszText = name.Ptr_non_const();
  _headerComboBox.InsertItem(&item);
  
  #endif

  if (addToList)
    ComboBoxPaths.Add(name);
}

extern UString RootFolder_GetName_Computer(int &iconIndex);
extern UString RootFolder_GetName_Network(int &iconIndex);
extern UString RootFolder_GetName_Documents(int &iconIndex);

bool CPanel::OnComboBoxCommand(UINT code, LPARAM /* param */, LRESULT &result)
{
  result = FALSE;
  switch (code)
  {
    case CBN_DROPDOWN:
    {
      ComboBoxPaths.Clear();
      _headerComboBox.ResetContent();
      
      unsigned i;
      UStringVector pathParts;
      
      SplitPathToParts(_currentFolderPrefix, pathParts);
      UString sumPass;
      if (!pathParts.IsEmpty())
        pathParts.DeleteBack();
      for (i = 0; i < pathParts.Size(); i++)
      {
        UString name = pathParts[i];
        sumPass += name;
        sumPass.Add_PathSepar();
        CFileInfo info;
        DWORD attrib = FILE_ATTRIBUTE_DIRECTORY;
        if (info.Find(us2fs(sumPass)))
          attrib = info.Attrib;
        AddComboBoxItem(name.IsEmpty() ? L"\\" : name, GetRealIconIndex(us2fs(sumPass), attrib), i, false);
        ComboBoxPaths.Add(sumPass);
      }

      #ifndef UNDER_CE

      int iconIndex;
      UString name;
      name = RootFolder_GetName_Documents(iconIndex);
      AddComboBoxItem(name, iconIndex, 0, true);

      name = RootFolder_GetName_Computer(iconIndex);
      AddComboBoxItem(name, iconIndex, 0, true);
        
      FStringVector driveStrings;
      MyGetLogicalDriveStrings(driveStrings);
      for (i = 0; i < driveStrings.Size(); i++)
      {
        FString s = driveStrings[i];
        ComboBoxPaths.Add(fs2us(s));
        int iconIndex2 = GetRealIconIndex(s, 0);
        if (s.Len() > 0 && s.Back() == FCHAR_PATH_SEPARATOR)
          s.DeleteBack();
        AddComboBoxItem(fs2us(s), iconIndex2, 1, false);
      }

      name = RootFolder_GetName_Network(iconIndex);
      AddComboBoxItem(name, iconIndex, 0, true);

      #endif
    
      return false;
    }

    case CBN_SELENDOK:
    {
      int index = _headerComboBox.GetCurSel();
      if (index >= 0)
      {
        UString pass = ComboBoxPaths[index];
        _headerComboBox.SetCurSel(-1);
        // _headerComboBox.SetText(pass); // it's fix for seclecting by mouse.
        if (BindToPathAndRefresh(pass) == S_OK)
        {
          PostMsg(kSetFocusToListView);
          #ifdef UNDER_CE
          PostMsg(kRefresh_HeaderComboBox);
          #endif
          return true;
        }
      }
      return false;
    }
    /*
    case CBN_CLOSEUP:
    {
      LoadFullPathAndShow();
      true;

    }
    case CBN_SELCHANGE:
    {
      // LoadFullPathAndShow();
      return true;
    }
    */
  }
  return false;
}

bool CPanel::OnNotifyComboBox(LPNMHDR NON_CE_VAR(header), LRESULT & NON_CE_VAR(result))
{
  #ifndef UNDER_CE
  switch (header->code)
  {
    case CBEN_BEGINEDIT:
    {
      _lastFocusedIsList = false;
      _panelCallback->PanelWasFocused();
      break;
    }
    #ifndef _UNICODE
    case CBEN_ENDEDIT:
    {
      return OnNotifyComboBoxEndEdit((PNMCBEENDEDIT)header, result);
    }
    #endif
    case CBEN_ENDEDITW:
    {
      return OnNotifyComboBoxEndEdit((PNMCBEENDEDITW)header, result);
    }
  }
  #endif
  return false;
}


void CPanel::FoldersHistory()
{
  CListViewDialog listViewDialog;
  listViewDialog.DeleteIsAllowed = true;
  listViewDialog.SelectFirst = true;
  LangString(IDS_FOLDERS_HISTORY, listViewDialog.Title);
  _appState->FolderHistory.GetList(listViewDialog.Strings);
  if (listViewDialog.Create(GetParent()) != IDOK)
    return;
  UString selectString;
  if (listViewDialog.StringsWereChanged)
  {
    _appState->FolderHistory.RemoveAll();
    for (int i = listViewDialog.Strings.Size() - 1; i >= 0; i--)
      _appState->FolderHistory.AddString(listViewDialog.Strings[i]);
    if (listViewDialog.FocusedItemIndex >= 0)
      selectString = listViewDialog.Strings[listViewDialog.FocusedItemIndex];
  }
  else
  {
    if (listViewDialog.FocusedItemIndex >= 0)
      selectString = listViewDialog.Strings[listViewDialog.FocusedItemIndex];
  }
  if (listViewDialog.FocusedItemIndex >= 0)
    BindToPathAndRefresh(selectString);
}


UString CPanel::GetParentDirPrefix() const
{
  UString s;
  if (!_currentFolderPrefix.IsEmpty())
  {
    wchar_t c = _currentFolderPrefix.Back();
    if (IS_PATH_SEPAR(c) || c == ':')
    {
      s = _currentFolderPrefix;
      s.DeleteBack();
      if (s != L"\\\\." &&
          s != L"\\\\?")
      {
        int pos = s.ReverseFind_PathSepar();
        if (pos >= 0)
          s.DeleteFrom((unsigned)(pos + 1));
      }
    }
  }
  return s;
}


void CPanel::OpenParentFolder()
{
  LoadFullPath(); // Maybe we don't need it ??
  
  UString parentFolderPrefix;
  UString focusedName;
  
  if (!_currentFolderPrefix.IsEmpty())
  {
    wchar_t c = _currentFolderPrefix.Back();
    if (IS_PATH_SEPAR(c) || c == ':')
    {
      focusedName = _currentFolderPrefix;
      focusedName.DeleteBack();
      /*
      if (c == ':' && !focusedName.IsEmpty() && IS_PATH_SEPAR(focusedName.Back()))
      {
        focusedName.DeleteBack();
      }
      else
      */
      if (focusedName != L"\\\\." &&
          focusedName != L"\\\\?")
      {
        int pos = focusedName.ReverseFind_PathSepar();
        if (pos >= 0)
        {
          parentFolderPrefix = focusedName;
          parentFolderPrefix.DeleteFrom((unsigned)(pos + 1));
          focusedName.DeleteFrontal(pos + 1);
        }
      }
    }
  }

  CDisableTimerProcessing disableTimerProcessing(*this);
  CDisableNotify disableNotify(*this);
  
  CMyComPtr<IFolderFolder> newFolder;
  _folder->BindToParentFolder(&newFolder);

  // newFolder.Release(); // for test
  
  if (newFolder)
    SetNewFolder(newFolder);
  else
  {
    bool needSetFolder = true;
    if (!_parentFolders.IsEmpty())
    {
      {
        const CFolderLink &link = _parentFolders.Back();
        parentFolderPrefix = link.ParentFolderPath;
        focusedName = link.RelPath;
      }
      CloseOneLevel();
      needSetFolder = (!_folder);
    }
    
    if (needSetFolder)
    {
      {
        COpenResult openRes;
        BindToPath(parentFolderPrefix, UString(), openRes);
      }
    }
  }
    
  CSelectedState state;
  state.FocusedName = focusedName;
  state.FocusedName_Defined = true;
  /*
  if (!focusedName.IsEmpty())
    state.SelectedNames.Add(focusedName);
  */
  LoadFullPath();
  // ::SetCurrentDirectory(::_currentFolderPrefix);
  RefreshListCtrl(state);
  // _listView.EnsureVisible(_listView.GetFocusedItem(), false);
}


void CPanel::CloseOneLevel()
{
  ReleaseFolder();
  _library.Free();
  {
    CFolderLink &link = _parentFolders.Back();
    if (link.ParentFolder)
      SetNewFolder(link.ParentFolder);
    _library.Attach(link.Library.Detach());
  }
  if (_parentFolders.Size() > 1)
    OpenParentArchiveFolder();
  _parentFolders.DeleteBack();
  if (_parentFolders.IsEmpty())
    _flatMode = _flatModeForDisk;
}

void CPanel::CloseOpenFolders()
{
  while (!_parentFolders.IsEmpty())
    CloseOneLevel();
  _flatMode = _flatModeForDisk;
  ReleaseFolder();
  _library.Free();
}

void CPanel::OpenRootFolder()
{
  CDisableTimerProcessing disableTimerProcessing(*this);
  CDisableNotify disableNotify(*this);
  _parentFolders.Clear();
  SetToRootFolder();
  RefreshListCtrl();
  // ::SetCurrentDirectory(::_currentFolderPrefix);
  /*
  BeforeChangeFolder();
  _currentFolderPrefix.Empty();
  AfterChangeFolder();
  SetCurrentPathText();
  RefreshListCtrl(UString(), 0, UStringVector());
  _listView.EnsureVisible(_listView.GetFocusedItem(), false);
  */
}

void CPanel::OpenDrivesFolder()
{
  CloseOpenFolders();
  #ifdef UNDER_CE
  NFsFolder::CFSFolder *folderSpec = new NFsFolder::CFSFolder;
  SetNewFolder(folderSpec);
  folderSpec->InitToRoot();
  #else
  CFSDrives *folderSpec = new CFSDrives;
  SetNewFolder(folderSpec);
  folderSpec->Init();
  #endif
  RefreshListCtrl();
}

void CPanel::OpenFolder(int index)
{
  if (index == kParentIndex)
  {
    OpenParentFolder();
    return;
  }
  CMyComPtr<IFolderFolder> newFolder;
  HRESULT res = _folder->BindToFolder(index, &newFolder);
  if (res != 0)
  {
    MessageBox_Error_HRESULT(res);
    return;
  }
  if (!newFolder)
    return;
  SetNewFolder(newFolder);
  LoadFullPath();
  RefreshListCtrl();
  // 17.02: fixed : now we don't select first item
  // _listView.SetItemState_Selected(_listView.GetFocusedItem());
  _listView.EnsureVisible(_listView.GetFocusedItem(), false);
}

void CPanel::OpenAltStreams()
{
  CRecordVector<UInt32> indices;
  GetOperatedItemIndices(indices);
  Int32 realIndex = -1;
  if (indices.Size() > 1)
    return;
  if (indices.Size() == 1)
    realIndex = indices[0];

  if (_folderAltStreams)
  {
    CMyComPtr<IFolderFolder> newFolder;
    _folderAltStreams->BindToAltStreams(realIndex, &newFolder);
    if (newFolder)
    {
      CDisableTimerProcessing disableTimerProcessing(*this);
      CDisableNotify disableNotify(*this);
      SetNewFolder(newFolder);
      RefreshListCtrl();
      return;
    }
    return;
  }
  
  #if defined(_WIN32) && !defined(UNDER_CE)
  UString path;
  if (realIndex >= 0)
    path = GetItemFullPath(realIndex);
  else
  {
    path = GetFsPath();
    if (!NName::IsDriveRootPath_SuperAllowed(us2fs(path)))
      if (!path.IsEmpty() && IS_PATH_SEPAR(path.Back()))
        path.DeleteBack();
  }

  path += ':';
  BindToPathAndRefresh(path);
  #endif
}

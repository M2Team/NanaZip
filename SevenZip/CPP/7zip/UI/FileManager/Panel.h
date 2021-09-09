// Panel.h

#ifndef __PANEL_H
#define __PANEL_H

#include "../../../Common/MyWindows.h"

#include <ShlObj.h>

#include "../../../../C/Alloc.h"

#include "../../../Common/Defs.h"
#include "../../../Common/MyCom.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/Handle.h"
#include "../../../Windows/PropVariantConv.h"
#include "../../../Windows/Synchronization.h"

#include "../../../Windows/Control/ComboBox.h"
#include "../../../Windows/Control/Edit.h"
#include "../../../Windows/Control/ListView.h"
#include "../../../Windows/Control/ReBar.h"
#include "../../../Windows/Control/Static.h"
#include "../../../Windows/Control/StatusBar.h"
#include "../../../Windows/Control/ToolBar.h"
#include "../../../Windows/Control/Window2.h"

#include "../../Archive/IArchive.h"

#include "ExtractCallback.h"

#include "AppState.h"
#include "IFolder.h"
#include "MyCom2.h"
#include "ProgressDialog2.h"
#include "SysIconUtils.h"

#ifdef UNDER_CE
#define NON_CE_VAR(_v_)
#else
#define NON_CE_VAR(_v_) _v_
#endif

const int kParentFolderID = 100;

const int kParentIndex = -1;
const UInt32 kParentIndex_UInt32 = (UInt32)(Int32)kParentIndex;

#if !defined(_WIN32) || defined(UNDER_CE)
#define ROOT_FS_FOLDER L"\\"
#else
#define ROOT_FS_FOLDER L"C:\\"
#endif

struct CPanelCallback
{
  virtual void OnTab() = 0;
  virtual void SetFocusToPath(unsigned index) = 0;
  virtual void OnCopy(bool move, bool copyToSame) = 0;
  virtual void OnSetSameFolder() = 0;
  virtual void OnSetSubFolder() = 0;
  virtual void PanelWasFocused() = 0;
  virtual void DragBegin() = 0;
  virtual void DragEnd() = 0;
  virtual void RefreshTitle(bool always) = 0;
};

void PanelCopyItems();


struct CPropColumn
{
  int Order;
  PROPID ID;
  VARTYPE Type;
  bool IsVisible;
  bool IsRawProp;
  UInt32 Width;
  UString Name;

  bool IsEqualTo(const CPropColumn &a) const
  {
    return Order == a.Order
        && ID == a.ID
        && Type == a.Type
        && IsVisible == a.IsVisible
        && IsRawProp == a.IsRawProp
        && Width == a.Width
        && Name == a.Name;
  }

  int Compare(const CPropColumn &a) const { return MyCompare(Order, a.Order); }

  int Compare_NameFirst(const CPropColumn &a) const
  {
    if (ID == kpidName)
    {
      if (a.ID != kpidName)
        return -1;
    }
    else if (a.ID == kpidName)
      return 1;
    return MyCompare(Order, a.Order);
  }
};


class CPropColumns: public CObjectVector<CPropColumn>
{
public:
  int FindItem_for_PropID(PROPID id) const
  {
    FOR_VECTOR (i, (*this))
      if ((*this)[i].ID == id)
        return i;
    return -1;
  }

  bool IsEqualTo(const CPropColumns &props) const
  {
    if (Size() != props.Size())
      return false;
    FOR_VECTOR (i, (*this))
      if (!(*this)[i].IsEqualTo(props[i]))
        return false;
    return true;
  }
};


struct CTempFileInfo
{
  UInt32 FileIndex;  // index of file in folder
  UString RelPath;   // Relative path of file from Folder
  FString FolderPath;
  FString FilePath;
  NWindows::NFile::NFind::CFileInfo FileInfo;
  bool NeedDelete;

  CTempFileInfo(): FileIndex((UInt32)(Int32)-1), NeedDelete(false) {}
  void DeleteDirAndFile() const
  {
    if (NeedDelete)
    {
      NWindows::NFile::NDir::DeleteFileAlways(FilePath);
      NWindows::NFile::NDir::RemoveDir(FolderPath);
    }
  }
  bool WasChanged(const NWindows::NFile::NFind::CFileInfo &newFileInfo) const
  {
    return newFileInfo.Size != FileInfo.Size ||
        CompareFileTime(&newFileInfo.MTime, &FileInfo.MTime) != 0;
  }
};

struct CFolderLink: public CTempFileInfo
{
  NWindows::NDLL::CLibrary Library;
  CMyComPtr<IFolderFolder> ParentFolder; // can be NULL, if parent is FS folder (in _parentFolders[0])
  UString ParentFolderPath; // including tail slash (doesn't include paths parts of parent in next level)
  bool UsePassword;
  UString Password;
  bool IsVirtual;

  UString VirtualPath; // without tail slash
  CFolderLink(): UsePassword(false), IsVirtual(false) {}

  bool WasChanged(const NWindows::NFile::NFind::CFileInfo &newFileInfo) const
  {
    return IsVirtual || CTempFileInfo::WasChanged(newFileInfo);
  }

};

enum MyMessages
{
  // we can use WM_USER, since we have defined new window class.
  // so we don't need WM_APP.
  kShiftSelectMessage = WM_USER + 1,
  kReLoadMessage,
  kSetFocusToListView,
  kOpenItemChanged,
  kRefresh_StatusBar
  #ifdef UNDER_CE
  , kRefresh_HeaderComboBox
  #endif
};

UString GetFolderPath(IFolderFolder *folder);

class CPanel;

class CMyListView: public NWindows::NControl::CListView2
{
public:
  CPanel *_panel;
  LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam);
};

/*
class CMyComboBox: public NWindows::NControl::CComboBoxEx
{
public:
  WNDPROC _origWindowProc;
  CPanel *_panel;
  LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam);
};
*/
class CMyComboBoxEdit: public NWindows::NControl::CEdit
{
public:
  WNDPROC _origWindowProc;
  CPanel *_panel;
  LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam);
};

struct CSelectedState
{
  int FocusedItem;
  bool SelectFocused;
  bool FocusedName_Defined;
  UString FocusedName;
  UStringVector SelectedNames;
  
  CSelectedState(): FocusedItem(-1), SelectFocused(true), FocusedName_Defined(false) {}
};

#ifdef UNDER_CE
#define MY_NMLISTVIEW_NMITEMACTIVATE NMLISTVIEW
#else
#define MY_NMLISTVIEW_NMITEMACTIVATE NMITEMACTIVATE
#endif

struct CCopyToOptions
{
  bool streamMode;
  bool moveMode;
  bool testMode;
  bool includeAltStreams;
  bool replaceAltStreamChars;
  bool showErrorMessages;

  UString folder;

  UStringVector hashMethods;

  CVirtFileSystem *VirtFileSystemSpec;
  ISequentialOutStream *VirtFileSystem;
  
  CCopyToOptions():
      streamMode(false),
      moveMode(false),
      testMode(false),
      includeAltStreams(true),
      replaceAltStreamChars(false),
      showErrorMessages(false),
      VirtFileSystemSpec(NULL),
      VirtFileSystem(NULL)
      {}
};
  


struct COpenResult
{
  // bool needOpenArc;
  // out:
  bool ArchiveIsOpened;
  bool Encrypted;
  UString ErrorMessage;

  COpenResult():
      // needOpenArc(false),
      ArchiveIsOpened(false), Encrypted(false) {}
};




class CPanel: public NWindows::NControl::CWindow2
{
  CExtToIconMap _extToIconMap;
  UINT _baseID;
  int _comboBoxID;
  UINT _statusBarID;

  CAppState *_appState;

  bool OnCommand(int code, int itemID, LPARAM lParam, LRESULT &result);
  LRESULT OnMessage(UINT message, WPARAM wParam, LPARAM lParam);
  virtual bool OnCreate(CREATESTRUCT *createStruct);
  virtual bool OnSize(WPARAM wParam, int xSize, int ySize);
  virtual void OnDestroy();
  virtual bool OnNotify(UINT controlID, LPNMHDR lParam, LRESULT &result);

  void AddComboBoxItem(const UString &name, int iconIndex, int indent, bool addToList);

  bool OnComboBoxCommand(UINT code, LPARAM param, LRESULT &result);
  
  #ifndef UNDER_CE
  
  LRESULT OnNotifyComboBoxEnter(const UString &s);
  bool OnNotifyComboBoxEndEdit(PNMCBEENDEDITW info, LRESULT &result);
  #ifndef _UNICODE
  bool OnNotifyComboBoxEndEdit(PNMCBEENDEDIT info, LRESULT &result);
  #endif

  #endif

  bool OnNotifyReBar(LPNMHDR lParam, LRESULT &result);
  bool OnNotifyComboBox(LPNMHDR lParam, LRESULT &result);
  void OnItemChanged(NMLISTVIEW *item);
  void OnNotifyActivateItems();
  bool OnNotifyList(LPNMHDR lParam, LRESULT &result);
  void OnDrag(LPNMLISTVIEW nmListView);
  bool OnKeyDown(LPNMLVKEYDOWN keyDownInfo, LRESULT &result);
  BOOL OnBeginLabelEdit(LV_DISPINFOW * lpnmh);
  BOOL OnEndLabelEdit(LV_DISPINFOW * lpnmh);
  void OnColumnClick(LPNMLISTVIEW info);
  bool OnCustomDraw(LPNMLVCUSTOMDRAW lplvcd, LRESULT &result);


public:
  HWND _mainWindow;
  CPanelCallback *_panelCallback;

  void SysIconsWereChanged() { _extToIconMap.Clear(); }

  void DeleteItems(bool toRecycleBin);
  void CreateFolder();
  void CreateFile();
  bool CorrectFsPath(const UString &path, UString &result);
  // bool IsPathForPlugin(const UString &path);

private:

  void ChangeWindowSize(int xSize, int ySize);
 
  HRESULT InitColumns();
  void DeleteColumn(unsigned index);
  void AddColumn(const CPropColumn &prop);

  void SetFocusedSelectedItem(int index, bool select);
  
  void OnShiftSelectMessage();
  void OnArrowWithShift();

  void OnInsert();
  // void OnUpWithShift();
  // void OnDownWithShift();
public:
  void UpdateSelection();
  void SelectSpec(bool selectMode);
  void SelectByType(bool selectMode);
  void SelectAll(bool selectMode);
  void InvertSelection();
private:

  // UString GetFileType(UInt32 index);
  LRESULT SetItemText(LVITEMW &item);

  // CRecordVector<PROPID> m_ColumnsPropIDs;

public:
  NWindows::NControl::CReBar _headerReBar;
  NWindows::NControl::CToolBar _headerToolBar;
  NWindows::NControl::
    #ifdef UNDER_CE
    CComboBox
    #else
    CComboBoxEx
    #endif
    _headerComboBox;
  UStringVector ComboBoxPaths;
  // CMyComboBox _headerComboBox;
  CMyComboBoxEdit _comboBoxEdit;
  CMyListView _listView;
  bool _thereAre_ListView_Items;
  NWindows::NControl::CStatusBar _statusBar;
  bool _lastFocusedIsList;
  // NWindows::NControl::CStatusBar _statusBar2;

  DWORD _exStyle;
  bool _showDots;
  bool _showRealFileIcons;
  // bool _virtualMode;
  // CUIntVector _realIndices;
  bool _enableItemChangeNotify;
  bool _mySelectMode;

  int _timestampLevel;


  void RedrawListItems()
  {
    _listView.RedrawAllItems();
  }


  CBoolVector _selectedStatusVector;

  CSelectedState _selectedState;
  bool _thereAreDeletedItems;
  bool _markDeletedItems;

  bool PanelCreated;

  void DeleteListItems()
  {
    if (_thereAre_ListView_Items)
    {
      bool b = _enableItemChangeNotify;
      _enableItemChangeNotify = false;
      _listView.DeleteAllItems();
      _thereAre_ListView_Items = false;
      _enableItemChangeNotify = b;
    }
  }

  HWND GetParent() const;

  UInt32 GetRealIndex(const LVITEMW &item) const
  {
    /*
    if (_virtualMode)
      return _realIndices[item.iItem];
    */
    return (UInt32)item.lParam;
  }
  
  int GetRealItemIndex(int indexInListView) const
  {
    /*
    if (_virtualMode)
      return indexInListView;
    */
    LPARAM param;
    if (!_listView.GetItemParam(indexInListView, param))
      throw 1;
    return (int)param;
  }

  UInt32 _ListViewMode;
  int _xSize;

  bool _flatMode;
  bool _flatModeForDisk;
  bool _flatModeForArc;

  // bool _showNtfsStrems_Mode;
  // bool _showNtfsStrems_ModeForDisk;
  // bool _showNtfsStrems_ModeForArc;

  bool _dontShowMode;


  UString _currentFolderPrefix;
  
  CObjectVector<CFolderLink> _parentFolders;
  NWindows::NDLL::CLibrary _library;
  
  CMyComPtr<IFolderFolder> _folder;
  CMyComPtr<IFolderCompare> _folderCompare;
  CMyComPtr<IFolderGetItemName> _folderGetItemName;
  CMyComPtr<IArchiveGetRawProps> _folderRawProps;
  CMyComPtr<IFolderAltStreams> _folderAltStreams;
  CMyComPtr<IFolderOperations> _folderOperations;

  void ReleaseFolder();
  void SetNewFolder(IFolderFolder *newFolder);

  // CMyComPtr<IFolderGetSystemIconIndex> _folderGetSystemIconIndex;

  UStringVector _fastFolders;

  void GetSelectedNames(UStringVector &selectedNames);
  void SaveSelectedState(CSelectedState &s);
  HRESULT RefreshListCtrl(const CSelectedState &s);
  HRESULT RefreshListCtrl_SaveFocused();

  bool GetItem_BoolProp(UInt32 itemIndex, PROPID propID) const;
  bool IsItem_Deleted(int itemIndex) const;
  bool IsItem_Folder(int itemIndex) const;
  bool IsItem_AltStream(int itemIndex) const;

  UString GetItemName(int itemIndex) const;
  UString GetItemName_for_Copy(int itemIndex) const;
  void GetItemName(int itemIndex, UString &s) const;
  UString GetItemPrefix(int itemIndex) const;
  UString GetItemRelPath(int itemIndex) const;
  UString GetItemRelPath2(int itemIndex) const;
  UString GetItemFullPath(int itemIndex) const;
  UInt64 GetItem_UInt64Prop(int itemIndex, PROPID propID) const;
  UInt64 GetItemSize(int itemIndex) const;

  ////////////////////////
  // PanelFolderChange.cpp

  void SetToRootFolder();
  HRESULT BindToPath(const UString &fullPath, const UString &arcFormat, COpenResult &openRes); // can be prefix
  HRESULT BindToPathAndRefresh(const UString &path);
  void OpenDrivesFolder();
  
  void SetBookmark(unsigned index);
  void OpenBookmark(unsigned index);
  
  void LoadFullPath();
  void LoadFullPathAndShow();
  void FoldersHistory();
  void OpenParentFolder();
  void CloseOneLevel();
  void CloseOpenFolders();
  void OpenRootFolder();

  UString GetParentDirPrefix() const;

  HRESULT Create(HWND mainWindow, HWND parentWindow,
      UINT id,
      const UString &currentFolderPrefix,
      const UString &arcFormat,
      CPanelCallback *panelCallback,
      CAppState *appState,
      bool needOpenArc,
      COpenResult &openRes);

  void SetFocusToList();
  void SetFocusToLastRememberedItem();


  void SaveListViewInfo();

  CPanel() :
      _thereAre_ListView_Items(false),
      _exStyle(0),
      _showDots(false),
      _showRealFileIcons(false),
      // _virtualMode(flase),
      _enableItemChangeNotify(true),
      _mySelectMode(false),
      _timestampLevel(kTimestampPrintLevel_MIN),

      _thereAreDeletedItems(false),
      _markDeletedItems(true),
      PanelCreated(false),

      _ListViewMode(3),
      _xSize(300),

      _flatMode(false),
      _flatModeForDisk(false),
      _flatModeForArc(false),

      // _showNtfsStrems_Mode(false),
      // _showNtfsStrems_ModeForDisk(false),
      // _showNtfsStrems_ModeForArc(false),

      _dontShowMode(false),

      _needSaveInfo(false),
      _startGroupSelect(0),
      _selectionIsDefined(false)
  {}

  void SetExtendedStyle()
  {
    if (_listView != 0)
      _listView.SetExtendedListViewStyle(_exStyle);
  }


  bool _needSaveInfo;
  UString _typeIDString;
  CListViewInfo _listViewInfo;
  
  CPropColumns _columns;
  CPropColumns _visibleColumns;
  
  PROPID _sortID;
  // int _sortIndex;
  bool _ascending;
  Int32 _isRawSortProp;

  void SetSortRawStatus();

  void Release();
  ~CPanel();
  void OnLeftClick(MY_NMLISTVIEW_NMITEMACTIVATE *itemActivate);
  bool OnRightClick(MY_NMLISTVIEW_NMITEMACTIVATE *itemActivate, LRESULT &result);
  void ShowColumnsContextMenu(int x, int y);

  void OnTimer();
  void OnReload();
  bool OnContextMenu(HANDLE windowHandle, int xPos, int yPos);

  CMyComPtr<IContextMenu> _sevenZipContextMenu;
  CMyComPtr<IContextMenu> _systemContextMenu;
  HRESULT CreateShellContextMenu(
      const CRecordVector<UInt32> &operatedIndices,
      CMyComPtr<IContextMenu> &systemContextMenu);
  void CreateSystemMenu(HMENU menu,
      const CRecordVector<UInt32> &operatedIndices,
      CMyComPtr<IContextMenu> &systemContextMenu);
  void CreateSevenZipMenu(HMENU menu,
      const CRecordVector<UInt32> &operatedIndices,
      CMyComPtr<IContextMenu> &sevenZipContextMenu);
  void CreateFileMenu(HMENU menu,
      CMyComPtr<IContextMenu> &sevenZipContextMenu,
      CMyComPtr<IContextMenu> &systemContextMenu,
      bool programMenu);
  void CreateFileMenu(HMENU menu);
  bool InvokePluginCommand(unsigned id);
  bool InvokePluginCommand(unsigned id, IContextMenu *sevenZipContextMenu,
      IContextMenu *systemContextMenu);

  void InvokeSystemCommand(const char *command);
  void Properties();
  void EditCut();
  void EditCopy();
  void EditPaste();

  int _startGroupSelect;

  bool _selectionIsDefined;
  bool _selectMark;
  int _prevFocusedItem;

 
  // void SortItems(int index);
  void SortItemsWithPropID(PROPID propID);

  void GetSelectedItemsIndices(CRecordVector<UInt32> &indices) const;
  void GetOperatedItemIndices(CRecordVector<UInt32> &indices) const;
  void GetAllItemIndices(CRecordVector<UInt32> &indices) const;
  void GetOperatedIndicesSmart(CRecordVector<UInt32> &indices) const;
  // void GetOperatedListViewIndices(CRecordVector<UInt32> &indices) const;
  void KillSelection();

  UString GetFolderTypeID() const;
  
  bool IsFolderTypeEqTo(const char *s) const;
  bool IsRootFolder() const;
  bool IsFSFolder() const;
  bool IsFSDrivesFolder() const;
  bool IsAltStreamsFolder() const;
  bool IsArcFolder() const;
  
  /*
    c:\Dir
    Computer\
    \\?\
    \\.\
  */
  bool Is_IO_FS_Folder() const
  {
    return IsFSFolder() || IsFSDrivesFolder() || IsAltStreamsFolder();
  }

  bool Is_Slow_Icon_Folder() const
  {
    return IsFSFolder() || IsAltStreamsFolder();
  }

  // bool IsFsOrDrivesFolder() const { return IsFSFolder() || IsFSDrivesFolder(); }
  bool IsDeviceDrivesPrefix() const { return _currentFolderPrefix == L"\\\\.\\"; }
  bool IsSuperDrivesPrefix() const { return _currentFolderPrefix == L"\\\\?\\"; }
  
  /*
    c:\Dir
    Computer\
    \\?\
  */
  bool IsFsOrPureDrivesFolder() const { return IsFSFolder() || (IsFSDrivesFolder() && !IsDeviceDrivesPrefix()); }

  /*
    c:\Dir
    Computer\
    \\?\
    \\SERVER\
  */
  bool IsFolder_with_FsItems() const
  {
    if (IsFsOrPureDrivesFolder())
      return true;
    #if defined(_WIN32) && !defined(UNDER_CE)
    FString prefix = us2fs(GetFsPath());
    return (prefix.Len() == NWindows::NFile::NName::GetNetworkServerPrefixSize(prefix));
    #else
    return false;
    #endif
  }

  UString GetFsPath() const;
  UString GetDriveOrNetworkPrefix() const;

  bool DoesItSupportOperations() const { return _folderOperations != NULL; }
  bool IsThereReadOnlyFolder() const;
  bool CheckBeforeUpdate(UINT resourceID);

  bool _processTimer;
  bool _processNotify;
  bool _processStatusBar;

  class CDisableTimerProcessing
  {
    CLASS_NO_COPY(CDisableTimerProcessing);

    bool _processTimer;

    CPanel &_panel;
    
    public:

    CDisableTimerProcessing(CPanel &panel): _panel(panel) { Disable(); }
    ~CDisableTimerProcessing() { Restore(); }
    void Disable()
    {
      _processTimer = _panel._processTimer;
      _panel._processTimer = false;
    }
    void Restore()
    {
      _panel._processTimer = _processTimer;
    }
  };

  class CDisableNotify
  {
    CLASS_NO_COPY(CDisableNotify);

    bool _processNotify;
    bool _processStatusBar;

    CPanel &_panel;

    public:

    CDisableNotify(CPanel &panel): _panel(panel) { Disable(); }
    ~CDisableNotify() { Restore(); }
    void Disable()
    {
      _processNotify = _panel._processNotify;
      _processStatusBar = _panel._processStatusBar;
      _panel._processNotify = false;
      _panel._processStatusBar = false;
    }
    void SetMemMode_Enable()
    {
      _processNotify = true;
      _processStatusBar = true;
    }
    void Restore()
    {
      _panel._processNotify = _processNotify;
      _panel._processStatusBar = _processStatusBar;
    }
  };

  void InvalidateList() { _listView.InvalidateRect(NULL, true); }

  HRESULT RefreshListCtrl();

  
  // void MessageBox_Info(LPCWSTR message, LPCWSTR caption) const;
  // void MessageBox_Warning(LPCWSTR message) const;
  void MessageBox_Error_Caption(LPCWSTR message, LPCWSTR caption) const;
  void MessageBox_Error(LPCWSTR message) const;
  void MessageBox_Error_HRESULT_Caption(HRESULT errorCode, LPCWSTR caption) const;
  void MessageBox_Error_HRESULT(HRESULT errorCode) const;
  void MessageBox_Error_2Lines_Message_HRESULT(LPCWSTR message, HRESULT errorCode) const;
  void MessageBox_LastError(LPCWSTR caption) const;
  void MessageBox_LastError() const;
  void MessageBox_Error_LangID(UINT resourceID) const;
  void MessageBox_Error_UnsupportOperation() const;
  // void MessageBoxErrorForUpdate(HRESULT errorCode, UINT resourceID);


  void OpenAltStreams();

  void OpenFocusedItemAsInternal(const wchar_t *type = NULL);
  void OpenSelectedItems(bool internal);

  void OpenFolderExternal(int index);

  void OpenFolder(int index);
  HRESULT OpenParentArchiveFolder();
  
  HRESULT OpenAsArc(IInStream *inStream,
      const CTempFileInfo &tempFileInfo,
      const UString &virtualFilePath,
      const UString &arcFormat,
      COpenResult &openRes);

  HRESULT OpenAsArc_Msg(IInStream *inStream,
      const CTempFileInfo &tempFileInfo,
      const UString &virtualFilePath,
      const UString &arcFormat
      // , bool showErrorMessage
      );
  
  HRESULT OpenAsArc_Name(const UString &relPath, const UString &arcFormat
      // , bool showErrorMessage
      );
  HRESULT OpenAsArc_Index(int index, const wchar_t *type /* = NULL */
      // , bool showErrorMessage
      );
  
  void OpenItemInArchive(int index, bool tryInternal, bool tryExternal,
      bool editMode, bool useEditor, const wchar_t *type = NULL);
  
  HRESULT OnOpenItemChanged(UInt32 index, const wchar_t *fullFilePath, bool usePassword, const UString &password);
  LRESULT OnOpenItemChanged(LPARAM lParam);

  bool IsVirus_Message(const UString &name);
  void OpenItem(int index, bool tryInternal, bool tryExternal, const wchar_t *type = NULL);
  void EditItem(bool useEditor);
  void EditItem(int index, bool useEditor);

  void RenameFile();
  void ChangeComment();

  void SetListViewMode(UInt32 index);
  UInt32 GetListViewMode() const { return _ListViewMode; }
  PROPID GetSortID() const { return _sortID; }

  void ChangeFlatMode();
  void Change_ShowNtfsStrems_Mode();
  bool GetFlatMode() const { return _flatMode; }
  // bool Get_ShowNtfsStrems_Mode() const { return _showNtfsStrems_Mode; }

  bool AutoRefresh_Mode;
  void Set_AutoRefresh_Mode(bool mode)
  {
    AutoRefresh_Mode = mode;
  }
  
  void Post_Refresh_StatusBar();
  void Refresh_StatusBar();

  void AddToArchive();

  void GetFilePaths(const CRecordVector<UInt32> &indices, UStringVector &paths, bool allowFolders = false);
  void ExtractArchives();
  void TestArchives();

  HRESULT CopyTo(CCopyToOptions &options,
      const CRecordVector<UInt32> &indices,
      UStringVector *messages,
      bool &usePassword, UString &password);

  HRESULT CopyTo(CCopyToOptions &options, const CRecordVector<UInt32> &indices, UStringVector *messages)
  {
    bool usePassword = false;
    UString password;
    if (_parentFolders.Size() > 0)
    {
      const CFolderLink &fl = _parentFolders.Back();
      usePassword = fl.UsePassword;
      password = fl.Password;
    }
    return CopyTo(options, indices, messages, usePassword, password);
  }

  HRESULT CopyFrom(bool moveMode, const UString &folderPrefix, const UStringVector &filePaths,
      bool showErrorMessages, UStringVector *messages);

  void CopyFromNoAsk(const UStringVector &filePaths);
  void CopyFromAsk(const UStringVector &filePaths);

  // empty folderPath means create new Archive to path of first fileName.
  void DropObject(IDataObject * dataObject, const UString &folderPath);

  // empty folderPath means create new Archive to path of first fileName.
  void CompressDropFiles(const UStringVector &fileNames, const UString &folderPath);

  void RefreshTitle(bool always = false) { _panelCallback->RefreshTitle(always);  }
  void RefreshTitleAlways() { RefreshTitle(true);  }

  UString GetItemsInfoString(const CRecordVector<UInt32> &indices);
};

class CMyBuffer
{
  void *_data;
public:
  CMyBuffer(): _data(0) {}
  operator void *() { return _data; }
  bool Allocate(size_t size)
  {
    if (_data != 0)
      return false;
    _data = ::MidAlloc(size);
    return _data != 0;
  }
  ~CMyBuffer() { ::MidFree(_data); }
};

class CExitEventLauncher
{
public:
  NWindows::NSynchronization::CManualResetEvent _exitEvent;
  bool _needExit;
  CRecordVector< ::CThread > _threads;
  unsigned _numActiveThreads;
    
  CExitEventLauncher()
  {
    _needExit = false;
    if (_exitEvent.Create(false) != S_OK)
      throw 9387173;
    _needExit = true;
    _numActiveThreads = 0;
  };

  ~CExitEventLauncher() { Exit(true); }

  void Exit(bool hardExit);
};

extern CExitEventLauncher g_ExitEventLauncher;

#endif

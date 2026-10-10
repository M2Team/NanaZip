// BrowseDialog2.cpp

#include "StdAfx.h"

#ifdef UNDER_CE
#include <commdlg.h>
#endif

#include <windowsx.h>

#include "../../../Common/IntToString.h"
#include "../../../Common/MyCom.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/Wildcard.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/Menu.h"
#include "../../../Windows/ProcessUtils.h"
#include "../../../Windows/PropVariantConv.h"
#include "../../../Windows/Shell.h"
#include "../../../Windows/Control/ComboBox.h"
#include "../../../Windows/Control/Dialog.h"
#include "../../../Windows/Control/Edit.h"
#include "../../../Windows/Control/ListView.h"

#include "../Explorer/MyMessages.h"

#ifndef Z7_NO_REGISTRY
// **************** NanaZip Modification Start ****************
// #include "HelpUtils.h"
// **************** NanaZip Modification End ****************
#endif

#include "../Common/PropIDUtils.h"

#include "PropertyNameRes.h"
#include "RegistryUtils.h"
#include "SysIconUtils.h"
#include "FormatUtils.h"
#include "LangUtils.h"

#include "resource.h"
#include "BrowseDialog2Res.h"
#include "BrowseDialog2.h"

using namespace NWindows;
using namespace NFile;
using namespace NName;
using namespace NFind;

#ifndef _UNICODE
extern bool g_IsNT;
#endif

extern bool g_LVN_ITEMACTIVATE_Support;

static const int kParentIndex = -1;
// static const UINT k_Message_RefreshPathEdit = WM_APP + 1;


static const wchar_t * const k_Message_Link_operation_was_Blocked =
    // **************** NanaZip Modification Start ****************
    // L"link openning was blocked by 7-Zip";
    L"link opening was blocked by NanaZip";
    // **************** NanaZip Modification End ****************

extern UString HResultToMessage(HRESULT errorCode);

static void MessageBox_HResError(HWND wnd, HRESULT errorCode, const wchar_t *name)
{
  UString s = HResultToMessage(errorCode);
  if (name)
  {
    s.Add_LF();
    s += name;
  }
  ShowErrorMessage(wnd, s);
}

static void MessageBox_LastError_path(HWND wnd, const FString &path)
{
  const HRESULT hres = GetLastError_noZero_HRESULT();
  MessageBox_HResError(wnd, hres, fs2us(path));
}


static const UInt32 k_EnumerateDirsLimit = 200;
static const UInt32 k_EnumerateFilesLimit = 2000;

struct CBrowseItem
{
  unsigned MainFileIndex;
  int SubFileIndex;
  bool WasInterrupted;
  UInt32 NumFiles;
  UInt32 NumDirs;
  UInt32 NumRootItems;
  UInt64 Size;

  void Construct()
  {
    // MainFileIndex = 0;
    SubFileIndex = -1;
    WasInterrupted = false;
    NumFiles = 0;
    NumDirs = 0;
    NumRootItems = 0;
    Size = 0;
  }
};


struct CBrowseEnumerator
{
  FString Path; // folder path without slash at the end
  CFileInfo fi; // temp
  CFileInfo fi_SubFile;
  CBrowseItem bi;

  CBrowseEnumerator()
  {
    bi.Construct();
  }
  void Enumerate(unsigned level);
  bool NeedInterrupt() const
  {
    return bi.NumFiles >= k_EnumerateFilesLimit
        || bi.NumDirs  >= k_EnumerateDirsLimit;
  }
};

void CBrowseEnumerator::Enumerate(unsigned level)
{
  Path.Add_PathSepar();
  const unsigned len = Path.Len();
  CObjectVector<FString> names;
  {
    CEnumerator enumerator;
    enumerator.SetDirPrefix(Path);
    while (enumerator.Next(fi))
    {
      if (level == 0)
      {
        if (bi.NumRootItems == 0)
          fi_SubFile = fi;
        bi.NumRootItems++;
      }

      if (fi.IsDir())
      {
        bi.NumDirs++;
        if (!fi.HasReparsePoint())
          names.Add(fi.Name);
      }
      else
      {
        bi.NumFiles++;
        bi.Size += fi.Size;
      }

      if (level != 0 || bi.NumRootItems > 1)
        if (NeedInterrupt())
        {
          bi.WasInterrupted = true;
          return;
        }
    }
  }

  FOR_VECTOR (i, names)
  {
    if (NeedInterrupt())
    {
      bi.WasInterrupted = true;
      return;
    }
    Path.DeleteFrom(len);
    Path += names[i];
    Enumerate(level + 1);
  }
}




class CBrowseDialog2: public NControl::CModalDialog
{
  CRecordVector<CBrowseItem> _items;
  CObjectVector<CFileInfo> _files;

  NControl::CListView _list;
  // NControl::CEdit _pathEdit;
  NControl::CComboBox _filterCombo;

  CExtToIconMap _extToIconMap;
  int _sortIndex;
  int _columnIndex_fileNameInDir;
  int _columnIndex_NumFiles;
  int _columnIndex_NumDirs;
  bool _ascending;
 #ifndef Z7_SFX
  bool _showDots;
 #endif
  UString _topDirPrefix; // we don't open parent of that folder
  UString DirPrefix;

  virtual bool OnInit() Z7_override;
  virtual bool OnSize(WPARAM wParam, int xSize, int ySize) Z7_override;
  virtual bool OnMessage(UINT message, WPARAM wParam, LPARAM lParam) Z7_override;
  virtual bool OnNotify(UINT controlID, LPNMHDR header) Z7_override;
  // virtual bool OnCommand(unsigned code, unsigned itemID, LPARAM lParam) Z7_override;
  virtual bool OnButtonClicked(unsigned buttonID, HWND buttonHWND) Z7_override;
  virtual void OnOK() Z7_override;

  bool OnKeyDown(LPNMLVKEYDOWN keyDownInfo);

  // void Post_RefreshPathEdit() { PostMsg(k_Message_RefreshPathEdit); }
  bool GetParentPath(const UString &path, UString &parentPrefix, UString &name);
  // Reload changes DirPrefix. Don't send DirPrefix in pathPrefix parameter
  HRESULT Reload(const UString &pathPrefix, const UStringVector &selectedNames, const UString &focusedName);
  HRESULT Reload(const UString &pathPrefix, const UString &selectedNames);
  HRESULT Reload();

  void ChangeSorting_and_Reload(int columnIndex);

  const CFileInfo & Get_MainFileInfo_for_realIndex(unsigned realIndex) const
  {
    return _files[_items[realIndex].MainFileIndex];
  }

  const FString & Get_MainFileName_for_realIndex(unsigned realIndex) const
  {
    return Get_MainFileInfo_for_realIndex(realIndex).Name;
  }

  void Reload_WithErrorMessage();
  void OpenParentFolder();
  // void SetPathEditText();
  void PrintFileProps(UString &s, const CFileInfo &file);
  void Show_FileProps_Window(const CFileInfo &file);
  void OnItemEnter();
  // void FinishOnOK();
  void OnDelete(/* bool toRecycleBin */);
  // **************** NanaZip Modification Start ****************
  // virtual void OnHelp() Z7_override;
  // **************** NanaZip Modification End ****************
  bool OnContextMenu(HANDLE windowHandle, int xPos, int yPos);

  int GetRealItemIndex(int indexInListView) const
  {
    LPARAM param;
    if (!_list.GetItemParam((unsigned)indexInListView, param))
      return (int)-1;
    return (int)param;
  }

  void GetSelected_RealIndexes(CUIntVector &vector);

public:
  // bool TempMode;
  // bool Show_Non7zDirs_InTemp;
  // int FilterIndex;  // [in / out]
  // CObjectVector<CBrowseFilterInfo> Filters;

  UString TempFolderPath; // with slash
  UString Title;

  bool IsExactTempFolder(const UString &pathPrefix) const
  {
    return CompareFileNames(pathPrefix, TempFolderPath) == 0;
  }

  CBrowseDialog2():
   #ifndef Z7_SFX
      _showDots(false)
   #endif
      // , TempMode(false)
      // Show_Non7zDirs_InTemp(false),
      // FilterIndex(-1)
    {}
  INT_PTR Create(HWND parent = NULL) { return CModalDialog::Create(IDD_BROWSE2, parent); }
  int CompareItems(LPARAM lParam1, LPARAM lParam2) const;
};


#ifdef Z7_LANG
static const UInt32 kLangIDs[] =
{
  IDS_BUTTON_DELETE,
  IDM_VIEW_REFRESH
};
#endif

bool CBrowseDialog2::OnInit()
{
  #ifdef Z7_LANG
  LangSetDlgItems(*this, kLangIDs, Z7_ARRAY_SIZE(kLangIDs));
  #endif
  if (!Title.IsEmpty())
    SetText(Title);

  _list.Attach(GetItem(IDL_BROWSE2));
  _filterCombo.Attach(GetItem(IDC_BROWSE2_FILTER));

  _ascending = true;
  _sortIndex = 0;
  _columnIndex_fileNameInDir = -1;
  _columnIndex_NumFiles = -1;
  _columnIndex_NumDirs = -1;
  // _pathEdit.Attach(GetItem(IDE_BROWSE_PATH));

  #ifndef UNDER_CE
  _list.SetUnicodeFormat();
  #endif

  #ifndef Z7_SFX
  {
    CFmSettings st;
    st.Load();

    DWORD extendedStyle = 0;
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
    if (extendedStyle)
      _list.SetExtendedListViewStyle(extendedStyle);
    _showDots = st.ShowDots;
  }
  #endif

  {
    /*
    Filters.Clear(); // for debug
    if (Filters.IsEmpty() && !FolderMode)
    {
      CBrowseFilterInfo &f = Filters.AddNew();
      const UString mask("*.*");
      f.Masks.Add(mask);
      // f.Description = "(";
      f.Description += mask;
      // f.Description += ")";
    }
    */
    // **************** NanaZip Modification Start ****************
    _filterCombo.AddString(L"NanaZip temp files (7z*)");
    // **************** NanaZip Modification End ****************
    _filterCombo.SetCurSel(0);
    EnableItem(IDC_BROWSE2_FILTER, false);
#if 0
    FOR_VECTOR (i, Filters)
    {
      _filterCombo.AddString(Filters[i].Description);
    }
    if (Filters.Size() <= 1)
    {
      EnableItem(IDC_BROWSE_FILTER, false);
    }
    if (/* FilterIndex >= 0 && */ (unsigned)FilterIndex < Filters.Size())
      _filterCombo.SetCurSel(FilterIndex);
#endif
  }

  _list.SetImageList(Shell_Get_SysImageList_smallIcons(true), LVSIL_SMALL);
  _list.SetImageList(Shell_Get_SysImageList_smallIcons(false), LVSIL_NORMAL);

  unsigned columnIndex = 0;
  _list.InsertColumn(columnIndex++, LangString(IDS_PROP_NAME), 100);
  _list.InsertColumn(columnIndex++, LangString(IDS_PROP_MTIME), 100);
  {
    LV_COLUMNW column;
    column.iSubItem = (int)columnIndex;
    column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    column.fmt = LVCFMT_RIGHT;
    column.cx = 100;
    UString s = LangString(IDS_PROP_SIZE);
    column.pszText = s.Ptr_non_const();
    _list.InsertColumn(columnIndex++, &column);

    // if (TempMode)
    {
      _columnIndex_NumFiles = (int)columnIndex;
      s = LangString(IDS_PROP_FILES);
      column.pszText = s.Ptr_non_const();
      _list.InsertColumn(columnIndex++, &column);

      _columnIndex_NumDirs = (int)columnIndex;
      s = LangString(IDS_PROP_FOLDERS);
      column.pszText = s.Ptr_non_const();
      _list.InsertColumn(columnIndex++, &column);

      _columnIndex_fileNameInDir = (int)columnIndex;
      s = LangString(IDS_PROP_NAME);
      s += "-2";
      _list.InsertColumn(columnIndex++, s, 100);
    }
  }

  _list.InsertItem(0, L"12345678901234567"
      #ifndef UNDER_CE
      L"1234567890"
      #endif
      );
  _list.SetSubItem(0, 1, L"2009-09-09"
      #ifndef UNDER_CE
      L" 09:09:09"
      #endif
      );
  _list.SetSubItem(0, 2, L"99999 MB+");

  if (_columnIndex_NumFiles >= 0)
    _list.SetSubItem(0, (unsigned)_columnIndex_NumFiles, L"123456789+");
  if (_columnIndex_NumDirs >= 0)
    _list.SetSubItem(0, (unsigned)_columnIndex_NumDirs, L"123456789+");
  if (_columnIndex_fileNameInDir >= 0)
    _list.SetSubItem(0, (unsigned)_columnIndex_fileNameInDir, L"12345678901234567890");

  for (unsigned i = 0; i < columnIndex; i++)
    _list.SetColumnWidthAuto((int)i);
  _list.DeleteAllItems();

  // if (TempMode)
  {
    _sortIndex = 1; // for MTime column
    // _ascending = false;
  }


  NormalizeSize();

  _topDirPrefix.Empty();
  {
    unsigned rootSize = GetRootPrefixSize(TempFolderPath);
    #if defined(_WIN32) && !defined(UNDER_CE)
    // We can go up from root folder to drives list
    if (IsDrivePath(TempFolderPath))
      rootSize = 0;
    else if (IsSuperPath(TempFolderPath))
    {
      if (IsDrivePath(TempFolderPath.Ptr(kSuperPathPrefixSize)))
        rootSize = kSuperPathPrefixSize;
    }
    #endif
    _topDirPrefix.SetFrom(TempFolderPath, rootSize);
  }

  if (Reload(TempFolderPath, UString()) != S_OK)
  {
    // return false;
  }
/*
  UString name;
  DirPrefix = TempFolderPath;
  for (;;)
  {
    UString baseFolder = DirPrefix;
    if (Reload(baseFolder, name) == S_OK)
      break;
    name.Empty();
    if (DirPrefix.IsEmpty())
      break;
    UString parent, name2;
    GetParentPath(DirPrefix, parent, name2);
    DirPrefix = parent;
  }
*/

  #ifndef UNDER_CE
  /* If we clear UISF_HIDEFOCUS, the focus rectangle in ListView will be visible,
     even if we use mouse for pressing the button to open this dialog. */
  PostMsg(Z7_WIN_WM_UPDATEUISTATE, MAKEWPARAM(Z7_WIN_UIS_CLEAR, Z7_WIN_UISF_HIDEFOCUS));
  #endif

  /*
  */

  return CModalDialog::OnInit();
}


bool CBrowseDialog2::OnSize(WPARAM /* wParam */, int xSize, int ySize)
{
  int mx, my;
  {
    RECT r;
    GetClientRectOfItem(IDS_BUTTON_DELETE, r);
    mx = r.left;
    my = r.top;
  }
  InvalidateRect(NULL);

  const int xLim = xSize - mx;
  {
    RECT r;
    GetClientRectOfItem(IDT_BROWSE2_FOLDER, r);
    MoveItem(IDT_BROWSE2_FOLDER, r.left, r.top, xLim - r.left, RECT_SIZE_Y(r));
  }

  int bx1, bx2, by;
  GetItemSizes(IDCLOSE, bx1, by);
  GetItemSizes(IDHELP,  bx2, by);
  const int y = ySize - my - by;
  const int x = xLim - bx1;
  MoveItem(IDCLOSE, x - mx - bx2, y, bx1, by);
  MoveItem(IDHELP,  x,            y, bx2, by);
  /*
  int yPathSize;
  {
    RECT r;
    GetClientRectOfItem(IDE_BROWSE_PATH, r);
    yPathSize = RECT_SIZE_Y(r);
    _pathEdit.Move(r.left, y - my - yPathSize - my - yPathSize, xLim - r.left, yPathSize);
  }
  */
  // Y_Size of ComboBox is tricky. Can we use it?
  int yFilterSize;
  {
    RECT r;
    GetClientRectOfItem(IDC_BROWSE2_FILTER, r);
    yFilterSize = RECT_SIZE_Y(r);
    _filterCombo.Move(r.left, y - my - yFilterSize, xLim - r.left, yFilterSize);
  }
  {
    RECT r;
    GetClientRectOfItem(IDL_BROWSE2, r);
    _list.Move(r.left, r.top, xLim - r.left, y - my - yFilterSize - my - r.top);
  }
  return false;
}


bool CBrowseDialog2::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  /*
  if (message == k_Message_RefreshPathEdit)
  {
    // SetPathEditText();
    return true;
  }
  */
  if (message == WM_CONTEXTMENU)
  {
    if (OnContextMenu((HANDLE)wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)))
      return true;
  }
  return CModalDialog::OnMessage(message, wParam, lParam);
}


/*
bool CBrowseDialog2::OnCommand(unsigned code, unsigned itemID, LPARAM lParam)
{
  if (code == CBN_SELCHANGE)
  {
    switch (itemID)
    {
      case IDC_BROWSE2_FILTER:
      {
        Reload();
        return true;
      }
    }
  }
  return CModalDialog::OnCommand(code, itemID, lParam);
}
*/

bool CBrowseDialog2::OnNotify(UINT /* controlID */, LPNMHDR header)
{
  if (header->hwndFrom != _list)
  {
    if (::GetParent(header->hwndFrom) == _list)
    {
      // NMHDR:code is UINT
      // NM_RCLICK is unsigned in windows sdk
      // NM_RCLICK is int      in MinGW
      if (header->code == (UINT)NM_RCLICK)
      {
#ifdef UNDER_CE
#define MY_NMLISTVIEW_NMITEMACTIVATE NMLISTVIEW
#else
#define MY_NMLISTVIEW_NMITEMACTIVATE NMITEMACTIVATE
#endif
        MY_NMLISTVIEW_NMITEMACTIVATE *itemActivate = (MY_NMLISTVIEW_NMITEMACTIVATE *)header;
        if (itemActivate->hdr.hwndFrom == HWND(_list))
          return false;
        /*
          POINT point;
          ::GetCursorPos(&point);
          ShowColumnsContextMenu(point.x, point.y);
        */
        // we want to disable menu for columns.
        // to return the value from a dialog procedure we must
        // call SetMsgResult(val) and return true;
        // NM_RCLICK : Return nonzero to not allow the default processing
        SetMsgResult(TRUE); // do not allow default processing
        return true;
      }
    }
    return false;
  }

  switch (header->code)
  {
    case LVN_ITEMACTIVATE:
      if (g_LVN_ITEMACTIVATE_Support)
        OnItemEnter();
      break;
    case NM_DBLCLK:
    case NM_RETURN: // probably it's unused
      if (!g_LVN_ITEMACTIVATE_Support)
        OnItemEnter();
      break;
    case LVN_COLUMNCLICK:
    {
      const int index = LPNMLISTVIEW(header)->iSubItem;
      ChangeSorting_and_Reload(index);
      return false;
    }
    case LVN_KEYDOWN:
    {
      bool boolResult = OnKeyDown(LPNMLVKEYDOWN(header));
      // Post_RefreshPathEdit();
      return boolResult;
    }
    /*
    case NM_RCLICK:
    case NM_CLICK:
    case LVN_BEGINDRAG:
      Post_RefreshPathEdit();
      break;
    */
  }

  return false;
}

bool CBrowseDialog2::OnKeyDown(LPNMLVKEYDOWN keyDownInfo)
{
  const bool ctrl = IsKeyDown(VK_CONTROL);
  // const bool alt = IsKeyDown(VK_MENU);
  // const bool leftCtrl = IsKeyDown(VK_LCONTROL);
  // const bool rightCtrl = IsKeyDown(VK_RCONTROL);
  // const bool shift = IsKeyDown(VK_SHIFT);

  switch (keyDownInfo->wVKey)
  {
    case VK_BACK:
      OpenParentFolder();
      return true;
    case 'R':
      if (ctrl)
      {
        Reload_WithErrorMessage();
        return true;
      }
      return false;
    case VK_F3:
    case VK_F5:
    case VK_F6:
      if (ctrl)
      {
        int index = 0; // name
              if (keyDownInfo->wVKey == VK_F5)  index = 1; // MTime
        else  if (keyDownInfo->wVKey == VK_F6)  index = 2; // Size
        ChangeSorting_and_Reload(index);
        Reload_WithErrorMessage();
        return true;
      }
      return false;
    case 'A':
      if (ctrl)
      {
        // if (TempMode)
          _list.SelectAll();
        return true;
      }
      return false;

    case VK_DELETE:
      // if (TempMode)
        OnDelete(/* !shift */);
      return true;
#if 0
    case VK_NEXT:
    case VK_PRIOR:
    {
      if (ctrl && !alt && !shift)
      {
        if (keyDownInfo->wVKey == VK_NEXT)
          OnItemEnter();
        else
          OpenParentFolder();
        SetMsgResult(TRUE); // to disable processing
        return true;
      }
      break;
    }
#endif
  }
  return false;
}


bool CBrowseDialog2::OnButtonClicked(unsigned buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDB_BROWSE2_PARENT: OpenParentFolder(); break;
    case IDS_BUTTON_DELETE:
    {
      OnDelete(/* !IsKeyDown(VK_SHIFT) */);
      break;
    }
    case IDM_VIEW_REFRESH:
    {
      Reload_WithErrorMessage();
      break;
    }
    default: return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
  }
  _list.SetFocus();
  return true;
}



static void PrintPropsPrefix(UString &s, UInt32 id)
{
  s.Add_LF();
  s += "    ";
  AddLangString(s, id);
  s += ": ";
}

wchar_t *Browse_ConvertSizeToString(UInt64 v, wchar_t *s);

static void Browse_ConvertSizeToString(const CBrowseItem &bi, wchar_t *s)
{
  s = Browse_ConvertSizeToString(bi.Size, s);
  if (bi.WasInterrupted)
  {
    *s++ = '+';
    *s = 0;
  }
}

void AddSizeValue(UString &s, UInt64 value);

static void PrintProps_Size(UString &s, UInt64 size)
{
  PrintPropsPrefix(s, IDS_PROP_SIZE);
#if 1
  AddSizeValue(s, size);
#else
  s.Add_UInt64(size);
  if (size >= 10000)
  {
    s += " (";
    wchar_t temp[64];
    Browse_ConvertSizeToString(size, temp);
    s += temp;
    s.Add_Char(')');
  }
#endif
}

static void PrintProps_MTime(UString &s, const CFileInfo &fi)
{
  PrintPropsPrefix(s, IDS_PROP_MTIME);
  char t[64];
  ConvertUtcFileTimeToString(fi.MTime, t);
  s += t;
}


static void PrintProps_Name(UString &s, const CFileInfo &fi)
{
  s += fs2us(fi.Name);
  if (fi.IsDir())
    s.Add_PathSepar();
}

static void PrintProps_Attrib(UString &s, const CFileInfo &fi)
{
  PrintPropsPrefix(s, IDS_PROP_ATTRIBUTES);
  char props[64];
  ConvertWinAttribToString(props, fi.Attrib);
  s += props;
#if 0
  if (fi.HasReparsePoint())
  {
    s.Add_LF();
    s += "IsLink: +";
  }
#endif
}

static void PrintProps(UString &s, const CBrowseItem &bi,
    const CFileInfo &fi, const CFileInfo *fi2)
{
  PrintProps_Name(s, fi);
  PrintProps_Attrib(s, fi);
  if (bi.NumDirs != 0)
  {
    PrintPropsPrefix(s, IDS_PROP_FOLDERS);
    s.Add_UInt32(bi.NumDirs);
    if (bi.WasInterrupted)
      s += "+";
  }
  if (bi.NumFiles != 0)
  {
    PrintPropsPrefix(s, IDS_PROP_FILES);
    s.Add_UInt32(bi.NumFiles);
    if (bi.WasInterrupted)
      s += "+";
  }
  {
    PrintProps_Size(s, bi.Size);
    if (bi.WasInterrupted)
      s += "+";
  }

  PrintProps_MTime(s, fi);

  if (fi2)
  {
    s.Add_LF();
    s += "----------------";
    s.Add_LF();
    PrintProps_Name(s, *fi2);
    PrintProps_Attrib(s, *fi2);
    if (!fi2->IsDir())
      PrintProps_Size(s, fi2->Size);
    PrintProps_MTime(s, *fi2);
  }
}


void CBrowseDialog2::GetSelected_RealIndexes(CUIntVector &vector)
{
  vector.Clear();
  int index = -1;
  for (;;)
  {
    index = _list.GetNextSelectedItem(index);
    if (index < 0)
      break;
    const int realIndex = GetRealItemIndex(index);
    if (realIndex >= 0)
      vector.Add((unsigned)realIndex);
  }
}


void CBrowseDialog2::PrintFileProps(UString &s, const CFileInfo &file)
{
  CFileInfo file2;
  FString path = us2fs(DirPrefix);
  path += file.Name;
  if (!file2.Find(path))
  {
    MessageBox_LastError_path(*this, path);
    Reload_WithErrorMessage();
    return;
  }
  CBrowseEnumerator enumer;
  enumer.bi.Size = file2.Size;
  if (file2.IsDir() && !file2.HasReparsePoint())
  {
    enumer.Path = path;
    enumer.Enumerate(0); // level
  }
  PrintProps(s, enumer.bi, file2,
      enumer.bi.NumRootItems == 1 ? &enumer.fi_SubFile : NULL);
}


void CBrowseDialog2::Show_FileProps_Window(const CFileInfo &file)
{
  UString s;
  PrintFileProps(s, file);
  MessageBoxW(*this, s, LangString(IDS_PROPERTIES), MB_OK);
}


void CBrowseDialog2::OnDelete(/* bool toRecycleBin */)
{
#if 1
  // we don't want deleting in non temp folders
  if (!DirPrefix.IsPrefixedBy(TempFolderPath))
    return;
#endif

  CUIntVector indices;
  GetSelected_RealIndexes(indices);
  if (indices.Size() == 0)
    return;
  {
    UInt32 titleID, messageID;
    UString messageParam;
    UString s2;
    if (indices.Size() == 1)
    {
      const unsigned index = indices[0];
      const CBrowseItem &bi = _items[index];
      const CFileInfo &file = _files[bi.MainFileIndex];
      PrintFileProps(s2, file);
      messageParam = fs2us(file.Name);
      if (file.IsDir())
      {
        titleID = IDS_CONFIRM_FOLDER_DELETE;
        messageID = IDS_WANT_TO_DELETE_FOLDER;
      }
      else
      {
        titleID = IDS_CONFIRM_FILE_DELETE;
        messageID = IDS_WANT_TO_DELETE_FILE;
      }
    }
    else
    {
      titleID = IDS_CONFIRM_ITEMS_DELETE;
      messageID = IDS_WANT_TO_DELETE_ITEMS;
      messageParam = NumberToString(indices.Size());

      for (UInt32 i = 0; i < indices.Size(); i++)
      {
        if (i >= 10)
        {
          s2 += "...";
          break;
        }
        const CBrowseItem &bi = _items[indices[i]];
        const CFileInfo &fi = _files[bi.MainFileIndex];
        PrintProps_Name(s2, fi);
        s2.Add_LF();
      }
    }
    UString s = MyFormatNew(messageID, messageParam);
    if (!s2.IsEmpty())
    {
      s.Add_LF();
      s.Add_LF();
      s += s2;
    }
    if (::MessageBoxW((HWND)*this, s, LangString(titleID),
        MB_YESNOCANCEL | MB_ICONQUESTION) != IDYES)
      return;
  }

  for (UInt32 i = 0; i < indices.Size(); i++)
  {
    const unsigned index = indices[i];
    bool result = true;
    const CBrowseItem &bi = _items[index];
    const CFileInfo &fi = _files[bi.MainFileIndex];
    if (fi.Name.IsEmpty())
      return; // some error
    const FString fullPath = us2fs(DirPrefix) + fi.Name;
    if (fi.IsDir())
      result = NFile::NDir::RemoveDirWithSubItems(fullPath);
    else
      result = NFile::NDir::DeleteFileAlways(fullPath);
    if (!result)
    {
      MessageBox_LastError_path(*this, fullPath);
      return;
    }
  }

  Reload_WithErrorMessage();
}


// **************** NanaZip Modification Start ****************
// Removed from NanaZip.
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
#ifndef Z7_NO_REGISTRY
#define kHelpTopic "fm/temp.htm"
void CBrowseDialog2::OnHelp()
{
  // **************** NanaZip Modification Start ****************
  // ShowHelpWindow(kHelpTopic);
  // **************** NanaZip Modification End ****************
  CModalDialog::OnHelp();
}
#endif
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
// **************** NanaZip Modification End ****************


HRESULT ShellFolder_ParseDisplayName(IShellFolder *shellFolder,
    HWND hwnd, const UString &path, LPITEMIDLIST *ppidl);

HRESULT StartApplication(const UString &dir, const UString &path, HWND window, CProcess &process);
HRESULT StartApplication(const UString &dir, const UString &path, HWND window, CProcess &process)
{
  UString path2 = path;
  UINT32 result;
  {
#ifdef _WIN32
  NShell::CItemIDList pidl;
  // SHELLEXECUTEINFO::pidl is more accurate way than SHELLEXECUTEINFO::lpFile
  {
    CMyComPtr<IShellFolder> desktop;
    if (SHGetDesktopFolder(&desktop) == S_OK && desktop)
      if (ShellFolder_ParseDisplayName(desktop,
          NULL, // HWND : do we need (window) or NULL here?
          path,
          &pidl) != S_OK)
        pidl.Detach();
  }
  {
    const int dot = path2.ReverseFind_Dot();
    const int separ = path2.ReverseFind_PathSepar();
    if (separ != (int)path2.Len() - 1)
      if (dot < 0 || dot < separ)
        path2.Add_Dot();
  }
#endif // _WIN32

#ifndef _UNICODE
  if (g_IsNT)
  {
    SHELLEXECUTEINFOW execInfo;
    memset(&execInfo, 0, sizeof(execInfo));
    // execInfo.hwnd = NULL;
    // execInfo.lpVerb = NULL;
    // execInfo.lpFile = NULL;
    // execInfo.lpDirectory = NULL;
    // execInfo.lpParameters = NULL;
    // execInfo.hProcess = NULL;
    execInfo.cbSize = sizeof(execInfo);
    execInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_DDEWAIT;
    if (!dir.IsEmpty())
      execInfo.lpDirectory = dir;
    execInfo.nShow = SW_SHOWNORMAL;

    if ((LPCITEMIDLIST)pidl)
    {
      execInfo.lpIDList = pidl;
      execInfo.fMask |= SEE_MASK_IDLIST;
    }
    else
      execInfo.lpFile = path2;

typedef BOOL (WINAPI * Func_ShellExecuteExW)(LPSHELLEXECUTEINFOW lpExecInfo);
Z7_DIAGNOSTIC_IGNORE_CAST_FUNCTION
    const
    Func_ShellExecuteExW
       f_ShellExecuteExW = Z7_GET_PROC_ADDRESS(
    Func_ShellExecuteExW, ::GetModuleHandleW(L"shell32.dll"),
        "ShellExecuteExW");
    if (!f_ShellExecuteExW)
      return 0;
    f_ShellExecuteExW(&execInfo);
    result = (UINT32)(UINT_PTR)execInfo.hInstApp;
    process.Attach(execInfo.hProcess);
  }
  else
#endif
  {
    SHELLEXECUTEINFO execInfo;
    memset(&execInfo, 0, sizeof(execInfo));
    // execInfo.hwnd = NULL;
    // execInfo.lpVerb = NULL;
    // execInfo.lpFile = NULL;
    // execInfo.lpDirectory = NULL;
    // execInfo.lpParameters = NULL;
    // execInfo.hProcess = NULL;
    execInfo.cbSize = sizeof(execInfo);
    execInfo.fMask = SEE_MASK_NOCLOSEPROCESS
      #ifndef UNDER_CE
      | SEE_MASK_FLAG_DDEWAIT
      #endif
      ;
    execInfo.nShow = SW_SHOWNORMAL;
    const CSysString sysPath (GetSystemString(path2));
    const CSysString sysDir (GetSystemString(dir));
    #ifndef UNDER_CE
    if (!sysDir.IsEmpty())
      execInfo.lpDirectory = sysDir;
    #endif

    if ((LPCITEMIDLIST)pidl)
    {
      execInfo.lpIDList = pidl;
      execInfo.fMask |= SEE_MASK_IDLIST;
    }
    else
      execInfo.lpFile = sysPath;
    ::ShellExecuteEx(&execInfo);
    result = (UINT32)(UINT_PTR)execInfo.hInstApp;
    process.Attach(execInfo.hProcess);
  }
  // DEBUG_PRINT_NUM("-- ShellExecuteEx -- execInfo.hInstApp = ", result)
  }
  if (result <= 32)
  {
    switch (result)
    {
      case SE_ERR_NOASSOC:
        MessageBox_HResError(window,
          GetLastError_noZero_HRESULT(),
          NULL
          // L"There is no application associated with the given file name extension",
          );
    }
    return E_FAIL; // fixed in 15.13. Can we use it for any Windows version?
  }
  return S_OK;
}

void StartApplicationDontWait(const UString &dir, const UString &path, HWND window);
void StartApplicationDontWait(const UString &dir, const UString &path, HWND window)
{
  CProcess process;
  StartApplication(dir, path, window, process);
}


static UString GetQuotedString2(const UString &s)
{
  UString s2 ('\"');
  s2 += s;
  s2.Add_Char('\"');
  return s2;
}


bool CBrowseDialog2::OnContextMenu(HANDLE windowHandle, int xPos, int yPos)
{
  if (windowHandle != _list)
    return false;

  CUIntVector indices;
  GetSelected_RealIndexes(indices);

  // negative x,y are possible for multi-screen modes.
  // x=-1 && y=-1 for keyboard call (SHIFT+F10 and others).
#if 1 // 0 : for debug
  if (xPos == -1 && yPos == -1)
#endif
  {
/*
    if (indices.Size() == 0)
    {
      xPos = 0;
      yPos = 0;
    }
    else
*/
    {
      const int itemIndex = _list.GetFocusedItem();
      if (itemIndex == -1)
        return false;
      RECT rect;
      if (!_list.GetItemRect(itemIndex, &rect, LVIR_ICON))
        return false;
      // rect : rect of file icon relative to listVeiw.
      xPos = (rect.left + rect.right) / 2;
      yPos = (rect.top + rect.bottom) / 2;
      RECT r;
      GetClientRectOfItem(IDL_BROWSE2, r);
      if (yPos < 0 || yPos >= RECT_SIZE_Y(r))
        yPos = 0;
    }
    POINT point = {xPos, yPos};
    _list.ClientToScreen(&point);
    xPos = point.x;
    yPos = point.y;
  }

  const UInt32 k_CmdId_Delete = 1;
  const UInt32 k_CmdId_Open_Explorer = 2;
  const UInt32 k_CmdId_Open_7zip = 3;
  const UInt32 k_CmdId_Props = 4;
  int menuResult;
  {
    CMenu menu;
    CMenuDestroyer menuDestroyer(menu);
    menu.CreatePopup();

    unsigned numMenuItems = 0;
    // unsigned defaultCmd = 0;

    for (unsigned cmd = k_CmdId_Delete; cmd <= k_CmdId_Props; cmd++)
    {
      if (cmd == k_CmdId_Delete)
      {
        if (/* !TempMode || */ indices.Size() == 0)
          continue;
        // defaultCmd = cmd;
      }
      else if (indices.Size() > 1)
        break;


      if (numMenuItems != 0)
      {
        if (cmd == k_CmdId_Open_Explorer)
          menu.AppendItem(MF_SEPARATOR, 0, (LPCTSTR)NULL);
        if (cmd == k_CmdId_Props)
          menu.AppendItem(MF_SEPARATOR, 0, (LPCTSTR)NULL);
      }

      const UINT flags = MF_STRING;
      UString s;
      if (cmd == k_CmdId_Delete)
      {
        s = LangString(IDS_BUTTON_DELETE);
        s += "\tDelete";
      }
      else if (cmd == k_CmdId_Open_Explorer)
      {
        s = LangString(IDM_OPEN_OUTSIDE);
        if (s.IsEmpty())
          s = "Open Outside";
        s += "\tShift+Enter";
      }
      else if (cmd == k_CmdId_Open_7zip)
      {
        s = LangString(IDM_OPEN_OUTSIDE);
        if (s.IsEmpty())
          s = "Open Outside";
        // **************** NanaZip Modification Start ****************
        // s += " : 7-Zip";
        s += " : NanaZip";
        // **************** NanaZip Modification End ****************
      }
      else if (cmd == k_CmdId_Props)
      {
        s = LangString(IDS_PROPERTIES);
        if (s.IsEmpty())
          s = "Properties";
        s += "\tAlt+Enter";
      }
      else
        break;
      s.RemoveChar(L'&');
      menu.AppendItem(flags, cmd, s);
      numMenuItems++;
    }
    // default item is useless for us
    /*
    if (defaultCmd != 0)
      SetMenuDefaultItem(menu, (unsigned)defaultCmd,
        FALSE); // byPos
    */
    /* hwnd for TrackPopupMenuEx(): DOCS:
      A handle to the window that owns the shortcut menu.
      This window receives all messages from the menu.
      The window does not receive a WM_COMMAND message from the menu
      until the function returns.
      If you specify TPM_NONOTIFY in the fuFlags parameter,
      the function does not send messages to the window identified by hwnd.
    */
    if (numMenuItems == 0)
      return true;
    menuResult = menu.Track(TPM_LEFTALIGN | TPM_RETURNCMD | TPM_NONOTIFY,
        xPos, yPos, *this);
    /* menu.Track() return value is zero, if the user cancels
       the menu without making a selection, or if an error occurs */
    if (menuResult <= 0)
      return true;
  }

  if (menuResult == k_CmdId_Delete)
  {
    OnDelete(/* !IsKeyDown(VK_SHIFT) */);
    return true;
  }

  if (indices.Size() <= 1)
  {
    UString fullPath = DirPrefix;
    if (indices.Size() != 0)
    {
      const CBrowseItem &bi = _items[indices[0]];
      const CFileInfo &file = _files[bi.MainFileIndex];
      if (file.HasReparsePoint())
      {
        // we don't want external program was used to work with Links
        ShowErrorMessage(*this, k_Message_Link_operation_was_Blocked);
        return true;
      }
      fullPath += fs2us(file.Name);
    }
    if (menuResult == k_CmdId_Open_Explorer)
    {
      StartApplicationDontWait(DirPrefix, fullPath, (HWND)*this);
      return true;
    }

    if (menuResult == k_CmdId_Open_7zip)
    {
      UString imageName = fs2us(NWindows::NDLL::GetModuleDirPrefix());
      imageName += "7zFM.exe";
      WRes wres;
      {
        CProcess process;
        wres = process.Create(imageName, GetQuotedString2(fullPath), NULL); // curDir
      }
      if (wres != 0)
      {
        const HRESULT hres = HRESULT_FROM_WIN32(wres);
        MessageBox_HResError(*this, hres, imageName);
      }
      return true;
    }

    if (indices.Size() == 1)
    if (menuResult == k_CmdId_Props)
    {
      const CBrowseItem &bi = _items[indices[0]];
      const CFileInfo &file = _files[bi.MainFileIndex];
      Show_FileProps_Window(file);
      return true;
    }
  }

  return true;
}



struct CWaitCursor2
{
  HCURSOR _waitCursor;
  HCURSOR _oldCursor;

  CWaitCursor2():
      _waitCursor(NULL),
      _oldCursor(NULL)
    {}
  void Set()
  {
    if (!_waitCursor)
    {
      _waitCursor = LoadCursor(NULL, IDC_WAIT);
      if (_waitCursor)
        _oldCursor = SetCursor(_waitCursor);
    }
  }
  ~CWaitCursor2()
  {
    if (_waitCursor)
      SetCursor(_oldCursor);
  }
};


void CBrowseDialog2::OnOK()
{
  /* DOCS:
    If a dialog box or one of its controls currently has the input focus,
    then pressing the ENTER key causes Windows to send a WM_COMMAND message
    with the idItem (wParam) parameter set to the ID of the default command button.
    If the dialog box does not have a default command button,
    then the idItem parameter is set to IDOK by default.

    We process IDOK here for Enter pressing, because we have no DEFPUSHBUTTON.
  */
  if (GetFocus() == _list)
  {
    OnItemEnter();
    return;
  }
  // Enter can be pressed in another controls (Edit).
  // So we don't need End() call here
}


bool CBrowseDialog2::GetParentPath(const UString &path, UString &parentPrefix, UString &name)
{
  parentPrefix.Empty();
  name.Empty();
  if (path.IsEmpty())
    return false;
  if (_topDirPrefix == path)
    return false;
  UString s = path;
  if (IS_PATH_SEPAR(s.Back()))
    s.DeleteBack();
  if (s.IsEmpty())
    return false;
  if (IS_PATH_SEPAR(s.Back()))
    return false;
  const unsigned pos1 = (unsigned)(s.ReverseFind_PathSepar() + 1);
  parentPrefix.SetFrom(s, pos1);
  name = s.Ptr(pos1);
  return true;
}


int CBrowseDialog2::CompareItems(LPARAM lParam1, LPARAM lParam2) const
{
  if (lParam1 == lParam2)      return 0;
  if (lParam1 == kParentIndex) return -1;
  if (lParam2 == kParentIndex) return 1;

  const int index1 = (int)lParam1;
  const int index2 = (int)lParam2;

  const CBrowseItem &item1 = _items[index1];
  const CBrowseItem &item2 = _items[index2];

  const CFileInfo &f1 = _files[item1.MainFileIndex];
  const CFileInfo &f2 = _files[item2.MainFileIndex];

  const bool isDir2 = f2.IsDir();
  if (f1.IsDir())
  {
    if (!isDir2) return -1;
  }
  else if (isDir2) return 1;

  const int res2 = MyCompare(index1, index2);
  int res = 0;
  switch (_sortIndex)
  {
    case 0: res = CompareFileNames(fs2us(f1.Name), fs2us(f2.Name)); break;
    case 1: res = CompareFileTime(&f1.MTime, &f2.MTime); break;
    case 2: res = MyCompare(item1.Size, item2.Size); break;
    case 3: res = MyCompare(item1.NumFiles, item2.NumFiles); break;
    case 4: res = MyCompare(item1.NumDirs, item2.NumDirs); break;
    case 5:
    {
      const int sub1 = item1.SubFileIndex;
      const int sub2 = item2.SubFileIndex;
      if (sub1 < 0)
      {
        if (sub2 >= 0)
          res = -1;
      }
      else if (sub2 < 0)
        res = 1;
      else
        res = CompareFileNames(fs2us(_files[sub1].Name), fs2us(_files[sub2].Name));
      break;
    }
  }
  if (res == 0)
    res = res2;
  return _ascending ? res: -res;
}

static int CALLBACK CompareItems2(LPARAM lParam1, LPARAM lParam2, LPARAM lpData)
{
  return ((CBrowseDialog2 *)lpData)->CompareItems(lParam1, lParam2);
}


static const FChar *FindNonHexChar_F(const FChar *s Z7_lifetimebound) throw()
{
  for (;;)
  {
    const FChar c = (FChar)*s++; // pointer can go 1 byte after end
    if ( (c < '0' || c > '9')
      && (c < 'a' || c > 'z')
      && (c < 'A' || c > 'Z'))
    return s - 1;
  }
}


void CBrowseDialog2::Reload_WithErrorMessage()
{
  const HRESULT res = Reload();
  if (res != S_OK)
    MessageBox_HResError(*this, res, DirPrefix);
}

void CBrowseDialog2::ChangeSorting_and_Reload(int columnIndex)
{
  if (columnIndex == _sortIndex)
    _ascending = !_ascending;
  else
  {
    _ascending = (columnIndex == 0 || columnIndex == _columnIndex_fileNameInDir); // for name columns
    _sortIndex = columnIndex;
  }
  Reload_WithErrorMessage();
}


// Reload changes DirPrefix. Don't send DirPrefix in pathPrefix parameter
HRESULT CBrowseDialog2::Reload(const UString &pathPrefix, const UString &selectedName)
{
  UStringVector selectedVector;
  if (!selectedName.IsEmpty())
    selectedVector.Add(selectedName);
  return Reload(pathPrefix, selectedVector, selectedName);
}


HRESULT CBrowseDialog2::Reload(const UString &pathPrefix, const UStringVector &selectedVector2, const UString &focusedName)
{
  UStringVector selectedVector = selectedVector2;
  selectedVector.Sort();
  CObjectVector<CFileInfo> files;
  CRecordVector<CBrowseItem> items;
  CWaitCursor2 waitCursor;

  #ifndef UNDER_CE
  bool isDrive = false;
  if (pathPrefix.IsEmpty() || pathPrefix.IsEqualTo(kSuperPathPrefix))
  {
    isDrive = true;
    FStringVector drives;
    if (!MyGetLogicalDriveStrings(drives))
      return GetLastError_noZero_HRESULT();
    FOR_VECTOR (i, drives)
    {
      const FString &d = drives[i];
      if (d.Len() < 2 || d.Back() != '\\')
        return E_FAIL;
      CBrowseItem item;
      item.Construct();
      item.MainFileIndex = files.Size();
      CFileInfo &fi = files.AddNew();
      fi.SetAsDir();
      fi.Name = d;
      fi.Name.DeleteBack();
      items.Add(item);
    }
  }
  else
  #endif
  {
    {
      CEnumerator enumerator;
      enumerator.SetDirPrefix(us2fs(pathPrefix));
      CFileInfo fi;
      FString tail;

      const bool isTempFolder = (
          // TempMode &&
          IsExactTempFolder(pathPrefix)
          );
      for (;;)
      {
        {
          bool found;
          if (!enumerator.Next(fi, found))
            return GetLastError_noZero_HRESULT();
          if (!found)
            break;
        }
        if (isTempFolder)
        {
          // if (!Show_Non7zDirs_InTemp)
          {
            if (!fi.Name.IsPrefixedBy_Ascii_NoCase("7z"))
              continue;
            tail = fi.Name.Ptr(2);
            if ( !tail.IsPrefixedBy_Ascii_NoCase("E") // drag and drop / Copy / create to email
              && !tail.IsPrefixedBy_Ascii_NoCase("O") // open
              && !tail.IsPrefixedBy_Ascii_NoCase("S")) // SFXSetup
               continue;
            const FChar *beg = tail.Ptr(1);
            const FChar *end = FindNonHexChar_F(beg);
            if (end - beg != 8 || *end != 0)
              continue;
          }
        }
        CBrowseItem item;
        item.Construct();
        item.MainFileIndex = files.Size();
        item.Size = fi.Size;
        files.Add(fi);
        items.Add(item);
      }
    }

    UInt64 cnt = items.Size();
    // if (TempMode)
    {
      FOR_VECTOR (i, items)
      {
        CBrowseItem &item = items[i];
        const CFileInfo &fi = files[item.MainFileIndex];
        if (!fi.IsDir() || fi.HasReparsePoint())
          continue;

        CBrowseEnumerator enumer;
        // we need to keep MainFileIndex and Size value of item:
        enumer.bi = item; // don't change it
        enumer.Path = us2fs(pathPrefix);
        enumer.Path += fi.Name;
        enumer.Enumerate(0); // level
        item = enumer.bi;
        if (item.NumRootItems == 1)
        {
          item.SubFileIndex = (int)files.Size();
          files.Add(enumer.fi_SubFile);
        }
        cnt += item.NumDirs;
        cnt += item.NumFiles;
        if (cnt > 1000)
          waitCursor.Set();
      }
    }
  }
  _items = items;
  _files = files;

  DirPrefix = pathPrefix;

  EnableItem(IDB_BROWSE2_PARENT, !IsExactTempFolder(pathPrefix));

  SetItemText(IDT_BROWSE2_FOLDER, DirPrefix);

  _list.SetRedraw(false);
  _list.DeleteAllItems();

  LVITEMW item;

  unsigned index = 0;
  int cursorIndex = -1;

  #ifndef Z7_SFX
  if (_showDots && _topDirPrefix != DirPrefix)
  {
    item.iItem = (int)index;
    const UString itemName ("..");
    if (focusedName == itemName)
      cursorIndex = (int)index;
    /*
    if (selectedVector.IsEmpty()
        // && focusedName.IsEmpty()
        // && focusedName == ".."
        )
      cursorIndex = (int)index;
    */
    item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
    unsigned subItem = 0;
    item.iSubItem = (int)(subItem++);
    item.lParam = kParentIndex;
    item.pszText = itemName.Ptr_non_const();
    item.iImage = _extToIconMap.GetIconIndex(FILE_ATTRIBUTE_DIRECTORY, DirPrefix);
    if (item.iImage < 0)
      item.iImage = 0;
    _list.InsertItem(&item);
#if 0
    for (int k = 1; k < 6; k++)
      _list.SetSubItem(index, subItem++, L"2");
#endif
    index++;
  }
  #endif

  for (unsigned i = 0; i < _items.Size(); i++, index++)
  {
    item.iItem = (int)index;
    const CBrowseItem &bi = _items[i];
    const CFileInfo &fi = _files[bi.MainFileIndex];
    const UString name = fs2us(fi.Name);
    // if (!selectedName.IsEmpty() && CompareFileNames(name, selectedName) == 0)
    item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
    item.state = 0;
    if (selectedVector.FindInSorted(name) != -1)
    {
      /*
      if (cursorIndex == -1)
        cursorIndex = (int)index;
      */
      item.mask |= LVIF_STATE;
      item.state |= LVIS_SELECTED;
    }
    if (focusedName == name)
    {
      if (cursorIndex == -1)
        cursorIndex = (int)index;
      item.mask |= LVIF_STATE;
      item.state |= LVIS_FOCUSED;
    }

    unsigned subItem = 0;
    item.iSubItem = (int)(subItem++);
    item.lParam = (LPARAM)i;
    item.pszText = name.Ptr_non_const();

    const UString fullPath = DirPrefix + name;
    #ifndef UNDER_CE
    if (isDrive)
    {
      item.iImage = Shell_GetFileInfo_SysIconIndex_for_Path(
          fi.Name + FCHAR_PATH_SEPARATOR,
          FILE_ATTRIBUTE_DIRECTORY);
    }
    else
    #endif
      item.iImage = _extToIconMap.GetIconIndex(fi.Attrib, fullPath);
    if (item.iImage < 0)
        item.iImage = 0;
    _list.InsertItem(&item);
    wchar_t s[64];
    {
      s[0] = 0;
      if (!FILETIME_IsZero(fi.MTime))
        ConvertUtcFileTimeToString(fi.MTime, s,
            #ifndef UNDER_CE
              kTimestampPrintLevel_MIN
            #else
              kTimestampPrintLevel_DAY
            #endif
              );
      _list.SetSubItem(index, subItem++, s);
    }
    {
      s[0] = 0;
      Browse_ConvertSizeToString(bi, s);
      _list.SetSubItem(index, subItem++, s);
    }
    if (_columnIndex_NumFiles >= 0)
    {
      UString s2;
      if (fi.HasReparsePoint())
      {
        s2 = "Link";
      }
      else if (bi.NumFiles != 0)
      {
        s2.Add_UInt32(bi.NumFiles);
        if (bi.WasInterrupted)
          s2 += "+";
      }
      _list.SetSubItem(index, subItem, s2);
    }
    subItem++;
    if (_columnIndex_NumDirs >= 0 && bi.NumDirs != 0)
    {
      UString s2;
      s2.Add_UInt32(bi.NumDirs);
      if (bi.WasInterrupted)
        s2 += "+";
      _list.SetSubItem(index, subItem, s2);
    }
    subItem++;
    if (_columnIndex_fileNameInDir >= 0 && bi.SubFileIndex >= 0)
    {
      _list.SetSubItem(index, subItem, fs2us(_files[bi.SubFileIndex].Name));
    }
    subItem++;
  }

  if (_list.GetItemCount() > 0 && cursorIndex >= 0)
  {
    // _list.SetItemState_FocusedSelected(cursorIndex);
    // _list.SetItemState_Focused(cursorIndex);
  }
  _list.SortItems(CompareItems2, (LPARAM)this);
  if (_list.GetItemCount() > 0 && cursorIndex < 0)
  {
    if (selectedVector.IsEmpty())
      _list.SetItemState_FocusedSelected(0);
    else
       _list.SetItemState(0, LVIS_FOCUSED, LVIS_FOCUSED);
  }
  _list.EnsureVisible(_list.GetFocusedItem(), false);
  _list.SetRedraw(true);
  _list.InvalidateRect(NULL, true);
  return S_OK;
}



HRESULT CBrowseDialog2::Reload()
{
  UStringVector selected;
  {
    CUIntVector indexes;
    GetSelected_RealIndexes(indexes);
    FOR_VECTOR (i, indexes)
      selected.Add(fs2us(Get_MainFileName_for_realIndex(indexes[i])));
  }
  UString focusedName;
  const int focusedItem = _list.GetFocusedItem();
  if (focusedItem >= 0)
  {
    const int realIndex = GetRealItemIndex(focusedItem);
    if (realIndex != kParentIndex)
      focusedName = fs2us(Get_MainFileName_for_realIndex((unsigned)realIndex));
  }
  const UString dirPathTemp = DirPrefix;
  return Reload(dirPathTemp, selected, focusedName);
}


void CBrowseDialog2::OpenParentFolder()
{
#if 1 // 0 : for debug
  // we don't allow to go to parent of TempFolder.
  // if (TempMode)
  {
    if (IsExactTempFolder(DirPrefix))
      return;
  }
#endif

  UString parent, selected;
  if (GetParentPath(DirPrefix, parent, selected))
    Reload(parent, selected);
}


void CBrowseDialog2::OnItemEnter()
{
  const bool alt = IsKeyDown(VK_MENU);
  const bool ctrl = IsKeyDown(VK_CONTROL);
  const bool shift = IsKeyDown(VK_SHIFT);

  const int index = _list.GetNextSelectedItem(-1);
  if (index < 0)
    return;
  if (_list.GetNextSelectedItem(index) >= 0)
    return; // more than one selected
  const int realIndex = GetRealItemIndex(index);
  if (realIndex == kParentIndex)
    OpenParentFolder();
  else
  {
    const CBrowseItem &bi = _items[realIndex];
    const CFileInfo &file = _files[bi.MainFileIndex];
    if (alt)
    {
      Show_FileProps_Window(file);
      return;
    }
    if (file.HasReparsePoint())
    {
      // we don't want Link open operation,
      // because user can think that it's usual folder/file (non-link).
      ShowErrorMessage(*this, k_Message_Link_operation_was_Blocked);
      return;
    }
    bool needExternal = true;
    if (file.IsDir())
    {
      if (!shift || alt || ctrl) // open folder in Explorer:
        needExternal = false;
    }
    const UString fullPath = DirPrefix + fs2us(file.Name);
    if (needExternal)
    {
      StartApplicationDontWait(DirPrefix, fullPath, (HWND)*this);
      return;
    }
    UString s = fullPath;
    s.Add_PathSepar();
    const HRESULT res = Reload(s, UString());
    if (res != S_OK)
      MessageBox_HResError(*this, res, s);
    // SetPathEditText();
  }
}


void MyBrowseForTempFolder(HWND owner)
{
  FString tempPathF;
  if (!NFile::NDir::MyGetTempPath(tempPathF) || tempPathF.IsEmpty())
  {
    MessageBox_LastError_path(owner, tempPathF);
    return;
  }
  CBrowseDialog2 dialog;

  LangString_OnlyFromLangFile(IDM_TEMP_DIR, dialog.Title);
  dialog.Title.Replace(L"...", L"");
  if (dialog.Title.IsEmpty())
    dialog.Title = "Delete Temporary Files";

  dialog.TempFolderPath = fs2us(tempPathF);
  dialog.Create(owner);
  // we can exit from dialog with 2 ways:
  // IDCANCEL : Esc Key, or close icons
  // IDCLOSE  : with Close button
}

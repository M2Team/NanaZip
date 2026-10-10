// PanelItems.cpp

#include "StdAfx.h"

#include "../../../../C/Sort.h"

#include "../../../Windows/FileName.h"
#include "../../../Windows/Menu.h"
#include "../../../Windows/PropVariant.h"
#include "../../../Windows/PropVariantConv.h"

#include "../../PropID.h"

#include "../Common/ExtractingFilePath.h"

#include "resource.h"

#include "LangUtils.h"
#include "Panel.h"
#include "PropertyName.h"
#include "RootFolder.h"

using namespace NWindows;

static bool GetColumnVisible(PROPID propID, bool isFsFolder)
{
  if (isFsFolder)
  {
    switch (propID)
    {
      case kpidATime:
      case kpidChangeTime:
      case kpidAttrib:
      case kpidPackSize:
      case kpidINode:
      case kpidLinks:
      case kpidNtReparse:
        return false;
    }
  }
  return true;
}

static unsigned GetColumnWidth(PROPID propID, VARTYPE /* varType */)
{
  switch (propID)
  {
    case kpidName: return 160;
  }
  return 100;
}

static int GetColumnAlign(PROPID propID, VARTYPE varType)
{
  switch (propID)
  {
    case kpidCTime:
    case kpidATime:
    case kpidMTime:
    case kpidChangeTime:
      return LVCFMT_LEFT;
  }

  switch (varType)
  {
    case VT_UI1:
    case VT_I2:
    case VT_UI2:
    case VT_I4:
    case VT_INT:
    case VT_UI4:
    case VT_UINT:
    case VT_I8:
    case VT_UI8:
    case VT_BOOL:
      return LVCFMT_RIGHT;

    case VT_EMPTY:
    case VT_I1:
    case VT_FILETIME:
    case VT_BSTR:
      return LVCFMT_LEFT;

    default:
      return LVCFMT_CENTER;
  }
}


static int ItemProperty_Compare_NameFirst(void *const *a1, void *const *a2, void * /* param */)
{
  return (*(*((const CPropColumn *const *)a1))).Compare_NameFirst
         (*(*((const CPropColumn *const *)a2)));
}

HRESULT CPanel::InitColumns()
{
  SaveListViewInfo();

  // DeleteListItems();
  _selectedStatusVector.Clear();

  {
    // ReadListViewInfo();
    const UString oldType = _typeIDString;
    _typeIDString = GetFolderTypeID();
    // an empty _typeIDString is allowed.

    // we read registry only for new FolderTypeID
    if (!_needSaveInfo || _typeIDString != oldType)
      _listViewInfo.Read(_typeIDString);

    // folders with same FolderTypeID can have different columns
    // so we still read columns for that case.
    // if (_needSaveInfo && _typeIDString == oldType) return S_OK;
  }

  // PROPID sortID;
  /*
  if (_listViewInfo.SortIndex >= 0)
    sortID = _listViewInfo.Columns[_listViewInfo.SortIndex].PropID;
  */
  // sortID = _listViewInfo.SortID;

  _ascending = _listViewInfo.Ascending;

  _columns.Clear();

  const bool isFsFolder = IsFSFolder() || IsAltStreamsFolder();

  {
    UInt32 numProps;
    _folder->GetNumberOfProperties(&numProps);

    for (UInt32 i = 0; i < numProps; i++)
    {
      CMyComBSTR name;
      PROPID propID;
      VARTYPE varType;
      HRESULT res = _folder->GetPropertyInfo(i, &name, &propID, &varType);

      if (res != S_OK)
      {
        /* We can return ERROR, but in that case, other code will not be called,
           and user can see empty window without error message. So we just ignore that field */
        continue;
      }
      if (propID == kpidIsDir)
        continue;
      CPropColumn prop;
      prop.Type = varType;
      prop.ID = propID;
      prop.Name = GetNameOfProperty(propID, name);
      prop.Order = -1;
      prop.IsVisible = GetColumnVisible(propID, isFsFolder);
      prop.Width = GetColumnWidth(propID, varType);
      prop.IsRawProp = false;
      _columns.Add(prop);
    }

    /*
    {
      // debug column
      CPropColumn prop;
      prop.Type = VT_BSTR;
      prop.ID = 2000;
      prop.Name = "Debug";
      prop.Order = -1;
      prop.IsVisible = true;
      prop.Width = 300;
      prop.IsRawProp = false;
      _columns.Add(prop);
    }
    */
  }

  if (_folderRawProps)
  {
    UInt32 numProps;
    _folderRawProps->GetNumRawProps(&numProps);

    for (UInt32 i = 0; i < numProps; i++)
    {
      CMyComBSTR name;
      PROPID propID;
      const HRESULT res = _folderRawProps->GetRawPropInfo(i, &name, &propID);
      if (res != S_OK)
        continue;
      CPropColumn prop;
      prop.Type = VT_EMPTY;
      prop.ID = propID;
      prop.Name = GetNameOfProperty(propID, name);
      prop.Order = -1;
      prop.IsVisible = GetColumnVisible(propID, isFsFolder);
      prop.Width = GetColumnWidth(propID, VT_BSTR);
      prop.IsRawProp = true;
      _columns.Add(prop);
    }
  }

  unsigned order = 0;
  unsigned i;

  for (i = 0; i < _listViewInfo.Columns.Size(); i++)
  {
    const CColumnInfo &columnInfo = _listViewInfo.Columns[i];
    const int index = _columns.FindItem_for_PropID(columnInfo.PropID);
    if (index >= 0)
    {
      CPropColumn &item = _columns[index];
      if (item.Order >= 0)
        continue; // we ignore duplicated items
      bool isVisible = columnInfo.IsVisible;
      // we enable kpidName, if it was disabled by some incorrect code
      if (columnInfo.PropID == kpidName)
        isVisible = true;
      item.IsVisible = isVisible;
      item.Width = columnInfo.Width;
      if (isVisible)
        item.Order = (int)(order++);
      continue;
    }
  }

  for (i = 0; i < _columns.Size(); i++)
  {
    CPropColumn &item = _columns[i];
    if (item.IsVisible && item.Order < 0)
      item.Order = (int)(order++);
  }

  for (i = 0; i < _columns.Size(); i++)
  {
    CPropColumn &item = _columns[i];
    if (item.Order < 0)
      item.Order = (int)(order++);
  }

  CPropColumns newColumns;

  for (i = 0; i < _columns.Size(); i++)
  {
    const CPropColumn &prop = _columns[i];
    if (prop.IsVisible)
      newColumns.Add(prop);
  }


  /*
  _sortIndex = 0;
  if (_listViewInfo.SortIndex >= 0)
  {
    int sortIndex = _columns.FindItem_for_PropID(sortID);
    if (sortIndex >= 0)
      _sortIndex = sortIndex;
  }
  */

  if (_listViewInfo.IsLoaded)
    _sortID = _listViewInfo.SortID;
  else
  {
    _sortID = 0;
    if (IsFSFolder() || IsAltStreamsFolder() || IsArcFolder())
      _sortID = kpidName;
  }

  /* There are restrictions in ListView control:
     1) main column (kpidName) must have (LV_COLUMNW::iSubItem = 0)
        So we need special sorting for columns.
     2) when we add new column, LV_COLUMNW::iOrder cannot be larger than already inserted columns)
        So we set column order after all columns are added.
  */
  newColumns.Sort(ItemProperty_Compare_NameFirst, NULL);

  if (newColumns.IsEqualTo(_visibleColumns))
    return S_OK;

  CIntArr columns(newColumns.Size());
  for (i = 0; i < newColumns.Size(); i++)
    columns[i] = -1;

  bool orderError = false;

  for (i = 0; i < newColumns.Size(); i++)
  {
    const CPropColumn &prop = newColumns[i];
    if (prop.Order < (int)newColumns.Size() && columns[prop.Order] == -1)
      columns[prop.Order] = (int)i;
    else
      orderError = true;
  }

  for (;;)
  {
    const unsigned numColumns = _visibleColumns.Size();
    if (numColumns == 0)
      break;
    DeleteColumn(numColumns - 1);
  }

  for (i = 0; i < newColumns.Size(); i++)
    AddColumn(newColumns[i]);

  // columns[0], columns[1], .... should be displayed from left to right:
  if (!orderError)
    _listView.SetColumnOrderArray(_visibleColumns.Size(), columns);

  _needSaveInfo = true;

  return S_OK;
}


void CPanel::DeleteColumn(unsigned index)
{
  _visibleColumns.Delete(index);
  _listView.DeleteColumn(index);
}

void CPanel::AddColumn(const CPropColumn &prop)
{
  const unsigned index = _visibleColumns.Size();

  LV_COLUMNW column;
  column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM | LVCF_ORDER;
  column.cx = (int)prop.Width;
  column.fmt = GetColumnAlign(prop.ID, prop.Type);
  column.iOrder = (int)index; // must be <= _listView.ItemCount
  column.iSubItem = (int)index; // must be <= _listView.ItemCount
  column.pszText = const_cast<wchar_t *>((const wchar_t *)prop.Name);

  _visibleColumns.Add(prop);
  _listView.InsertColumn(index, &column);
}


HRESULT CPanel::RefreshListCtrl()
{
  CSelectedState state;
  return RefreshListCtrl(state);
}

int CALLBACK CompareItems(LPARAM lParam1, LPARAM lParam2, LPARAM lpData);


void CPanel::GetSelectedNames(UStringVector &selectedNames)
{
  CRecordVector<UInt32> indices;
  Get_ItemIndices_Selected(indices);
  selectedNames.ClearAndReserve(indices.Size());
  FOR_VECTOR (i, indices)
    selectedNames.AddInReserved(GetItemRelPath(indices[i]));

  /*
  for (int i = 0; i < _listView.GetItemCount(); i++)
  {
    const int kSize = 1024;
    WCHAR name[kSize + 1];
    LVITEMW item;
    item.iItem = i;
    item.pszText = name;
    item.cchTextMax = kSize;
    item.iSubItem = 0;
    item.mask = LVIF_TEXT | LVIF_PARAM;
    if (!_listView.GetItem(&item))
      continue;
    const unsigned realIndex = GetRealIndex(item);
    if (realIndex == kParentIndex)
      continue;
    if (_selectedStatusVector[realIndex])
      selectedNames.Add(item.pszText);
  }
  */
  selectedNames.Sort();
}

void CPanel::SaveSelectedState(CSelectedState &s)
{
  s.FocusedName_Defined = false;
  s.FocusedName.Empty();
  s.SelectFocused = true; // false;
  s.SelectedNames.Clear();
  s.FocusedItem = _listView.GetFocusedItem();
  {
    if (s.FocusedItem >= 0)
    {
      const unsigned realIndex = GetRealItemIndex(s.FocusedItem);
      if (realIndex != kParentIndex)
      {
        s.FocusedName = GetItemRelPath(realIndex);
        s.FocusedName_Defined = true;

        s.SelectFocused = _listView.IsItemSelected(s.FocusedItem);

        /*
        const int kSize = 1024;
        WCHAR name[kSize + 1];
        LVITEMW item;
        item.iItem = focusedItem;
        item.pszText = name;
        item.cchTextMax = kSize;
        item.iSubItem = 0;
        item.mask = LVIF_TEXT;
        if (_listView.GetItem(&item))
        focusedName = item.pszText;
        */
      }
    }
  }
  GetSelectedNames(s.SelectedNames);
}

/*
HRESULT CPanel::RefreshListCtrl(const CSelectedState &s)
{
  bool selectFocused = s.SelectFocused;
  if (_mySelectMode)
    selectFocused = true;
  return RefreshListCtrl2(
      s.FocusedItem >= 0, // allowEmptyFocusedName
      s.FocusedName, s.FocusedItem, selectFocused, s.SelectedNames);
}
*/

HRESULT CPanel::RefreshListCtrl_SaveFocused(bool onTimer)
{
  CSelectedState state;
  SaveSelectedState(state);
  state.CalledFromTimer = onTimer;
  return RefreshListCtrl(state);
}

void CPanel::SetFocusedSelectedItem(int index, bool select)
{
  UINT state = LVIS_FOCUSED;
  if (select)
    state |= LVIS_SELECTED;
  _listView.SetItemState(index, state, state);
  if (!_mySelectMode && select)
  {
    const unsigned realIndex = GetRealItemIndex(index);
    if (realIndex != kParentIndex)
      _selectedStatusVector[realIndex] = true;
  }
}

// #define PRINT_STAT

#ifdef PRINT_STAT
  void Print_OnNotify(const char *name);
#else
  #define Print_OnNotify(x)
#endif



/*

extern UInt32 g_NumGroups;
extern DWORD g_start_tick;
extern DWORD g_prev_tick;
extern DWORD g_Num_SetItemText;
extern UInt32 g_NumMessages;
*/

HRESULT CPanel::RefreshListCtrl(const CSelectedState &state)
{
  m_DropHighlighted_SelectionIndex = -1;
  m_DropHighlighted_SubFolderName.Empty();

  if (!_folder)
    return S_OK;

  /*
  g_start_tick = GetTickCount();
  g_Num_SetItemText = 0;
  g_NumMessages = 0;
  */

  _dontShowMode = false;
  if (!state.CalledFromTimer)
    LoadFullPathAndShow();
  // OutputDebugStringA("=======\n");
  // OutputDebugStringA("s1 \n");
  CDisableTimerProcessing timerProcessing(*this);
  CDisableNotify disableNotify(*this);

  int focusedPos = state.FocusedItem;
  if (focusedPos < 0)
    focusedPos = 0;

  _listView.SetRedraw(false);
  // m_RedrawEnabled = false;

  LVITEMW item;
  ZeroMemory(&item, sizeof(item));

  // DWORD tickCount0 = GetTickCount();

  // _enableItemChangeNotify = false;
  DeleteListItems();
  _enableItemChangeNotify = true;

  int listViewItemCount = 0;

  _selectedStatusVector.Clear();
  // _realIndices.Clear();
  _startGroupSelect = 0;

  _selectionIsDefined = false;

  // m_Files.Clear();

  /*
  if (!_folder)
  {
    // throw 1;
    SetToRootFolder();
  }
  */

  // **************** NanaZip Modification Start ****************
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
  _headerToolBar.EnableButton(kParentFolderID, !IsRootFolder());
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
  _addressBarControl.IsUpButtonEnabled(!IsRootFolder());
  // **************** NanaZip Modification End ****************

  {
    CMyComPtr<IFolderSetFlatMode> folderSetFlatMode;
    _folder.QueryInterface(IID_IFolderSetFlatMode, &folderSetFlatMode);
    if (folderSetFlatMode)
      folderSetFlatMode->SetFlatMode(BoolToInt(_flatMode));
  }

  /*
  {
    CMyComPtr<IFolderSetShowNtfsStreamsMode> setShow;
    _folder.QueryInterface(IID_IFolderSetShowNtfsStreamsMode, &setShow);
    if (setShow)
      setShow->SetShowNtfsStreamsMode(BoolToInt(_showNtfsStrems_Mode));
  }
  */

  _isDirVector.Clear();
  // DWORD tickCount1 = GetTickCount();
  IFolderFolder *folder = _folder;
  RINOK(_folder->LoadItems())
  // DWORD tickCount2 = GetTickCount();
  // OutputDebugString(TEXT("Start Dir\n"));
  RINOK(InitColumns())

  UInt32 numItems;
  _folder->GetNumberOfItems(&numItems);
  {
    NCOM::CPropVariant prop;
    _isDirVector.ClearAndSetSize(numItems);
    bool *vec = _isDirVector.NonConstData();
    HRESULT hres = S_OK;
    unsigned i;
    for (i = 0; i < numItems; i++)
    {
      hres = folder->GetProperty(i, kpidIsDir, &prop);
      if (hres != S_OK)
        break;
      bool v = false;
      if (prop.vt == VT_BOOL)
        v = VARIANT_BOOLToBool(prop.boolVal);
      else if (prop.vt != VT_EMPTY)
        break;
      vec[i] = v;
    }
    if (i != numItems)
    {
      _isDirVector.Clear();
      if (hres == S_OK)
        hres = E_FAIL;
    }
    RINOK(hres)
  }

  const bool showDots = _showDots && !IsRootFolder();

  _listView.SetItemCount(numItems + (showDots ? 1 : 0));

  _selectedStatusVector.ClearAndReserve(numItems);
  int cursorIndex = -1;

  CMyComPtr<IFolderGetSystemIconIndex> folderGetSystemIconIndex;
#if 1 // 0 : for debug local icons loading
  if (!Is_Slow_Icon_Folder() || _showRealFileIcons)
    _folder.QueryInterface(IID_IFolderGetSystemIconIndex, &folderGetSystemIconIndex);
#endif

  const bool isFSDrivesFolder = IsFSDrivesFolder();
  const bool isArcFolder = IsArcFolder();

  if (!IsFSFolder())
  {
    CMyComPtr<IGetFolderArcProps> getFolderArcProps;
    _folder.QueryInterface(IID_IGetFolderArcProps, &getFolderArcProps);
    _thereAreDeletedItems = false;
    if (getFolderArcProps)
    {
      CMyComPtr<IFolderArcProps> arcProps;
      getFolderArcProps->GetFolderArcProps(&arcProps);
      if (arcProps)
      {
        UInt32 numLevels;
        if (arcProps->GetArcNumLevels(&numLevels) != S_OK)
          numLevels = 0;
        NCOM::CPropVariant prop;
        if (arcProps->GetArcProp(numLevels - 1, kpidIsDeleted, &prop) == S_OK)
          if (prop.vt == VT_BOOL && VARIANT_BOOLToBool(prop.boolVal))
            _thereAreDeletedItems = true;
      }
    }
  }

  _thereAre_ListView_Items = true;

  // OutputDebugStringA("\n\n");

  Print_OnNotify("===== Before Load");

  // #define USE_EMBED_ITEM

  if (showDots)
  {
    const UString itemName ("..");
    item.iItem = listViewItemCount;
    if (itemName == state.FocusedName)
      cursorIndex = listViewItemCount;
    item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
    int subItem = 0;
    item.iSubItem = subItem++;
    item.lParam = (LPARAM)(int)kParentIndex;
    #ifdef USE_EMBED_ITEM
    item.pszText = const_cast<wchar_t *>((const wchar_t *)itemName);
    #else
    item.pszText = LPSTR_TEXTCALLBACKW;
    #endif
    // const UInt32 attrib = FILE_ATTRIBUTE_DIRECTORY;
    item.iImage = g_Ext_to_Icon_Map.GetIconIndex_DIR();
        // g_Ext_to_Icon_Map.GetIconIndex(attrib, itemName);
    if (item.iImage < 0)
        item.iImage = 0;
    if (_listView.InsertItem(&item) == -1)
      return E_FAIL;
    listViewItemCount++;
  }

  // OutputDebugStringA("S1\n");

  UString correctedName, itemName, relPath, focusedNamePure, selectedName;
  bool needCheckSlash = false;
  if (state.FocusedName_Defined)
  {
    focusedNamePure = state.FocusedName;
    const int pos = focusedNamePure.ReverseFind_PathSepar();
    if (pos >= 0)
    {
      focusedNamePure.DeleteFrontal((unsigned)pos + 1);
      needCheckSlash = true;
    }
  }
  if (state.SelectedNames.Size() == 1)
  {
    selectedName = state.SelectedNames[0];
    const int pos = selectedName.ReverseFind_PathSepar();
    if (pos >= 0)
    {
      selectedName.DeleteFrontal((unsigned)pos + 1);
      needCheckSlash = true;
    }
  }

  for (UInt32 i = 0; i < numItems; i++)
  {
    const wchar_t *name = NULL;
    unsigned nameLen = 0;

    if (_folderGetItemName)
      _folderGetItemName->GetItemName(i, &name, &nameLen);
    if (!name)
    {
      GetItemName(i, itemName);
      name = itemName;
      nameLen = itemName.Len();
    }

    bool selected = false;

    if (state.SelectedNames.Size() > 1
        || (state.SelectedNames.Size() == 1 && selectedName == name)
        || (state.FocusedName_Defined && focusedNamePure == name)
        || (needCheckSlash && NWindows::NFile::NName::FindSepar(name) >= 0))
    {
      relPath.Empty();
      // relPath += GetItemPrefix(i);
      if (_flatMode)
      {
        const wchar_t *prefix = NULL;
        if (_folderGetItemName)
        {
          unsigned prefixLen = 0;
          _folderGetItemName->GetItemPrefix(i, &prefix, &prefixLen);
          if (prefix)
            relPath = prefix;
        }
        if (!prefix)
        {
          NCOM::CPropVariant prop;
          if (_folder->GetProperty(i, kpidPrefix, &prop) != S_OK)
            throw 2723400;
          if (prop.vt == VT_BSTR)
            relPath.SetFromBstr(prop.bstrVal);
        }
      }
      relPath += name;
      if (relPath == state.FocusedName)
        cursorIndex = listViewItemCount;
      if (state.SelectedNames.FindInSorted(relPath) != -1)
        selected = true;
    }

    _selectedStatusVector.AddInReserved(selected);

    item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;

    if (!_mySelectMode)
      if (selected)
      {
        item.mask |= LVIF_STATE;
        item.state = LVIS_SELECTED;
      }

    int subItem = 0;
    item.iItem = listViewItemCount;

    item.iSubItem = subItem++;
    item.lParam = (LPARAM)i;

    /*
    int finish = nameLen - 4;
    int j;
    for (j = 0; j < finish; j++)
    {
      if (name[j    ] == ' ' &&
          name[j + 1] == ' ' &&
          name[j + 2] == ' ' &&
          name[j + 3] == ' ' &&
          name[j + 4] == ' ')
        break;
    }
    if (j < finish)
    {
      correctedName.Empty();
      correctedName = "virus";
      int pos = 0;
      for (;;)
      {
        int posNew = itemName.Find(L"     ", pos);
        if (posNew < 0)
        {
          correctedName += itemName.Ptr(pos);
          break;
        }
        correctedName += itemName.Mid(pos, posNew - pos);
        correctedName += " ... ";
        pos = posNew;
        while (itemName[++pos] == ' ');
      }
      item.pszText = const_cast<wchar_t *>((const wchar_t *)correctedName);
    }
    else
    */
    {
      #ifdef USE_EMBED_ITEM
      item.pszText = const_cast<wchar_t *>((const wchar_t *)name);
      #else
      item.pszText = LPSTR_TEXTCALLBACKW;
      #endif
      /* LPSTR_TEXTCALLBACKW works, but in some cases there are problems,
      since we block notify handler.
      LPSTR_TEXTCALLBACKW can be 2-3 times faster for loading in this loop. */
    }

    bool defined = false;
    item.iImage = -1;

    if (folderGetSystemIconIndex)
    {
      const HRESULT res = folderGetSystemIconIndex->GetSystemIconIndex(i, &item.iImage);
      if (res == S_OK)
      {
        // item.iImage = -1; // for debug
        defined = (item.iImage > 0);
#if 0 // 0: can be slower: 2 attempts for some paths.
      // 1: faster, but we can get default icon for some cases (where non default icon is possible)

        if (item.iImage == 0)
        {
          // (item.iImage == 0) means default icon.
          // But (item.iImage == 0) also can be returned for exe/ico files,
          // if filePath is LONG PATH (path_len() >= MAX_PATH).
          // Also we want to show split icon (.001) for any split extension: 001 002 003.
          // Are there another cases for (item.iImage == 0) for files with known extensions?
          // We don't want to do second attempt to request icon,
          // if it also will return (item.iImage == 0).

          int dotPos = -1;
          for (unsigned k = 0;; k++)
          {
            const wchar_t c = name[k];
            if (c == 0)
              break;
            if (c == '.')
              dotPos = (int)i;
            // we don't need IS_PATH_SEPAR check, because we have only (fileName) doesn't include path prefix.
            // if (IS_PATH_SEPAR(c) || c == ':') dotPos = -1;
          }
          defined = true;
          if (dotPos >= 0)
          {
#if 0
            const wchar_t *ext = name + dotPos;
            if (StringsAreEqualNoCase_Ascii(ext, ".exe") ||
                StringsAreEqualNoCase_Ascii(ext, ".ico"))
#endif
              defined = false;
          }
        }
#endif
      }
    }

    if (!defined)
    {
      UInt32 attrib = 0;
      {
        NCOM::CPropVariant prop;
        RINOK(_folder->GetProperty(i, kpidAttrib, &prop))
        if (prop.vt == VT_UI4)
        {
          attrib = prop.ulVal;
          if (isArcFolder)
          {
            // if attrib (high 16-bits) is supposed from posix,
            // we keep only low bits (basic Windows attrib flags):
            if (attrib & 0xF0000000)
              attrib &= 0x3FFF;
          }
        }
      }
      if (IsItem_Folder(i))
        attrib |= FILE_ATTRIBUTE_DIRECTORY;
      else
        attrib &= ~(UInt32)FILE_ATTRIBUTE_DIRECTORY;

      item.iImage = -1;
      if (isFSDrivesFolder)
      {
        FString fs (us2fs((UString)name));
        fs.Add_PathSepar();
        item.iImage = Shell_GetFileInfo_SysIconIndex_for_Path(fs, attrib);
        // item.iImage = 0; // for debug
      }
      if (item.iImage < 0) // <= 0 check?
        item.iImage = g_Ext_to_Icon_Map.GetIconIndex(attrib, name);
    }

    // item.iImage = -1; // for debug
    if (item.iImage < 0)
        item.iImage = 0; // default image
    if (_listView.InsertItem(&item) == -1)
      return E_FAIL;
    listViewItemCount++;
  }

  /*
    xp-64: there is different order when Windows calls CPanel::OnNotify for _listView modes:
    Details      : after whole code
    List         : 2 times:
                        1) - ListView.SotRedraw()
                        2) - after whole code
    Small Icons  :
    Large icons  : 2 times:
                        1) - ListView.Sort()
                        2) - after whole code (calls with reverse order of items)

    So we need to allow Notify(), when windows requests names during the following code.
  */

  Print_OnNotify("after Load");

  disableNotify.SetMemMode_Enable();
  disableNotify.Restore();

  if (_listView.GetItemCount() > 0 && cursorIndex >= 0)
    SetFocusedSelectedItem(cursorIndex, state.SelectFocused);

  Print_OnNotify("after SetFocusedSelectedItem");
  // OutputDebugStringA("Sort before");
  SetSortRawStatus();
  _listView.SortItems(CompareItems, (LPARAM)this);
  // OutputDebugStringA("=== Sort after");
  Print_OnNotify("after  Sort");

  if (cursorIndex < 0 && _listView.GetItemCount() > 0)
  {
    if (focusedPos >= _listView.GetItemCount())
      focusedPos = _listView.GetItemCount() - 1;
    // we select item only in showDots mode.
    SetFocusedSelectedItem(focusedPos, showDots && (focusedPos == 0));
  }

  // m_RedrawEnabled = true;

  Print_OnNotify("after  SetFocusedSelectedItem2");

  _listView.EnsureVisible(_listView.GetFocusedItem(), false);

  // disableNotify.SetMemMode_Enable();
  // disableNotify.Restore();

  Print_OnNotify("after  EnsureVisible");

  _listView.SetRedraw(true);

  Print_OnNotify("after  SetRedraw");

  _listView.InvalidateRect(NULL, true);

  Print_OnNotify("after InvalidateRect");
  /*
  _listView.UpdateWindow();
  */
  Refresh_StatusBar();
  /*
  char s[256];
  sprintf(s,
      // "attribMap = %5d, extMap = %5d, "
      "delete = %5d, load = %5d, list = %5d, sort = %5d, end = %5d",
      // g_Ext_to_Icon_Map._attribMap.Size(),
      // g_Ext_to_Icon_Map._extMap.Size(),
      tickCount1 - tickCount0,
      tickCount2 - tickCount1,
      tickCount3 - tickCount2,
      tickCount4 - tickCount3,
      tickCount5 - tickCount4
      );
  sprintf(s,
      "5 = %5d, 6 = %5d, 7 = %5d, 8 = %5d, 9 = %5d",
      tickCount5 - tickCount4,
      tickCount6 - tickCount5,
      tickCount7 - tickCount6,
      tickCount8 - tickCount7,
      tickCount9 - tickCount8
      );
  OutputDebugStringA(s);
  */
  return S_OK;
}


void CPanel::Get_ItemIndices_Selected(CRecordVector<UInt32> &indices) const
{
  indices.Clear();
  /*
  int itemIndex = -1;
  while ((itemIndex = _listView.GetNextSelectedItem(itemIndex)) != -1)
  {
    LPARAM param;
    if (_listView.GetItemParam(itemIndex, param))
      indices.Add(param);
  }
  HeapSort(&indices.Front(), indices.Size());
  */
  const bool *v = _selectedStatusVector.ConstData();
  const unsigned size = _selectedStatusVector.Size();
  for (unsigned i = 0; i < size; i++)
    if (v[i])
      indices.Add(i);
}


void CPanel::Get_ItemIndices_Operated(CRecordVector<UInt32> &indices) const
{
  Get_ItemIndices_Selected(indices);
  if (!indices.IsEmpty())
    return;
  if (_listView.GetSelectedCount() == 0)
    return;
  const int focusedItem = _listView.GetFocusedItem();
  if (focusedItem >= 0)
  {
    if (_listView.IsItemSelected(focusedItem))
    {
      const unsigned realIndex = GetRealItemIndex(focusedItem);
      if (realIndex != kParentIndex)
        indices.Add(realIndex);
    }
  }
}

void CPanel::Get_ItemIndices_All(CRecordVector<UInt32> &indices) const
{
  indices.Clear();
  UInt32 numItems;
  if (_folder->GetNumberOfItems(&numItems) != S_OK)
    return;
  indices.ClearAndSetSize(numItems);
  UInt32 *vec = indices.NonConstData();
  for (UInt32 i = 0; i < numItems; i++)
    vec[i] = i;
}

void CPanel::Get_ItemIndices_OperSmart(CRecordVector<UInt32> &indices) const
{
  Get_ItemIndices_Operated(indices);
  if (indices.IsEmpty() || (indices.Size() == 1 && indices[0] == (UInt32)(Int32)-1))
    Get_ItemIndices_All(indices);
}

/*
void CPanel::GetOperatedListViewIndices(CRecordVector<UInt32> &indices) const
{
  indices.Clear();
  int numItems = _listView.GetItemCount();
  for (int i = 0; i < numItems; i++)
  {
    const unsigned realIndex = GetRealItemIndex(i);
    if (realIndex >= 0)
      if (_selectedStatusVector[realIndex])
        indices.Add(i);
  }
  if (indices.IsEmpty())
  {
    const int focusedItem = _listView.GetFocusedItem();
      if (focusedItem >= 0)
        indices.Add(focusedItem);
  }
}
*/

void CPanel::EditItem(bool useEditor)
{
  if (!useEditor)
  {
    CMyComPtr<IFolderCalcItemFullSize> calcItemFullSize;
    _folder.QueryInterface(IID_IFolderCalcItemFullSize, &calcItemFullSize);
    if (calcItemFullSize)
    {
      bool needRefresh = false;
      CRecordVector<UInt32> indices;
      Get_ItemIndices_Operated(indices);
      FOR_VECTOR (i, indices)
      {
        UInt32 index = indices[i];
        if (IsItem_Folder(index))
        {
          calcItemFullSize->CalcItemFullSize(index, NULL);
          needRefresh = true;
        }
      }
      if (needRefresh)
      {
        // _listView.RedrawItem(0);
        // _listView.RedrawAllItems();
        InvalidateList();
        return;
      }
    }
  }


  const int focusedItem = _listView.GetFocusedItem();
  if (focusedItem < 0)
    return;
  const unsigned realIndex = GetRealItemIndex(focusedItem);
  if (realIndex == kParentIndex)
    return;
  if (!IsItem_Folder(realIndex))
    EditItem(realIndex, useEditor);
}

void CPanel::OpenFocusedItemAsInternal(const wchar_t *type)
{
  const int focusedItem = _listView.GetFocusedItem();
  if (focusedItem < 0)
    return;
  const unsigned realIndex = GetRealItemIndex(focusedItem);
  if (IsItem_Folder(realIndex))
    OpenFolder(realIndex);
  else
    OpenItem(realIndex, true, false, type);
}

void CPanel::OpenSelectedItems(bool tryInternal)
{
  CRecordVector<UInt32> indices;
  Get_ItemIndices_Operated(indices);
  if (indices.Size() > 20)
  {
    MessageBox_Error_LangID(IDS_TOO_MANY_ITEMS);
    return;
  }

  const int focusedItem = _listView.GetFocusedItem();
  if (focusedItem >= 0)
  {
    const unsigned realIndex = GetRealItemIndex(focusedItem);
    if (realIndex == kParentIndex && (tryInternal || indices.Size() == 0) && _listView.IsItemSelected(focusedItem))
      indices.Insert(0, realIndex);
  }

  // bool dirIsStarted = false;
  FOR_VECTOR (i, indices)
  {
    UInt32 index = indices[i];
    // CFileInfo &aFile = m_Files[index];
    if (IsItem_Folder(index))
    {
      // if (!dirIsStarted)
      {
        if (tryInternal)
        {
          OpenFolder(index);
          // dirIsStarted = true;
          break;
        }
        else
          OpenFolderExternal(index);
      }
    }
    else
      OpenItem(index, (tryInternal && indices.Size() == 1), true);
  }
}

UString CPanel::GetItemName(unsigned itemIndex) const
{
  if (itemIndex == kParentIndex)
    return L"..";
  NCOM::CPropVariant prop;
  if (_folder->GetProperty(itemIndex, kpidName, &prop) != S_OK)
    throw 2723400;
  if (prop.vt != VT_BSTR)
    throw 2723401;
  return prop.bstrVal;
}

UString CPanel::GetItemName_for_Copy(unsigned itemIndex) const
{
  if (itemIndex == kParentIndex)
    return L"..";
  UString s;
  {
    NCOM::CPropVariant prop;
    if (_folder->GetProperty(itemIndex, kpidOutName, &prop) == S_OK)
    {
      if (prop.vt == VT_BSTR)
        s = prop.bstrVal;
      else if (prop.vt != VT_EMPTY)
        throw 2723401;
    }
    if (s.IsEmpty())
      s = GetItemName(itemIndex);
  }
  return Get_Correct_FsFile_Name(s);
}

void CPanel::GetItemName(unsigned itemIndex, UString &s) const
{
  if (itemIndex == kParentIndex)
  {
    s = "..";
    return;
  }
  NCOM::CPropVariant prop;
  if (_folder->GetProperty(itemIndex, kpidName, &prop) != S_OK)
    throw 2723400;
  if (prop.vt != VT_BSTR)
    throw 2723401;
  s.SetFromBstr(prop.bstrVal);
}

UString CPanel::GetItemPrefix(unsigned itemIndex) const
{
  UString prefix;
  if (itemIndex != kParentIndex)
  {
    NCOM::CPropVariant prop;
    if (_folder->GetProperty(itemIndex, kpidPrefix, &prop) != S_OK)
      throw 2723400;
    if (prop.vt == VT_BSTR)
      prefix.SetFromBstr(prop.bstrVal);
  }
  return prefix;
}

UString CPanel::GetItemRelPath(unsigned itemIndex) const
{
  return GetItemPrefix(itemIndex) + GetItemName(itemIndex);
}

UString CPanel::GetItemRelPath2(unsigned itemIndex) const
{
  UString s = GetItemRelPath(itemIndex);
  #if defined(_WIN32) && !defined(UNDER_CE)
  if (s.Len() == 2 && NFile::NName::IsDrivePath2(s))
  {
    if (IsFSDrivesFolder() && !IsDeviceDrivesPrefix())
      s.Add_PathSepar();
  }
  #endif
  return s;
}


void CPanel::Add_ItemRelPath2_To_String(unsigned itemIndex, UString &s) const
{
  if (itemIndex == kParentIndex)
  {
    s += "..";
    return;
  }

  const unsigned start = s.Len();
  NCOM::CPropVariant prop;
  if (_folder->GetProperty(itemIndex, kpidPrefix, &prop) != S_OK)
    throw 2723400;
  if (prop.vt == VT_BSTR)
    s += prop.bstrVal;

  const wchar_t *name = NULL;
  unsigned nameLen = 0;

  if (_folderGetItemName)
    _folderGetItemName->GetItemName(itemIndex, &name, &nameLen);
  if (name)
    s += name;
  else
  {
    prop.Clear();
    if (_folder->GetProperty(itemIndex, kpidName, &prop) != S_OK)
      throw 2723400;
    if (prop.vt != VT_BSTR)
      throw 2723401;
    s += prop.bstrVal;
  }

  #if defined(_WIN32) && !defined(UNDER_CE)
    if (s.Len() - start == 2 && NFile::NName::IsDrivePath2(s.Ptr(start)))
    {
      if (IsFSDrivesFolder() && !IsDeviceDrivesPrefix())
        s.Add_PathSepar();
    }
  #endif
}


UString CPanel::GetItemFullPath(unsigned itemIndex) const
{
  return GetFsPath() + GetItemRelPath2(itemIndex);
}

bool CPanel::GetItem_BoolProp(UInt32 itemIndex, PROPID propID) const
{
  NCOM::CPropVariant prop;
  if (_folder->GetProperty(itemIndex, propID, &prop) != S_OK)
    throw 2723400;
  if (prop.vt == VT_BOOL)
    return VARIANT_BOOLToBool(prop.boolVal);
  if (prop.vt == VT_EMPTY)
    return false;
  throw 2723401;
}

bool CPanel::IsItem_Deleted(unsigned itemIndex) const
{
  if (itemIndex == kParentIndex)
    return false;
  return GetItem_BoolProp(itemIndex, kpidIsDeleted);
}

bool CPanel::IsItem_Folder(unsigned itemIndex) const
{
  if (itemIndex == kParentIndex)
    return true;
  if (itemIndex < _isDirVector.Size())
    return _isDirVector[itemIndex];
  return GetItem_BoolProp(itemIndex, kpidIsDir);
}

bool CPanel::IsItem_AltStream(unsigned itemIndex) const
{
  if (itemIndex == kParentIndex)
    return false;
  return GetItem_BoolProp(itemIndex, kpidIsAltStream);
}

UInt64 CPanel::GetItem_UInt64Prop(unsigned itemIndex, PROPID propID) const
{
  if (itemIndex == kParentIndex)
    return 0;
  NCOM::CPropVariant prop;
  if (_folder->GetProperty(itemIndex, propID, &prop) != S_OK)
    throw 2723400;
  UInt64 val = 0;
  if (ConvertPropVariantToUInt64(prop, val))
    return val;
  return 0;
}

UInt64 CPanel::GetItemSize(unsigned itemIndex) const
{
  if (itemIndex == kParentIndex)
    return 0;
  if (_folderGetItemName)
    return _folderGetItemName->GetItemSize(itemIndex);
  return GetItem_UInt64Prop(itemIndex, kpidSize);
}

void CPanel::SaveListViewInfo()
{
  if (!_needSaveInfo)
    return;

  unsigned i;

  for (i = 0; i < _visibleColumns.Size(); i++)
  {
    CPropColumn &prop = _visibleColumns[i];
    LVCOLUMN winColumnInfo;
    winColumnInfo.mask = LVCF_ORDER | LVCF_WIDTH;
    if (!_listView.GetColumn(i, &winColumnInfo))
      throw 1;
    prop.Order = winColumnInfo.iOrder;
    prop.Width = (UInt32)(Int32)winColumnInfo.cx;
  }

  CListViewInfo viewInfo;

  // PROPID sortPropID = _columns[_sortIndex].ID;
  const PROPID sortPropID = _sortID;

  // we save columns as "sorted by order" to registry

  CPropColumns sortedProperties = _visibleColumns;

  sortedProperties.Sort();

  for (i = 0; i < sortedProperties.Size(); i++)
  {
    const CPropColumn &prop = sortedProperties[i];
    CColumnInfo columnInfo;
    columnInfo.IsVisible = prop.IsVisible;
    columnInfo.PropID = prop.ID;
    columnInfo.Width = prop.Width;
    viewInfo.Columns.Add(columnInfo);
  }

  for (i = 0; i < _columns.Size(); i++)
  {
    const CPropColumn &prop = _columns[i];
    if (sortedProperties.FindItem_for_PropID(prop.ID) < 0)
    {
      CColumnInfo columnInfo;
      columnInfo.IsVisible = false;
      columnInfo.PropID = prop.ID;
      columnInfo.Width = prop.Width;
      viewInfo.Columns.Add(columnInfo);
    }
  }

  viewInfo.SortID = sortPropID;
  viewInfo.Ascending = _ascending;
  viewInfo.IsLoaded = true;
  if (!_listViewInfo.IsEqual(viewInfo))
  {
    viewInfo.Save(_typeIDString);
    _listViewInfo = viewInfo;
  }
}


bool CPanel::OnRightClick(MY_NMLISTVIEW_NMITEMACTIVATE *itemActivate, LRESULT &result)
{
  if (itemActivate->hdr.hwndFrom == HWND(_listView))
    return false;
  POINT point;
  ::GetCursorPos(&point);
  ShowColumnsContextMenu(point.x, point.y);
  result = TRUE;
  return true;
}

void CPanel::ShowColumnsContextMenu(int x, int y)
{
  CMenu menu;
  CMenuDestroyer menuDestroyer(menu);

  menu.CreatePopup();

  const int kCommandStart = 100;
  FOR_VECTOR (i, _columns)
  {
    const CPropColumn &prop = _columns[i];
    UINT flags =  MF_STRING;
    if (prop.IsVisible)
      flags |= MF_CHECKED;
    if (i == 0)
      flags |= MF_GRAYED;
    menu.AppendItem(flags, kCommandStart + i, prop.Name);
  }

  const int menuResult = menu.Track(TPM_LEFTALIGN | TPM_RETURNCMD | TPM_NONOTIFY, x, y, _listView);

  if (menuResult >= kCommandStart && menuResult <= kCommandStart + (int)_columns.Size())
  {
    const unsigned index = (unsigned)(menuResult - kCommandStart);
    CPropColumn &prop = _columns[index];
    prop.IsVisible = !prop.IsVisible;

    if (prop.IsVisible)
    {
      prop.Order = (int)_visibleColumns.Size();
      AddColumn(prop);
    }
    else
    {
      const int visibleIndex = _visibleColumns.FindItem_for_PropID(prop.ID);
      if (visibleIndex >= 0)
      {
        /*
        if (_sortIndex == index)
        {
        _sortIndex = 0;
        _ascending = true;
        }
        */
        if (_sortID == prop.ID)
        {
          _sortID = kpidName;
          _ascending = true;
        }
        DeleteColumn((unsigned)visibleIndex);
      }
    }
  }
}

void CPanel::OnReload(bool onTimer)
{
  const HRESULT res = RefreshListCtrl_SaveFocused(onTimer);
  if (res != S_OK)
    MessageBox_Error_HRESULT(res);
}

void CPanel::OnTimer()
{
  if (!_processTimer)
    return;
  if (!AutoRefresh_Mode)
    return;
  if (!_folder) // it's unexpected case, but we use it as additional protection.
    return;
  {
    CMyComPtr<IFolderWasChanged> folderWasChanged;
    _folder.QueryInterface(IID_IFolderWasChanged, &folderWasChanged);
    if (!folderWasChanged)
      return;
    Int32 wasChanged;
    if (folderWasChanged->WasChanged(&wasChanged) != S_OK || wasChanged == 0)
      return;
  }
  OnReload(true); // onTimer
}

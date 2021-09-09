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

static int GetColumnWidth(PROPID propID, VARTYPE /* varType */)
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
  return (*(*((const CPropColumn *const *)a1))).Compare_NameFirst(*(*((const CPropColumn *const *)a2)));
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

  bool isFsFolder = IsFSFolder() || IsAltStreamsFolder();

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
      HRESULT res = _folderRawProps->GetRawPropInfo(i, &name, &propID);
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
    int index = _columns.FindItem_for_PropID(columnInfo.PropID);
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
        item.Order = order++;
      continue;
    }
  }

  for (i = 0; i < _columns.Size(); i++)
  {
    CPropColumn &item = _columns[i];
    if (item.IsVisible && item.Order < 0)
      item.Order = order++;
  }
  
  for (i = 0; i < _columns.Size(); i++)
  {
    CPropColumn &item = _columns[i];
    if (item.Order < 0)
      item.Order = order++;
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
      columns[prop.Order] = i;
    else
      orderError = true;
  }

  for (;;)
  {
    unsigned numColumns = _visibleColumns.Size();
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
  const int index = _visibleColumns.Size();
  
  LV_COLUMNW column;
  column.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM | LVCF_ORDER;
  column.cx = prop.Width;
  column.fmt = GetColumnAlign(prop.ID, prop.Type);
  column.iOrder = index; // must be <= _listView.ItemCount
  column.iSubItem = index; // must be <= _listView.ItemCount
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
  GetSelectedItemsIndices(indices);
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
    int realIndex = GetRealIndex(item);
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
      int realIndex = GetRealItemIndex(s.FocusedItem);
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

HRESULT CPanel::RefreshListCtrl_SaveFocused()
{
  CSelectedState state;
  SaveSelectedState(state);
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
    int realIndex = GetRealItemIndex(index);
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
  if (!_folder)
    return S_OK;

  /*
  g_start_tick = GetTickCount();
  g_Num_SetItemText = 0;
  g_NumMessages = 0;
  */

  _dontShowMode = false;
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

  if (!_folder)
  {
    // throw 1;
    SetToRootFolder();
  }
  
  _headerToolBar.EnableButton(kParentFolderID, !IsRootFolder());

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

  // DWORD tickCount1 = GetTickCount();
  RINOK(_folder->LoadItems());
  // DWORD tickCount2 = GetTickCount();
  RINOK(InitColumns());

  // OutputDebugString(TEXT("Start Dir\n"));
  UInt32 numItems;
  _folder->GetNumberOfItems(&numItems);

  bool showDots = _showDots && !IsRootFolder();

  _listView.SetItemCount(numItems + (showDots ? 1 : 0));

  _selectedStatusVector.ClearAndReserve(numItems);
  int cursorIndex = -1;

  CMyComPtr<IFolderGetSystemIconIndex> folderGetSystemIconIndex;
  if (!Is_Slow_Icon_Folder() || _showRealFileIcons)
    _folder.QueryInterface(IID_IFolderGetSystemIconIndex, &folderGetSystemIconIndex);

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
    UString itemName ("..");
    item.iItem = listViewItemCount;
    if (itemName == state.FocusedName)
      cursorIndex = listViewItemCount;
    item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
    int subItem = 0;
    item.iSubItem = subItem++;
    item.lParam = kParentIndex;
    #ifdef USE_EMBED_ITEM
    item.pszText = const_cast<wchar_t *>((const wchar_t *)itemName);
    #else
    item.pszText = LPSTR_TEXTCALLBACKW;
    #endif
    UInt32 attrib = FILE_ATTRIBUTE_DIRECTORY;
    item.iImage = _extToIconMap.GetIconIndex(attrib, itemName);
    if (item.iImage < 0)
      item.iImage = 0;
    if (_listView.InsertItem(&item) == -1)
      return E_FAIL;
    listViewItemCount++;
  }
  
  // OutputDebugStringA("S1\n");

  UString correctedName;
  UString itemName;
  UString relPath;
  
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
    
    if (state.FocusedName_Defined || !state.SelectedNames.IsEmpty())
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
      if (state.SelectedNames.FindInSorted(relPath) >= 0)
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
    item.lParam = i;
    
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
  
    if (folderGetSystemIconIndex)
    {
      folderGetSystemIconIndex->GetSystemIconIndex(i, &item.iImage);
      defined = (item.iImage > 0);
    }

    if (!defined)
    {
      UInt32 attrib = 0;
      {
        NCOM::CPropVariant prop;
        RINOK(_folder->GetProperty(i, kpidAttrib, &prop));
        if (prop.vt == VT_UI4)
          attrib = prop.ulVal;
      }
      if (IsItem_Folder(i))
        attrib |= FILE_ATTRIBUTE_DIRECTORY;

      if (_currentFolderPrefix.IsEmpty())
      {
        int iconIndexTemp;
        GetRealIconIndex(us2fs((UString)name) + FCHAR_PATH_SEPARATOR, attrib, iconIndexTemp);
        item.iImage = iconIndexTemp;
      }
      else
      {
        item.iImage = _extToIconMap.GetIconIndex(attrib, name);
      }
    }
    
    if (item.iImage < 0)
      item.iImage = 0;

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
  
  SetSortRawStatus();
  _listView.SortItems(CompareItems, (LPARAM)this);
  
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
      // _extToIconMap._attribMap.Size(),
      // _extToIconMap._extMap.Size(),
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


void CPanel::GetSelectedItemsIndices(CRecordVector<UInt32> &indices) const
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
  const bool *v = &_selectedStatusVector.Front();
  unsigned size = _selectedStatusVector.Size();
  for (unsigned i = 0; i < size; i++)
    if (v[i])
      indices.Add(i);
}


void CPanel::GetOperatedItemIndices(CRecordVector<UInt32> &indices) const
{
  GetSelectedItemsIndices(indices);
  if (!indices.IsEmpty())
    return;
  if (_listView.GetSelectedCount() == 0)
    return;
  int focusedItem = _listView.GetFocusedItem();
  if (focusedItem >= 0)
  {
    if (_listView.IsItemSelected(focusedItem))
    {
      int realIndex = GetRealItemIndex(focusedItem);
      if (realIndex != kParentIndex)
        indices.Add(realIndex);
    }
  }
}

void CPanel::GetAllItemIndices(CRecordVector<UInt32> &indices) const
{
  indices.Clear();
  UInt32 numItems;
  if (_folder->GetNumberOfItems(&numItems) == S_OK)
    for (UInt32 i = 0; i < numItems; i++)
      indices.Add(i);
}

void CPanel::GetOperatedIndicesSmart(CRecordVector<UInt32> &indices) const
{
  GetOperatedItemIndices(indices);
  if (indices.IsEmpty() || (indices.Size() == 1 && indices[0] == (UInt32)(Int32)-1))
    GetAllItemIndices(indices);
}

/*
void CPanel::GetOperatedListViewIndices(CRecordVector<UInt32> &indices) const
{
  indices.Clear();
  int numItems = _listView.GetItemCount();
  for (int i = 0; i < numItems; i++)
  {
    int realIndex = GetRealItemIndex(i);
    if (realIndex >= 0)
      if (_selectedStatusVector[realIndex])
        indices.Add(i);
  }
  if (indices.IsEmpty())
  {
    int focusedItem = _listView.GetFocusedItem();
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
      GetOperatedItemIndices(indices);
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


  int focusedItem = _listView.GetFocusedItem();
  if (focusedItem < 0)
    return;
  int realIndex = GetRealItemIndex(focusedItem);
  if (realIndex == kParentIndex)
    return;
  if (!IsItem_Folder(realIndex))
    EditItem(realIndex, useEditor);
}

void CPanel::OpenFocusedItemAsInternal(const wchar_t *type)
{
  int focusedItem = _listView.GetFocusedItem();
  if (focusedItem < 0)
    return;
  int realIndex = GetRealItemIndex(focusedItem);
  if (IsItem_Folder(realIndex))
    OpenFolder(realIndex);
  else
    OpenItem(realIndex, true, false, type);
}

void CPanel::OpenSelectedItems(bool tryInternal)
{
  CRecordVector<UInt32> indices;
  GetOperatedItemIndices(indices);
  if (indices.Size() > 20)
  {
    MessageBox_Error_LangID(IDS_TOO_MANY_ITEMS);
    return;
  }
  
  int focusedItem = _listView.GetFocusedItem();
  if (focusedItem >= 0)
  {
    int realIndex = GetRealItemIndex(focusedItem);
    if (realIndex == kParentIndex && (tryInternal || indices.Size() == 0) && _listView.IsItemSelected(focusedItem))
      indices.Insert(0, realIndex);
  }

  bool dirIsStarted = false;
  FOR_VECTOR (i, indices)
  {
    UInt32 index = indices[i];
    // CFileInfo &aFile = m_Files[index];
    if (IsItem_Folder(index))
    {
      if (!dirIsStarted)
      {
        if (tryInternal)
        {
          OpenFolder(index);
          dirIsStarted = true;
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

UString CPanel::GetItemName(int itemIndex) const
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

UString CPanel::GetItemName_for_Copy(int itemIndex) const
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

void CPanel::GetItemName(int itemIndex, UString &s) const
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

UString CPanel::GetItemPrefix(int itemIndex) const
{
  if (itemIndex == kParentIndex)
    return UString();
  NCOM::CPropVariant prop;
  if (_folder->GetProperty(itemIndex, kpidPrefix, &prop) != S_OK)
    throw 2723400;
  UString prefix;
  if (prop.vt == VT_BSTR)
    prefix.SetFromBstr(prop.bstrVal);
  return prefix;
}

UString CPanel::GetItemRelPath(int itemIndex) const
{
  return GetItemPrefix(itemIndex) + GetItemName(itemIndex);
}

UString CPanel::GetItemRelPath2(int itemIndex) const
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

UString CPanel::GetItemFullPath(int itemIndex) const
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

bool CPanel::IsItem_Deleted(int itemIndex) const
{
  if (itemIndex == kParentIndex)
    return false;
  return GetItem_BoolProp(itemIndex, kpidIsDeleted);
}

bool CPanel::IsItem_Folder(int itemIndex) const
{
  if (itemIndex == kParentIndex)
    return true;
  return GetItem_BoolProp(itemIndex, kpidIsDir);
}

bool CPanel::IsItem_AltStream(int itemIndex) const
{
  if (itemIndex == kParentIndex)
    return false;
  return GetItem_BoolProp(itemIndex, kpidIsAltStream);
}

UInt64 CPanel::GetItem_UInt64Prop(int itemIndex, PROPID propID) const
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

UInt64 CPanel::GetItemSize(int itemIndex) const
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
    prop.Width = winColumnInfo.cx;
  }

  CListViewInfo viewInfo;
  
  // PROPID sortPropID = _columns[_sortIndex].ID;
  PROPID sortPropID = _sortID;
  
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


bool CPanel::OnRightClick(MY_NMLISTVIEW_NMITEMACTIVATE *itemActiveate, LRESULT &result)
{
  if (itemActiveate->hdr.hwndFrom == HWND(_listView))
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
  
  int menuResult = menu.Track(TPM_LEFTALIGN | TPM_RETURNCMD | TPM_NONOTIFY, x, y, _listView);
  
  if (menuResult >= kCommandStart && menuResult <= kCommandStart + (int)_columns.Size())
  {
    int index = menuResult - kCommandStart;
    CPropColumn &prop = _columns[index];
    prop.IsVisible = !prop.IsVisible;

    if (prop.IsVisible)
    {
      prop.Order = _visibleColumns.Size();
      AddColumn(prop);
    }
    else
    {
      int visibleIndex = _visibleColumns.FindItem_for_PropID(prop.ID);
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
        DeleteColumn(visibleIndex);
      }
    }
  }
}

void CPanel::OnReload()
{
  HRESULT res = RefreshListCtrl_SaveFocused();
  if (res != S_OK)
    MessageBox_Error_HRESULT(res);
}

void CPanel::OnTimer()
{
  if (!_processTimer)
    return;
  if (!AutoRefresh_Mode)
    return;
  CMyComPtr<IFolderWasChanged> folderWasChanged;
  if (_folder.QueryInterface(IID_IFolderWasChanged, &folderWasChanged) != S_OK)
    return;
  Int32 wasChanged;
  if (folderWasChanged->WasChanged(&wasChanged) != S_OK)
    return;
  if (wasChanged == 0)
    return;
  OnReload();
}

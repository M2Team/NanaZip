// PanelSelect.cpp

#include "StdAfx.h"

#include "resource.h"

#include "../../../Common/StringConvert.h"
#include "../../../Common/Wildcard.h"

#include "ComboDialog.h"
#include "LangUtils.h"
#include "Panel.h"

void CPanel::OnShiftSelectMessage()
{
  if (!_mySelectMode)
    return;
  int focusedItem = _listView.GetFocusedItem();
  if (focusedItem < 0)
    return;
  if (!_selectionIsDefined)
    return;
  int startItem = MyMin(focusedItem, _prevFocusedItem);
  int finishItem = MyMax(focusedItem, _prevFocusedItem);
    
  int numItems = _listView.GetItemCount();
  for (int i = 0; i < numItems; i++)
  {
    int realIndex = GetRealItemIndex(i);
    if (realIndex == kParentIndex)
      continue;
    if (i >= startItem && i <= finishItem)
      if (_selectedStatusVector[realIndex] != _selectMark)
      {
        _selectedStatusVector[realIndex] = _selectMark;
        _listView.RedrawItem(i);
      }
  }

  _prevFocusedItem = focusedItem;
}

void CPanel::OnArrowWithShift()
{
  if (!_mySelectMode)
    return;
  int focusedItem = _listView.GetFocusedItem();
  if (focusedItem < 0)
    return;
  int realIndex = GetRealItemIndex(focusedItem);
  
  if (_selectionIsDefined)
  {
    if (realIndex != kParentIndex)
      _selectedStatusVector[realIndex] = _selectMark;
  }
  else
  {
    if (realIndex == kParentIndex)
    {
      _selectionIsDefined = true;
      _selectMark = true;
    }
    else
    {
      _selectionIsDefined = true;
      _selectMark = !_selectedStatusVector[realIndex];
      _selectedStatusVector[realIndex] = _selectMark;
    }
  }
  
  _prevFocusedItem = focusedItem;
  PostMsg(kShiftSelectMessage);
  _listView.RedrawItem(focusedItem);
}

void CPanel::OnInsert()
{
  /*
  const int kState = CDIS_MARKED; // LVIS_DROPHILITED;
  UINT state = (_listView.GetItemState(focusedItem, LVIS_CUT) == 0) ?
      LVIS_CUT : 0;
  _listView.SetItemState(focusedItem, state, LVIS_CUT);
  // _listView.SetItemState_Selected(focusedItem);
  */

  int focusedItem = _listView.GetFocusedItem();
  if (focusedItem < 0)
    return;
  
  int realIndex = GetRealItemIndex(focusedItem);
  if (realIndex != kParentIndex)
  {
    bool isSelected = !_selectedStatusVector[realIndex];
    _selectedStatusVector[realIndex] = isSelected;
    if (!_mySelectMode)
      _listView.SetItemState_Selected(focusedItem, isSelected);
    _listView.RedrawItem(focusedItem);
  }

  int nextIndex = focusedItem + 1;
  if (nextIndex < _listView.GetItemCount())
  {
    _listView.SetItemState_FocusedSelected(nextIndex);
    _listView.EnsureVisible(nextIndex, false);
  }
}

/*
void CPanel::OnUpWithShift()
{
  int focusedItem = _listView.GetFocusedItem();
  if (focusedItem < 0)
    return;
  int index = GetRealItemIndex(focusedItem);
  if (index == kParentIndex)
    return;
  _selectedStatusVector[index] = !_selectedStatusVector[index];
  _listView.RedrawItem(index);
}

void CPanel::OnDownWithShift()
{
  int focusedItem = _listView.GetFocusedItem();
  if (focusedItem < 0)
    return;
  int index = GetRealItemIndex(focusedItem);
  if (index == kParentIndex)
    return;
  _selectedStatusVector[index] = !_selectedStatusVector[index];
  _listView.RedrawItem(index);
}
*/

void CPanel::UpdateSelection()
{
  if (!_mySelectMode)
  {
    bool enableTemp = _enableItemChangeNotify;
    _enableItemChangeNotify = false;
    int numItems = _listView.GetItemCount();
    for (int i = 0; i < numItems; i++)
    {
      int realIndex = GetRealItemIndex(i);
      if (realIndex != kParentIndex)
        _listView.SetItemState_Selected(i, _selectedStatusVector[realIndex]);
    }
    _enableItemChangeNotify = enableTemp;
  }
  _listView.RedrawAllItems();
}


void CPanel::SelectSpec(bool selectMode)
{
  CComboDialog dlg;
  LangString(selectMode ? IDS_SELECT : IDS_DESELECT, dlg.Title );
  LangString(IDS_SELECT_MASK, dlg.Static);
  dlg.Value = '*';
  if (dlg.Create(GetParent()) != IDOK)
    return;
  const UString &mask = dlg.Value;
  FOR_VECTOR (i, _selectedStatusVector)
    if (DoesWildcardMatchName(mask, GetItemName(i)))
       _selectedStatusVector[i] = selectMode;
  UpdateSelection();
}

void CPanel::SelectByType(bool selectMode)
{
  int focusedItem = _listView.GetFocusedItem();
  if (focusedItem < 0)
    return;
  int realIndex = GetRealItemIndex(focusedItem);
  UString name = GetItemName(realIndex);
  bool isItemFolder = IsItem_Folder(realIndex);

  if (isItemFolder)
  {
    FOR_VECTOR (i, _selectedStatusVector)
      if (IsItem_Folder(i) == isItemFolder)
        _selectedStatusVector[i] = selectMode;
  }
  else
  {
    int pos = name.ReverseFind_Dot();
    if (pos < 0)
    {
      FOR_VECTOR (i, _selectedStatusVector)
        if (IsItem_Folder(i) == isItemFolder && GetItemName(i).ReverseFind_Dot() < 0)
          _selectedStatusVector[i] = selectMode;
    }
    else
    {
      UString mask ('*');
      mask += name.Ptr((unsigned)pos);
      FOR_VECTOR (i, _selectedStatusVector)
        if (IsItem_Folder(i) == isItemFolder && DoesWildcardMatchName(mask, GetItemName(i)))
          _selectedStatusVector[i] = selectMode;
    }
  }

  UpdateSelection();
}

void CPanel::SelectAll(bool selectMode)
{
  FOR_VECTOR (i, _selectedStatusVector)
    _selectedStatusVector[i] = selectMode;
  UpdateSelection();
}

void CPanel::InvertSelection()
{
  if (!_mySelectMode)
  {
    unsigned numSelected = 0;
    FOR_VECTOR (i, _selectedStatusVector)
      if (_selectedStatusVector[i])
        numSelected++;
    // 17.02: fixed : now we invert item even, if single item is selected
    /*
    if (numSelected == 1)
    {
      int focused = _listView.GetFocusedItem();
      if (focused >= 0)
      {
        int realIndex = GetRealItemIndex(focused);
        if (realIndex >= 0)
          if (_selectedStatusVector[realIndex])
            _selectedStatusVector[realIndex] = false;
      }
    }
    */
  }
  FOR_VECTOR (i, _selectedStatusVector)
    _selectedStatusVector[i] = !_selectedStatusVector[i];
  UpdateSelection();
}

void CPanel::KillSelection()
{
  SelectAll(false);
  // ver 20.01: now we don't like that focused will be selected item.
  //   So the following code was disabled:
  /*
  if (!_mySelectMode)
  {
    int focused = _listView.GetFocusedItem();
    if (focused >= 0)
    {
      // CPanel::OnItemChanged notify for LVIS_SELECTED change doesn't work here. Why?
      // so we change _selectedStatusVector[realIndex] here.
      int realIndex = GetRealItemIndex(focused);
      if (realIndex != kParentIndex)
        _selectedStatusVector[realIndex] = true;
      _listView.SetItemState_Selected(focused);
    }
  }
  */
}

void CPanel::OnLeftClick(MY_NMLISTVIEW_NMITEMACTIVATE *itemActivate)
{
  if (itemActivate->hdr.hwndFrom != HWND(_listView))
    return;
  // It will work only for Version 4.71 (IE 4);
  int indexInList = itemActivate->iItem;
  if (indexInList < 0)
    return;
  
  #ifndef UNDER_CE
  if ((itemActivate->uKeyFlags & LVKF_SHIFT) != 0)
  {
    // int focusedIndex = _listView.GetFocusedItem();
    int focusedIndex = _startGroupSelect;
    if (focusedIndex < 0)
      return;
    int startItem = MyMin(focusedIndex, indexInList);
    int finishItem = MyMax(focusedIndex, indexInList);

    int numItems = _listView.GetItemCount();
    for (int i = 0; i < numItems; i++)
    {
      int realIndex = GetRealItemIndex(i);
      if (realIndex == kParentIndex)
        continue;
      bool selected = (i >= startItem && i <= finishItem);
      if (_selectedStatusVector[realIndex] != selected)
      {
        _selectedStatusVector[realIndex] = selected;
        _listView.RedrawItem(i);
      }
    }
  }
  else
  #endif
  {
    _startGroupSelect = indexInList;
  
    #ifndef UNDER_CE
    if ((itemActivate->uKeyFlags & LVKF_CONTROL) != 0)
    {
      int realIndex = GetRealItemIndex(indexInList);
      if (realIndex != kParentIndex)
      {
        _selectedStatusVector[realIndex] = !_selectedStatusVector[realIndex];
        _listView.RedrawItem(indexInList);
      }
    }
    #endif
  }

  return;
}

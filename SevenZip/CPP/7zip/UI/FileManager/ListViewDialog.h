// ListViewDialog.h

#ifndef __LISTVIEW_DIALOG_H
#define __LISTVIEW_DIALOG_H

#include "../../../Windows/Control/Dialog.h"
#include "../../../Windows/Control/ListView.h"

#include "ListViewDialogRes.h"

class CListViewDialog: public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CListView _listView;
  virtual void OnOK();
  virtual bool OnInit();
  virtual bool OnSize(WPARAM wParam, int xSize, int ySize);
  virtual bool OnNotify(UINT controlID, LPNMHDR header);
  void CopyToClipboard();
  void DeleteItems();
  void ShowItemInfo();
  void OnEnter();
public:
  UString Title;
  
  bool SelectFirst;
  bool DeleteIsAllowed;
  bool StringsWereChanged;
  
  UStringVector Strings;
  UStringVector Values;
  
  int FocusedItemIndex;
  unsigned NumColumns;

  INT_PTR Create(HWND wndParent = 0) { return CModalDialog::Create(IDD_LISTVIEW, wndParent); }

  CListViewDialog():
    SelectFirst(false),
    DeleteIsAllowed(false),
    StringsWereChanged(false),
    FocusedItemIndex(-1),
    NumColumns(1)
    {}
};

#endif

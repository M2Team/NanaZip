// ListViewDialog.h

#ifndef ZIP7_INC_LISTVIEW_DIALOG_H
#define ZIP7_INC_LISTVIEW_DIALOG_H

#include "../../../Windows/Control/Dialog.h"
#include "../../../Windows/Control/ListView.h"

#include "ListViewDialogRes.h"

class CListViewDialog: public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CListView _listView;
  virtual void OnOK() Z7_override;
  virtual bool OnInit() Z7_override;
  virtual bool OnSize(WPARAM wParam, int xSize, int ySize) Z7_override;
  virtual bool OnNotify(UINT controlID, LPNMHDR header) Z7_override;
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

  INT_PTR Create(HWND wndParent = NULL) { return CModalDialog::Create(IDD_LISTVIEW, wndParent); }

  CListViewDialog():
    SelectFirst(false),
    DeleteIsAllowed(false),
    StringsWereChanged(false),
    FocusedItemIndex(-1),
    NumColumns(1)
    {}
};

#endif

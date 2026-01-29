// EditDialog.h

#ifndef ZIP7_INC_EDIT_DIALOG_H
#define ZIP7_INC_EDIT_DIALOG_H

#include "../../../Windows/Control/Dialog.h"
#include "../../../Windows/Control/Edit.h"

#include "EditDialogRes.h"

class CEditDialog: public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CEdit _edit;
  virtual bool OnInit() Z7_override;
  virtual bool OnSize(WPARAM wParam, int xSize, int ySize) Z7_override;
public:
  UString Title;
  UString Text;

  INT_PTR Create(HWND wndParent = NULL) { return CModalDialog::Create(IDD_EDIT_DLG, wndParent); }

  CEditDialog() {}
};

#endif

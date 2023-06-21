// EditDialog.h

#ifndef __EDIT_DIALOG_H
#define __EDIT_DIALOG_H

#include "../../../../../ThirdParty/LZMA/CPP/Windows/Control/Dialog.h"
#include "../../../../../ThirdParty/LZMA/CPP/Windows/Control/Edit.h"

#include "EditDialogRes.h"

class CEditDialog: public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CEdit _edit;
  virtual bool OnInit();
  virtual bool OnSize(WPARAM wParam, int xSize, int ySize);
public:
  UString Title;
  UString Text;

  INT_PTR Create(HWND wndParent = 0) { return CModalDialog::Create(IDD_EDIT_DLG, wndParent); }

  CEditDialog() {}
};

#endif

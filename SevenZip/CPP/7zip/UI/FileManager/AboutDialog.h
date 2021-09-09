// AboutDialog.h
 
#ifndef __ABOUT_DIALOG_H
#define __ABOUT_DIALOG_H

#include "../../../Windows/Control/Dialog.h"

#include "AboutDialogRes.h"

class CAboutDialog: public NWindows::NControl::CModalDialog
{
public:
  virtual bool OnInit();
  virtual void OnHelp();
  virtual bool OnButtonClicked(int buttonID, HWND buttonHWND);
  INT_PTR Create(HWND wndParent = 0) { return CModalDialog::Create(IDD_ABOUT, wndParent); }
};

#endif

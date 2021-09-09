// PasswordDialog.h

#ifndef __PASSWORD_DIALOG_H
#define __PASSWORD_DIALOG_H

#include "../../../Windows/Control/Dialog.h"
#include "../../../Windows/Control/Edit.h"

#include "PasswordDialogRes.h"

class CPasswordDialog: public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CEdit _passwordEdit;

  virtual void OnOK();
  virtual bool OnInit();
  virtual bool OnButtonClicked(int buttonID, HWND buttonHWND);
  void SetTextSpec();
  void ReadControls();
public:
  UString Password;
  bool ShowPassword;
  
  CPasswordDialog(): ShowPassword(false) {}
  INT_PTR Create(HWND parentWindow = 0) { return CModalDialog::Create(IDD_PASSWORD, parentWindow); }
};

#endif

﻿// PasswordDialog.cpp

#include "StdAfx.h"

#include "PasswordDialog.h"

#ifdef LANG
#include "LangUtils.h"
#endif

#ifdef LANG
static const UInt32 kLangIDs[] =
{
  IDT_PASSWORD_ENTER,
  IDX_PASSWORD_SHOW
};
#endif

void CPasswordDialog::ReadControls()
{
  _passwordEdit.GetText(Password);
  ShowPassword = IsButtonCheckedBool(IDX_PASSWORD_SHOW);
}

void CPasswordDialog::SetTextSpec()
{
  _passwordEdit.SetPasswordChar(ShowPassword ? 0: TEXT('*'));
  _passwordEdit.SetText(Password);
}

bool CPasswordDialog::OnInit()
{
  #ifdef LANG
  LangSetWindowText(*this, IDD_PASSWORD);
  LangSetDlgItems(*this, kLangIDs, ARRAY_SIZE(kLangIDs));
  #endif
  _passwordEdit.Attach(GetItem(IDE_PASSWORD_PASSWORD));
  CheckButton(IDX_PASSWORD_SHOW, ShowPassword);
  SetTextSpec();
  return CModalDialog::OnInit();
}

bool CPasswordDialog::OnButtonClicked(int buttonID, HWND buttonHWND)
{
  if (buttonID == IDX_PASSWORD_SHOW)
  {
    ReadControls();
    SetTextSpec();
    return true;
  }
  return CDialog::OnButtonClicked(buttonID, buttonHWND);
}

void CPasswordDialog::OnOK()
{
  ReadControls();
  CModalDialog::OnOK();
}

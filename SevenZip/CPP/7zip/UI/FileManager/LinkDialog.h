// LinkDialog.h

#ifndef __LINK_DIALOG_H
#define __LINK_DIALOG_H

#include "../../../Windows/Control/Dialog.h"
#include "../../../Windows/Control/ComboBox.h"

#include "LinkDialogRes.h"

class CLinkDialog: public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CComboBox _pathFromCombo;
  NWindows::NControl::CComboBox _pathToCombo;
  
  virtual bool OnInit();
  virtual bool OnSize(WPARAM wParam, int xSize, int ySize);
  virtual bool OnButtonClicked(int buttonID, HWND buttonHWND);
  void OnButton_SetPath(bool to);
  void OnButton_Link();

  void ShowLastErrorMessage();
  void ShowError(const wchar_t *s);
  void Set_LinkType_Radio(int idb);
public:
  UString CurDirPrefix;
  UString FilePath;
  UString AnotherPath;
  
  INT_PTR Create(HWND parentWindow = 0)
    { return CModalDialog::Create(IDD_LINK, parentWindow); }
};

#endif

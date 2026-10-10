// LinkDialog.h

#ifndef ZIP7_INC_LINK_DIALOG_H
#define ZIP7_INC_LINK_DIALOG_H

#include "../../../Windows/Control/Dialog.h"
#include "../../../Windows/Control/ComboBox.h"

#include "LinkDialogRes.h"

class CLinkDialog: public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CComboBox _pathFromCombo;
  NWindows::NControl::CComboBox _pathToCombo;

  virtual bool OnInit() Z7_override;
  virtual bool OnSize(WPARAM wParam, int xSize, int ySize) Z7_override;
  virtual bool OnButtonClicked(unsigned buttonID, HWND buttonHWND) Z7_override;
  void OnButton_SetPath(bool to);
  void OnButton_Link();

  void ShowLastErrorMessage();
  void ShowError(const wchar_t *s);
  void Set_LinkType_Radio(unsigned idb);
public:
  UString CurDirPrefix;
  UString FilePath;
  UString AnotherPath;

  INT_PTR Create(HWND parentWindow = NULL)
    { return CModalDialog::Create(IDD_LINK, parentWindow); }
};

#endif

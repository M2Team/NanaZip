// CopyDialog.h

#ifndef __COPY_DIALOG_H
#define __COPY_DIALOG_H

#include "../../../Windows/Control/ComboBox.h"
#include "../../../Windows/Control/Dialog.h"

#include "CopyDialogRes.h"

const int kCopyDialog_NumInfoLines = 11;

class CCopyDialog: public NWindows::NControl::CModalDialog
{
// **************** NanaZip Modification Start ****************
  static LRESULT CALLBACK ModernWindowHandler(
      _In_ HWND hWnd,
      _In_ UINT uMsg,
      _In_ WPARAM wParam,
      _In_ LPARAM lParam,
      _In_ UINT_PTR uIdSubclass,
      _In_ DWORD_PTR dwRefData);

  bool ModernMessageRouter(UINT uMsg, WPARAM wParam, LPARAM lParam);

  void ModernOK();

#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
  NWindows::NControl::CComboBox _path;
  virtual void OnOK();
  virtual bool OnInit();
  virtual bool OnSize(WPARAM wParam, int xSize, int ySize);
  void OnButtonSetPath();
  bool OnButtonClicked(int buttonID, HWND buttonHWND);
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
// **************** NanaZip Modification End ****************
public:
  UString Title;
  UString Static;
  UString Value;
  UString Info;
  UStringVector Strings;
// **************** NanaZip Modification Start ****************
  HWND m_WindowHandle = nullptr;
  bool m_FirstRun = false;
  int m_ReturnCode = IDCLOSE;
// **************** NanaZip Modification End ****************

// **************** NanaZip Modification Start ****************
  INT_PTR Create(HWND parentWindow = 0);
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
  INT_PTR Create(HWND parentWindow = 0) { return CModalDialog::Create(IDD_COPY, parentWindow); }
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
// **************** NanaZip Modification End ****************
};

#endif

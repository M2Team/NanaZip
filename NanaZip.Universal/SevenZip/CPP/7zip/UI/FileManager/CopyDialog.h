// CopyDialog.h

#ifndef ZIP7_INC_COPY_DIALOG_H
#define ZIP7_INC_COPY_DIALOG_H

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
  void ModernExtractAll();

#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
  NWindows::NControl::CComboBox _path;
  virtual void OnOK() Z7_override;
  virtual bool OnInit() Z7_override;
  virtual bool OnSize(WPARAM wParam, int xSize, int ySize) Z7_override;
  virtual bool OnButtonClicked(unsigned buttonID, HWND buttonHWND) Z7_override;
  void OnButtonSetPath();
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
  bool m_ShowExtractAll = false;
  // **************** NanaZip Modification End ****************

// **************** NanaZip Modification Start ****************
  INT_PTR Create(HWND parentWindow = nullptr);
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
  INT_PTR Create(HWND parentWindow = NULL) { return CModalDialog::Create(IDD_COPY, parentWindow); }
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
// **************** NanaZip Modification End ****************
};

#endif

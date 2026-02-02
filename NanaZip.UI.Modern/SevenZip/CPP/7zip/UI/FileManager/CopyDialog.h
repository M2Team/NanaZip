// CopyDialog.h

#ifndef __COPY_DIALOG_H
#define __COPY_DIALOG_H

#include "../../../Windows/Control/ComboBox.h"
#include "../../../Windows/Control/Dialog.h"

#include "CopyDialogRes.h"

#include <winrt/NanaZip.Modern.h>

const int kCopyDialog_NumInfoLines = 10 /* 11 */;

class CCopyDialog /* : public NWindows::NControl::CModalDialog */
{
  // NWindows::NControl::CComboBox _path;
  // virtual void OnOK();
  // virtual bool OnInit();
  // virtual bool OnSize(WPARAM wParam, int xSize, int ySize);
  void OnButtonSetPath();
  // bool OnButtonClicked(int buttonID, HWND buttonHWND);

  winrt::NanaZip::Modern::CopyPage m_CopyPage = nullptr;
  HWND m_IslandsHwnd = nullptr;
public:
  UString Title;
  UString Static;
  UString Value;
  UString Info;
  UStringVector Strings;
  INT_PTR Result = IDCLOSE;

  INT_PTR Create(HWND parentWindow = 0);
};

#endif

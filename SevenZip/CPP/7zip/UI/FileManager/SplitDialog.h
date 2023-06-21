// SplitDialog.h

#ifndef __SPLIT_DIALOG_H
#define __SPLIT_DIALOG_H

#include "../../../../../ThirdParty/LZMA/CPP/Windows/Control/Dialog.h"
#include "../../../../../ThirdParty/LZMA/CPP/Windows/Control/ComboBox.h"

#include "SplitDialogRes.h"

class CSplitDialog: public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CComboBox _pathCombo;
  NWindows::NControl::CComboBox _volumeCombo;
  virtual void OnOK();
  virtual bool OnInit();
  virtual bool OnSize(WPARAM wParam, int xSize, int ySize);
  virtual bool OnButtonClicked(int buttonID, HWND buttonHWND);
  void OnButtonSetPath();
public:
  UString FilePath;
  UString Path;
  CRecordVector<UInt64> VolumeSizes;
  INT_PTR Create(HWND parentWindow = 0)
    { return CModalDialog::Create(IDD_SPLIT, parentWindow); }
};

#endif

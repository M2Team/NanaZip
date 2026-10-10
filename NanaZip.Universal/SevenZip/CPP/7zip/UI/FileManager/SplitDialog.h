// SplitDialog.h

#ifndef ZIP7_INC_SPLIT_DIALOG_H
#define ZIP7_INC_SPLIT_DIALOG_H

#include "../../../Windows/Control/Dialog.h"
#include "../../../Windows/Control/ComboBox.h"

#include "SplitDialogRes.h"

class CSplitDialog: public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CComboBox _pathCombo;
  NWindows::NControl::CComboBox _volumeCombo;
  virtual void OnOK() Z7_override;
  virtual bool OnInit() Z7_override;
  virtual bool OnSize(WPARAM wParam, int xSize, int ySize) Z7_override;
  virtual bool OnButtonClicked(unsigned buttonID, HWND buttonHWND) Z7_override;
  void OnButtonSetPath();
public:
  UString FilePath;
  UString Path;
  CRecordVector<UInt64> VolumeSizes;
  INT_PTR Create(HWND parentWindow = NULL)
    { return CModalDialog::Create(IDD_SPLIT, parentWindow); }
};

#endif

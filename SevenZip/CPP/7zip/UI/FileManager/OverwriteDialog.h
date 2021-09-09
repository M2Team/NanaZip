// OverwriteDialog.h

#ifndef __OVERWRITE_DIALOG_H
#define __OVERWRITE_DIALOG_H

#include "../../../Windows/Control/Dialog.h"

#include "DialogSize.h"
#include "OverwriteDialogRes.h"

namespace NOverwriteDialog
{
  struct CFileInfo
  {
    bool SizeIsDefined;
    bool TimeIsDefined;
    UInt64 Size;
    FILETIME Time;
    UString Name;
    
    void SetTime(const FILETIME *t)
    {
      if (!t)
        TimeIsDefined = false;
      else
      {
        TimeIsDefined = true;
        Time = *t;
      }
    }

    void SetSize(UInt64 size)
    {
      SizeIsDefined = true;
      Size = size;
    }

    void SetSize(const UInt64 *size)
    {
      if (!size)
        SizeIsDefined = false;
      else
        SetSize(*size);
    }
  };
}

class COverwriteDialog: public NWindows::NControl::CModalDialog
{
  bool _isBig;

  void SetFileInfoControl(int textID, int iconID, const NOverwriteDialog::CFileInfo &fileInfo);
  virtual bool OnInit();
  bool OnButtonClicked(int buttonID, HWND buttonHWND);
  void ReduceString(UString &s);

public:
  bool ShowExtraButtons;
  bool DefaultButton_is_NO;


  COverwriteDialog(): ShowExtraButtons(true), DefaultButton_is_NO(false) {}

  INT_PTR Create(HWND parent = 0)
  {
    BIG_DIALOG_SIZE(280, 200);
    #ifdef UNDER_CE
    _isBig = isBig;
    #else
    _isBig = true;
    #endif
    return CModalDialog::Create(SIZED_DIALOG(IDD_OVERWRITE), parent);
  }

  NOverwriteDialog::CFileInfo OldFileInfo;
  NOverwriteDialog::CFileInfo NewFileInfo;
};

#endif

// OverwriteDialog.h

#ifndef ZIP7_INC_OVERWRITE_DIALOG_H
#define ZIP7_INC_OVERWRITE_DIALOG_H

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

  void SetFileInfoControl(unsigned textID, unsigned iconID, const NOverwriteDialog::CFileInfo &fileInfo);
  virtual bool OnInit() Z7_override;
  virtual bool OnButtonClicked(unsigned buttonID, HWND buttonHWND) Z7_override;
  void ReduceString(UString &s);

public:
  bool ShowExtraButtons;
  bool DefaultButton_is_NO;


  COverwriteDialog(): ShowExtraButtons(true), DefaultButton_is_NO(false) {}

  INT_PTR Create(HWND parent = NULL)
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

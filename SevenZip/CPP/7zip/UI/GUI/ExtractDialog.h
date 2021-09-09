// ExtractDialog.h

#ifndef __EXTRACT_DIALOG_H
#define __EXTRACT_DIALOG_H

#include "ExtractDialogRes.h"

#include "../../../Windows/Control/ComboBox.h"
#include "../../../Windows/Control/Edit.h"

#include "../Common/ExtractMode.h"

#include "../FileManager/DialogSize.h"

#ifndef NO_REGISTRY
#include "../Common/ZipRegistry.h"
#endif

namespace NExtractionDialog
{
  /*
  namespace NFilesMode
  {
    enum EEnum
    {
      kSelected,
      kAll,
      kSpecified
    };
  }
  */
}

class CExtractDialog: public NWindows::NControl::CModalDialog
{
  #ifdef NO_REGISTRY
  NWindows::NControl::CDialogChildControl _path;
  #else
  NWindows::NControl::CComboBox _path;
  #endif

  #ifndef _SFX
  NWindows::NControl::CEdit _pathName;
  NWindows::NControl::CEdit _passwordControl;
  NWindows::NControl::CComboBox _pathMode;
  NWindows::NControl::CComboBox _overwriteMode;
  #endif

  #ifndef _SFX
  // int GetFilesMode() const;
  void UpdatePasswordControl();
  #endif
  
  void OnButtonSetPath();

  void CheckButton_TwoBools(UINT id, const CBoolPair &b1, const CBoolPair &b2);
  void GetButton_Bools(UINT id, CBoolPair &b1, CBoolPair &b2);
  virtual bool OnInit();
  virtual bool OnButtonClicked(int buttonID, HWND buttonHWND);
  virtual void OnOK();
  
  #ifndef NO_REGISTRY

  virtual void OnHelp();

  NExtract::CInfo _info;
  
  #endif
  
  bool IsShowPasswordChecked() const { return IsButtonCheckedBool(IDX_PASSWORD_SHOW); }
public:
  // bool _enableSelectedFilesButton;
  // bool _enableFilesButton;
  // NExtractionDialog::NFilesMode::EEnum FilesMode;

  UString DirPath;
  UString ArcPath;

  #ifndef _SFX
  UString Password;
  #endif
  bool PathMode_Force;
  bool OverwriteMode_Force;
  NExtract::NPathMode::EEnum PathMode;
  NExtract::NOverwriteMode::EEnum OverwriteMode;

  #ifndef _SFX
  // CBoolPair AltStreams;
  CBoolPair NtSecurity;
  #endif

  CBoolPair ElimDup;

  INT_PTR Create(HWND aWndParent = 0)
  {
    #ifdef _SFX
    BIG_DIALOG_SIZE(240, 64);
    #else
    BIG_DIALOG_SIZE(300, 160);
    #endif
    return CModalDialog::Create(SIZED_DIALOG(IDD_EXTRACT), aWndParent);
  }

  CExtractDialog():
    PathMode_Force(false),
    OverwriteMode_Force(false)
  {
    ElimDup.Val = true;
  }

};

#endif

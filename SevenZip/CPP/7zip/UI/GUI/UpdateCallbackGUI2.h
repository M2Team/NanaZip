// UpdateCallbackGUI2.h

#ifndef __UPDATE_CALLBACK_GUI2_H
#define __UPDATE_CALLBACK_GUI2_H

#include "../FileManager/ProgressDialog2.h"

class CUpdateCallbackGUI2
{
  UStringVector _lang_Ops;
  UString _emptyString;
public:
  UString Password;
  bool PasswordIsDefined;
  bool PasswordWasAsked;
  UInt64 NumFiles;

  UString _lang_Removing;

  CUpdateCallbackGUI2():
      PasswordIsDefined(false),
      PasswordWasAsked(false),
      NumFiles(0)
      {}
  
  // ~CUpdateCallbackGUI2();
  void Init();

  CProgressDialog *ProgressDialog;

  HRESULT SetOperation_Base(UInt32 notifyOp, const wchar_t *name, bool isDir);
  HRESULT ShowAskPasswordDialog();
};

#endif

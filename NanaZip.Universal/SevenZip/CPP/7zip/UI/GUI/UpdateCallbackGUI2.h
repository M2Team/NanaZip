// UpdateCallbackGUI2.h

#ifndef ZIP7_INC_UPDATE_CALLBACK_GUI2_H
#define ZIP7_INC_UPDATE_CALLBACK_GUI2_H

#include "../FileManager/ProgressDialog2.h"

class CUpdateCallbackGUI2
{
public:
  CProgressDialog *ProgressDialog;
protected:
  UString _arcMoving_name1;
  UString _arcMoving_name2;
  UInt64 _arcMoving_percents;
  UInt64 _arcMoving_total;
  UInt64 _arcMoving_current;
  Int32  _arcMoving_updateMode;
public:
  bool PasswordIsDefined;
  bool PasswordWasAsked;
  UInt64 NumFiles;
  UString Password;
protected:
  UStringVector _lang_Ops;
  UString _lang_Removing;
  UString _lang_Moving;
  UString _emptyString;

  HRESULT MoveArc_UpdateStatus();
  HRESULT MoveArc_Start_Base(const wchar_t *srcTempPath, const wchar_t *destFinalPath, UInt64 /* totalSize */, Int32 updateMode);
  HRESULT MoveArc_Progress_Base(UInt64 totalSize, UInt64 currentSize);
  HRESULT MoveArc_Finish_Base();

public:

  CUpdateCallbackGUI2():
      _arcMoving_percents(0),
      _arcMoving_total(0),
      _arcMoving_current(0),
      _arcMoving_updateMode(0),
      PasswordIsDefined(false),
      PasswordWasAsked(false),
      NumFiles(0)
      {}
  
  void Init();

  HRESULT SetOperation_Base(UInt32 notifyOp, const wchar_t *name, bool isDir);
  HRESULT ShowAskPasswordDialog();
};

#endif

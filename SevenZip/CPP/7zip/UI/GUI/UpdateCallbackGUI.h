// UpdateCallbackGUI.h

#ifndef __UPDATE_CALLBACK_GUI_H
#define __UPDATE_CALLBACK_GUI_H

#include "../Common/Update.h"
#include "../Common/ArchiveOpenCallback.h"

#include "UpdateCallbackGUI2.h"

class CUpdateCallbackGUI:
  public IOpenCallbackUI,
  public IUpdateCallbackUI2,
  public CUpdateCallbackGUI2
{
public:
  // CUpdateCallbackGUI();
  // ~CUpdateCallbackGUI();

  bool AskPassword;

  void Init();

  CUpdateCallbackGUI():
      AskPassword(false)
      {}

  INTERFACE_IUpdateCallbackUI2(;)
  INTERFACE_IOpenCallbackUI(;)

  FStringVector FailedFiles;
};

#endif

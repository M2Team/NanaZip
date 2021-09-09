// FoldersPage.h
 
#ifndef __FOLDERS_PAGE_H
#define __FOLDERS_PAGE_H

#include "../../../Windows/Control/PropertyPage.h"

#include "../Common/ZipRegistry.h"

class CFoldersPage : public NWindows::NControl::CPropertyPage
{
  NWorkDir::CInfo m_WorkDirInfo;
  NWindows::NControl::CDialogChildControl m_WorkPath;

  bool _needSave;
  bool _initMode;

  void MyEnableControls();
  void ModifiedEvent();
  
  void OnFoldersWorkButtonPath();
  int GetWorkMode() const;
  void GetWorkDir(NWorkDir::CInfo &workDirInfo);
  // bool WasChanged();
  virtual bool OnInit();
  virtual bool OnCommand(int code, int itemID, LPARAM lParam);
  virtual void OnNotifyHelp();
  virtual LONG OnApply();
  virtual bool OnButtonClicked(int buttonID, HWND buttonHWND);
};

#endif

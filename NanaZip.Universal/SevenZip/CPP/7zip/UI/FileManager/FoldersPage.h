// FoldersPage.h

#ifndef ZIP7_INC_FOLDERS_PAGE_H
#define ZIP7_INC_FOLDERS_PAGE_H

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
  virtual bool OnInit() Z7_override;
  virtual bool OnCommand(unsigned code, unsigned itemID, LPARAM lParam) Z7_override;
  // **************** NanaZip Modification Start ****************
  // virtual void OnNotifyHelp() Z7_override;
  // **************** NanaZip Modification End ****************
  virtual LONG OnApply() Z7_override;
  virtual bool OnButtonClicked(unsigned buttonID, HWND buttonHWND) Z7_override;
};

#endif

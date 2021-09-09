// EditPage.h
 
#ifndef __EDIT_PAGE_H
#define __EDIT_PAGE_H

#include "../../../Windows/Control/PropertyPage.h"
#include "../../../Windows/Control/Edit.h"

struct CEditPageCtrl
{
  NWindows::NControl::CEdit Edit;
  bool WasChanged;
  int Ctrl;
  int Button;
};

class CEditPage: public NWindows::NControl::CPropertyPage
{
  CEditPageCtrl _ctrls[3];

  bool _initMode;
public:
  virtual bool OnInit();
  virtual void OnNotifyHelp();
  virtual bool OnCommand(int code, int itemID, LPARAM param);
  virtual LONG OnApply();
  virtual bool OnButtonClicked(int buttonID, HWND buttonHWND);
};

#endif

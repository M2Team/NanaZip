// EditPage.h

#ifndef ZIP7_INC_EDIT_PAGE_H
#define ZIP7_INC_EDIT_PAGE_H

#include "../../../Windows/Control/PropertyPage.h"
#include "../../../Windows/Control/Edit.h"

struct CEditPageCtrl
{
  NWindows::NControl::CEdit Edit;
  bool WasChanged;
  unsigned Ctrl;
  unsigned Button;
};

class CEditPage: public NWindows::NControl::CPropertyPage
{
  CEditPageCtrl _ctrls[3];

  bool _initMode;
public:
  virtual bool OnInit() Z7_override;
  // **************** NanaZip Modification Start ****************
  // virtual void OnNotifyHelp() Z7_override;
  // **************** NanaZip Modification End ****************
  virtual bool OnCommand(unsigned code, unsigned itemID, LPARAM param) Z7_override;
  virtual LONG OnApply() Z7_override;
  virtual bool OnButtonClicked(unsigned buttonID, HWND buttonHWND) Z7_override;
};

#endif

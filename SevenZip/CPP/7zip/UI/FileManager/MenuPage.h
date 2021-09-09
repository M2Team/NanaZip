// MenuPage.h
 
#ifndef __MENU_PAGE_H
#define __MENU_PAGE_H

#include "../../../Windows/Control/PropertyPage.h"
#include "../../../Windows/Control/ListView.h"

struct CShellDll
{
  FString Path;
  bool wasChanged;
  bool prevValue;
  int ctrl;
  UInt32 wow;

  CShellDll(): wasChanged (false), prevValue(false), ctrl(0), wow(0) {}
};

class CMenuPage: public NWindows::NControl::CPropertyPage
{
  bool _initMode;

  bool _cascaded_Changed;
  bool _menuIcons_Changed;
  bool _elimDup_Changed;
  bool _flags_Changed;

  void Clear_MenuChanged()
  {
    _cascaded_Changed = false;
    _menuIcons_Changed = false;
    _elimDup_Changed = false;
    _flags_Changed = false;
  }
  
  #ifndef UNDER_CE
  CShellDll _dlls[2];
  #endif
  
  NWindows::NControl::CListView _listView;

  virtual bool OnInit();
  virtual void OnNotifyHelp();
  virtual bool OnNotify(UINT controlID, LPNMHDR lParam);
  virtual bool OnItemChanged(const NMLISTVIEW *info);
  virtual LONG OnApply();
  virtual bool OnButtonClicked(int buttonID, HWND buttonHWND);
public:
};

#endif

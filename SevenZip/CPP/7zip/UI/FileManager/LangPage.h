// LangPage.h
 
#ifndef __LANG_PAGE_H
#define __LANG_PAGE_H

#include "../../../Windows/Control/PropertyPage.h"
#include "../../../Windows/Control/ComboBox.h"

class CLangPage: public NWindows::NControl::CPropertyPage
{
  NWindows::NControl::CComboBox _langCombo;
  UStringVector _paths;

  bool _needSave;
public:
  bool LangWasChanged;
  
  CLangPage(): _needSave(false), LangWasChanged(false) {}
  virtual bool OnInit();
  virtual void OnNotifyHelp();
  virtual bool OnCommand(int code, int itemID, LPARAM param);
  virtual LONG OnApply();
};

#endif

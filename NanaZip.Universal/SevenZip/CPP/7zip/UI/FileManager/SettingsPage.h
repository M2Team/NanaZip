// SettingsPage.h

#ifndef ZIP7_INC_SETTINGS_PAGE_H
#define ZIP7_INC_SETTINGS_PAGE_H

#include "../../../Windows/Control/PropertyPage.h"
// #include "../../../Windows/Control/ComboBox.h"
#include "../../../Windows/Control/Edit.h"

class CSettingsPage: public NWindows::NControl::CPropertyPage
{
  bool _wasChanged;
  bool _largePages_wasChanged;
  bool _memx_wasChanged;
  bool _initMode;
  /*
  bool _wasChanged_MemLimit;
  NWindows::NControl::CComboBox _memCombo;
  UStringVector _memLimitStrings;
  UInt64 _ramSize;
  UInt64 _ramSize_Defined;

  int AddMemComboItem(UInt64 size, UInt64 percents = 0, bool isDefault = false);
  */

  // void EnableSubItems();
  bool OnCommand(unsigned code, unsigned itemID, LPARAM param) Z7_override;
  virtual bool OnButtonClicked(unsigned buttonID, HWND buttonHWND) Z7_override;
  virtual bool OnInit() Z7_override;
  // **************** NanaZip Modification Start ****************
  // virtual void OnNotifyHelp() Z7_override;
  // **************** NanaZip Modification End ****************
  virtual LONG OnApply() Z7_override;

  void EnableSpin(bool enable);
public:
};

#endif

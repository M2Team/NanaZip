// MenuPage.h

#ifndef __MENU_PAGE_H
#define __MENU_PAGE_H

#include "../../../Windows/Control/PropertyPage.h"
#include "../../../Windows/Control/ComboBox.h"
#include "../../../Windows/Control/ListView.h"

class CMenuPage: public NWindows::NControl::CPropertyPage
{
  bool _initMode;

  bool _elimDup_Changed;
  bool _writeZone_Changed;
  bool _flags_Changed;

  // **************** NanaZip Modification Start ****************
  bool m_ExtractOnOpenChanged;
  // **************** NanaZip Modification End ****************

  void Clear_MenuChanged()
  {
    _elimDup_Changed = false;
    _writeZone_Changed = false;
    _flags_Changed = false;

    // **************** NanaZip Modification Start ****************
    m_ExtractOnOpenChanged = false;
    // **************** NanaZip Modification End ****************
  }

  NWindows::NControl::CListView _listView;
  NWindows::NControl::CComboBox _zoneCombo;

  virtual bool OnInit();
  virtual bool OnNotify(UINT controlID, LPNMHDR lParam);
  virtual bool OnItemChanged(const NMLISTVIEW *info);
  virtual LONG OnApply();
  virtual bool OnButtonClicked(int buttonID, HWND buttonHWND);
  virtual bool OnCommand(int code, int itemID, LPARAM param);
public:
};

#endif

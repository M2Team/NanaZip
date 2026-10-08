// MenuPage.h

#ifndef ZIP7_INC_MENU_PAGE_H
#define ZIP7_INC_MENU_PAGE_H

#include "../../../Windows/Control/PropertyPage.h"
#include "../../../Windows/Control/ComboBox.h"
#include "../../../Windows/Control/ListView.h"

// **************** NanaZip Modification Start ****************
// Removed from NanaZip.
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
struct CShellDll
{
  FString Path;
  bool wasChanged;
  bool prevValue;
  unsigned ctrl;
  UInt32 wow;

  CShellDll(): wasChanged (false), prevValue(false), ctrl(0), wow(0) {}
};
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
// **************** NanaZip Modification End ****************

class CMenuPage: public NWindows::NControl::CPropertyPage
{
  bool _initMode;

  // **************** NanaZip Modification Start ****************
  // Removed from NanaZip.
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
  bool _cascaded_Changed;
  bool _menuIcons_Changed;
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
  // **************** NanaZip Modification End ****************
  bool _elimDup_Changed;
  bool _writeZone_Changed;
  bool _flags_Changed;

  // **************** NanaZip Modification Start ****************
  bool m_ExtractOnOpenChanged;
  // **************** NanaZip Modification End ****************

  void Clear_MenuChanged()
  {
    // **************** NanaZip Modification Start ****************
    // Removed from NanaZip.
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
    _cascaded_Changed = false;
    _menuIcons_Changed = false;
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
    // **************** NanaZip Modification End ****************
    _elimDup_Changed = false;
    _writeZone_Changed = false;
    _flags_Changed = false;

    // **************** NanaZip Modification Start ****************
    m_ExtractOnOpenChanged = false;
    // **************** NanaZip Modification End ****************
  }

  // **************** NanaZip Modification Start ****************
  // Removed from NanaZip.
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
  #ifndef UNDER_CE
  CShellDll _dlls[2];
  #endif
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
  // **************** NanaZip Modification End ****************

  NWindows::NControl::CListView _listView;
  NWindows::NControl::CComboBox _zoneCombo;

  virtual bool OnInit() Z7_override;
  // **************** NanaZip Modification Start ****************
  // virtual void OnNotifyHelp() Z7_override;
  // **************** NanaZip Modification End ****************
  virtual bool OnNotify(UINT controlID, LPNMHDR lParam) Z7_override;
  virtual LONG OnApply() Z7_override;
  virtual bool OnButtonClicked(unsigned buttonID, HWND buttonHWND) Z7_override;
  virtual bool OnCommand(unsigned code, unsigned itemID, LPARAM param) Z7_override;

  bool OnItemChanged(const NMLISTVIEW* info);
};

#endif

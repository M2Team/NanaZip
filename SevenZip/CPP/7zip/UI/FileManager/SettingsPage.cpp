// SettingsPage.cpp

#include "StdAfx.h"

#include "../../../Common/StringConvert.h"

#ifndef UNDER_CE
#include "../../../Windows/MemoryLock.h"
#endif

#include "LangUtils.h"
#include "RegistryUtils.h"
#include "SettingsPage.h"

#include "SettingsPageRes.h"

using namespace NWindows;

static const UInt32 kLangIDs[] =
{
  IDX_SETTINGS_SHOW_DOTS,
  IDX_SETTINGS_SHOW_REAL_FILE_ICONS,
  IDX_SETTINGS_SHOW_SYSTEM_MENU,
  IDX_SETTINGS_FULL_ROW,
  IDX_SETTINGS_SHOW_GRID,
  IDX_SETTINGS_SINGLE_CLICK,
  IDX_SETTINGS_ALTERNATIVE_SELECTION,
  IDX_SETTINGS_LARGE_PAGES,
  IDX_SETTINGS_WANT_ARC_HISTORY,
  IDX_SETTINGS_WANT_PATH_HISTORY,
  IDX_SETTINGS_WANT_COPY_HISTORY,
  IDX_SETTINGS_WANT_FOLDER_HISTORY,
  IDX_SETTINGS_LOWERCASE_HASHES
};

extern bool IsLargePageSupported();

bool CSettingsPage::OnInit()
{
  _wasChanged = false;
  _largePages_wasChanged = false;

  LangSetDlgItems(*this, kLangIDs, ARRAY_SIZE(kLangIDs));

  CFmSettings st;
  st.Load();

  CheckButton(IDX_SETTINGS_SHOW_DOTS, st.ShowDots);
  CheckButton(IDX_SETTINGS_SHOW_REAL_FILE_ICONS, st.ShowRealFileIcons);
  CheckButton(IDX_SETTINGS_FULL_ROW, st.FullRow);
  CheckButton(IDX_SETTINGS_SHOW_GRID, st.ShowGrid);
  CheckButton(IDX_SETTINGS_SINGLE_CLICK, st.SingleClick);
  CheckButton(IDX_SETTINGS_ALTERNATIVE_SELECTION, st.AlternativeSelection);
  // CheckButton(IDX_SETTINGS_UNDERLINE, st.Underline);

  CheckButton(IDX_SETTINGS_SHOW_SYSTEM_MENU, st.ShowSystemMenu);

  if (IsLargePageSupported())
    CheckButton(IDX_SETTINGS_LARGE_PAGES, ReadLockMemoryEnable());
  else
    EnableItem(IDX_SETTINGS_LARGE_PAGES, false);

  CheckButton(IDX_SETTINGS_WANT_ARC_HISTORY, st.ArcHistory);
  CheckButton(IDX_SETTINGS_WANT_PATH_HISTORY, st.PathHistory);
  CheckButton(IDX_SETTINGS_WANT_COPY_HISTORY, st.CopyHistory);
  CheckButton(IDX_SETTINGS_WANT_FOLDER_HISTORY, st.FolderHistory);
  CheckButton(IDX_SETTINGS_LOWERCASE_HASHES, st.LowercaseHashes);
  // EnableSubItems();

  return CPropertyPage::OnInit();
}

/*
void CSettingsPage::EnableSubItems()
{
  EnableItem(IDX_SETTINGS_UNDERLINE, IsButtonCheckedBool(IDX_SETTINGS_SINGLE_CLICK));
}
*/

LONG CSettingsPage::OnApply()
{
  if (_wasChanged)
  {
    CFmSettings st;
    st.ShowDots = IsButtonCheckedBool(IDX_SETTINGS_SHOW_DOTS);
    st.ShowRealFileIcons = IsButtonCheckedBool(IDX_SETTINGS_SHOW_REAL_FILE_ICONS);
    st.FullRow = IsButtonCheckedBool(IDX_SETTINGS_FULL_ROW);
    st.ShowGrid = IsButtonCheckedBool(IDX_SETTINGS_SHOW_GRID);
    st.SingleClick = IsButtonCheckedBool(IDX_SETTINGS_SINGLE_CLICK);
    st.AlternativeSelection = IsButtonCheckedBool(IDX_SETTINGS_ALTERNATIVE_SELECTION);
    st.ArcHistory = IsButtonCheckedBool(IDX_SETTINGS_WANT_ARC_HISTORY);
    st.PathHistory = IsButtonCheckedBool(IDX_SETTINGS_WANT_PATH_HISTORY);
    st.CopyHistory = IsButtonCheckedBool(IDX_SETTINGS_WANT_COPY_HISTORY);
    st.FolderHistory = IsButtonCheckedBool(IDX_SETTINGS_WANT_FOLDER_HISTORY);
    st.LowercaseHashes = IsButtonCheckedBool(IDX_SETTINGS_LOWERCASE_HASHES);
    // st.Underline = IsButtonCheckedBool(IDX_SETTINGS_UNDERLINE);

    st.ShowSystemMenu = IsButtonCheckedBool(IDX_SETTINGS_SHOW_SYSTEM_MENU);

    st.Save();

    _wasChanged = false;
  }

  #ifndef UNDER_CE
  if (_largePages_wasChanged)
  {
    if (IsLargePageSupported())
    {
      bool enable = IsButtonCheckedBool(IDX_SETTINGS_LARGE_PAGES);
      NSecurity::EnablePrivilege_LockMemory(enable);
      SaveLockMemoryEnable(enable);
    }
    _largePages_wasChanged = false;
  }
  #endif

  return PSNRET_NOERROR;
}

bool CSettingsPage::OnButtonClicked(int buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDX_SETTINGS_SINGLE_CLICK:
    /*
      EnableSubItems();
      break;
    */
    case IDX_SETTINGS_SHOW_DOTS:
    case IDX_SETTINGS_SHOW_SYSTEM_MENU:
    case IDX_SETTINGS_SHOW_REAL_FILE_ICONS:
    case IDX_SETTINGS_FULL_ROW:
    case IDX_SETTINGS_SHOW_GRID:
    case IDX_SETTINGS_ALTERNATIVE_SELECTION:
    case IDX_SETTINGS_WANT_ARC_HISTORY:
    case IDX_SETTINGS_WANT_PATH_HISTORY:
    case IDX_SETTINGS_WANT_COPY_HISTORY:
    case IDX_SETTINGS_WANT_FOLDER_HISTORY:
    case IDX_SETTINGS_LOWERCASE_HASHES:
      _wasChanged = true;
      break;

    case IDX_SETTINGS_LARGE_PAGES:
      _largePages_wasChanged = true;
      break;

    default:
      return CPropertyPage::OnButtonClicked(buttonID, buttonHWND);
  }

  Changed();
  return true;
}

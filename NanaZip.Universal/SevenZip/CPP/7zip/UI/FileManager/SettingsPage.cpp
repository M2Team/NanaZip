// SettingsPage.cpp

#include "StdAfx.h"

#include <CommCtrl.h>

// #include "../../../Common/IntToString.h"
// #include "../../../Common/StringConvert.h"
#include "../../../Common/StringToInt.h"

#ifndef UNDER_CE
#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/MemoryLock.h"
#include "../../../Windows/System.h"
#endif

#include "../Explorer/MyMessages.h"

#include "../Common/ZipRegistry.h"

// **************** NanaZip Modification Start ****************
// #include "HelpUtils.h"
// **************** NanaZip Modification End ****************
#include "LangUtils.h"
#include "RegistryUtils.h"
#include "SettingsPage.h"
#include "SettingsPageRes.h"

using namespace NWindows;

#ifdef Z7_LANG
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
  IDT_MEM_USAGE_EXTRACT
  // , IDT_COMPRESS_MEMORY
};
#endif

#define kSettingsTopic "FM/options.htm#settings"

extern bool IsLargePageSupported();

/*
static void AddMemSize(UString &res, UInt64 size, bool needRound = false)
{
  char c;
  unsigned moveBits = 0;
  if (needRound)
  {
    UInt64 rn = 0;
    if (size >= (1 << 31))
      rn = (1 << 28) - 1;
    UInt32 kRound = (1 << 20) - 1;
    if (rn < kRound)
      rn = kRound;
    size += rn;
    size &= ~rn;
  }
  if (size >= ((UInt64)1 << 31) && (size & 0x3FFFFFFF) == 0)
    { moveBits = 30; c = 'G'; }
  else
    { moveBits = 20; c = 'M'; }
  res.Add_UInt64(size >> moveBits);
  res.Add_Space();
  if (moveBits != 0)
    res += c;
  res += 'B';
}


int CSettingsPage::AddMemComboItem(UInt64 size, UInt64 percents, bool isDefault)
{
  UString sUser;
  UString sRegistry;
  if (size == 0)
  {
    UString s;
    s.Add_UInt64(percents);
    s += '%';
    if (isDefault)
      sUser = "* ";
    else
      sRegistry = s;
    sUser += s;
  }
  else
  {
    AddMemSize(sUser, size);
    sRegistry = sUser;
    for (;;)
    {
      int pos = sRegistry.Find(L' ');
      if (pos < 0)
        break;
      sRegistry.Delete(pos);
    }
    if (!sRegistry.IsEmpty())
      if (sRegistry.Back() == 'B')
        sRegistry.DeleteBack();
  }
  const int index = (int)_memCombo.AddString(sUser);
  _memCombo.SetItemData(index, _memLimitStrings.Size());
  _memLimitStrings.Add(sRegistry);
  return index;
}
*/

bool CSettingsPage::OnInit()
{
  _initMode = true;
  _wasChanged = false;
  _largePages_wasChanged = false;
  _memx_wasChanged = false;
  /*
  _wasChanged_MemLimit = false;
  _memLimitStrings.Clear();
  _memCombo.Attach(GetItem(IDC_SETTINGS_MEM));
  */

#ifdef Z7_LANG
  LangSetDlgItems(*this, kLangIDs, Z7_ARRAY_SIZE(kLangIDs));
#endif

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


  /*
  NCompression::CMemUse mu;
  bool needSetCur = NCompression::MemLimit_Load(mu);
  UInt64 curMemLimit;
  {
    AddMemComboItem(0, 90, true);
    _memCombo.SetCurSel(0);
  }
  if (mu.IsPercent)
  {
    const int index = AddMemComboItem(0, mu.Val);
    _memCombo.SetCurSel(index);
    needSetCur = false;
  }
  {
    _ramSize = (size_t)sizeof(size_t) << 29;
    _ramSize_Defined = NSystem::GetRamSize(_ramSize);
    UString s;
    if (_ramSize_Defined)
    {
      s += "/ ";
      AddMemSize(s, _ramSize, true);
    }
    SetItemText(IDT_SETTINGS_MEM_RAM, s);

    curMemLimit = mu.GetBytes(_ramSize);

    // size = 100 << 20; // for debug only;
    for (unsigned i = (27) * 2;; i++)
    {
      UInt64 size = (UInt64)(2 + (i & 1)) << (i / 2);
      if (i > (20 + sizeof(size_t) * 3 * 1 - 1) * 2)
        size = (UInt64)(Int64)-1;
      if (needSetCur && (size >= curMemLimit))
      {
        const int index = AddMemComboItem(curMemLimit);
        _memCombo.SetCurSel(index);
        needSetCur = false;
        if (size == curMemLimit)
          continue;
      }
      if (size == (UInt64)(Int64)-1)
        break;
      AddMemComboItem(size);
    }
  }
  */

  // EnableSubItems();


  {
    size_t ramSize = (size_t)sizeof(size_t) << 29;
    const bool ramSize_defined = NWindows::NSystem::GetRamSize(ramSize);
    // ramSize *= 10; // for debug
    UInt32 ramSize_GB = (UInt32)(((UInt64)ramSize + (1u << 29)) >> 30);
    if (ramSize_GB == 0)
      ramSize_GB = 1;
    UString s ("GB");
    if (ramSize_defined)
    {
      s += " / ";
      s.Add_UInt64(ramSize_GB);
      s += " GB (RAM)";
    }
    SetItemText(IDT_SETTINGS_MEM_GB, s);

    const UINT valMin = 1;
    UINT valMax = 64; // 64GB for RAR7
    if (ramSize_defined /* && ramSize_GB > valMax */)
    {
      const UINT k_max_val = 1u << 14;
      if (ramSize_GB >= k_max_val)
        valMax = k_max_val;
      else if (ramSize_GB > 1)
        valMax = (UINT)ramSize_GB - 1;
      else
        valMax = 1;
    }

    UInt32 limit = NExtract::Read_LimitGB();
    if (limit != 0 && limit != (UInt32)(Int32)-1)
      CheckButton(IDX_SETTINGS_MEM_SET, true);
    else
    {
      limit = 4;
      EnableSpin(false);
    }
    SendItemMessage(IDC_SETTINGS_MEM_SPIN, UDM_SETRANGE, 0, MAKELPARAM(valMax, valMin)); // Sets the controls direction
    // UDM_SETPOS doesn't set value larger than max value (valMax) of range:
    SendItemMessage(IDC_SETTINGS_MEM_SPIN, UDM_SETPOS, 0, (LPARAM)limit);
    s.Empty();
    s.Add_UInt32(limit);
    SetItemText(IDE_SETTINGS_MEM_SPIN_EDIT, s);
  }

  _initMode = false;
  return CPropertyPage::OnInit();
}


void CSettingsPage::EnableSpin(bool enable)
{
  EnableItem(IDC_SETTINGS_MEM_SPIN, enable);
  EnableItem(IDE_SETTINGS_MEM_SPIN_EDIT, enable);
}


/*
void CSettingsPage::EnableSubItems()
{
  EnableItem(IDX_SETTINGS_UNDERLINE, IsButtonCheckedBool(IDX_SETTINGS_SINGLE_CLICK));
}
*/

/*
static void AddSize_MB(UString &s, UInt64 size)
{
  s.Add_UInt64((size + (1 << 20) - 1) >> 20);
  s += " MB";
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
      const bool enable = IsButtonCheckedBool(IDX_SETTINGS_LARGE_PAGES);
      NSecurity::EnablePrivilege_LockMemory(enable);
      SaveLockMemoryEnable(enable);
    }
    _largePages_wasChanged = false;
  }
  #endif

  if (_memx_wasChanged)
  {
    UInt32 val = (UInt32)(Int32)-1;
    if (IsButtonCheckedBool(IDX_SETTINGS_MEM_SET))
    {
      UString s;
      GetItemText(IDE_SETTINGS_MEM_SPIN_EDIT, s);
      const wchar_t *end;
      val = ConvertStringToUInt32(s.Ptr(), &end);
      if (s.IsEmpty() || *end != 0 || val > (1u << 30))
      {
        // L"Incorrect value"
        ShowErrorMessage(*this, NError::MyFormatMessage(E_INVALIDARG));
        return PSNRET_INVALID;
      }
    }
    NExtract::Save_LimitGB(val);
    _memx_wasChanged = false;
  }

  /*
  if (_wasChanged_MemLimit)
  {
    const unsigned index = (int)_memCombo.GetItemData_of_CurSel();
    const UString str = _memLimitStrings[index];

    bool needSave = true;

    NCompression::CMemUse mu;

    if (_ramSize_Defined)
      mu.Parse(str);
    if (mu.IsDefined)
    {
      const UInt64 usage64 = mu.GetBytes(_ramSize);
      if (_ramSize <= usage64)
      {
        UString s2 = LangString(IDT_COMPRESS_MEMORY);
        if (s2.IsEmpty())
          GetItemText(IDT_COMPRESS_MEMORY, s2);
        UString s;

        s += "The selected value is not safe for system performance.";
        s.Add_LF();
        s += "The memory consumption for compression operation will exceed RAM size.";
        s.Add_LF();
        s.Add_LF();
        AddSize_MB(s, usage64);

        if (!s2.IsEmpty())
        {
          s += " : ";
          s += s2;
        }

        s.Add_LF();
        AddSize_MB(s, _ramSize);
        s += " : RAM";

        s.Add_LF();
        s.Add_LF();
        s += "Are you sure you want set that unsafe value for memory usage?";

        int res = MessageBoxW(*this, s, L"7-Zip", MB_YESNOCANCEL | MB_ICONQUESTION);
        if (res != IDYES)
          needSave = false;
      }
    }

    if (needSave)
    {
      NCompression::MemLimit_Save(str);
      _wasChanged_MemLimit = false;
    }
    else
      return PSNRET_INVALID_NOCHANGEPAGE;
  }
  */

  return PSNRET_NOERROR;
}

// **************** NanaZip Modification Start ****************
// Removed from NanaZip.
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
void CSettingsPage::OnNotifyHelp()
{
  ShowHelpWindow(kSettingsTopic);
}
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
// **************** NanaZip Modification End ****************

bool CSettingsPage::OnCommand(unsigned code, unsigned itemID, LPARAM param)
{
  if (!_initMode)
  {
    if (code == EN_CHANGE && itemID == IDE_SETTINGS_MEM_SPIN_EDIT)
    {
      _memx_wasChanged = true;
      Changed();
    }
    /*
    if (code == CBN_SELCHANGE)
    {
      switch (itemID)
      {
        case IDC_SETTINGS_MEM:
        {
          _wasChanged_MemLimit = true;
          Changed();
          break;
        }
      }
    }
    */
  }
  return CPropertyPage::OnCommand(code, itemID, param);
}

bool CSettingsPage::OnButtonClicked(unsigned buttonID, HWND buttonHWND)
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
      _wasChanged = true;
      break;

    case IDX_SETTINGS_LARGE_PAGES:
      _largePages_wasChanged = true;
      break;

    case IDX_SETTINGS_MEM_SET:
    {
      _memx_wasChanged = true;
      EnableSpin(IsButtonCheckedBool(IDX_SETTINGS_MEM_SET));
      break;
    }

    default:
      return CPropertyPage::OnButtonClicked(buttonID, buttonHWND);
  }

  Changed();
  return true;
}

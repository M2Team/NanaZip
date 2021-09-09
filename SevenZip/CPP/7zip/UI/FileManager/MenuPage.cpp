// MenuPage.cpp

#include "StdAfx.h"

#include "../Common/ZipRegistry.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileFind.h"

#include "../Explorer/ContextMenuFlags.h"
#include "../Explorer/RegistryContextMenu.h"
#include "../Explorer/resource.h"

#include "../FileManager/PropertyNameRes.h"

#include "../GUI/ExtractDialogRes.h"

#include "FormatUtils.h"
#include "HelpUtils.h"
#include "LangUtils.h"
#include "MenuPage.h"
#include "MenuPageRes.h"


using namespace NWindows;
using namespace NContextMenuFlags;

static const UInt32 kLangIDs[] =
{
  IDX_SYSTEM_INTEGRATE_TO_MENU,
  IDX_SYSTEM_CASCADED_MENU,
  IDX_SYSTEM_ICON_IN_MENU,
  IDX_EXTRACT_ELIM_DUP,
  IDT_SYSTEM_CONTEXT_MENU_ITEMS
};

#define kMenuTopic "fm/options.htm#sevenZip"

struct CContextMenuItem
{
  int ControlID;
  UInt32 Flag;
};

static const CContextMenuItem kMenuItems[] =
{
  { IDS_CONTEXT_OPEN, kOpen },
  { IDS_CONTEXT_OPEN, kOpenAs },
  { IDS_CONTEXT_EXTRACT, kExtract },
  { IDS_CONTEXT_EXTRACT_HERE, kExtractHere },
  { IDS_CONTEXT_EXTRACT_TO, kExtractTo },

  { IDS_CONTEXT_TEST, kTest },

  { IDS_CONTEXT_COMPRESS, kCompress },
  { IDS_CONTEXT_COMPRESS_TO, kCompressTo7z },
  { IDS_CONTEXT_COMPRESS_TO, kCompressToZip },

  #ifndef UNDER_CE
  { IDS_CONTEXT_COMPRESS_EMAIL, kCompressEmail },
  { IDS_CONTEXT_COMPRESS_TO_EMAIL, kCompressTo7zEmail },
  { IDS_CONTEXT_COMPRESS_TO_EMAIL, kCompressToZipEmail },
  #endif

  { IDS_PROP_CHECKSUM, kCRC }
};


#if !defined(_WIN64)
extern bool g_Is_Wow64;
#endif

#ifndef KEY_WOW64_64KEY
  #define KEY_WOW64_64KEY (0x0100)
#endif

#ifndef KEY_WOW64_32KEY
  #define KEY_WOW64_32KEY (0x0200)
#endif

bool CMenuPage::OnInit()
{
  _initMode = true;

  Clear_MenuChanged();
  
  LangSetDlgItems(*this, kLangIDs, ARRAY_SIZE(kLangIDs));

  #ifdef UNDER_CE

  HideItem(IDX_SYSTEM_INTEGRATE_TO_MENU);
  HideItem(IDX_SYSTEM_INTEGRATE_TO_MENU_2);

  #else

  {
    UString s;
    {
      CWindow window(GetItem(IDX_SYSTEM_INTEGRATE_TO_MENU));
      window.GetText(s);
    }
    UString bit64 = LangString(IDS_PROP_BIT64);
    if (bit64.IsEmpty())
      bit64 = "64-bit";
    #ifdef _WIN64
      bit64.Replace(L"64", L"32");
    #endif
    s.Add_Space();
    s += '(';
    s += bit64;
    s += ')';
    SetItemText(IDX_SYSTEM_INTEGRATE_TO_MENU_2, s);
  }

  const FString prefix = NDLL::GetModuleDirPrefix();
  
  _dlls[0].ctrl = IDX_SYSTEM_INTEGRATE_TO_MENU;
  _dlls[1].ctrl = IDX_SYSTEM_INTEGRATE_TO_MENU_2;
  
  _dlls[0].wow = 0;
  _dlls[1].wow =
      #ifdef _WIN64
        KEY_WOW64_32KEY
      #else
        KEY_WOW64_64KEY
      #endif
      ;

  for (unsigned d = 0; d < 2; d++)
  {
    CShellDll &dll = _dlls[d];

    dll.wasChanged = false;

    #ifndef _WIN64
    if (d != 0 && !g_Is_Wow64)
    {
      HideItem(dll.ctrl);
      continue;
    }
    #endif

    FString &path = dll.Path;
    path = prefix;
    path += (d == 0 ? "7-zip.dll" :
        #ifdef _WIN64
          "7-zip32.dll"
        #else
          "7-zip64.dll"
        #endif
        );


    if (!NFile::NFind::DoesFileExist_Raw(path))
    {
      path.Empty();
      EnableItem(dll.ctrl, false);
    }
    else
    {
      dll.prevValue = CheckContextMenuHandler(fs2us(path), dll.wow);
      CheckButton(dll.ctrl, dll.prevValue);
    }
  }

  #endif


  CContextMenuInfo ci;
  ci.Load();

  CheckButton(IDX_SYSTEM_CASCADED_MENU, ci.Cascaded.Val);
  CheckButton(IDX_SYSTEM_ICON_IN_MENU, ci.MenuIcons.Val);
  CheckButton(IDX_EXTRACT_ELIM_DUP, ci.ElimDup.Val);

  _listView.Attach(GetItem(IDL_SYSTEM_OPTIONS));

  const UInt32 newFlags = LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT;
  _listView.SetExtendedListViewStyle(newFlags, newFlags);

  _listView.InsertColumn(0, L"", 200);

  for (unsigned i = 0; i < ARRAY_SIZE(kMenuItems); i++)
  {
    const CContextMenuItem &menuItem = kMenuItems[i];

    UString s = LangString(menuItem.ControlID);
    if (menuItem.Flag == kCRC)
      s = "CRC SHA";
    if (menuItem.Flag == kOpenAs ||
        menuItem.Flag == kCRC)
      s += " >";

    switch (menuItem.ControlID)
    {
      case IDS_CONTEXT_EXTRACT_TO:
      {
        s = MyFormatNew(s, LangString(IDS_CONTEXT_FOLDER));
        break;
      }
      case IDS_CONTEXT_COMPRESS_TO:
      case IDS_CONTEXT_COMPRESS_TO_EMAIL:
      {
        UString s2 = LangString(IDS_CONTEXT_ARCHIVE);
        switch (menuItem.Flag)
        {
          case kCompressTo7z:
          case kCompressTo7zEmail:
            s2 += (".7z");
            break;
          case kCompressToZip:
          case kCompressToZipEmail:
            s2 += (".zip");
            break;
        }
        s = MyFormatNew(s, s2);
        break;
      }
    }

    int itemIndex = _listView.InsertItem(i, s);
    _listView.SetCheckState(itemIndex, ((ci.Flags & menuItem.Flag) != 0));
  }

  _listView.SetColumnWidthAuto(0);
  _initMode = false;

  return CPropertyPage::OnInit();
}


#ifndef UNDER_CE

static void ShowMenuErrorMessage(const wchar_t *m, HWND hwnd)
{
  MessageBoxW(hwnd, m, L"7-Zip", MB_ICONERROR);
}

#endif


LONG CMenuPage::OnApply()
{
  #ifndef UNDER_CE
  
  for (unsigned d = 2; d != 0;)
  {
    d--;
    CShellDll &dll = _dlls[d];
    if (dll.wasChanged && !dll.Path.IsEmpty())
    {
      bool newVal = IsButtonCheckedBool(dll.ctrl);
      LONG res = SetContextMenuHandler(newVal, fs2us(dll.Path), dll.wow);
      if (res != ERROR_SUCCESS && (dll.prevValue != newVal || newVal))
        ShowMenuErrorMessage(NError::MyFormatMessage(res), *this);
      dll.prevValue = CheckContextMenuHandler(fs2us(dll.Path), dll.wow);
      CheckButton(dll.ctrl, dll.prevValue);
      dll.wasChanged = false;
    }
  }

  #endif

  if (_cascaded_Changed || _menuIcons_Changed || _elimDup_Changed || _flags_Changed)
  {
    CContextMenuInfo ci;
    ci.Cascaded.Val = IsButtonCheckedBool(IDX_SYSTEM_CASCADED_MENU);
    ci.Cascaded.Def = _cascaded_Changed;

    ci.MenuIcons.Val = IsButtonCheckedBool(IDX_SYSTEM_ICON_IN_MENU);
    ci.MenuIcons.Def = _menuIcons_Changed;
    
    ci.ElimDup.Val = IsButtonCheckedBool(IDX_EXTRACT_ELIM_DUP);
    ci.ElimDup.Def = _elimDup_Changed;

    ci.Flags = 0;
    
    for (unsigned i = 0; i < ARRAY_SIZE(kMenuItems); i++)
      if (_listView.GetCheckState(i))
        ci.Flags |= kMenuItems[i].Flag;
    
    ci.Flags_Def = _flags_Changed;
    ci.Save();

    Clear_MenuChanged();
  }

  // UnChanged();

  return PSNRET_NOERROR;
}

void CMenuPage::OnNotifyHelp()
{
  ShowHelpWindow(kMenuTopic);
}

bool CMenuPage::OnButtonClicked(int buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    #ifndef UNDER_CE
    case IDX_SYSTEM_INTEGRATE_TO_MENU:
    case IDX_SYSTEM_INTEGRATE_TO_MENU_2:
    {
      for (unsigned d = 0; d < 2; d++)
      {
        CShellDll &dll = _dlls[d];
        if (buttonID == dll.ctrl && !dll.Path.IsEmpty())
          dll.wasChanged = true;
      }
      break;
    }
    #endif

    case IDX_SYSTEM_CASCADED_MENU: _cascaded_Changed = true; break;
    case IDX_SYSTEM_ICON_IN_MENU: _menuIcons_Changed = true; break;
    case IDX_EXTRACT_ELIM_DUP: _elimDup_Changed = true; break;
      
    default:
      return CPropertyPage::OnButtonClicked(buttonID, buttonHWND);
  }
  
  Changed();
  return true;
}

bool CMenuPage::OnNotify(UINT controlID, LPNMHDR lParam)
{
  if (lParam->hwndFrom == HWND(_listView))
  {
    switch (lParam->code)
    {
      case (LVN_ITEMCHANGED):
        return OnItemChanged((const NMLISTVIEW *)lParam);
    }
  }
  return CPropertyPage::OnNotify(controlID, lParam);
}


bool CMenuPage::OnItemChanged(const NMLISTVIEW *info)
{
  if (_initMode)
    return true;
  if ((info->uChanged & LVIF_STATE) != 0)
  {
    UINT oldState = info->uOldState & LVIS_STATEIMAGEMASK;
    UINT newState = info->uNewState & LVIS_STATEIMAGEMASK;
    if (oldState != newState)
    {
      _flags_Changed = true;
      Changed();
    }
  }
  return true;
}

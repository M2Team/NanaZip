// MenuPage.cpp

#include "StdAfx.h"

#include "../Common/ZipRegistry.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileFind.h"

#include "../Explorer/ContextMenuFlags.h"
#include "../Explorer/resource.h"

#include "../FileManager/PropertyNameRes.h"

#include "../GUI/ExtractDialogRes.h"

#include "FormatUtils.h"
#include "LangUtils.h"
#include "MenuPage.h"
#include "MenuPageRes.h"


using namespace NWindows;
using namespace NContextMenuFlags;

static const UInt32 kLangIDs[] =
{
  IDB_SYSTEM_ASSOCIATE,
  IDX_EXTRACT_ELIM_DUP,
  IDT_SYSTEM_CONTEXT_MENU_ITEMS
};

struct CContextMenuItem
{
  int ControlID;
  UInt32 Flag;
};

static const CContextMenuItem kMenuItems[] =
{
  { IDS_CONTEXT_OPEN, kOpen },
  { IDS_CONTEXT_TEST, kTest },

  { IDS_CONTEXT_EXTRACT, kExtract },
  { IDS_CONTEXT_EXTRACT_HERE, kExtractHere },
  { IDS_CONTEXT_EXTRACT_TO, kExtractTo },

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

  CContextMenuInfo ci;
  ci.Load();

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
      s = "HASH";

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

LONG CMenuPage::OnApply()
{
  if (_elimDup_Changed || _flags_Changed)
  {
    CContextMenuInfo ci;

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

bool CMenuPage::OnButtonClicked(int buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDX_EXTRACT_ELIM_DUP: _elimDup_Changed = true; break;

    case IDB_SYSTEM_ASSOCIATE:
    {
        ::ShellExecuteW(
            nullptr,
            L"open",
            L"ms-settings:defaultapps",
            nullptr,
            nullptr,
            SW_SHOWNORMAL);
        return true;
    }

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

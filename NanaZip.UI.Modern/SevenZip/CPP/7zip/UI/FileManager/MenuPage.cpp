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

#include <appmodel.h>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>

using namespace NWindows;
using namespace NContextMenuFlags;

namespace winrt
{
    using Windows::Foundation::Uri;
    using Windows::System::Launcher;
    using Windows::System::LauncherOptions;
}

static const UInt32 kLangIDs[] =
{
  IDB_SYSTEM_ASSOCIATE,
  IDX_EXTRACT_ELIM_DUP,
  IDT_SYSTEM_ZONE,
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
// **************** NanaZip Modification Start ****************
  { IDS_CONTEXT_EXTRACT_HERE_SMART, kExtractHereSmart },
// **************** NanaZip Modification End ****************
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


static void LoadLang_Spec(UString &s, UInt32 id, const char *eng)
{
  LangString(id, s);
  if (s.IsEmpty())
    s = eng;
  s.RemoveChar(L'&');
}


bool CMenuPage::OnInit()
{
  _initMode = true;

  Clear_MenuChanged();

  LangSetDlgItems(*this, kLangIDs, ARRAY_SIZE(kLangIDs));

  CContextMenuInfo ci;
  ci.Load();

  CheckButton(IDX_EXTRACT_ELIM_DUP, ci.ElimDup.Val);

  _listView.Attach(GetItem(IDL_SYSTEM_OPTIONS));
  _zoneCombo.Attach(GetItem(IDC_SYSTEM_ZONE));

  {
    unsigned wz = ci.WriteZone;
    if (wz == (UInt32)(Int32)-1)
      wz = 0;
    for (unsigned i = 0; i <= 3; i++)
    {
      unsigned val = i;
      UString s;
      if (i == 3)
      {
        if (wz < 3)
          break;
        val = wz;
      }
      else
      {
        #define MY_IDYES  406
        #define MY_IDNO   407
        if (i == 0)
          LoadLang_Spec(s, MY_IDNO, "No");
        else if (i == 1)
          LoadLang_Spec(s, MY_IDYES, "Yes");
        else
          LangString(IDT_ZONE_FOR_OFFICE, s);
      }
      if (s.IsEmpty())
        s.Add_UInt32(val);
      if (i == 0)
        s.Insert(0, L"* ");
      const int index = (int)_zoneCombo.AddString(s);
      _zoneCombo.SetItemData(index, val);
      if (val == wz)
        _zoneCombo.SetCurSel(index);
    }
  }


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
  if (_elimDup_Changed || _writeZone_Changed || _flags_Changed)
  {
    CContextMenuInfo ci;

    ci.ElimDup.Val = IsButtonCheckedBool(IDX_EXTRACT_ELIM_DUP);
    ci.ElimDup.Def = _elimDup_Changed;

    {
      int zoneIndex = (int)_zoneCombo.GetItemData_of_CurSel();
      if (zoneIndex <= 0)
        zoneIndex = -1;
      ci.WriteZone = (UInt32)(Int32)zoneIndex;
    }

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
    // case IDX_EXTRACT_WRITE_ZONE: _writeZone_Changed = true; break;

    case IDB_SYSTEM_ASSOCIATE:
    {
        try
        {
            // GetCurrentApplicationUserModelId can't work properly when the
            // program starts from Visual Studio, because NanaZip will be
            // allocated a wrong AUMID.
            UINT32 nLength = 0;
            ::GetCurrentApplicationUserModelId(
                &nLength,
                nullptr);
            std::wstring AUMID(
                nLength,
                '\0');
            winrt::check_hresult(::GetCurrentApplicationUserModelId(
                &nLength,
                AUMID.data()));
            winrt::Uri NavigateURI(
                L"ms-settings:defaultapps?registeredAUMID="
                + AUMID);
            winrt::LauncherOptions Options;
            Options.TargetApplicationPackageFamilyName(
                L"windows.immersivecontrolpanel_cw5n1h2txyewy");
            winrt::Launcher::LaunchUriAsync(
                NavigateURI,
                Options).get();
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    default:
      return CPropertyPage::OnButtonClicked(buttonID, buttonHWND);
  }

  Changed();
  return true;
}


bool CMenuPage::OnCommand(int code, int itemID, LPARAM param)
{
  if (code == CBN_SELCHANGE && itemID == IDC_SYSTEM_ZONE)
  {
    _writeZone_Changed = true;
    Changed();
    return true;
  }
  return CPropertyPage::OnCommand(code, itemID, param);
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

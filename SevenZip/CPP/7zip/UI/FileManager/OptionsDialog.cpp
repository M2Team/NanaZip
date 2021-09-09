// OptionsDialog.cpp

#include "StdAfx.h"

#include "../../../Windows/Control/Dialog.h"
#include "../../../Windows/Control/PropertyPage.h"

#include "DialogSize.h"

#include "EditPage.h"
#include "EditPageRes.h"
#include "FoldersPage.h"
#include "FoldersPageRes.h"
#include "LangPage.h"
#include "LangPageRes.h"
#include "MenuPage.h"
#include "MenuPageRes.h"
#include "SettingsPage.h"
#include "SettingsPageRes.h"
#include "SystemPage.h"
#include "SystemPageRes.h"

#include "App.h"
#include "LangUtils.h"
#include "MyLoadMenu.h"

#include "resource.h"

using namespace NWindows;

void OptionsDialog(HWND hwndOwner, HINSTANCE hInstance);
void OptionsDialog(HWND hwndOwner, HINSTANCE /* hInstance */)
{
  CSystemPage systemPage;
  CMenuPage menuPage;
  CFoldersPage foldersPage;
  CEditPage editPage;
  CSettingsPage settingsPage;
  CLangPage langPage;

  CObjectVector<NControl::CPageInfo> pages;
  BIG_DIALOG_SIZE(200, 200);

  const UINT pageIDs[] = {
      SIZED_DIALOG(IDD_SYSTEM),
      SIZED_DIALOG(IDD_MENU),
      SIZED_DIALOG(IDD_FOLDERS),
      SIZED_DIALOG(IDD_EDIT),
      SIZED_DIALOG(IDD_SETTINGS),
      SIZED_DIALOG(IDD_LANG) };

  NControl::CPropertyPage *pagePointers[] = { &systemPage,  &menuPage, &foldersPage, &editPage, &settingsPage, &langPage };
  
  for (unsigned i = 0; i < ARRAY_SIZE(pageIDs); i++)
  {
    NControl::CPageInfo &page = pages.AddNew();
    page.ID = pageIDs[i];
    LangString_OnlyFromLangFile(page.ID, page.Title);
    page.Page = pagePointers[i];
  }

  INT_PTR res = NControl::MyPropertySheet(pages, hwndOwner, LangString(IDS_OPTIONS));
  
  if (res != -1 && res != 0)
  {
    if (langPage.LangWasChanged)
    {
      // g_App._window.SetText(LangString(IDS_APP_TITLE, 0x03000000));
      MyLoadMenu();
      g_App.ReloadToolbars();
      g_App.MoveSubWindows(); // we need it to change list window aafter _toolBar.AutoSize();
      g_App.ReloadLang();
    }
  
    /*
    if (systemPage.WasChanged)
    {
      // probably it doesn't work, since image list is locked?
      g_App.SysIconsWereChanged();
    }
    */
    
    g_App.SetListSettings();
    g_App.RefreshAllPanels();
    // ::PostMessage(hwndOwner, kLangWasChangedMessage, 0 , 0);
  }
}

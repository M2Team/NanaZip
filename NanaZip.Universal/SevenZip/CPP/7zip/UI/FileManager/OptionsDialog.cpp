// OptionsDialog.cpp

#include "StdAfx.h"

#include "../../../Windows/Control/Dialog.h"
#include "../../../Windows/Control/PropertyPage.h"

#include "DialogSize.h"

#include "EditPage.h"
#include "EditPageRes.h"
#include "FoldersPage.h"
#include "FoldersPageRes.h"
// **************** NanaZip Modification Start ****************
// Removed from NanaZip.
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
#include "LangPage.h"
#include "LangPageRes.h"
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
// **************** NanaZip Modification End ****************
#include "MenuPage.h"
#include "MenuPageRes.h"
#include "SettingsPage.h"
#include "SettingsPageRes.h"
// **************** NanaZip Modification Start ****************
// Removed from NanaZip.
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
#include "SystemPage.h"
#include "SystemPageRes.h"
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
// **************** NanaZip Modification End ****************

#include "App.h"
#include "LangUtils.h"
#include "MyLoadMenu.h"

#include "resource.h"

using namespace NWindows;

void OptionsDialog(HWND hwndOwner, HINSTANCE hInstance);
void OptionsDialog(HWND hwndOwner, HINSTANCE /* hInstance */)
{
  // **************** NanaZip Modification Start ****************
  // Removed from NanaZip.
  // CSystemPage systemPage;
  // **************** NanaZip Modification End ****************
  CMenuPage menuPage;
  CFoldersPage foldersPage;
  CEditPage editPage;
  CSettingsPage settingsPage;
  // **************** NanaZip Modification Start ****************
  // Removed from NanaZip.
  // CLangPage langPage;
  // **************** NanaZip Modification End ****************

  CObjectVector<NControl::CPageInfo> pages;
  BIG_DIALOG_SIZE(200, 200);

  const UINT pageIDs[] = {
      // **************** NanaZip Modification Start ****************
      // Removed from NanaZip.
      SIZED_DIALOG(IDD_SYSTEM),
      // **************** NanaZip Modification End ****************
      SIZED_DIALOG(IDD_MENU),
      SIZED_DIALOG(IDD_FOLDERS),
      SIZED_DIALOG(IDD_EDIT),
      SIZED_DIALOG(IDD_SETTINGS),
      // **************** NanaZip Modification Start ****************
      // Removed from NanaZip.
      SIZED_DIALOG(IDD_LANG) };
      // **************** NanaZip Modification End ****************

  // **************** NanaZip Modification Start ****************
  // NControl::CPropertyPage *pagePointers[] = { &systemPage,  &menuPage, &foldersPage, &editPage, &settingsPage, &langPage };
  NControl::CPropertyPage *pagePointers[] = { &menuPage, &foldersPage, &editPage, &settingsPage, };
  // **************** NanaZip Modification End ****************

  for (unsigned i = 0; i < Z7_ARRAY_SIZE(pageIDs); i++)
  {
    NControl::CPageInfo &page = pages.AddNew();
    page.ID = pageIDs[i];
    #ifdef Z7_LANG
    LangString_OnlyFromLangFile(page.ID, page.Title);
    #endif
    page.Page = pagePointers[i];
  }

  const INT_PTR res = NControl::MyPropertySheet(pages, hwndOwner, LangString(IDS_OPTIONS));

  if (res != -1 && res != 0)
  {
    // **************** NanaZip Modification Start ****************
    // Removed from NanaZip.
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
    if (langPage.LangWasChanged)
    {
      // g_App._window.SetText(LangString(IDS_APP_TITLE, 0x03000000));
      MyLoadMenu(true); // needResetMenu
      g_App.ReloadToolbars();
      g_App.MoveSubWindows(); // we need it to change list window aafter _toolBar.AutoSize();
      g_App.ReloadLangItems();
    }
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
    // **************** NanaZip Modification End ****************

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

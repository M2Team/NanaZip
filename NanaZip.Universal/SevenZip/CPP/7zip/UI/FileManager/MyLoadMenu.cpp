// MyLoadMenu.cpp

#include "StdAfx.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/Menu.h"
#include "../../../Windows/TimeUtils.h"
#include "../../../Windows/Control/Dialog.h"

#include "../../PropID.h"

#include "../Common/CompressCall.h"

// **************** NanaZip Modification Start ****************
// #include "AboutDialog.h"
// **************** NanaZip Modification End ****************
#include "App.h"
#include "BrowseDialog2.h"
// **************** NanaZip Modification Start ****************
// #include "HelpUtils.h"
// **************** NanaZip Modification End ****************
#include "LangUtils.h"
#include "MyLoadMenu.h"
#include "RegistryUtils.h"

#include "PropertyNameRes.h"
#include "resource.h"

// **************** NanaZip Modification Start ****************
#include <NanaZip.Modern.h>
extern CCodecs* g_CodecsObj;
// **************** NanaZip Modification End ****************

using namespace NWindows;

static const UINT k_MenuID_OpenBookmark = 830;
static const UINT k_MenuID_SetBookmark = 810;
static const UINT k_MenuID_TimePopup = IDM_VIEW_TIME_POPUP;
static const UINT k_MenuID_Time = IDM_VIEW_TIME;

#if 0
// static const UINT k_MenuID_Bookmark_Temp = 850;
#endif

extern HINSTANCE g_hInstance;

#define kFMHelpTopic "FM/index.htm"

extern void OptionsDialog(HWND hwndOwner, HINSTANCE hInstance);

enum
{
  k_MenuIndex_File = 0,
  k_MenuIndex_Edit,
  k_MenuIndex_View,
  k_MenuIndex_Bookmarks
};

#ifdef Z7_LANG
static const UInt32 k_LangID_TopMenuItems[] =
{
  IDM_FILE,
  IDM_EDIT,
  IDM_VIEW,
  IDM_FAVORITES,
  IDM_TOOLS,
  IDM_HELP
};

static const UInt32 k_LangID_Toolbars = IDM_VIEW_TOOLBARS;
static const UInt32 k_LangID_AddToFavorites = IDM_ADD_TO_FAVORITES;

static const CIDLangPair kIDLangPairs[] =
{
  { IDCLOSE, 557 }, // IDM_EXIT
  { IDM_VIEW_ARANGE_BY_NAME, IDS_PROP_NAME },
  { IDM_VIEW_ARANGE_BY_TYPE, IDS_PROP_FILE_TYPE },
  { IDM_VIEW_ARANGE_BY_DATE, IDS_PROP_MTIME },
  { IDM_VIEW_ARANGE_BY_SIZE, IDS_PROP_SIZE }
};

static int FindLangItem(unsigned controlID)
{
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(kIDLangPairs); i++)
    if (kIDLangPairs[i].ControlID == controlID)
      return (int)i;
  return -1;
}
#endif

static unsigned GetSortControlID(PROPID propID)
{
  switch (propID)
  {
    case kpidName:       return IDM_VIEW_ARANGE_BY_NAME;
    case kpidExtension:  return IDM_VIEW_ARANGE_BY_TYPE;
    case kpidMTime:      return IDM_VIEW_ARANGE_BY_DATE;
    case kpidSize:       return IDM_VIEW_ARANGE_BY_SIZE;
    case kpidNoProperty: return IDM_VIEW_ARANGE_NO_SORT;
  }
  return IDM_VIEW_ARANGE_BY_NAME;
  // IDM_VIEW_ARANGE_NO_SORT;
  // return -1;
}

/*
#if _MSC_VER > 1400
// GetVersion was declared deprecated
#pragma warning(disable : 4996)
#endif

static bool g_IsNew_fMask = false;
static class CInit_fMask
{
public:
  CInit_fMask()
  {
    DWORD v = GetVersion();
    v = ((v & 0xff) << 8) | ((v >> 8) & 0xFF);
    g_IsNew_fMask = (v > 0x400); // (win98/win2000) or newer
  }
} g_Init_fMask;
static UINT Get_fMask_for_String()
  { return g_IsNew_fMask ? MIIM_STRING : MIIM_TYPE; }
static UINT Get_fMask_for_FType_and_String()
  { return g_IsNew_fMask ? (MIIM_STRING | MIIM_FTYPE) : MIIM_TYPE; }
*/

/*
We can use new MIIM_STRING / MIIM_FTYPE flags in the following conditions:
  1) we run at new Windows (win98/win2000) or newer
  2) also we probably must set MENUITEMINFO::cbSize as sizeof of full
     (MENUITEMINFO) that was compiled with (WINVER >= 0x0500)
But it's simpler to use old MIIM_TYPE without these complex checks.
*/

// /*
static inline UINT Get_fMask_for_String() { return MIIM_TYPE; }
static inline UINT Get_fMask_for_FType_and_String() { return MIIM_TYPE; }
// */

static bool Is_MenuItem_TimePopup(const CMenuItem &item)
{
  return item.wID == k_MenuID_TimePopup ||
    item.StringValue.IsPrefixedBy_Ascii_NoCase("20");
}

#ifdef Z7_LANG
static void MyChangeMenu(HMENU menuLoc, unsigned menuID, unsigned level, unsigned menuIndex)
{
  CMenu menu;
  menu.Attach(menuLoc);

  for (unsigned i = 0;; i++)
  {
    CMenuItem item;
    /* here we can use
         Get_fMask_for_String() or
         Get_fMask_for_FType_and_String()
       We want to change only String of menu item.
       It's not required to change (fType) of menu item.
       We can look (fType) to check for SEPARATOR item.
       But String of separator is empty and (wID == 0).
       So we can check for SEPARATOR without (fType) requesting.
       So it's enough to use Get_fMask_for_String() here */
    item.fMask =
        Get_fMask_for_String()
        // Get_fMask_for_FType_and_String()
        | MIIM_SUBMENU | MIIM_ID;
    if (!menu.GetItem(i, true, item))
      break;
    {
      UString newString;
      if (item.hSubMenu)
      {
        /* in win10:
           MENU+POPUP:
             (wID == item.hSubMenu)
           MENUEX+POPUP where ID is not set:
             (wID == 0)
           MENU+SEPARATOR
             (wID == 0)
        */
        UInt32 langID = item.wID;
        if (langID >= (1 << 16))
        {
          // here we try to exclude the case (wID == item.hSubMenu) if (MENU+POPUP)
          continue;
        }
        if (langID == 0)
        {
          if (level == 0)
          {
            if (i < Z7_ARRAY_SIZE(k_LangID_TopMenuItems))
              langID = k_LangID_TopMenuItems[i];
          }
          else if (level == 1)
          {
            if (menuID == IDM_FAVORITES || (menuID == 0 && menuIndex == k_MenuIndex_Bookmarks))
              langID = k_LangID_AddToFavorites;
            else if (menuID == IDM_VIEW || (menuID == 0 && menuIndex == k_MenuIndex_View))
            {
              if (Is_MenuItem_TimePopup(item))
                langID = k_MenuID_TimePopup;
              else
                langID = k_LangID_Toolbars;
            }
          }
        }
        if (langID == k_MenuID_TimePopup)
          continue;
        if (langID != k_LangID_AddToFavorites)
          MyChangeMenu(item.hSubMenu, langID, level + 1, i);
        if (langID == 0)
          continue;
        LangString_OnlyFromLangFile(langID, newString);
        if (newString.IsEmpty())
          continue;
      }
      else
      {
        if (item.fMask & (MIIM_TYPE | MIIM_FTYPE))
        if (item.IsSeparator())
          continue;
        if (item.StringValue.IsEmpty())
          continue;
        const int langPos = FindLangItem(item.wID);
        // we don't need lang change for CRC items!!!
        const UInt32 langID = langPos >= 0 ? kIDLangPairs[langPos].LangID : item.wID;
        if (langID == 0)
          continue;

        // **************** NanaZip Modification Start ****************
        // */# localization logic removed from NanaZip.
        #if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
        if (langID == IDM_OPEN_INSIDE_ONE ||
            langID == IDM_OPEN_INSIDE_PARSER)
        {
          LangString_OnlyFromLangFile(IDM_OPEN_INSIDE, newString);
          if (newString.IsEmpty())
            continue;
          newString.Replace(L"&", L"");
          const int tabPos = newString.Find(L"\t");
          if (tabPos >= 0)
            newString.DeleteFrom(tabPos);
          newString += (langID == IDM_OPEN_INSIDE_ONE ? " *" : " #");
        }
        else if (langID == IDM_BENCHMARK2)
        #endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
        if (langID == IDM_BENCHMARK2)
        // **************** NanaZip Modification End ****************
        {
          LangString_OnlyFromLangFile(IDM_BENCHMARK, newString);
          if (newString.IsEmpty())
            continue;
          newString.Replace(L"&", L"");
          const int tabPos = newString.Find(L"\t");
          if (tabPos >= 0)
            newString.DeleteFrom(tabPos);
          newString += " 2";
        }
        else
        {
          LangString_OnlyFromLangFile(langID, newString);
        }

        if (newString.IsEmpty())
          continue;

        const int tabPos = item.StringValue.ReverseFind(L'\t');
        if (tabPos >= 0)
          newString += item.StringValue.Ptr(tabPos);
      }

      {
        item.StringValue = newString;
        // we want to change only String
        item.fMask = Get_fMask_for_String();
        menu.SetItem(i, true, item);
      }
    }
  }
}
#endif

static CMenu g_FileMenu;

// **************** NanaZip Modification Start ****************
HMENU g_MoreMenu = nullptr;
// **************** NanaZip Modification End ****************

static struct CFileMenuDestroyer
{
  ~CFileMenuDestroyer() { if ((HMENU)g_FileMenu) g_FileMenu.Destroy(); }
} g_FileMenuDestroyer;


static void CopyMenu(HMENU srcMenuSpec, HMENU destMenuSpec);

static void CopyPopMenu_IfRequired(CMenuItem &item)
{
  /* if (item.hSubMenu) is defined
  {
    - it creates new (popup) menu
    - it copies menu items from old item.hSubMenu menu to new (popup) menu
    - it sets item.hSubMenu to handle of created (popup) menu
  } */
  if (item.hSubMenu)
  {
    CMenu popup;
    popup.CreatePopup();
    CopyMenu(item.hSubMenu, popup);
    item.hSubMenu = popup;
  }
}

/* destMenuSpec must be non-NULL handle to created empty popup menu */
static void CopyMenu(HMENU srcMenuSpec, HMENU destMenuSpec)
{
  CMenu srcMenu;
  srcMenu.Attach(srcMenuSpec);
  CMenu destMenu;
  destMenu.Attach(destMenuSpec);
  unsigned startPos = 0;
  for (unsigned i = 0;; i++)
  {
    CMenuItem item;
    item.fMask = MIIM_SUBMENU | MIIM_STATE | MIIM_ID | Get_fMask_for_FType_and_String();
    if (!srcMenu.GetItem(i, true, item))
      break;
    CopyPopMenu_IfRequired(item);
    if (destMenu.InsertItem(startPos, true, item))
      startPos++;
  }
}


/* use for (needResetMenu):
    false : for call from program window creation code
    true  : for another calls : (from Options language change)
*/
// **************** NanaZip Modification Start ****************
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
void MyLoadMenu(bool needResetMenu)
{
  #ifdef UNDER_CE

  const HMENU oldMenu = g_App._commandBar.GetMenu(0);
  if (oldMenu)
    ::DestroyMenu(oldMenu);
  /* BOOL b = */ g_App._commandBar.InsertMenubar(g_hInstance, IDM_MENU, 0);
  const HMENU baseMenu = g_App._commandBar.GetMenu(0);
  // if (startInit)
  // SetIdsForSubMenus(baseMenu, 0, 0);
  if (!g_LangID.IsEmpty())
    MyChangeMenu(baseMenu, 0, 0);
  g_App._commandBar.DrawMenuBar(0);

  #else // UNDER_CE

  const HWND hWnd = g_HWND;
  bool menuWasChanged = false;
  /*
     We must reload to english default menu for at least two cases:
     - if some submenu was changed (File or another submenu can be changed after menu activating).
     - for change from non-english lang to another partial non-english lang,
       where we still need some english strings.
     But we reload menu to default menu everytime except of program starting stage.
     That scheme is simpler than complex checks for exact conditions for menu reload.
  */
  if (needResetMenu)
  {
    const HMENU oldMenu = ::GetMenu(hWnd);
    const HMENU newMenu = ::LoadMenu(g_hInstance, MAKEINTRESOURCE(IDM_MENU));
    // docs for SetMenu(): the window is redrawn to reflect the menu change.
    if (newMenu && ::SetMenu(hWnd, newMenu))
      ::DestroyMenu(oldMenu);
    menuWasChanged = true;
  }
  const HMENU baseMenu = ::GetMenu(hWnd);
  // if (startInit)
  // SetIdsForSubMenus(baseMenu, 0, 0);
  #ifdef Z7_LANG
  if (!g_Lang.IsEmpty()) // !g_LangID.IsEmpty() &&
  {
    MyChangeMenu(baseMenu, 0, 0, 0);
    menuWasChanged = true;
  }
  #endif

  if (menuWasChanged)
    ::DrawMenuBar(hWnd);

  #endif // UNDER_CE

  // menuWasChanged = false; // for debug
  if (menuWasChanged || !(HMENU)g_FileMenu)
  {
    if ((HMENU)g_FileMenu)
      g_FileMenu.Destroy();
    g_FileMenu.CreatePopup();
    CopyMenu(::GetSubMenu(baseMenu, k_MenuIndex_File), g_FileMenu);
  }
}
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
void MyLoadMenu(bool /* needResetMenu */)
{
  const HMENU menu = ::LoadMenuW(g_hInstance, MAKEINTRESOURCEW(IDM_MENU));
  #ifdef Z7_LANG
  MyChangeMenu(menu, 0, 0, 0);
  #endif

  g_MoreMenu = ::CreatePopupMenu();
  CopyMenu(menu, g_MoreMenu);

  ::DestroyMenu(menu);

  if ((HMENU)g_FileMenu)
  {
    g_FileMenu.Destroy();
  }
  g_FileMenu.CreatePopup();
  CopyMenu(::GetSubMenu(g_MoreMenu, k_MenuIndex_File), g_FileMenu);
}
// **************** NanaZip Modification End ****************

void OnMenuActivating(HWND /* hWnd */, HMENU hMenu, int position)
{
  // **************** NanaZip Modification Start ****************
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
  HMENU mainMenu =
    #ifdef UNDER_CE
    g_App._commandBar.GetMenu(0);
    #else
    ::GetMenu(g_HWND)
    #endif
    ;
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
  HMENU mainMenu = g_MoreMenu;
  // **************** NanaZip Modification End ****************

  if (::GetSubMenu(mainMenu, position) != hMenu)
    return;

  if (position == k_MenuIndex_File)
  {
    CMenu menu;
    menu.Attach(hMenu);
    menu.RemoveAllItems();
    g_App.GetFocusedPanel().CreateFileMenu(hMenu);
  }
  else if (position == k_MenuIndex_Edit)
  {
    /*
    CMenu menu;
    menu.Attach(hMenu);
    menu.EnableItem(IDM_EDIT_CUT, MF_ENABLED);
    menu.EnableItem(IDM_EDIT_COPY, MF_ENABLED);
    menu.EnableItem(IDM_EDIT_PASTE, IsClipboardFormatAvailableHDROP() ? MF_ENABLED : MF_GRAYED);
    */
  }
  else if (position == k_MenuIndex_View)
  {
    // View;
    CMenu menu;
    menu.Attach(hMenu);
    menu.CheckRadioItem(
        IDM_VIEW_LARGE_ICONS, IDM_VIEW_DETAILS,
        IDM_VIEW_LARGE_ICONS + g_App.GetListViewMode(), MF_BYCOMMAND);

    menu.CheckRadioItem(
        IDM_VIEW_ARANGE_BY_NAME,
        IDM_VIEW_ARANGE_NO_SORT,
        GetSortControlID(g_App.GetSortID()),
        MF_BYCOMMAND);

    menu.CheckItemByID(IDM_VIEW_TWO_PANELS, g_App.NumPanels == 2);
    menu.CheckItemByID(IDM_VIEW_FLAT_VIEW, g_App.GetFlatMode());
    // **************** NanaZip Modification Start ****************
    // Removed from NanaZip.
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
    menu.CheckItemByID(IDM_VIEW_ARCHIVE_TOOLBAR, g_App.ShowArchiveToolbar);
    menu.CheckItemByID(IDM_VIEW_STANDARD_TOOLBAR, g_App.ShowStandardToolbar);
    menu.CheckItemByID(IDM_VIEW_TOOLBARS_LARGE_BUTTONS, g_App.LargeButtons);
    menu.CheckItemByID(IDM_VIEW_TOOLBARS_SHOW_BUTTONS_TEXT, g_App.ShowButtonsLables);
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
    // **************** NanaZip Modification End ****************
    menu.CheckItemByID(IDM_VIEW_AUTO_REFRESH, g_App.Get_AutoRefresh_Mode());
    // menu.CheckItemByID(IDM_VIEW_SHOW_STREAMS, g_App.Get_ShowNtfsStrems_Mode());
    // menu.CheckItemByID(IDM_VIEW_SHOW_DELETED, g_App.ShowDeletedFiles);

    for (unsigned i = 0;; i++)
    {
      CMenuItem item;
      item.fMask = Get_fMask_for_String() | MIIM_SUBMENU | MIIM_ID;
      item.fType = MFT_STRING;
      if (!menu.GetItem(i, true, item))
        break;
      if (item.hSubMenu && Is_MenuItem_TimePopup(item))
      {
        FILETIME ft;
        NTime::GetCurUtcFileTime(ft);

        {
          wchar_t s[64];
          s[0] = 0;
          if (ConvertUtcFileTimeToString(ft, s, kTimestampPrintLevel_DAY))
            item.StringValue = s;
        }

        item.fMask = Get_fMask_for_String() | MIIM_ID;
        item.fType = MFT_STRING;
        item.wID = k_MenuID_TimePopup;
        menu.SetItem(i, true, item);

        CMenu subMenu;
        subMenu.Attach(menu.GetSubMenu((int)i));
        subMenu.RemoveAllItems();

        const int k_TimeLevels[] =
        {
          kTimestampPrintLevel_DAY,
          kTimestampPrintLevel_MIN,
          kTimestampPrintLevel_SEC,
          // 1,2,3,4,5,6,
          kTimestampPrintLevel_NTFS,
          kTimestampPrintLevel_NS
        };

        unsigned last = k_MenuID_Time;
        unsigned selectedCommand = 0;
        g_App._timestampLevels.Clear();
        unsigned id = k_MenuID_Time;

        for (unsigned k = 0; k < Z7_ARRAY_SIZE(k_TimeLevels); k++)
        {
          wchar_t s[64];
          s[0] = 0;
          const int timestampLevel = k_TimeLevels[k];
          if (ConvertUtcFileTimeToString(ft, s, timestampLevel))
          {
            if (subMenu.AppendItem(MF_STRING, id, s))
            {
              last = id;
              g_App._timestampLevels.Add(timestampLevel);
              if (g_App.GetTimestampLevel() == timestampLevel)
                selectedCommand = id;
              id++;
            }
          }
        }
        if (selectedCommand != 0)
          menu.CheckRadioItem(k_MenuID_Time, last, selectedCommand, MF_BYCOMMAND);

        if (subMenu.AppendItem(MF_STRING, IDM_VIEW_TIME_UTC, L"UTC"))
          subMenu.CheckItemByID(IDM_VIEW_TIME_UTC, g_Timestamp_Show_UTC);
      }
    }
  }
  else if (position == k_MenuIndex_Bookmarks)
  {
    CMenu menu;
    menu.Attach(hMenu);

    CMenu subMenu;
    subMenu.Attach(menu.GetSubMenu(0));
    subMenu.RemoveAllItems();
    unsigned i;

    for (i = 0; i < 10; i++)
    {
      UString s = LangString(IDS_BOOKMARK);
      s.Add_Space();
      const char c = (char)(L'0' + i);
      s.Add_Char(c);
      s += "\tAlt+Shift+";
      s.Add_Char(c);
      subMenu.AppendItem(MF_STRING, k_MenuID_SetBookmark + i, s);
    }

    menu.RemoveAllItemsFrom(2);

    for (i = 0; i < 10; i++)
    {
      UString s = g_App.AppState.FastFolders.GetString(i);
      const int kMaxSize = 100;
      const int kFirstPartSize = kMaxSize / 2;
      if (s.Len() > kMaxSize)
      {
        s.Delete(kFirstPartSize, s.Len() - kMaxSize);
        s.Insert(kFirstPartSize, L" ... ");
      }
      if (s.IsEmpty())
        s = '-';
      s += "\tAlt+";
      s.Add_Char((char)('0' + i));
      menu.AppendItem(MF_STRING, k_MenuID_OpenBookmark + i, s);
    }
#if 0
    {
      FString tempPathF;
      if (NFile::NDir::MyGetTempPath(tempPathF))
      {
        menu.AppendItem(MF_SEPARATOR, 0, (LPCTSTR)NULL);
        UString s;
        s = "Temp : ";
        s  += fs2us(tempPathF);
        menu.AppendItem(MF_STRING, k_MenuID_Bookmark_Temp, s);
      }
    }
#endif
  }
}

/*
It doesn't help
void OnMenuUnActivating(HWND hWnd, HMENU hMenu, int id)
{
  if (::GetSubMenu(::GetMenu(g_HWND), 0) != hMenu)
    return;
}
*/

static const unsigned g_Zvc_IDs[] =
{
  IDM_VER_EDIT,
  IDM_VER_COMMIT,
  IDM_VER_REVERT,
  IDM_VER_DIFF
};

static const char * const g_Zvc_Strings[] =
{
    "Ver Edit (&1)"
  , "Ver Commit"
  , "Ver Revert"
  , "Ver Diff (&0)"
};

void CFileMenu::Load(HMENU hMenu, unsigned startPos)
{
  CMenu destMenu;
  destMenu.Attach(hMenu);

  UString diffPath;
  ReadRegDiff(diffPath);

  unsigned numRealItems = startPos;

  const bool isBigScreen = NControl::IsDialogSizeOK(40, 200, g_HWND);

  for (unsigned i = 0;; i++)
  {
    CMenuItem item;

    item.fMask = MIIM_SUBMENU | MIIM_STATE | MIIM_ID | Get_fMask_for_FType_and_String();
    item.fType = MFT_STRING;

    if (!g_FileMenu.GetItem(i, true, item))
      break;

    {
      if (!programMenu && item.wID == IDCLOSE)
        continue;

      if (item.wID == IDM_DIFF && diffPath.IsEmpty())
        continue;

      if (item.wID == IDM_OPEN_INSIDE_ONE || item.wID == IDM_OPEN_INSIDE_PARSER)
      {
        // We use diff as "super mode" marker for additional commands.
        /*
        if (diffPath.IsEmpty())
          continue;
        */
      }

      if (item.wID == IDM_BENCHMARK2)
      {
        // We use diff as "super mode" marker for additional commands.
        if (diffPath.IsEmpty())
          continue;
      }

      bool isOneFsFile = (isFsFolder && numItems == 1 && allAreFiles);
      bool disable = (!isOneFsFile && (item.wID == IDM_SPLIT || item.wID == IDM_COMBINE));

      if (readOnly)
      {
        switch (item.wID)
        {
          case IDM_RENAME:
          case IDM_MOVE_TO:
          case IDM_DELETE:
          case IDM_COMMENT:
          case IDM_CREATE_FOLDER:
          case IDM_CREATE_FILE:
            disable = true;
        }
      }

      if (isHashFolder)
      {
        switch (item.wID)
        {
          case IDM_OPEN:
          case IDM_OPEN_INSIDE:
          case IDM_OPEN_INSIDE_ONE:
          case IDM_OPEN_INSIDE_PARSER:
          case IDM_OPEN_OUTSIDE:
          case IDM_FILE_VIEW:
          case IDM_FILE_EDIT:
          // case IDM_RENAME:
          case IDM_COPY_TO:
          case IDM_MOVE_TO:
          // case IDM_DELETE:
          case IDM_COMMENT:
          case IDM_CREATE_FOLDER:
          case IDM_CREATE_FILE:
          case IDM_LINK:
          case IDM_DIFF:
            disable = true;
        }
      }


      if (item.wID == IDM_LINK && numItems != 1)
        disable = true;

      if (item.wID == IDM_ALT_STREAMS)
        disable = !isAltStreamsSupported;

      if (!isBigScreen && (disable || item.IsSeparator()))
        continue;

      CopyPopMenu_IfRequired(item);
      if (destMenu.InsertItem(startPos, true, item))
      {
        if (disable)
          destMenu.EnableItem(startPos, MF_BYPOSITION | MF_GRAYED);
        startPos++;
      }

      if (!item.IsSeparator())
        numRealItems = startPos;
    }
  }

  UString vercPath;
  if (!diffPath.IsEmpty() && isFsFolder && allAreFiles && numItems == 1)
    ReadReg_VerCtrlPath(vercPath);

  if (!vercPath.IsEmpty())
  {
    NFile::NFind::CFileInfo fi;
    if (fi.Find(FilePath) && fi.Size < ((UInt32)1 << 31) && !fi.IsDir())
    {
      for (unsigned k = 0; k < Z7_ARRAY_SIZE(g_Zvc_IDs); k++)
      {
        const unsigned id = g_Zvc_IDs[k];
        if (fi.IsReadOnly())
        {
          if (id == IDM_VER_COMMIT ||
              id == IDM_VER_REVERT ||
              id == IDM_VER_DIFF)
            continue;
        }
        else
        {
          if (id == IDM_VER_EDIT)
            continue;
        }

        CMenuItem item;
        UString s (g_Zvc_Strings[k]);
        if (destMenu.AppendItem(MF_STRING, id, s))
        {
          startPos++;
          numRealItems = startPos;
        }
      }
    }
  }

  destMenu.RemoveAllItemsFrom(numRealItems);
}

bool ExecuteFileCommand(unsigned id)
{
  if (id >= kMenuCmdID_Plugin_Start)
  {
    g_App.GetFocusedPanel().InvokePluginCommand(id);
    g_App.GetFocusedPanel()._sevenZipContextMenu.Release();
    g_App.GetFocusedPanel()._systemContextMenu.Release();
    return true;
  }

  switch (id)
  {
    // File
    case IDM_OPEN: g_App.OpenItem(); break;

    case IDM_OPEN_INSIDE:        g_App.OpenItemInside(NULL); break;
    case IDM_OPEN_INSIDE_ONE:    g_App.OpenItemInside(L"*"); break;
    case IDM_OPEN_INSIDE_PARSER: g_App.OpenItemInside(L"#"); break;

    case IDM_OPEN_OUTSIDE: g_App.OpenItemOutside(); break;
    case IDM_FILE_VIEW: g_App.EditItem(false); break;
    case IDM_FILE_EDIT: g_App.EditItem(true); break;
    case IDM_RENAME: g_App.Rename(); break;
    case IDM_COPY_TO: g_App.CopyTo(); break;
    case IDM_MOVE_TO: g_App.MoveTo(); break;
    case IDM_DELETE: g_App.Delete(!IsKeyDown(VK_SHIFT)); break;

    case IDM_HASH_ALL: g_App.CalculateCrc("*"); break;
    case IDM_CRC32: g_App.CalculateCrc("CRC32"); break;
    case IDM_CRC64: g_App.CalculateCrc("CRC64"); break;
    case IDM_XXH64: g_App.CalculateCrc("XXH64"); break;
    case IDM_MD5: g_App.CalculateCrc("MD5"); break;
    case IDM_SHA1: g_App.CalculateCrc("SHA1"); break;
    case IDM_SHA256: g_App.CalculateCrc("SHA256"); break;
    case IDM_SHA384: g_App.CalculateCrc("SHA384"); break;
    case IDM_SHA512: g_App.CalculateCrc("SHA512"); break;
    case IDM_SHA3_256: g_App.CalculateCrc("SHA3-256"); break;
    case IDM_BLAKE2SP: g_App.CalculateCrc("BLAKE2sp"); break;

    case IDM_DIFF: g_App.DiffFiles(); break;

    case IDM_VER_EDIT:
    case IDM_VER_COMMIT:
    case IDM_VER_REVERT:
    case IDM_VER_DIFF:
        g_App.VerCtrl(id); break;

    case IDM_SPLIT: g_App.Split(); break;
    case IDM_COMBINE: g_App.Combine(); break;
    case IDM_PROPERTIES: g_App.Properties(); break;
    case IDM_COMMENT: g_App.Comment(); break;
    case IDM_CREATE_FOLDER: g_App.CreateFolder(); break;
    case IDM_CREATE_FILE: g_App.CreateFile(); break;
    #ifndef UNDER_CE
    case IDM_LINK: g_App.Link(); break;
    case IDM_ALT_STREAMS: g_App.OpenAltStreams(); break;
    #endif
    default: return false;
  }
  return true;
}

static void MyBenchmark(bool totalMode)
{
  CPanel::CDisableTimerProcessing disableTimerProcessing1(g_App.Panels[0]);
  CPanel::CDisableTimerProcessing disableTimerProcessing2(g_App.Panels[1]);
  Benchmark(totalMode);
}

bool OnMenuCommand(HWND hWnd, unsigned id)
{
  if (ExecuteFileCommand(id))
    return true;

  switch (id)
  {
    // File
    case IDCLOSE:
      // SendMessage(hWnd, WM_ACTIVATE, MAKEWPARAM(WA_INACTIVE, 0), (LPARAM)hWnd);
      // g_ExitEventLauncher.Exit(false);
      SendMessage(hWnd, WM_CLOSE, 0, 0);
      break;

    // Edit
    /*
    case IDM_EDIT_CUT:
      g_App.EditCut();
      break;
    case IDM_EDIT_COPY:
      g_App.EditCopy();
      break;
    case IDM_EDIT_PASTE:
      g_App.EditPaste();
      break;
    */
    case IDM_SELECT_ALL:
      g_App.SelectAll(true);
      g_App.Refresh_StatusBar();
      break;
    case IDM_DESELECT_ALL:
      g_App.SelectAll(false);
      g_App.Refresh_StatusBar();
      break;
    case IDM_INVERT_SELECTION:
      g_App.InvertSelection();
      g_App.Refresh_StatusBar();
      break;
    case IDM_SELECT:
      g_App.SelectSpec(true);
      g_App.Refresh_StatusBar();
      break;
    case IDM_DESELECT:
      g_App.SelectSpec(false);
      g_App.Refresh_StatusBar();
      break;
    case IDM_SELECT_BY_TYPE:
      g_App.SelectByType(true);
      g_App.Refresh_StatusBar();
      break;
    case IDM_DESELECT_BY_TYPE:
      g_App.SelectByType(false);
      g_App.Refresh_StatusBar();
      break;

    //View
    case IDM_VIEW_LARGE_ICONS:
    case IDM_VIEW_SMALL_ICONS:
    case IDM_VIEW_LIST:
    case IDM_VIEW_DETAILS:
    {
      UINT index = id - IDM_VIEW_LARGE_ICONS;
      if (index < 4)
      {
        g_App.SetListViewMode(index);
        /*
        CMenu menu;
        menu.Attach(::GetSubMenu(::GetMenu(hWnd), k_MenuIndex_View));
        menu.CheckRadioItem(IDM_VIEW_LARGE_ICONS, IDM_VIEW_DETAILS,
            id, MF_BYCOMMAND);
        */
      }
      break;
    }
    case IDM_VIEW_ARANGE_BY_NAME: g_App.SortItemsWithPropID(kpidName); break;
    case IDM_VIEW_ARANGE_BY_TYPE: g_App.SortItemsWithPropID(kpidExtension); break;
    case IDM_VIEW_ARANGE_BY_DATE: g_App.SortItemsWithPropID(kpidMTime); break;
    case IDM_VIEW_ARANGE_BY_SIZE: g_App.SortItemsWithPropID(kpidSize); break;
    case IDM_VIEW_ARANGE_NO_SORT: g_App.SortItemsWithPropID(kpidNoProperty); break;

    case IDM_OPEN_ROOT_FOLDER:    g_App.OpenRootFolder(); break;
    case IDM_OPEN_PARENT_FOLDER:  g_App.OpenParentFolder(); break;
    case IDM_FOLDERS_HISTORY:     g_App.FoldersHistory(); break;
    case IDM_VIEW_FLAT_VIEW:      g_App.ChangeFlatMode(); break;
    case IDM_VIEW_REFRESH:        g_App.RefreshView(); break;
    case IDM_VIEW_AUTO_REFRESH:   g_App.Change_AutoRefresh_Mode(); break;

    // case IDM_VIEW_SHOW_STREAMS:     g_App.Change_ShowNtfsStrems_Mode(); break;
    /*
    case IDM_VIEW_SHOW_DELETED:
    {
      g_App.Change_ShowDeleted();
      bool isChecked = g_App.ShowDeletedFiles;
      Save_ShowDeleted(isChecked);
    }
    */

    case IDM_VIEW_TWO_PANELS:       g_App.SwitchOnOffOnePanel(); break;
    // **************** NanaZip Modification Start ****************
    // Removed from NanaZip.
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
    case IDM_VIEW_STANDARD_TOOLBAR: g_App.SwitchStandardToolbar(); break;
    case IDM_VIEW_ARCHIVE_TOOLBAR:  g_App.SwitchArchiveToolbar(); break;

    case IDM_VIEW_TOOLBARS_SHOW_BUTTONS_TEXT: g_App.SwitchButtonsLables(); break;
    case IDM_VIEW_TOOLBARS_LARGE_BUTTONS:     g_App.SwitchLargeButtons(); break;
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
    // **************** NanaZip Modification End ****************

    case IDM_VIEW_TIME_UTC:
      g_Timestamp_Show_UTC = !g_Timestamp_Show_UTC;
      g_App.RedrawListItems_InPanels();
      break;

    // Tools
    case IDM_OPTIONS: OptionsDialog(hWnd, g_hInstance); break;

    case IDM_BENCHMARK: MyBenchmark(false); break;
    case IDM_BENCHMARK2: MyBenchmark(true); break;

    // **************** NanaZip Modification Start ****************
    // Removed from NanaZip.
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
    // Help
    case IDM_HELP_CONTENTS:
      ShowHelpWindow(kFMHelpTopic);
      break;
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
    // **************** NanaZip Modification End ****************
    case IDM_ABOUT:
    {
      // **************** NanaZip Modification Start ****************
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
      CAboutDialog dialog;
      dialog.Create(hWnd);
      break;
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
      UString ExtendedMessage;
      if (g_CodecsObj)
      {
          g_CodecsObj->GetCodecsErrorMessage(ExtendedMessage);
      }
      ::K7ModernShowAboutDialog(hWnd, ExtendedMessage);
      break;
      // **************** NanaZip Modification End ****************
    }

    case IDM_TEMP_DIR:
    {
      /*
      CPanel &panel = g_App.GetFocusedPanel();
      FString tempPathF;
      if (NFile::NDir::MyGetTempPath(tempPathF))
        panel.BindToPathAndRefresh(tempPathF);
      */
      MyBrowseForTempFolder(g_HWND);
      break;
    }

    default:
    {
      if (id >= k_MenuID_OpenBookmark && id <= k_MenuID_OpenBookmark + 9)
      {
        g_App.OpenBookmark(id - k_MenuID_OpenBookmark);
        return true;
      }
      else if (id >= k_MenuID_SetBookmark && id <= k_MenuID_SetBookmark + 9)
      {
        g_App.SetBookmark(id - k_MenuID_SetBookmark);
        return true;
      }
      else if (id >= k_MenuID_Time && (unsigned)id < k_MenuID_Time + g_App._timestampLevels.Size())
      {
        g_App.SetTimestampLevel(g_App._timestampLevels[id - k_MenuID_Time]);
        return true;
      }
      return false;
    }
  }
  return true;
}

// MyLoadMenu.cpp

#include "StdAfx.h"

#include "../../../Windows/Menu.h"
#include "../../../Windows/TimeUtils.h"
#include "../../../Windows/Control/Dialog.h"

#include "../../PropID.h"

#include "../Common/CompressCall.h"

#include "AboutDialog.h"
#include "App.h"
#include "HelpUtils.h"
#include "LangUtils.h"
#include "MyLoadMenu.h"
#include "RegistryUtils.h"

#include "resource.h"

using namespace NWindows;

static const UINT kOpenBookmarkMenuID = 830;
static const UINT kSetBookmarkMenuID = 810;
static const UINT kMenuID_Time_Parent = 760;
static const UINT kMenuID_Time = 761;

extern HINSTANCE g_hInstance;

#define kFMHelpTopic "FM/index.htm"

extern void OptionsDialog(HWND hwndOwner, HINSTANCE hInstance);

enum
{
  kMenuIndex_File = 0,
  kMenuIndex_Edit,
  kMenuIndex_View,
  kMenuIndex_Bookmarks
};

static const UInt32 kTopMenuLangIDs[] = { 500, 501, 502, 503, 504, 505 };

static const UInt32 kAddToFavoritesLangID = 800;
static const UInt32 kToolbarsLangID = 733;

static const CIDLangPair kIDLangPairs[] =
{
  { IDCLOSE, 557 },
  { IDM_VIEW_ARANGE_BY_NAME, 1004 },
  { IDM_VIEW_ARANGE_BY_TYPE, 1020 },
  { IDM_VIEW_ARANGE_BY_DATE, 1012 },
  { IDM_VIEW_ARANGE_BY_SIZE, 1007 }
};

static int FindLangItem(unsigned controlID)
{
  for (unsigned i = 0; i < ARRAY_SIZE(kIDLangPairs); i++)
    if (kIDLangPairs[i].ControlID == controlID)
      return i;
  return -1;
}

static int GetSortControlID(PROPID propID)
{
  switch (propID)
  {
    case kpidName:       return IDM_VIEW_ARANGE_BY_NAME;
    case kpidExtension:  return IDM_VIEW_ARANGE_BY_TYPE;
    case kpidMTime:      return IDM_VIEW_ARANGE_BY_DATE;
    case kpidSize:       return IDM_VIEW_ARANGE_BY_SIZE;
    case kpidNoProperty: return IDM_VIEW_ARANGE_NO_SORT;
  }
  return -1;
}

/*
static bool g_IsNew_fMask = true;

class CInit_fMask
{
public:
  CInit_fMask()
  {
    g_IsNew_fMask = false;
    OSVERSIONINFO vi;
    vi.dwOSVersionInfoSize = sizeof(vi);
    if (::GetVersionEx(&vi))
    {
      g_IsNew_fMask = (vi.dwMajorVersion > 4 ||
        (vi.dwMajorVersion == 4 && vi.dwMinorVersion > 0));
    }
    g_IsNew_fMask = false;
  }
} g_Init_fMask;

// it's hack for supporting Windows NT
// constants are from WinUser.h

#if (WINVER < 0x0500)
#define MIIM_STRING      0x00000040
#define MIIM_BITMAP      0x00000080
#define MIIM_FTYPE       0x00000100
#endif

static UINT Get_fMask_for_String()
{
  return g_IsNew_fMask ? MIIM_STRING : MIIM_TYPE;
}

static UINT Get_fMask_for_FType_and_String()
{
  return g_IsNew_fMask ? (MIIM_STRING | MIIM_FTYPE) : MIIM_TYPE;
}
*/

static inline UINT Get_fMask_for_String() { return MIIM_TYPE; }
static inline UINT Get_fMask_for_FType_and_String() { return MIIM_TYPE; }


static void MyChangeMenu(HMENU menuLoc, int level, int menuIndex)
{
  CMenu menu;
  menu.Attach(menuLoc);
  
  for (unsigned i = 0;; i++)
  {
    CMenuItem item;
    item.fMask = Get_fMask_for_String() | MIIM_SUBMENU | MIIM_ID;
    item.fType = MFT_STRING;
    if (!menu.GetItem(i, true, item))
      break;
    {
      UString newString;
      if (item.hSubMenu)
      {
        UInt32 langID = 0;
        if (level == 1 && menuIndex == kMenuIndex_Bookmarks)
          langID = kAddToFavoritesLangID;
        else
        {
          MyChangeMenu(item.hSubMenu, level + 1, i);
          if (level == 1 && menuIndex == kMenuIndex_View)
          {
            if (item.wID == kMenuID_Time_Parent || item.StringValue.IsPrefixedBy_Ascii_NoCase("20"))
              continue;
            else
              langID = kToolbarsLangID;
          }
          else if (level == 0 && i < ARRAY_SIZE(kTopMenuLangIDs))
            langID = kTopMenuLangIDs[i];
          else
            continue;
        }
        
        LangString_OnlyFromLangFile(langID, newString);
        
        if (newString.IsEmpty())
          continue;
      }
      else
      {
        if (item.IsSeparator())
          continue;
        int langPos = FindLangItem(item.wID);

        // we don't need lang change for CRC items!!!

        UInt32 langID = langPos >= 0 ? kIDLangPairs[langPos].LangID : item.wID;
        
        if (langID == IDM_OPEN_INSIDE_ONE || langID == IDM_OPEN_INSIDE_PARSER)
        {
          LangString_OnlyFromLangFile(IDM_OPEN_INSIDE, newString);
          if (newString.IsEmpty())
            continue;
          newString.Replace(L"&", L"");
          int tabPos = newString.Find(L"\t");
          if (tabPos >= 0)
            newString.DeleteFrom(tabPos);
          newString += (langID == IDM_OPEN_INSIDE_ONE ? " *" : " #");
        }
        else if (langID == IDM_BENCHMARK2)
        {
          LangString_OnlyFromLangFile(IDM_BENCHMARK, newString);
          if (newString.IsEmpty())
            continue;
          newString.Replace(L"&", L"");
          int tabPos = newString.Find(L"\t");
          if (tabPos >= 0)
            newString.DeleteFrom(tabPos);
          newString += " 2";
        }
        else
          LangString_OnlyFromLangFile(langID, newString);

        if (newString.IsEmpty())
          continue;

        int tabPos = item.StringValue.ReverseFind(L'\t');
        if (tabPos >= 0)
          newString += item.StringValue.Ptr(tabPos);
      }

      {
        item.StringValue = newString;
        item.fMask = Get_fMask_for_String();
        item.fType = MFT_STRING;
        menu.SetItem(i, true, item);
      }
    }
  }
}

static CMenu g_FileMenu;

static struct CFileMenuDestroyer
{
  ~CFileMenuDestroyer() { if ((HMENU)g_FileMenu != 0) g_FileMenu.Destroy(); }
} g_FileMenuDestroyer;


static void CopyMenu(HMENU srcMenuSpec, HMENU destMenuSpec);

static void CopyPopMenu_IfRequired(CMenuItem &item)
{
  if (item.hSubMenu)
  {
    CMenu popup;
    popup.CreatePopup();
    CopyMenu(item.hSubMenu, popup);
    item.hSubMenu = popup;
  }
}

static void CopyMenu(HMENU srcMenuSpec, HMENU destMenuSpec)
{
  CMenu srcMenu;
  srcMenu.Attach(srcMenuSpec);
  CMenu destMenu;
  destMenu.Attach(destMenuSpec);
  int startPos = 0;
  for (int i = 0;; i++)
  {
    CMenuItem item;
    item.fMask = MIIM_SUBMENU | MIIM_STATE | MIIM_ID | Get_fMask_for_FType_and_String();
    item.fType = MFT_STRING;

    if (!srcMenu.GetItem(i, true, item))
      break;

    CopyPopMenu_IfRequired(item);
    if (destMenu.InsertItem(startPos, true, item))
      startPos++;
  }
}

void MyLoadMenu()
{
  HMENU baseMenu;

  #ifdef UNDER_CE

  HMENU oldMenu = g_App._commandBar.GetMenu(0);
  if (oldMenu)
    ::DestroyMenu(oldMenu);
  /* BOOL b = */ g_App._commandBar.InsertMenubar(g_hInstance, IDM_MENU, 0);
  baseMenu = g_App._commandBar.GetMenu(0);
  // if (startInit)
  // SetIdsForSubMenes(baseMenu, 0, 0);
  if (!g_LangID.IsEmpty())
    MyChangeMenu(baseMenu, 0, 0);
  g_App._commandBar.DrawMenuBar(0);
 
  #else

  HWND hWnd = g_HWND;
  HMENU oldMenu = ::GetMenu(hWnd);
  ::SetMenu(hWnd, ::LoadMenu(g_hInstance, MAKEINTRESOURCE(IDM_MENU)));
  ::DestroyMenu(oldMenu);
  baseMenu = ::GetMenu(hWnd);
  // if (startInit)
  // SetIdsForSubMenes(baseMenu, 0, 0);
  if (!g_LangID.IsEmpty())
    MyChangeMenu(baseMenu, 0, 0);
  ::DrawMenuBar(hWnd);

  #endif

  if ((HMENU)g_FileMenu != 0)
    g_FileMenu.Destroy();
  g_FileMenu.CreatePopup();
  CopyMenu(::GetSubMenu(baseMenu, 0), g_FileMenu);
}

void OnMenuActivating(HWND /* hWnd */, HMENU hMenu, int position)
{
  HMENU mainMenu =
    #ifdef UNDER_CE
    g_App._commandBar.GetMenu(0);
    #else
    ::GetMenu(g_HWND)
    #endif
    ;
  
  if (::GetSubMenu(mainMenu, position) != hMenu)
    return;
  
  if (position == kMenuIndex_File)
  {
    CMenu menu;
    menu.Attach(hMenu);
    menu.RemoveAllItems();
    g_App.GetFocusedPanel().CreateFileMenu(hMenu);
  }
  else if (position == kMenuIndex_Edit)
  {
    /*
    CMenu menu;
    menu.Attach(hMenu);
    menu.EnableItem(IDM_EDIT_CUT, MF_ENABLED);
    menu.EnableItem(IDM_EDIT_COPY, MF_ENABLED);
    menu.EnableItem(IDM_EDIT_PASTE, IsClipboardFormatAvailableHDROP() ? MF_ENABLED : MF_GRAYED);
    */
  }
  else if (position == kMenuIndex_View)
  {
    // View;
    CMenu menu;
    menu.Attach(hMenu);
    menu.CheckRadioItem(IDM_VIEW_LARGE_ICONS, IDM_VIEW_DETAILS,
      IDM_VIEW_LARGE_ICONS + g_App.GetListViewMode(), MF_BYCOMMAND);

    menu.CheckRadioItem(IDM_VIEW_ARANGE_BY_NAME, IDM_VIEW_ARANGE_NO_SORT,
        GetSortControlID(g_App.GetSortID()), MF_BYCOMMAND);
    
    menu.CheckItemByID(IDM_VIEW_TWO_PANELS, g_App.NumPanels == 2);
    menu.CheckItemByID(IDM_VIEW_FLAT_VIEW, g_App.GetFlatMode());
    menu.CheckItemByID(IDM_VIEW_ARCHIVE_TOOLBAR, g_App.ShowArchiveToolbar);
    menu.CheckItemByID(IDM_VIEW_STANDARD_TOOLBAR, g_App.ShowStandardToolbar);
    menu.CheckItemByID(IDM_VIEW_TOOLBARS_LARGE_BUTTONS, g_App.LargeButtons);
    menu.CheckItemByID(IDM_VIEW_TOOLBARS_SHOW_BUTTONS_TEXT, g_App.ShowButtonsLables);
    menu.CheckItemByID(IDM_VIEW_AUTO_REFRESH, g_App.Get_AutoRefresh_Mode());
    // menu.CheckItemByID(IDM_VIEW_SHOW_STREAMS, g_App.Get_ShowNtfsStrems_Mode());
    // menu.CheckItemByID(IDM_VIEW_SHOW_DELETED, g_App.ShowDeletedFiles);

    for (int i = 0;; i++)
    {
      CMenuItem item;
      item.fMask = Get_fMask_for_String() | MIIM_SUBMENU | MIIM_ID;
      item.fType = MFT_STRING;
      if (!menu.GetItem(i, true, item))
        break;
      if (item.hSubMenu && (item.wID == kMenuID_Time_Parent
          || item.StringValue.IsPrefixedBy_Ascii_NoCase("20")
          ))
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
        item.wID = kMenuID_Time_Parent;
        menu.SetItem(i, true, item);

        CMenu subMenu;
        subMenu.Attach(menu.GetSubMenu(i));
        subMenu.RemoveAllItems();
        
        const int k_TimeLevels[] =
        {
          kTimestampPrintLevel_DAY,
          kTimestampPrintLevel_MIN,
          kTimestampPrintLevel_SEC,
          // 1,2,3,4,5,6,
          kTimestampPrintLevel_NTFS
        };

        unsigned last = kMenuID_Time;
        unsigned selectedCommand = 0;
        g_App._timestampLevels.Clear();
        unsigned id = kMenuID_Time;
        
        for (unsigned k = 0; k < ARRAY_SIZE(k_TimeLevels); k++)
        {
          wchar_t s[64];
          s[0] = 0;
          int timestampLevel = k_TimeLevels[k];
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
          menu.CheckRadioItem(kMenuID_Time, last, selectedCommand, MF_BYCOMMAND);
      }
    }
  }
  else if (position == kMenuIndex_Bookmarks)
  {
    CMenu menu;
    menu.Attach(hMenu);

    CMenu subMenu;
    subMenu.Attach(menu.GetSubMenu(0));
    subMenu.RemoveAllItems();
    int i;
    
    for (i = 0; i < 10; i++)
    {
      UString s = LangString(IDS_BOOKMARK);
      s.Add_Space();
      char c = (char)(L'0' + i);
      s += c;
      s += "\tAlt+Shift+";
      s += c;
      subMenu.AppendItem(MF_STRING, kSetBookmarkMenuID + i, s);
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
      s += (char)('0' + i);
      menu.AppendItem(MF_STRING, kOpenBookmarkMenuID + i, s);
    }
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

      if (item.wID == IDM_LINK && numItems != 1)
        disable = true;

      if (item.wID == IDM_ALT_STREAMS)
        disable = !isAltStreamsSupported;

      bool isBigScreen = NControl::IsDialogSizeOK(40, 200);

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
      for (unsigned k = 0; k < ARRAY_SIZE(g_Zvc_IDs); k++)
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
    case IDM_SHA1: g_App.CalculateCrc("SHA1"); break;
    case IDM_SHA256: g_App.CalculateCrc("SHA256"); break;
    
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
      SendMessage(hWnd, WM_ACTIVATE, MAKEWPARAM(WA_INACTIVE, 0), (LPARAM)hWnd);
      g_ExitEventLauncher.Exit(false);
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
        menu.Attach(::GetSubMenu(::GetMenu(hWnd), kMenuIndex_View));
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
    case IDM_VIEW_STANDARD_TOOLBAR: g_App.SwitchStandardToolbar(); break;
    case IDM_VIEW_ARCHIVE_TOOLBAR:  g_App.SwitchArchiveToolbar(); break;

    case IDM_VIEW_TOOLBARS_SHOW_BUTTONS_TEXT: g_App.SwitchButtonsLables(); break;
    case IDM_VIEW_TOOLBARS_LARGE_BUTTONS:     g_App.SwitchLargeButtons(); break;

    // Tools
    case IDM_OPTIONS: OptionsDialog(hWnd, g_hInstance); break;
          
    case IDM_BENCHMARK: MyBenchmark(false); break;
    case IDM_BENCHMARK2: MyBenchmark(true); break;

    // Help
    case IDM_HELP_CONTENTS:
      ShowHelpWindow(kFMHelpTopic);
      break;
    case IDM_ABOUT:
    {
      CAboutDialog dialog;
      dialog.Create(hWnd);
      break;
    }
    default:
    {
      if (id >= kOpenBookmarkMenuID && id <= kOpenBookmarkMenuID + 9)
      {
        g_App.OpenBookmark(id - kOpenBookmarkMenuID);
        return true;
      }
      else if (id >= kSetBookmarkMenuID && id <= kSetBookmarkMenuID + 9)
      {
        g_App.SetBookmark(id - kSetBookmarkMenuID);
        return true;
      }
      else if (id >= kMenuID_Time && (unsigned)id <= kMenuID_Time + g_App._timestampLevels.Size())
      {
        unsigned index = id - kMenuID_Time;
        g_App.SetTimestampLevel(g_App._timestampLevels[index]);
        return true;
      }
      return false;
    }
  }
  return true;
}

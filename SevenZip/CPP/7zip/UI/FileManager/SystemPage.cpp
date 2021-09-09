// SystemPage.cpp

#include "StdAfx.h"

#include "../../../Common/MyWindows.h"

#include <ShlObj.h>

#include "../../../Common/Defs.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/ErrorMsg.h"

#include "HelpUtils.h"
#include "IFolder.h"
#include "LangUtils.h"
#include "PropertyNameRes.h"
#include "SystemPage.h"
#include "SystemPageRes.h"

using namespace NWindows;

#ifndef _UNICODE
extern bool g_IsNT;
#endif

static const UInt32 kLangIDs[] =
{
  IDT_SYSTEM_ASSOCIATE
};

#define kSystemTopic "FM/options.htm#system"

CSysString CModifiedExtInfo::GetString() const
{
  const char *s;
  if (State == kExtState_7Zip)
    s = "7-Zip";
  else if (State == kExtState_Clear)
    s = "";
  else if (Other7Zip)
    s = "[7-Zip]";
  else
    return ProgramKey;
  return CSysString (s);
};


int CSystemPage::AddIcon(const UString &iconPath, int iconIndex)
{
  if (iconPath.IsEmpty())
    return -1;
  if (iconIndex == -1)
    iconIndex = 0;
  
  HICON hicon;
  
  #ifdef UNDER_CE
  ExtractIconExW(iconPath, iconIndex, NULL, &hicon, 1);
  if (!hicon)
  #else
  // we expand path from REG_EXPAND_SZ registry item.
  UString path;
  DWORD size = MAX_PATH + 10;
  DWORD needLen = ::ExpandEnvironmentStringsW(iconPath, path.GetBuf(size + 2), size);
  path.ReleaseBuf_CalcLen(size);
  if (needLen == 0 || needLen >= size)
    path = iconPath;
  int num = ExtractIconExW(path, iconIndex, NULL, &hicon, 1);
  if (num != 1 || !hicon)
  #endif
    return -1;
  
  _imageList.AddIcon(hicon);
  DestroyIcon(hicon);
  return _numIcons++;
}


void CSystemPage::RefreshListItem(unsigned group, unsigned listIndex)
{
  const CAssoc &assoc = _items[GetRealIndex(listIndex)];
  _listView.SetSubItem(listIndex, group + 1, assoc.Pair[group].GetString());
  LVITEMW newItem;
  memset(&newItem, 0, sizeof(newItem));
  newItem.iItem = listIndex;
  newItem.mask = LVIF_IMAGE;
  newItem.iImage = assoc.GetIconIndex();
  _listView.SetItem(&newItem);
}


void CSystemPage::ChangeState(unsigned group, const CUIntVector &indices)
{
  if (indices.IsEmpty())
    return;

  bool thereAreClearItems = false;
  unsigned counters[3] = { 0, 0, 0 };
  
  unsigned i;
  for (i = 0; i < indices.Size(); i++)
  {
    const CModifiedExtInfo &mi = _items[GetRealIndex(indices[i])].Pair[group];
    int state = kExtState_7Zip;
    if (mi.State == kExtState_7Zip)
      state = kExtState_Clear;
    else if (mi.State == kExtState_Clear)
    {
      thereAreClearItems = true;
      if (mi.Other)
        state = kExtState_Other;
    }
    counters[state]++;
  }

  int state = kExtState_Clear;
  if (counters[kExtState_Other] != 0)
    state = kExtState_Other;
  else if (counters[kExtState_7Zip] != 0)
    state = kExtState_7Zip;
  
  for (i = 0; i < indices.Size(); i++)
  {
    unsigned listIndex = indices[i];
    CAssoc &assoc = _items[GetRealIndex(listIndex)];
    CModifiedExtInfo &mi = assoc.Pair[group];
    bool change = false;
    
    switch (state)
    {
      case kExtState_Clear: change = true; break;
      case kExtState_Other: change = mi.Other; break;
      default: change = !(mi.Other && thereAreClearItems); break;
    }
    
    if (change)
    {
      mi.State = state;
      RefreshListItem(group, listIndex);
    }
  }
  
  _needSave = true;
  Changed();
}


bool CSystemPage::OnInit()
{
  _needSave = false;

  LangSetDlgItems(*this, kLangIDs, ARRAY_SIZE(kLangIDs));

  _listView.Attach(GetItem(IDL_SYSTEM_ASSOCIATE));
  _listView.SetUnicodeFormat();
  DWORD newFlags = LVS_EX_FULLROWSELECT;
  _listView.SetExtendedListViewStyle(newFlags, newFlags);

  _numIcons = 0;
  _imageList.Create(16, 16, ILC_MASK | ILC_COLOR32, 0, 0);

  _listView.SetImageList(_imageList, LVSIL_SMALL);

  _listView.InsertColumn(0, LangString(IDS_PROP_FILE_TYPE), 72);

  UString s;

  #if NUM_EXT_GROUPS == 1
    s = "Program";
  #else
    #ifndef UNDER_CE
      const unsigned kSize = 256;
      BOOL res;

      DWORD size = kSize;

      #ifndef _UNICODE
      if (!g_IsNT)
      {
        AString s2;
        res = GetUserNameA(s2.GetBuf(size), &size);
        s2.ReleaseBuf_CalcLen(MyMin((unsigned)size, kSize));
        s = GetUnicodeString(s2);
      }
      else
      #endif
      {
        res = GetUserNameW(s.GetBuf(size), &size);
        s.ReleaseBuf_CalcLen(MyMin((unsigned)size, kSize));
      }
    
      if (!res)
    #endif
        s = "Current User";
  #endif

  LV_COLUMNW ci;
  ci.mask = LVCF_TEXT | LVCF_FMT | LVCF_WIDTH | LVCF_SUBITEM;
  ci.cx = 128;
  ci.fmt = LVCFMT_CENTER;
  ci.pszText = s.Ptr_non_const();
  ci.iSubItem = 1;
  _listView.InsertColumn(1, &ci);

  #if NUM_EXT_GROUPS > 1
  {
    LangString(IDS_SYSTEM_ALL_USERS, s);
    ci.pszText = s.Ptr_non_const();
    ci.iSubItem = 2;
    _listView.InsertColumn(2, &ci);
  }
  #endif

  _extDB.Read();
  _items.Clear();

  FOR_VECTOR (i, _extDB.Exts)
  {
    const CExtPlugins &extInfo = _extDB.Exts[i];

    LVITEMW item;
    item.iItem = i;
    item.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
    item.lParam = i;
    item.iSubItem = 0;
    // ListView always uses internal iImage that is 0 by default?
    // so we always use LVIF_IMAGE.
    item.iImage = -1;
    item.pszText = extInfo.Ext.Ptr_non_const();

    CAssoc assoc;
    const CPluginToIcon &plug = extInfo.Plugins[0];
    assoc.SevenZipImageIndex = AddIcon(plug.IconPath, plug.IconIndex);

    CSysString texts[NUM_EXT_GROUPS];
    unsigned g;
    for (g = 0; g < NUM_EXT_GROUPS; g++)
    {
      CModifiedExtInfo &mi = assoc.Pair[g];
      mi.ReadFromRegistry(GetHKey(g), GetSystemString(extInfo.Ext));
      mi.SetState(plug.IconPath);
      mi.ImageIndex = AddIcon(mi.IconPath, mi.IconIndex);
      texts[g] = mi.GetString();
    }
    item.iImage = assoc.GetIconIndex();
    int itemIndex = _listView.InsertItem(&item);
    for (g = 0; g < NUM_EXT_GROUPS; g++)
      _listView.SetSubItem(itemIndex, 1 + g, texts[g]);
    _items.Add(assoc);
  }
  
  if (_listView.GetItemCount() > 0)
    _listView.SetItemState(0, LVIS_FOCUSED, LVIS_FOCUSED);

  return CPropertyPage::OnInit();
}


static UString GetProgramCommand()
{
  UString s ('\"');
  s += fs2us(NDLL::GetModuleDirPrefix());
  s += "7zFM.exe\" \"%1\"";
  return s;
}


LONG CSystemPage::OnApply()
{
  if (!_needSave)
    return PSNRET_NOERROR;

  const UString command = GetProgramCommand();
  
  LONG res = 0;

  FOR_VECTOR (listIndex, _extDB.Exts)
  {
    unsigned realIndex = GetRealIndex(listIndex);
    const CExtPlugins &extInfo = _extDB.Exts[realIndex];
    CAssoc &assoc = _items[realIndex];

    for (unsigned g = 0; g < NUM_EXT_GROUPS; g++)
    {
      CModifiedExtInfo &mi = assoc.Pair[g];
      HKEY key = GetHKey(g);
      
      if (mi.OldState != mi.State)
      {
        LONG res2 = 0;
        
        if (mi.State == kExtState_7Zip)
        {
          UString title = extInfo.Ext;
          title += " Archive";
          const CPluginToIcon &plug = extInfo.Plugins[0];
          res2 = NRegistryAssoc::AddShellExtensionInfo(key, GetSystemString(extInfo.Ext),
              title, command, plug.IconPath, plug.IconIndex);
        }
        else if (mi.State == kExtState_Clear)
          res2 = NRegistryAssoc::DeleteShellExtensionInfo(key, GetSystemString(extInfo.Ext));
        
        if (res == 0)
          res = res2;
        if (res2 == 0)
          mi.OldState = mi.State;
        
        mi.State = mi.OldState;
        RefreshListItem(g, listIndex);
      }
    }
  }
  
  #ifndef UNDER_CE
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
  #endif

  WasChanged = true;

  _needSave = false;
  
  if (res != 0)
    MessageBoxW(*this, NError::MyFormatMessage(res), L"7-Zip", MB_ICONERROR);
  
  return PSNRET_NOERROR;
}


void CSystemPage::OnNotifyHelp()
{
  ShowHelpWindow(kSystemTopic);
}


bool CSystemPage::OnButtonClicked(int buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    /*
    case IDC_SYSTEM_SELECT_ALL:
      _listView.SelectAll();
      return true;
    */
    case IDB_SYSTEM_CURRENT:
    case IDB_SYSTEM_ALL:
      ChangeState(buttonID == IDB_SYSTEM_CURRENT ? 0 : 1);
      return true;
  }
  return CPropertyPage::OnButtonClicked(buttonID, buttonHWND);
}


bool CSystemPage::OnNotify(UINT controlID, LPNMHDR lParam)
{
  if (lParam->hwndFrom == HWND(_listView))
  {
    switch (lParam->code)
    {
      case NM_RETURN:
      {
        ChangeState(0);
        return true;
      }

      case NM_CLICK:
      {
        #ifdef UNDER_CE
        NMLISTVIEW *item = (NMLISTVIEW *)lParam;
        #else
        NMITEMACTIVATE *item = (NMITEMACTIVATE *)lParam;
        if (item->uKeyFlags == 0)
        #endif
        {
          if (item->iItem >= 0)
          {
            // unsigned realIndex = GetRealIndex(item->iItem);
            if (item->iSubItem >= 1 && item->iSubItem <= 2)
            {
              CUIntVector indices;
              indices.Add(item->iItem);
              ChangeState(item->iSubItem < 2 ? 0 : 1, indices);
            }
          }
        }
        break;
      }
      
      case LVN_KEYDOWN:
      {
        if (OnListKeyDown(LPNMLVKEYDOWN(lParam)))
          return true;
        break;
      }
      
      /*
      case NM_RCLICK:
      case NM_DBLCLK:
      case LVN_BEGINRDRAG:
        // PostMessage(kRefreshpluginsListMessage, 0);
        PostMessage(kUpdateDatabase, 0);
        break;
      */
    }
  }
  return CPropertyPage::OnNotify(controlID, lParam);
}


void CSystemPage::ChangeState(unsigned group)
{
  CUIntVector indices;
  
  int itemIndex = -1;
  while ((itemIndex = _listView.GetNextSelectedItem(itemIndex)) != -1)
    indices.Add(itemIndex);
  
  if (indices.IsEmpty())
    FOR_VECTOR (i, _items)
      indices.Add(i);
  
  ChangeState(group, indices);
}


bool CSystemPage::OnListKeyDown(LPNMLVKEYDOWN keyDownInfo)
{
  bool ctrl = IsKeyDown(VK_CONTROL);
  bool alt = IsKeyDown(VK_MENU);

  if (alt)
    return false;

  if ((ctrl && keyDownInfo->wVKey == 'A')
      || (!ctrl && keyDownInfo->wVKey == VK_MULTIPLY))
  {
    _listView.SelectAll();
    return true;
  }

  switch (keyDownInfo->wVKey)
  {
    case VK_SPACE:
    case VK_ADD:
    case VK_SUBTRACT:
    case VK_SEPARATOR:
    case VK_DIVIDE:

    #ifndef UNDER_CE
    case VK_OEM_PLUS:
    case VK_OEM_MINUS:
    #endif

      if (!ctrl)
      {
        ChangeState(keyDownInfo->wVKey == VK_SPACE ? 0 : 1);
        return true;
      }
      break;
  }

  return false;
}

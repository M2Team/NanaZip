// PanelListNotify.cpp

#include "StdAfx.h"

#include "resource.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/PropVariant.h"
#include "../../../Windows/PropVariantConv.h"

#include "../Common/PropIDUtils.h"
#include "../../PropID.h"

#include "App.h"
#include "Panel.h"
#include "FormatUtils.h"

using namespace NWindows;

/* Unicode characters for space:
0x009C STRING TERMINATOR
0x00B7 Middle dot
0x237D Shouldered open box
0x2420 Symbol for space
0x2422 Blank symbol
0x2423 Open box
*/

#define SPACE_REPLACE_CHAR (wchar_t)(0x2423)
#define SPACE_TERMINATOR_CHAR (wchar_t)(0x9C)

#define INT_TO_STR_SPEC(v) \
  while (v >= 10) { temp[i++] = (unsigned char)('0' + (unsigned)(v % 10)); v /= 10; } \
  *s++ = (unsigned char)('0' + (unsigned)v);

static void ConvertSizeToString(UInt64 val, wchar_t *s) throw()
{
  unsigned char temp[32];
  unsigned i = 0;
  
  if (val <= (UInt32)0xFFFFFFFF)
  {
    UInt32 val32 = (UInt32)val;
    INT_TO_STR_SPEC(val32)
  }
  else
  {
    INT_TO_STR_SPEC(val)
  }

  if (i < 3)
  {
    if (i != 0)
    {
      *s++ = temp[(size_t)i - 1];
      if (i == 2)
        *s++ = temp[0];
    }
    *s = 0;
    return;
  }

  unsigned r = i % 3;
  if (r != 0)
  {
    s[0] = temp[--i];
    if (r == 2)
      s[1] = temp[--i];
    s += r;
  }

  do
  {
    s[0] = ' ';
    s[1] = temp[(size_t)i - 1];
    s[2] = temp[(size_t)i - 2];
    s[3] = temp[(size_t)i - 3];
    s += 4;
  }
  while (i -= 3);
  
  *s = 0;
}

UString ConvertSizeToString(UInt64 value);
UString ConvertSizeToString(UInt64 value)
{
  wchar_t s[32];
  ConvertSizeToString(value, s);
  return s;
}

static inline unsigned GetHex(unsigned v)
{
  return (v < 10) ? ('0' + v) : ('A' + (v - 10));
}

/*
static void HexToString(char *dest, const Byte *data, UInt32 size)
{
  for (UInt32 i = 0; i < size; i++)
  {
    unsigned b = data[i];
    dest[0] = GetHex((b >> 4) & 0xF);
    dest[1] = GetHex(b & 0xF);
    dest += 2;
  }
  *dest = 0;
}
*/

bool IsSizeProp(UINT propID) throw();
bool IsSizeProp(UINT propID) throw()
{
  switch (propID)
  {
    case kpidSize:
    case kpidPackSize:
    case kpidNumSubDirs:
    case kpidNumSubFiles:
    case kpidOffset:
    case kpidLinks:
    case kpidNumBlocks:
    case kpidNumVolumes:
    case kpidPhySize:
    case kpidHeadersSize:
    case kpidTotalSize:
    case kpidFreeSpace:
    case kpidClusterSize:
    case kpidNumErrors:
    case kpidNumStreams:
    case kpidNumAltStreams:
    case kpidAltStreamsSize:
    case kpidVirtualSize:
    case kpidUnpackSize:
    case kpidTotalPhySize:
    case kpidTailSize:
    case kpidEmbeddedStubSize:
      return true;
  }
  return false;
}



/*
#include <stdio.h>

UInt64 GetCpuTicks()
{
    #ifdef _WIN64
      return __rdtsc();
    #else
      UInt32 lowVal, highVal;
      __asm RDTSC;
      __asm mov lowVal, EAX;
      __asm mov highVal, EDX;
      return ((UInt64)highVal << 32) | lowVal;
    #endif
}

UInt32 g_NumGroups;
UInt64 g_start_tick;
UInt64 g_prev_tick;
DWORD g_Num_SetItemText;
UInt32 g_NumMessages;
*/

LRESULT CPanel::SetItemText(LVITEMW &item)
{
  if (_dontShowMode)
    return 0;
  UInt32 realIndex = GetRealIndex(item);

  // g_Num_SetItemText++;

  /*
  if ((item.mask & LVIF_IMAGE) != 0)
  {
    bool defined  = false;
    CComPtr<IFolderGetSystemIconIndex> folderGetSystemIconIndex;
    _folder.QueryInterface(&folderGetSystemIconIndex);
    if (folderGetSystemIconIndex)
    {
      folderGetSystemIconIndex->GetSystemIconIndex(index, &item.iImage);
      defined = (item.iImage > 0);
    }
    if (!defined)
    {
      NCOM::CPropVariant prop;
      _folder->GetProperty(index, kpidAttrib, &prop);
      UINT32 attrib = 0;
      if (prop.vt == VT_UI4)
        attrib = prop.ulVal;
      else if (IsItemFolder(index))
        attrib |= FILE_ATTRIBUTE_DIRECTORY;
      if (_currentFolderPrefix.IsEmpty())
        throw 1;
      else
        item.iImage = _extToIconMap.GetIconIndex(attrib, GetSystemString(GetItemName(index)));
    }
    // item.iImage = 1;
  }
  */

  if ((item.mask & LVIF_TEXT) == 0)
    return 0;

  LPWSTR text = item.pszText;

  if (item.cchTextMax > 0)
    text[0] = 0;

  if (item.cchTextMax <= 1)
    return 0;
  
  const CPropColumn &property = _visibleColumns[item.iSubItem];
  PROPID propID = property.ID;

  if (realIndex == kParentIndex_UInt32)
  {
    if (propID == kpidName)
    {
      if (item.cchTextMax > 2)
      {
        text[0] = '.';
        text[1] = '.';
        text[2] = 0;
      }
    }
    return 0;
  }

  /*
  // List-view in report-view in Windows 10 is slow (50+ ms) for page change.
  // that code shows the time of page reload for items
  // if you know how to improve the speed of list view refresh, notify 7-Zip developer

  // if (propID == 2000)
  // if (propID == kpidName)
  {
    // debug column;
    // DWORD dw = GetCpuTicks();
    UInt64 dw = GetCpuTicks();
    UInt64 deltaLast = dw - g_prev_tick;
    #define conv_ticks(t) ((unsigned)((t) / 100000))
    if (deltaLast > 1000u * 1000 * 1000)
    {
      UInt64 deltaFull = g_prev_tick - g_start_tick;
      char s[128];
      sprintf(s, "%d", conv_ticks(deltaFull));
      OutputDebugStringA(s);
      g_start_tick = dw;
      g_NumGroups++;
    }
    g_prev_tick = dw;
    UString u;
    char s[128];
    UInt64 deltaFull = dw - g_start_tick;
    // for (int i = 0; i < 100000; i++)
    sprintf(s, "%d %d %d-%d ", g_NumMessages, g_Num_SetItemText, g_NumGroups, conv_ticks(deltaFull));
    // sprintf(s, "%d-%d ", g_NumGroups, conv_ticks(deltaFull));
    u = s;
    lstrcpyW(text, u.Ptr());
    text += u.Len();

    // dw = GetCpuTicks();
    // deltaFull = dw - g_prev_tick;
    // sprintf(s, "-%d ", conv_ticks(deltaFull));
    // u = s;
    // lstrcpyW(text, u.Ptr());
    // text += u.Len();

    if (propID != kpidName)
      return 0;
  }
  */


  if (property.IsRawProp)
  {
    const void *data;
    UInt32 dataSize;
    UInt32 propType;
    RINOK(_folderRawProps->GetRawProp(realIndex, propID, &data, &dataSize, &propType));
    unsigned limit = item.cchTextMax - 1;
    if (dataSize == 0)
    {
      text[0] = 0;
      return 0;
    }
    
    if (propID == kpidNtReparse)
    {
      UString s;
      ConvertNtReparseToString((const Byte *)data, dataSize, s);
      if (!s.IsEmpty())
      {
        unsigned i;
        for (i = 0; i < limit; i++)
        {
          wchar_t c = s[i];
          if (c == 0)
            break;
          text[i] = c;
        }
        text[i] = 0;
        return 0;
      }
    }
    else if (propID == kpidNtSecure)
    {
      AString s;
      ConvertNtSecureToString((const Byte *)data, dataSize, s);
      if (!s.IsEmpty())
      {
        unsigned i;
        for (i = 0; i < limit; i++)
        {
          wchar_t c = (Byte)s[i];
          if (c == 0)
            break;
          text[i] = c;
        }
        text[i] = 0;
        return 0;
      }
    }
    {
      const unsigned kMaxDataSize = 64;
      if (dataSize > kMaxDataSize)
      {
        char temp[32];
        MyStringCopy(temp, "data:");
        ConvertUInt32ToString(dataSize, temp + 5);
        unsigned i;
        for (i = 0; i < limit; i++)
        {
          wchar_t c = (Byte)temp[i];
          if (c == 0)
            break;
          text[i] = c;
        }
        text[i] = 0;
      }
      else
      {
        if (dataSize > limit)
          dataSize = limit;
        WCHAR *dest = text;
        for (UInt32 i = 0; i < dataSize; i++)
        {
          unsigned b = ((const Byte *)data)[i];
          dest[0] = (WCHAR)GetHex((b >> 4) & 0xF);
          dest[1] = (WCHAR)GetHex(b & 0xF);
          dest += 2;
        }
        *dest = 0;
      }
    }
    return 0;
  }
  /*
  {
    NCOM::CPropVariant prop;
    if (propID == kpidType)
      string = GetFileType(index);
    else
    {
      HRESULT result = m_ArchiveFolder->GetProperty(index, propID, &prop);
      if (result != S_OK)
      {
        // PrintMessage("GetPropertyValue error");
        return 0;
      }
      string = ConvertPropertyToString(prop, propID, false);
    }
  }
  */
  // const NFind::CFileInfo &aFileInfo = m_Files[index];

  NCOM::CPropVariant prop;
  /*
  bool needRead = true;
  if (propID == kpidSize)
  {
    CComPtr<IFolderGetItemFullSize> getItemFullSize;
    if (_folder.QueryInterface(&getItemFullSize) == S_OK)
    {
      if (getItemFullSize->GetItemFullSize(index, &prop) == S_OK)
        needRead = false;
    }
  }
  if (needRead)
  */

  if (item.cchTextMax < 32)
    return 0;

  if (propID == kpidName)
  {
    if (_folderGetItemName)
    {
      const wchar_t *name = NULL;
      unsigned nameLen = 0;
      _folderGetItemName->GetItemName(realIndex, &name, &nameLen);
      
      if (name)
      {
        unsigned dest = 0;
        unsigned limit = item.cchTextMax - 1;
        
        for (unsigned i = 0; dest < limit;)
        {
          wchar_t c = name[i++];
          if (c == 0)
            break;
          text[dest++] = c;
          
          if (c != ' ')
          {
            if (c != 0x202E) // RLO
              continue;
            text[(size_t)dest - 1] = '_';
            continue;
          }
          
          if (name[i] != ' ')
            continue;
          
          unsigned t = 1;
          for (; name[i + t] == ' '; t++);

          if (t >= 4 && dest + 4 < limit)
          {
            text[dest++] = '.';
            text[dest++] = '.';
            text[dest++] = '.';
            text[dest++] = ' ';
            i += t;
          }
        }

        if (dest == 0)
          text[dest++]= '_';

        #ifdef _WIN32
        else if (text[(size_t)dest - 1] == ' ')
        {
          if (dest < limit)
            text[dest++] = SPACE_TERMINATOR_CHAR;
          else
            text[dest - 1] = SPACE_REPLACE_CHAR;
        }
        #endif

        text[dest] = 0;
        // OutputDebugStringW(text);
        return 0;
      }
    }
  }
  
  if (propID == kpidPrefix)
  {
    if (_folderGetItemName)
    {
      const wchar_t *name = NULL;
      unsigned nameLen = 0;
      _folderGetItemName->GetItemPrefix(realIndex, &name, &nameLen);
      if (name)
      {
        unsigned dest = 0;
        unsigned limit = item.cchTextMax - 1;
        for (unsigned i = 0; dest < limit;)
        {
          wchar_t c = name[i++];
          if (c == 0)
            break;
          text[dest++] = c;
        }
        text[dest] = 0;
        return 0;
      }
    }
  }
  
  HRESULT res = _folder->GetProperty(realIndex, propID, &prop);
  
  if (res != S_OK)
  {
    MyStringCopy(text, L"Error: ");
    // s = UString("Error: ") + HResultToMessage(res);
  }
  else if ((prop.vt == VT_UI8 || prop.vt == VT_UI4 || prop.vt == VT_UI2) && IsSizeProp(propID))
  {
    UInt64 v = 0;
    ConvertPropVariantToUInt64(prop, v);
    ConvertSizeToString(v, text);
  }
  else if (prop.vt == VT_BSTR)
  {
    unsigned limit = item.cchTextMax - 1;
    const wchar_t *src = prop.bstrVal;
    unsigned i;
    for (i = 0; i < limit; i++)
    {
      wchar_t c = src[i];
      if (c == 0) break;
      if (c == 0xA) c = ' ';
      if (c == 0xD) c = ' ';
      text[i] = c;
    }
    text[i] = 0;
  }
  else
  {
    char temp[64];
    ConvertPropertyToShortString2(temp, prop, propID, _timestampLevel);
    unsigned i;
    unsigned limit = item.cchTextMax - 1;
    for (i = 0; i < limit; i++)
    {
      wchar_t c = (Byte)temp[i];
      if (c == 0)
        break;
      text[i] = c;
    }
    text[i] = 0;
  }
  
  return 0;
}

#ifndef UNDER_CE
extern DWORD g_ComCtl32Version;
#endif

void CPanel::OnItemChanged(NMLISTVIEW *item)
{
  int index = (int)item->lParam;
  if (index == kParentIndex)
    return;
  bool oldSelected = (item->uOldState & LVIS_SELECTED) != 0;
  bool newSelected = (item->uNewState & LVIS_SELECTED) != 0;
  // Don't change this code. It works only with such check
  if (oldSelected != newSelected)
    _selectedStatusVector[index] = newSelected;
}

extern bool g_LVN_ITEMACTIVATE_Support;

void CPanel::OnNotifyActivateItems()
{
  bool alt = IsKeyDown(VK_MENU);
  bool ctrl = IsKeyDown(VK_CONTROL);
  bool shift = IsKeyDown(VK_SHIFT);
  if (!shift && alt && !ctrl)
    Properties();
  else
    OpenSelectedItems(!shift || alt || ctrl);
}

bool CPanel::OnNotifyList(LPNMHDR header, LRESULT &result)
{
  switch (header->code)
  {
    case LVN_ITEMCHANGED:
    {
      if (_enableItemChangeNotify)
      {
        if (!_mySelectMode)
          OnItemChanged((LPNMLISTVIEW)header);
        
        // Post_Refresh_StatusBar();
        /* 9.26: we don't call Post_Refresh_StatusBar.
           it was very slow if we select big number of files
           and then clead slection by selecting just new file.
           probably it called slow Refresh_StatusBar for each item deselection.
           I hope Refresh_StatusBar still will be called for each key / mouse action.
        */
      }
      return false;
    }
    /*

    case LVN_ODSTATECHANGED:
      {
      break;
      }
    */

    case LVN_GETDISPINFOW:
    {
      LV_DISPINFOW *dispInfo = (LV_DISPINFOW *)header;

      //is the sub-item information being requested?

      if ((dispInfo->item.mask & LVIF_TEXT) != 0 ||
          (dispInfo->item.mask & LVIF_IMAGE) != 0)
        SetItemText(dispInfo->item);
      {
        // 20.03:
        result = 0;
        return true;
        // old 7-Zip:
        // return false;
      }
    }
    case LVN_KEYDOWN:
    {
      LPNMLVKEYDOWN keyDownInfo = LPNMLVKEYDOWN(header);
      bool boolResult = OnKeyDown(keyDownInfo, result);
      switch (keyDownInfo->wVKey)
      {
        case VK_CONTROL:
        case VK_SHIFT:
        case VK_MENU:
          break;
        default:
          Post_Refresh_StatusBar();
      }
      return boolResult;
    }

    case LVN_COLUMNCLICK:
      OnColumnClick(LPNMLISTVIEW(header));
      return false;

    case LVN_ITEMACTIVATE:
      if (g_LVN_ITEMACTIVATE_Support)
      {
        OnNotifyActivateItems();
        return false;
      }
      break;
    case NM_DBLCLK:
    case NM_RETURN:
      if (!g_LVN_ITEMACTIVATE_Support)
      {
        OnNotifyActivateItems();
        return false;
      }
      break;

    case NM_RCLICK:
      Post_Refresh_StatusBar();
      break;

    /*
      return OnRightClick((LPNMITEMACTIVATE)header, result);
    */
      /*
      case NM_CLICK:
      SendRefreshStatusBarMessage();
      return 0;
      
        // TODO : Handler default action...
        return 0;
        case LVN_ITEMCHANGED:
        {
        NMLISTVIEW *pNMLV = (NMLISTVIEW *) lpnmh;
        SelChange(pNMLV);
        return TRUE;
        }
        case NM_SETFOCUS:
        return onSetFocus(NULL);
        case NM_KILLFOCUS:
        return onKillFocus(NULL);
      */
    case NM_CLICK:
    {
      // we need SetFocusToList, if we drag-select items from other panel.
      SetFocusToList();
      Post_Refresh_StatusBar();
      if (_mySelectMode)
        #ifndef UNDER_CE
        if (g_ComCtl32Version >= MAKELONG(71, 4))
        #endif
          OnLeftClick((MY_NMLISTVIEW_NMITEMACTIVATE *)header);
      return false;
    }
    case LVN_BEGINLABELEDITW:
      result = OnBeginLabelEdit((LV_DISPINFOW *)header);
      return true;
    case LVN_ENDLABELEDITW:
      result = OnEndLabelEdit((LV_DISPINFOW *)header);
      return true;

    case NM_CUSTOMDRAW:
    {
      if (_mySelectMode || (_markDeletedItems && _thereAreDeletedItems))
        return OnCustomDraw((LPNMLVCUSTOMDRAW)header, result);
      break;
    }
    case LVN_BEGINDRAG:
    {
      OnDrag((LPNMLISTVIEW)header);
      Post_Refresh_StatusBar();
      break;
    }
    // case LVN_BEGINRDRAG:
  }
  return false;
}

bool CPanel::OnCustomDraw(LPNMLVCUSTOMDRAW lplvcd, LRESULT &result)
{
  switch (lplvcd->nmcd.dwDrawStage)
  {
  case CDDS_PREPAINT :
    result = CDRF_NOTIFYITEMDRAW;
    return true;
    
  case CDDS_ITEMPREPAINT:
    /*
    SelectObject(lplvcd->nmcd.hdc,
    GetFontForItem(lplvcd->nmcd.dwItemSpec,
    lplvcd->nmcd.lItemlParam) );
    lplvcd->clrText = GetColorForItem(lplvcd->nmcd.dwItemSpec,
    lplvcd->nmcd.lItemlParam);
    lplvcd->clrTextBk = GetBkColorForItem(lplvcd->nmcd.dwItemSpec,
    lplvcd->nmcd.lItemlParam);
    */
    int realIndex = (int)lplvcd->nmcd.lItemlParam;
    lplvcd->clrTextBk = _listView.GetBkColor();
    if (_mySelectMode)
    {
      if (realIndex != kParentIndex && _selectedStatusVector[realIndex])
       lplvcd->clrTextBk = RGB(255, 192, 192);
    }

    if (_markDeletedItems && _thereAreDeletedItems)
    {
      if (IsItem_Deleted(realIndex))
        lplvcd->clrText = RGB(255, 0, 0);
    }
    // lplvcd->clrText = RGB(0, 0, 0);
    // result = CDRF_NEWFONT;
    result = CDRF_NOTIFYITEMDRAW;
    return true;
    
    // return false;
    // return true;
    /*
    case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
    if (lplvcd->iSubItem == 0)
    {
    // lplvcd->clrText = RGB(255, 0, 0);
    lplvcd->clrTextBk = RGB(192, 192, 192);
    }
    else
    {
    lplvcd->clrText = RGB(0, 0, 0);
    lplvcd->clrTextBk = RGB(255, 255, 255);
    }
    return true;
    */

        /* At this point, you can change the background colors for the item
        and any subitems and return CDRF_NEWFONT. If the list-view control
        is in report mode, you can simply return CDRF_NOTIFYSUBITEMREDRAW
        to customize the item's subitems individually */
  }
  return false;
}

void CPanel::Refresh_StatusBar()
{
  /*
  g_name_cnt++;
  char s[256];
  sprintf(s, "g_name_cnt = %8d", g_name_cnt);
  OutputDebugStringA(s);
  */
  // DWORD dw = GetTickCount();

  CRecordVector<UInt32> indices;
  GetOperatedItemIndices(indices);

  wchar_t temp[32];
  ConvertUInt32ToString(indices.Size(), temp);
  wcscat(temp, L" / ");
  ConvertUInt32ToString(_selectedStatusVector.Size(), temp + wcslen(temp));

  // UString s1 = MyFormatNew(g_App.LangString_N_SELECTED_ITEMS, NumberToString(indices.Size()));
  // UString s1 = MyFormatNew(IDS_N_SELECTED_ITEMS, NumberToString(indices.Size()));
  _statusBar.SetText(0, MyFormatNew(g_App.LangString_N_SELECTED_ITEMS, temp));
  // _statusBar.SetText(0, MyFormatNew(IDS_N_SELECTED_ITEMS, NumberToString(indices.Size())));

  wchar_t selectSizeString[32];
  selectSizeString[0] = 0;

  if (indices.Size() > 0)
  {
    // for (unsigned ttt = 0; ttt < 1000; ttt++) {
    UInt64 totalSize = 0;
    FOR_VECTOR (i, indices)
      totalSize += GetItemSize(indices[i]);
    ConvertSizeToString(totalSize, selectSizeString);
    // }
  }
  _statusBar.SetText(1, selectSizeString);

  int focusedItem = _listView.GetFocusedItem();
  wchar_t sizeString[32];
  sizeString[0] = 0;
  wchar_t dateString[32];
  dateString[0] = 0;
  if (focusedItem >= 0 && _listView.GetSelectedCount() > 0)
  {
    int realIndex = GetRealItemIndex(focusedItem);
    if (realIndex != kParentIndex)
    {
      ConvertSizeToString(GetItemSize(realIndex), sizeString);
      NCOM::CPropVariant prop;
      if (_folder->GetProperty(realIndex, kpidMTime, &prop) == S_OK)
      {
        char dateString2[32];
        dateString2[0] = 0;
        ConvertPropertyToShortString2(dateString2, prop, kpidMTime);
        for (unsigned i = 0;; i++)
        {
          char c = dateString2[i];
          dateString[i] = (Byte)c;
          if (c == 0)
            break;
        }
      }
    }
  }
  _statusBar.SetText(2, sizeString);
  _statusBar.SetText(3, dateString);
  
  // _statusBar.SetText(4, nameString);
  // _statusBar2.SetText(1, MyFormatNew(L"{0} bytes", NumberToStringW(totalSize)));
  // }
  /*
  dw = GetTickCount() - dw;
  sprintf(s, "status = %8d ms", dw);
  OutputDebugStringA(s);
  */
}

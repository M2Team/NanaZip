// PanelKey.cpp

#include "StdAfx.h"

#include "Panel.h"
#include "HelpUtils.h"

#include "../../PropID.h"
#include "App.h"

using namespace NWindows;

// #define kHelpTopic "FM/index.htm"

struct CVKeyPropIDPair
{
  WORD VKey;
  PROPID PropID;
};

static const CVKeyPropIDPair g_VKeyPropIDPairs[] =
{
  { VK_F3, kpidName },
  { VK_F4, kpidExtension },
  { VK_F5, kpidMTime },
  { VK_F6, kpidSize },
  { VK_F7, kpidNoProperty }
};

static int FindVKeyPropIDPair(WORD vKey)
{
  for (unsigned i = 0; i < ARRAY_SIZE(g_VKeyPropIDPairs); i++)
    if (g_VKeyPropIDPairs[i].VKey == vKey)
      return i;
  return -1;
}


bool CPanel::OnKeyDown(LPNMLVKEYDOWN keyDownInfo, LRESULT &result)
{
  if (keyDownInfo->wVKey == VK_TAB && keyDownInfo->hdr.hwndFrom == _listView)
  {
    _panelCallback->OnTab();
    return false;
  }
  bool alt = IsKeyDown(VK_MENU);
  bool ctrl = IsKeyDown(VK_CONTROL);
  // bool leftCtrl = IsKeyDown(VK_LCONTROL);
  bool rightCtrl = IsKeyDown(VK_RCONTROL);
  bool shift = IsKeyDown(VK_SHIFT);
  result = 0;

  if (keyDownInfo->wVKey >= '0' && keyDownInfo->wVKey <= '9' &&
      (rightCtrl || alt))
  {
    int index = keyDownInfo->wVKey - '0';
    if (shift)
    {
      SetBookmark(index);
      return true;
    }
    else
    {
      OpenBookmark(index);
      return true;
    }
  }

  if ((keyDownInfo->wVKey == VK_F2 ||
    keyDownInfo->wVKey == VK_F1) && alt && !ctrl && !shift)
  {
    _panelCallback->SetFocusToPath(keyDownInfo->wVKey == VK_F1 ? 0 : 1);
    return true;
  }

  if ((keyDownInfo->wVKey == VK_F9) && !alt && !ctrl && !shift)
  {
    g_App.SwitchOnOffOnePanel();
  }

  if (keyDownInfo->wVKey >= VK_F3 && keyDownInfo->wVKey <= VK_F12 && ctrl)
  {
    int index = FindVKeyPropIDPair(keyDownInfo->wVKey);
    if (index >= 0)
      SortItemsWithPropID(g_VKeyPropIDPairs[index].PropID);
  }

  switch (keyDownInfo->wVKey)
  {
    case VK_SHIFT:
    {
      _selectionIsDefined = false;
      _prevFocusedItem = _listView.GetFocusedItem();
      break;
    }
    /*
    case VK_F1:
    {
      // ShowHelpWindow(NULL, kHelpTopic);
      break;
    }
    */
    case VK_F2:
    {
      if (!alt && !ctrl &&!shift)
      {
        RenameFile();
        return true;
      }
      break;
    }
    case VK_F3:
    {
      if (!alt && !ctrl && !shift)
      {
        EditItem(false);
        return true;
      }
      break;
    }
    case VK_F4:
    {
      if (!alt && !ctrl && !shift)
      {
        EditItem(true);
        return true;
      }
      if (!alt && !ctrl && shift)
      {
        CreateFile();
        return true;
      }
      break;
    }
    case VK_F5:
    {
      if (!alt && !ctrl)
      {
        _panelCallback->OnCopy(false, shift);
        return true;
      }
      break;
    }
    case VK_F6:
    {
      if (!alt && !ctrl)
      {
        _panelCallback->OnCopy(true, shift);
        return true;
      }
      break;
    }
    case VK_F7:
    {
      if (!alt && !ctrl && !shift)
      {
        /* we can process F7 via menu ACCELERATOR.
          But menu loading can be slow in case of UNC paths and system menu.
          So we use don't use ACCELERATOR */
        CreateFolder();
        return true;
      }
      break;
    }
    case VK_DELETE:
    {
      DeleteItems(!shift);
      return true;
    }
    case VK_INSERT:
    {
      if (!alt)
      {
        if (ctrl && !shift)
        {
          EditCopy();
          return true;
        }
        if (shift && !ctrl)
        {
          EditPaste();
          return true;
        }
        if (!shift && !ctrl && _mySelectMode)
        {
          OnInsert();
          return true;
        }
      }
      return false;
    }
    case VK_DOWN:
    {
      if (shift)
        OnArrowWithShift();
      return false;
    }
    case VK_UP:
    {
      if (alt)
        _panelCallback->OnSetSameFolder();
      else if (shift)
        OnArrowWithShift();
      return false;
    }
    case VK_RIGHT:
    {
      if (alt)
        _panelCallback->OnSetSubFolder();
      else if (shift)
        OnArrowWithShift();
      return false;
    }
    case VK_LEFT:
    {
      if (alt)
        _panelCallback->OnSetSubFolder();
      else if (shift)
        OnArrowWithShift();
      return false;
    }
    case VK_NEXT:
    {
      if (ctrl && !alt && !shift)
      {
        // EnterToFocused();
        return true;
      }
      break;
    }
    case VK_ADD:
    {
      if (alt)
        SelectByType(true);
      else if (shift)
        SelectAll(true);
      else if (!ctrl)
        SelectSpec(true);
      return true;
    }
    case VK_SUBTRACT:
    {
      if (alt)
        SelectByType(false);
      else if (shift)
        SelectAll(false);
      else
        SelectSpec(false);
      return true;
    }
    /*
    case VK_DELETE:
      CommandDelete();
      return 0;
    case VK_F1:
      CommandHelp();
      return 0;
    */
    case VK_BACK:
      OpenParentFolder();
      return true;
    /*
    case VK_DIVIDE:
    case '\\':
    case '/':
    case VK_OEM_5:
    {
      // OpenRootFolder();
      OpenDrivesFolder();

      return true;
    }
    */
    case 'A':
      if (ctrl)
      {
        SelectAll(true);
        return true;
      }
      return false;
    case 'X':
      if (ctrl)
      {
        EditCut();
        return true;
      }
      return false;
    case 'C':
      if (ctrl)
      {
        EditCopy();
        return true;
      }
      return false;
    case 'V':
      if (ctrl)
      {
        EditPaste();
        return true;
      }
      return false;
    case 'N':
      if (ctrl)
      {
        CreateFile();
        return true;
      }
      return false;
    case 'R':
      if (ctrl)
      {
        OnReload();
        return true;
      }
      return false;
    case 'Z':
      if (ctrl)
      {
        ChangeComment();
        return true;
      }
      return false;
    case '1':
    case '2':
    case '3':
    case '4':
      if (ctrl)
      {
        int styleIndex = keyDownInfo->wVKey - '1';
        SetListViewMode(styleIndex);
        return true;
      }
      return false;
    case VK_MULTIPLY:
      {
        InvertSelection();
        return true;
      }
    case VK_F12:
      if (alt && !ctrl && !shift)
      {
        FoldersHistory();
        return true;
      }
  }
  return false;
}

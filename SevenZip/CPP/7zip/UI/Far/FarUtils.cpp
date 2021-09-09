// FarUtils.cpp

#include "StdAfx.h"

// #include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#ifndef UNDER_CE
#include "../../../Windows/Console.h"
#endif
#include "../../../Windows/Defs.h"
#include "../../../Windows/ErrorMsg.h"

#include "FarUtils.h"

using namespace NWindows;

namespace NFar {

CStartupInfo g_StartupInfo;

const char kRegistryKeyDelimiter = '\\';

void CStartupInfo::Init(const PluginStartupInfo &pluginStartupInfo,
    const char *pluginNameForRegistry)
{
  m_Data = pluginStartupInfo;
  m_RegistryPath = pluginStartupInfo.RootKey;
  m_RegistryPath += kRegistryKeyDelimiter;
  m_RegistryPath += pluginNameForRegistry;
}

const char *CStartupInfo::GetMsgString(int messageId)
{
  return (const char*)m_Data.GetMsg(m_Data.ModuleNumber, messageId);
}

int CStartupInfo::ShowMessage(unsigned int flags,
    const char *helpTopic, const char **items, int numItems, int numButtons)
{
  return m_Data.Message(m_Data.ModuleNumber, flags, helpTopic,
      items, numItems, numButtons);
}

namespace NMessageID
{
  enum
  {
    kOk,
    kCancel,
    kWarning,
    kError
  };
}

int CStartupInfo::ShowWarningWithOk(const char **items, int numItems)
{
  return ShowMessage(FMSG_WARNING | FMSG_MB_OK, NULL, items, numItems, 0);
}

extern const char *g_PluginName_for_Error;

void CStartupInfo::SetErrorTitle(AString &s)
{
  if (g_PluginName_for_Error)
  {
    s += g_PluginName_for_Error;
    s += ": ";
  }
  s += GetMsgString(NMessageID::kError);
}

/*
int CStartupInfo::ShowErrorMessage(const char *message)
{
  AString s;
  SetErrorTitle(s);
  const char *items[]= { s, message };
  return ShowWarningWithOk(items, ARRAY_SIZE(items));
}
*/

int CStartupInfo::ShowErrorMessage2(const char *m1, const char *m2)
{
  AString s;
  SetErrorTitle(s);
  const char *items[]= { s, m1, m2 };
  return ShowWarningWithOk(items, ARRAY_SIZE(items));
}

static void SplitString(const AString &src, AStringVector &destStrings)
{
  destStrings.Clear();
  AString s;
  unsigned len = src.Len();
  if (len == 0)
    return;
  for (unsigned i = 0; i < len; i++)
  {
    char c = src[i];
    if (c == '\n')
    {
      if (!s.IsEmpty())
      {
        destStrings.Add(s);
        s.Empty();
      }
    }
    else
      s += c;
  }
  if (!s.IsEmpty())
    destStrings.Add(s);
}

int CStartupInfo::ShowErrorMessage(const char *message)
{
  AStringVector strings;
  SplitString((AString)message, strings);
  const unsigned kNumStringsMax = 20;
  const char *items[kNumStringsMax + 1];
  unsigned pos = 0;
  items[pos++] = GetMsgString(NMessageID::kError);
  for (unsigned i = 0; i < strings.Size() && pos < kNumStringsMax; i++)
    items[pos++] = strings[i];
  items[pos++] = GetMsgString(NMessageID::kOk);

  return ShowMessage(FMSG_WARNING, NULL, items, pos, 1);
}

/*
int CStartupInfo::ShowMessageLines(const char *message)
{
  AString s = GetMsgString(NMessageID::kError);
  s.Add_LF();
  s += message;
  return ShowMessage(FMSG_WARNING | FMSG_MB_OK | FMSG_ALLINONE, NULL,
      (const char **)(const char *)s, 1, 0);
}
*/

int CStartupInfo::ShowMessage(int messageId)
{
  return ShowErrorMessage(GetMsgString(messageId));
}

int CStartupInfo::ShowDialog(int X1, int Y1, int X2, int Y2,
    const char *helpTopic, struct FarDialogItem *items, int numItems)
{
  return m_Data.Dialog(m_Data.ModuleNumber, X1, Y1, X2, Y2, (char *)helpTopic,
      items, numItems);
}

int CStartupInfo::ShowDialog(int sizeX, int sizeY,
    const char *helpTopic, struct FarDialogItem *items, int numItems)
{
  return ShowDialog(-1, -1, sizeX, sizeY, helpTopic, items, numItems);
}

inline static BOOL GetBOOLValue(bool v) { return (v? TRUE: FALSE); }

void CStartupInfo::InitDialogItems(const CInitDialogItem  *srcItems,
    FarDialogItem *destItems, int numItems)
{
  for (int i = 0; i < numItems; i++)
  {
    const CInitDialogItem &srcItem = srcItems[i];
    FarDialogItem &destItem = destItems[i];

    destItem.Type = srcItem.Type;
    destItem.X1 = srcItem.X1;
    destItem.Y1 = srcItem.Y1;
    destItem.X2 = srcItem.X2;
    destItem.Y2 = srcItem.Y2;
    destItem.Focus = GetBOOLValue(srcItem.Focus);
    if (srcItem.HistoryName != NULL)
      destItem.History = srcItem.HistoryName;
    else
      destItem.Selected = GetBOOLValue(srcItem.Selected);
    destItem.Flags = srcItem.Flags;
    destItem.DefaultButton = GetBOOLValue(srcItem.DefaultButton);

    if (srcItem.DataMessageId < 0)
      MyStringCopy(destItem.Data, srcItem.DataString);
    else
      MyStringCopy(destItem.Data, GetMsgString(srcItem.DataMessageId));

    /*
    if ((unsigned int)Init[i].Data < 0xFFF)
      MyStringCopy(destItem.Data, GetMsg((unsigned int)srcItem.Data));
    else
      MyStringCopy(destItem.Data,srcItem.Data);
    */
  }
}

// --------------------------------------------

HANDLE CStartupInfo::SaveScreen(int X1, int Y1, int X2, int Y2)
{
  return m_Data.SaveScreen(X1, Y1, X2, Y2);
}

HANDLE CStartupInfo::SaveScreen()
{
  return SaveScreen(0, 0, -1, -1);
}

void CStartupInfo::RestoreScreen(HANDLE handle)
{
  m_Data.RestoreScreen(handle);
}

CSysString CStartupInfo::GetFullKeyName(const char *keyName) const
{
  AString s = m_RegistryPath;
  if (keyName && *keyName)
  {
    s += kRegistryKeyDelimiter;
    s += keyName;
  }
  return (CSysString)s;
}


LONG CStartupInfo::CreateRegKey(HKEY parentKey,
    const char *keyName, NRegistry::CKey &destKey) const
{
  return destKey.Create(parentKey, GetFullKeyName(keyName));
}

LONG CStartupInfo::OpenRegKey(HKEY parentKey,
    const char *keyName, NRegistry::CKey &destKey) const
{
  return destKey.Open(parentKey, GetFullKeyName(keyName));
}

void CStartupInfo::SetRegKeyValue(HKEY parentKey, const char *keyName,
    LPCTSTR valueName, LPCTSTR value) const
{
  NRegistry::CKey regKey;
  CreateRegKey(parentKey, keyName, regKey);
  regKey.SetValue(valueName, value);
}

void CStartupInfo::SetRegKeyValue(HKEY parentKey, const char *keyName,
    LPCTSTR valueName, UInt32 value) const
{
  NRegistry::CKey regKey;
  CreateRegKey(parentKey, keyName, regKey);
  regKey.SetValue(valueName, value);
}

void CStartupInfo::SetRegKeyValue(HKEY parentKey, const char *keyName,
    LPCTSTR valueName, bool value) const
{
  NRegistry::CKey regKey;
  CreateRegKey(parentKey, keyName, regKey);
  regKey.SetValue(valueName, value);
}

CSysString CStartupInfo::QueryRegKeyValue(HKEY parentKey, const char *keyName,
    LPCTSTR valueName, const CSysString &valueDefault) const
{
  NRegistry::CKey regKey;
  if (OpenRegKey(parentKey, keyName, regKey) != ERROR_SUCCESS)
    return valueDefault;
  
  CSysString value;
  if (regKey.QueryValue(valueName, value) != ERROR_SUCCESS)
    return valueDefault;
  
  return value;
}

UInt32 CStartupInfo::QueryRegKeyValue(HKEY parentKey, const char *keyName,
    LPCTSTR valueName, UInt32 valueDefault) const
{
  NRegistry::CKey regKey;
  if (OpenRegKey(parentKey, keyName, regKey) != ERROR_SUCCESS)
    return valueDefault;
  
  UInt32 value;
  if (regKey.QueryValue(valueName, value) != ERROR_SUCCESS)
    return valueDefault;
  
  return value;
}

bool CStartupInfo::QueryRegKeyValue(HKEY parentKey, const char *keyName,
    LPCTSTR valueName, bool valueDefault) const
{
  NRegistry::CKey regKey;
  if (OpenRegKey(parentKey, keyName, regKey) != ERROR_SUCCESS)
    return valueDefault;
  
  bool value;
  if (regKey.QueryValue(valueName, value) != ERROR_SUCCESS)
    return valueDefault;
  
  return value;
}

bool CStartupInfo::Control(HANDLE pluginHandle, int command, void *param)
{
  return BOOLToBool(m_Data.Control(pluginHandle, command, param));
}

bool CStartupInfo::ControlRequestActivePanel(int command, void *param)
{
  return Control(INVALID_HANDLE_VALUE, command, param);
}

bool CStartupInfo::ControlGetActivePanelInfo(PanelInfo &panelInfo)
{
  return ControlRequestActivePanel(FCTL_GETPANELINFO, &panelInfo);
}

bool CStartupInfo::ControlSetSelection(const PanelInfo &panelInfo)
{
  return ControlRequestActivePanel(FCTL_SETSELECTION, (void *)&panelInfo);
}

bool CStartupInfo::ControlGetActivePanelCurrentItemInfo(
    PluginPanelItem &pluginPanelItem)
{
  PanelInfo panelInfo;
  if (!ControlGetActivePanelInfo(panelInfo))
    return false;
  if (panelInfo.ItemsNumber <= 0)
    throw "There are no items";
  pluginPanelItem = panelInfo.PanelItems[panelInfo.CurrentItem];
  return true;
}

bool CStartupInfo::ControlGetActivePanelSelectedOrCurrentItems(
    CObjectVector<PluginPanelItem> &pluginPanelItems)
{
  pluginPanelItems.Clear();
  PanelInfo panelInfo;
  if (!ControlGetActivePanelInfo(panelInfo))
    return false;
  if (panelInfo.ItemsNumber <= 0)
    throw "There are no items";
  if (panelInfo.SelectedItemsNumber == 0)
    pluginPanelItems.Add(panelInfo.PanelItems[panelInfo.CurrentItem]);
  else
    for (int i = 0; i < panelInfo.SelectedItemsNumber; i++)
      pluginPanelItems.Add(panelInfo.SelectedItems[i]);
  return true;
}

bool CStartupInfo::ControlClearPanelSelection()
{
  PanelInfo panelInfo;
  if (!ControlGetActivePanelInfo(panelInfo))
    return false;
  for (int i = 0; i < panelInfo.ItemsNumber; i++)
    panelInfo.PanelItems[i].Flags &= ~PPIF_SELECTED;
  return ControlSetSelection(panelInfo);
}

////////////////////////////////////////////////
// menu function

int CStartupInfo::Menu(
    int x,
    int y,
    int maxHeight,
    unsigned int flags,
    const char *title,
    const char *aBottom,
    const char *helpTopic,
    int *breakKeys,
    int *breakCode,
    struct FarMenuItem *items,
    int numItems)
{
  return m_Data.Menu(m_Data.ModuleNumber, x, y, maxHeight, flags, (char *)title,
      (char *)aBottom, (char *)helpTopic, breakKeys, breakCode, items, numItems);
}

int CStartupInfo::Menu(
    unsigned int flags,
    const char *title,
    const char *helpTopic,
    struct FarMenuItem *items,
    int numItems)
{
  return Menu(-1, -1, 0, flags, title, NULL, helpTopic, NULL,
      NULL, items, numItems);
}

int CStartupInfo::Menu(
    unsigned int flags,
    const char *title,
    const char *helpTopic,
    const AStringVector &items,
    int selectedItem)
{
  CRecordVector<FarMenuItem> farMenuItems;
  FOR_VECTOR (i, items)
  {
    FarMenuItem item;
    item.Checked = 0;
    item.Separator = 0;
    item.Selected = ((int)i == selectedItem);
    const AString reducedString (items[i].Left(ARRAY_SIZE(item.Text) - 1));
    MyStringCopy(item.Text, reducedString);
    farMenuItems.Add(item);
  }
  return Menu(flags, title, helpTopic, &farMenuItems.Front(), farMenuItems.Size());
}


//////////////////////////////////
// CScreenRestorer

CScreenRestorer::~CScreenRestorer()
{
  Restore();
}
void CScreenRestorer::Save()
{
  if (m_Saved)
    return;
  m_HANDLE = g_StartupInfo.SaveScreen();
  m_Saved = true;
}

void CScreenRestorer::Restore()
{
  if (m_Saved)
  {
    g_StartupInfo.RestoreScreen(m_HANDLE);
    m_Saved = false;
  }
};

int PrintErrorMessage(const char *message, unsigned code)
{
  AString s (message);
  s += " #";
  s.Add_UInt32((UInt32)code);
  return g_StartupInfo.ShowErrorMessage(s);
}

int PrintErrorMessage(const char *message, const char *text)
{
  return g_StartupInfo.ShowErrorMessage2(message, text);
}


void ReduceString(UString &s, unsigned size)
{
  if (s.Len() > size)
  {
    if (size > 5)
      size -= 5;
    s.Delete(size / 2, s.Len() - size);
    s.Insert(size / 2, L" ... ");
  }
}

int PrintErrorMessage(const char *message, const wchar_t *name, unsigned maxLen)
{
  UString s = name;
  ReduceString(s, maxLen);
  return PrintErrorMessage(message, UnicodeStringToMultiByte(s, CP_OEMCP));
}

int ShowSysErrorMessage(DWORD errorCode)
{
  UString message = NError::MyFormatMessage(errorCode);
  return g_StartupInfo.ShowErrorMessage(UnicodeStringToMultiByte(message, CP_OEMCP));
}

int ShowLastErrorMessage()
{
  return ShowSysErrorMessage(::GetLastError());
}

int ShowSysErrorMessage(DWORD errorCode, const wchar_t *name)
{
  const UString s = NError::MyFormatMessage(errorCode);
  return g_StartupInfo.ShowErrorMessage2(
      UnicodeStringToMultiByte(s, CP_OEMCP),
      UnicodeStringToMultiByte(name, CP_OEMCP));
}


bool WasEscPressed()
{
  #ifdef UNDER_CE
  return false;
  #else
  NConsole::CIn inConsole;
  HANDLE handle = ::GetStdHandle(STD_INPUT_HANDLE);
  if (handle == INVALID_HANDLE_VALUE)
    return true;
  inConsole.Attach(handle);
  for (;;)
  {
    DWORD numEvents;
    if (!inConsole.GetNumberOfEvents(numEvents))
      return true;
    if (numEvents == 0)
      return false;

    INPUT_RECORD event;
    if (!inConsole.ReadEvent(event, numEvents))
      return true;
    if (event.EventType == KEY_EVENT &&
        event.Event.KeyEvent.bKeyDown &&
        event.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE)
      return true;
  }
  #endif
}

}

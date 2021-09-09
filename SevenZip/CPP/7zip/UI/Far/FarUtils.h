// FarUtils.h

#ifndef __FAR_UTILS_H
#define __FAR_UTILS_H

#include "FarPlugin.h"

#include "../../../Windows/Registry.h"

namespace NFar {

namespace NFileOperationReturnCode
{
  enum EEnum
  {
    kInterruptedByUser = -1,
    kError = 0,
    kSuccess = 1
  };
}

namespace NEditorReturnCode
{
  enum EEnum
  {
    kOpenError = 0,
    kFileWasChanged = 1,
    kFileWasNotChanged = 2,
    kInterruptedByUser = 3
  };
}

struct CInitDialogItem
{
  DialogItemTypes Type;
  int X1,Y1,X2,Y2;
  bool Focus;
  bool Selected;
  unsigned int Flags; //FarDialogItemFlags Flags;
  bool DefaultButton;
  int DataMessageId;
  const char *DataString;
  const char *HistoryName;
  // void InitToFarDialogItem(struct FarDialogItem &anItemDest);
};

class CStartupInfo
{
  PluginStartupInfo m_Data;
  AString m_RegistryPath;

  CSysString GetFullKeyName(const char *keyName) const;
  LONG CreateRegKey(HKEY parentKey,
    const char *keyName, NWindows::NRegistry::CKey &destKey) const;
  LONG OpenRegKey(HKEY parentKey,
    const char *keyName, NWindows::NRegistry::CKey &destKey) const;

public:
  void Init(const PluginStartupInfo &pluginStartupInfo,
      const char *pluginNameForRegistry);
  const char *GetMsgString(int messageId);
  
  int ShowMessage(unsigned int flags, const char *helpTopic,
      const char **items, int numItems, int numButtons);
  int ShowWarningWithOk(const char **items, int numItems);
 
  void SetErrorTitle(AString &s);
  int ShowErrorMessage(const char *message);
  int ShowErrorMessage2(const char *m1, const char *m2);
  // int ShowMessageLines(const char *messageLines);
  int ShowMessage(int messageId);

  int ShowDialog(int X1, int Y1, int X2, int Y2,
      const char *helpTopic, struct FarDialogItem *items, int numItems);
  int ShowDialog(int sizeX, int sizeY,
      const char *helpTopic, struct FarDialogItem *items, int numItems);

  void InitDialogItems(const CInitDialogItem *srcItems,
      FarDialogItem *destItems, int numItems);
  
  HANDLE SaveScreen(int X1, int Y1, int X2, int Y2);
  HANDLE SaveScreen();
  void RestoreScreen(HANDLE handle);

  void SetRegKeyValue(HKEY parentKey, const char *keyName,
      const LPCTSTR valueName, LPCTSTR value) const;
  void SetRegKeyValue(HKEY hRoot, const char *keyName,
      const LPCTSTR valueName, UInt32 value) const;
  void SetRegKeyValue(HKEY hRoot, const char *keyName,
      const LPCTSTR valueName, bool value) const;

  CSysString QueryRegKeyValue(HKEY parentKey, const char *keyName,
      LPCTSTR valueName, const CSysString &valueDefault) const;

  UInt32 QueryRegKeyValue(HKEY parentKey, const char *keyName,
      LPCTSTR valueName, UInt32 valueDefault) const;

  bool QueryRegKeyValue(HKEY parentKey, const char *keyName,
      LPCTSTR valueName, bool valueDefault) const;

  bool Control(HANDLE plugin, int command, void *param);
  bool ControlRequestActivePanel(int command, void *param);
  bool ControlGetActivePanelInfo(PanelInfo &panelInfo);
  bool ControlSetSelection(const PanelInfo &panelInfo);
  bool ControlGetActivePanelCurrentItemInfo(PluginPanelItem &pluginPanelItem);
  bool ControlGetActivePanelSelectedOrCurrentItems(
      CObjectVector<PluginPanelItem> &pluginPanelItems);

  bool ControlClearPanelSelection();

  int Menu(
      int x,
      int y,
      int maxHeight,
      unsigned int flags,
      const char *title,
      const char *aBottom,
      const char *helpTopic,
      int *breakKeys,
      int *breakCode,
      FarMenuItem *items,
      int numItems);
  int Menu(
      unsigned int flags,
      const char *title,
      const char *helpTopic,
      FarMenuItem *items,
      int numItems);

  int Menu(
      unsigned int flags,
      const char *title,
      const char *helpTopic,
      const AStringVector &items,
      int selectedItem);

  int Editor(const char *fileName, const char *title,
      int X1, int Y1, int X2, int Y2, DWORD flags, int startLine, int startChar)
      { return m_Data.Editor((char *)fileName, (char *)title, X1, Y1, X2, Y2,
        flags, startLine, startChar); }
  int Editor(const char *fileName)
      { return Editor(fileName, NULL, 0, 0, -1, -1, 0, -1, -1); }

  int Viewer(const char *fileName, const char *title,
      int X1, int Y1, int X2, int Y2, DWORD flags)
      { return m_Data.Viewer((char *)fileName, (char *)title, X1, Y1, X2, Y2, flags); }
  int Viewer(const char *fileName)
      { return Viewer(fileName, NULL, 0, 0, -1, -1, VF_NONMODAL); }

};

class CScreenRestorer
{
  bool m_Saved;
  HANDLE m_HANDLE;
public:
  CScreenRestorer(): m_Saved(false){};
  ~CScreenRestorer();
  void Save();
  void Restore();
};


extern CStartupInfo g_StartupInfo;


int PrintErrorMessage(const char *message, unsigned code);
int PrintErrorMessage(const char *message, const char *text);
int PrintErrorMessage(const char *message, const wchar_t *name, unsigned maxLen = 70);

#define  MY_TRY_BEGIN  try {

#define  MY_TRY_END1(x)  }\
  catch(unsigned n) { PrintErrorMessage(x, n);  return; }\
  catch(const CSysString &s) { PrintErrorMessage(x, s); return; }\
  catch(const char *s) { PrintErrorMessage(x, s); return; }\
  catch(...) { g_StartupInfo.ShowErrorMessage(x);  return; }

#define  MY_TRY_END2(x, y)  }\
  catch(unsigned n) { PrintErrorMessage(x, n); return y; }\
  catch(const AString &s) { PrintErrorMessage(x, s); return y; }\
  catch(const char *s) { PrintErrorMessage(x, s); return y; }\
  catch(const UString &s) { PrintErrorMessage(x, s); return y; }\
  catch(const wchar_t *s) { PrintErrorMessage(x, s); return y; }\
  catch(...) { g_StartupInfo.ShowErrorMessage(x); return y; }

int ShowSysErrorMessage(DWORD errorCode);
int ShowSysErrorMessage(DWORD errorCode, const wchar_t *name);
int ShowLastErrorMessage();

bool WasEscPressed();

void ReduceString(UString &s, unsigned size);

}

#endif

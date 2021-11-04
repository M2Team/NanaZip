// ContextMenu.h

#ifndef __CONTEXT_MENU_H
#define __CONTEXT_MENU_H

#include "../../../Common/MyWindows.h"

#include <ShlObj.h>

#include "MyExplorerCommand.h"

#include "../../../Common/MyString.h"

#include "../FileManager/MyCom2.h"

enum ECtxCommandType
{
  CtxCommandType_Normal,
  CtxCommandType_OpenRoot,
  CtxCommandType_OpenChild,
  CtxCommandType_CrcRoot,
  CtxCommandType_CrcChild,
};
   

class CZipContextMenu:
  public IContextMenu,
  public IShellExtInit,
  public IExplorerCommand,
  public IEnumExplorerCommand,
  public CMyUnknownImp
{
public:

  enum ECommandInternalID
  {
    kCommandNULL,
    kOpen,
    kExtract,
    kExtractHere,
    kExtractTo,
    kTest,
    kCompress,
    kCompressEmail,
    kCompressTo7z,
    kCompressTo7zEmail,
    kCompressToZip,
    kCompressToZipEmail,
    kHash_CRC32,
    kHash_CRC64,
    kHash_SHA1,
    kHash_SHA256,
    kHash_All,
    kHash_Generate_SHA256,
    kHash_TestArc
  };
  
  MY_UNKNOWN_IMP4_MT(
      IContextMenu,
      IShellExtInit,
      IExplorerCommand,
      IEnumExplorerCommand
      )

  // IShellExtInit
  STDMETHOD(Initialize)(LPCITEMIDLIST pidlFolder, LPDATAOBJECT dataObject, HKEY hkeyProgID);

  // IContextMenu
  STDMETHOD(QueryContextMenu)(HMENU hmenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags);
  STDMETHOD(InvokeCommand)(LPCMINVOKECOMMANDINFO lpici);
  STDMETHOD(GetCommandString)(UINT_PTR idCmd, UINT uType, UINT *pwReserved, LPSTR pszName, UINT cchMax);

  HRESULT InitContextMenu(const wchar_t *folder, const wchar_t * const *names, unsigned numFiles);

  void LoadItems(IShellItemArray *psiItemArray);

  // IExplorerCommand
  STDMETHOD (GetTitle)   (IShellItemArray *psiItemArray, LPWSTR *ppszName);
  STDMETHOD (GetIcon)    (IShellItemArray *psiItemArray, LPWSTR *ppszIcon);
  STDMETHOD (GetToolTip) (IShellItemArray *psiItemArray, LPWSTR *ppszInfotip);
  STDMETHOD (GetCanonicalName) (GUID *pguidCommandName);
  STDMETHOD (GetState)   (IShellItemArray *psiItemArray, BOOL fOkToBeSlow, EXPCMDSTATE *pCmdState);
  STDMETHOD (Invoke)     (IShellItemArray *psiItemArray, IBindCtx *pbc);
  STDMETHOD (GetFlags)   (EXPCMDFLAGS *pFlags);
  STDMETHOD (EnumSubCommands) (IEnumExplorerCommand **ppEnum);

  // IEnumExplorerCommand
  STDMETHOD (Next) (ULONG celt, IExplorerCommand **pUICommand, ULONG *pceltFetched);
  STDMETHOD (Skip) (ULONG celt);
  STDMETHOD (Reset) (void);
  STDMETHOD (Clone) (IEnumExplorerCommand **ppenum);

  CZipContextMenu();
  ~CZipContextMenu();

  struct CCommandMapItem
  {
    ECommandInternalID CommandInternalID;
    UString Verb;
    UString UserString;
    // UString HelpString;
    UString Folder;
    UString ArcName;
    UString ArcType;
    bool IsPopup;
    ECtxCommandType CtxCommandType;

    CCommandMapItem():
        IsPopup(false),
        CtxCommandType(CtxCommandType_Normal)
        {}

    bool IsSubMenu() const
    {
      return
          CtxCommandType == CtxCommandType_CrcRoot ||
          CtxCommandType == CtxCommandType_OpenRoot;
    }
  };

private:

  bool _isMenuForFM;
  UStringVector _fileNames;
  bool _dropMode;
  UString _dropPath;
  CObjectVector<CCommandMapItem> _commandMap;
  CObjectVector<CCommandMapItem> _commandMap_Cur;

  HBITMAP _bitmap;
  CBoolPair _elimDup;

  bool IsSeparator;
  bool IsRoot;
  CObjectVector< CMyComPtr<IExplorerCommand> > SubCommands;
  ULONG CurrentSubCommand;

  void Set_UserString_in_LastCommand(const UString &s)
  {
    _commandMap.Back().UserString = s;
  }

  HRESULT GetFileNames(LPDATAOBJECT dataObject, UStringVector &fileNames);
  int FindVerb(const UString &verb);
  void FillCommand(ECommandInternalID id, UString &mainString, CCommandMapItem &cmi);
  void AddCommand(ECommandInternalID id, UString &mainString, CCommandMapItem &cmi);
  void AddMapItem_ForSubMenu(const char *ver);

  HRESULT InvokeCommandCommon(const CCommandMapItem &cmi);
};

#endif

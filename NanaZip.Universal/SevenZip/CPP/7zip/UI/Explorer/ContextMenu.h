// ContextMenu.h

#ifndef ZIP7_INC_CONTEXT_MENU_H
#define ZIP7_INC_CONTEXT_MENU_H

#include "../../../Windows/Shell.h"

#include "MyExplorerCommand.h"

#include "../FileManager/MyCom2.h"

#ifdef CMF_EXTENDEDVERBS
#define Z7_WIN_CMF_EXTENDEDVERBS  CMF_EXTENDEDVERBS
#else
#define Z7_WIN_CMF_EXTENDEDVERBS  0x00000100
#endif

enum enum_CtxCommandType
{
  CtxCommandType_Normal,
  CtxCommandType_OpenRoot,
  CtxCommandType_OpenChild,
  CtxCommandType_CrcRoot,
  CtxCommandType_CrcChild
};


class CZipContextMenu Z7_final:
  public IContextMenu,
  public IShellExtInit,
  public IExplorerCommand,
  public IEnumExplorerCommand,
  public CMyUnknownImp
{
  Z7_COM_UNKNOWN_IMP_4_MT(
      IContextMenu,
      IShellExtInit,
      IExplorerCommand,
      IEnumExplorerCommand
      )

  // IShellExtInit
  STDMETHOD(Initialize)(LPCITEMIDLIST pidlFolder, LPDATAOBJECT dataObject, HKEY hkeyProgID) Z7_override;

  // IContextMenu
  STDMETHOD(QueryContextMenu)(HMENU hmenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags) Z7_override;
  STDMETHOD(InvokeCommand)(LPCMINVOKECOMMANDINFO lpici) Z7_override;
  STDMETHOD(GetCommandString)(
      #ifdef Z7_OLD_WIN_SDK
        UINT
      #else
        UINT_PTR
      #endif
      idCmd, UINT uType, UINT *pwReserved, LPSTR pszName, UINT cchMax) Z7_override;

  // IExplorerCommand
  STDMETHOD (GetTitle)   (IShellItemArray *psiItemArray, LPWSTR *ppszName) Z7_override;
  STDMETHOD (GetIcon)    (IShellItemArray *psiItemArray, LPWSTR *ppszIcon) Z7_override;
  STDMETHOD (GetToolTip) (IShellItemArray *psiItemArray, LPWSTR *ppszInfotip) Z7_override;
  STDMETHOD (GetCanonicalName) (GUID *pguidCommandName) Z7_override;
  STDMETHOD (GetState)   (IShellItemArray *psiItemArray, BOOL fOkToBeSlow, EXPCMDSTATE *pCmdState) Z7_override;
  STDMETHOD (Invoke)     (IShellItemArray *psiItemArray, IBindCtx *pbc) Z7_override;
  STDMETHOD (GetFlags)   (EXPCMDFLAGS *pFlags) Z7_override;
  STDMETHOD (EnumSubCommands) (IEnumExplorerCommand **ppEnum) Z7_override;

  // IEnumExplorerCommand
  STDMETHOD (Next) (ULONG celt, IExplorerCommand **pUICommand, ULONG *pceltFetched) Z7_override;
  STDMETHOD (Skip) (ULONG celt) Z7_override;
  STDMETHOD (Reset) (void) Z7_override;
  STDMETHOD (Clone) (IEnumExplorerCommand **ppenum) Z7_override;

public:

  enum enum_CommandInternalID
  {
    kCommandNULL,
    kOpen,
    kExtract,
    kExtractHere,
    // **************** NanaZip Modification Start ****************
    kExtractHereSmart,
    // **************** NanaZip Modification End ****************
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
    kHash_XXH64,
    kHash_MD5,
    kHash_SHA1,
    kHash_SHA256,
    kHash_SHA384,
    kHash_SHA512,
    kHash_SHA3_256,
    kHash_BLAKE2SP,
    kHash_All,
    kHash_Generate_SHA256,
    kHash_TestArc
  };

public:
  void Init_For_7zFM()
  {
    // _isMenuForFM = true;
    // _fileNames_WereReduced = false;
  }

  void LoadItems(IShellItemArray *psiItemArray);

  CZipContextMenu();
  ~CZipContextMenu();

  struct CCommandMapItem
  {
    enum_CommandInternalID CommandInternalID;
    UString Verb;
    UString UserString;
    // UString HelpString;
    UString Folder;
    UString ArcName;
    UString ArcType;
    bool IsPopup;
    enum_CtxCommandType CtxCommandType;

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

  UStringVector _fileNames;
  NWindows::NShell::CFileAttribs _attribs;

private:
  bool _isMenuForFM;
  bool _fileNames_WereReduced;  // = true, if only first 16 items were used in QueryContextMenu()
  bool _dropMode;
  UString _dropPath;
  CObjectVector<CCommandMapItem> _commandMap;
  CObjectVector<CCommandMapItem> _commandMap_Cur;

  HBITMAP _bitmap;
  UInt32 _writeZone;
  CBoolPair _elimDup;

  bool IsSeparator;
  bool IsRoot;
  CObjectVector< CMyComPtr<IExplorerCommand> > SubCommands;
  unsigned CurrentSubCommand;

  void Set_UserString_in_LastCommand(const UString &s)
  {
    _commandMap.Back().UserString = s;
  }

  int FindVerb(const UString &verb) const;
  void FillCommand(enum_CommandInternalID id, UString &mainString, CCommandMapItem &cmi) const;
  void AddCommand(enum_CommandInternalID id, UString &mainString, CCommandMapItem &cmi);
  void AddMapItem_ForSubMenu(const char *ver);

  HRESULT InvokeCommandCommon(const CCommandMapItem &cmi);
};

#endif

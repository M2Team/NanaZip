// ContextMenu.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/COM.h"
#include "../../../Windows/DLL.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/MemoryGlobal.h"
#include "../../../Windows/Menu.h"
#include "../../../Windows/ProcessUtils.h"
#include "../../../Windows/Shell.h"

#include "../Common/ArchiveName.h"
#include "../Common/CompressCall.h"
#include "../Common/ExtractingFilePath.h"
#include "../Common/ZipRegistry.h"

#include "../FileManager/FormatUtils.h"

#ifdef LANG
#include "../FileManager/LangUtils.h"
#endif

#include "ContextMenu.h"
#include "ContextMenuFlags.h"
#include "MyMessages.h"

#include "resource.h"

// #define SHOW_DEBUG_CTX_MENU

#ifdef SHOW_DEBUG_CTX_MENU
#include <stdio.h>
#endif

using namespace NWindows;
using namespace NFile;
using namespace NDir;

#ifndef UNDER_CE
#define EMAIL_SUPPORT 1
#endif

extern LONG g_DllRefCount;

#ifdef _WIN32
extern HINSTANCE g_hInstance;
#endif
    
CZipContextMenu::CZipContextMenu():
   _isMenuForFM(false),
   _bitmap(NULL)
{
  InterlockedIncrement(&g_DllRefCount);
  _bitmap = ::LoadBitmap(g_hInstance, MAKEINTRESOURCE(IDB_MENU_LOGO));
}

CZipContextMenu::~CZipContextMenu()
{
  if (_bitmap != NULL)
    DeleteObject(_bitmap);
  InterlockedDecrement(&g_DllRefCount);
}

HRESULT CZipContextMenu::GetFileNames(LPDATAOBJECT dataObject, UStringVector &fileNames)
{
  fileNames.Clear();
  if (!dataObject)
    return E_INVALIDARG;

  #ifndef UNDER_CE
  
  FORMATETC fmte = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  NCOM::CStgMedium stgMedium;
  HRESULT result = dataObject->GetData(&fmte, &stgMedium);
  if (result != S_OK)
    return result;
  stgMedium._mustBeReleased = true;

  NShell::CDrop drop(false);
  NMemory::CGlobalLock globalLock(stgMedium->hGlobal);
  drop.Attach((HDROP)globalLock.GetPointer());
  drop.QueryFileNames(fileNames);
  
  #endif

  return S_OK;
}

// IShellExtInit

STDMETHODIMP CZipContextMenu::Initialize(LPCITEMIDLIST pidlFolder, LPDATAOBJECT dataObject, HKEY /* hkeyProgID */)
{
  // OutputDebugString(TEXT("::Initialize\r\n"));
  _dropMode = false;
  _dropPath.Empty();
  if (pidlFolder != 0)
  {
    #ifndef UNDER_CE
    if (NShell::GetPathFromIDList(pidlFolder, _dropPath))
    {
      // OutputDebugString(path);
      // OutputDebugString(TEXT("\r\n"));
      NName::NormalizeDirPathPrefix(_dropPath);
      _dropMode = !_dropPath.IsEmpty();
    }
    else
    #endif
      _dropPath.Empty();
  }

  /*
  m_IsFolder = false;
  if (pidlFolder == 0)
  */
  // pidlFolder is NULL :(
  return GetFileNames(dataObject, _fileNames);
}

HRESULT CZipContextMenu::InitContextMenu(const wchar_t * /* folder */, const wchar_t * const *names, unsigned numFiles)
{
  _isMenuForFM = true;
  _fileNames.Clear();
  for (UInt32 i = 0; i < numFiles; i++)
  {
    // MessageBoxW(0, names[i], NULL, 0);
    // OutputDebugStringW(names[i]);
    _fileNames.Add(names[i]);
  }
  _dropMode = false;
  return S_OK;
}


/////////////////////////////
// IContextMenu

static LPCSTR const kMainVerb = "SevenZip";
static LPCSTR const kOpenCascadedVerb = "SevenZip.OpenWithType.";
static LPCSTR const kCheckSumCascadedVerb = "SevenZip.Checksum";


struct CContextMenuCommand
{
  UInt32 flag;
  CZipContextMenu::ECommandInternalID CommandInternalID;
  LPCSTR Verb;
  UINT ResourceID;
};

#define CMD_REC(cns, verb, ids)  { NContextMenuFlags::cns, CZipContextMenu::cns, verb, ids }

static const CContextMenuCommand g_Commands[] =
{
  CMD_REC( kOpen,        "Open",        IDS_CONTEXT_OPEN),
  CMD_REC( kExtract,     "Extract",     IDS_CONTEXT_EXTRACT),
  CMD_REC( kExtractHere, "ExtractHere", IDS_CONTEXT_EXTRACT_HERE),
  CMD_REC( kExtractTo,   "ExtractTo",   IDS_CONTEXT_EXTRACT_TO),
  CMD_REC( kTest,        "Test",        IDS_CONTEXT_TEST),
  CMD_REC( kCompress,           "Compress",           IDS_CONTEXT_COMPRESS),
  CMD_REC( kCompressEmail,      "CompressEmail",      IDS_CONTEXT_COMPRESS_EMAIL),
  CMD_REC( kCompressTo7z,       "CompressTo7z",       IDS_CONTEXT_COMPRESS_TO),
  CMD_REC( kCompressTo7zEmail,  "CompressTo7zEmail",  IDS_CONTEXT_COMPRESS_TO_EMAIL),
  CMD_REC( kCompressToZip,      "CompressToZip",      IDS_CONTEXT_COMPRESS_TO),
  CMD_REC( kCompressToZipEmail, "CompressToZipEmail", IDS_CONTEXT_COMPRESS_TO_EMAIL)
};


struct CHashCommand
{
  CZipContextMenu::ECommandInternalID CommandInternalID;
  LPCSTR UserName;
  LPCSTR MethodName;
};

static const CHashCommand g_HashCommands[] =
{
  { CZipContextMenu::kHash_CRC32,  "CRC-32",  "CRC32" },
  { CZipContextMenu::kHash_CRC64,  "CRC-64",  "CRC64" },
  { CZipContextMenu::kHash_SHA1,   "SHA-1",   "SHA1" },
  { CZipContextMenu::kHash_SHA256, "SHA-256", "SHA256" },
  { CZipContextMenu::kHash_All,    "*",       "*" }
};


static int FindCommand(CZipContextMenu::ECommandInternalID &id)
{
  for (unsigned i = 0; i < ARRAY_SIZE(g_Commands); i++)
    if (g_Commands[i].CommandInternalID == id)
      return i;
  return -1;
}


void CZipContextMenu::FillCommand(ECommandInternalID id, UString &mainString, CCommandMapItem &cmi)
{
  mainString.Empty();
  int i = FindCommand(id);
  if (i < 0)
    throw 201908;
  const CContextMenuCommand &command = g_Commands[(unsigned)i];
  cmi.CommandInternalID = command.CommandInternalID;
  cmi.Verb = kMainVerb;
  cmi.Verb += command.Verb;
  // cmi.HelpString = cmi.Verb;
  LangString(command.ResourceID, mainString);
  // return true;
}

  
void CZipContextMenu::AddCommand(ECommandInternalID id, UString &mainString, CCommandMapItem &cmi)
{
  FillCommand(id, mainString, cmi);
  _commandMap.Add(cmi);
}


static void MyInsertMenu(CMenu &menu, int pos, UINT id, const UString &s, HBITMAP bitmap)
{
  CMenuItem mi;
  mi.fType = MFT_STRING;
  mi.fMask = MIIM_TYPE | MIIM_ID;
  if (bitmap)
    mi.fMask |= MIIM_CHECKMARKS;
  mi.wID = id;
  mi.StringValue = s;
  mi.hbmpUnchecked = bitmap;
  // mi.hbmpChecked = bitmap; // do we need hbmpChecked ???
  if (!menu.InsertItem(pos, true, mi))
    throw 20190816;

  // SetMenuItemBitmaps also works
  // ::SetMenuItemBitmaps(menu, pos, MF_BYPOSITION, bitmap, NULL);
}


static void MyAddSubMenu(
    CObjectVector<CZipContextMenu::CCommandMapItem> &_commandMap,
    const char *verb,
    CMenu &menu, int pos, UINT id, const UString &s, HMENU hSubMenu, HBITMAP bitmap)
{
  CZipContextMenu::CCommandMapItem cmi;
  cmi.CommandInternalID = CZipContextMenu::kCommandNULL;
  cmi.Verb = verb;
  // cmi.HelpString = verb;
  _commandMap.Add(cmi);

  CMenuItem mi;
  mi.fType = MFT_STRING;
  mi.fMask = MIIM_SUBMENU | MIIM_TYPE | MIIM_ID;
  if (bitmap)
    mi.fMask |= MIIM_CHECKMARKS;
  mi.wID = id;
  mi.hSubMenu = hSubMenu;
  mi.hbmpUnchecked = bitmap;
  
  mi.StringValue = s;
  if (!menu.InsertItem(pos, true, mi))
    throw 20190817;
}


static const char * const kArcExts[] =
{
    "7z"
  , "bz2"
  , "gz"
  , "rar"
  , "zip"
};

static bool IsItArcExt(const UString &ext)
{
  for (unsigned i = 0; i < ARRAY_SIZE(kArcExts); i++)
    if (ext.IsEqualTo_Ascii_NoCase(kArcExts[i]))
      return true;
  return false;
}

UString GetSubFolderNameForExtract(const UString &arcName);
UString GetSubFolderNameForExtract(const UString &arcName)
{
  int dotPos = arcName.ReverseFind_Dot();
  if (dotPos < 0)
    return Get_Correct_FsFile_Name(arcName) + L'~';

  const UString ext = arcName.Ptr(dotPos + 1);
  UString res = arcName.Left(dotPos);
  res.TrimRight();
  dotPos = res.ReverseFind_Dot();
  if (dotPos > 0)
  {
    const UString ext2 = res.Ptr(dotPos + 1);
    if ((ext.IsEqualTo_Ascii_NoCase("001") && IsItArcExt(ext2))
        || (ext.IsEqualTo_Ascii_NoCase("rar") &&
          (  ext2.IsEqualTo_Ascii_NoCase("part001")
          || ext2.IsEqualTo_Ascii_NoCase("part01")
          || ext2.IsEqualTo_Ascii_NoCase("part1"))))
      res.DeleteFrom(dotPos);
    res.TrimRight();
  }
  return Get_Correct_FsFile_Name(res);
}

static void ReduceString(UString &s)
{
  const unsigned kMaxSize = 64;
  if (s.Len() <= kMaxSize)
    return;
  s.Delete(kMaxSize / 2, s.Len() - kMaxSize);
  s.Insert(kMaxSize / 2, L" ... ");
}

static UString GetQuotedReducedString(const UString &s)
{
  UString s2 = s;
  ReduceString(s2);
  s2.Replace(L"&", L"&&");
  return GetQuotedString(s2);
}

static void MyFormatNew_ReducedName(UString &s, const UString &name)
{
  s = MyFormatNew(s, GetQuotedReducedString(name));
}

static const char * const kExtractExludeExtensions =
  " 3gp"
  " aac ans ape asc asm asp aspx avi awk"
  " bas bat bmp"
  " c cs cls clw cmd cpp csproj css ctl cxx"
  " def dep dlg dsp dsw"
  " eps"
  " f f77 f90 f95 fla flac frm"
  " gif"
  " h hpp hta htm html hxx"
  " ico idl inc ini inl"
  " java jpeg jpg js"
  " la lnk log"
  " mak manifest wmv mov mp3 mp4 mpe mpeg mpg m4a"
  " ofr ogg"
  " pac pas pdf php php3 php4 php5 phptml pl pm png ps py pyo"
  " ra rb rc reg rka rm rtf"
  " sed sh shn shtml sln sql srt swa"
  " tcl tex tiff tta txt"
  " vb vcproj vbs"
  " wav wma wv"
  " xml xsd xsl xslt"
  " ";

/*
static const char * const kNoOpenAsExtensions =
  " 7z arj bz2 cab chm cpio flv gz lha lzh lzma rar swm tar tbz2 tgz wim xar xz z zip ";
*/

static const char * const kOpenTypes[] =
{
    ""
  , "*"
  , "#"
  , "#:e"
  // , "#:a"
  , "7z"
  , "zip"
  , "cab"
  , "rar"
};

static bool FindExt(const char *p, const FString &name)
{
  int dotPos = name.ReverseFind_Dot();
  if (dotPos < 0 || dotPos == (int)name.Len() - 1)
    return false;

  AString s;
  
  for (unsigned pos = dotPos + 1;; pos++)
  {
    wchar_t c = name[pos];
    if (c == 0)
      break;
    if (c >= 0x80)
      return false;
    s += (char)MyCharLower_Ascii((char)c);
  }
  
  for (unsigned i = 0; p[i] != 0;)
  {
    unsigned j;
    for (j = i; p[j] != ' '; j++);
    if (s.Len() == j - i && memcmp(p + i, (const char *)s, s.Len()) == 0)
      return true;
    i = j + 1;
  }
  
  return false;
}

static bool DoNeedExtract(const FString &name)
{
  return !FindExt(kExtractExludeExtensions, name);
}

// we must use diferent Verbs for Popup subMenu.
void CZipContextMenu::AddMapItem_ForSubMenu(const char *verb)
{
  CCommandMapItem cmi;
  cmi.CommandInternalID = kCommandNULL;
  cmi.Verb = verb;
  // cmi.HelpString = verb;
  _commandMap.Add(cmi);
}


static HRESULT RETURN_WIN32_LastError_AS_HRESULT()
{
  DWORD lastError = ::GetLastError();
  if (lastError == 0)
    return E_FAIL;
  return HRESULT_FROM_WIN32(lastError);
}


/*
  we add CCommandMapItem to _commandMap for each new Mene ID.
  so then we use _commandMap[offset].
  That way we can execute commands that have menu item.
  Another non-implemented way:
    We can return the number off all possible commnad in QueryContextMenu().
    so the caller could call InvokeCommand() via string verb aven
    without using menu items.
*/
    


STDMETHODIMP CZipContextMenu::QueryContextMenu(HMENU hMenu, UINT indexMenu,
      UINT commandIDFirst, UINT commandIDLast, UINT flags)
{
  COM_TRY_BEGIN
  try {

  #ifdef SHOW_DEBUG_CTX_MENU
  { char s[256]; sprintf(s, "QueryContextMenu: index=%d first=%d last=%d flags=%x _files=%d",
    indexMenu, commandIDFirst, commandIDLast, flags, _fileNames.Size()); OutputDebugStringA(s); }

  /*
  for (UInt32 i = 0; i < _fileNames.Size(); i++)
  {
    OutputDebugStringW(_fileNames[i]);
  }
  */
  #endif

  LoadLangOneTime();
  
  if (_fileNames.Size() == 0)
  {
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
    // return E_INVALIDARG;
  }

  if (commandIDFirst > commandIDLast)
    return E_INVALIDARG;


  UINT currentCommandID = commandIDFirst;
  
  if ((flags & 0x000F) != CMF_NORMAL
      && (flags & CMF_VERBSONLY) == 0
      && (flags & CMF_EXPLORE) == 0)
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, currentCommandID - commandIDFirst);
  // return MAKE_HRESULT(SEVERITY_SUCCESS, 0, currentCommandID);
  // 19.01 : we changed from (currentCommandID) to (currentCommandID - commandIDFirst)
  // why it was so before?

  _commandMap.Clear();

  CMenu popupMenu;
  CMenuDestroyer menuDestroyer;

  CContextMenuInfo ci;
  ci.Load();

  _elimDup = ci.ElimDup;

  HBITMAP bitmap = NULL;
  if (ci.MenuIcons.Val)
    bitmap = _bitmap;

  UINT subIndex = indexMenu;
  
  if (ci.Cascaded.Val)
  {
    if (!popupMenu.CreatePopup())
      return RETURN_WIN32_LastError_AS_HRESULT();
    menuDestroyer.Attach(popupMenu);

    /* 9.31: we commented the following code. Probably we don't need.
    Check more systems. Maybe it was for old Windows? */
    /*
    AddMapItem_ForSubMenu();
    currentCommandID++;
    */
    subIndex = 0;
  }
  else
  {
    popupMenu.Attach(hMenu);
    CMenuItem mi;
    mi.fType = MFT_SEPARATOR;
    mi.fMask = MIIM_TYPE;
    popupMenu.InsertItem(subIndex++, true, mi);
  }

  UInt32 contextMenuFlags = ci.Flags;

  NFind::CFileInfo fi0;
  FString folderPrefix;
  
  if (_fileNames.Size() > 0)
  {
    const UString &fileName = _fileNames.Front();

    #if defined(_WIN32) && !defined(UNDER_CE)
    if (NName::IsDevicePath(us2fs(fileName)))
    {
      // CFileInfo::Find can be slow for device files. So we don't call it.
      // we need only name here.
      fi0.Name = us2fs(fileName.Ptr(NName::kDevicePathPrefixSize)); // change it 4 - must be constant
      folderPrefix =
        #ifdef UNDER_CE
          "\\";
        #else
          "C:\\";
        #endif
    }
    else
    #endif
    {
      if (!fi0.Find(us2fs(fileName)))
      {
        throw 20190820;
        // return RETURN_WIN32_LastError_AS_HRESULT();
      }
      GetOnlyDirPrefix(us2fs(fileName), folderPrefix);
    }
  }

  UString mainString;
  
  if (_fileNames.Size() == 1 && currentCommandID + 14 <= commandIDLast)
  {
    if (!fi0.IsDir() && DoNeedExtract(fi0.Name))
    {
      // Open
      bool thereIsMainOpenItem = ((contextMenuFlags & NContextMenuFlags::kOpen) != 0);
      if (thereIsMainOpenItem)
      {
        CCommandMapItem cmi;
        AddCommand(kOpen, mainString, cmi);
        MyInsertMenu(popupMenu, subIndex++, currentCommandID++, mainString, bitmap);
      }
      if ((contextMenuFlags & NContextMenuFlags::kOpenAs) != 0
          // && (!thereIsMainOpenItem || !FindExt(kNoOpenAsExtensions, fi0.Name))
          )
      {
        CMenu subMenu;
        if (subMenu.CreatePopup())
        {
          MyAddSubMenu(_commandMap, kOpenCascadedVerb, popupMenu, subIndex++, currentCommandID++, LangString(IDS_CONTEXT_OPEN), subMenu, bitmap);
          
          UINT subIndex2 = 0;
          for (unsigned i = (thereIsMainOpenItem ? 1 : 0); i < ARRAY_SIZE(kOpenTypes); i++)
          {
            CCommandMapItem cmi;
            if (i == 0)
              FillCommand(kOpen, mainString, cmi);
            else
            {
              mainString = kOpenTypes[i];
              cmi.CommandInternalID = kOpen;
              cmi.Verb = kMainVerb;
              cmi.Verb += ".Open.";
              cmi.Verb += mainString;
              // cmi.HelpString = cmi.Verb;
              cmi.ArcType = mainString;
            }
            _commandMap.Add(cmi);
            MyInsertMenu(subMenu, subIndex2++, currentCommandID++, mainString, bitmap);
          }

          subMenu.Detach();
        }
      }
    }
  }

  if (_fileNames.Size() > 0 && currentCommandID + 10 <= commandIDLast)
  {
    bool needExtract = (!fi0.IsDir() && DoNeedExtract(fi0.Name));
    
    if (!needExtract)
    {
      for (unsigned i = 1; i < _fileNames.Size(); i++)
      {
        NFind::CFileInfo fi;
        if (!fi.Find(us2fs(_fileNames[i])))
        {
          throw 20190821;
          // return RETURN_WIN32_LastError_AS_HRESULT();
        }
        if (!fi.IsDir() && DoNeedExtract(fi.Name))
        {
          needExtract = true;
          break;
        }
      }
    }
    
    // const UString &fileName = _fileNames.Front();
    
    if (needExtract)
    {
      {
        UString baseFolder = fs2us(folderPrefix);
        if (_dropMode)
          baseFolder = _dropPath;
    
        UString specFolder ('*');
        if (_fileNames.Size() == 1)
          specFolder = GetSubFolderNameForExtract(fs2us(fi0.Name));
        specFolder.Add_PathSepar();

        if ((contextMenuFlags & NContextMenuFlags::kExtract) != 0)
        {
          // Extract
          CCommandMapItem cmi;
          cmi.Folder = baseFolder + specFolder;
          AddCommand(kExtract, mainString, cmi);
          MyInsertMenu(popupMenu, subIndex++, currentCommandID++, mainString, bitmap);
        }

        if ((contextMenuFlags & NContextMenuFlags::kExtractHere) != 0)
        {
          // Extract Here
          CCommandMapItem cmi;
          cmi.Folder = baseFolder;
          AddCommand(kExtractHere, mainString, cmi);
          MyInsertMenu(popupMenu, subIndex++, currentCommandID++, mainString, bitmap);
        }

        if ((contextMenuFlags & NContextMenuFlags::kExtractTo) != 0)
        {
          // Extract To
          CCommandMapItem cmi;
          UString s;
          cmi.Folder = baseFolder + specFolder;
          AddCommand(kExtractTo, s, cmi);
          MyFormatNew_ReducedName(s, specFolder);
          MyInsertMenu(popupMenu, subIndex++, currentCommandID++, s, bitmap);
        }
      }

      if ((contextMenuFlags & NContextMenuFlags::kTest) != 0)
      {
        // Test
        CCommandMapItem cmi;
        AddCommand(kTest, mainString, cmi);
        MyInsertMenu(popupMenu, subIndex++, currentCommandID++, mainString, bitmap);
      }
    }
    
    const UString arcName = CreateArchiveName(_fileNames, _fileNames.Size() == 1 ? &fi0 : NULL);

    UString arcName7z = arcName;
    arcName7z += ".7z";
    UString arcNameZip = arcName;
    arcNameZip += ".zip";

    // Compress
    if ((contextMenuFlags & NContextMenuFlags::kCompress) != 0)
    {
      CCommandMapItem cmi;
      if (_dropMode)
        cmi.Folder = _dropPath;
      else
        cmi.Folder = fs2us(folderPrefix);
      cmi.ArcName = arcName;
      AddCommand(kCompress, mainString, cmi);
      MyInsertMenu(popupMenu, subIndex++, currentCommandID++, mainString, bitmap);
    }

    #ifdef EMAIL_SUPPORT
    // CompressEmail
    if ((contextMenuFlags & NContextMenuFlags::kCompressEmail) != 0 && !_dropMode)
    {
      CCommandMapItem cmi;
      cmi.ArcName = arcName;
      AddCommand(kCompressEmail, mainString, cmi);
      MyInsertMenu(popupMenu, subIndex++, currentCommandID++, mainString, bitmap);
    }
    #endif

    // CompressTo7z
    if (contextMenuFlags & NContextMenuFlags::kCompressTo7z &&
        !arcName7z.IsEqualTo_NoCase(fs2us(fi0.Name)))
    {
      CCommandMapItem cmi;
      UString s;
      if (_dropMode)
        cmi.Folder = _dropPath;
      else
        cmi.Folder = fs2us(folderPrefix);
      cmi.ArcName = arcName7z;
      cmi.ArcType = "7z";
      AddCommand(kCompressTo7z, s, cmi);
      MyFormatNew_ReducedName(s, arcName7z);
      MyInsertMenu(popupMenu, subIndex++, currentCommandID++, s, bitmap);
    }

    #ifdef EMAIL_SUPPORT
    // CompressTo7zEmail
    if ((contextMenuFlags & NContextMenuFlags::kCompressTo7zEmail) != 0  && !_dropMode)
    {
      CCommandMapItem cmi;
      UString s;
      cmi.ArcName = arcName7z;
      cmi.ArcType = "7z";
      AddCommand(kCompressTo7zEmail, s, cmi);
      MyFormatNew_ReducedName(s, arcName7z);
      MyInsertMenu(popupMenu, subIndex++, currentCommandID++, s, bitmap);
    }
    #endif

    // CompressToZip
    if (contextMenuFlags & NContextMenuFlags::kCompressToZip &&
        !arcNameZip.IsEqualTo_NoCase(fs2us(fi0.Name)))
    {
      CCommandMapItem cmi;
      UString s;
      if (_dropMode)
        cmi.Folder = _dropPath;
      else
        cmi.Folder = fs2us(folderPrefix);
      cmi.ArcName = arcNameZip;
      cmi.ArcType = "zip";
      AddCommand(kCompressToZip, s, cmi);
      MyFormatNew_ReducedName(s, arcNameZip);
      MyInsertMenu(popupMenu, subIndex++, currentCommandID++, s, bitmap);
    }

    #ifdef EMAIL_SUPPORT
    // CompressToZipEmail
    if ((contextMenuFlags & NContextMenuFlags::kCompressToZipEmail) != 0  && !_dropMode)
    {
      CCommandMapItem cmi;
      UString s;
      cmi.ArcName = arcNameZip;
      cmi.ArcType = "zip";
      AddCommand(kCompressToZipEmail, s, cmi);
      MyFormatNew_ReducedName(s, arcNameZip);
      MyInsertMenu(popupMenu, subIndex++, currentCommandID++, s, bitmap);
    }
    #endif
  }


  // don't use InsertMenu:  See MSDN:
  // PRB: Duplicate Menu Items In the File Menu For a Shell Context Menu Extension
  // ID: Q214477
  
  if (ci.Cascaded.Val)
  {
    CMenu menu;
    menu.Attach(hMenu);
    menuDestroyer.Disable();
    MyAddSubMenu(_commandMap, kMainVerb, menu, indexMenu++, currentCommandID++, (UString)"7-Zip", popupMenu.Detach(), bitmap);
  }
  else
  {
    popupMenu.Detach();
    indexMenu = subIndex;
  }

  
  if (!_isMenuForFM &&
      ((contextMenuFlags & NContextMenuFlags::kCRC) != 0
      && currentCommandID + 1 < commandIDLast))
  {
    CMenu subMenu;
    // CMenuDestroyer menuDestroyer_CRC;
    
    UINT subIndex_CRC = 0;
    
    if (subMenu.CreatePopup())
    {
      // menuDestroyer_CRC.Attach(subMenu);
      
      CMenu menu;
      menu.Attach(hMenu);
      // menuDestroyer_CRC.Disable();
      MyAddSubMenu(_commandMap, kCheckSumCascadedVerb, menu, indexMenu++, currentCommandID++, (UString)"CRC SHA", subMenu, bitmap);

      for (unsigned i = 0; i < ARRAY_SIZE(g_HashCommands); i++)
      {
        if (currentCommandID >= commandIDLast)
          break;
        const CHashCommand &hc = g_HashCommands[i];
        CCommandMapItem cmi;
        cmi.CommandInternalID = hc.CommandInternalID;
        cmi.Verb = kCheckSumCascadedVerb;
        cmi.Verb += '.';
        cmi.Verb += hc.MethodName;
        // cmi.HelpString = cmi.Verb;
        _commandMap.Add(cmi);
        MyInsertMenu(subMenu, subIndex_CRC++, currentCommandID++, (UString)hc.UserName, bitmap);
      }
      
      subMenu.Detach();
    }
  }

  #ifdef SHOW_DEBUG_CTX_MENU
  { char s[256]; sprintf(s, "Commands=%d currentCommandID - commandIDFirst = %d",
    _commandMap.Size(), currentCommandID - commandIDFirst); OutputDebugStringA(s); }
  #endif

  if (_commandMap.Size() != currentCommandID - commandIDFirst)
    throw 20190818;
  return MAKE_HRESULT(SEVERITY_SUCCESS, 0, currentCommandID - commandIDFirst);

  }
  catch(...)
  {
    /* we added some menu items already : num_added_menu_items,
       So we MUST return (number_of_defined_ids), where (number_of_defined_ids >= num_added_menu_items)
       This will prevent incorrect menu working, when same IDs can be
       assigned in multiple menu items from different subhandlers.
       And we must add items to _commandMap before adding to menu.
     */
    #ifdef SHOW_DEBUG_CTX_MENU
    { char s[256]; sprintf(s, "catch() exception: Commands=%d",
      _commandMap.Size()); OutputDebugStringA(s); }
    #endif
    // if (_commandMap.Size() != 0)
      return MAKE_HRESULT(SEVERITY_SUCCESS, 0, _commandMap.Size());
    // throw;
  }
  COM_TRY_END
}


int CZipContextMenu::FindVerb(const UString &verb)
{
  FOR_VECTOR (i, _commandMap)
    if (_commandMap[i].Verb == verb)
      return i;
  return -1;
}

static UString Get7zFmPath()
{
  return fs2us(NWindows::NDLL::GetModuleDirPrefix()) + L"7zFM.exe";
}


#ifdef UNDER_CE
  #define MY__IS_INTRESOURCE(_r) ((((ULONG_PTR)(_r)) >> 16) == 0)
#else
  #define MY__IS_INTRESOURCE(_r) IS_INTRESOURCE(_r)
#endif


#ifdef SHOW_DEBUG_CTX_MENU
static void PrintStringA(const char *name, LPCSTR ptr)
{
  AString m;
  m += name;
  m += ": ";
  char s[32];
  sprintf(s, "%p", ptr);
  m += s;
  if (!MY__IS_INTRESOURCE(ptr))
  {
    m += ": \"";
    m += ptr;
    m += "\"";
  }
  OutputDebugStringA(m);
}

#if !defined(UNDER_CE)
static void PrintStringW(const char *name, LPCWSTR ptr)
{
  UString m;
  m += name;
  m += ": ";
  char s[32];
  sprintf(s, "%p", ptr);
  m += s;
  if (!MY__IS_INTRESOURCE(ptr))
  {
    m += ": \"";
    m += ptr;
    m += "\"";
  }
  OutputDebugStringW(m);
}
#endif
#endif


STDMETHODIMP CZipContextMenu::InvokeCommand(LPCMINVOKECOMMANDINFO commandInfo)
{
  COM_TRY_BEGIN

  #ifdef SHOW_DEBUG_CTX_MENU

  { char s[1280]; sprintf(s,
      #ifdef _WIN64
      "64"
      #else
      "32"
      #endif
      ": InvokeCommand: cbSize=%d flags=%x "
    , commandInfo->cbSize, commandInfo->fMask); OutputDebugStringA(s); }

    PrintStringA("Verb", commandInfo->lpVerb);
    PrintStringA("Parameters", commandInfo->lpParameters);
    PrintStringA("Directory", commandInfo->lpDirectory);
  #endif

  int commandOffset = -1;

  // xp64 / Win10 : explorer.exe sends 0 in lpVerbW
  // MSDN: if (IS_INTRESOURCE(lpVerbW)), we must use LOWORD(lpVerb) as sommand offset

  // FIXME: MINGW doesn't define CMINVOKECOMMANDINFOEX
  #if !defined(UNDER_CE) /* && defined(_MSC_VER) */
  bool unicodeVerb = false;
  if (commandInfo->cbSize == sizeof(CMINVOKECOMMANDINFOEX) &&
      (commandInfo->fMask & CMIC_MASK_UNICODE) != 0)
  {
    LPCMINVOKECOMMANDINFOEX commandInfoEx = (LPCMINVOKECOMMANDINFOEX)commandInfo;
    if (!MY__IS_INTRESOURCE(commandInfoEx->lpVerbW))
    {
      unicodeVerb = true;
      commandOffset = FindVerb(commandInfoEx->lpVerbW);
    }
    
    #ifdef SHOW_DEBUG_CTX_MENU
    PrintStringW("VerbW", commandInfoEx->lpVerbW);
    PrintStringW("ParametersW", commandInfoEx->lpParametersW);
    PrintStringW("DirectoryW", commandInfoEx->lpDirectoryW);
    PrintStringW("TitleW", commandInfoEx->lpTitleW);
    PrintStringA("Title", commandInfoEx->lpTitle);
    #endif
  }
  if (!unicodeVerb)
  #endif
  {
    #ifdef SHOW_DEBUG_CTX_MENU
    OutputDebugStringA("use non-UNICODE verb");
    #endif
    // if (HIWORD(commandInfo->lpVerb) == 0)
    if (MY__IS_INTRESOURCE(commandInfo->lpVerb))
      commandOffset = LOWORD(commandInfo->lpVerb);
    else
      commandOffset = FindVerb(GetUnicodeString(commandInfo->lpVerb));
  }

  #ifdef SHOW_DEBUG_CTX_MENU
  { char s[128]; sprintf(s, "commandOffset=%d",
      commandOffset); OutputDebugStringA(s); }
  #endif

  if (commandOffset < 0 || (unsigned)commandOffset >= _commandMap.Size())
    return E_INVALIDARG;

  const CCommandMapItem &cmi = _commandMap[(unsigned)commandOffset];
  ECommandInternalID cmdID = cmi.CommandInternalID;

  try
  {
    switch (cmdID)
    {
      case kOpen:
      {
        UString params;
        params = GetQuotedString(_fileNames[0]);
        if (!cmi.ArcType.IsEmpty())
        {
          params += " -t";
          params += cmi.ArcType;
        }
        MyCreateProcess(Get7zFmPath(), params);
        break;
      }
      case kExtract:
      case kExtractHere:
      case kExtractTo:
      {
        ExtractArchives(_fileNames, cmi.Folder,
            (cmdID == kExtract), // showDialog
            (cmdID == kExtractTo) && _elimDup.Val // elimDup
            );
        break;
      }
      case kTest:
      {
        TestArchives(_fileNames);
        break;
      }
      case kCompress:
      case kCompressEmail:
      case kCompressTo7z:
      case kCompressTo7zEmail:
      case kCompressToZip:
      case kCompressToZipEmail:
      {
        bool email =
            (cmdID == kCompressEmail) ||
            (cmdID == kCompressTo7zEmail) ||
            (cmdID == kCompressToZipEmail);
        bool showDialog =
            (cmdID == kCompress) ||
            (cmdID == kCompressEmail);
        bool addExtension = (cmdID == kCompress || cmdID == kCompressEmail);
        CompressFiles(cmi.Folder,
            cmi.ArcName, cmi.ArcType,
            addExtension,
            _fileNames, email, showDialog, false);
        break;
      }
      
      case kHash_CRC32:
      case kHash_CRC64:
      case kHash_SHA1:
      case kHash_SHA256:
      case kHash_All:
      {
        for (unsigned i = 0; i < ARRAY_SIZE(g_HashCommands); i++)
        {
          const CHashCommand &hc = g_HashCommands[i];
          if (hc.CommandInternalID == cmdID)
          {
            CalcChecksum(_fileNames, (UString)hc.MethodName);
            break;
          }
        }
        break;
      }
      case kCommandNULL:
        break;
    }
  }
  catch(...)
  {
    ::MessageBoxW(0, L"Error", L"7-Zip", MB_ICONERROR);
  }
  return S_OK;
  COM_TRY_END
}



static void MyCopyString(void *dest, const UString &src, bool writeInUnicode, UINT size)
{
  if (size != 0)
    size--;
  if (writeInUnicode)
  {
    UString s = src;
    s.DeleteFrom(size);
    MyStringCopy((wchar_t *)dest, s);
  }
  else
  {
    AString s = GetAnsiString(src);
    s.DeleteFrom(size);
    MyStringCopy((char *)dest, s);
  }
}


STDMETHODIMP CZipContextMenu::GetCommandString(UINT_PTR commandOffset, UINT uType,
    UINT * /* pwReserved */ , LPSTR pszName, UINT cchMax)
{
  COM_TRY_BEGIN

  const int cmdOffset = (int)commandOffset;
  
  #ifdef SHOW_DEBUG_CTX_MENU
  { char s[256]; sprintf(s, "GetCommandString: cmdOffset=%d uType=%d cchMax = %d",
      cmdOffset, uType, cchMax); OutputDebugStringA(s); }
  #endif

  if (uType == GCS_VALIDATEA || uType == GCS_VALIDATEW)
  {
    if (cmdOffset < 0 || (unsigned)cmdOffset >= _commandMap.Size())
      return S_FALSE;
    else
      return S_OK;
  }

  if (cmdOffset < 0 || (unsigned)cmdOffset >= _commandMap.Size())
  {
    #ifdef SHOW_DEBUG_CTX_MENU
    OutputDebugStringA("---------------- cmdOffset: E_INVALIDARG");
    #endif
    return E_INVALIDARG;
  }

  const CCommandMapItem &cmi = _commandMap[(unsigned)cmdOffset];

  if (uType == GCS_HELPTEXTA || uType == GCS_HELPTEXTW)
  {
    // we can return "Verb" here for debug purposes.
    // HelpString;
    MyCopyString(pszName, cmi.Verb, uType == GCS_HELPTEXTW, cchMax);
    return S_OK;
  }

  if (uType == GCS_VERBA || uType == GCS_VERBW)
  {
    MyCopyString(pszName, cmi.Verb, uType == GCS_VERBW, cchMax);
    return S_OK;
  }
  
  return E_INVALIDARG;
  
  COM_TRY_END
}

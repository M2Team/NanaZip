// FileFolderPluginOpen.cpp

#include "StdAfx.h"

#include "resource.h"

#include "../../../Windows/FileName.h"
#include "../../../Windows/Thread.h"

#include "../Agent/Agent.h"
#include "../GUI/ExtractRes.h"

#include "FileFolderPluginOpen.h"
#include "FormatUtils.h"
#include "LangUtils.h"
#include "OpenCallback.h"
#include "PluginLoader.h"
#include "PropertyName.h"
#include "RegistryPlugins.h"

using namespace NWindows;

struct CThreadArchiveOpen
{
  UString Path;
  UString ArcFormat;
  CMyComPtr<IInStream> InStream;
  CMyComPtr<IFolderManager> FolderManager;
  CMyComPtr<IProgress> OpenCallback;
  COpenArchiveCallback *OpenCallbackSpec;

  CMyComPtr<IFolderFolder> Folder;
  HRESULT Result;

  void Process()
  {
    try
    {
      CProgressCloser closer(OpenCallbackSpec->ProgressDialog);
      Result = FolderManager->OpenFolderFile(InStream, Path, ArcFormat, &Folder, OpenCallback);
    }
    catch(...) { Result = E_FAIL; }
  }
  
  static THREAD_FUNC_DECL MyThreadFunction(void *param)
  {
    ((CThreadArchiveOpen *)param)->Process();
    return 0;
  }
};

/*
static int FindPlugin(const CObjectVector<CPluginInfo> &plugins, const UString &pluginName)
{
  for (int i = 0; i < plugins.Size(); i++)
    if (plugins[i].Name.CompareNoCase(pluginName) == 0)
      return i;
  return -1;
}
*/

static void SplitNameToPureNameAndExtension(const FString &fullName,
    FString &pureName, FString &extensionDelimiter, FString &extension)
{
  int index = fullName.ReverseFind_Dot();
  if (index < 0)
  {
    pureName = fullName;
    extensionDelimiter.Empty();
    extension.Empty();
  }
  else
  {
    pureName.SetFrom(fullName, index);
    extensionDelimiter = '.';
    extension = fullName.Ptr((unsigned)index + 1);
  }
}


struct CArcLevelInfo
{
  UString Error;
  UString Path;
  UString Type;
  UString ErrorType;
  UString ErrorFlags;
};


struct CArcLevelsInfo
{
  CObjectVector<CArcLevelInfo> Levels; // LastLevel Is NON-OPEN
};


UString GetOpenArcErrorMessage(UInt32 errorFlags);


static void GetFolderLevels(CMyComPtr<IFolderFolder> &folder, CArcLevelsInfo &levels)
{
  levels.Levels.Clear();

  CMyComPtr<IGetFolderArcProps> getFolderArcProps;
  folder.QueryInterface(IID_IGetFolderArcProps, &getFolderArcProps);
  
  if (!getFolderArcProps)
    return;
  CMyComPtr<IFolderArcProps> arcProps;
  getFolderArcProps->GetFolderArcProps(&arcProps);
  if (!arcProps)
    return;

  UInt32 numLevels;
  if (arcProps->GetArcNumLevels(&numLevels) != S_OK)
    numLevels = 0;
  
  for (UInt32 level = 0; level <= numLevels; level++)
  {
    const PROPID propIDs[] = { kpidError, kpidPath, kpidType, kpidErrorType };

    CArcLevelInfo lev;
    
    for (Int32 i = 0; i < 4; i++)
    {
      CMyComBSTR name;
      NCOM::CPropVariant prop;
      if (arcProps->GetArcProp(level, propIDs[i], &prop) != S_OK)
        continue;
      if (prop.vt != VT_EMPTY)
      {
        UString *s = NULL;
        switch (propIDs[i])
        {
          case kpidError: s = &lev.Error; break;
          case kpidPath: s = &lev.Path; break;
          case kpidType: s = &lev.Type; break;
          case kpidErrorType: s = &lev.ErrorType; break;
        }
        *s = (prop.vt == VT_BSTR) ? prop.bstrVal : L"?";
      }
    }

    {
      NCOM::CPropVariant prop;
      if (arcProps->GetArcProp(level, kpidErrorFlags, &prop) == S_OK)
      {
        UInt32 flags = GetOpenArcErrorFlags(prop);
        if (flags != 0)
          lev.ErrorFlags = GetOpenArcErrorMessage(flags);
      }
    }

    levels.Levels.Add(lev);
  }
}
    
static UString GetBracedType(const wchar_t *type)
{
  UString s ('[');
  s += type;
  s += ']';
  return s;
}

static void GetFolderError(CMyComPtr<IFolderFolder> &folder, UString &open_Errors, UString &nonOpen_Errors)
{
  CArcLevelsInfo levs;
  GetFolderLevels(folder, levs);
  open_Errors.Empty();
  nonOpen_Errors.Empty();

  FOR_VECTOR (i, levs.Levels)
  {
    bool isNonOpenLevel = (i == 0);
    const CArcLevelInfo &lev = levs.Levels[levs.Levels.Size() - 1 - i];

    UString m;
    
    if (!lev.ErrorType.IsEmpty())
    {
      m = MyFormatNew(IDS_CANT_OPEN_AS_TYPE, GetBracedType(lev.ErrorType));
      if (!isNonOpenLevel)
      {
        m.Add_LF();
        m += MyFormatNew(IDS_IS_OPEN_AS_TYPE, GetBracedType(lev.Type));
      }
    }

    if (!lev.Error.IsEmpty())
    {
      if (!m.IsEmpty())
        m.Add_LF();
      m += GetBracedType(lev.Type);
      m += " : ";
      m += GetNameOfProperty(kpidError, L"Error");
      m += " : ";
      m += lev.Error;
    }

    if (!lev.ErrorFlags.IsEmpty())
    {
      if (!m.IsEmpty())
        m.Add_LF();
      m += GetNameOfProperty(kpidErrorFlags, L"Errors");
      m += ": ";
      m += lev.ErrorFlags;
    }
    
    if (!m.IsEmpty())
    {
      if (isNonOpenLevel)
      {
        UString &s = nonOpen_Errors;
        s += lev.Path;
        s.Add_LF();
        s += m;
      }
      else
      {
        UString &s = open_Errors;
        if (!s.IsEmpty())
          s += "--------------------\n";
        s += lev.Path;
        s.Add_LF();
        s += m;
      }
    }
  }
}


HRESULT CFfpOpen::OpenFileFolderPlugin(IInStream *inStream,
    const FString &path, const UString &arcFormat, HWND parentWindow)
{
  CObjectVector<CPluginInfo> plugins;
  ReadFileFolderPluginInfoList(plugins);

  FString extension, name, pureName, dot;

  int slashPos = path.ReverseFind_PathSepar();
  FString dirPrefix;
  FString fileName;
  if (slashPos >= 0)
  {
    dirPrefix.SetFrom(path, (unsigned)(slashPos + 1));
    fileName = path.Ptr((unsigned)(slashPos + 1));
  }
  else
    fileName = path;

  SplitNameToPureNameAndExtension(fileName, pureName, dot, extension);

  /*
  if (!extension.IsEmpty())
  {
    CExtInfo extInfo;
    if (ReadInternalAssociation(extension, extInfo))
    {
      for (int i = extInfo.Plugins.Size() - 1; i >= 0; i--)
      {
        int pluginIndex = FindPlugin(plugins, extInfo.Plugins[i]);
        if (pluginIndex >= 0)
        {
          const CPluginInfo plugin = plugins[pluginIndex];
          plugins.Delete(pluginIndex);
          plugins.Insert(0, plugin);
        }
      }
    }
  }
  */

  ErrorMessage.Empty();

  FOR_VECTOR (i, plugins)
  {
    const CPluginInfo &plugin = plugins[i];
    if (!plugin.ClassIDDefined)
      continue;
    CPluginLibrary library;

    CThreadArchiveOpen t;

    if (plugin.FilePath.IsEmpty())
      t.FolderManager = new CArchiveFolderManager;
    else if (library.LoadAndCreateManager(plugin.FilePath, plugin.ClassID, &t.FolderManager) != S_OK)
      continue;

    t.OpenCallbackSpec = new COpenArchiveCallback;
    t.OpenCallback = t.OpenCallbackSpec;
    t.OpenCallbackSpec->PasswordIsDefined = Encrypted;
    t.OpenCallbackSpec->Password = Password;
    t.OpenCallbackSpec->ParentWindow = parentWindow;

    if (inStream)
      t.OpenCallbackSpec->SetSubArchiveName(fs2us(fileName));
    else
    {
      RINOK(t.OpenCallbackSpec->LoadFileInfo2(dirPrefix, fileName));
    }

    t.InStream = inStream;
    t.Path = fs2us(path);
    t.ArcFormat = arcFormat;

    const UString progressTitle = LangString(IDS_OPENNING);
    {
      CProgressDialog &pd = t.OpenCallbackSpec->ProgressDialog;
      pd.MainWindow = parentWindow;
      pd.MainTitle = "7-Zip"; // LangString(IDS_APP_TITLE);
      pd.MainAddTitle = progressTitle + L' ';
      pd.WaitMode = true;
    }

    {
      NWindows::CThread thread;
      RINOK(thread.Create(CThreadArchiveOpen::MyThreadFunction, &t));
      t.OpenCallbackSpec->StartProgressDialog(progressTitle, thread);
    }

    if (t.Result != S_FALSE && t.Result != S_OK)
      return t.Result;

    if (t.Folder)
    {
      UString open_Errors, nonOpen_Errors;
      GetFolderError(t.Folder, open_Errors, nonOpen_Errors);
      if (!nonOpen_Errors.IsEmpty())
      {
        ErrorMessage = nonOpen_Errors;
        // if (t.Result != S_OK) return t.Result;
        /* if there are good open leves, and non0open level,
           we could force error as critical error and return error here
           but it's better to allow to open such rachives */
        // return S_FALSE;
      }
    }

    // if (openCallbackSpec->PasswordWasAsked)
    {
      Encrypted = t.OpenCallbackSpec->PasswordIsDefined;
      Password = t.OpenCallbackSpec->Password;
    }

    if (t.Result == S_OK)
    {
      Library.Attach(library.Detach());
      // Folder.Attach(t.Folder.Detach());
      Folder = t.Folder;
    }
    
    return t.Result;
  }

  return S_FALSE;
}

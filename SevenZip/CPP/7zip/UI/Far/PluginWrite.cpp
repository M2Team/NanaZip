// PluginWrite.cpp

#include "StdAfx.h"

#include <stdio.h>

#include "Plugin.h"

#include "../../../Common/StringConvert.h"
#include "../../../Common/Wildcard.h"

#include "../../../Windows/FileName.h"
#include "../../../Windows/FileFind.h"

#include "../Common/ZipRegistry.h"

#include "../Agent/Agent.h"

#include "ProgressBox.h"
#include "Messages.h"
#include "UpdateCallbackFar.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;
using namespace NFar;

using namespace NUpdateArchive;

static const char * const kHelpTopic = "Update";

static const char * const kArchiveHistoryKeyName = "7-ZipArcName";

static const UInt32 g_MethodMap[] = { 0, 1, 3, 5, 7, 9 };

static HRESULT SetOutProperties(IOutFolderArchive *outArchive, UInt32 method)
{
  CMyComPtr<ISetProperties> setProperties;
  if (outArchive->QueryInterface(IID_ISetProperties, (void **)&setProperties) == S_OK)
  {
    /*
    UStringVector realNames;
    realNames.Add(UString("x"));
    NCOM::CPropVariant value = (UInt32)method;
    CRecordVector<const wchar_t *> names;
    FOR_VECTOR (i, realNames)
      names.Add(realNames[i]);
    RINOK(setProperties->SetProperties(&names.Front(), &value, names.Size()));
    */
    NCOM::CPropVariant value = (UInt32)method;
    const wchar_t *name = L"x";
    RINOK(setProperties->SetProperties(&name, &value, 1));
  }
  return S_OK;
}

/*
HRESULT CPlugin::AfterUpdate(CWorkDirTempFile &tempFile, const UStringVector &pathVector)
{
  _folder.Release();
  m_ArchiveHandler->Close();
  
  RINOK(tempFile.MoveToOriginal(true));
  
  RINOK(m_ArchiveHandler->ReOpen(NULL)); // check it

  m_ArchiveHandler->BindToRootFolder(&_folder);
  FOR_VECTOR (i, pathVector)
  {
    CMyComPtr<IFolderFolder> newFolder;
    _folder->BindToFolder(pathVector[i], &newFolder);
    if (!newFolder)
      break;
    _folder = newFolder;
  }
  return S_OK;
}
*/

NFileOperationReturnCode::EEnum CPlugin::PutFiles(
  struct PluginPanelItem *panelItems, int numItems,
  int moveMode, int opMode)
{
  /*
  if (moveMode != 0)
  {
    g_StartupInfo.ShowMessage(NMessageID::kMoveIsNotSupported);
    return NFileOperationReturnCode::kError;
  }
  */

  if (numItems <= 0)
    return NFileOperationReturnCode::kError;

  if (_agent->IsThereReadOnlyArc())
  {
    g_StartupInfo.ShowMessage(NMessageID::kUpdateNotSupportedForThisArchive);
    return NFileOperationReturnCode::kError;
  }

  const int kYSize = 14;
  const int kXMid = 38;

  NCompression::CInfo compressionInfo;
  compressionInfo.Load();

  unsigned methodIndex = 0;

  unsigned i;
  for (i = ARRAY_SIZE(g_MethodMap); i != 0;)
  {
    i--;
    if (compressionInfo.Level >= g_MethodMap[i])
    {
      methodIndex = i;
      break;
    }
  }

  const int kMethodRadioIndex = 2;
  const int kModeRadioIndex = kMethodRadioIndex + 7;

  struct CInitDialogItem initItems[]={
    { DI_DOUBLEBOX, 3, 1, 72, kYSize - 2, false, false, 0, false, NMessageID::kUpdateTitle, NULL, NULL },
    
    { DI_SINGLEBOX, 4, 2, kXMid - 2, 2 + 7, false, false, 0, false, NMessageID::kUpdateMethod, NULL, NULL },
    
    { DI_RADIOBUTTON, 6, 3, 0, 0, methodIndex == 0, methodIndex == 0, DIF_GROUP, false, NMessageID::kUpdateMethod_Store, NULL, NULL },
    { DI_RADIOBUTTON, 6, 4, 0, 0, methodIndex == 1, methodIndex == 1, 0, false, NMessageID::kUpdateMethod_Fastest, NULL, NULL },
    { DI_RADIOBUTTON, 6, 5, 0, 0, methodIndex == 2, methodIndex == 2, 0, false, NMessageID::kUpdateMethod_Fast, NULL, NULL },
    { DI_RADIOBUTTON, 6, 6, 0, 0, methodIndex == 3, methodIndex == 3, 0, false, NMessageID::kUpdateMethod_Normal, NULL, NULL },
    { DI_RADIOBUTTON, 6, 7, 0, 0, methodIndex == 4, methodIndex == 4, 0, false, NMessageID::kUpdateMethod_Maximum, NULL, NULL },
    { DI_RADIOBUTTON, 6, 8, 0, 0, methodIndex == 5, methodIndex == 5, 0, false, NMessageID::kUpdateMethod_Ultra, NULL, NULL },
    
    { DI_SINGLEBOX, kXMid, 2, 70, 2 + 5, false, false, 0, false, NMessageID::kUpdateMode, NULL, NULL },
    
    { DI_RADIOBUTTON, kXMid + 2, 3, 0, 0, false, true, DIF_GROUP, false, NMessageID::kUpdateMode_Add, NULL, NULL },
    { DI_RADIOBUTTON, kXMid + 2, 4, 0, 0, false, false,        0, false, NMessageID::kUpdateMode_Update, NULL, NULL },
    { DI_RADIOBUTTON, kXMid + 2, 5, 0, 0, false, false,        0, false, NMessageID::kUpdateMode_Fresh, NULL, NULL },
    { DI_RADIOBUTTON, kXMid + 2, 6, 0, 0, false, false,        0, false, NMessageID::kUpdateMode_Sync, NULL, NULL },
  
    { DI_TEXT, 3, kYSize - 4, 0, 0, false, false, DIF_BOXCOLOR|DIF_SEPARATOR, false, -1, "", NULL  },
    
    { DI_BUTTON, 0, kYSize - 3, 0, 0, false, false, DIF_CENTERGROUP, true, NMessageID::kUpdateAdd, NULL, NULL  },
    { DI_BUTTON, 0, kYSize - 3, 0, 0, false, false, DIF_CENTERGROUP, false, NMessageID::kCancel, NULL, NULL  }
  };
  
  const int kNumDialogItems = ARRAY_SIZE(initItems);
  const int kOkButtonIndex = kNumDialogItems - 2;
  FarDialogItem dialogItems[kNumDialogItems];
  g_StartupInfo.InitDialogItems(initItems, dialogItems, kNumDialogItems);
  int askCode = g_StartupInfo.ShowDialog(76, kYSize,
      kHelpTopic, dialogItems, kNumDialogItems);
  if (askCode != kOkButtonIndex)
    return NFileOperationReturnCode::kInterruptedByUser;

  compressionInfo.Level = g_MethodMap[0];
  for (i = 0; i < ARRAY_SIZE(g_MethodMap); i++)
    if (dialogItems[kMethodRadioIndex + i].Selected)
      compressionInfo.Level = g_MethodMap[i];

  const CActionSet *actionSet;

       if (dialogItems[kModeRadioIndex    ].Selected) actionSet = &k_ActionSet_Add;
  else if (dialogItems[kModeRadioIndex + 1].Selected) actionSet = &k_ActionSet_Update;
  else if (dialogItems[kModeRadioIndex + 2].Selected) actionSet = &k_ActionSet_Fresh;
  else if (dialogItems[kModeRadioIndex + 3].Selected) actionSet = &k_ActionSet_Sync;
  else throw 51751;

  compressionInfo.Save();

  CWorkDirTempFile tempFile;;
  if (tempFile.CreateTempFile(m_FileName) != S_OK)
    return NFileOperationReturnCode::kError;


  /*
  CSysStringVector fileNames;
  for (int i = 0; i < numItems; i++)
  {
    const PluginPanelItem &panelItem = panelItems[i];
    CSysString fullName;
    if (!MyGetFullPathName(panelItem.FindData.cFileName, fullName))
      return NFileOperationReturnCode::kError;
    fileNames.Add(fullName);
  }
  */

  CScreenRestorer screenRestorer;
  CProgressBox progressBox;
  CProgressBox *progressBoxPointer = NULL;
  if ((opMode & OPM_SILENT) == 0 && (opMode & OPM_FIND ) == 0)
  {
    screenRestorer.Save();

    progressBoxPointer = &progressBox;
    progressBox.Init(
        // g_StartupInfo.GetMsgString(NMessageID::kWaitTitle),
        g_StartupInfo.GetMsgString(NMessageID::kUpdating));
  }
 
  UStringVector pathVector;
  GetPathParts(pathVector);
  
  UStringVector fileNames;
  fileNames.ClearAndReserve(numItems);
  for (i = 0; i < (unsigned)numItems; i++)
    fileNames.AddInReserved(MultiByteToUnicodeString(panelItems[i].FindData.cFileName, CP_OEMCP));
  CObjArray<const wchar_t *> fileNamePointers(numItems);
  for (i = 0; i < (unsigned)numItems; i++)
    fileNamePointers[i] = fileNames[i];

  CMyComPtr<IOutFolderArchive> outArchive;
  HRESULT result = m_ArchiveHandler.QueryInterface(IID_IOutFolderArchive, &outArchive);
  if (result != S_OK)
  {
    g_StartupInfo.ShowMessage(NMessageID::kUpdateNotSupportedForThisArchive);
    return NFileOperationReturnCode::kError;
  }

  /*
  BYTE actionSetByte[NUpdateArchive::NPairState::kNumValues];
  for (i = 0; i < NUpdateArchive::NPairState::kNumValues; i++)
    actionSetByte[i] = (BYTE)actionSet->StateActions[i];
  */

  CUpdateCallback100Imp *updateCallbackSpec = new CUpdateCallback100Imp;
  CMyComPtr<IFolderArchiveUpdateCallback> updateCallback(updateCallbackSpec );
  
  updateCallbackSpec->Init(/* m_ArchiveHandler, */ progressBoxPointer);
  updateCallbackSpec->PasswordIsDefined = PasswordIsDefined;
  updateCallbackSpec->Password = Password;

  if (SetOutProperties(outArchive, compressionInfo.Level) != S_OK)
    return NFileOperationReturnCode::kError;

  /*
  outArchive->SetFolder(_folder);
  outArchive->SetFiles(L"", fileNamePointers, numItems);
  // FStringVector requestedPaths;
  // FStringVector processedPaths;
  result = outArchive->DoOperation2(
      // &requestedPaths, &processedPaths,
      NULL, NULL,
      tempFile.OutStream, actionSetByte, NULL, updateCallback);
  updateCallback.Release();
  outArchive.Release();

  if (result == S_OK)
  {
    result = AfterUpdate(tempFile, pathVector);
  }
  */

  {
    result = _agent->SetFiles(L"", fileNamePointers, numItems);
    if (result == S_OK)
    {
      CAgentFolder *agentFolder = NULL;
      {
        CMyComPtr<IArchiveFolderInternal> afi;
        _folder.QueryInterface(IID_IArchiveFolderInternal, &afi);
        if (afi)
          afi->GetAgentFolder(&agentFolder);
      }
      if (agentFolder)
        result = agentFolder->CommonUpdateOperation(AGENT_OP_Uni,
            (moveMode != 0), NULL, actionSet, NULL, 0, updateCallback);
      else
        result = E_FAIL;
    }
  }

  if (result != S_OK)
  {
    ShowSysErrorMessage(result);
    return NFileOperationReturnCode::kError;
  }

  return NFileOperationReturnCode::kSuccess;
}

namespace NPathType
{
  enum EEnum
  {
    kLocal,
    kUNC
  };
  EEnum GetPathType(const UString &path);
}

struct CParsedPath
{
  UString Prefix; // Disk or UNC with slash
  UStringVector PathParts;
  void ParsePath(const UString &path);
  UString MergePath() const;
};

static const char kDirDelimiter = CHAR_PATH_SEPARATOR;
static const wchar_t kDiskDelimiter = L':';

namespace NPathType
{
  EEnum GetPathType(const UString &path)
  {
    if (path.Len() <= 2)
      return kLocal;
    if (path[0] == kDirDelimiter && path[1] == kDirDelimiter)
      return kUNC;
    return kLocal;
  }
}

void CParsedPath::ParsePath(const UString &path)
{
  int curPos = 0;
  switch (NPathType::GetPathType(path))
  {
    case NPathType::kLocal:
    {
      int posDiskDelimiter = path.Find(kDiskDelimiter);
      if (posDiskDelimiter >= 0)
      {
        curPos = posDiskDelimiter + 1;
        if ((int)path.Len() > curPos)
          if (path[curPos] == kDirDelimiter)
            curPos++;
      }
      break;
    }
    case NPathType::kUNC:
    {
      // the bug was fixed:
      curPos = path.Find((wchar_t)kDirDelimiter, 2);
      if (curPos < 0)
        curPos = path.Len();
      else
        curPos++;
    }
  }
  Prefix = path.Left(curPos);
  SplitPathToParts(path.Ptr(curPos), PathParts);
}

UString CParsedPath::MergePath() const
{
  UString result = Prefix;
  FOR_VECTOR (i, PathParts)
  {
    if (i != 0)
      result += kDirDelimiter;
    result += PathParts[i];
  }
  return result;
}


static void SetArcName(UString &arcName, const CArcInfoEx &arcInfo)
{
  if (!arcInfo.Flags_KeepName())
  {
    int dotPos = arcName.ReverseFind_Dot();
    int slashPos = arcName.ReverseFind_PathSepar();
    if (dotPos > slashPos + 1)
      arcName.DeleteFrom(dotPos);
  }
  arcName += '.';
  arcName += arcInfo.GetMainExt();
}

HRESULT CompressFiles(const CObjectVector<PluginPanelItem> &pluginPanelItems)
{
  if (pluginPanelItems.Size() == 0)
    return E_FAIL;

  UStringVector fileNames;
  {
    FOR_VECTOR (i, pluginPanelItems)
    {
      const PluginPanelItem &panelItem = pluginPanelItems[i];
      if (strcmp(panelItem.FindData.cFileName, "..") == 0 &&
          NFind::NAttributes::IsDir(panelItem.FindData.dwFileAttributes))
        return E_FAIL;
      if (strcmp(panelItem.FindData.cFileName, ".") == 0 &&
          NFind::NAttributes::IsDir(panelItem.FindData.dwFileAttributes))
        return E_FAIL;
      FString fullPath;
      FString fileNameUnicode = us2fs(MultiByteToUnicodeString(panelItem.FindData.cFileName, CP_OEMCP));
      if (!MyGetFullPathName(fileNameUnicode, fullPath))
        return E_FAIL;
      fileNames.Add(fs2us(fullPath));
    }
  }

  NCompression::CInfo compressionInfo;
  compressionInfo.Load();
  
  int archiverIndex = -1;

  /*
  CCodecs *codecs = new CCodecs;
  CMyComPtr<ICompressCodecsInfo> compressCodecsInfo = codecs;
  if (codecs->Load() != S_OK)
    throw "Can't load 7-Zip codecs";
  */
  
  if (LoadGlobalCodecs() != S_OK)
    throw "Can't load 7-Zip codecs";

  CCodecs *codecs = g_CodecsObj;

  {
    FOR_VECTOR (i, codecs->Formats)
    {
      const CArcInfoEx &arcInfo = codecs->Formats[i];
      if (arcInfo.UpdateEnabled)
      {
        if (archiverIndex == -1)
          archiverIndex = i;
        if (MyStringCompareNoCase(arcInfo.Name, compressionInfo.ArcType) == 0)
          archiverIndex = i;
      }
    }
  }

  if (archiverIndex < 0)
    throw "there is no output handler";

  UString resultPath;
  {
    CParsedPath parsedPath;
    parsedPath.ParsePath(fileNames.Front());
    if (parsedPath.PathParts.Size() == 0)
      return E_FAIL;
    if (fileNames.Size() == 1 || parsedPath.PathParts.Size() == 1)
    {
      // CSysString pureName, dot, extension;
      resultPath = parsedPath.PathParts.Back();
    }
    else
    {
      parsedPath.PathParts.DeleteBack();
      resultPath = parsedPath.PathParts.Back();
    }
  }
  UString archiveNameSrc = resultPath;
  UString arcName = archiveNameSrc;

  int prevFormat = archiverIndex;
  SetArcName(arcName, codecs->Formats[archiverIndex]);
  
  const CActionSet *actionSet = &k_ActionSet_Add;

  for (;;)
  {
    AString archiveNameA (UnicodeStringToMultiByte(arcName, CP_OEMCP));
    const int kYSize = 16;
    const int kXMid = 38;
  
    const int kArchiveNameIndex = 2;
    const int kMethodRadioIndex = kArchiveNameIndex + 2;
    const int kModeRadioIndex = kMethodRadioIndex + 7;

    // char updateAddToArchiveString[512];
    AString str1;
    {
      const CArcInfoEx &arcInfo = codecs->Formats[archiverIndex];
      const AString s (UnicodeStringToMultiByte(arcInfo.Name, CP_OEMCP));
      str1 = g_StartupInfo.GetMsgString(NMessageID::kUpdateAddToArchive);
      str1.Replace(AString ("%s"), s);
      /*
      sprintf(updateAddToArchiveString,
        g_StartupInfo.GetMsgString(NMessageID::kUpdateAddToArchive), (const char *)s);
      */
    }

    unsigned methodIndex = 0;
    unsigned i;
    for (i = ARRAY_SIZE(g_MethodMap); i != 0;)
    {
      i--;
      if (compressionInfo.Level >= g_MethodMap[i])
      {
        methodIndex = i;
        break;
      }
    }

    const struct CInitDialogItem initItems[]=
    {
      { DI_DOUBLEBOX, 3, 1, 72, kYSize - 2, false, false, 0, false, NMessageID::kUpdateTitle, NULL, NULL },

      { DI_TEXT, 5, 2, 0, 0, false, false, 0, false, -1, str1, NULL },
      
      { DI_EDIT, 5, 3, 70, 3, true, false, DIF_HISTORY, false, -1, archiveNameA, kArchiveHistoryKeyName},
      // { DI_EDIT, 5, 3, 70, 3, true, false, 0, false, -1, arcName, NULL},
      
      { DI_SINGLEBOX, 4, 4, kXMid - 2, 4 + 7, false, false, 0, false, NMessageID::kUpdateMethod, NULL, NULL },
      
      { DI_RADIOBUTTON, 6, 5, 0, 0, false, methodIndex == 0, DIF_GROUP, false, NMessageID::kUpdateMethod_Store, NULL, NULL },
      { DI_RADIOBUTTON, 6, 6, 0, 0, false, methodIndex == 1, 0, false, NMessageID::kUpdateMethod_Fastest, NULL, NULL },
      { DI_RADIOBUTTON, 6, 7, 0, 0, false, methodIndex == 2, 0, false, NMessageID::kUpdateMethod_Fast, NULL, NULL },
      { DI_RADIOBUTTON, 6, 8, 0, 0, false, methodIndex == 3, 0, false, NMessageID::kUpdateMethod_Normal, NULL, NULL },
      { DI_RADIOBUTTON, 6, 9, 0, 0, false, methodIndex == 4, 0, false, NMessageID::kUpdateMethod_Maximum, NULL, NULL },
      { DI_RADIOBUTTON, 6,10, 0, 0, false, methodIndex == 5, 0, false, NMessageID::kUpdateMethod_Ultra, NULL, NULL },
      
      { DI_SINGLEBOX, kXMid, 4, 70, 4 + 5, false, false, 0, false, NMessageID::kUpdateMode, NULL, NULL },
      
      { DI_RADIOBUTTON, kXMid + 2, 5, 0, 0, false, actionSet == &k_ActionSet_Add, DIF_GROUP, false, NMessageID::kUpdateMode_Add, NULL, NULL },
      { DI_RADIOBUTTON, kXMid + 2, 6, 0, 0, false, actionSet == &k_ActionSet_Update,      0, false, NMessageID::kUpdateMode_Update, NULL, NULL },
      { DI_RADIOBUTTON, kXMid + 2, 7, 0, 0, false, actionSet == &k_ActionSet_Fresh,       0, false, NMessageID::kUpdateMode_Fresh, NULL, NULL },
      { DI_RADIOBUTTON, kXMid + 2, 8, 0, 0, false, actionSet == &k_ActionSet_Sync,        0, false, NMessageID::kUpdateMode_Sync, NULL, NULL },
      
      { DI_TEXT, 3, kYSize - 4, 0, 0, false, false, DIF_BOXCOLOR|DIF_SEPARATOR, false, -1, "", NULL  },
      
      { DI_BUTTON, 0, kYSize - 3, 0, 0, false, false, DIF_CENTERGROUP, true, NMessageID::kUpdateAdd, NULL, NULL  },
      { DI_BUTTON, 0, kYSize - 3, 0, 0, false, false, DIF_CENTERGROUP, false, NMessageID::kUpdateSelectArchiver, NULL, NULL  },
      { DI_BUTTON, 0, kYSize - 3, 0, 0, false, false, DIF_CENTERGROUP, false, NMessageID::kCancel, NULL, NULL  }
    };

    const int kNumDialogItems = ARRAY_SIZE(initItems);
    
    const int kOkButtonIndex = kNumDialogItems - 3;
    const int kSelectarchiverButtonIndex = kNumDialogItems - 2;

    FarDialogItem dialogItems[kNumDialogItems];
    g_StartupInfo.InitDialogItems(initItems, dialogItems, kNumDialogItems);
    int askCode = g_StartupInfo.ShowDialog(76, kYSize,
        kHelpTopic, dialogItems, kNumDialogItems);

    archiveNameA = dialogItems[kArchiveNameIndex].Data;
    archiveNameA.Trim();
    MultiByteToUnicodeString2(arcName, archiveNameA, CP_OEMCP);

    compressionInfo.Level = g_MethodMap[0];
    for (i = 0; i < ARRAY_SIZE(g_MethodMap); i++)
      if (dialogItems[kMethodRadioIndex + i].Selected)
        compressionInfo.Level = g_MethodMap[i];

         if (dialogItems[kModeRadioIndex    ].Selected) actionSet = &k_ActionSet_Add;
    else if (dialogItems[kModeRadioIndex + 1].Selected) actionSet = &k_ActionSet_Update;
    else if (dialogItems[kModeRadioIndex + 2].Selected) actionSet = &k_ActionSet_Fresh;
    else if (dialogItems[kModeRadioIndex + 3].Selected) actionSet = &k_ActionSet_Sync;
    else throw 51751;

    if (askCode == kSelectarchiverButtonIndex)
    {
      CIntVector indices;
      AStringVector archiverNames;
      FOR_VECTOR (k, codecs->Formats)
      {
        const CArcInfoEx &arc = codecs->Formats[k];
        if (arc.UpdateEnabled)
        {
          indices.Add(k);
          archiverNames.Add(GetOemString(arc.Name));
        }
      }
    
      int index = g_StartupInfo.Menu(FMENU_AUTOHIGHLIGHT,
          g_StartupInfo.GetMsgString(NMessageID::kUpdateSelectArchiverMenuTitle),
          NULL, archiverNames, archiverIndex);
      if (index >= 0)
      {
        const CArcInfoEx &prevArchiverInfo = codecs->Formats[prevFormat];
        if (prevArchiverInfo.Flags_KeepName())
        {
          const UString &prevExtension = prevArchiverInfo.GetMainExt();
          const unsigned prevExtensionLen = prevExtension.Len();
          if (arcName.Len() >= prevExtensionLen &&
              MyStringCompareNoCase(arcName.RightPtr(prevExtensionLen), prevExtension) == 0)
          {
            int pos = arcName.Len() - prevExtensionLen;
            if (pos > 2)
            {
              if (arcName[pos - 1] == '.')
                arcName.DeleteFrom(pos - 1);
            }
          }
        }

        archiverIndex = indices[index];
        const CArcInfoEx &arcInfo = codecs->Formats[archiverIndex];
        prevFormat = archiverIndex;
        
        if (arcInfo.Flags_KeepName())
          arcName = archiveNameSrc;
        SetArcName(arcName, arcInfo);
      }
      continue;
    }

    if (askCode != kOkButtonIndex)
      return E_ABORT;
    
    break;
  }

  const CArcInfoEx &archiverInfoFinal = codecs->Formats[archiverIndex];
  compressionInfo.ArcType = archiverInfoFinal.Name;
  compressionInfo.Save();

  NWorkDir::CInfo workDirInfo;
  workDirInfo.Load();

  FString fullArcName;
  if (!MyGetFullPathName(us2fs(arcName), fullArcName))
    return E_FAIL;
   
  CWorkDirTempFile tempFile;
  RINOK(tempFile.CreateTempFile(fullArcName));

  CScreenRestorer screenRestorer;
  CProgressBox progressBox;
  CProgressBox *progressBoxPointer = NULL;

  screenRestorer.Save();

  progressBoxPointer = &progressBox;
  progressBox.Init(
      // g_StartupInfo.GetMsgString(NMessageID::kWaitTitle),
      g_StartupInfo.GetMsgString(NMessageID::kUpdating));


  NFind::CFileInfo fileInfo;

  CMyComPtr<IOutFolderArchive> outArchive;

  CMyComPtr<IInFolderArchive> archiveHandler;
  if (fileInfo.Find(fullArcName))
  {
    if (fileInfo.IsDir())
      throw "There is Directory with such name";

    CAgent *agentSpec = new CAgent;
    archiveHandler = agentSpec;
    // CLSID realClassID;
    CMyComBSTR archiveType;
    RINOK(agentSpec->Open(NULL,
        GetUnicodeString(fullArcName, CP_OEMCP), UString(),
        // &realClassID,
        &archiveType,
        NULL));

    if (MyStringCompareNoCase(archiverInfoFinal.Name, (const wchar_t *)archiveType) != 0)
      throw "Type of existing archive differs from specified type";
    HRESULT result = archiveHandler.QueryInterface(
        IID_IOutFolderArchive, &outArchive);
    if (result != S_OK)
    {
      g_StartupInfo.ShowMessage(NMessageID::kUpdateNotSupportedForThisArchive);
      return E_FAIL;
    }
  }
  else
  {
    // HRESULT result = outArchive.CoCreateInstance(classID);
    CAgent *agentSpec = new CAgent;
    outArchive = agentSpec;

    /*
    HRESULT result = outArchive.CoCreateInstance(CLSID_CAgentArchiveHandler);
    if (result != S_OK)
    {
      g_StartupInfo.ShowMessage(NMessageID::kUpdateNotSupportedForThisArchive);
      return E_FAIL;
    }
    */
  }

  CObjArray<const wchar_t *> fileNamePointers(fileNames.Size());
  
  unsigned i;
  for (i = 0; i < fileNames.Size(); i++)
    fileNamePointers[i] = fileNames[i];

  outArchive->SetFolder(NULL);
  outArchive->SetFiles(L"", fileNamePointers, fileNames.Size());
  BYTE actionSetByte[NUpdateArchive::NPairState::kNumValues];
  for (i = 0; i < NUpdateArchive::NPairState::kNumValues; i++)
    actionSetByte[i] = (BYTE)actionSet->StateActions[i];

  CUpdateCallback100Imp *updateCallbackSpec = new CUpdateCallback100Imp;
  CMyComPtr<IFolderArchiveUpdateCallback> updateCallback(updateCallbackSpec );
  
  updateCallbackSpec->Init(/* archiveHandler, */ progressBoxPointer);


  RINOK(SetOutProperties(outArchive, compressionInfo.Level));

  // FStringVector requestedPaths;
  // FStringVector processedPaths;
  HRESULT result = outArchive->DoOperation(
      // &requestedPaths, &processedPaths,
      NULL, NULL,
      codecs, archiverIndex,
      tempFile.OutStream, actionSetByte,
      NULL, updateCallback);
  updateCallback.Release();
  outArchive.Release();

  if (result != S_OK)
  {
    ShowSysErrorMessage(result);
    return result;
  }
 
  if (archiveHandler)
  {
    archiveHandler->Close();
  }

  result = tempFile.MoveToOriginal(archiveHandler != NULL);
  if (result != S_OK)
  {
    ShowSysErrorMessage(result);
    return result;
  }
  return S_OK;
}


static const char * const k_CreateFolder_History = "NewFolder"; // we use default FAR folder name

HRESULT CPlugin::CreateFolder()
{
  if (_agent->IsThereReadOnlyArc())
  {
    g_StartupInfo.ShowMessage(NMessageID::kUpdateNotSupportedForThisArchive);
    return TRUE;
  }

  UString destPathU;
  {
    const int kXSize = 60;
    const int kYSize = 8;
    const int kPathIndex = 2;

    AString destPath ("New Folder");

    const struct CInitDialogItem initItems[]={
      { DI_DOUBLEBOX, 3, 1, kXSize - 4, kYSize - 2, false, false, 0, false,
          -1, "Create Folder", NULL },

      { DI_TEXT, 5, 2, 0, 0, false, false, 0, false, -1, "Folder name:", NULL },
      
      { DI_EDIT, 5, 3, kXSize - 6, 3, true, false, DIF_HISTORY, false, -1, destPath, k_CreateFolder_History },
      
      { DI_BUTTON, 0, kYSize - 3, 0, 0, false, false, DIF_CENTERGROUP, true, NMessageID::kOk, NULL, NULL },
      { DI_BUTTON, 0, kYSize - 3, 0, 0, false, false, DIF_CENTERGROUP, false, NMessageID::kCancel, NULL, NULL }
    };
   
    const int kNumDialogItems = ARRAY_SIZE(initItems);
    const int kOkButtonIndex = kNumDialogItems - 2;

    FarDialogItem dialogItems[kNumDialogItems];
    g_StartupInfo.InitDialogItems(initItems, dialogItems, kNumDialogItems);
    for (;;)
    {
      int askCode = g_StartupInfo.ShowDialog(kXSize, kYSize,
          NULL, // kHelpTopic
          dialogItems, kNumDialogItems);
      if (askCode != kOkButtonIndex)
        return E_ABORT;
      destPath = dialogItems[kPathIndex].Data;
      destPathU = GetUnicodeString(destPath, CP_OEMCP);
      destPathU.Trim();
      if (!destPathU.IsEmpty())
        break;
      g_StartupInfo.ShowErrorMessage("You must specify folder name");
    }

  }

  CScreenRestorer screenRestorer;
  CProgressBox progressBox;
  CProgressBox *progressBoxPointer = NULL;
  // if ((opMode & OPM_SILENT) == 0 && (opMode & OPM_FIND ) == 0)
  {
    screenRestorer.Save();

    progressBoxPointer = &progressBox;
    progressBox.Init(
        // g_StartupInfo.GetMsgString(NMessageID::kWaitTitle),
        g_StartupInfo.GetMsgString(NMessageID::kDeleting));
  }

  CUpdateCallback100Imp *updateCallbackSpec = new CUpdateCallback100Imp;
  CMyComPtr<IFolderArchiveUpdateCallback> updateCallback(updateCallbackSpec);
  
  updateCallbackSpec->Init(/* m_ArchiveHandler, */ progressBoxPointer);
  updateCallbackSpec->PasswordIsDefined = PasswordIsDefined;
  updateCallbackSpec->Password = Password;

  HRESULT result;
  {
    CMyComPtr<IFolderOperations> folderOperations;
    result = _folder.QueryInterface(IID_IFolderOperations, &folderOperations);
    if (folderOperations)
      result = folderOperations->CreateFolder(destPathU, updateCallback);
    else if (result != S_OK)
      result = E_FAIL;
  }

  if (result != S_OK)
  {
    ShowSysErrorMessage(result);
    return result;
  }

  g_StartupInfo.Control(this, FCTL_UPDATEPANEL, (void *)1);
  g_StartupInfo.Control(this, FCTL_REDRAWPANEL, NULL);
  
  PanelInfo panelInfo;

  if (g_StartupInfo.ControlGetActivePanelInfo(panelInfo))
  {
    const AString destPath (GetOemString(destPathU));
    
    for (int i = 0; i < panelInfo.ItemsNumber; i++)
    {
      const PluginPanelItem &pi = panelInfo.PanelItems[i];
      if (strcmp(destPath, pi.FindData.cFileName) == 0)
      {
        PanelRedrawInfo panelRedrawInfo;
        panelRedrawInfo.CurrentItem = i;
        panelRedrawInfo.TopPanelItem = 0;
        g_StartupInfo.Control(this, FCTL_REDRAWPANEL, &panelRedrawInfo);
        break;
      }
    }
  }

  SetCurrentDirVar();
  return S_OK;
}

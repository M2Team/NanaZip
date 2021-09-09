// PluginDelete.cpp

#include "StdAfx.h"

#include <stdio.h>

#include "../../../Common/StringConvert.h"
#include "FarUtils.h"

#include "Messages.h"
#include "Plugin.h"
#include "UpdateCallbackFar.h"

using namespace NFar;

int CPlugin::DeleteFiles(PluginPanelItem *panelItems, int numItems, int opMode)
{
  if (numItems == 0)
    return FALSE;
  if (_agent->IsThereReadOnlyArc())
  {
    g_StartupInfo.ShowMessage(NMessageID::kUpdateNotSupportedForThisArchive);
    return FALSE;
  }
  if ((opMode & OPM_SILENT) == 0)
  {
    const char *msgItems[]=
    {
      g_StartupInfo.GetMsgString(NMessageID::kDeleteTitle),
      g_StartupInfo.GetMsgString(NMessageID::kDeleteFiles),
      g_StartupInfo.GetMsgString(NMessageID::kDeleteDelete),
      g_StartupInfo.GetMsgString(NMessageID::kDeleteCancel)
    };

    // char msg[1024];
    AString str1;

    if (numItems == 1)
    {
      str1 = g_StartupInfo.GetMsgString(NMessageID::kDeleteFile);
      AString name (panelItems[0].FindData.cFileName);
      const unsigned kSizeLimit = 48;
      if (name.Len() > kSizeLimit)
      {
        UString s = MultiByteToUnicodeString(name, CP_OEMCP);
        ReduceString(s, kSizeLimit);
        name = UnicodeStringToMultiByte(s, CP_OEMCP);
      }
      str1.Replace(AString ("%.40s"), name);
      msgItems[1] = str1;
      // sprintf(msg, g_StartupInfo.GetMsgString(NMessageID::kDeleteFile), panelItems[0].FindData.cFileName);
      // msgItems[2] = msg;
    }
    else if (numItems > 1)
    {
      str1 = g_StartupInfo.GetMsgString(NMessageID::kDeleteNumberOfFiles);
      {
        AString n;
        n.Add_UInt32(numItems);
        str1.Replace(AString ("%d"), n);
      }
      msgItems[1] = str1;
      // sprintf(msg, g_StartupInfo.GetMsgString(NMessageID::kDeleteNumberOfFiles), numItems);
      // msgItems[1] = msg;
    }
    if (g_StartupInfo.ShowMessage(FMSG_WARNING, NULL, msgItems, ARRAY_SIZE(msgItems), 2) != 0)
      return (FALSE);
  }

  CScreenRestorer screenRestorer;
  CProgressBox progressBox;
  CProgressBox *progressBoxPointer = NULL;
  if ((opMode & OPM_SILENT) == 0 && (opMode & OPM_FIND ) == 0)
  {
    screenRestorer.Save();

    progressBoxPointer = &progressBox;
    progressBox.Init(
        // g_StartupInfo.GetMsgString(NMessageID::kWaitTitle),
        g_StartupInfo.GetMsgString(NMessageID::kDeleting));
  }

  /*
  CWorkDirTempFile tempFile;
  if (tempFile.CreateTempFile(m_FileName) != S_OK)
    return FALSE;
  */

  CObjArray<UInt32> indices(numItems);
  int i;
  for (i = 0; i < numItems; i++)
    indices[i] = (UInt32)panelItems[i].UserData;

  /*
  UStringVector pathVector;
  GetPathParts(pathVector);
  
  CMyComPtr<IOutFolderArchive> outArchive;
  HRESULT result = m_ArchiveHandler.QueryInterface(IID_IOutFolderArchive, &outArchive);
  if (result != S_OK)
  {
    g_StartupInfo.ShowMessage(NMessageID::kUpdateNotSupportedForThisArchive);
    return FALSE;
  }
  */

  CUpdateCallback100Imp *updateCallbackSpec = new CUpdateCallback100Imp;
  CMyComPtr<IFolderArchiveUpdateCallback> updateCallback(updateCallbackSpec);
  
  updateCallbackSpec->Init(/* m_ArchiveHandler, */ progressBoxPointer);
  updateCallbackSpec->PasswordIsDefined = PasswordIsDefined;
  updateCallbackSpec->Password = Password;

  /*
  outArchive->SetFolder(_folder);
  result = outArchive->DeleteItems(tempFile.OutStream, indices, numItems, updateCallback);
  updateCallback.Release();
  outArchive.Release();

  if (result == S_OK)
  {
    result = AfterUpdate(tempFile, pathVector);
  }
  */

  HRESULT result;
  {
    CMyComPtr<IFolderOperations> folderOperations;
    result = _folder.QueryInterface(IID_IFolderOperations, &folderOperations);
    if (folderOperations)
      result = folderOperations->Delete(indices, numItems, updateCallback);
    else if (result != S_OK)
      result = E_FAIL;
  }

  if (result != S_OK)
  {
    ShowSysErrorMessage(result);
    return FALSE;
  }

  SetCurrentDirVar();
  return TRUE;
}

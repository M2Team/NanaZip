// Plugin.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/Wildcard.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/PropVariantConv.h"

#include "../Common/PropIDUtils.h"

#include "FarUtils.h"
#include "Messages.h"
#include "Plugin.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;
using namespace NFar;

// This function is unused
int CompareFileNames_ForFolderList(const wchar_t *s1, const wchar_t *s2)
{
  return MyStringCompareNoCase(s1, s2);
}


CPlugin::CPlugin(const FString &fileName, CAgent *agent, UString archiveTypeName):
    _agent(agent),
    m_FileName(fileName),
    _archiveTypeName(archiveTypeName),
    PasswordIsDefined(false)
{
  m_ArchiveHandler = agent;
  if (!m_FileInfo.Find(m_FileName))
    throw "error";
  m_ArchiveHandler->BindToRootFolder(&_folder);
}

CPlugin::~CPlugin() {}

static void MyGetFileTime(IFolderFolder *folder, UInt32 itemIndex,
    PROPID propID, FILETIME &fileTime)
{
  NCOM::CPropVariant prop;
  if (folder->GetProperty(itemIndex, propID, &prop) != S_OK)
    throw 271932;
  if (prop.vt == VT_EMPTY)
  {
    fileTime.dwHighDateTime = 0;
    fileTime.dwLowDateTime = 0;
  }
  else
  {
    if (prop.vt != VT_FILETIME)
      throw 4191730;
    fileTime = prop.filetime;
  }
}

#define kDotsReplaceString "[[..]]"
#define kDotsReplaceStringU L"[[..]]"
  
static void CopyStrLimited(char *dest, const AString &src, unsigned len)
{
  len--;
  if (src.Len() < len)
    len = src.Len();
  memcpy(dest, src, sizeof(dest[0]) * len);
  dest[len] = 0;
}

#define COPY_STR_LIMITED(dest, src) CopyStrLimited(dest, src, ARRAY_SIZE(dest))

void CPlugin::ReadPluginPanelItem(PluginPanelItem &panelItem, UInt32 itemIndex)
{
  NCOM::CPropVariant prop;
  if (_folder->GetProperty(itemIndex, kpidName, &prop) != S_OK)
    throw 271932;

  if (prop.vt != VT_BSTR)
    throw 272340;

  AString oemString (UnicodeStringToMultiByte(prop.bstrVal, CP_OEMCP));
  if (oemString == "..")
    oemString = kDotsReplaceString;

  COPY_STR_LIMITED(panelItem.FindData.cFileName, oemString);
  panelItem.FindData.cAlternateFileName[0] = 0;

  if (_folder->GetProperty(itemIndex, kpidAttrib, &prop) != S_OK)
    throw 271932;
  if (prop.vt == VT_UI4)
    panelItem.FindData.dwFileAttributes  = prop.ulVal;
  else if (prop.vt == VT_EMPTY)
    panelItem.FindData.dwFileAttributes = m_FileInfo.Attrib;
  else
    throw 21631;

  if (_folder->GetProperty(itemIndex, kpidIsDir, &prop) != S_OK)
    throw 271932;
  if (prop.vt == VT_BOOL)
  {
    if (VARIANT_BOOLToBool(prop.boolVal))
      panelItem.FindData.dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
  }
  else if (prop.vt != VT_EMPTY)
    throw 21632;

  if (_folder->GetProperty(itemIndex, kpidSize, &prop) != S_OK)
    throw 271932;
  UInt64 length = 0;
  ConvertPropVariantToUInt64(prop, length);
  panelItem.FindData.nFileSizeLow = (UInt32)length;
  panelItem.FindData.nFileSizeHigh = (UInt32)(length >> 32);

  MyGetFileTime(_folder, itemIndex, kpidCTime, panelItem.FindData.ftCreationTime);
  MyGetFileTime(_folder, itemIndex, kpidATime, panelItem.FindData.ftLastAccessTime);
  MyGetFileTime(_folder, itemIndex, kpidMTime, panelItem.FindData.ftLastWriteTime);

  if (panelItem.FindData.ftLastWriteTime.dwHighDateTime == 0 &&
      panelItem.FindData.ftLastWriteTime.dwLowDateTime == 0)
    panelItem.FindData.ftLastWriteTime = m_FileInfo.MTime;

  if (_folder->GetProperty(itemIndex, kpidPackSize, &prop) != S_OK)
    throw 271932;
  length = 0;
  ConvertPropVariantToUInt64(prop, length);
  panelItem.PackSize = UInt32(length);
  panelItem.PackSizeHigh = UInt32(length >> 32);

  panelItem.Flags = 0;
  panelItem.NumberOfLinks = 0;

  panelItem.Description = NULL;
  panelItem.Owner = NULL;
  panelItem.CustomColumnData = NULL;
  panelItem.CustomColumnNumber = 0;

  panelItem.CRC32 = 0;
  panelItem.Reserved[0] = 0;
  panelItem.Reserved[1] = 0;
}

int CPlugin::GetFindData(PluginPanelItem **panelItems, int *itemsNumber, int opMode)
{
  // CScreenRestorer screenRestorer;
  if ((opMode & OPM_SILENT) == 0 && (opMode & OPM_FIND ) == 0)
  {
    /*
    screenRestorer.Save();
    const char *msgItems[]=
    {
      g_StartupInfo.GetMsgString(NMessageID::kWaitTitle),
        g_StartupInfo.GetMsgString(NMessageID::kReadingList)
    };
    g_StartupInfo.ShowMessage(0, NULL, msgItems, ARRAY_SIZE(msgItems), 0);
    */
  }

  UInt32 numItems;
  _folder->GetNumberOfItems(&numItems);
  *panelItems = new PluginPanelItem[numItems];
  try
  {
    for (UInt32 i = 0; i < numItems; i++)
    {
      PluginPanelItem &panelItem = (*panelItems)[i];
      ReadPluginPanelItem(panelItem, i);
      panelItem.UserData = i;
    }
  }
  catch(...)
  {
    delete [](*panelItems);
    throw;
  }
  *itemsNumber = numItems;
  return(TRUE);
}

void CPlugin::FreeFindData(struct PluginPanelItem *panelItems, int itemsNumber)
{
  for (int i = 0; i < itemsNumber; i++)
    if (panelItems[i].Description != NULL)
      delete []panelItems[i].Description;
  delete []panelItems;
}

void CPlugin::EnterToDirectory(const UString &dirName)
{
  CMyComPtr<IFolderFolder> newFolder;
  UString s = dirName;
  if (dirName == kDotsReplaceStringU)
    s = "..";
  _folder->BindToFolder(s, &newFolder);
  if (!newFolder)
  {
    if (dirName.IsEmpty())
      return;
    else
      throw 40325;
  }
  _folder = newFolder;
}

int CPlugin::SetDirectory(const char *aszDir, int /* opMode */)
{
  UString path = MultiByteToUnicodeString(aszDir, CP_OEMCP);
  if (path == WSTRING_PATH_SEPARATOR)
  {
    _folder.Release();
    m_ArchiveHandler->BindToRootFolder(&_folder);
  }
  else if (path == L"..")
  {
    CMyComPtr<IFolderFolder> newFolder;
    _folder->BindToParentFolder(&newFolder);
    if (!newFolder)
      throw 40312;
    _folder = newFolder;
  }
  else if (path.IsEmpty())
    EnterToDirectory(path);
  else
  {
    if (path[0] == WCHAR_PATH_SEPARATOR)
    {
      _folder.Release();
      m_ArchiveHandler->BindToRootFolder(&_folder);
      path.DeleteFrontal(1);
    }
    UStringVector pathParts;
    SplitPathToParts(path, pathParts);
    FOR_VECTOR (i, pathParts)
      EnterToDirectory(pathParts[i]);
  }
  SetCurrentDirVar();
  return TRUE;
}

void CPlugin::GetPathParts(UStringVector &pathParts)
{
  pathParts.Clear();
  CMyComPtr<IFolderFolder> folderItem = _folder;
  for (;;)
  {
    CMyComPtr<IFolderFolder> newFolder;
    folderItem->BindToParentFolder(&newFolder);
    if (!newFolder)
      break;
    NCOM::CPropVariant prop;
    if (folderItem->GetFolderProperty(kpidName, &prop) == S_OK)
      if (prop.vt == VT_BSTR)
        pathParts.Insert(0, (const wchar_t *)prop.bstrVal);
    folderItem = newFolder;
  }
}

void CPlugin::SetCurrentDirVar()
{
  m_CurrentDir.Empty();

  /*
  // kpidPath path has tail slash, but we don't need it for compatibility with default FAR style
  NCOM::CPropVariant prop;
  if (_folder->GetFolderProperty(kpidPath, &prop) == S_OK)
    if (prop.vt == VT_BSTR)
    {
      m_CurrentDir = (wchar_t *)prop.bstrVal;
      // if (!m_CurrentDir.IsEmpty())
    }
  m_CurrentDir.InsertAtFront(WCHAR_PATH_SEPARATOR);
  */

  UStringVector pathParts;
  GetPathParts(pathParts);
  FOR_VECTOR (i, pathParts)
  {
    m_CurrentDir.Add_PathSepar();
    m_CurrentDir += pathParts[i];
  }
}

static const char * const kPluginFormatName = "7-ZIP";


static int FindPropNameID(PROPID propID)
{
  if (propID > NMessageID::k_Last_PropId_supported_by_plugin)
    return -1;
  return NMessageID::kNoProperty + propID;
}

/*
struct CPropertyIDInfo
{
  PROPID PropID;
  const char *FarID;
  int Width;
  // char CharID;
};

static CPropertyIDInfo kPropertyIDInfos[] =
{
  { kpidName, "N", 0},
  { kpidSize, "S", 8},
  { kpidPackSize, "P", 8},
  { kpidAttrib, "A", 0},
  { kpidCTime, "DC", 14},
  { kpidATime, "DA", 14},
  { kpidMTime, "DM", 14},
  
  { kpidSolid, NULL, 0, 'S'},
  { kpidEncrypted, NULL, 0, 'P'},

  { kpidDictionarySize, IDS_PROPERTY_DICTIONARY_SIZE },
  { kpidSplitBefore, NULL, 'B'},
  { kpidSplitAfter, NULL, 'A'},
  { kpidComment, NULL, 'C'},
  { kpidCRC, IDS_PROPERTY_CRC }
  // { kpidType, L"Type" }
};

static const int kNumPropertyIDInfos = ARRAY_SIZE(kPropertyIDInfos);

static int FindPropertyInfo(PROPID propID)
{
  for (int i = 0; i < kNumPropertyIDInfos; i++)
    if (kPropertyIDInfos[i].PropID == propID)
      return i;
  return -1;
}
*/

// char *g_Titles[] = { "a", "f", "v" };
/*
static void SmartAddToString(AString &destString, const char *srcString)
{
  if (!destString.IsEmpty())
    destString += ',';
  destString += srcString;
}
*/

/*
void CPlugin::AddColumn(PROPID propID)
{
  int index = FindPropertyInfo(propID);
  if (index >= 0)
  {
    for (int i = 0; i < m_ProxyHandler->m_InternalProperties.Size(); i++)
    {
      const CArchiveItemProperty &aHandlerProperty = m_ProxyHandler->m_InternalProperties[i];
      if (aHandlerProperty.ID == propID)
        break;
    }
    if (i == m_ProxyHandler->m_InternalProperties.Size())
      return;

    const CPropertyIDInfo &propertyIDInfo = kPropertyIDInfos[index];
    SmartAddToString(PanelModeColumnTypes, propertyIDInfo.FarID);
    char tmp[32];
    itoa(propertyIDInfo.Width, tmp, 10);
    SmartAddToString(PanelModeColumnWidths, tmp);
    return;
  }
}
*/

static AString GetNameOfProp(PROPID propID, const wchar_t *name)
{
  int farID = FindPropNameID(propID);
  if (farID >= 0)
    return (AString)g_StartupInfo.GetMsgString(farID);
  if (name)
    return UnicodeStringToMultiByte(name, CP_OEMCP);
  char s[16];
  ConvertUInt32ToString(propID, s);
  return (AString)s;
}

static AString GetNameOfProp2(PROPID propID, const wchar_t *name)
{
  AString s (GetNameOfProp(propID, name));
  if (s.Len() > (kInfoPanelLineSize - 1))
    s.DeleteFrom(kInfoPanelLineSize - 1);
  return s;
}

static AString ConvertSizeToString(UInt64 value)
{
  char s[32];
  ConvertUInt64ToString(value, s);
  unsigned i = MyStringLen(s);
  unsigned pos = ARRAY_SIZE(s);
  s[--pos] = 0;
  while (i > 3)
  {
    s[--pos] = s[--i];
    s[--pos] = s[--i];
    s[--pos] = s[--i];
    s[--pos] = ' ';
  }
  while (i > 0)
    s[--pos] = s[--i];
  return (AString)(s + pos);
}

static AString PropToString(const NCOM::CPropVariant &prop, PROPID propID)
{
  if (prop.vt == VT_BSTR)
  {
    AString s (UnicodeStringToMultiByte(prop.bstrVal, CP_OEMCP));
    s.Replace((char)0xA, ' ');
    s.Replace((char)0xD, ' ');
    return s;
  }
  if (prop.vt == VT_BOOL)
  {
    int messageID = VARIANT_BOOLToBool(prop.boolVal) ?
      NMessageID::kYes : NMessageID::kNo;
    return (AString)g_StartupInfo.GetMsgString(messageID);
  }
  if (prop.vt != VT_EMPTY)
  {
    if ((prop.vt == VT_UI8 || prop.vt == VT_UI4) && (
        propID == kpidSize ||
        propID == kpidPackSize ||
        propID == kpidNumSubDirs ||
        propID == kpidNumSubFiles ||
        propID == kpidNumBlocks ||
        propID == kpidPhySize ||
        propID == kpidHeadersSize ||
        propID == kpidClusterSize ||
        propID == kpidUnpackSize
        ))
    {
      UInt64 v = 0;
      ConvertPropVariantToUInt64(prop, v);
      return ConvertSizeToString(v);
    }
    {
      char sz[64];
      ConvertPropertyToShortString2(sz, prop, propID);
      return (AString)sz;
    }
  }
  return AString();
}

static AString PropToString2(const NCOM::CPropVariant &prop, PROPID propID)
{
  AString s (PropToString(prop, propID));
  if (s.Len() > (kInfoPanelLineSize - 1))
    s.DeleteFrom(kInfoPanelLineSize - 1);
  return s;
}

static void AddPropertyString(InfoPanelLine *lines, unsigned &numItems, PROPID propID, const wchar_t *name,
    const NCOM::CPropVariant &prop)
{
  if (prop.vt != VT_EMPTY)
  {
    AString val (PropToString2(prop, propID));
    if (!val.IsEmpty())
    {
      InfoPanelLine &item = lines[numItems++];
      COPY_STR_LIMITED(item.Text, GetNameOfProp2(propID, name));
      COPY_STR_LIMITED(item.Data, val);
    }
  }
}

static void InsertSeparator(InfoPanelLine *lines, unsigned &numItems)
{
  if (numItems < kNumInfoLinesMax)
  {
    InfoPanelLine &item = lines[numItems++];
    *item.Text = 0;
    *item.Data = 0;
    item.Separator = TRUE;
  }
}

void CPlugin::GetOpenPluginInfo(struct OpenPluginInfo *info)
{
  info->StructSize = sizeof(*info);
  info->Flags = OPIF_USEFILTER | OPIF_USESORTGROUPS | OPIF_USEHIGHLIGHTING |
              OPIF_ADDDOTS | OPIF_COMPAREFATTIME;

  COPY_STR_LIMITED(m_FileNameBuffer, UnicodeStringToMultiByte(fs2us(m_FileName), CP_OEMCP));
  info->HostFile = m_FileNameBuffer; // test it it is not static
  
  COPY_STR_LIMITED(m_CurrentDirBuffer, UnicodeStringToMultiByte(m_CurrentDir, CP_OEMCP));
  info->CurDir = m_CurrentDirBuffer;

  info->Format = kPluginFormatName;

  {
  UString name;
  {
    FString dirPrefix, fileName;
    GetFullPathAndSplit(m_FileName, dirPrefix, fileName);
    name = fs2us(fileName);
  }

  m_PannelTitle = ' ';
  m_PannelTitle += _archiveTypeName;
  m_PannelTitle += ':';
  m_PannelTitle += name;
  m_PannelTitle.Add_Space();
  if (!m_CurrentDir.IsEmpty())
  {
    // m_PannelTitle += '\\';
    m_PannelTitle += m_CurrentDir;
  }
 
  COPY_STR_LIMITED(m_PannelTitleBuffer, UnicodeStringToMultiByte(m_PannelTitle, CP_OEMCP));
  info->PanelTitle = m_PannelTitleBuffer;

  }

  memset(m_InfoLines, 0, sizeof(m_InfoLines));
  m_InfoLines[0].Text[0] = 0;
  m_InfoLines[0].Separator = TRUE;

  MyStringCopy(m_InfoLines[1].Text, g_StartupInfo.GetMsgString(NMessageID::kArchiveType));
  MyStringCopy(m_InfoLines[1].Data, (const char *)UnicodeStringToMultiByte(_archiveTypeName, CP_OEMCP));

  unsigned numItems = 2;

  {
    CMyComPtr<IFolderProperties> folderProperties;
    _folder.QueryInterface(IID_IFolderProperties, &folderProperties);
    if (folderProperties)
    {
      UInt32 numProps;
      if (folderProperties->GetNumberOfFolderProperties(&numProps) == S_OK)
      {
        for (UInt32 i = 0; i < numProps && numItems < kNumInfoLinesMax; i++)
        {
          CMyComBSTR name;
          PROPID propID;
          VARTYPE vt;
          if (folderProperties->GetFolderPropertyInfo(i, &name, &propID, &vt) != S_OK)
            continue;
          NCOM::CPropVariant prop;
          if (_folder->GetFolderProperty(propID, &prop) != S_OK || prop.vt == VT_EMPTY)
            continue;
          
          InfoPanelLine &item = m_InfoLines[numItems++];
          COPY_STR_LIMITED(item.Text, GetNameOfProp2(propID, name));
          COPY_STR_LIMITED(item.Data, PropToString2(prop, propID));
        }
      }
    }
  }

  /*
  if (numItems < kNumInfoLinesMax)
  {
    InsertSeparator(m_InfoLines, numItems);
  }
  */

  {
    CMyComPtr<IGetFolderArcProps> getFolderArcProps;
    _folder.QueryInterface(IID_IGetFolderArcProps, &getFolderArcProps);
    if (getFolderArcProps)
    {
      CMyComPtr<IFolderArcProps> getProps;
      getFolderArcProps->GetFolderArcProps(&getProps);
      if (getProps)
      {
        UInt32 numLevels;
        if (getProps->GetArcNumLevels(&numLevels) != S_OK)
          numLevels = 0;
        for (UInt32 level2 = 0; level2 < numLevels; level2++)
        {
          {
            UInt32 level = numLevels - 1 - level2;
            UInt32 numProps;
            if (getProps->GetArcNumProps(level, &numProps) == S_OK)
            {
              InsertSeparator(m_InfoLines, numItems);
              for (Int32 i = -3; i < (Int32)numProps && numItems < kNumInfoLinesMax; i++)
              {
                CMyComBSTR name;
                PROPID propID;
                VARTYPE vt;
                switch (i)
                {
                  case -3: propID = kpidPath; break;
                  case -2: propID = kpidType; break;
                  case -1: propID = kpidError; break;
                  default:
                    if (getProps->GetArcPropInfo(level, i, &name, &propID, &vt) != S_OK)
                      continue;
                }
                NCOM::CPropVariant prop;
                if (getProps->GetArcProp(level, propID, &prop) != S_OK)
                  continue;
                AddPropertyString(m_InfoLines, numItems, propID, name, prop);
              }
            }
          }
          if (level2 != numLevels - 1)
          {
            UInt32 level = numLevels - 1 - level2;
            UInt32 numProps;
            if (getProps->GetArcNumProps2(level, &numProps) == S_OK)
            {
              InsertSeparator(m_InfoLines, numItems);
              for (Int32 i = 0; i < (Int32)numProps && numItems < kNumInfoLinesMax; i++)
              {
                CMyComBSTR name;
                PROPID propID;
                VARTYPE vt;
                if (getProps->GetArcPropInfo2(level, i, &name, &propID, &vt) != S_OK)
                  continue;
                NCOM::CPropVariant prop;
                if (getProps->GetArcProp2(level, propID, &prop) != S_OK)
                  continue;
                AddPropertyString(m_InfoLines, numItems, propID, name, prop);
              }
            }
          }
        }
      }
    }
  }

  //m_InfoLines[1].Separator = 0;

  info->InfoLines = m_InfoLines;
  info->InfoLinesNumber = numItems;

  
  info->DescrFiles = NULL;
  info->DescrFilesNumber = 0;

  PanelModeColumnTypes.Empty();
  PanelModeColumnWidths.Empty();

  /*
  AddColumn(kpidName);
  AddColumn(kpidSize);
  AddColumn(kpidPackSize);
  AddColumn(kpidMTime);
  AddColumn(kpidCTime);
  AddColumn(kpidATime);
  AddColumn(kpidAttrib);
  
  _PanelMode.ColumnTypes = (char *)(const char *)PanelModeColumnTypes;
  _PanelMode.ColumnWidths = (char *)(const char *)PanelModeColumnWidths;
  _PanelMode.ColumnTitles = NULL;
  _PanelMode.FullScreen = TRUE;
  _PanelMode.DetailedStatus = FALSE;
  _PanelMode.AlignExtensions = FALSE;
  _PanelMode.CaseConversion = FALSE;
  _PanelMode.StatusColumnTypes = "N";
  _PanelMode.StatusColumnWidths = "0";
  _PanelMode.Reserved[0] = 0;
  _PanelMode.Reserved[1] = 0;

  info->PanelModesArray = &_PanelMode;
  info->PanelModesNumber = 1;
  */

  info->PanelModesArray = NULL;
  info->PanelModesNumber = 0;

  info->StartPanelMode = 0;
  info->StartSortMode = 0;
  info->KeyBar = NULL;
  info->ShortcutData = NULL;
}

struct CArchiveItemProperty
{
  AString Name;
  PROPID ID;
  VARTYPE Type;
};

static inline char GetHex(Byte value)
{
  return (char)((value < 10) ? ('0' + value) : ('A' + (value - 10)));
}

HRESULT CPlugin::ShowAttributesWindow()
{
  PluginPanelItem pluginPanelItem;
  if (!g_StartupInfo.ControlGetActivePanelCurrentItemInfo(pluginPanelItem))
    return S_FALSE;
  if (strcmp(pluginPanelItem.FindData.cFileName, "..") == 0 &&
        NFind::NAttributes::IsDir(pluginPanelItem.FindData.dwFileAttributes))
    return S_FALSE;
  int itemIndex = (int)pluginPanelItem.UserData;

  CObjectVector<CArchiveItemProperty> properties;
  UInt32 numProps;
  RINOK(_folder->GetNumberOfProperties(&numProps));
  unsigned i;
  for (i = 0; i < numProps; i++)
  {
    CMyComBSTR name;
    PROPID propID;
    VARTYPE vt;
    RINOK(_folder->GetPropertyInfo(i, &name, &propID, &vt));
    CArchiveItemProperty prop;
    prop.Type = vt;
    prop.ID = propID;
    if (prop.ID  == kpidPath)
      prop.ID = kpidName;
    prop.Name = GetNameOfProp(propID, name);
    properties.Add(prop);
  }

  int size = 2;
  CRecordVector<CInitDialogItem> initDialogItems;

  int xSize = 70;
  {
    const CInitDialogItem idi =
      { DI_DOUBLEBOX, 3, 1, xSize - 4, size - 2, false, false, 0, false, NMessageID::kProperties, NULL, NULL };
    initDialogItems.Add(idi);
  }

  AStringVector values;

  const int kStartY = 3;

  for (i = 0; i < properties.Size(); i++)
  {
    const CArchiveItemProperty &property = properties[i];

    int startY = kStartY + values.Size();

    {
      CInitDialogItem idi =
        { DI_TEXT, 5, startY, 0, 0, false, false, 0, false, 0, NULL, NULL };
      idi.DataMessageId = FindPropNameID(property.ID);
      if (idi.DataMessageId < 0)
        idi.DataString = property.Name;
      initDialogItems.Add(idi);
    }
    
    NCOM::CPropVariant prop;
    RINOK(_folder->GetProperty(itemIndex, property.ID, &prop));
    values.Add(PropToString(prop, property.ID));
    
    {
      const CInitDialogItem idi =
        { DI_TEXT, 30, startY, 0, 0, false, false, 0, false, -1, NULL, NULL };
      initDialogItems.Add(idi);
    }
  }

  CMyComPtr<IArchiveGetRawProps> _folderRawProps;
  _folder.QueryInterface(IID_IArchiveGetRawProps, &_folderRawProps);

  CObjectVector<CArchiveItemProperty> properties2;

  if (_folderRawProps)
  {
    _folderRawProps->GetNumRawProps(&numProps);

    for (i = 0; i < numProps; i++)
    {
      CMyComBSTR name;
      PROPID propID;
      if (_folderRawProps->GetRawPropInfo(i, &name, &propID) != S_OK)
        continue;
      CArchiveItemProperty prop;
      prop.Type = VT_EMPTY;
      prop.ID = propID;
      if (prop.ID  == kpidPath)
        prop.ID = kpidName;
      prop.Name = GetNameOfProp(propID, name);
      properties2.Add(prop);
    }

    for (i = 0; i < properties2.Size(); i++)
    {
      const CArchiveItemProperty &property = properties2[i];
      CMyComBSTR name;
      
      const void *data;
      UInt32 dataSize;
      UInt32 propType;
      if (_folderRawProps->GetRawProp(itemIndex, property.ID, &data, &dataSize, &propType) != S_OK)
        continue;
      
      if (dataSize != 0)
      {
        AString s;
        if (property.ID == kpidNtSecure)
          ConvertNtSecureToString((const Byte *)data, dataSize, s);
        else
        {
          const UInt32 kMaxDataSize = 64;
          if (dataSize > kMaxDataSize)
          {
            s += "data:";
            s.Add_UInt32(dataSize);
          }
          else
          {
            for (UInt32 k = 0; k < dataSize; k++)
            {
              Byte b = ((const Byte *)data)[k];
              s += GetHex((Byte)((b >> 4) & 0xF));
              s += GetHex((Byte)(b & 0xF));
            }
          }
        }

        int startY = kStartY + values.Size();

        {
          CInitDialogItem idi =
            { DI_TEXT, 5, startY, 0, 0, false, false, 0, false, 0, NULL, NULL };
          idi.DataMessageId = FindPropNameID(property.ID);
          if (idi.DataMessageId < 0)
            idi.DataString = property.Name;
          initDialogItems.Add(idi);
        }
        
        values.Add(s);
        
        {
          const CInitDialogItem idi =
            { DI_TEXT, 30, startY, 0, 0, false, false, 0, false, -1, NULL, NULL };
          initDialogItems.Add(idi);
        }
      }
    }
  }

  unsigned numLines = values.Size();
  for (i = 0; i < numLines; i++)
  {
    CInitDialogItem &idi = initDialogItems[1 + i * 2 + 1];
    idi.DataString = values[i];
  }
  
  unsigned numDialogItems = initDialogItems.Size();
  
  CObjArray<FarDialogItem> dialogItems(numDialogItems);
  g_StartupInfo.InitDialogItems(&initDialogItems.Front(), dialogItems, numDialogItems);
  
  unsigned maxLen = 0;
  
  for (i = 0; i < numLines; i++)
  {
    FarDialogItem &dialogItem = dialogItems[1 + i * 2];
    unsigned len = (unsigned)strlen(dialogItem.Data);
    if (len > maxLen)
      maxLen = len;
  }
  
  unsigned maxLen2 = 0;
  const unsigned kSpace = 10;
  
  for (i = 0; i < numLines; i++)
  {
    FarDialogItem &dialogItem = dialogItems[1 + i * 2 + 1];
    unsigned len = (int)strlen(dialogItem.Data);
    if (len > maxLen2)
      maxLen2 = len;
    dialogItem.X1 = maxLen + kSpace;
  }
  
  size = numLines + 6;
  xSize = maxLen + kSpace + maxLen2 + 5;
  FarDialogItem &firstDialogItem = dialogItems[0];
  firstDialogItem.Y2 = size - 2;
  firstDialogItem.X2 = xSize - 4;
  
  /* int askCode = */ g_StartupInfo.ShowDialog(xSize, size, NULL, dialogItems, numDialogItems);
  return S_OK;
}

int CPlugin::ProcessKey(int key, unsigned int controlState)
{
  if (key == VK_F7 && controlState == 0)
  {
    CreateFolder();
    return TRUE;
  }

  if (controlState == PKF_CONTROL && key == 'A')
  {
    HRESULT result = ShowAttributesWindow();
    if (result == S_OK)
      return TRUE;
    if (result == S_FALSE)
      return FALSE;
    throw "Error";
  }
  
  if ((controlState & PKF_ALT) != 0 && key == VK_F6)
  {
    FString folderPath;
    if (!GetOnlyDirPrefix(m_FileName, folderPath))
      return FALSE;
    PanelInfo panelInfo;
    g_StartupInfo.ControlGetActivePanelInfo(panelInfo);
    GetFilesReal(panelInfo.SelectedItems,
        panelInfo.SelectedItemsNumber, FALSE,
        UnicodeStringToMultiByte(fs2us(folderPath), CP_OEMCP), OPM_SILENT, true);
    g_StartupInfo.Control(this, FCTL_UPDATEPANEL, NULL);
    g_StartupInfo.Control(this, FCTL_REDRAWPANEL, NULL);
    g_StartupInfo.Control(this, FCTL_UPDATEANOTHERPANEL, NULL);
    g_StartupInfo.Control(this, FCTL_REDRAWANOTHERPANEL, NULL);
    return TRUE;
  }

  return FALSE;
}

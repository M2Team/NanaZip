// UpdateGUI.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/StringToInt.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/Thread.h"

#include "../Common/WorkDir.h"

#include "../Explorer/MyMessages.h"

#include "../FileManager/LangUtils.h"
#include "../FileManager/StringUtils.h"
#include "../FileManager/resourceGui.h"

#include "CompressDialog.h"
#include "UpdateGUI.h"

#include "resource2.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;

static const char * const kDefaultSfxModule = "7z.sfx";
static const char * const kSFXExtension = "exe";

extern void AddMessageToString(UString &dest, const UString &src);

UString HResultToMessage(HRESULT errorCode);

class CThreadUpdating: public CProgressThreadVirt
{
  HRESULT ProcessVirt();
public:
  CCodecs *codecs;
  const CObjectVector<COpenType> *formatIndices;
  const UString *cmdArcPath;
  CUpdateCallbackGUI *UpdateCallbackGUI;
  NWildcard::CCensor *WildcardCensor;
  CUpdateOptions *Options;
  bool needSetPath;
};
 
HRESULT CThreadUpdating::ProcessVirt()
{
  CUpdateErrorInfo ei;
  HRESULT res = UpdateArchive(codecs, *formatIndices, *cmdArcPath,
      *WildcardCensor, *Options,
      ei, UpdateCallbackGUI, UpdateCallbackGUI, needSetPath);
  FinalMessage.ErrorMessage.Message = ei.Message.Ptr();
  ErrorPaths = ei.FileNames;
  if (res != S_OK)
    return res;
  return HRESULT_FROM_WIN32(ei.SystemError);
}

static void AddProp(CObjectVector<CProperty> &properties, const char *name, const UString &value)
{
  CProperty prop;
  prop.Name = name;
  prop.Value = value;
  properties.Add(prop);
}

static void AddProp(CObjectVector<CProperty> &properties, const char *name, UInt32 value)
{
  char tmp[32];
  ConvertUInt64ToString(value, tmp);
  AddProp(properties, name, UString(tmp));
}

static void AddProp(CObjectVector<CProperty> &properties, const char *name, bool value)
{
  AddProp(properties, name, UString(value ? "on": "off"));
}

static bool IsThereMethodOverride(bool is7z, const UString &propertiesString)
{
  UStringVector strings;
  SplitString(propertiesString, strings);
  FOR_VECTOR (i, strings)
  {
    const UString &s = strings[i];
    if (is7z)
    {
      const wchar_t *end;
      UInt64 n = ConvertStringToUInt64(s, &end);
      if (n == 0 && *end == L'=')
        return true;
    }
    else
    {
      if (s.Len() > 0)
        if (s[0] == L'm' && s[1] == L'=')
          return true;
    }
  }
  return false;
}

static void ParseAndAddPropertires(CObjectVector<CProperty> &properties,
    const UString &propertiesString)
{
  UStringVector strings;
  SplitString(propertiesString, strings);
  FOR_VECTOR (i, strings)
  {
    const UString &s = strings[i];
    CProperty property;
    int index = s.Find(L'=');
    if (index < 0)
      property.Name = s;
    else
    {
      property.Name.SetFrom(s, index);
      property.Value = s.Ptr(index + 1);
    }
    properties.Add(property);
  }
}

static UString GetNumInBytesString(UInt64 v)
{
  char s[32];
  ConvertUInt64ToString(v, s);
  size_t len = MyStringLen(s);
  s[len++] = 'B';
  s[len] = '\0';
  return UString(s);
}

static void SetOutProperties(
    CObjectVector<CProperty> &properties,
    bool is7z,
    UInt32 level,
    bool setMethod,
    const UString &method,
    UInt64 dict64,
    bool orderMode,
    UInt32 order,
    bool solidIsSpecified, UInt64 solidBlockSize,
    bool multiThreadIsAllowed, UInt32 numThreads,
    const UString &encryptionMethod,
    bool encryptHeadersIsAllowed, bool encryptHeaders,
    bool /* sfxMode */)
{
  if (level != (UInt32)(Int32)-1)
    AddProp(properties, "x", (UInt32)level);
  if (setMethod)
  {
    if (!method.IsEmpty())
      AddProp(properties, is7z ? "0": "m", method);
    if (dict64 != (UInt64)(Int64)-1)
    {
      AString name;
      if (is7z)
        name = "0";
      name += (orderMode ? "mem" : "d");
      AddProp(properties, name, GetNumInBytesString(dict64));
    }
    if (order != (UInt32)(Int32)-1)
    {
      AString name;
      if (is7z)
        name = "0";
      name += (orderMode ? "o" : "fb");
      AddProp(properties, name, (UInt32)order);
    }
  }
    
  if (!encryptionMethod.IsEmpty())
    AddProp(properties, "em", encryptionMethod);

  if (encryptHeadersIsAllowed)
    AddProp(properties, "he", encryptHeaders);
  if (solidIsSpecified)
    AddProp(properties, "s", GetNumInBytesString(solidBlockSize));
  if (multiThreadIsAllowed)
    AddProp(properties, "mt", numThreads);
}

struct C_UpdateMode_ToAction_Pair
{
  NCompressDialog::NUpdateMode::EEnum UpdateMode;
  const NUpdateArchive::CActionSet *ActionSet;
};

static const C_UpdateMode_ToAction_Pair g_UpdateMode_Pairs[] =
{
  { NCompressDialog::NUpdateMode::kAdd,    &NUpdateArchive::k_ActionSet_Add },
  { NCompressDialog::NUpdateMode::kUpdate, &NUpdateArchive::k_ActionSet_Update },
  { NCompressDialog::NUpdateMode::kFresh,  &NUpdateArchive::k_ActionSet_Fresh },
  { NCompressDialog::NUpdateMode::kSync,   &NUpdateArchive::k_ActionSet_Sync }
};

static int FindActionSet(const NUpdateArchive::CActionSet &actionSet)
{
  for (unsigned i = 0; i < ARRAY_SIZE(g_UpdateMode_Pairs); i++)
    if (actionSet.IsEqualTo(*g_UpdateMode_Pairs[i].ActionSet))
      return i;
  return -1;
}

static int FindUpdateMode(NCompressDialog::NUpdateMode::EEnum mode)
{
  for (unsigned i = 0; i < ARRAY_SIZE(g_UpdateMode_Pairs); i++)
    if (mode == g_UpdateMode_Pairs[i].UpdateMode)
      return i;
  return -1;
}


static HRESULT ShowDialog(
    CCodecs *codecs,
    const CObjectVector<NWildcard::CCensorPath> &censor,
    CUpdateOptions &options,
    CUpdateCallbackGUI *callback, HWND hwndParent)
{
  if (options.Commands.Size() != 1)
    throw "It must be one command";
  /*
  FString currentDirPrefix;
  #ifndef UNDER_CE
  {
    if (!MyGetCurrentDirectory(currentDirPrefix))
      return E_FAIL;
    NName::NormalizeDirPathPrefix(currentDirPrefix);
  }
  #endif
  */

  bool oneFile = false;
  NFind::CFileInfo fileInfo;
  UString name;
  
  /*
  if (censor.Pairs.Size() > 0)
  {
    const NWildcard::CPair &pair = censor.Pairs[0];
    if (pair.Head.IncludeItems.Size() > 0)
    {
      const NWildcard::CItem &item = pair.Head.IncludeItems[0];
      if (item.ForFile)
      {
        name = pair.Prefix;
        FOR_VECTOR (i, item.PathParts)
        {
          if (i > 0)
            name.Add_PathSepar();
          name += item.PathParts[i];
        }
        if (fileInfo.Find(us2fs(name)))
        {
          if (censor.Pairs.Size() == 1 && pair.Head.IncludeItems.Size() == 1)
            oneFile = !fileInfo.IsDir();
        }
      }
    }
  }
  */
  if (censor.Size() > 0)
  {
    const NWildcard::CCensorPath &cp = censor[0];
    if (cp.Include)
    {
      {
        if (fileInfo.Find(us2fs(cp.Path)))
        {
          if (censor.Size() == 1)
            oneFile = !fileInfo.IsDir();
        }
      }
    }
  }

  
  #if defined(_WIN32) && !defined(UNDER_CE)
  CCurrentDirRestorer curDirRestorer;
  #endif
  CCompressDialog dialog;
  NCompressDialog::CInfo &di = dialog.Info;
  dialog.ArcFormats = &codecs->Formats;

  if (options.MethodMode.Type_Defined)
    di.FormatIndex = options.MethodMode.Type.FormatIndex;
  
  FOR_VECTOR (i, codecs->Formats)
  {
    const CArcInfoEx &ai = codecs->Formats[i];
    if (!ai.UpdateEnabled)
      continue;
    if (!oneFile && ai.Flags_KeepName())
      continue;
    if ((int)i != di.FormatIndex)
      if (ai.Name.IsEqualTo_Ascii_NoCase("swfc"))
        if (!oneFile || name.Len() < 4 || !StringsAreEqualNoCase_Ascii(name.RightPtr(4), ".swf"))
          continue;
    dialog.ArcIndices.Add(i);
  }
  if (dialog.ArcIndices.IsEmpty())
  {
    ShowErrorMessage(L"No Update Engines");
    return E_FAIL;
  }

  // di.ArchiveName = options.ArchivePath.GetFinalPath();
  di.ArcPath = options.ArchivePath.GetPathWithoutExt();
  dialog.OriginalFileName = fs2us(fileInfo.Name);

  di.PathMode = options.PathMode;
    
  // di.CurrentDirPrefix = currentDirPrefix;
  di.SFXMode = options.SfxMode;
  di.OpenShareForWrite = options.OpenShareForWrite;
  di.DeleteAfterCompressing = options.DeleteAfterCompressing;

  di.SymLinks = options.SymLinks;
  di.HardLinks = options.HardLinks;
  di.AltStreams = options.AltStreams;
  di.NtSecurity = options.NtSecurity;
  
  if (callback->PasswordIsDefined)
    di.Password = callback->Password;
    
  di.KeepName = !oneFile;

  NUpdateArchive::CActionSet &actionSet = options.Commands.Front().ActionSet;
 
  {
    int index = FindActionSet(actionSet);
    if (index < 0)
      return E_NOTIMPL;
    di.UpdateMode = g_UpdateMode_Pairs[(unsigned)index].UpdateMode;
  }

  if (dialog.Create(hwndParent) != IDOK)
    return E_ABORT;

  options.DeleteAfterCompressing = di.DeleteAfterCompressing;

  options.SymLinks = di.SymLinks;
  options.HardLinks = di.HardLinks;
  options.AltStreams = di.AltStreams;
  options.NtSecurity = di.NtSecurity;
 
  #if defined(_WIN32) && !defined(UNDER_CE)
  curDirRestorer.NeedRestore = dialog.CurrentDirWasChanged;
  #endif
  
  options.VolumesSizes = di.VolumeSizes;
  /*
  if (di.VolumeSizeIsDefined)
  {
    MyMessageBox(L"Splitting to volumes is not supported");
    return E_FAIL;
  }
  */

 
  {
    int index = FindUpdateMode(di.UpdateMode);
    if (index < 0)
      return E_FAIL;
    actionSet = *g_UpdateMode_Pairs[index].ActionSet;
  }

  options.PathMode = di.PathMode;

  const CArcInfoEx &archiverInfo = codecs->Formats[di.FormatIndex];
  callback->PasswordIsDefined = (!di.Password.IsEmpty());
  if (callback->PasswordIsDefined)
    callback->Password = di.Password;

  options.MethodMode.Properties.Clear();

  bool is7z = archiverInfo.Name.IsEqualTo_Ascii_NoCase("7z");
  bool methodOverride = IsThereMethodOverride(is7z, di.Options);

  SetOutProperties(
      options.MethodMode.Properties,
      is7z,
      di.Level,
      !methodOverride,
      di.Method,
      di.Dict64,
      di.OrderMode, di.Order,
      di.SolidIsSpecified, di.SolidBlockSize,
      di.MultiThreadIsAllowed, di.NumThreads,
      di.EncryptionMethod,
      di.EncryptHeadersIsAllowed, di.EncryptHeaders,
      di.SFXMode);
  
  options.OpenShareForWrite = di.OpenShareForWrite;
  ParseAndAddPropertires(options.MethodMode.Properties, di.Options);

  if (di.SFXMode)
    options.SfxMode = true;
  options.MethodMode.Type = COpenType();
  options.MethodMode.Type_Defined = true;
  options.MethodMode.Type.FormatIndex = di.FormatIndex;

  options.ArchivePath.VolExtension = archiverInfo.GetMainExt();
  if (di.SFXMode)
    options.ArchivePath.BaseExtension = kSFXExtension;
  else
    options.ArchivePath.BaseExtension = options.ArchivePath.VolExtension;
  options.ArchivePath.ParseFromPath(di.ArcPath, k_ArcNameMode_Smart);

  NWorkDir::CInfo workDirInfo;
  workDirInfo.Load();
  options.WorkingDir.Empty();
  if (workDirInfo.Mode != NWorkDir::NMode::kCurrent)
  {
    FString fullPath;
    MyGetFullPathName(us2fs(di.ArcPath), fullPath);
    FString namePart;
    options.WorkingDir = GetWorkDir(workDirInfo, fullPath, namePart);
    CreateComplexDir(options.WorkingDir);
  }
  return S_OK;
}

HRESULT UpdateGUI(
    CCodecs *codecs,
    const CObjectVector<COpenType> &formatIndices,
    const UString &cmdArcPath,
    NWildcard::CCensor &censor,
    CUpdateOptions &options,
    bool showDialog,
    bool &messageWasDisplayed,
    CUpdateCallbackGUI *callback,
    HWND hwndParent)
{
  messageWasDisplayed = false;
  bool needSetPath  = true;
  if (showDialog)
  {
    RINOK(ShowDialog(codecs, censor.CensorPaths, options, callback, hwndParent));
    needSetPath = false;
  }
  if (options.SfxMode && options.SfxModule.IsEmpty())
  {
    options.SfxModule = NWindows::NDLL::GetModuleDirPrefix();
    options.SfxModule += kDefaultSfxModule;
  }

  CThreadUpdating tu;

  tu.needSetPath = needSetPath;

  tu.codecs = codecs;
  tu.formatIndices = &formatIndices;
  tu.cmdArcPath = &cmdArcPath;

  tu.UpdateCallbackGUI = callback;
  tu.UpdateCallbackGUI->ProgressDialog = &tu;
  tu.UpdateCallbackGUI->Init();

  UString title = LangString(IDS_PROGRESS_COMPRESSING);

  /*
  if (hwndParent != 0)
  {
    tu.ProgressDialog.MainWindow = hwndParent;
    // tu.ProgressDialog.MainTitle = fileName;
    tu.ProgressDialog.MainAddTitle = title + L' ';
  }
  */

  tu.WildcardCensor = &censor;
  tu.Options = &options;
  tu.IconID = IDI_ICON;

  RINOK(tu.Create(title, hwndParent));

  messageWasDisplayed = tu.ThreadFinishedOK && tu.MessagesDisplayed;
  return tu.Result;
}

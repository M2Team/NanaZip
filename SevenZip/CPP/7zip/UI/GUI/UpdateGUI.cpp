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

static const char * const kDefaultSfxModule = "NanaZip.Core.Windows.sfx";
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


// parse command line properties

static bool ParseProp_Time_BoolPair(const CProperty &prop, const char *name, CBoolPair &bp)
{
  if (!prop.Name.IsPrefixedBy_Ascii_NoCase(name))
    return false;
  const UString rem = prop.Name.Ptr((unsigned)strlen(name));
  UString val = prop.Value;
  if (!rem.IsEmpty())
  {
    if (!val.IsEmpty())
      return true;
    val = rem;
  }
  bool res;
  if (StringToBool(val, res))
  {
    bp.Val = res;
    bp.Def = true;
  }
  return true;
}

static void ParseProp(
    const CProperty &prop,
    NCompressDialog::CInfo &di)
{
  if (ParseProp_Time_BoolPair(prop, "tm", di.MTime)) return;
  if (ParseProp_Time_BoolPair(prop, "tc", di.CTime)) return;
  if (ParseProp_Time_BoolPair(prop, "ta", di.ATime)) return;
}

static void ParseProperties(
    const CObjectVector<CProperty> &properties,
    NCompressDialog::CInfo &di)
{
  FOR_VECTOR (i, properties)
  {
    ParseProp(properties[i], di);
  }
}





static void AddProp_UString(CObjectVector<CProperty> &properties, const char *name, const UString &value)
{
  CProperty prop;
  prop.Name = name;
  prop.Value = value;
  properties.Add(prop);
}

static void AddProp_UInt32(CObjectVector<CProperty> &properties, const char *name, UInt32 value)
{
  UString s;
  s.Add_UInt32(value);
  AddProp_UString(properties, name, s);
}

static void AddProp_bool(CObjectVector<CProperty> &properties, const char *name, bool value)
{
  AddProp_UString(properties, name, UString(value ? "on": "off"));
}


static void AddProp_BoolPair(CObjectVector<CProperty> &properties,
    const char *name, const CBoolPair &bp)
{
  if (bp.Def)
    AddProp_bool(properties, name, bp.Val);
}



static void SplitOptionsToStrings(const UString &src, UStringVector &strings)
{
  SplitString(src, strings);
  FOR_VECTOR (i, strings)
  {
    UString &s = strings[i];
    if (s.Len() > 2
        && s[0] == '-'
        && MyCharLower_Ascii(s[1]) == 'm')
      s.DeleteFrontal(2);
  }
}

static bool IsThereMethodOverride(bool is7z, const UStringVector &strings)
{
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
    const UStringVector &strings)
{
  FOR_VECTOR (i, strings)
  {
    const UString &s = strings[i];
    CProperty property;
    const int index = s.Find(L'=');
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


static void AddProp_Size(CObjectVector<CProperty> &properties, const char *name, const UInt64 size)
{
  UString s;
  s.Add_UInt64(size);
  s += 'b';
  AddProp_UString(properties, name, s);
}


static void SetOutProperties(
    CObjectVector<CProperty> &properties,
    const NCompressDialog::CInfo &di,
    bool is7z,
    bool setMethod)
{
  if (di.Level != (UInt32)(Int32)-1)
    AddProp_UInt32(properties, "x", (UInt32)di.Level);
  if (setMethod)
  {
    if (!di.Method.IsEmpty())
      AddProp_UString(properties, is7z ? "0": "m", di.Method);
    if (di.Dict64 != (UInt64)(Int64)-1)
    {
      AString name;
      if (is7z)
        name = "0";
      name += (di.OrderMode ? "mem" : "d");
      AddProp_Size(properties, name, di.Dict64);
    }
    if (di.Order != (UInt32)(Int32)-1)
    {
      AString name;
      if (is7z)
        name = "0";
      name += (di.OrderMode ? "o" : "fb");
      AddProp_UInt32(properties, name, (UInt32)di.Order);
    }
  }
    
  if (!di.EncryptionMethod.IsEmpty())
    AddProp_UString(properties, "em", di.EncryptionMethod);

  if (di.EncryptHeadersIsAllowed)
    AddProp_bool(properties, "he", di.EncryptHeaders);

  if (di.SolidIsSpecified)
    AddProp_Size(properties, "s", di.SolidBlockSize);
  
  if (
      // di.MultiThreadIsAllowed &&
      di.NumThreads != (UInt32)(Int32)-1)
    AddProp_UInt32(properties, "mt", di.NumThreads);
  
  const NCompression::CMemUse &memUse = di.MemUsage;
  if (memUse.IsDefined)
  {
    const char *kMemUse = "memuse";
    if (memUse.IsPercent)
    {
      UString s;
      // s += 'p'; // for debug: alternate percent method
      s.Add_UInt64(memUse.Val);
      s += '%';
      AddProp_UString(properties, kMemUse, s);
    }
    else
      AddProp_Size(properties, kMemUse, memUse.Val);
  }

  AddProp_BoolPair(properties, "tm", di.MTime);
  AddProp_BoolPair(properties, "tc", di.CTime);
  AddProp_BoolPair(properties, "ta", di.ATime);

  if (di.TimePrec != (UInt32)(Int32)-1)
    AddProp_UInt32(properties, "tp", di.TimePrec);
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
  {
    CObjectVector<CCodecInfoUser> userCodecs;
    codecs->Get_CodecsInfoUser_Vector(userCodecs);
    dialog.SetMethods(userCodecs);
  }

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
    {
      if (ai.Flags_HashHandler())
        continue;
      if (ai.Name.IsEqualTo_Ascii_NoCase("swfc"))
        if (!oneFile || name.Len() < 4 || !StringsAreEqualNoCase_Ascii(name.RightPtr(4), ".swf"))
          continue;
    }
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
  if (options.SetArcMTime)
    di.SetArcMTime.SetTrueTrue();
  if (options.PreserveATime)
    di.PreserveATime.SetTrueTrue();
  
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

  ParseProperties(options.MethodMode.Properties, di);

  if (dialog.Create(hwndParent) != IDOK)
    return E_ABORT;

  options.DeleteAfterCompressing = di.DeleteAfterCompressing;

  options.SymLinks = di.SymLinks;
  options.HardLinks = di.HardLinks;
  options.AltStreams = di.AltStreams;
  options.NtSecurity = di.NtSecurity;
  options.SetArcMTime = di.SetArcMTime.Val;
  if (di.PreserveATime.Def)
    options.PreserveATime = di.PreserveATime.Val;
 
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

  // we clear command line options, and fill options form Dialog
  options.MethodMode.Properties.Clear();

  const bool is7z = archiverInfo.Is_7z();

  UStringVector optionStrings;
  SplitOptionsToStrings(di.Options, optionStrings);
  const bool methodOverride = IsThereMethodOverride(is7z, optionStrings);

  SetOutProperties(options.MethodMode.Properties, di,
      is7z,
      !methodOverride); // setMethod
  
  options.OpenShareForWrite = di.OpenShareForWrite;
  ParseAndAddPropertires(options.MethodMode.Properties, optionStrings);

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
  if (!formatIndices.IsEmpty())
  {
    const int fin = formatIndices[0].FormatIndex;
    if (fin >= 0)
      if (codecs->Formats[fin].Flags_HashHandler())
        title = LangString(IDS_CHECKSUM_CALCULATING);
  }

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

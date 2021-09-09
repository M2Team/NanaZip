// ZipRegistry.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/Registry.h"
#include "../../../Windows/Synchronization.h"

#include "ZipRegistry.h"

using namespace NWindows;
using namespace NRegistry;

static NSynchronization::CCriticalSection g_CS;
#define CS_LOCK NSynchronization::CCriticalSectionLock lock(g_CS);

static LPCTSTR const kCuPrefix = TEXT("Software") TEXT(STRING_PATH_SEPARATOR) TEXT("7-Zip") TEXT(STRING_PATH_SEPARATOR);

static CSysString GetKeyPath(LPCTSTR path) { return kCuPrefix + (CSysString)path; }

static LONG OpenMainKey(CKey &key, LPCTSTR keyName)
{
  return key.Open(HKEY_CURRENT_USER, GetKeyPath(keyName), KEY_READ);
}

static LONG CreateMainKey(CKey &key, LPCTSTR keyName)
{
  return key.Create(HKEY_CURRENT_USER, GetKeyPath(keyName));
}

static void Key_Set_BoolPair(CKey &key, LPCTSTR name, const CBoolPair &b)
{
  if (b.Def)
    key.SetValue(name, b.Val);
}

static void Key_Get_BoolPair(CKey &key, LPCTSTR name, CBoolPair &b)
{
  b.Val = false;
  b.Def = (key.GetValue_IfOk(name, b.Val) == ERROR_SUCCESS);
}

static void Key_Get_BoolPair_true(CKey &key, LPCTSTR name, CBoolPair &b)
{
  b.Val = true;
  b.Def = (key.GetValue_IfOk(name, b.Val) == ERROR_SUCCESS);
}

namespace NExtract
{

static LPCTSTR const kKeyName = TEXT("Extraction");

static LPCTSTR const kExtractMode = TEXT("ExtractMode");
static LPCTSTR const kOverwriteMode = TEXT("OverwriteMode");
static LPCTSTR const kShowPassword = TEXT("ShowPassword");
static LPCTSTR const kPathHistory = TEXT("PathHistory");
static LPCTSTR const kSplitDest = TEXT("SplitDest");
static LPCTSTR const kElimDup = TEXT("ElimDup");
// static LPCTSTR const kAltStreams = TEXT("AltStreams");
static LPCTSTR const kNtSecur = TEXT("Security");

void CInfo::Save() const
{
  CS_LOCK
  CKey key;
  CreateMainKey(key, kKeyName);

  if (PathMode_Force)
    key.SetValue(kExtractMode, (UInt32)PathMode);
  if (OverwriteMode_Force)
    key.SetValue(kOverwriteMode, (UInt32)OverwriteMode);

  Key_Set_BoolPair(key, kSplitDest, SplitDest);
  Key_Set_BoolPair(key, kElimDup, ElimDup);
  // Key_Set_BoolPair(key, kAltStreams, AltStreams);
  Key_Set_BoolPair(key, kNtSecur, NtSecurity);
  Key_Set_BoolPair(key, kShowPassword, ShowPassword);

  key.RecurseDeleteKey(kPathHistory);
  key.SetValue_Strings(kPathHistory, Paths);
}

void Save_ShowPassword(bool showPassword)
{
  CS_LOCK
  CKey key;
  CreateMainKey(key, kKeyName);
  key.SetValue(kShowPassword, showPassword);
}

void CInfo::Load()
{
  PathMode = NPathMode::kCurPaths;
  PathMode_Force = false;
  OverwriteMode = NOverwriteMode::kAsk;
  OverwriteMode_Force = false;
  
  SplitDest.Val = true;

  Paths.Clear();

  CS_LOCK
  CKey key;
  if (OpenMainKey(key, kKeyName) != ERROR_SUCCESS)
    return;
  
  key.GetValue_Strings(kPathHistory, Paths);
  UInt32 v;
  if (key.QueryValue(kExtractMode, v) == ERROR_SUCCESS && v <= NPathMode::kAbsPaths)
  {
    PathMode = (NPathMode::EEnum)v;
    PathMode_Force = true;
  }
  if (key.QueryValue(kOverwriteMode, v) == ERROR_SUCCESS && v <= NOverwriteMode::kRenameExisting)
  {
    OverwriteMode = (NOverwriteMode::EEnum)v;
    OverwriteMode_Force = true;
  }

  Key_Get_BoolPair_true(key, kSplitDest, SplitDest);

  Key_Get_BoolPair(key, kElimDup, ElimDup);
  // Key_Get_BoolPair(key, kAltStreams, AltStreams);
  Key_Get_BoolPair(key, kNtSecur, NtSecurity);
  Key_Get_BoolPair(key, kShowPassword, ShowPassword);
}

bool Read_ShowPassword()
{
  CS_LOCK
  CKey key;
  bool showPassword = false;
  if (OpenMainKey(key, kKeyName) != ERROR_SUCCESS)
    return showPassword;
  key.GetValue_IfOk(kShowPassword, showPassword);
  return showPassword;
}

}

namespace NCompression
{

static LPCTSTR const kKeyName = TEXT("Compression");

static LPCTSTR const kArcHistory = TEXT("ArcHistory");
static LPCWSTR const kArchiver = L"Archiver";
static LPCTSTR const kShowPassword = TEXT("ShowPassword");
static LPCTSTR const kEncryptHeaders = TEXT("EncryptHeaders");

static LPCTSTR const kOptionsKeyName = TEXT("Options");

static LPCTSTR const kLevel = TEXT("Level");
static LPCTSTR const kDictionary = TEXT("Dictionary");
static LPCTSTR const kOrder = TEXT("Order");
static LPCTSTR const kBlockSize = TEXT("BlockSize");
static LPCTSTR const kNumThreads = TEXT("NumThreads");
static LPCWSTR const kMethod = L"Method";
static LPCWSTR const kOptions = L"Options";
static LPCWSTR const kEncryptionMethod = L"EncryptionMethod";

static LPCTSTR const kNtSecur = TEXT("Security");
static LPCTSTR const kAltStreams = TEXT("AltStreams");
static LPCTSTR const kHardLinks = TEXT("HardLinks");
static LPCTSTR const kSymLinks = TEXT("SymLinks");

static void SetRegString(CKey &key, LPCWSTR name, const UString &value)
{
  if (value.IsEmpty())
    key.DeleteValue(name);
  else
    key.SetValue(name, value);
}

static void SetRegUInt32(CKey &key, LPCTSTR name, UInt32 value)
{
  if (value == (UInt32)(Int32)-1)
    key.DeleteValue(name);
  else
    key.SetValue(name, value);
}

static void GetRegString(CKey &key, LPCWSTR name, UString &value)
{
  if (key.QueryValue(name, value) != ERROR_SUCCESS)
    value.Empty();
}

static void GetRegUInt32(CKey &key, LPCTSTR name, UInt32 &value)
{
  if (key.QueryValue(name, value) != ERROR_SUCCESS)
    value = (UInt32)(Int32)-1;
}

void CInfo::Save() const
{
  CS_LOCK

  CKey key;
  CreateMainKey(key, kKeyName);

  Key_Set_BoolPair(key, kNtSecur, NtSecurity);
  Key_Set_BoolPair(key, kAltStreams, AltStreams);
  Key_Set_BoolPair(key, kHardLinks, HardLinks);
  Key_Set_BoolPair(key, kSymLinks, SymLinks);
  
  key.SetValue(kShowPassword, ShowPassword);
  key.SetValue(kLevel, (UInt32)Level);
  key.SetValue(kArchiver, ArcType);
  key.SetValue(kShowPassword, ShowPassword);
  key.SetValue(kEncryptHeaders, EncryptHeaders);
  key.RecurseDeleteKey(kArcHistory);
  key.SetValue_Strings(kArcHistory, ArcPaths);

  key.RecurseDeleteKey(kOptionsKeyName);
  {
    CKey optionsKey;
    optionsKey.Create(key, kOptionsKeyName);
    FOR_VECTOR (i, Formats)
    {
      const CFormatOptions &fo = Formats[i];
      CKey fk;
      fk.Create(optionsKey, fo.FormatID);
      
      SetRegUInt32(fk, kLevel, fo.Level);
      SetRegUInt32(fk, kDictionary, fo.Dictionary);
      SetRegUInt32(fk, kOrder, fo.Order);
      SetRegUInt32(fk, kBlockSize, fo.BlockLogSize);
      SetRegUInt32(fk, kNumThreads, fo.NumThreads);

      SetRegString(fk, kMethod, fo.Method);
      SetRegString(fk, kOptions, fo.Options);
      SetRegString(fk, kEncryptionMethod, fo.EncryptionMethod);
    }
  }
}

void CInfo::Load()
{
  ArcPaths.Clear();
  Formats.Clear();

  Level = 5;
  ArcType = L"7z";
  ShowPassword = false;
  EncryptHeaders = false;

  CS_LOCK
  CKey key;

  if (OpenMainKey(key, kKeyName) != ERROR_SUCCESS)
    return;

  Key_Get_BoolPair(key, kNtSecur, NtSecurity);
  Key_Get_BoolPair(key, kAltStreams, AltStreams);
  Key_Get_BoolPair(key, kHardLinks, HardLinks);
  Key_Get_BoolPair(key, kSymLinks, SymLinks);

  key.GetValue_Strings(kArcHistory, ArcPaths);
  
  {
    CKey optionsKey;
    if (optionsKey.Open(key, kOptionsKeyName, KEY_READ) == ERROR_SUCCESS)
    {
      CSysStringVector formatIDs;
      optionsKey.EnumKeys(formatIDs);
      FOR_VECTOR (i, formatIDs)
      {
        CKey fk;
        CFormatOptions fo;
        fo.FormatID = formatIDs[i];
        if (fk.Open(optionsKey, fo.FormatID, KEY_READ) == ERROR_SUCCESS)
        {
          GetRegString(fk, kOptions, fo.Options);
          GetRegString(fk, kMethod, fo.Method);
          GetRegString(fk, kEncryptionMethod, fo.EncryptionMethod);

          GetRegUInt32(fk, kLevel, fo.Level);
          GetRegUInt32(fk, kDictionary, fo.Dictionary);
          GetRegUInt32(fk, kOrder, fo.Order);
          GetRegUInt32(fk, kBlockSize, fo.BlockLogSize);
          GetRegUInt32(fk, kNumThreads, fo.NumThreads);

          Formats.Add(fo);
        }
      }
    }
  }

  UString a;
  if (key.QueryValue(kArchiver, a) == ERROR_SUCCESS)
    ArcType = a;
  key.GetValue_IfOk(kLevel, Level);
  key.GetValue_IfOk(kShowPassword, ShowPassword);
  key.GetValue_IfOk(kEncryptHeaders, EncryptHeaders);
}

}

static LPCTSTR const kOptionsInfoKeyName = TEXT("Options");

namespace NWorkDir
{
static LPCTSTR const kWorkDirType = TEXT("WorkDirType");
static LPCWSTR const kWorkDirPath = L"WorkDirPath";
static LPCTSTR const kTempRemovableOnly = TEXT("TempRemovableOnly");


void CInfo::Save()const
{
  CS_LOCK
  CKey key;
  CreateMainKey(key, kOptionsInfoKeyName);
  key.SetValue(kWorkDirType, (UInt32)Mode);
  key.SetValue(kWorkDirPath, fs2us(Path));
  key.SetValue(kTempRemovableOnly, ForRemovableOnly);
}

void CInfo::Load()
{
  SetDefault();

  CS_LOCK
  CKey key;
  if (OpenMainKey(key, kOptionsInfoKeyName) != ERROR_SUCCESS)
    return;

  UInt32 dirType;
  if (key.QueryValue(kWorkDirType, dirType) != ERROR_SUCCESS)
    return;
  switch (dirType)
  {
    case NMode::kSystem:
    case NMode::kCurrent:
    case NMode::kSpecified:
      Mode = (NMode::EEnum)dirType;
  }
  UString pathU;
  if (key.QueryValue(kWorkDirPath, pathU) == ERROR_SUCCESS)
    Path = us2fs(pathU);
  else
  {
    Path.Empty();
    if (Mode == NMode::kSpecified)
      Mode = NMode::kSystem;
  }
  key.GetValue_IfOk(kTempRemovableOnly, ForRemovableOnly);
}

}

static LPCTSTR const kCascadedMenu = TEXT("CascadedMenu");
static LPCTSTR const kContextMenu = TEXT("ContextMenu");
static LPCTSTR const kMenuIcons = TEXT("MenuIcons");
static LPCTSTR const kElimDup = TEXT("ElimDupExtract");

void CContextMenuInfo::Save() const
{
  CS_LOCK
  CKey key;
  CreateMainKey(key, kOptionsInfoKeyName);
  
  Key_Set_BoolPair(key, kCascadedMenu, Cascaded);
  Key_Set_BoolPair(key, kMenuIcons, MenuIcons);
  Key_Set_BoolPair(key, kElimDup, ElimDup);
  
  if (Flags_Def)
    key.SetValue(kContextMenu, Flags);
}

void CContextMenuInfo::Load()
{
  Cascaded.Val = true;
  Cascaded.Def = false;

  MenuIcons.Val = false;
  MenuIcons.Def = false;

  ElimDup.Val = true;
  ElimDup.Def = false;

  Flags = (UInt32)(Int32)-1;
  Flags_Def = false;
  
  CS_LOCK
  
  CKey key;
  if (OpenMainKey(key, kOptionsInfoKeyName) != ERROR_SUCCESS)
    return;
  
  Key_Get_BoolPair_true(key, kCascadedMenu, Cascaded);
  Key_Get_BoolPair_true(key, kElimDup, ElimDup);
  Key_Get_BoolPair(key, kMenuIcons, MenuIcons);

  Flags_Def = (key.GetValue_IfOk(kContextMenu, Flags) == ERROR_SUCCESS);
}

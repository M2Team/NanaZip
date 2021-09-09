// RegistryUtils.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"

#include "../../../Windows/Registry.h"

#include "RegistryUtils.h"

using namespace NWindows;
using namespace NRegistry;

#define REG_PATH_7Z TEXT("Software") TEXT(STRING_PATH_SEPARATOR) TEXT("7-Zip")

static LPCTSTR const kCUBasePath = REG_PATH_7Z;
static LPCTSTR const kCU_FMPath = REG_PATH_7Z TEXT(STRING_PATH_SEPARATOR) TEXT("FM");
// static LPCTSTR const kLM_Path = REG_PATH_7Z TEXT(STRING_PATH_SEPARATOR) TEXT("FM");

static LPCWSTR const kLangValueName = L"Lang";

static LPCWSTR const kViewer = L"Viewer";
static LPCWSTR const kEditor = L"Editor";
static LPCWSTR const kDiff = L"Diff";
static LPCWSTR const kVerCtrlPath = L"7vc";

static LPCTSTR const kShowDots = TEXT("ShowDots");
static LPCTSTR const kShowRealFileIcons = TEXT("ShowRealFileIcons");
static LPCTSTR const kFullRow = TEXT("FullRow");
static LPCTSTR const kShowGrid = TEXT("ShowGrid");
static LPCTSTR const kSingleClick = TEXT("SingleClick");
static LPCTSTR const kAlternativeSelection = TEXT("AlternativeSelection");
// static LPCTSTR const kUnderline = TEXT("Underline");

static LPCTSTR const kShowSystemMenu = TEXT("ShowSystemMenu");

// static LPCTSTR const kLockMemoryAdd = TEXT("LockMemoryAdd");
static LPCTSTR const kLargePages = TEXT("LargePages");

static LPCTSTR const kFlatViewName = TEXT("FlatViewArc");
// static LPCTSTR const kShowDeletedFiles = TEXT("ShowDeleted");

static void SaveCuString(LPCTSTR keyPath, LPCWSTR valuePath, LPCWSTR value)
{
  CKey key;
  key.Create(HKEY_CURRENT_USER, keyPath);
  key.SetValue(valuePath, value);
}

static void ReadCuString(LPCTSTR keyPath, LPCWSTR valuePath, UString &res)
{
  res.Empty();
  CKey key;
  if (key.Open(HKEY_CURRENT_USER, keyPath, KEY_READ) == ERROR_SUCCESS)
    key.QueryValue(valuePath, res);
}

void SaveRegLang(const UString &path) { SaveCuString(kCUBasePath, kLangValueName, path); }
void ReadRegLang(UString &path) { ReadCuString(kCUBasePath, kLangValueName, path); }

void SaveRegEditor(bool useEditor, const UString &path) { SaveCuString(kCU_FMPath, useEditor ? kEditor : kViewer, path); }
void ReadRegEditor(bool useEditor, UString &path) { ReadCuString(kCU_FMPath, useEditor ? kEditor : kViewer, path); }

void SaveRegDiff(const UString &path) { SaveCuString(kCU_FMPath, kDiff, path); }
void ReadRegDiff(UString &path) { ReadCuString(kCU_FMPath, kDiff, path); }

void ReadReg_VerCtrlPath(UString &path) { ReadCuString(kCU_FMPath, kVerCtrlPath, path); }

static void Save7ZipOption(LPCTSTR value, bool enabled)
{
  CKey key;
  key.Create(HKEY_CURRENT_USER, kCUBasePath);
  key.SetValue(value, enabled);
}

static void SaveOption(LPCTSTR value, bool enabled)
{
  CKey key;
  key.Create(HKEY_CURRENT_USER, kCU_FMPath);
  key.SetValue(value, enabled);
}

static bool Read7ZipOption(LPCTSTR value, bool defaultValue)
{
  CKey key;
  if (key.Open(HKEY_CURRENT_USER, kCUBasePath, KEY_READ) == ERROR_SUCCESS)
  {
    bool enabled;
    if (key.QueryValue(value, enabled) == ERROR_SUCCESS)
      return enabled;
  }
  return defaultValue;
}

static void ReadOption(CKey &key, LPCTSTR value, bool &dest)
{
  bool enabled = false;
  if (key.QueryValue(value, enabled) == ERROR_SUCCESS)
    dest = enabled;
}

/*
static void SaveLmOption(LPCTSTR value, bool enabled)
{
  CKey key;
  key.Create(HKEY_LOCAL_MACHINE, kLM_Path);
  key.SetValue(value, enabled);
}

static bool ReadLmOption(LPCTSTR value, bool defaultValue)
{
  CKey key;
  if (key.Open(HKEY_LOCAL_MACHINE, kLM_Path, KEY_READ) == ERROR_SUCCESS)
  {
    bool enabled;
    if (key.QueryValue(value, enabled) == ERROR_SUCCESS)
      return enabled;
  }
  return defaultValue;
}
*/

void CFmSettings::Save() const
{
  SaveOption(kShowDots, ShowDots);
  SaveOption(kShowRealFileIcons, ShowRealFileIcons);
  SaveOption(kFullRow, FullRow);
  SaveOption(kShowGrid, ShowGrid);
  SaveOption(kSingleClick, SingleClick);
  SaveOption(kAlternativeSelection, AlternativeSelection);
  // SaveOption(kUnderline, Underline);

  SaveOption(kShowSystemMenu, ShowSystemMenu);
}

void CFmSettings::Load()
{
  ShowDots = false;
  ShowRealFileIcons = false;
  FullRow = false;
  ShowGrid = false;
  SingleClick = false;
  AlternativeSelection = false;
  // Underline = false;

  ShowSystemMenu = false;

  CKey key;
  if (key.Open(HKEY_CURRENT_USER, kCU_FMPath, KEY_READ) == ERROR_SUCCESS)
  {
    ReadOption(key, kShowDots, ShowDots);
    ReadOption(key, kShowRealFileIcons, ShowRealFileIcons);
    ReadOption(key, kFullRow, FullRow);
    ReadOption(key, kShowGrid, ShowGrid);
    ReadOption(key, kSingleClick, SingleClick);
    ReadOption(key, kAlternativeSelection, AlternativeSelection);
    // ReadOption(key, kUnderline, Underline);

    ReadOption(key, kShowSystemMenu, ShowSystemMenu );
  }
}


// void SaveLockMemoryAdd(bool enable) { SaveLmOption(kLockMemoryAdd, enable); }
// bool ReadLockMemoryAdd() { return ReadLmOption(kLockMemoryAdd, true); }

void SaveLockMemoryEnable(bool enable) { Save7ZipOption(kLargePages, enable); }
bool ReadLockMemoryEnable() { return Read7ZipOption(kLargePages, false); }

static CSysString GetFlatViewName(UInt32 panelIndex)
{
  TCHAR panelString[16];
  ConvertUInt32ToString(panelIndex, panelString);
  return (CSysString)kFlatViewName + panelString;
}

void SaveFlatView(UInt32 panelIndex, bool enable) { SaveOption(GetFlatViewName(panelIndex), enable); }

bool ReadFlatView(UInt32 panelIndex)
{
  bool enabled = false;
  CKey key;
  if (key.Open(HKEY_CURRENT_USER, kCU_FMPath, KEY_READ) == ERROR_SUCCESS)
    ReadOption(key, GetFlatViewName(panelIndex), enabled);
  return enabled;
}

/*
void Save_ShowDeleted(bool enable) { SaveOption(kShowDeletedFiles, enable); }
bool Read_ShowDeleted() { return ReadOption(kShowDeletedFiles, false); }
*/

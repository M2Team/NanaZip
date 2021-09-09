// ViewSettings.cpp

#include "StdAfx.h"

#include "../../../../C/CpuArch.h"
 
#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/Registry.h"
#include "../../../Windows/Synchronization.h"

#include "ViewSettings.h"

using namespace NWindows;
using namespace NRegistry;

#define REG_PATH_FM TEXT("Software") TEXT(STRING_PATH_SEPARATOR) TEXT("7-Zip") TEXT(STRING_PATH_SEPARATOR) TEXT("FM")

static LPCTSTR const kCUBasePath = REG_PATH_FM;
static LPCTSTR const kCulumnsKeyName = REG_PATH_FM TEXT(STRING_PATH_SEPARATOR) TEXT("Columns");

static LPCTSTR const kPositionValueName = TEXT("Position");
static LPCTSTR const kPanelsInfoValueName = TEXT("Panels");
static LPCTSTR const kToolbars = TEXT("Toolbars");

static LPCWSTR const kPanelPathValueName = L"PanelPath";

static LPCTSTR const kListMode = TEXT("ListMode");
static LPCTSTR const kFolderHistoryValueName = TEXT("FolderHistory");
static LPCTSTR const kFastFoldersValueName = TEXT("FolderShortcuts");
static LPCTSTR const kCopyHistoryValueName = TEXT("CopyHistory");

static NSynchronization::CCriticalSection g_CS;

#define Set32(p, v) SetUi32(((Byte *)p), v)
#define SetBool(p, v) Set32(p, ((v) ? 1 : 0))

#define Get32(p, dest) dest = GetUi32((const Byte *)p)
#define GetBool(p, dest) dest = (GetUi32(p) != 0);

/*
struct CColumnHeader
{
  UInt32 Version;
  UInt32 SortID;
  UInt32 Ascending; // bool
};
*/

static const UInt32 kListViewHeaderSize = 3 * 4;
static const UInt32 kColumnInfoSize = 3 * 4;
static const UInt32 kListViewVersion = 1;

void CListViewInfo::Save(const UString &id) const
{
  const UInt32 dataSize = kListViewHeaderSize + kColumnInfoSize * Columns.Size();
  CByteArr buf(dataSize);

  Set32(buf, kListViewVersion);
  Set32(buf + 4, SortID);
  SetBool(buf + 8, Ascending);
  FOR_VECTOR (i, Columns)
  {
    const CColumnInfo &column = Columns[i];
    Byte *p = buf + kListViewHeaderSize + i * kColumnInfoSize;
    Set32(p, column.PropID);
    SetBool(p + 4, column.IsVisible);
    Set32(p + 8, column.Width);
  }
  {
    NSynchronization::CCriticalSectionLock lock(g_CS);
    CKey key;
    key.Create(HKEY_CURRENT_USER, kCulumnsKeyName);
    key.SetValue(GetSystemString(id), (const Byte *)buf, dataSize);
  }
}

void CListViewInfo::Read(const UString &id)
{
  Clear();
  CByteBuffer buf;
  UInt32 size;
  {
    NSynchronization::CCriticalSectionLock lock(g_CS);
    CKey key;
    if (key.Open(HKEY_CURRENT_USER, kCulumnsKeyName, KEY_READ) != ERROR_SUCCESS)
      return;
    if (key.QueryValue(GetSystemString(id), buf, size) != ERROR_SUCCESS)
      return;
  }
  if (size < kListViewHeaderSize)
    return;
  UInt32 version;
  Get32(buf, version);
  if (version != kListViewVersion)
    return;
  Get32(buf + 4, SortID);
  GetBool(buf + 8, Ascending);

  IsLoaded = true;

  size -= kListViewHeaderSize;
  if (size % kColumnInfoSize != 0)
    return;
  unsigned numItems = size / kColumnInfoSize;
  Columns.ClearAndReserve(numItems);
  for (unsigned i = 0; i < numItems; i++)
  {
    CColumnInfo column;
    const Byte *p = buf + kListViewHeaderSize + i * kColumnInfoSize;
    Get32(p, column.PropID);
    GetBool(p + 4, column.IsVisible);
    Get32(p + 8, column.Width);
    Columns.AddInReserved(column);
  }
}


/*
struct CWindowPosition
{
  RECT Rect;
  UInt32 Maximized; // bool
};

struct CPanelsInfo
{
  UInt32 NumPanels;
  UInt32 CurrentPanel;
  UInt32 SplitterPos;
};
*/

static const UInt32 kWindowPositionHeaderSize = 5 * 4;
static const UInt32 kPanelsInfoHeaderSize = 3 * 4;

void CWindowInfo::Save() const
{
  NSynchronization::CCriticalSectionLock lock(g_CS);
  CKey key;
  key.Create(HKEY_CURRENT_USER, kCUBasePath);
  {
    Byte buf[kWindowPositionHeaderSize];
    Set32(buf,      rect.left);
    Set32(buf +  4, rect.top);
    Set32(buf +  8, rect.right);
    Set32(buf + 12, rect.bottom);
    SetBool(buf + 16, maximized);
    key.SetValue(kPositionValueName, buf, kWindowPositionHeaderSize);
  }
  {
    Byte buf[kPanelsInfoHeaderSize];
    Set32(buf,      numPanels);
    Set32(buf +  4, currentPanel);
    Set32(buf +  8, splitterPos);
    key.SetValue(kPanelsInfoValueName, buf, kPanelsInfoHeaderSize);
  }
}

static bool QueryBuf(CKey &key, LPCTSTR name, CByteBuffer &buf, UInt32 dataSize)
{
  UInt32 size;
  return key.QueryValue(name, buf, size) == ERROR_SUCCESS && size == dataSize;
}

void CWindowInfo::Read(bool &windowPosDefined, bool &panelInfoDefined)
{
  windowPosDefined = false;
  panelInfoDefined = false;
  NSynchronization::CCriticalSectionLock lock(g_CS);
  CKey key;
  if (key.Open(HKEY_CURRENT_USER, kCUBasePath, KEY_READ) != ERROR_SUCCESS)
    return;
  CByteBuffer buf;
  if (QueryBuf(key, kPositionValueName, buf, kWindowPositionHeaderSize))
  {
    Get32(buf,      rect.left);
    Get32(buf +  4, rect.top);
    Get32(buf +  8, rect.right);
    Get32(buf + 12, rect.bottom);
    GetBool(buf + 16, maximized);
    windowPosDefined = true;
  }
  if (QueryBuf(key, kPanelsInfoValueName, buf, kPanelsInfoHeaderSize))
  {
    Get32(buf,      numPanels);
    Get32(buf +  4, currentPanel);
    Get32(buf +  8, splitterPos);
    panelInfoDefined = true;
  }
  return;
}


static void SaveUi32Val(const TCHAR *name, UInt32 value)
{
  CKey key;
  key.Create(HKEY_CURRENT_USER, kCUBasePath);
  key.SetValue(name, value);
}

static bool ReadUi32Val(const TCHAR *name, UInt32 &value)
{
  CKey key;
  if (key.Open(HKEY_CURRENT_USER, kCUBasePath, KEY_READ) != ERROR_SUCCESS)
    return false;
  return key.QueryValue(name, value) == ERROR_SUCCESS;
}

void SaveToolbarsMask(UInt32 toolbarMask)
{
  SaveUi32Val(kToolbars, toolbarMask);
}

static const UInt32 kDefaultToolbarMask = ((UInt32)1 << 31) | 8 | 4 | 1;

UInt32 ReadToolbarsMask()
{
  UInt32 mask;
  if (!ReadUi32Val(kToolbars, mask))
    return kDefaultToolbarMask;
  return mask;
}


void CListMode::Save() const
{
  UInt32 t = 0;
  for (int i = 0; i < 2; i++)
    t |= ((Panels[i]) & 0xFF) << (i * 8);
  SaveUi32Val(kListMode, t);
}

void CListMode::Read()
{
  Init();
  UInt32 t;
  if (!ReadUi32Val(kListMode, t))
    return;
  for (int i = 0; i < 2; i++)
  {
    Panels[i] = (t & 0xFF);
    t >>= 8;
  }
}

static UString GetPanelPathName(UInt32 panelIndex)
{
  UString s (kPanelPathValueName);
  s.Add_UInt32(panelIndex);
  return s;
}

void SavePanelPath(UInt32 panel, const UString &path)
{
  NSynchronization::CCriticalSectionLock lock(g_CS);
  CKey key;
  key.Create(HKEY_CURRENT_USER, kCUBasePath);
  key.SetValue(GetPanelPathName(panel), path);
}

bool ReadPanelPath(UInt32 panel, UString &path)
{
  NSynchronization::CCriticalSectionLock lock(g_CS);
  CKey key;
  if (key.Open(HKEY_CURRENT_USER, kCUBasePath, KEY_READ) != ERROR_SUCCESS)
    return false;
  return (key.QueryValue(GetPanelPathName(panel), path) == ERROR_SUCCESS);
}


static void SaveStringList(LPCTSTR valueName, const UStringVector &folders)
{
  NSynchronization::CCriticalSectionLock lock(g_CS);
  CKey key;
  key.Create(HKEY_CURRENT_USER, kCUBasePath);
  key.SetValue_Strings(valueName, folders);
}

static void ReadStringList(LPCTSTR valueName, UStringVector &folders)
{
  folders.Clear();
  NSynchronization::CCriticalSectionLock lock(g_CS);
  CKey key;
  if (key.Open(HKEY_CURRENT_USER, kCUBasePath, KEY_READ) == ERROR_SUCCESS)
    key.GetValue_Strings(valueName, folders);
}

void SaveFolderHistory(const UStringVector &folders)
  { SaveStringList(kFolderHistoryValueName, folders); }
void ReadFolderHistory(UStringVector &folders)
  { ReadStringList(kFolderHistoryValueName, folders); }

void SaveFastFolders(const UStringVector &folders)
  { SaveStringList(kFastFoldersValueName, folders); }
void ReadFastFolders(UStringVector &folders)
  { ReadStringList(kFastFoldersValueName, folders); }

void SaveCopyHistory(const UStringVector &folders)
  { SaveStringList(kCopyHistoryValueName, folders); }
void ReadCopyHistory(UStringVector &folders)
  { ReadStringList(kCopyHistoryValueName, folders); }

void AddUniqueStringToHeadOfList(UStringVector &list, const UString &s)
{
  for (unsigned i = 0; i < list.Size();)
    if (s.IsEqualTo_NoCase(list[i]))
      list.Delete(i);
    else
      i++;
  list.Insert(0, s);
}

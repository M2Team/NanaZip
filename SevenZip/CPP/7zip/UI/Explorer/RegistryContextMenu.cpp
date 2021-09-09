// RegistryContextMenu.cpp

#include "StdAfx.h"

#include "../../../Common/StringConvert.h"

#include "../../../Windows/Registry.h"

#include "RegistryContextMenu.h"

using namespace NWindows;
using namespace NRegistry;

#ifndef UNDER_CE

// does extension can work, if Approved is removed ?
// CLISID (and Approved ?) items are separated for 32-bit and 64-bit code.
// shellex items shared by 32-bit and 64-bit code?

#define k_Clsid_A "{23170F69-40C1-278A-1000-000100020000}"

static LPCTSTR const k_Clsid = TEXT(k_Clsid_A);
static LPCTSTR const k_ShellExtName = TEXT("7-Zip Shell Extension");

static LPCTSTR const k_Approved = TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved");
static LPCTSTR const k_Inproc = TEXT("InprocServer32");

static LPCSTR const k_KeyPostfix_ContextMenu = "\\shellex\\ContextMenuHandlers\\7-Zip";
static LPCSTR const k_KeyPostfix_DragDrop    = "\\shellex\\DragDropHandlers\\7-Zip";

static LPCSTR const k_KeyName_File      = "*";
static LPCSTR const k_KeyName_Folder    = "Folder";
static LPCSTR const k_KeyName_Directory = "Directory";
static LPCSTR const k_KeyName_Drive     = "Drive";

static LPCSTR const k_shellex_Prefixes[] =
{
  k_KeyName_File,
  k_KeyName_Folder,
  k_KeyName_Directory,
  k_KeyName_Drive
};

static const bool k_shellex_Statuses[2][4] =
{
  { true, true, true, false },
  { false, false, true, true }
};


// can we use static RegDeleteKeyExW in _WIN64 mode?
// is it supported by Windows 2003 x64?

/*
#ifdef _WIN64

#define INIT_REG_WOW

#else
*/

typedef
// WINADVAPI
LONG (APIENTRY *Func_RegDeleteKeyExW)(HKEY hKey, LPCWSTR lpSubKey, REGSAM samDesired, DWORD Reserved);
static Func_RegDeleteKeyExW func_RegDeleteKeyExW;

static void Init_RegDeleteKeyExW()
{
  if (!func_RegDeleteKeyExW)
    func_RegDeleteKeyExW = (Func_RegDeleteKeyExW)
      (void *)GetProcAddress(GetModuleHandleW(L"advapi32.dll"), "RegDeleteKeyExW");
}

#define INIT_REG_WOW if (wow != 0) Init_RegDeleteKeyExW();

// #endif

static LONG MyRegistry_DeleteKey(HKEY parentKey, LPCTSTR name, UInt32 wow)
{
  if (wow == 0)
    return RegDeleteKey(parentKey, name);
  
  /*
  #ifdef _WIN64
  return RegDeleteKeyExW
  #else
  */
  if (!func_RegDeleteKeyExW)
    return E_NOTIMPL;
  return func_RegDeleteKeyExW
  // #endif
      (parentKey, GetUnicodeString(name), wow, 0);
}

static LONG MyRegistry_DeleteKey_HKCR(LPCTSTR name, UInt32 wow)
{
  return MyRegistry_DeleteKey(HKEY_CLASSES_ROOT, name, wow);
}

// static NSynchronization::CCriticalSection g_CS;

static AString Get_ContextMenuHandler_KeyName(LPCSTR keyName)
  { return (AString)keyName + k_KeyPostfix_ContextMenu; }

/*
static CSysString Get_DragDropHandler_KeyName(LPCTSTR keyName)
  { return (AString)keyName + k_KeyPostfix_DragDrop); }
*/

static bool CheckHandlerCommon(const AString &keyName, UInt32 wow)
{
  CKey key;
  if (key.Open(HKEY_CLASSES_ROOT, (CSysString)keyName, KEY_READ | wow) != ERROR_SUCCESS)
    return false;
  CSysString value;
  if (key.QueryValue(NULL, value) != ERROR_SUCCESS)
    return false;
  return StringsAreEqualNoCase_Ascii(value, k_Clsid_A);
}

bool CheckContextMenuHandler(const UString &path, UInt32 wow)
{
  // NSynchronization::CCriticalSectionLock lock(g_CS);

  CSysString s ("CLSID\\");
  s += k_Clsid_A;
  s += "\\InprocServer32";
  
  {
    NRegistry::CKey key;
    if (key.Open(HKEY_CLASSES_ROOT, s, KEY_READ | wow) != ERROR_SUCCESS)
      return false;
    UString regPath;
    if (key.QueryValue(NULL, regPath) != ERROR_SUCCESS)
      return false;
    if (!path.IsEqualTo_NoCase(regPath))
      return false;
  }
  
  return
       CheckHandlerCommon(Get_ContextMenuHandler_KeyName(k_KeyName_File), wow);
  /*
    && CheckHandlerCommon(Get_ContextMenuHandler_KeyName(k_KeyName_Directory), wow)
    // && CheckHandlerCommon(Get_ContextMenuHandler_KeyName(k_KeyName_Folder))

    && CheckHandlerCommon(Get_DragDropHandler_KeyName(k_KeyName_Directory), wow)
    && CheckHandlerCommon(Get_DragDropHandler_KeyName(k_KeyName_Drive), wow);
  */
}


static LONG MyCreateKey(CKey &key, HKEY parentKey, LPCTSTR keyName, UInt32 wow)
{
  return key.Create(parentKey, keyName, REG_NONE,
      REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS | wow);
}

LONG SetContextMenuHandler(bool setMode, const UString &path, UInt32 wow)
{
  // NSynchronization::CCriticalSectionLock lock(g_CS);

  INIT_REG_WOW

  LONG res;

  {
  CSysString s ("CLSID\\");
  s += k_Clsid_A;

  if (setMode)
  {
    {
      CKey key;
      res = MyCreateKey(key, HKEY_CLASSES_ROOT, s, wow);
      if (res == ERROR_SUCCESS)
      {
        key.SetValue(NULL, k_ShellExtName);
        CKey keyInproc;
        res = MyCreateKey(keyInproc, key, k_Inproc, wow);
        if (res == ERROR_SUCCESS)
        {
          res = keyInproc.SetValue(NULL, path);
          keyInproc.SetValue(TEXT("ThreadingModel"), TEXT("Apartment"));
        }
      }
    }
    
    {
      CKey key;
      if (MyCreateKey(key, HKEY_LOCAL_MACHINE, k_Approved, wow) == ERROR_SUCCESS)
        key.SetValue(k_Clsid, k_ShellExtName);
    }
  }
  else
  {
    CSysString s2 (s);
    s2 += "\\InprocServer32";

    MyRegistry_DeleteKey_HKCR(s2, wow);
    res = MyRegistry_DeleteKey_HKCR(s, wow);
  }
  }

  // shellex items probably are shared beween 32-bit and 64-bit apps. So we don't delete items for delete operation.
  if (setMode)
  for (unsigned i = 0; i < 2; i++)
  {
    for (unsigned k = 0; k < ARRAY_SIZE(k_shellex_Prefixes); k++)
    {
      CSysString s (k_shellex_Prefixes[k]);
      s += (i == 0 ? k_KeyPostfix_ContextMenu : k_KeyPostfix_DragDrop);
      if (k_shellex_Statuses[i][k])
      {
        CKey key;
        MyCreateKey(key, HKEY_CLASSES_ROOT, s, wow);
        key.SetValue(NULL, k_Clsid);
      }
      else
        MyRegistry_DeleteKey_HKCR(s, wow);
    }
  }

  return res;
}

#endif

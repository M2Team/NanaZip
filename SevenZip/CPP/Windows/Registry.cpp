// Windows/Registry.cpp

#include "StdAfx.h"

#include <wchar.h>
// #include <stdio.h>

#ifndef _UNICODE
#include "../Common/StringConvert.h"
#endif
#include "Registry.h"

#ifndef _UNICODE
extern bool g_IsNT;
#endif

namespace NWindows {
namespace NRegistry {

#define MYASSERT(expr) // _ASSERTE(expr)
#define MY_ASSUME(expr)

/*
static void Error()
{
  #ifdef _CONSOLE
  printf("\nregistry error\n");
  #else
  MessageBoxW(0, L"registry error", L"", 0);
  // exit(1);
  #endif
}

#define MY_ASSUME(expr) { if (!(expr)) Error(); }
*/

LONG CKey::Create(HKEY parentKey, LPCTSTR keyName,
    LPTSTR keyClass, DWORD options, REGSAM accessMask,
    LPSECURITY_ATTRIBUTES securityAttributes, LPDWORD disposition) throw()
{
  MY_ASSUME(parentKey != NULL);
  DWORD dispositionReal;
  HKEY key = NULL;
  LONG res = RegCreateKeyEx(parentKey, keyName, 0, keyClass,
      options, accessMask, securityAttributes, &key, &dispositionReal);
  if (disposition != NULL)
    *disposition = dispositionReal;
  if (res == ERROR_SUCCESS)
  {
    res = Close();
    _object = key;
  }
  return res;
}

LONG CKey::Open(HKEY parentKey, LPCTSTR keyName, REGSAM accessMask) throw()
{
  MY_ASSUME(parentKey != NULL);
  HKEY key = NULL;
  LONG res = RegOpenKeyEx(parentKey, keyName, 0, accessMask, &key);
  if (res == ERROR_SUCCESS)
  {
    res = Close();
    MYASSERT(res == ERROR_SUCCESS);
    _object = key;
  }
  return res;
}

LONG CKey::Close() throw()
{
  LONG res = ERROR_SUCCESS;
  if (_object != NULL)
  {
    res = RegCloseKey(_object);
    _object = NULL;
  }
  return res;
}

// win95, win98: deletes sunkey and all its subkeys
// winNT to be deleted must not have subkeys
LONG CKey::DeleteSubKey(LPCTSTR subKeyName) throw()
{
  MY_ASSUME(_object != NULL);
  return RegDeleteKey(_object, subKeyName);
}

LONG CKey::RecurseDeleteKey(LPCTSTR subKeyName) throw()
{
  CKey key;
  LONG res = key.Open(_object, subKeyName, KEY_READ | KEY_WRITE);
  if (res != ERROR_SUCCESS)
    return res;
  FILETIME fileTime;
  const UInt32 kBufSize = MAX_PATH + 1; // 256 in ATL
  DWORD size = kBufSize;
  TCHAR buffer[kBufSize];
  while (RegEnumKeyEx(key._object, 0, buffer, &size, NULL, NULL, NULL, &fileTime) == ERROR_SUCCESS)
  {
    res = key.RecurseDeleteKey(buffer);
    if (res != ERROR_SUCCESS)
      return res;
    size = kBufSize;
  }
  key.Close();
  return DeleteSubKey(subKeyName);
}


/////////////////////////
// Value Functions

static inline UInt32 BoolToUINT32(bool value) {  return (value ? 1: 0); }
static inline bool UINT32ToBool(UInt32 value) {  return (value != 0); }


LONG CKey::DeleteValue(LPCTSTR name) throw()
{
  MY_ASSUME(_object != NULL);
  return ::RegDeleteValue(_object, name);
}

#ifndef _UNICODE
LONG CKey::DeleteValue(LPCWSTR name)
{
  MY_ASSUME(_object != NULL);
  if (g_IsNT)
    return ::RegDeleteValueW(_object, name);
  return DeleteValue(name == 0 ? 0 : (LPCSTR)GetSystemString(name));
}
#endif

LONG CKey::SetValue(LPCTSTR name, UInt32 value) throw()
{
  MY_ASSUME(_object != NULL);
  return RegSetValueEx(_object, name, 0, REG_DWORD,
      (const BYTE *)&value, sizeof(UInt32));
}

LONG CKey::SetValue(LPCTSTR name, bool value) throw()
{
  return SetValue(name, BoolToUINT32(value));
}

LONG CKey::SetValue(LPCTSTR name, LPCTSTR value) throw()
{
  MYASSERT(value != NULL);
  MY_ASSUME(_object != NULL);
  return RegSetValueEx(_object, name, 0, REG_SZ,
      (const BYTE *)value, ((DWORD)lstrlen(value) + 1) * sizeof(TCHAR));
}

/*
LONG CKey::SetValue(LPCTSTR name, const CSysString &value)
{
  MYASSERT(value != NULL);
  MY_ASSUME(_object != NULL);
  return RegSetValueEx(_object, name, NULL, REG_SZ,
      (const BYTE *)(const TCHAR *)value, (value.Len() + 1) * sizeof(TCHAR));
}
*/

#ifndef _UNICODE

LONG CKey::SetValue(LPCWSTR name, LPCWSTR value)
{
  MYASSERT(value != NULL);
  MY_ASSUME(_object != NULL);
  if (g_IsNT)
    return RegSetValueExW(_object, name, 0, REG_SZ,
      (const BYTE * )value, (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
  return SetValue(name == 0 ? 0 : (LPCSTR)GetSystemString(name),
    value == 0 ? 0 : (LPCSTR)GetSystemString(value));
}

#endif


LONG CKey::SetValue(LPCTSTR name, const void *value, UInt32 size) throw()
{
  MYASSERT(value != NULL);
  MY_ASSUME(_object != NULL);
  return RegSetValueEx(_object, name, 0, REG_BINARY,
      (const BYTE *)value, size);
}

LONG SetValue(HKEY parentKey, LPCTSTR keyName, LPCTSTR valueName, LPCTSTR value)
{
  MYASSERT(value != NULL);
  CKey key;
  LONG res = key.Create(parentKey, keyName);
  if (res == ERROR_SUCCESS)
    res = key.SetValue(valueName, value);
  return res;
}

LONG CKey::SetKeyValue(LPCTSTR keyName, LPCTSTR valueName, LPCTSTR value) throw()
{
  MYASSERT(value != NULL);
  CKey key;
  LONG res = key.Create(_object, keyName);
  if (res == ERROR_SUCCESS)
    res = key.SetValue(valueName, value);
  return res;
}

LONG CKey::QueryValue(LPCTSTR name, UInt32 &value) throw()
{
  DWORD type = 0;
  DWORD count = sizeof(DWORD);
  LONG res = RegQueryValueEx(_object, name, NULL, &type,
    (LPBYTE)&value, &count);
  MYASSERT((res != ERROR_SUCCESS) || (type == REG_DWORD));
  MYASSERT((res != ERROR_SUCCESS) || (count == sizeof(UInt32)));
  return res;
}

LONG CKey::QueryValue(LPCTSTR name, bool &value) throw()
{
  UInt32 uintValue = BoolToUINT32(value);
  LONG res = QueryValue(name, uintValue);
  value = UINT32ToBool(uintValue);
  return res;
}

LONG CKey::GetValue_IfOk(LPCTSTR name, UInt32 &value) throw()
{
  UInt32 newVal;
  LONG res = QueryValue(name, newVal);
  if (res == ERROR_SUCCESS)
    value = newVal;
  return res;
}

LONG CKey::GetValue_IfOk(LPCTSTR name, bool &value) throw()
{
  bool newVal = false;
  LONG res = QueryValue(name, newVal);
  if (res == ERROR_SUCCESS)
    value = newVal;
  return res;
}

LONG CKey::QueryValue(LPCTSTR name, LPTSTR value, UInt32 &count) throw()
{
  DWORD type = 0;
  LONG res = RegQueryValueEx(_object, name, NULL, &type, (LPBYTE)value, (DWORD *)&count);
  MYASSERT((res != ERROR_SUCCESS) || (type == REG_SZ) || (type == REG_MULTI_SZ) || (type == REG_EXPAND_SZ));
  return res;
}

LONG CKey::QueryValue(LPCTSTR name, CSysString &value)
{
  value.Empty();
  DWORD type = 0;
  UInt32 curSize = 0;
  LONG res = RegQueryValueEx(_object, name, NULL, &type, NULL, (DWORD *)&curSize);
  if (res != ERROR_SUCCESS && res != ERROR_MORE_DATA)
    return res;
  UInt32 curSize2 = curSize;
  res = QueryValue(name, value.GetBuf(curSize), curSize2);
  if (curSize > curSize2)
    curSize = curSize2;
  value.ReleaseBuf_CalcLen(curSize / sizeof(TCHAR));
  return res;
}


#ifndef _UNICODE

LONG CKey::QueryValue(LPCWSTR name, LPWSTR value, UInt32 &count)
{
  DWORD type = 0;
  LONG res = RegQueryValueExW(_object, name, NULL, &type, (LPBYTE)value, (DWORD *)&count);
  MYASSERT((res != ERROR_SUCCESS) || (type == REG_SZ) || (type == REG_MULTI_SZ) || (type == REG_EXPAND_SZ));
  return res;
}

LONG CKey::QueryValue(LPCWSTR name, UString &value)
{
  value.Empty();
  DWORD type = 0;
  UInt32 curSize = 0;

  LONG res;

  if (g_IsNT)
  {
    res = RegQueryValueExW(_object, name, NULL, &type, NULL, (DWORD *)&curSize);
    if (res != ERROR_SUCCESS && res != ERROR_MORE_DATA)
      return res;
    UInt32 curSize2 = curSize;
    res = QueryValue(name, value.GetBuf(curSize), curSize2);
    if (curSize > curSize2)
      curSize = curSize2;
    value.ReleaseBuf_CalcLen(curSize / sizeof(wchar_t));
  }
  else
  {
    AString vTemp;
    res = QueryValue(name == 0 ? 0 : (LPCSTR)GetSystemString(name), vTemp);
    value = GetUnicodeString(vTemp);
  }
  
  return res;
}

#endif


LONG CKey::QueryValue(LPCTSTR name, void *value, UInt32 &count) throw()
{
  DWORD type = 0;
  LONG res = RegQueryValueEx(_object, name, NULL, &type, (LPBYTE)value, (DWORD *)&count);
  MYASSERT((res != ERROR_SUCCESS) || (type == REG_BINARY));
  return res;
}


LONG CKey::QueryValue(LPCTSTR name, CByteBuffer &value, UInt32 &dataSize)
{
  DWORD type = 0;
  dataSize = 0;
  LONG res = RegQueryValueEx(_object, name, NULL, &type, NULL, (DWORD *)&dataSize);
  if (res != ERROR_SUCCESS && res != ERROR_MORE_DATA)
    return res;
  value.Alloc(dataSize);
  return QueryValue(name, (BYTE *)value, dataSize);
}

LONG CKey::EnumKeys(CSysStringVector &keyNames)
{
  keyNames.Clear();
  CSysString keyName;
  for (DWORD index = 0; ; index++)
  {
    const unsigned kBufSize = MAX_PATH + 1; // 256 in ATL
    FILETIME lastWriteTime;
    UInt32 nameSize = kBufSize;
    LONG result = ::RegEnumKeyEx(_object, index, keyName.GetBuf(kBufSize),
        (DWORD *)&nameSize, NULL, NULL, NULL, &lastWriteTime);
    keyName.ReleaseBuf_CalcLen(kBufSize);
    if (result == ERROR_NO_MORE_ITEMS)
      break;
    if (result != ERROR_SUCCESS)
      return result;
    keyNames.Add(keyName);
  }
  return ERROR_SUCCESS;
}

LONG CKey::SetValue_Strings(LPCTSTR valueName, const UStringVector &strings)
{
  size_t numChars = 0;
  
  unsigned i;
  
  for (i = 0; i < strings.Size(); i++)
    numChars += strings[i].Len() + 1;
  
  CObjArray<wchar_t> buffer(numChars);
  size_t pos = 0;
  
  for (i = 0; i < strings.Size(); i++)
  {
    const UString &s = strings[i];
    size_t size = s.Len() + 1;
    wmemcpy(buffer + pos, s, size);
    pos += size;
  }
  return SetValue(valueName, buffer, (UInt32)numChars * sizeof(wchar_t));
}

LONG CKey::GetValue_Strings(LPCTSTR valueName, UStringVector &strings)
{
  strings.Clear();
  CByteBuffer buffer;
  UInt32 dataSize = 0;
  LONG res = QueryValue(valueName, buffer, dataSize);
  if (res != ERROR_SUCCESS)
    return res;
  if (dataSize > buffer.Size())
    return E_FAIL;
  if (dataSize % sizeof(wchar_t) != 0)
    return E_FAIL;

  const wchar_t *data = (const wchar_t *)(const void *)(const Byte  *)buffer;
  size_t numChars = dataSize / sizeof(wchar_t);
  size_t prev = 0;
  UString s;
  
  for (size_t i = 0; i < numChars; i++)
  {
    if (data[i] == 0)
    {
      s = data + prev;
      strings.Add(s);
      prev = i + 1;
    }
  }
  
  return res;
}

}}

// DLLExports.cpp
//
// Notes:
// Win2000:
// If I register at HKCR\Folder\ShellEx then DLL is locked.
// otherwise it unloads after explorer closing.
// but if I call menu for desktop items it's locked all the time

#include "StdAfx.h"

#include "../../../Common/MyWindows.h"

#include <OleCtl.h>

#include "../../../Common/MyInitGuid.h"

#include "../../../Common/ComTry.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/NtCheck.h"
#include "../../../Windows/Registry.h"

#include "../FileManager/IFolder.h"

#include "ContextMenu.h"

static LPCTSTR const k_ShellExtName = TEXT("7-Zip Shell Extension");
static LPCTSTR const k_Approved = TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved");

// {23170F69-40C1-278A-1000-000100020000}
static LPCTSTR const k_Clsid = TEXT("{23170F69-40C1-278A-1000-000100020000}");

DEFINE_GUID(CLSID_CZipContextMenu,
    k_7zip_GUID_Data1,
    k_7zip_GUID_Data2,
    k_7zip_GUID_Data3_Common,
    0x10, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00);

using namespace NWindows;

extern
HINSTANCE g_hInstance;
HINSTANCE g_hInstance = 0;

extern
HWND g_HWND;
HWND g_HWND = 0;

extern
LONG g_DllRefCount;
LONG g_DllRefCount = 0; // Reference count of this DLL.


// #define ODS(sz) OutputDebugString(L#sz)

class CShellExtClassFactory:
  public IClassFactory,
  public CMyUnknownImp
{
public:
  CShellExtClassFactory() { InterlockedIncrement(&g_DllRefCount); }
  ~CShellExtClassFactory() { InterlockedDecrement(&g_DllRefCount); }

  MY_UNKNOWN_IMP1_MT(IClassFactory)
  
  STDMETHODIMP CreateInstance(LPUNKNOWN, REFIID, void**);
  STDMETHODIMP LockServer(BOOL);
};

STDMETHODIMP CShellExtClassFactory::CreateInstance(LPUNKNOWN pUnkOuter,
    REFIID riid, void **ppvObj)
{
  // ODS("CShellExtClassFactory::CreateInstance()\r\n");
  *ppvObj = NULL;
  if (pUnkOuter)
    return CLASS_E_NOAGGREGATION;
  
  CZipContextMenu *shellExt;
  try
  {
    shellExt = new CZipContextMenu();
  }
  catch(...) { return E_OUTOFMEMORY; }
  if (!shellExt)
    return E_OUTOFMEMORY;
  
  HRESULT res = shellExt->QueryInterface(riid, ppvObj);
  if (res != S_OK)
    delete shellExt;
  return res;
}


STDMETHODIMP CShellExtClassFactory::LockServer(BOOL /* fLock */)
{
  return S_OK; // Check it
}


#if defined(_UNICODE) && !defined(_WIN64) && !defined(UNDER_CE)
#define NT_CHECK_FAIL_ACTION return FALSE;
#endif

extern "C"
BOOL WINAPI DllMain(
  #ifdef UNDER_CE
  HANDLE hInstance
  #else
  HINSTANCE hInstance
  #endif
  , DWORD dwReason, LPVOID);

extern "C"
BOOL WINAPI DllMain(
  #ifdef UNDER_CE
  HANDLE hInstance
  #else
  HINSTANCE hInstance
  #endif
  , DWORD dwReason, LPVOID)
{
  if (dwReason == DLL_PROCESS_ATTACH)
  {
    g_hInstance = (HINSTANCE)hInstance;
    // ODS("In DLLMain, DLL_PROCESS_ATTACH\r\n");
    NT_CHECK
  }
  else if (dwReason == DLL_PROCESS_DETACH)
  {
    // ODS("In DLLMain, DLL_PROCESS_DETACH\r\n");
  }
  return TRUE;
}


// Used to determine whether the DLL can be unloaded by OLE

STDAPI DllCanUnloadNow(void)
{
  // ODS("In DLLCanUnloadNow\r\n");
  return (g_DllRefCount == 0 ? S_OK : S_FALSE);
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
  // ODS("In DllGetClassObject\r\n");
  *ppv = NULL;
  if (IsEqualIID(rclsid, CLSID_CZipContextMenu))
  {
    CShellExtClassFactory *cf;
    try
    {
      cf = new CShellExtClassFactory;
    }
    catch(...) { return E_OUTOFMEMORY; }
    if (!cf)
      return E_OUTOFMEMORY;
    HRESULT res = cf->QueryInterface(riid, ppv);
    if (res != S_OK)
      delete cf;
    return res;
  }
  return CLASS_E_CLASSNOTAVAILABLE;
  // return _Module.GetClassObject(rclsid, riid, ppv);
}


static BOOL RegisterServer()
{
  FString modulePath;
  if (!NDLL::MyGetModuleFileName(modulePath))
    return FALSE;
  const UString modulePathU = fs2us(modulePath);
  
  CSysString s ("CLSID\\");
  s += k_Clsid;
  
  {
    NRegistry::CKey key;
    if (key.Create(HKEY_CLASSES_ROOT, s, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE) != NOERROR)
      return FALSE;
    key.SetValue(NULL, k_ShellExtName);
    NRegistry::CKey keyInproc;
    if (keyInproc.Create(key, TEXT("InprocServer32"), NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE) != NOERROR)
      return FALSE;
    keyInproc.SetValue(NULL, modulePathU);
    keyInproc.SetValue(TEXT("ThreadingModel"), TEXT("Apartment"));
  }
 
  #if !defined(_WIN64) && !defined(UNDER_CE)
  if (IsItWindowsNT())
  #endif
  {
    NRegistry::CKey key;
    if (key.Create(HKEY_LOCAL_MACHINE, k_Approved, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE) == NOERROR)
      key.SetValue(k_Clsid, k_ShellExtName);
  }
  
  return TRUE;
}

STDAPI DllRegisterServer(void)
{
  return RegisterServer() ?  S_OK: SELFREG_E_CLASS;
}

static BOOL UnregisterServer()
{
  CSysString s ("CLSID\\");
  s += k_Clsid;

  RegDeleteKey(HKEY_CLASSES_ROOT, s + TEXT("\\InprocServer32"));
  RegDeleteKey(HKEY_CLASSES_ROOT, s);

  #if !defined(_WIN64) && !defined(UNDER_CE)
  if (IsItWindowsNT())
  #endif
  {
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, k_Approved, 0, KEY_SET_VALUE, &hKey) == NOERROR)
    {
      RegDeleteValue(hKey, k_Clsid);
      RegCloseKey(hKey);
    }
  }

  return TRUE;
}

STDAPI DllUnregisterServer(void)
{
  return UnregisterServer() ? S_OK: SELFREG_E_CLASS;
}

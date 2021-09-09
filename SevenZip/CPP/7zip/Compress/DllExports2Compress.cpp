// DllExports2Compress.cpp

#include "StdAfx.h"

#include "../../Common/MyInitGuid.h"

#include "../ICoder.h"

#include "../Common/RegisterCodec.h"

extern "C"
BOOL WINAPI DllMain(
  #ifdef UNDER_CE
  HANDLE
  #else
  HINSTANCE
  #endif
  /* hInstance */, DWORD /* dwReason */, LPVOID /*lpReserved*/)
{
  return TRUE;
}

STDAPI CreateCoder(const GUID *clsid, const GUID *iid, void **outObject);

STDAPI CreateObject(const GUID *clsid, const GUID *iid, void **outObject)
{
  return CreateCoder(clsid, iid, outObject);
}

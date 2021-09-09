// DLLExports.cpp

#include "StdAfx.h"

#if defined(_7ZIP_LARGE_PAGES)
#include "../../../C/Alloc.h"
#endif

#include "../../Common/MyInitGuid.h"

#include "../../Common/ComTry.h"

#include "../../Windows/NtCheck.h"
#include "../../Windows/PropVariant.h"

#include "../ICoder.h"
#include "../IPassword.h"

#include "../Common/CreateCoder.h"

#include "IArchive.h"

HINSTANCE g_hInstance;

#define NT_CHECK_FAIL_ACTION return FALSE;

extern "C"
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/)
{
  if (dwReason == DLL_PROCESS_ATTACH)
  {
    g_hInstance = hInstance;
    NT_CHECK;
  }
  return TRUE;
}

DEFINE_GUID(CLSID_CArchiveHandler,
    k_7zip_GUID_Data1,
    k_7zip_GUID_Data2,
    k_7zip_GUID_Data3_Common,
    0x10, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00, 0x00);

STDAPI CreateArchiver(const GUID *classID, const GUID *iid, void **outObject);

STDAPI CreateObject(const GUID *clsid, const GUID *iid, void **outObject)
{
  return CreateArchiver(clsid, iid, outObject);
}

STDAPI SetLargePageMode()
{
  #if defined(_7ZIP_LARGE_PAGES)
  SetLargePageSize();
  #endif
  return S_OK;
}

extern bool g_CaseSensitive;

STDAPI SetCaseSensitive(Int32 caseSensitive)
{
  g_CaseSensitive = (caseSensitive != 0);
  return S_OK;
}

#ifdef EXTERNAL_CODECS

CExternalCodecs g_ExternalCodecs;

STDAPI SetCodecs(ICompressCodecsInfo *compressCodecsInfo)
{
  COM_TRY_BEGIN

  // OutputDebugStringA(compressCodecsInfo ? "SetCodecs" : "SetCodecs NULL");
  if (compressCodecsInfo)
  {
    g_ExternalCodecs.GetCodecs = compressCodecsInfo;
    return g_ExternalCodecs.Load();
  }
  g_ExternalCodecs.ClearAndRelease();
  return S_OK;

  COM_TRY_END
}

#else

STDAPI SetCodecs(ICompressCodecsInfo *)
{
  return S_OK;
}

#endif

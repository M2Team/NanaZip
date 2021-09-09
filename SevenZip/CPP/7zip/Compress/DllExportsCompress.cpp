// DllExportsCompress.cpp

#include "StdAfx.h"

#include "../../Common/MyInitGuid.h"

#include "../ICoder.h"

#include "../Common/RegisterCodec.h"

static const unsigned kNumCodecsMax = 48;
unsigned g_NumCodecs = 0;
const CCodecInfo *g_Codecs[kNumCodecsMax];
void RegisterCodec(const CCodecInfo *codecInfo) throw()
{
  if (g_NumCodecs < kNumCodecsMax)
    g_Codecs[g_NumCodecs++] = codecInfo;
}

static const unsigned kNumHashersMax = 16;
unsigned g_NumHashers = 0;
const CHasherInfo *g_Hashers[kNumHashersMax];
void RegisterHasher(const CHasherInfo *hashInfo) throw()
{
  if (g_NumHashers < kNumHashersMax)
    g_Hashers[g_NumHashers++] = hashInfo;
}

#ifdef _WIN32

extern "C"
BOOL WINAPI DllMain(
  #ifdef UNDER_CE
  HANDLE
  #else
  HINSTANCE
  #endif
  , DWORD /* dwReason */, LPVOID /*lpReserved*/);

extern "C"
BOOL WINAPI DllMain(
  #ifdef UNDER_CE
  HANDLE
  #else
  HINSTANCE
  #endif
  , DWORD /* dwReason */, LPVOID /*lpReserved*/)
{
  return TRUE;
}
#endif

STDAPI CreateCoder(const GUID *clsid, const GUID *iid, void **outObject);

STDAPI CreateObject(const GUID *clsid, const GUID *iid, void **outObject);
STDAPI CreateObject(const GUID *clsid, const GUID *iid, void **outObject)
{
  return CreateCoder(clsid, iid, outObject);
}

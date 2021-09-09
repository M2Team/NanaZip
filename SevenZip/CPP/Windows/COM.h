// Windows/COM.h

#ifndef __WINDOWS_COM_H
#define __WINDOWS_COM_H

#include "../Common/MyString.h"

namespace NWindows {
namespace NCOM {

#ifdef _WIN32
  
class CComInitializer
{
public:
  CComInitializer()
  {
    #ifdef UNDER_CE
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    #else
    // it's single thread. Do we need multithread?
    CoInitialize(NULL);
    #endif
  };
  ~CComInitializer() { CoUninitialize(); }
};

class CStgMedium
{
  STGMEDIUM _object;
public:
  bool _mustBeReleased;
  CStgMedium(): _mustBeReleased(false) {}
  ~CStgMedium() { Free(); }
  void Free()
  {
    if (_mustBeReleased)
      ReleaseStgMedium(&_object);
    _mustBeReleased = false;
  }
  const STGMEDIUM* operator->() const { return &_object;}
  STGMEDIUM* operator->() { return &_object;}
  STGMEDIUM* operator&() { return &_object; }
};

#endif

/*
//////////////////////////////////
// GUID <--> String Conversions
UString GUIDToStringW(REFGUID guid);
AString GUIDToStringA(REFGUID guid);
#ifdef UNICODE
  #define GUIDToString GUIDToStringW
#else
  #define GUIDToString GUIDToStringA
#endif

HRESULT StringToGUIDW(const wchar_t *string, GUID &classID);
HRESULT StringToGUIDA(const char *string, GUID &classID);
#ifdef UNICODE
  #define StringToGUID StringToGUIDW
#else
  #define StringToGUID StringToGUIDA
#endif
*/

}}

#endif

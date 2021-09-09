// Windows/DLL.h

#ifndef __WINDOWS_DLL_H
#define __WINDOWS_DLL_H

#include "../Common/MyString.h"

namespace NWindows {
namespace NDLL {

#ifdef _WIN32

#ifdef UNDER_CE
#define My_GetProcAddress(module, procName) (void *)::GetProcAddressA(module, procName)
#else
#define My_GetProcAddress(module, procName) (void *)::GetProcAddress(module, procName)
#endif

/* Win32: Don't call CLibrary::Free() and FreeLibrary() from another
    FreeLibrary() code: detaching code in DLL entry-point or in
    destructors of global objects in DLL module. */

class CLibrary
{
  HMODULE _module;

  // CLASS_NO_COPY(CLibrary);
public:
  CLibrary(): _module(NULL) {};
  ~CLibrary() { Free(); }

  operator HMODULE() const { return _module; }
  HMODULE* operator&() { return &_module; }
  bool IsLoaded() const { return (_module != NULL); }

  void Attach(HMODULE m)
  {
    Free();
    _module = m;
  }
  HMODULE Detach()
  {
    HMODULE m = _module;
    _module = NULL;
    return m;
  }

  bool Free() throw();
  bool LoadEx(CFSTR path, DWORD flags = LOAD_LIBRARY_AS_DATAFILE) throw();
  bool Load(CFSTR path) throw();
  // FARPROC
  void *GetProc(LPCSTR procName) const { return My_GetProcAddress(_module, procName); }
};

#else

typedef void * HMODULE;
// typedef int (*FARPROC)();
// typedef void *FARPROC;

class CLibrary
{
  HMODULE _module;

  // CLASS_NO_COPY(CLibrary);
public:
  CLibrary(): _module(NULL) {};
  ~CLibrary() { Free(); }

  bool Free() throw();
  bool Load(CFSTR path) throw();
  // FARPROC
  void *GetProc(LPCSTR procName) const; // { return My_GetProcAddress(_module, procName); }
};

#endif

bool MyGetModuleFileName(FString &path);

FString GetModuleDirPrefix();

}}

#endif

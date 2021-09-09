// Common/MyInitGuid.h

#ifndef __COMMON_MY_INITGUID_H
#define __COMMON_MY_INITGUID_H

/*
This file must be included only to one C++ file in project before
declarations of COM interfaces with DEFINE_GUID macro.

Each GUID must be initialized exactly once in project.
There are two different versions of the DEFINE_GUID macro in guiddef.h (MyGuidDef.h):
  - if INITGUID is not defined:  DEFINE_GUID declares an external reference to the symbol name.
  - if INITGUID is     defined:  DEFINE_GUID initializes the symbol name to the value of the GUID.

Also we need IID_IUnknown that is initialized in some file for linking:
  MSVC:  by default the linker uses some lib file that contains IID_IUnknown
  MinGW: add -luuid switch for linker
  WinCE: we define IID_IUnknown in this file
  Other: we define IID_IUnknown in this file
*/

#ifdef __clang__
  #pragma clang diagnostic ignored "-Wmissing-variable-declarations"
#endif

#ifdef _WIN32

#ifdef UNDER_CE
#include <basetyps.h>
#endif

#include <InitGuid.h>

#ifdef UNDER_CE
DEFINE_GUID(IID_IUnknown,
0x00000000, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
#endif

#else

#define INITGUID
#include "MyGuidDef.h"
DEFINE_GUID(IID_IUnknown,
0x00000000, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);

#endif


#endif

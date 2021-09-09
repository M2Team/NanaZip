// stdafx.h

#ifndef __STDAFX_H
#define __STDAFX_H

/* we used 0x0400 for Windows NT supporting (MENUITEMINFOW)
   But now menu problem is fixed. So it's OK to use 0x0500 (Windows 2000) */

// #define _WIN32_WINNT 0x0400
#define _WIN32_WINNT 0x0500
#define WINVER _WIN32_WINNT

#include "../../../Common/Common.h"

// #include "../../../Common/MyWindows.h"

// #include <CommCtrl.h>
// #include <ShlObj.h>
// #include <Shlwapi.h>

#endif

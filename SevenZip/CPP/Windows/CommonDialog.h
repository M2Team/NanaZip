// Windows/CommonDialog.h

#ifndef __WINDOWS_COMMON_DIALOG_H
#define __WINDOWS_COMMON_DIALOG_H

#include "../Common/MyString.h"

namespace NWindows {

bool MyGetOpenFileName(HWND hwnd, LPCWSTR title,
    LPCWSTR initialDir,  // can be NULL, so dir prefix in filePath will be used
    LPCWSTR filePath,    // full path
    LPCWSTR filterDescription,  // like "All files (*.*)"
    LPCWSTR filter,             // like "*.exe"
    UString &resPath
    #ifdef UNDER_CE
    , bool openFolder = false
    #endif
);

}

#endif

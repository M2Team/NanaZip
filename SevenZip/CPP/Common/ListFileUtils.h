// Common/ListFileUtils.h

#ifndef __COMMON_LIST_FILE_UTILS_H
#define __COMMON_LIST_FILE_UTILS_H

#include "MyString.h"
#include "MyTypes.h"

#define MY__CP_UTF16   1200
#define MY__CP_UTF16BE 1201

// bool ReadNamesFromListFile(CFSTR fileName, UStringVector &strings, UINT codePage = CP_OEMCP);

 // = CP_OEMCP
bool ReadNamesFromListFile2(CFSTR fileName, UStringVector &strings, UINT codePage,
    DWORD &lastError);

#endif

// BrowseDialog.h

#ifndef __BROWSE_DIALOG_H
#define __BROWSE_DIALOG_H

#include "../../../Common/MyString.h"

bool MyBrowseForFolder(HWND owner, LPCWSTR title, LPCWSTR path, UString &resultPath);
bool MyBrowseForFile(HWND owner, LPCWSTR title, LPCWSTR path, LPCWSTR filterDescription, LPCWSTR filter, UString &resultPath);

/* CorrectFsPath removes undesirable characters in names (dots and spaces at the end of file)
   But it doesn't change "bad" name in any of the following cases:
     - path is Super Path (with \\?\ prefix)
     - path is relative and relBase is Super Path
     - there is file or dir in filesystem with specified "bad" name */

bool CorrectFsPath(const UString &relBase, const UString &path, UString &result);

bool Dlg_CreateFolder(HWND wnd, UString &destName);

#endif

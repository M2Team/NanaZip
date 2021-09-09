// MyMessages.h

#ifndef __MY_MESSAGES_H
#define __MY_MESSAGES_H

#include "../../../Common/MyString.h"

void ShowErrorMessage(HWND window, LPCWSTR message);
inline void ShowErrorMessage(LPCWSTR message) { ShowErrorMessage(0, message); }

void ShowErrorMessageHwndRes(HWND window, UInt32 langID);
void ShowErrorMessageRes(UInt32 langID);

void ShowLastErrorMessage(HWND window = 0);

#endif

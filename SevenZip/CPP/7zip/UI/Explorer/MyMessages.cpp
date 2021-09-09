// MyMessages.cpp

#include "StdAfx.h"

#include "MyMessages.h"

#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/ResourceString.h"

#include "../FileManager/LangUtils.h"

using namespace NWindows;

void ShowErrorMessage(HWND window, LPCWSTR message)
{
  ::MessageBoxW(window, message, L"7-Zip", MB_OK | MB_ICONSTOP);
}

void ShowErrorMessageHwndRes(HWND window, UINT resID)
{
  ShowErrorMessage(window, LangString(resID));
}

void ShowErrorMessageRes(UINT resID)
{
  ShowErrorMessageHwndRes(0, resID);
}

static void ShowErrorMessageDWORD(HWND window, DWORD errorCode)
{
  ShowErrorMessage(window, NError::MyFormatMessage(errorCode));
}

void ShowLastErrorMessage(HWND window)
{
  ShowErrorMessageDWORD(window, ::GetLastError());
}

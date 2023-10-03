// MyMessages.cpp

#include "StdAfx.h"

#include "MyMessages.h"

#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/ResourceString.h"

#include "../FileManager/LangUtils.h"

using namespace NWindows;

void ShowErrorMessage(HWND window, LPCWSTR message)
{
  // **************** NanaZip Modification Start ****************
  //::MessageBoxW(window, message, L"7-Zip", MB_OK | MB_ICONSTOP);
  ::MessageBoxW(window, message, L"NanaZip", MB_OK | MB_ICONSTOP);
  // **************** NanaZip Modification End ****************
}

void ShowErrorMessageHwndRes(HWND window, UINT resID)
{
  UString s = LangString(resID);
  if (s.IsEmpty())
    s.Add_UInt32(resID);
  ShowErrorMessage(window, s);
}

void ShowErrorMessageRes(UINT resID)
{
  ShowErrorMessageHwndRes(NULL, resID);
}

static void ShowErrorMessageDWORD(HWND window, DWORD errorCode)
{
  ShowErrorMessage(window, NError::MyFormatMessage(errorCode));
}

void ShowLastErrorMessage(HWND window)
{
  ShowErrorMessageDWORD(window, ::GetLastError());
}

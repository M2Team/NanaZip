// AboutDialog.cpp

#include "StdAfx.h"

#include "../../../../C/CpuArch.h"

#include "../../../../../NanaZip/Mile.Project.Properties.h"

#include "../Common/LoadCodecs.h"

#include "AboutDialog.h"
#include "PropertyNameRes.h"

#include "LangUtils.h"

static const UInt32 kLangIDs[] =
{
  IDT_ABOUT_INFO
};

#define kHomePageURL TEXT("https://github.com/M2Team/NanaZip")

#define LLL_(quote) L##quote
#define LLL(quote) LLL_(quote)

extern CCodecs *g_CodecsObj;

bool CAboutDialog::OnInit()
{
  #ifdef EXTERNAL_CODECS
  if (g_CodecsObj)
  {
    UString s;
    g_CodecsObj->GetCodecsErrorMessage(s);
    if (!s.IsEmpty())
      MessageBoxW(GetParent(), s, L"NanaZip", MB_ICONERROR);
  }
  #endif

  LangSetDlgItems(*this, kLangIDs, ARRAY_SIZE(kLangIDs));
  SetItemText(IDT_ABOUT_VERSION, UString("NanaZip " MILE_PROJECT_VERSION_UTF8_STRING " (" MY_CPU_NAME ")"));

  LangSetWindowText(*this, IDD_ABOUT);
  NormalizePosition();
  return CModalDialog::OnInit();
}

bool CAboutDialog::OnButtonClicked(int buttonID, HWND buttonHWND)
{
  LPCTSTR url;
  switch (buttonID)
  {
    case IDB_ABOUT_HOMEPAGE: url = kHomePageURL; break;
    default:
      return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
  }

  #ifdef UNDER_CE
  SHELLEXECUTEINFO s;
  memset(&s, 0, sizeof(s));
  s.cbSize = sizeof(s);
  s.lpFile = url;
  ::ShellExecuteEx(&s);
  #else
  ::ShellExecute(NULL, NULL, url, NULL, NULL, SW_SHOWNORMAL);
  #endif

  return true;
}

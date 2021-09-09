// HelpUtils.cpp

#include "StdAfx.h"

#include "HelpUtils.h"

#if defined(UNDER_CE) || !defined(_WIN32) /* || !defined(_MSC_VER) */

void ShowHelpWindow(LPCSTR)
{
}

#else

#include <HtmlHelp.h>

#include "../../../Common/StringConvert.h"

#include "../../../Windows/DLL.h"

#define kHelpFileName "7-zip.chm::/"

void ShowHelpWindow(LPCSTR topicFile)
{
  FString path = NWindows::NDLL::GetModuleDirPrefix();
  path += kHelpFileName;
  path += topicFile;
  // HWND hwnd = NULL;
  HtmlHelp(NULL, GetSystemString(fs2us(path)), HH_DISPLAY_TOPIC, 0);
}

#endif

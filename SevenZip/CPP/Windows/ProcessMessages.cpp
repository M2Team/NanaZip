// Windows/ProcessMessages.cpp

#include "../../../ThirdParty/LZMA/CPP/Windows/StdAfx.h"

#include "ProcessMessages.h"

namespace NWindows {

void ProcessMessages(HWND window)
{
  MSG msg;
  while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
  {
    if (window == (HWND) NULL || !IsDialogMessage(window, &msg))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
}

}

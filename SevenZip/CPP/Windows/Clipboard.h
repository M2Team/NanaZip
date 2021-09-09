// Windows/Clipboard.h

#ifndef __CLIPBOARD_H
#define __CLIPBOARD_H

#include "../Common/MyString.h"

namespace NWindows {

class CClipboard
{
  bool m_Open;
public:
  CClipboard(): m_Open(false) {};
  ~CClipboard() { Close(); }
  bool Open(HWND wndNewOwner) throw();
  bool Close() throw();
};

bool ClipboardIsFormatAvailableHDROP();

// bool ClipboardGetFileNames(UStringVector &names);
// bool ClipboardGetTextString(AString &s);
bool ClipboardSetText(HWND owner, const UString &s);

}

#endif

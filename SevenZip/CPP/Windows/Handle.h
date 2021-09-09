// Windows/Handle.h

#ifndef __WINDOWS_HANDLE_H
#define __WINDOWS_HANDLE_H

#include "../Common/MyTypes.h"

namespace NWindows {

class CHandle  MY_UNCOPYABLE
{
protected:
  HANDLE _handle;
public:
  operator HANDLE() { return _handle; }
  CHandle(): _handle(NULL) {}
  ~CHandle() { Close(); }
  bool IsCreated() const { return (_handle != NULL); }
  bool Close()
  {
    if (_handle == NULL)
      return true;
    if (!::CloseHandle(_handle))
      return false;
    _handle = NULL;
    return true;
  }
  void Attach(HANDLE handle) { _handle = handle; }
  HANDLE Detach()
  {
    HANDLE handle = _handle;
    _handle = NULL;
    return handle;
  }
};

}

#endif

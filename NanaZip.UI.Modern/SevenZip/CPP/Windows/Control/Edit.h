// Windows/Control/Edit.h

#ifndef __WINDOWS_CONTROL_EDIT_H
#define __WINDOWS_CONTROL_EDIT_H

// **************** NanaZip Modification Start ****************
#include <Shlwapi.h>
// **************** NanaZip Modification End ****************

#include "../Window.h"

namespace NWindows {
namespace NControl {

class CEdit: public CWindow
{
public:
  // **************** NanaZip Modification Start ****************
  void Attach(HWND newWindow) override {
    _window = newWindow;
    ::SHAutoComplete(
        _window,
        SHACF_AUTOAPPEND_FORCE_OFF | SHACF_AUTOSUGGEST_FORCE_OFF);
  }
  // **************** NanaZip Modification End ****************
  void SetPasswordChar(WPARAM c) { SendMsg(EM_SETPASSWORDCHAR, c); }
};

}}

#endif

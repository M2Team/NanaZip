// Windows/Control/Edit.h

#ifndef __WINDOWS_CONTROL_EDIT_H
#define __WINDOWS_CONTROL_EDIT_H

#include "../Window.h"

namespace NWindows {
namespace NControl {

class CEdit: public CWindow
{
public:
  void SetPasswordChar(WPARAM c) { SendMsg(EM_SETPASSWORDCHAR, c); }
};

}}

#endif

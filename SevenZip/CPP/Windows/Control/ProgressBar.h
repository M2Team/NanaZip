// Windows/Control/ProgressBar.h

#ifndef __WINDOWS_CONTROL_PROGRESSBAR_H
#define __WINDOWS_CONTROL_PROGRESSBAR_H

#include "../../Common/MyWindows.h"

#include <CommCtrl.h>

#include "../Window.h"

namespace NWindows {
namespace NControl {

class CProgressBar: public CWindow
{
public:
  LRESULT SetPos(int pos) { return SendMsg(PBM_SETPOS, pos, 0); }
  LRESULT DeltaPos(int increment) { return SendMsg(PBM_DELTAPOS, increment, 0); }
  UINT GetPos() { return (UINT)SendMsg(PBM_GETPOS, 0, 0); }
  LRESULT SetRange(unsigned short minValue, unsigned short maxValue) { return SendMsg(PBM_SETRANGE, 0, MAKELPARAM(minValue, maxValue)); }
  DWORD SetRange32(int minValue, int maxValue) { return (DWORD)SendMsg(PBM_SETRANGE32, minValue, maxValue); }
  int SetStep(int step) { return (int)SendMsg(PBM_SETSTEP, step, 0); }
  LRESULT StepIt() { return SendMsg(PBM_STEPIT, 0, 0); }
  INT GetRange(bool minValue, PPBRANGE range) { return (INT)SendMsg(PBM_GETRANGE, BoolToBOOL(minValue), (LPARAM)range); }
  
  #ifndef UNDER_CE
  COLORREF SetBarColor(COLORREF color) { return (COLORREF)SendMsg(PBM_SETBARCOLOR, 0, color); }
  COLORREF SetBackgroundColor(COLORREF color) { return (COLORREF)SendMsg(PBM_SETBKCOLOR, 0, color); }
  #endif
};

}}

#endif

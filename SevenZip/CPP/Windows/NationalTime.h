// Windows/NationalTime.h

#ifndef __WINDOWS_NATIONAL_TIME_H
#define __WINDOWS_NATIONAL_TIME_H

#include "../Common/MyString.h"

namespace NWindows {
namespace NNational {
namespace NTime {

bool MyGetTimeFormat(LCID locale, DWORD flags, CONST SYSTEMTIME *time,
    LPCTSTR format, CSysString &resultString);

bool MyGetDateFormat(LCID locale, DWORD flags, CONST SYSTEMTIME *time,
    LPCTSTR format, CSysString &resultString);

}}}

#endif

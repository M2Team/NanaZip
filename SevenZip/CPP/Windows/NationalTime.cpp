// Windows/NationalTime.cpp

#include "StdAfx.h"

#include "NationalTime.h"

namespace NWindows {
namespace NNational {
namespace NTime {

bool MyGetTimeFormat(LCID locale, DWORD flags, CONST SYSTEMTIME *time,
    LPCTSTR format, CSysString &resultString)
{
  resultString.Empty();
  int numChars = ::GetTimeFormat(locale, flags, time, format, NULL, 0);
  if (numChars == 0)
    return false;
  numChars = ::GetTimeFormat(locale, flags, time, format,
      resultString.GetBuf(numChars), numChars + 1);
  resultString.ReleaseBuf_CalcLen(numChars);
  return (numChars != 0);
}

bool MyGetDateFormat(LCID locale, DWORD flags, CONST SYSTEMTIME *time,
    LPCTSTR format, CSysString &resultString)
{
  resultString.Empty();
  int numChars = ::GetDateFormat(locale, flags, time, format, NULL, 0);
  if (numChars == 0)
    return false;
  numChars = ::GetDateFormat(locale, flags, time, format,
      resultString.GetBuf(numChars), numChars + 1);
  resultString.ReleaseBuf_CalcLen(numChars);
  return (numChars != 0);
}

}}}

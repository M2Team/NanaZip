// Windows/Control/ComboBox.cpp

#include "StdAfx.h"

#ifndef _UNICODE
#include "../../Common/StringConvert.h"
#endif

#include "ComboBox.h"

#ifndef _UNICODE
extern bool g_IsNT;
#endif

namespace NWindows {
namespace NControl {

LRESULT CComboBox::GetLBText(int index, CSysString &s)
{
  s.Empty();
  LRESULT len = GetLBTextLen(index); // length, excluding the terminating null character
  if (len == CB_ERR)
    return len;
  LRESULT len2 = GetLBText(index, s.GetBuf((unsigned)len));
  if (len2 == CB_ERR)
    return len;
  if (len > len2)
    len = len2;
  s.ReleaseBuf_CalcLen((unsigned)len);
  return len;
}

#ifndef _UNICODE
LRESULT CComboBox::AddString(LPCWSTR s)
{
  if (g_IsNT)
    return SendMsgW(CB_ADDSTRING, 0, (LPARAM)s);
  return AddString(GetSystemString(s));
}

LRESULT CComboBox::GetLBText(int index, UString &s)
{
  s.Empty();
  if (g_IsNT)
  {
    LRESULT len = SendMsgW(CB_GETLBTEXTLEN, MY__int_TO_WPARAM(index), 0);
    if (len == CB_ERR)
      return len;
    LRESULT len2 = SendMsgW(CB_GETLBTEXT, MY__int_TO_WPARAM(index), (LPARAM)s.GetBuf((unsigned)len));
    if (len2 == CB_ERR)
      return len;
    if (len > len2)
      len = len2;
    s.ReleaseBuf_CalcLen((unsigned)len);
    return len;
  }
  AString sa;
  LRESULT len = GetLBText(index, sa);
  if (len == CB_ERR)
    return len;
  s = GetUnicodeString(sa);
  return s.Len();
}
#endif

}}

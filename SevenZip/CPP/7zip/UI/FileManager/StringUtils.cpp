// StringUtils.cpp

#include "StdAfx.h"

#include "StringUtils.h"

void SplitStringToTwoStrings(const UString &src, UString &dest1, UString &dest2)
{
  dest1.Empty();
  dest2.Empty();
  bool quoteMode = false;
  for (unsigned i = 0; i < src.Len(); i++)
  {
    const wchar_t c = src[i];
    if (c == '\"')
      quoteMode = !quoteMode;
    else if (c == ' ' && !quoteMode)
    {
      dest2 = src.Ptr(i + 1);
      return;
    }
    else
      dest1 += c;
  }
}

void SplitString(const UString &srcString, UStringVector &destStrings)
{
  destStrings.Clear();
  unsigned len = srcString.Len();
  if (len == 0)
    return;
  UString s;
  for (unsigned i = 0; i < len; i++)
  {
    wchar_t c = srcString[i];
    if (c == ' ')
    {
      if (!s.IsEmpty())
      {
        destStrings.Add(s);
        s.Empty();
      }
    }
    else
      s += c;
  }
  if (!s.IsEmpty())
    destStrings.Add(s);
}

/*
UString JoinStrings(const UStringVector &srcStrings)
{

  UString s;
  FOR_VECTOR (i, srcStrings)
  {
    if (i != 0)
      s.Add_Space();
    s += srcStrings[i];
  }
  return s;
}
*/

// Common/TextConfig.cpp

#include "StdAfx.h"

#include "TextConfig.h"
#include "UTFConvert.h"

static inline bool IsDelimitChar(char c)
{
  return (c == ' ' || c == 0x0A || c == 0x0D || c == '\0' || c == '\t');
}
    
static AString GetIDString(const char *s, unsigned &finishPos)
{
  AString result;
  for (finishPos = 0; ; finishPos++)
  {
    char c = s[finishPos];
    if (IsDelimitChar(c) || c == '=')
      break;
    result += c;
  }
  return result;
}

static bool WaitNextLine(const AString &s, unsigned &pos)
{
  for (; pos < s.Len(); pos++)
    if (s[pos] == 0x0A)
      return true;
  return false;
}

static bool SkipSpaces(const AString &s, unsigned &pos)
{
  for (; pos < s.Len(); pos++)
  {
    char c = s[pos];
    if (!IsDelimitChar(c))
    {
      if (c != ';')
        return true;
      if (!WaitNextLine(s, pos))
        return false;
    }
  }
  return false;
}

bool GetTextConfig(const AString &s, CObjectVector<CTextConfigPair> &pairs)
{
  pairs.Clear();
  unsigned pos = 0;

  /////////////////////
  // read strings

  for (;;)
  {
    if (!SkipSpaces(s, pos))
      break;
    CTextConfigPair pair;
    unsigned finishPos;
    const AString temp (GetIDString(((const char *)s) + pos, finishPos));
    if (!ConvertUTF8ToUnicode(temp, pair.ID))
      return false;
    if (finishPos == 0)
      return false;
    pos += finishPos;
    if (!SkipSpaces(s, pos))
      return false;
    if (s[pos] != '=')
      return false;
    pos++;
    if (!SkipSpaces(s, pos))
      return false;
    if (s[pos] != '\"')
      return false;
    pos++;
    AString message;
    for (;;)
    {
      if (pos >= s.Len())
        return false;
      char c = s[pos++];
      if (c == '\"')
        break;
      if (c == '\\')
      {
        c = s[pos++];
        switch (c)
        {
          case 'n': message += '\n'; break;
          case 't': message += '\t'; break;
          case '\\': message += '\\'; break;
          case '\"': message += '\"'; break;
          default: message += '\\'; message += c; break;
        }
      }
      else
        message += c;
    }
    if (!ConvertUTF8ToUnicode(message, pair.String))
      return false;
    pairs.Add(pair);
  }
  return true;
}

int FindTextConfigItem(const CObjectVector<CTextConfigPair> &pairs, const char *id) throw()
{
  FOR_VECTOR (i, pairs)
    if (pairs[i].ID.IsEqualTo(id))
      return i;
  return -1;
}

UString GetTextConfigValue(const CObjectVector<CTextConfigPair> &pairs, const char *id)
{
  int index = FindTextConfigItem(pairs, id);
  if (index < 0)
    return UString();
  return pairs[index].String;
}

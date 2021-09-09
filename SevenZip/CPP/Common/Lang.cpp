// Common/Lang.cpp

#include "StdAfx.h"

#include "Lang.h"
#include "StringToInt.h"
#include "UTFConvert.h"

#include "../Windows/FileIO.h"

void CLang::Clear() throw()
{
  _ids.Clear();
  _offsets.Clear();
  delete []_text;
  _text = 0;
}

static const char * const kLangSignature = ";!@Lang2@!UTF-8!";

bool CLang::OpenFromString(const AString &s2)
{
  UString s;
  if (!ConvertUTF8ToUnicode(s2, s))
    return false;
  unsigned i = 0;
  if (s.IsEmpty())
    return false;
  if (s[0] == 0xFEFF)
    i++;

  for (const char *p = kLangSignature;; i++)
  {
    Byte c = (Byte)(*p++);
    if (c == 0)
      break;
    if (s[i] != c)
      return false;
  }

  _text = new wchar_t[s.Len() - i + 1];
  wchar_t *text = _text;

  Int32 id = -100;
  UInt32 pos = 0;

  while (i < s.Len())
  {
    unsigned start = pos;
    do
    {
      wchar_t c = s[i++];
      if (c == '\n')
        break;
      if (c == '\\')
      {
        if (i == s.Len())
          return false;
        c = s[i++];
        switch (c)
        {
          case '\n': return false;
          case 'n': c = '\n'; break;
          case 't': c = '\t'; break;
          case '\\': c = '\\'; break;
          default: text[pos++] = L'\\'; break;
        }
      }
      text[pos++] = c;
    }
    while (i < s.Len());

    {
      unsigned j = start;
      for (; j < pos; j++)
        if (text[j] != ' ')
          break;
      if (j == pos)
      {
        id++;
        continue;
      }
    }
    if (text[start] == ';')
    {
      pos = start;
      id++;
      continue;
    }
    
    text[pos++] = 0;
    const wchar_t *end;
    UInt32 id32 = ConvertStringToUInt32(text + start, &end);
    if (*end == 0)
    {
      if (id32 > ((UInt32)1 << 30) || (Int32)id32 < id)
        return false;
      id = (Int32)id32;
      pos = start;
      continue;
    }

    if (id < 0)
      return false;
    _ids.Add((UInt32)id++);
    _offsets.Add(start);
  }

  return true;
}

bool CLang::Open(CFSTR fileName, const char *id)
{
  Clear();
  NWindows::NFile::NIO::CInFile file;
  if (!file.Open(fileName))
    return false;
  UInt64 length;
  if (!file.GetLength(length))
    return false;
  if (length > (1 << 20))
    return false;
  
  AString s;
  const unsigned len = (unsigned)length;
  char *p = s.GetBuf(len);
  size_t processed;
  if (!file.ReadFull(p, len, processed))
    return false;
  file.Close();
  if (len != processed)
    return false;

  char *p2 = p;
  for (unsigned i = 0; i < len; i++)
  {
    char c = p[i];
    if (c == 0)
      break;
    if (c != 0x0D)
      *p2++ = c;
  }
  *p2 = 0;
  s.ReleaseBuf_SetLen((unsigned)(p2 - p));
  
  if (OpenFromString(s))
  {
    const wchar_t *name = Get(0);
    if (name && StringsAreEqual_Ascii(name, id))
      return true;
  }
  
  Clear();
  return false;
}

const wchar_t *CLang::Get(UInt32 id) const throw()
{
  int index = _ids.FindInSorted(id);
  if (index < 0)
    return NULL;
  return _text + (size_t)_offsets[(unsigned)index];
}

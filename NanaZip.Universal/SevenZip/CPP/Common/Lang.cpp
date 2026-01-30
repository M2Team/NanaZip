// Common/Lang.cpp

#include "StdAfx.h"

#include "Lang.h"
#include "StringToInt.h"
#include "UTFConvert.h"

#include "../Windows/FileIO.h"

// **************** NanaZip Modification Start ****************
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********

void CLang::Clear() throw()
{
  _ids.Clear();
  _offsets.Clear();
  Comments.Clear();
  delete []_text;
  _text = NULL;
}

static const char * const kLangSignature = ";!@Lang2@!UTF-8!\n";

bool CLang::OpenFromString(const AString &s2)
{
  UString su;
  if (!ConvertUTF8ToUnicode(s2, su))
    return false;
  if (su.IsEmpty())
    return false;
  const wchar_t *s = su;
  const wchar_t *sLim = s + su.Len();
  if (*s == 0xFEFF)
    s++;
  for (const char *p = kLangSignature;; s++)
  {
    const Byte c = (Byte)(*p++);
    if (c == 0)
      break;
    if (*s != c)
      return false;
  }

  wchar_t *text = new wchar_t[(size_t)(sLim - s) + 1];
  _text = text;

  UString comment;
  Int32 id = -1024;
  unsigned pos = 0;

  while (s != sLim)
  {
    const unsigned start = pos;
    do
    {
      wchar_t c = *s++;
      if (c == '\n')
        break;
      if (c == '\\')
      {
        if (s == sLim)
          return false;
        c = *s++;
        switch (c)
        {
          case '\n': return false;
          case 'n': c = '\n'; break;
          case 't': c = '\t'; break;
          case '\\': /* c = '\\'; */ break;
          default: text[pos++] = L'\\'; break;
        }
      }
      text[pos++] = c;
    }
    while (s != sLim);

    {
      unsigned j = start;
      for (; j < pos; j++)
        if (text[j] != ' ' && text[j] != '\t')
          break;
      if (j == pos)
      {
        id++;
        pos = start;
        continue;
      }
    }

    // start != pos
    text[pos++] = 0;

    if (text[start] == ';')
    {
      comment = text + start;
      comment.TrimRight();
      if (comment.Len() != 1)
        Comments.Add(comment);
      id++;
      pos = start;
      continue;
    }

    const wchar_t *end;
    const UInt32 id32 = ConvertStringToUInt32(text + start, &end);
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
    const char c = p[i];
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
  const int index = _ids.FindInSorted(id);
  if (index < 0)
    return NULL;
  return _text + (size_t)_offsets[(unsigned)index];
}

#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********

#ifdef Z7_SFX
#include "../Windows/ResourceString.h"

#include <mutex>
#include <map>
std::mutex g_LanguageLock;
std::map<UInt32, UString> g_LanguageMap;
#endif

void CLang::Clear() throw()
{

}

bool CLang::OpenFromString(const AString& s2)
{
    UNREFERENCED_PARAMETER(s2);
    return true;
}

bool CLang::Open(CFSTR fileName, const char* id)
{
    UNREFERENCED_PARAMETER(fileName);
    UNREFERENCED_PARAMETER(id);
    return true;
}

const wchar_t *CLang::Get(UInt32 id) const throw()
{
#ifdef Z7_SFX
  std::lock_guard Lock(g_LanguageLock);
  auto Iterator = g_LanguageMap.find(id);
  if (g_LanguageMap.end() == Iterator)
  {
      return g_LanguageMap.emplace(
          id,
          NWindows::MyLoadString(id)).first->second;
  }
  return Iterator->second;
#else
  UNREFERENCED_PARAMETER(id);
  return nullptr;
#endif
}

// **************** NanaZip Modification End ****************

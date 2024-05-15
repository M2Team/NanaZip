// Common/StdOutStream.cpp

#include "StdAfx.h"

#ifdef _WIN32
#include <tchar.h>
#endif

#include "IntToString.h"
#include "StdOutStream.h"
#include "StringConvert.h"
#include "UTFConvert.h"

CStdOutStream g_StdOut(stdout);
CStdOutStream g_StdErr(stderr);

/*
// #define kFileOpenMode "wt"

bool CStdOutStream::Open(const char *fileName) throw()
{
  Close();
  _stream = fopen(fileName, kFileOpenMode);
  _streamIsOpen = (_stream != 0);
  return _streamIsOpen;
}

bool CStdOutStream::Close() throw()
{
  if (!_streamIsOpen)
    return true;
  if (fclose(_stream) != 0)
    return false;
  _stream = 0;
  _streamIsOpen = false;
  return true;
}
*/

bool CStdOutStream::Flush() throw()
{
  return (fflush(_stream) == 0);
}

CStdOutStream & endl(CStdOutStream & outStream) throw()
{
  return outStream << '\n';
}

CStdOutStream & CStdOutStream::operator<<(const wchar_t *s)
{
  AString temp;
  UString s2(s);
  PrintUString(s2, temp);
  return *this;
}

void CStdOutStream::PrintUString(const UString &s, AString &temp)
{
  Convert_UString_to_AString(s, temp);
  *this << (const char *)temp;
}

void CStdOutStream::Convert_UString_to_AString(const UString &src, AString &dest)
{
  int codePage = CodePage;
  if (codePage == -1)
    codePage = CP_OEMCP;
  if ((unsigned)codePage == CP_UTF8)
    ConvertUnicodeToUTF8(src, dest);
  else
    UnicodeStringToMultiByte2(dest, src, (UINT)(unsigned)codePage);
}


static const wchar_t kReplaceChar = '_';

/*
void CStdOutStream::Normalize_UString_LF_Allowed(UString &s)
{
  if (!IsTerminalMode)
    return;

  const unsigned len = s.Len();
  wchar_t *d = s.GetBuf();

    for (unsigned i = 0; i < len; i++)
    {
      const wchar_t c = d[i];
      if (c == 0x1b || (c <= 13 && c >= 7 && c != '\n'))
        d[i] = kReplaceChar;
    }
}
*/

void CStdOutStream::Normalize_UString(UString &s)
{
  const unsigned len = s.Len();
  wchar_t *d = s.GetBuf();

  if (IsTerminalMode)
    for (unsigned i = 0; i < len; i++)
    {
      const wchar_t c = d[i];
      if ((c <= 13 && c >= 7) || c == 0x1b)
        d[i] = kReplaceChar;
    }
  else
    for (unsigned i = 0; i < len; i++)
    {
      const wchar_t c = d[i];
      if (c == '\n')
        d[i] = kReplaceChar;
    }
}

void CStdOutStream::Normalize_UString_Path(UString &s)
{
  if (ListPathSeparatorSlash.Def)
  {
#ifdef _WIN32
    if (ListPathSeparatorSlash.Val)
      s.Replace(L'\\', L'/');
#else
    if (!ListPathSeparatorSlash.Val)
      s.Replace(L'/', L'\\');
#endif
  }
  Normalize_UString(s);
}


/*
void CStdOutStream::Normalize_UString(UString &src)
{
  const wchar_t *s = src.Ptr();
  const unsigned len = src.Len();
  unsigned i;
  for (i = 0; i < len; i++)
  {
    const wchar_t c = s[i];
#if 0 && !defined(_WIN32)
    if (c == '\\') // IsTerminalMode &&
      break;
#endif
    if ((unsigned)c < 0x20)
      break;
  }
  if (i == len)
    return;

  UString temp;
  for (i = 0; i < len; i++)
  {
    wchar_t c = s[i];
#if 0 && !defined(_WIN32)
    if (c == '\\')
      temp += (wchar_t)L'\\';
    else
#endif
    if ((unsigned)c < 0x20)
    {
      if (c == '\n'
        || (IsTerminalMode && (c == 0x1b || (c <= 13 && c >= 7))))
      {
#if 1 || defined(_WIN32)
        c = (wchar_t)kReplaceChar;
#else
        temp += (wchar_t)L'\\';
             if (c == '\n') c = L'n';
        else if (c == '\r') c = L'r';
        else if (c == '\a') c = L'a';
        else if (c == '\b') c = L'b';
        else if (c == '\t') c = L't';
        else if (c == '\v') c = L'v';
        else if (c == '\f') c = L'f';
        else
        {
          temp += (wchar_t)(L'0' + (unsigned)c / 64);
          temp += (wchar_t)(L'0' + (unsigned)c / 8 % 8);
          c     = (wchar_t)(L'0' + (unsigned)c % 8);
        }
#endif
      }
    }
    temp += c;
  }
  src = temp;
}
*/

void CStdOutStream::NormalizePrint_UString_Path(const UString &s, UString &tempU, AString &tempA)
{
  tempU = s;
  Normalize_UString_Path(tempU);
  PrintUString(tempU, tempA);
}

void CStdOutStream::NormalizePrint_UString_Path(const UString &s)
{
  UString tempU;
  AString tempA;
  NormalizePrint_UString_Path(s, tempU, tempA);
}

void CStdOutStream::NormalizePrint_wstr_Path(const wchar_t *s)
{
  UString tempU = s;
  Normalize_UString_Path(tempU);
  AString tempA;
  PrintUString(tempU, tempA);
}

void CStdOutStream::NormalizePrint_UString(const UString &s)
{
  UString tempU = s;
  Normalize_UString(tempU);
  AString tempA;
  PrintUString(tempU, tempA);
}

CStdOutStream & CStdOutStream::operator<<(Int32 number) throw()
{
  char s[32];
  ConvertInt64ToString(number, s);
  return operator<<(s);
}

CStdOutStream & CStdOutStream::operator<<(Int64 number) throw()
{
  char s[32];
  ConvertInt64ToString(number, s);
  return operator<<(s);
}

CStdOutStream & CStdOutStream::operator<<(UInt32 number) throw()
{
  char s[16];
  ConvertUInt32ToString(number, s);
  return operator<<(s);
}

CStdOutStream & CStdOutStream::operator<<(UInt64 number) throw()
{
  char s[32];
  ConvertUInt64ToString(number, s);
  return operator<<(s);
}

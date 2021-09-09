// Common/IntToString.cpp

#include "StdAfx.h"

#include "../../C/CpuArch.h"

#include "IntToString.h"

#define CONVERT_INT_TO_STR(charType, tempSize) \
  unsigned char temp[tempSize]; unsigned i = 0; \
  while (val >= 10) { temp[i++] = (unsigned char)('0' + (unsigned)(val % 10)); val /= 10; } \
  *s++ = (charType)('0' + (unsigned)val); \
  while (i != 0) { i--; *s++ = (charType)temp[i]; } \
  *s = 0; \
  return s;

char * ConvertUInt32ToString(UInt32 val, char *s) throw()
{
  CONVERT_INT_TO_STR(char, 16);
}

char * ConvertUInt64ToString(UInt64 val, char *s) throw()
{
  if (val <= (UInt32)0xFFFFFFFF)
  {
    return ConvertUInt32ToString((UInt32)val, s);
  }
  CONVERT_INT_TO_STR(char, 24);
}

void ConvertUInt64ToOct(UInt64 val, char *s) throw()
{
  UInt64 v = val;
  unsigned i;
  for (i = 1;; i++)
  {
    v >>= 3;
    if (v == 0)
      break;
  }
  s[i] = 0;
  do
  {
    unsigned t = (unsigned)(val & 0x7);
    val >>= 3;
    s[--i] = (char)('0' + t);
  }
  while (i);
}


#define GET_HEX_CHAR(t) ((char)(((t < 10) ? ('0' + t) : ('A' + (t - 10)))))

static inline char GetHexChar(unsigned t) { return GET_HEX_CHAR(t); }


void ConvertUInt32ToHex(UInt32 val, char *s) throw()
{
  UInt32 v = val;
  unsigned i;
  for (i = 1;; i++)
  {
    v >>= 4;
    if (v == 0)
      break;
  }
  s[i] = 0;
  do
  {
    unsigned t = (unsigned)(val & 0xF);
    val >>= 4;
    s[--i] = GET_HEX_CHAR(t);
  }
  while (i);
}


void ConvertUInt64ToHex(UInt64 val, char *s) throw()
{
  UInt64 v = val;
  unsigned i;
  for (i = 1;; i++)
  {
    v >>= 4;
    if (v == 0)
      break;
  }
  s[i] = 0;
  do
  {
    unsigned t = (unsigned)(val & 0xF);
    val >>= 4;
    s[--i] = GET_HEX_CHAR(t);
  }
  while (i);
}

void ConvertUInt32ToHex8Digits(UInt32 val, char *s) throw()
{
  s[8] = 0;
  for (int i = 7; i >= 0; i--)
  {
    unsigned t = val & 0xF;
    val >>= 4;
    s[i] = GET_HEX_CHAR(t);;
  }
}

/*
void ConvertUInt32ToHex8Digits(UInt32 val, wchar_t *s)
{
  s[8] = 0;
  for (int i = 7; i >= 0; i--)
  {
    unsigned t = val & 0xF;
    val >>= 4;
    s[i] = (wchar_t)(((t < 10) ? ('0' + t) : ('A' + (t - 10))));
  }
}
*/

wchar_t * ConvertUInt32ToString(UInt32 val, wchar_t *s) throw()
{
  CONVERT_INT_TO_STR(wchar_t, 16);
}

wchar_t * ConvertUInt64ToString(UInt64 val, wchar_t *s) throw()
{
  if (val <= (UInt32)0xFFFFFFFF)
  {
    return ConvertUInt32ToString((UInt32)val, s);
  }
  CONVERT_INT_TO_STR(wchar_t, 24);
}

void ConvertInt64ToString(Int64 val, char *s) throw()
{
  if (val < 0)
  {
    *s++ = '-';
    val = -val;
  }
  ConvertUInt64ToString((UInt64)val, s);
}

void ConvertInt64ToString(Int64 val, wchar_t *s) throw()
{
  if (val < 0)
  {
    *s++ = L'-';
    val = -val;
  }
  ConvertUInt64ToString((UInt64)val, s);
}


static void ConvertByteToHex2Digits(unsigned v, char *s) throw()
{
  s[0] = GetHexChar(v >> 4);
  s[1] = GetHexChar(v & 0xF);
}

static void ConvertUInt16ToHex4Digits(UInt32 val, char *s) throw()
{
  ConvertByteToHex2Digits(val >> 8, s);
  ConvertByteToHex2Digits(val & 0xFF, s + 2);
}

char *RawLeGuidToString(const Byte *g, char *s) throw()
{
  ConvertUInt32ToHex8Digits(GetUi32(g   ),  s);  s += 8;  *s++ = '-';
  ConvertUInt16ToHex4Digits(GetUi16(g + 4), s);  s += 4;  *s++ = '-';
  ConvertUInt16ToHex4Digits(GetUi16(g + 6), s);  s += 4;  *s++ = '-';
  for (unsigned i = 0; i < 8; i++)
  {
    if (i == 2)
      *s++ = '-';
    ConvertByteToHex2Digits(g[8 + i], s);
    s += 2;
  }
  *s = 0;
  return s;
}

char *RawLeGuidToString_Braced(const Byte *g, char *s) throw()
{
  *s++ = '{';
  s = RawLeGuidToString(g, s);
  *s++ = '}';
  *s = 0;
  return s;
}

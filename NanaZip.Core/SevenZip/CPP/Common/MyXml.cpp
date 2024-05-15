// MyXml.cpp

#include "StdAfx.h"

#include "MyXml.h"
#include "StringToInt.h"

static bool IsValidChar(char c)
{
  return
    (c >= 'a' && c <= 'z') ||
    (c >= 'A' && c <= 'Z') ||
    (c >= '0' && c <= '9') ||
    c == '-';
}

static bool IsSpaceChar(char c)
{
  return (c == ' ' || c == '\t' || c == 0x0D || c == 0x0A);
}

#define SKIP_SPACES(s) while (IsSpaceChar(*s)) s++;

int CXmlItem::FindProp(const char *propName) const throw()
{
  FOR_VECTOR (i, Props)
    if (Props[i].Name == propName)
      return (int)i;
  return -1;
}

AString CXmlItem::GetPropVal(const char *propName) const
{
  const int index = FindProp(propName);
  if (index >= 0)
    return Props[(unsigned)index].Value;
  return AString();
}

bool CXmlItem::IsTagged(const char *tag) const throw()
{
  return (IsTag && Name == tag);
}

int CXmlItem::FindSubTag(const char *tag) const throw()
{
  FOR_VECTOR (i, SubItems)
    if (SubItems[i].IsTagged(tag))
      return (int)i;
  return -1;
}

const CXmlItem *CXmlItem::FindSubTag_GetPtr(const char *tag) const throw()
{
  FOR_VECTOR (i, SubItems)
  {
    const CXmlItem *p = &SubItems[i];
    if (p->IsTagged(tag))
      return p;
  }
  return NULL;
}

AString CXmlItem::GetSubString() const
{
  if (SubItems.Size() == 1)
  {
    const CXmlItem &item = SubItems[0];
    if (!item.IsTag)
      return item.Name;
  }
  return AString();
}

const AString * CXmlItem::GetSubStringPtr() const throw()
{
  if (SubItems.Size() == 1)
  {
    const CXmlItem &item = SubItems[0];
    if (!item.IsTag)
      return &item.Name;
  }
  return NULL;
}

AString CXmlItem::GetSubStringForTag(const char *tag) const
{
  const CXmlItem *item = FindSubTag_GetPtr(tag);
  if (item)
    return item->GetSubString();
  return AString();
}

const char * CXmlItem::ParseItem(const char *s, int numAllowedLevels)
{
  SKIP_SPACES(s)

  const char *beg = s;
  for (;;)
  {
    char c;
    c = *s; if (c == 0 || c == '<') break; s++;
    c = *s; if (c == 0 || c == '<') break; s++;
  }
  if (*s == 0)
    return NULL;
  {
    const size_t num = (size_t)(s - beg);
    if (num)
    {
      IsTag = false;
      Name.SetFrom_Chars_SizeT(beg, num);
      return s;
    }
  }
  
  IsTag = true;

  s++;
  SKIP_SPACES(s)

  beg = s;
  for (;; s++)
    if (!IsValidChar(*s))
      break;
  if (s == beg || *s == 0)
    return NULL;
  Name.SetFrom_Chars_SizeT(beg, (size_t)(s - beg));

  for (;;)
  {
    beg = s;
    SKIP_SPACES(s)
    if (*s == '/')
    {
      s++;
      // SKIP_SPACES(s)
      if (*s != '>')
        return NULL;
      return s + 1;
    }
    if (*s == '>')
    {
      s++;
      if (numAllowedLevels == 0)
        return NULL;
      SubItems.Clear();
      for (;;)
      {
        SKIP_SPACES(s)
        if (s[0] == '<' && s[1] == '/')
          break;
        CXmlItem &item = SubItems.AddNew();
        s = item.ParseItem(s, numAllowedLevels - 1);
        if (!s)
          return NULL;
      }

      s += 2;
      const unsigned len = Name.Len();
      const char *name = Name.Ptr();
      for (unsigned i = 0; i < len; i++)
        if (*s++ != *name++)
          return NULL;
      // s += len;
      if (s[0] != '>')
        return NULL;
      return s + 1;
    }
    if (beg == s)
      return NULL;

    // ReadProperty
    CXmlProp &prop = Props.AddNew();

    beg = s;
    for (;; s++)
    {
      char c = *s;
      if (!IsValidChar(c))
        break;
    }
    if (s == beg)
      return NULL;
    prop.Name.SetFrom_Chars_SizeT(beg, (size_t)(s - beg));
    
    SKIP_SPACES(s)
    if (*s != '=')
      return NULL;
    s++;
    SKIP_SPACES(s)
    if (*s != '\"')
      return NULL;
    s++;
    
    beg = s;
    for (;;)
    {
      char c = *s;
      if (c == 0)
        return NULL;
      if (c == '\"')
        break;
      s++;
    }
    prop.Value.SetFrom_Chars_SizeT(beg, (size_t)(s - beg));
    s++;
  }
}

static const char * SkipHeader(const char *s, const char *startString, const char *endString)
{
  SKIP_SPACES(s)
  if (IsString1PrefixedByString2(s, startString))
  {
    s = strstr(s, endString);
    if (!s)
      return NULL;
    s += strlen(endString);
  }
  return s;
}

void CXmlItem::AppendTo(AString &s) const
{
  if (IsTag)
    s += '<';
  s += Name;
  if (IsTag)
  {
    FOR_VECTOR (i, Props)
    {
      const CXmlProp &prop = Props[i];
      s.Add_Space();
      s += prop.Name;
      s += '=';
      s += '\"';
      s += prop.Value;
      s += '\"';
    }
    s += '>';
  }
  FOR_VECTOR (i, SubItems)
  {
    const CXmlItem &item = SubItems[i];
    if (i != 0 && !SubItems[i - 1].IsTag)
      s.Add_Space();
    item.AppendTo(s);
  }
  if (IsTag)
  {
    s += '<';
    s += '/';
    s += Name;
    s += '>';
  }
}

bool CXml::Parse(const char *s)
{
  s = SkipHeader(s, "<?xml",    "?>"); if (!s) return false;
  s = SkipHeader(s, "<!DOCTYPE", ">"); if (!s) return false;

  s = Root.ParseItem(s, 1000);
  if (!s || !Root.IsTag)
    return false;
  SKIP_SPACES(s)
  return *s == 0;
}

/*
void CXml::AppendTo(AString &s) const
{
  Root.AppendTo(s);
}
*/


void z7_xml_DecodeString(AString &temp)
{
  char * const beg = temp.GetBuf();
  char *dest = beg;
  const char *p = beg;
  for (;;)
  {
    char c = *p++;
    if (c == 0)
      break;
    if (c == '&')
    {
      if (p[0] == '#')
      {
        const char *end;
        const UInt32 number = ConvertStringToUInt32(p + 1, &end);
        if (*end == ';' && number != 0 && number <= 127)
        {
          p = end + 1;
          c = (char)number;
        }
      }
      else if (
          p[0] == 'a' &&
          p[1] == 'm' &&
          p[2] == 'p' &&
          p[3] == ';')
      {
        p += 4;
      }
      else if (
          p[0] == 'l' &&
          p[1] == 't' &&
          p[2] == ';')
      {
        p += 3;
        c = '<';
      }
      else if (
          p[0] == 'g' &&
          p[1] == 't' &&
          p[2] == ';')
      {
        p += 3;
        c = '>';
      }
      else if (
          p[0] == 'a' &&
          p[1] == 'p' &&
          p[2] == 'o' &&
          p[3] == 's' &&
          p[4] == ';')
      {
        p += 5;
        c = '\'';
      }
      else if (
          p[0] == 'q' &&
          p[1] == 'u' &&
          p[2] == 'o' &&
          p[3] == 't' &&
          p[4] == ';')
      {
        p += 5;
        c = '\"';
      }
    }
    *dest++ = c;
  }
  temp.ReleaseBuf_SetEnd((unsigned)(dest - beg));
}

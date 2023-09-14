// MyXml.cpp

#include "StdAfx.h"

#include "MyXml.h"

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
  int index = FindProp(propName);
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
  int index = FindSubTag(tag);
  if (index >= 0)
    return SubItems[(unsigned)index].GetSubString();
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
  if (s != beg)
  {
    IsTag = false;
    Name.SetFrom(beg, (unsigned)(s - beg));
    return s;
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
  Name.SetFrom(beg, (unsigned)(s - beg));

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
      unsigned len = Name.Len();
      for (unsigned i = 0; i < len; i++)
        if (s[i] != Name[i])
          return NULL;
      s += len;
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
    prop.Name.SetFrom(beg, (unsigned)(s - beg));
    
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
    prop.Value.SetFrom(beg, (unsigned)(s - beg));
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

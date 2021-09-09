// TextPairs.cpp

#include "StdAfx.h"

#include "TextPairs.h"

static const wchar_t kNewLineChar = '\n';
static const wchar_t kQuoteChar = '\"';

static const wchar_t kBOM = (wchar_t)0xFEFF;

static bool IsSeparatorChar(wchar_t c)
{
  return (c == ' ' || c == '\t');
}

static void RemoveCr(UString &s)
{
  s.RemoveChar(L'\x0D');
}

static UString GetIDString(const wchar_t *srcString, unsigned &finishPos)
{
  UString result;
  bool quotes = false;
  for (finishPos = 0;;)
  {
    wchar_t c = srcString[finishPos];
    if (c == 0)
      break;
    finishPos++;
    bool isSeparatorChar = IsSeparatorChar(c);
    if (c == kNewLineChar || (isSeparatorChar && !quotes)
        || (c == kQuoteChar && quotes))
      break;
    else if (c == kQuoteChar)
      quotes = true;
    else
      result += c;
  }
  result.Trim();
  RemoveCr(result);
  return result;
}

static UString GetValueString(const wchar_t *srcString, unsigned &finishPos)
{
  UString result;
  for (finishPos = 0;;)
  {
    wchar_t c = srcString[finishPos];
    if (c == 0)
      break;
    finishPos++;
    if (c == kNewLineChar)
      break;
    result += c;
  }
  result.Trim();
  RemoveCr(result);
  return result;
}

static bool GetTextPairs(const UString &srcString, CObjectVector<CTextPair> &pairs)
{
  pairs.Clear();
  unsigned pos = 0;
  
  if (srcString.Len() > 0)
  {
    if (srcString[0] == kBOM)
      pos++;
  }
  while (pos < srcString.Len())
  {
    unsigned finishPos;
    UString id = GetIDString((const wchar_t *)srcString + pos, finishPos);
    pos += finishPos;
    if (id.IsEmpty())
      continue;
    UString value = GetValueString((const wchar_t *)srcString + pos, finishPos);
    pos += finishPos;
    if (!id.IsEmpty())
    {
      CTextPair pair;
      pair.ID = id;
      pair.Value = value;
      pairs.Add(pair);
    }
  }
  return true;
}

static int ComparePairIDs(const UString &s1, const UString &s2)
  { return MyStringCompareNoCase(s1, s2); }

static int ComparePairItems(const CTextPair &p1, const CTextPair &p2)
  { return ComparePairIDs(p1.ID, p2.ID); }

static int ComparePairItems(void *const *a1, void *const *a2, void * /* param */)
  { return ComparePairItems(**(const CTextPair *const *)a1, **(const CTextPair *const *)a2); }

void CPairsStorage::Sort() { Pairs.Sort(ComparePairItems, 0); }

int CPairsStorage::FindID(const UString &id, int &insertPos) const
{
  int left = 0, right = Pairs.Size();
  while (left != right)
  {
    int mid = (left + right) / 2;
    int compResult = ComparePairIDs(id, Pairs[mid].ID);
    if (compResult == 0)
      return mid;
    if (compResult < 0)
      right = mid;
    else
      left = mid + 1;
  }
  insertPos = left;
  return -1;
}

int CPairsStorage::FindID(const UString &id) const
{
  int pos;
  return FindID(id, pos);
}

void CPairsStorage::AddPair(const CTextPair &pair)
{
  int insertPos;
  int pos = FindID(pair.ID, insertPos);
  if (pos >= 0)
    Pairs[pos] = pair;
  else
    Pairs.Insert(insertPos, pair);
}

void CPairsStorage::DeletePair(const UString &id)
{
  int pos = FindID(id);
  if (pos >= 0)
    Pairs.Delete(pos);
}

bool CPairsStorage::GetValue(const UString &id, UString &value) const
{
  value.Empty();
  int pos = FindID(id);
  if (pos < 0)
    return false;
  value = Pairs[pos].Value;
  return true;
}

UString CPairsStorage::GetValue(const UString &id) const
{
  int pos = FindID(id);
  if (pos < 0)
    return UString();
  return Pairs[pos].Value;
}

bool CPairsStorage::ReadFromString(const UString &text)
{
  bool result = ::GetTextPairs(text, Pairs);
  if (result)
    Sort();
  else
    Pairs.Clear();
  return result;
}

void CPairsStorage::SaveToString(UString &text) const
{
  FOR_VECTOR (i, Pairs)
  {
    const CTextPair &pair = Pairs[i];
    bool multiWord = (pair.ID.Find(L' ') >= 0);
    if (multiWord)
      text += '\"';
    text += pair.ID;
    if (multiWord)
      text += '\"';
    text += ' ';
    text += pair.Value;
    text += '\x0D';
    text.Add_LF();
  }
}

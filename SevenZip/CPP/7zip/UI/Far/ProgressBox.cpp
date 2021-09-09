// ProgressBox.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#include "FarUtils.h"
#include "ProgressBox.h"

void CPercentPrinterState::ClearCurState()
{
  Completed = 0;
  Total = ((UInt64)(Int64)-1);
  Files = 0;
  FilesTotal = 0;
  Command.Empty();
  FileName.Empty();
}

void CProgressBox::Init(const char *title)
{
  _title = title;
  _wasPrinted = false;
  StartTick = GetTickCount();
  _prevTick = StartTick;
  _prevElapsedSec = 0;
}

static unsigned GetPower32(UInt32 val)
{
  const unsigned kStart = 32;
  UInt32 mask = ((UInt32)1 << (kStart - 1));
  for (unsigned i = kStart;; i--)
  {
    if (i == 0 || (val & mask) != 0)
      return i;
    mask >>= 1;
  }
}

static unsigned GetPower64(UInt64 val)
{
  UInt32 high = (UInt32)(val >> 32);
  if (high == 0)
    return GetPower32((UInt32)val);
  return GetPower32(high) + 32;
}

static UInt64 MyMultAndDiv(UInt64 mult1, UInt64 mult2, UInt64 divider)
{
  unsigned pow1 = GetPower64(mult1);
  unsigned pow2 = GetPower64(mult2);
  while (pow1 + pow2 > 64)
  {
    if (pow1 > pow2) { pow1--; mult1 >>= 1; }
    else             { pow2--; mult2 >>= 1; }
    divider >>= 1;
  }
  UInt64 res = mult1 * mult2;
  if (divider != 0)
    res /= divider;
  return res;
}

#define UINT_TO_STR_2(val) { s[0] = (char)('0' + (val) / 10); s[1] = (char)('0' + (val) % 10); s += 2; }

static void GetTimeString(UInt64 timeValue, char *s)
{
  UInt64 hours = timeValue / 3600;
  UInt32 seconds = (UInt32)(timeValue - hours * 3600);
  UInt32 minutes = seconds / 60;
  seconds %= 60;
  if (hours > 99)
  {
    ConvertUInt64ToString(hours, s);
    for (; *s != 0; s++);
  }
  else
  {
    UInt32 hours32 = (UInt32)hours;
    UINT_TO_STR_2(hours32);
  }
  *s++ = ':'; UINT_TO_STR_2(minutes);
  *s++ = ':'; UINT_TO_STR_2(seconds);
  *s = 0;
}

void CProgressBox::ReduceString(const UString &src, AString &dest)
{
  UnicodeStringToMultiByte2(dest, src, CP_OEMCP);

  if (dest.Len() <= MaxLen)
    return;
  unsigned len = FileName.Len();
  for (; len != 0;)
  {
    unsigned delta = len / 8;
    if (delta == 0)
      delta = 1;
    len -= delta;
    _tempU = FileName;
    _tempU.Delete(len / 2, FileName.Len() - len);
    _tempU.Insert(len / 2, L" . ");
    UnicodeStringToMultiByte2(dest, _tempU, CP_OEMCP);
    if (dest.Len() <= MaxLen)
      return;
  }
  dest.Empty();
}

static void Print_UInt64_and_String(AString &s, UInt64 val, const char *name)
{
  char temp[32];
  ConvertUInt64ToString(val, temp);
  s += temp;
  s.Add_Space();
  s += name;
}


static void PrintSize_bytes_Smart(AString &s, UInt64 val)
{
  // Print_UInt64_and_String(s, val, "bytes");
  {
    char temp[32];
    ConvertUInt64ToString(val, temp);
    s += temp;
  }

  if (val == 0)
    return;

  unsigned numBits = 10;
  char c = 'K';
  char temp[4] = { 'K', 'i', 'B', 0 };
       if (val >= ((UInt64)10 << 30)) { numBits = 30; c = 'G'; }
  else if (val >= ((UInt64)10 << 20)) { numBits = 20; c = 'M'; }
  temp[0] = c;
  s += " (";
  Print_UInt64_and_String(s, ((val + ((UInt64)1 << numBits) - 1) >> numBits), temp);
  s += ')';
}


static const unsigned kPercentsSize = 4;

void CProgressBox::Print()
{
  DWORD tick = GetTickCount();
  DWORD elapsedTicks = tick - StartTick;
  DWORD elapsedSec = elapsedTicks / 1000;

  if (_wasPrinted)
  {
    if (elapsedSec == _prevElapsedSec)
    {
      if ((UInt32)(tick - _prevTick) < _tickStep)
        return;
      if (_printedState.IsEqualTo((const CPercentPrinterState &)*this))
        return;
    }
  }

  UInt64 cur = Completed;
  UInt64 total = Total;

  if (!UseBytesForPercents)
  {
    cur = Files;
    total = FilesTotal;
  }

  {
    _timeStr.Empty();
    _timeStr = "Elapsed time: ";
    char s[40];
    GetTimeString(elapsedSec, s);
    _timeStr += s;

    if (cur != 0)
    {
      UInt64 remainingTime = 0;
      if (cur < total)
        remainingTime = MyMultAndDiv(elapsedTicks, total - cur, cur);
      UInt64 remainingSec = remainingTime / 1000;
      _timeStr += "    Remaining time: ";
      
      GetTimeString(remainingSec, s);
      _timeStr += s;
    }
  }


  {
    _perc.Empty();
    char s[32];
    unsigned size;
    {
      UInt64 val = 0;
      if (total != (UInt64)(Int64)-1 && total != 0)
        val = cur * 100 / Total;
      
      ConvertUInt64ToString(val, s);
      size = (unsigned)strlen(s);
      s[size++] = '%';
      s[size] = 0;
    }
    
    unsigned len = size;
    while (len < kPercentsSize)
      len = kPercentsSize;
    len++;
    
    if (len < MaxLen)
    {
      unsigned numChars = MaxLen - len;
      unsigned filled = 0;
      if (total != (UInt64)(Int64)-1 && total != 0)
        filled = (unsigned)(cur * numChars / total);
      if (filled > numChars)
        filled = numChars;
      unsigned i = 0;
      for (i = 0; i < filled; i++)
        _perc += (char)(Byte)0xDB; // '=';
      for (; i < numChars; i++)
        _perc += (char)(Byte)0xB0; // '.';
    }
    
    _perc.Add_Space();
    while (size < kPercentsSize)
    {
      _perc.Add_Space();
      size++;
    }
    _perc += s;
  }

  _files.Empty();
  if (Files != 0 || FilesTotal != 0)
  {
    _files += "Files: ";
    char s[32];
    // if (Files != 0)
    {
      ConvertUInt64ToString(Files, s);
      _files += s;
    }
    if (FilesTotal != 0)
    {
      _files += " / ";
      ConvertUInt64ToString(FilesTotal, s);
      _files += s;
    }
  }

  _sizesStr.Empty();
  if (Total != 0)
  {
    _sizesStr += "Size: ";
    PrintSize_bytes_Smart(_sizesStr, Completed);
    if (Total != 0 && Total != (UInt64)(Int64)-1)
    {
      _sizesStr += " / ";
      PrintSize_bytes_Smart(_sizesStr, Total);
    }
  }

  _name1.Empty();
  _name2.Empty();

  if (!FileName.IsEmpty())
  {
    _name1U.Empty();
    _name2U.Empty();

    /*
    if (_isDir)
      s1 = _filePath;
    else
    */
    {
      int slashPos = FileName.ReverseFind_PathSepar();
      if (slashPos >= 0)
      {
        _name1U.SetFrom(FileName, slashPos + 1);
        _name2U = FileName.Ptr(slashPos + 1);
      }
      else
        _name2U = FileName;
    }
    ReduceString(_name1U, _name1);
    ReduceString(_name2U, _name2);
  }
  
  {
    const char *strings[] = { _title, _timeStr, _files, _sizesStr, Command, _name1, _name2, _perc };
    NFar::g_StartupInfo.ShowMessage(FMSG_LEFTALIGN, NULL, strings, ARRAY_SIZE(strings), 0);
  }

  _wasPrinted = true;
  _printedState = *this;
  _prevTick = tick;
  _prevElapsedSec = elapsedSec;
}

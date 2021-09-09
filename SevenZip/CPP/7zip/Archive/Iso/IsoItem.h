// Archive/IsoItem.h

#ifndef __ARCHIVE_ISO_ITEM_H
#define __ARCHIVE_ISO_ITEM_H

#include "../../../../C/CpuArch.h"

#include "../../../Common/MyString.h"
#include "../../../Common/MyBuffer.h"

#include "../../../Windows/TimeUtils.h"

#include "IsoHeader.h"

namespace NArchive {
namespace NIso {

struct CRecordingDateTime
{
  Byte Year;
  Byte Month;
  Byte Day;
  Byte Hour;
  Byte Minute;
  Byte Second;
  signed char GmtOffset; // min intervals from -48 (West) to +52 (East) recorded.
  
  bool GetFileTime(FILETIME &ft) const
  {
    UInt64 value;
    bool res = NWindows::NTime::GetSecondsSince1601(Year + 1900, Month, Day, Hour, Minute, Second, value);
    if (res)
    {
      value -= (Int64)((Int32)GmtOffset * 15 * 60);
      value *= 10000000;
    }
    ft.dwLowDateTime = (DWORD)value;
    ft.dwHighDateTime = (DWORD)(value >> 32);
    return res;
  }
};

enum EPx
{
  k_Px_Mode,
  k_Px_Links,
  k_Px_User,
  k_Px_Group,
  k_Px_SerialNumber

  // k_Px_Num
};

/*
enum ETf
{
  k_Tf_CTime,
  k_Tf_MTime,
  k_Tf_ATime,
  k_Tf_Attrib,
  k_Tf_Backup,
  k_Tf_Expiration,
  k_Tf_Effective

  // k_Tf_Num
};
*/

struct CDirRecord
{
  UInt32 ExtentLocation;
  UInt32 Size;
  CRecordingDateTime DateTime;
  Byte FileFlags;
  Byte FileUnitSize;
  Byte InterleaveGapSize;
  Byte ExtendedAttributeRecordLen;
  UInt16 VolSequenceNumber;
  CByteBuffer FileId;
  CByteBuffer SystemUse;

  bool AreMultiPartEqualWith(const CDirRecord &a) const
  {
    return FileId == a.FileId
        && (FileFlags & (~NFileFlags::kNonFinalExtent)) ==
        (a.FileFlags & (~NFileFlags::kNonFinalExtent));
  }

  bool IsDir() const { return (FileFlags & NFileFlags::kDirectory) != 0; }
  bool IsNonFinalExtent() const { return (FileFlags & NFileFlags::kNonFinalExtent) != 0; }

  bool IsSystemItem() const
  {
    if (FileId.Size() != 1)
      return false;
    Byte b = *(const Byte *)FileId;
    return (b == 0 || b == 1);
  }

  
  const Byte* FindSuspRecord(unsigned skipSize, Byte id0, Byte id1, unsigned &lenRes) const
  {
    lenRes = 0;
    if (SystemUse.Size() < skipSize)
      return 0;
    const Byte *p = (const Byte *)SystemUse + skipSize;
    unsigned rem = (unsigned)(SystemUse.Size() - skipSize);
    while (rem >= 5)
    {
      unsigned len = p[2];
      if (len < 3 || len > rem)
        return 0;
      if (p[0] == id0 && p[1] == id1 && p[3] == 1)
      {
        if (len < 4)
          return 0; // Check it
        lenRes = len - 4;
        return p + 4;
      }
      p += len;
      rem -= len;
    }
    return 0;
  }

  
  const Byte* GetNameCur(bool checkSusp, int skipSize, unsigned &nameLenRes) const
  {
    const Byte *res = NULL;
    unsigned len = 0;
    if (checkSusp)
      res = FindSuspRecord(skipSize, 'N', 'M', len);
    if (!res || len < 1)
    {
      res = (const Byte *)FileId;
      len = (unsigned)FileId.Size();
    }
    else
    {
      res++;
      len--;
    }
    unsigned i;
    for (i = 0; i < len; i++)
      if (res[i] == 0)
        break;
    nameLenRes = i;
    return res;
  }


  bool GetSymLink(int skipSize, AString &link) const
  {
    link.Empty();
    const Byte *p = NULL;
    unsigned len = 0;
    p = FindSuspRecord(skipSize, 'S', 'L', len);
    if (!p || len < 1)
      return false;

    if (*p != 0)
      return false;

    p++;
    len--;

    while (len != 0)
    {
      if (len < 2)
        return false;
      unsigned flags = p[0];
      unsigned cl = p[1];
      p += 2;
      len -= 2;

      if (cl > len)
        return false;

      bool needSlash = false;
      
           if (flags & (1 << 1)) link += "./";
      else if (flags & (1 << 2)) link += "../";
      else if (flags & (1 << 3)) link += '/';
      else
        needSlash = true;

      for (unsigned i = 0; i < cl; i++)
      {
        char c = p[i];
        if (c == 0)
        {
          break;
          // return false;
        }
        link += c;
      }

      p += cl;
      len -= cl;

      if (len == 0)
        break;

      if (needSlash)
        link += '/';
    }

    return true;
  }

  static bool GetLe32Be32(const Byte *p, UInt32 &dest)
  {
    UInt32 v1 = GetUi32(p);
    UInt32 v2 = GetBe32(p + 4);
    if (v1 == v2)
    {
      dest = v1;
      return true;
    }
    return false;
  }


  bool GetPx(int skipSize, unsigned pxType, UInt32 &val) const
  {
    val = 0;
    const Byte *p = NULL;
    unsigned len = 0;
    p = FindSuspRecord(skipSize, 'P', 'X', len);
    if (!p)
      return false;
    // px.Clear();
    if (len < ((unsigned)pxType + 1) * 8)
      return false;

    return GetLe32Be32(p + pxType * 8, val);
  }

  /*
  bool GetTf(int skipSize, unsigned pxType, CRecordingDateTime &t) const
  {
    const Byte *p = NULL;
    unsigned len = 0;
    p = FindSuspRecord(skipSize, 'T', 'F', len);
    if (!p)
      return false;
    if (len < 1)
      return false;
    Byte flags = *p++;
    len--;

    unsigned step = 7;
    if (flags & 0x80)
    {
      step = 17;
      return false;
    }

    if ((flags & (1 << pxType)) == 0)
      return false;

    for (unsigned i = 0; i < pxType; i++)
    {
      if (len < step)
        return false;
      if (flags & (1 << i))
      {
        p += step;
        len -= step;
      }
    }

    if (len < step)
      return false;
    
    t.Year = p[0];
    t.Month = p[1];
    t.Day = p[2];
    t.Hour = p[3];
    t.Minute = p[4];
    t.Second = p[5];
    t.GmtOffset = (signed char)p[6];

    return true;
  }
  */

  bool CheckSusp(const Byte *p, unsigned &startPos) const
  {
    if (p[0] == 'S' &&
        p[1] == 'P' &&
        p[2] == 0x7 &&
        p[3] == 0x1 &&
        p[4] == 0xBE &&
        p[5] == 0xEF)
    {
      startPos = p[6];
      return true;
    }
    return false;
  }

  bool CheckSusp(unsigned &startPos) const
  {
    const Byte *p = (const Byte *)SystemUse;
    unsigned len = (int)SystemUse.Size();
    const unsigned kMinLen = 7;
    if (len < kMinLen)
      return false;
    if (CheckSusp(p, startPos))
      return true;
    const unsigned kOffset2 = 14;
    if (len < kOffset2 + kMinLen)
      return false;
    return CheckSusp(p + kOffset2, startPos);
  }
};

}}

#endif

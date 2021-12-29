// RarVol.h

#ifndef __ARCHIVE_RAR_VOL_H
#define __ARCHIVE_RAR_VOL_H

#include "../../../Common/StringConvert.h"

#include "RarHeader.h"

namespace NArchive {
namespace NRar {

inline bool IsDigit(wchar_t c)
{
  return c >= L'0' && c <= L'9';
}

class CVolumeName
{
  bool _needChangeForNext;
  UString _before;
  UString _changed;
  UString _after;
public:
  CVolumeName(): _needChangeForNext(true) {};

  bool InitName(const UString &name, bool newStyle = true)
  {
    _needChangeForNext = true;
    _after.Empty();
    UString base (name);
    const int dotPos = name.ReverseFind_Dot();

    if (dotPos >= 0)
    {
      const UString ext (name.Ptr(dotPos + 1));
      if (ext.IsEqualTo_Ascii_NoCase("rar"))
      {
        _after = name.Ptr(dotPos);
        base.DeleteFrom(dotPos);
      }
      else if (ext.IsEqualTo_Ascii_NoCase("exe"))
      {
        _after = ".rar";
        base.DeleteFrom(dotPos);
      }
      else if (!newStyle)
      {
        if (ext.IsEqualTo_Ascii_NoCase("000") ||
            ext.IsEqualTo_Ascii_NoCase("001") ||
            ext.IsEqualTo_Ascii_NoCase("r00") ||
            ext.IsEqualTo_Ascii_NoCase("r01"))
        {
          _changed = ext;
          _before = name.Left(dotPos + 1);
          return true;
        }
      }
    }

    if (newStyle)
    {
      unsigned i = base.Len();

      for (; i != 0; i--)
        if (!IsDigit(base[i - 1]))
          break;

      if (i != base.Len())
      {
        _before = base.Left(i);
        _changed = base.Ptr(i);
        return true;
      }
    }
    
    _after.Empty();
    _before = base;
    _before += '.';
    _changed = "r00";
    _needChangeForNext = false;
    return true;
  }

  /*
  void MakeBeforeFirstName()
  {
    unsigned len = _changed.Len();
    _changed.Empty();
    for (unsigned i = 0; i < len; i++)
      _changed += L'0';
  }
  */

  UString GetNextName()
  {
    if (_needChangeForNext)
    {
      unsigned i = _changed.Len();
      if (i == 0)
        return UString();
      for (;;)
      {
        wchar_t c = _changed[--i];
        if (c == L'9')
        {
          c = L'0';
          _changed.ReplaceOneCharAtPos(i, c);
          if (i == 0)
          {
            _changed.InsertAtFront(L'1');
            break;
          }
          continue;
        }
        c++;
        _changed.ReplaceOneCharAtPos(i, c);
        break;
      }
    }
    
    _needChangeForNext = true;
    return _before + _changed + _after;
  }
};

}}

#endif

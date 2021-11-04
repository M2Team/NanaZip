// Common/MyTypes.h

#ifndef __COMMON_MY_TYPES_H
#define __COMMON_MY_TYPES_H

#include "../../C/7zTypes.h"

typedef int HRes;

struct CBoolPair
{
  bool Val;
  bool Def;

  CBoolPair(): Val(false), Def(false) {}
  
  void Init()
  {
    Val = false;
    Def = false;
  }

  void SetTrueTrue()
  {
    Val = true;
    Def = true;
  }

  void SetVal_as_Defined(bool val)
  {
    Val = val;
    Def = true;
  }
};

#define CLASS_NO_COPY(cls) \
  private: \
  cls(const cls &); \
  cls &operator=(const cls &);

class CUncopyable
{
protected:
  CUncopyable() {} // allow constructor
  // ~CUncopyable() {}
CLASS_NO_COPY(CUncopyable)
};

#define MY_UNCOPYABLE  :private CUncopyable
// #define MY_UNCOPYABLE

#endif

// MyCom2.h

#ifndef __MYCOM2_H
#define __MYCOM2_H

#include "../../../Common/MyCom.h"

#define MY_ADDREF_RELEASE_MT \
STDMETHOD_(ULONG, AddRef)() { InterlockedIncrement((LONG *)&__m_RefCount); return __m_RefCount; } \
STDMETHOD_(ULONG, Release)() { InterlockedDecrement((LONG *)&__m_RefCount); if (__m_RefCount != 0) \
  return __m_RefCount; delete this; return 0; }

#define MY_UNKNOWN_IMP_SPEC_MT2(i1, i) \
  MY_QUERYINTERFACE_BEGIN \
  MY_QUERYINTERFACE_ENTRY_UNKNOWN(i1) \
  i \
  MY_QUERYINTERFACE_END \
  MY_ADDREF_RELEASE_MT


#define MY_UNKNOWN_IMP1_MT(i) MY_UNKNOWN_IMP_SPEC_MT2( \
  i, \
  MY_QUERYINTERFACE_ENTRY(i) \
  )

#define MY_UNKNOWN_IMP2_MT(i1, i2) MY_UNKNOWN_IMP_SPEC_MT2( \
  i1, \
  MY_QUERYINTERFACE_ENTRY(i1) \
  MY_QUERYINTERFACE_ENTRY(i2) \
  )

#define MY_UNKNOWN_IMP3_MT(i1, i2, i3) MY_UNKNOWN_IMP_SPEC_MT2( \
  i1, \
  MY_QUERYINTERFACE_ENTRY(i1) \
  MY_QUERYINTERFACE_ENTRY(i2) \
  MY_QUERYINTERFACE_ENTRY(i3) \
  )

#endif

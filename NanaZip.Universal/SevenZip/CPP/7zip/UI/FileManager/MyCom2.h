// MyCom2.h

#ifndef ZIP7_INC_MYCOM2_H
#define ZIP7_INC_MYCOM2_H

#include "../../../Common/MyCom.h"

#define Z7_COM_UNKNOWN_IMP_SPEC_MT2(i1, i) \
  Z7_COM_QI_BEGIN \
  Z7_COM_QI_ENTRY_UNKNOWN(i1) \
  i \
  Z7_COM_QI_END_MT \
  Z7_COM_ADDREF_RELEASE_MT


#define Z7_COM_UNKNOWN_IMP_1_MT(i) \
  Z7_COM_UNKNOWN_IMP_SPEC_MT2( \
  i, \
  Z7_COM_QI_ENTRY(i) \
  )

#define Z7_COM_UNKNOWN_IMP_2_MT(i1, i2) \
  Z7_COM_UNKNOWN_IMP_SPEC_MT2( \
  i1, \
  Z7_COM_QI_ENTRY(i1) \
  Z7_COM_QI_ENTRY(i2) \
  )

#define Z7_COM_UNKNOWN_IMP_3_MT(i1, i2, i3) \
  Z7_COM_UNKNOWN_IMP_SPEC_MT2( \
  i1, \
  Z7_COM_QI_ENTRY(i1) \
  Z7_COM_QI_ENTRY(i2) \
  Z7_COM_QI_ENTRY(i3) \
  )

#define Z7_COM_UNKNOWN_IMP_4_MT(i1, i2, i3, i4) \
  Z7_COM_UNKNOWN_IMP_SPEC_MT2( \
  i1, \
  Z7_COM_QI_ENTRY(i1) \
  Z7_COM_QI_ENTRY(i2) \
  Z7_COM_QI_ENTRY(i3) \
  Z7_COM_QI_ENTRY(i4) \
  )

#endif

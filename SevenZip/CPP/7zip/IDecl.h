// IDecl.h

#ifndef __IDECL_H
#define __IDECL_H

#include "../Common/MyUnknown.h"

#define k_7zip_GUID_Data1 0x23170F69
#define k_7zip_GUID_Data2 0x40C1

#define k_7zip_GUID_Data3_Common  0x278A

#define k_7zip_GUID_Data3_Decoder 0x2790
#define k_7zip_GUID_Data3_Encoder 0x2791
#define k_7zip_GUID_Data3_Hasher  0x2792


#define DECL_INTERFACE_SUB(i, base, groupId, subId) \
  DEFINE_GUID(IID_ ## i, \
    k_7zip_GUID_Data1, \
    k_7zip_GUID_Data2, \
    k_7zip_GUID_Data3_Common, \
    0, 0, 0, (groupId), 0, (subId), 0, 0); \
  struct i: public base

#define DECL_INTERFACE(i, groupId, subId) DECL_INTERFACE_SUB(i, IUnknown, groupId, subId)

#endif

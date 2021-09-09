// Windows/PropVariantConv.h

#ifndef __PROP_VARIANT_CONV_H
#define __PROP_VARIANT_CONV_H

#include "../Common/MyTypes.h"

// provide at least 32 bytes for buffer including zero-end

#define kTimestampPrintLevel_DAY -3
// #define kTimestampPrintLevel_HOUR -2
#define kTimestampPrintLevel_MIN -1
#define kTimestampPrintLevel_SEC 0
#define kTimestampPrintLevel_NTFS 7

bool ConvertUtcFileTimeToString(const FILETIME &ft, char *s, int level = kTimestampPrintLevel_SEC) throw();
bool ConvertUtcFileTimeToString(const FILETIME &ft, wchar_t *s, int level = kTimestampPrintLevel_SEC) throw();

// provide at least 32 bytes for buffer including zero-end
// don't send VT_BSTR to these functions
void ConvertPropVariantToShortString(const PROPVARIANT &prop, char *dest) throw();
void ConvertPropVariantToShortString(const PROPVARIANT &prop, wchar_t *dest) throw();

inline bool ConvertPropVariantToUInt64(const PROPVARIANT &prop, UInt64 &value)
{
  switch (prop.vt)
  {
    case VT_UI8: value = (UInt64)prop.uhVal.QuadPart; return true;
    case VT_UI4: value = prop.ulVal; return true;
    case VT_UI2: value = prop.uiVal; return true;
    case VT_UI1: value = prop.bVal; return true;
    case VT_EMPTY: return false;
    default: throw 151199;
  }
}

#endif

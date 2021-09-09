// FormatUtils.h

#ifndef __FORMAT_UTILS_H
#define __FORMAT_UTILS_H

#include "../../../Common/MyTypes.h"
#include "../../../Common/MyString.h"

UString NumberToString(UInt64 number);

UString MyFormatNew(const UString &format, const UString &argument);
UString MyFormatNew(UINT resourceID, const UString &argument);

#endif

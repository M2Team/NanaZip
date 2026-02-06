// FormatUtils.h

#ifndef __FORMAT_UTILS_H
#define __FORMAT_UTILS_H

#include "../../../Common/MyTypes.h"
#include "../../../Common/MyString.h"

UString NumberToString(UInt64 number);
// **************** NanaZip Modification Start ****************
void ConvertFileSizeToSmartString(
    _In_ UInt64 value,
    _Out_ wchar_t *s);
// **************** NanaZip Modification End ****************

UString MyFormatNew(const UString &format, const UString &argument);
UString MyFormatNew(UINT resourceID, const UString &argument);

#endif

// FormatUtils.cpp

#include "StdAfx.h"
// **************** NanaZip Modification Start **************** 
#include <stdio.h>
// **************** NanaZip Modification End ****************

#include "../../../Common/IntToString.h"

#include "FormatUtils.h"

#include "LangUtils.h"

UString NumberToString(UInt64 number)
{
  wchar_t numberString[32];
  ConvertUInt64ToString(number, numberString);
  return numberString;
}
// **************** NanaZip Modification Start ****************
void ConvertFileSizeToSmartString(
    _In_ UInt64 value,
    _Out_ wchar_t *s)
{
    const wchar_t *units[] = { L"B", L"KB", L"MB", L"GB", L"TB", L"PB", L"EB" };
    int unitIndex = 0;
    double size = (double)value;

    while (size >= 1024.0 && unitIndex < 6)
    {
        size /= 1024.0;
        unitIndex++;
    }

    if (unitIndex == 0)
    {
        ::ConvertUInt64ToString(value, s);
        ::wcscat(s, L" B");
    }
    else
    {
        wchar_t temp[64];
        ::swprintf_s(temp, L"%.2f %s", size, units[unitIndex]);
        ::wcscpy(s, temp);
    }
}
// **************** NanaZip Modification End ****************

UString MyFormatNew(const UString &format, const UString &argument)
{
  UString result = format;
  result.Replace(L"{0}", argument);
  return result;
}

UString MyFormatNew(UINT resourceID, const UString &argument)
{
  return MyFormatNew(LangString(resourceID), argument);
}

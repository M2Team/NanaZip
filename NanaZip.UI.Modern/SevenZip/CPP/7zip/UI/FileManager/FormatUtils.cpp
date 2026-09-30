// FormatUtils.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"

#include "FormatUtils.h"

#include "LangUtils.h"

// **************** NanaZip Modification Start ****************

#include <stdio.h>

// **************** NanaZip Modification End ****************

UString NumberToString(UInt64 number)
{
  wchar_t numberString[32];
  ConvertUInt64ToString(number, numberString);
  return numberString;
}

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

// **************** NanaZip Modification Start ****************

void ConvertByteSizeToString(
    UInt64 ByteSize,
    _Out_writes_z_(TextBufferSize) wchar_t* TextBuffer,
    size_t TextBufferSize)
{
  if (TextBufferSize == 0)
  {
    return;
  }

  wchar_t const* const Units[] =
  {
    L"Byte",
    L"Bytes",
    L"KiB",
    L"MiB",
    L"GiB",
    L"TiB",
    L"PiB",
    L"EiB"
  };
  size_t const UnitsCount = ARRAY_SIZE(Units);

  // Output Format:
  // For ByteSize is 0 or 1: x Byte
  // For ByteSize is from 2 to 1023: x Bytes
  // For ByteSize is larger than 1023: x.xx {KiB, MiB, GiB, TiB, PiB, EiB}

  size_t UnitIndex = 0;
  double Result = static_cast<double>(ByteSize);

  if (ByteSize > 1)
  {
    for (UnitIndex = 1; UnitIndex < UnitsCount; ++UnitIndex)
    {
      if (Result < 1024.0)
      {
        break;
      }

      Result /= 1024.0;
    }

    // Keep two digits after the decimal point.
    Result = static_cast<UInt64>(Result * 100) / 100.0;
  }

  ::_snwprintf_s(
      TextBuffer,
      TextBufferSize,
      _TRUNCATE,
      (UnitIndex > 1) ? L"%.2f %s" : L"%.0f %s",
      Result,
      Units[UnitIndex]);
}

// **************** NanaZip Modification End ****************

// Windows/COM.cpp

#include "StdAfx.h"

/*

#include "COM.h"
#include "../Common/StringConvert.h"

namespace NWindows {
namespace NCOM {

// CoInitialize (NULL); must be called!

UString GUIDToStringW(REFGUID guid)
{
  UString s;
  const unsigned kSize = 48;
  StringFromGUID2(guid, s.GetBuf(kSize), kSize);
  s.ReleaseBuf_CalcLen(kSize);
  return s;
}

AString GUIDToStringA(REFGUID guid)
{
  return UnicodeStringToMultiByte(GUIDToStringW(guid));
}

HRESULT StringToGUIDW(const wchar_t *string, GUID &classID)
{
  return CLSIDFromString((wchar_t *)string, &classID);
}

HRESULT StringToGUIDA(const char *string, GUID &classID)
{
  return StringToGUIDW(MultiByteToUnicodeString(string), classID);
}

}}

*/

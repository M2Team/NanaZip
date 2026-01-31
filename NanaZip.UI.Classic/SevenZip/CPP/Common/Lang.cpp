// Common/Lang.cpp

#include "StdAfx.h"

#include "Lang.h"
#include "StringToInt.h"
#include "UTFConvert.h"

#include "../Windows/FileIO.h"

void CLang::Clear() throw()
{

}

bool CLang::OpenFromString(const AString &s2)
{
    UNREFERENCED_PARAMETER(s2);
    return true;
}

bool CLang::Open(CFSTR fileName, const char *id)
{
    UNREFERENCED_PARAMETER(fileName);
    UNREFERENCED_PARAMETER(id);
    return true;
}

const wchar_t *CLang::Get(UInt32 id) const throw()
{
    UNREFERENCED_PARAMETER(id);
    return nullptr;
}

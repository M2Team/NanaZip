// HandlerOut.cpp

#include "StdAfx.h"

#include "../../../Common/StringToInt.h"

#include "../Common/ParseProperties.h"

#include "HandlerOut.h"

namespace NArchive {

bool ParseSizeString(const wchar_t *s, const PROPVARIANT &prop, UInt64 percentsBase, UInt64 &res)
{
  if (*s == 0)
  {
    switch (prop.vt)
    {
      case VT_UI4: res = prop.ulVal; return true;
      case VT_UI8: res = prop.uhVal.QuadPart; return true;
      case VT_BSTR:
        s = prop.bstrVal;
        break;
      default: return false;
    }
  }
  else if (prop.vt != VT_EMPTY)
    return false;

  const wchar_t *end;
  UInt64 v = ConvertStringToUInt64(s, &end);
  if (s == end)
    return false;
  wchar_t c = *end;
  if (c == 0)
  {
    res = v;
    return true;
  }
  if (end[1] != 0)
    return false;

  if (c == '%')
  {
    res = percentsBase / 100 * v;
    return true;
  }

  unsigned numBits;
  switch (MyCharLower_Ascii(c))
  {
    case 'b': numBits =  0; break;
    case 'k': numBits = 10; break;
    case 'm': numBits = 20; break;
    case 'g': numBits = 30; break;
    case 't': numBits = 40; break;
    default: return false;
  }
  UInt64 val2 = v << numBits;
  if ((val2 >> numBits) != v)
    return false;
  res = val2;
  return true;
}

bool CCommonMethodProps::SetCommonProperty(const UString &name, const PROPVARIANT &value, HRESULT &hres)
{
  hres = S_OK;

  if (name.IsPrefixedBy_Ascii_NoCase("mt"))
  {
    #ifndef _7ZIP_ST
    hres = ParseMtProp(name.Ptr(2), value, _numProcessors, _numThreads);
    #endif
    return true;
  }
  
  if (name.IsPrefixedBy_Ascii_NoCase("memuse"))
  {
    if (!ParseSizeString(name.Ptr(6), value, _memAvail, _memUsage))
      hres = E_INVALIDARG;
    return true;
  }

  return false;
}


#ifndef EXTRACT_ONLY

static void SetMethodProp32(COneMethodInfo &m, PROPID propID, UInt32 value)
{
  if (m.FindProp(propID) < 0)
    m.AddProp32(propID, value);
}

void CMultiMethodProps::SetGlobalLevelTo(COneMethodInfo &oneMethodInfo) const
{
  UInt32 level = _level;
  if (level != (UInt32)(Int32)-1)
    SetMethodProp32(oneMethodInfo, NCoderPropID::kLevel, (UInt32)level);
}

#ifndef _7ZIP_ST
void CMultiMethodProps::SetMethodThreadsTo(COneMethodInfo &oneMethodInfo, UInt32 numThreads)
{
  SetMethodProp32(oneMethodInfo, NCoderPropID::kNumThreads, numThreads);
}
#endif

void CMultiMethodProps::InitMulti()
{
  _level = (UInt32)(Int32)-1;
  _analysisLevel = -1;
  _crcSize = 4;
  _autoFilter = true;
}

void CMultiMethodProps::Init()
{
  InitCommon();
  InitMulti();
  _methods.Clear();
  _filterMethod.Clear();
}


HRESULT CMultiMethodProps::SetProperty(const wchar_t *nameSpec, const PROPVARIANT &value)
{
  UString name = nameSpec;
  name.MakeLower_Ascii();
  if (name.IsEmpty())
    return E_INVALIDARG;
  
  if (name[0] == 'x')
  {
    name.Delete(0);
    _level = 9;
    return ParsePropToUInt32(name, value, _level);
  }

  if (name.IsPrefixedBy_Ascii_NoCase("yx"))
  {
    name.Delete(0, 2);
    UInt32 v = 9;
    RINOK(ParsePropToUInt32(name, value, v));
    _analysisLevel = (int)v;
    return S_OK;
  }
  
  if (name.IsPrefixedBy_Ascii_NoCase("crc"))
  {
    name.Delete(0, 3);
    _crcSize = 4;
    return ParsePropToUInt32(name, value, _crcSize);
  }

  {
    HRESULT hres;
    if (SetCommonProperty(name, value, hres))
      return hres;
  }
  
  UInt32 number;
  unsigned index = ParseStringToUInt32(name, number);
  UString realName = name.Ptr(index);
  if (index == 0)
  {
    if (name.IsEqualTo("f"))
    {
      HRESULT res = PROPVARIANT_to_bool(value, _autoFilter);
      if (res == S_OK)
        return res;
      if (value.vt != VT_BSTR)
        return E_INVALIDARG;
      return _filterMethod.ParseMethodFromPROPVARIANT(UString(), value);
    }
    number = 0;
  }
  if (number > 64)
    return E_FAIL;
  for (unsigned j = _methods.Size(); j <= number; j++)
    _methods.AddNew();
  return _methods[number].ParseMethodFromPROPVARIANT(realName, value);
}



void CSingleMethodProps::Init()
{
  InitCommon();
  InitSingle();
  Clear();
}


HRESULT CSingleMethodProps::SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps)
{
  Init();
  
  for (UInt32 i = 0; i < numProps; i++)
  {
    UString name = names[i];
    name.MakeLower_Ascii();
    if (name.IsEmpty())
      return E_INVALIDARG;
    const PROPVARIANT &value = values[i];
    if (name[0] == L'x')
    {
      UInt32 a = 9;
      RINOK(ParsePropToUInt32(name.Ptr(1), value, a));
      _level = a;
      AddProp_Level(a);
      continue;
    }
    {
      HRESULT hres;
      if (SetCommonProperty(name, value, hres))
      {
        RINOK(hres)
        continue;
      }
    }
    RINOK(ParseMethodFromPROPVARIANT(names[i], value));
  }

  return S_OK;
}

#endif

}

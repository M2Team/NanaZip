// HandlerOut.h

#ifndef __HANDLER_OUT_H
#define __HANDLER_OUT_H

#include "../../../Windows/System.h"

#include "../../Common/MethodProps.h"

namespace NArchive {

bool ParseSizeString(const wchar_t *name, const PROPVARIANT &prop, UInt64 percentsBase, UInt64 &res);

class CCommonMethodProps
{
protected:
  void InitCommon()
  {
    #ifndef _7ZIP_ST
    _numProcessors = _numThreads = NWindows::NSystem::GetNumberOfProcessors();
    #endif

    UInt64 memAvail = (UInt64)(sizeof(size_t)) << 28;
    _memAvail = memAvail;
    _memUsage = memAvail;
    if (NWindows::NSystem::GetRamSize(memAvail))
    {
      _memAvail = memAvail;
      _memUsage = memAvail / 32 * 17;
    }
  }

public:
  #ifndef _7ZIP_ST
  UInt32 _numThreads;
  UInt32 _numProcessors;
  #endif

  UInt64 _memUsage;
  UInt64 _memAvail;

  bool SetCommonProperty(const UString &name, const PROPVARIANT &value, HRESULT &hres);

  CCommonMethodProps() { InitCommon(); }
};


#ifndef EXTRACT_ONLY

class CMultiMethodProps: public CCommonMethodProps
{
  UInt32 _level;
  int _analysisLevel;

  void InitMulti();
public:
  UInt32 _crcSize;
  CObjectVector<COneMethodInfo> _methods;
  COneMethodInfo _filterMethod;
  bool _autoFilter;

  
  void SetGlobalLevelTo(COneMethodInfo &oneMethodInfo) const;

  #ifndef _7ZIP_ST
  static void SetMethodThreadsTo(COneMethodInfo &oneMethodInfo, UInt32 numThreads);
  #endif


  unsigned GetNumEmptyMethods() const
  {
    unsigned i;
    for (i = 0; i < _methods.Size(); i++)
      if (!_methods[i].IsEmpty())
        break;
    return i;
  }

  int GetLevel() const { return _level == (UInt32)(Int32)-1 ? 5 : (int)_level; }
  int GetAnalysisLevel() const { return _analysisLevel; }

  void Init();
  CMultiMethodProps() { InitMulti(); }

  HRESULT SetProperty(const wchar_t *name, const PROPVARIANT &value);
};


class CSingleMethodProps: public COneMethodInfo, public CCommonMethodProps
{
  UInt32 _level;

  void InitSingle()
  {
    _level = (UInt32)(Int32)-1;
  }

public:
  void Init();
  CSingleMethodProps() { InitSingle(); }
  
  int GetLevel() const { return _level == (UInt32)(Int32)-1 ? 5 : (int)_level; }
  HRESULT SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps);
};

#endif

}

#endif

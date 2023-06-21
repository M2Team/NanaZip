// ProgressMt.h

#ifndef __PROGRESSMT_H
#define __PROGRESSMT_H

#include "../../../../ThirdParty/LZMA/CPP/Common/MyCom.h"
#include "../../../../ThirdParty/LZMA/CPP/Common/MyVector.h"
#include "../../../../ThirdParty/LZMA/CPP/Windows/Synchronization.h"

#include "../../../../ThirdParty/LZMA/CPP/7zip/ICoder.h"
#include "../../../../ThirdParty/LZMA/CPP/7zip/IProgress.h"

class CMtCompressProgressMixer
{
  CMyComPtr<ICompressProgressInfo> _progress;
  CRecordVector<UInt64> InSizes;
  CRecordVector<UInt64> OutSizes;
  UInt64 TotalInSize;
  UInt64 TotalOutSize;
public:
  NWindows::NSynchronization::CCriticalSection CriticalSection;
  void Init(unsigned numItems, ICompressProgressInfo *progress);
  void Reinit(unsigned index);
  HRESULT SetRatioInfo(unsigned index, const UInt64 *inSize, const UInt64 *outSize);
};

class CMtCompressProgress:
  public ICompressProgressInfo,
  public CMyUnknownImp
{
  CMtCompressProgressMixer *_progress;
  unsigned _index;
public:
  void Init(CMtCompressProgressMixer *progress, unsigned index)
  {
    _progress = progress;
    _index = index;
  }
  void Reinit() { _progress->Reinit(_index); }

  MY_UNKNOWN_IMP

  STDMETHOD(SetRatioInfo)(const UInt64 *inSize, const UInt64 *outSize);
};

#endif

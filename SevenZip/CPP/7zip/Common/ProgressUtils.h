// ProgressUtils.h

#ifndef __PROGRESS_UTILS_H
#define __PROGRESS_UTILS_H

#include "../../Common/MyCom.h"

#include "../ICoder.h"
#include "../IProgress.h"

class CLocalProgress:
  public ICompressProgressInfo,
  public CMyUnknownImp
{
  CMyComPtr<IProgress> _progress;
  CMyComPtr<ICompressProgressInfo> _ratioProgress;
  bool _inSizeIsMain;
public:
  UInt64 ProgressOffset;
  UInt64 InSize;
  UInt64 OutSize;
  bool SendRatio;
  bool SendProgress;

  CLocalProgress();

  void Init(IProgress *progress, bool inSizeIsMain);
  HRESULT SetCur();

  MY_UNKNOWN_IMP1(ICompressProgressInfo)

  STDMETHOD(SetRatioInfo)(const UInt64 *inSize, const UInt64 *outSize);
};

#endif

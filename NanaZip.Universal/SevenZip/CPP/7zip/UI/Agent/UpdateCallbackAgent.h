// UpdateCallbackAgent.h

#ifndef ZIP7_INC_UPDATE_CALLBACK_AGENT_H
#define ZIP7_INC_UPDATE_CALLBACK_AGENT_H

#include "../Common/UpdateCallback.h"

#include "IFolderArchive.h"

class CUpdateCallbackAgent Z7_final: public IUpdateCallbackUI
{
  Z7_IFACE_IMP(IUpdateCallbackUI)

  CMyComPtr<ICryptoGetTextPassword2> _cryptoGetTextPassword;
  CMyComPtr<IFolderArchiveUpdateCallback> Callback;
  CMyComPtr<IFolderArchiveUpdateCallback2> Callback2;
  CMyComPtr<ICompressProgressInfo> _compressProgress;
public:
  void SetCallback(IFolderArchiveUpdateCallback *callback);
};

#endif

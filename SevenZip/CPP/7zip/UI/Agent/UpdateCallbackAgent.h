// UpdateCallbackAgent.h

#ifndef __UPDATE_CALLBACK_AGENT_H
#define __UPDATE_CALLBACK_AGENT_H

#include "../Common/UpdateCallback.h"

#include "IFolderArchive.h"

class CUpdateCallbackAgent: public IUpdateCallbackUI
{
  INTERFACE_IUpdateCallbackUI(;)
  
  CMyComPtr<ICryptoGetTextPassword2> _cryptoGetTextPassword;
  CMyComPtr<IFolderArchiveUpdateCallback> Callback;
  CMyComPtr<IFolderArchiveUpdateCallback2> Callback2;
  CMyComPtr<ICompressProgressInfo> _compressProgress;
public:
  void SetCallback(IFolderArchiveUpdateCallback *callback);
};

#endif

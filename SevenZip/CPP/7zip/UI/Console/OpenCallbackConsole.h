// OpenCallbackConsole.h

#ifndef __OPEN_CALLBACK_CONSOLE_H
#define __OPEN_CALLBACK_CONSOLE_H

#include "../../../Common/StdOutStream.h"

#include "../Common/ArchiveOpenCallback.h"

#include "PercentPrinter.h"

class COpenCallbackConsole: public IOpenCallbackUI
{
protected:
  CPercentPrinter _percent;

  CStdOutStream *_so;
  CStdOutStream *_se;

  bool _totalFilesDefined;
  // bool _totalBytesDefined;
  // UInt64 _totalFiles;
  UInt64 _totalBytes;

  bool NeedPercents() const { return _percent._so != NULL; }

public:

  bool MultiArcMode;

  void ClosePercents()
  {
    if (NeedPercents())
      _percent.ClosePrint(true);
  }

  COpenCallbackConsole():
      _totalFilesDefined(false),
      // _totalBytesDefined(false),
      _totalBytes(0),
      MultiArcMode(false)
      
      #ifndef _NO_CRYPTO
      , PasswordIsDefined(false)
      // , PasswordWasAsked(false)
      #endif
      
      {}

  virtual ~COpenCallbackConsole() {}
  
  void Init(CStdOutStream *outStream, CStdOutStream *errorStream, CStdOutStream *percentStream)
  {
    _so = outStream;
    _se = errorStream;
    _percent._so = percentStream;
  }

  INTERFACE_IOpenCallbackUI(;)
  
  #ifndef _NO_CRYPTO
  bool PasswordIsDefined;
  // bool PasswordWasAsked;
  UString Password;
  #endif
};

#endif

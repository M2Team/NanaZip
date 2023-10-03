﻿// ExtractCallbackConsole.h

#ifndef ZIP7_INC_EXTRACT_CALLBACK_CONSOLE_H
#define ZIP7_INC_EXTRACT_CALLBACK_CONSOLE_H

#include "../../../Common/StdOutStream.h"

#include "../../IPassword.h"

#include "../../Archive/IArchive.h"

#include "../Common/ArchiveExtractCallback.h"

#include "PercentPrinter.h"

#include "OpenCallbackConsole.h"

/*
struct CErrorPathCodes2
{
  FStringVector Paths;
  CRecordVector<DWORD> Codes;

  void AddError(const FString &path, DWORD systemError)
  {
    Paths.Add(path);
    Codes.Add(systemError);
  }
  void Clear()
  {
    Paths.Clear();
    Codes.Clear();
  }
};
*/

class CExtractScanConsole Z7_final: public IDirItemsCallback
{
  Z7_IFACE_IMP(IDirItemsCallback)

  CStdOutStream *_so;
  CStdOutStream *_se;
  CPercentPrinter _percent;

  // CErrorPathCodes2 ScanErrors;

  bool NeedPercents() const { return _percent._so != NULL; }
  
  void ClosePercentsAndFlush()
  {
    if (NeedPercents())
      _percent.ClosePrint(true);
    if (_so)
      _so->Flush();
  }

public:

  void Init(CStdOutStream *outStream, CStdOutStream *errorStream, CStdOutStream *percentStream)
  {
    _so = outStream;
    _se = errorStream;
    _percent._so = percentStream;
  }
  
  void SetWindowWidth(unsigned width) { _percent.MaxLen = width - 1; }

  void StartScanning();
  
  void CloseScanning()
  {
    if (NeedPercents())
      _percent.ClosePrint(true);
  }

  void PrintStat(const CDirItemsStat &st);
};




class CExtractCallbackConsole Z7_final:
  public IFolderArchiveExtractCallback,
  public IExtractCallbackUI,
  // public IArchiveExtractCallbackMessage,
  public IFolderArchiveExtractCallback2,
 #ifndef Z7_NO_CRYPTO
  public ICryptoGetTextPassword,
 #endif
  public COpenCallbackConsole,
  public CMyUnknownImp
{
  Z7_COM_QI_BEGIN2(IFolderArchiveExtractCallback)
  // Z7_COM_QI_ENTRY(IArchiveExtractCallbackMessage)
  Z7_COM_QI_ENTRY(IFolderArchiveExtractCallback2)
 #ifndef Z7_NO_CRYPTO
  Z7_COM_QI_ENTRY(ICryptoGetTextPassword)
 #endif
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

  Z7_IFACE_COM7_IMP(IProgress)
  Z7_IFACE_COM7_IMP(IFolderArchiveExtractCallback)
  Z7_IFACE_IMP(IExtractCallbackUI)
  // Z7_IFACE_COM7_IMP(IArchiveExtractCallbackMessage)
  Z7_IFACE_COM7_IMP(IFolderArchiveExtractCallback2)
 #ifndef Z7_NO_CRYPTO
  Z7_IFACE_COM7_IMP(ICryptoGetTextPassword)
 #endif
  

  AString _tempA;
  UString _tempU;

  UString _currentName;

  void ClosePercents_for_so()
  {
    if (NeedPercents() && _so == _percent._so)
      _percent.ClosePrint(false);
  }
  
  void ClosePercentsAndFlush()
  {
    if (NeedPercents())
      _percent.ClosePrint(true);
    if (_so)
      _so->Flush();
  }
public:
  UInt64 NumTryArcs;
  
  bool ThereIsError_in_Current;
  bool ThereIsWarning_in_Current;

  UInt64 NumOkArcs;
  UInt64 NumCantOpenArcs;
  UInt64 NumArcsWithError;
  UInt64 NumArcsWithWarnings;

  UInt64 NumOpenArcErrors;
  UInt64 NumOpenArcWarnings;
  
  UInt64 NumFileErrors;
  UInt64 NumFileErrors_in_Current;

  bool NeedFlush;
  unsigned PercentsNameLevel;
  unsigned LogLevel;

  CExtractCallbackConsole():
      NeedFlush(false),
      PercentsNameLevel(1),
      LogLevel(0)
      {}

  void SetWindowWidth(unsigned width) { _percent.MaxLen = width - 1; }

  void Init(CStdOutStream *outStream, CStdOutStream *errorStream, CStdOutStream *percentStream)
  {
    COpenCallbackConsole::Init(outStream, errorStream, percentStream);

    NumTryArcs = 0;
    
    ThereIsError_in_Current = false;
    ThereIsWarning_in_Current = false;

    NumOkArcs = 0;
    NumCantOpenArcs = 0;
    NumArcsWithError = 0;
    NumArcsWithWarnings = 0;

    NumOpenArcErrors = 0;
    NumOpenArcWarnings = 0;
    
    NumFileErrors = 0;
    NumFileErrors_in_Current = 0;
  }
};

#endif

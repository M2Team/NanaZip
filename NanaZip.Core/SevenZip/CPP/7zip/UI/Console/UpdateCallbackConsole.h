// UpdateCallbackConsole.h

#ifndef ZIP7_INC_UPDATE_CALLBACK_CONSOLE_H
#define ZIP7_INC_UPDATE_CALLBACK_CONSOLE_H

#include "../../../Common/StdOutStream.h"

#include "../Common/Update.h"

#include "PercentPrinter.h"

struct CErrorPathCodes
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


class CCallbackConsoleBase
{
protected:
  CPercentPrinter _percent;

  CStdOutStream *_so;
  CStdOutStream *_se;

  void CommonError(const FString &path, DWORD systemError, bool isWarning);
  // void CommonError(const char *message);

  HRESULT ScanError_Base(const FString &path, DWORD systemError);
  HRESULT OpenFileError_Base(const FString &name, DWORD systemError);
  HRESULT ReadingFileError_Base(const FString &name, DWORD systemError);

public:
  bool NeedPercents() const { return _percent._so != NULL; }

  bool StdOutMode;

  bool NeedFlush;
  unsigned PercentsNameLevel;
  unsigned LogLevel;

  AString _tempA;
  UString _tempU;

  CCallbackConsoleBase():
      StdOutMode(false),
      NeedFlush(false),
      PercentsNameLevel(1),
      LogLevel(0),
      NumNonOpenFiles(0)
      {}
  
  void SetWindowWidth(unsigned width) { _percent.MaxLen = width - 1; }

  void Init(
      CStdOutStream *outStream,
      CStdOutStream *errorStream,
      CStdOutStream *percentStream,
      bool disablePercents)
  {
    FailedFiles.Clear();

    _so = outStream;
    _se = errorStream;
    _percent._so = percentStream;
    _percent.DisablePrint = disablePercents;
  }

  void ClosePercents2()
  {
    if (NeedPercents())
      _percent.ClosePrint(true);
  }

  void ClosePercents_for_so()
  {
    if (NeedPercents() && _so == _percent._so)
      _percent.ClosePrint(false);
  }

  CErrorPathCodes FailedFiles;
  CErrorPathCodes ScanErrors;
  UInt64 NumNonOpenFiles;

  HRESULT PrintProgress(const wchar_t *name, bool isDir, const char *command, bool showInLog);

  // void PrintInfoLine(const UString &s);
  // void PrintPropInfo(UString &s, PROPID propID, const PROPVARIANT *value);
};


class CUpdateCallbackConsole Z7_final:
  public IUpdateCallbackUI2,
  public CCallbackConsoleBase
{
  // void PrintPropPair(const char *name, const wchar_t *val);
  Z7_IFACE_IMP(IUpdateCallbackUI)
  Z7_IFACE_IMP(IDirItemsCallback)
  Z7_IFACE_IMP(IUpdateCallbackUI2)
public:
  bool DeleteMessageWasShown;

  #ifndef Z7_NO_CRYPTO
  bool PasswordIsDefined;
  bool AskPassword;
  UString Password;
  #endif

  CUpdateCallbackConsole():
      DeleteMessageWasShown(false)
      #ifndef Z7_NO_CRYPTO
      , PasswordIsDefined(false)
      , AskPassword(false)
      #endif
      {}

  /*
  void Init(CStdOutStream *outStream)
  {
    CCallbackConsoleBase::Init(outStream);
  }
  */
  // ~CUpdateCallbackConsole() { if (NeedPercents()) _percent.ClosePrint(); }
};

#endif

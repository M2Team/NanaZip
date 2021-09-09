// HashCalc.h

#ifndef __HASH_CALC_H
#define __HASH_CALC_H

#include "../../../Common/Wildcard.h"

#include "../../Common/CreateCoder.h"
#include "../../Common/MethodProps.h"

#include "DirItem.h"

const unsigned k_HashCalc_DigestSize_Max = 64;

const unsigned k_HashCalc_NumGroups = 4;

enum
{
  k_HashCalc_Index_Current,
  k_HashCalc_Index_DataSum,
  k_HashCalc_Index_NamesSum,
  k_HashCalc_Index_StreamsSum
};

struct CHasherState
{
  CMyComPtr<IHasher> Hasher;
  AString Name;
  UInt32 DigestSize;
  Byte Digests[k_HashCalc_NumGroups][k_HashCalc_DigestSize_Max];
};

struct IHashCalc
{
  virtual void InitForNewFile() = 0;
  virtual void Update(const void *data, UInt32 size) = 0;
  virtual void SetSize(UInt64 size) = 0;
  virtual void Final(bool isDir, bool isAltStream, const UString &path) = 0;
};

struct CHashBundle: public IHashCalc
{
  CObjectVector<CHasherState> Hashers;

  UInt64 NumDirs;
  UInt64 NumFiles;
  UInt64 NumAltStreams;
  UInt64 FilesSize;
  UInt64 AltStreamsSize;
  UInt64 NumErrors;

  UInt64 CurSize;

  UString MainName;
  UString FirstFileName;

  HRESULT SetMethods(DECL_EXTERNAL_CODECS_LOC_VARS const UStringVector &methods);
  
  // void Init() {}
  CHashBundle()
  {
    NumDirs = NumFiles = NumAltStreams = FilesSize = AltStreamsSize = NumErrors = 0;
  }

  virtual ~CHashBundle() {};

  void InitForNewFile();
  void Update(const void *data, UInt32 size);
  void SetSize(UInt64 size);
  void Final(bool isDir, bool isAltStream, const UString &path);
};

#define INTERFACE_IHashCallbackUI(x) \
  INTERFACE_IDirItemsCallback(x) \
  virtual HRESULT StartScanning() x; \
  virtual HRESULT FinishScanning(const CDirItemsStat &st) x; \
  virtual HRESULT SetNumFiles(UInt64 numFiles) x; \
  virtual HRESULT SetTotal(UInt64 size) x; \
  virtual HRESULT SetCompleted(const UInt64 *completeValue) x; \
  virtual HRESULT CheckBreak() x; \
  virtual HRESULT BeforeFirstFile(const CHashBundle &hb) x; \
  virtual HRESULT GetStream(const wchar_t *name, bool isFolder) x; \
  virtual HRESULT OpenFileError(const FString &path, DWORD systemError) x; \
  virtual HRESULT SetOperationResult(UInt64 fileSize, const CHashBundle &hb, bool showHash) x; \
  virtual HRESULT AfterLastFile(CHashBundle &hb) x; \

struct IHashCallbackUI: public IDirItemsCallback
{
  INTERFACE_IHashCallbackUI(=0)
};

struct CHashOptions
{
  UStringVector Methods;
  bool PreserveATime;
  bool OpenShareForWrite;
  bool StdInMode;
  bool AltStreamsMode;
  CBoolPair SymLinks;

  NWildcard::ECensorPathMode PathMode;
 
  CHashOptions():
      PreserveATime(false),
      OpenShareForWrite(false),
      StdInMode(false),
      AltStreamsMode(false),
      PathMode(NWildcard::k_RelatPath) {};
};

HRESULT HashCalc(
    DECL_EXTERNAL_CODECS_LOC_VARS
    const NWildcard::CCensor &censor,
    const CHashOptions &options,
    AString &errorInfo,
    IHashCallbackUI *callback);

void AddHashHexToString(char *dest, const Byte *data, UInt32 size);

#endif

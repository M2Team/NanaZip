// ExtractCallback.h

#ifndef __EXTRACT_CALLBACK_H
#define __EXTRACT_CALLBACK_H

#include "../../../../C/Alloc.h"

#include "../../../Common/MyCom.h"
#include "../../../Common/StringConvert.h"

#ifndef _SFX
#include "../Agent/IFolderArchive.h"
#endif

#include "../Common/ArchiveExtractCallback.h"
#include "../Common/ArchiveOpenCallback.h"

#ifndef _NO_CRYPTO
#include "../../IPassword.h"
#endif

#ifndef _SFX
#include "IFolder.h"
#endif

#include "ProgressDialog2.h"

#ifdef LANG
#include "LangUtils.h"
#endif

#ifndef _SFX

class CGrowBuf
{
  Byte *_items;
  size_t _size;

  CLASS_NO_COPY(CGrowBuf);

public:
  bool ReAlloc_KeepData(size_t newSize, size_t keepSize)
  {
    void *buf = MyAlloc(newSize);
    if (!buf)
      return false;
    if (keepSize != 0)
      memcpy(buf, _items, keepSize);
    MyFree(_items);
    _items = (Byte *)buf;
    _size = newSize;
    return true;
  }

  CGrowBuf(): _items(0), _size(0) {}
  ~CGrowBuf() { MyFree(_items); }

  operator Byte *() { return _items; }
  operator const Byte *() const { return _items; }
  size_t Size() const { return _size; }
};

struct CVirtFile
{
  CGrowBuf Data;
  
  UInt64 Size; // real size
  UInt64 ExpectedSize; // the size from props request. 0 if unknown

  UString Name;

  bool CTimeDefined;
  bool ATimeDefined;
  bool MTimeDefined;
  bool AttribDefined;
  
  bool IsDir;
  bool IsAltStream;
  
  DWORD Attrib;

  FILETIME CTime;
  FILETIME ATime;
  FILETIME MTime;

  CVirtFile():
    CTimeDefined(false),
    ATimeDefined(false),
    MTimeDefined(false),
    AttribDefined(false),
    IsDir(false),
    IsAltStream(false) {}
};

class CVirtFileSystem:
  public ISequentialOutStream,
  public CMyUnknownImp
{
  UInt64 _totalAllocSize;

  size_t _pos;
  unsigned _numFlushed;
  bool _fileIsOpen;
  bool _fileMode;
  COutFileStream *_outFileStreamSpec;
  CMyComPtr<ISequentialOutStream> _outFileStream;
public:
  CObjectVector<CVirtFile> Files;
  UInt64 MaxTotalAllocSize;
  FString DirPrefix;
 
  CVirtFile &AddNewFile()
  {
    if (!Files.IsEmpty())
    {
      MaxTotalAllocSize -= Files.Back().Data.Size();
    }
    return Files.AddNew();
  }
  HRESULT CloseMemFile()
  {
    if (_fileMode)
    {
      return FlushToDisk(true);
    }
    CVirtFile &file = Files.Back();
    if (file.Data.Size() != file.Size)
    {
      file.Data.ReAlloc_KeepData((size_t)file.Size, (size_t)file.Size);
    }
    return S_OK;
  }

  bool IsStreamInMem() const
  {
    if (_fileMode)
      return false;
    if (Files.Size() < 1 || /* Files[0].IsAltStream || */ Files[0].IsDir)
      return false;
    return true;
  }

  size_t GetMemStreamWrittenSize() const { return _pos; }

  CVirtFileSystem(): _outFileStreamSpec(NULL), MaxTotalAllocSize((UInt64)0 - 1) {}

  void Init()
  {
    _totalAllocSize = 0;
    _fileMode = false;
    _pos = 0;
    _numFlushed = 0;
    _fileIsOpen = false;
  }

  HRESULT CloseFile(const FString &path);
  HRESULT FlushToDisk(bool closeLast);
  size_t GetPos() const { return _pos; }

  MY_UNKNOWN_IMP
  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
};

#endif
  
class CExtractCallbackImp:
  public IExtractCallbackUI, // it includes IFolderArchiveExtractCallback
  public IOpenCallbackUI,
  public IFolderArchiveExtractCallback2,
  #ifndef _SFX
  public IFolderOperationsExtractCallback,
  public IFolderExtractToStreamCallback,
  public ICompressProgressInfo,
  #endif
  #ifndef _NO_CRYPTO
  public ICryptoGetTextPassword,
  #endif
  public CMyUnknownImp
{
  HRESULT MessageError(const char *message, const FString &path);
  void Add_ArchiveName_Error();
public:
  MY_QUERYINTERFACE_BEGIN2(IFolderArchiveExtractCallback)
  MY_QUERYINTERFACE_ENTRY(IFolderArchiveExtractCallback2)
  #ifndef _SFX
  MY_QUERYINTERFACE_ENTRY(IFolderOperationsExtractCallback)
  MY_QUERYINTERFACE_ENTRY(IFolderExtractToStreamCallback)
  MY_QUERYINTERFACE_ENTRY(ICompressProgressInfo)
  #endif
  #ifndef _NO_CRYPTO
  MY_QUERYINTERFACE_ENTRY(ICryptoGetTextPassword)
  #endif
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE

  INTERFACE_IProgress(;)
  INTERFACE_IOpenCallbackUI(;)
  INTERFACE_IFolderArchiveExtractCallback(;)
  INTERFACE_IFolderArchiveExtractCallback2(;)
  // STDMETHOD(SetTotalFiles)(UInt64 total);
  // STDMETHOD(SetCompletedFiles)(const UInt64 *value);

  INTERFACE_IExtractCallbackUI(;)

  #ifndef _SFX
  // IFolderOperationsExtractCallback
  STDMETHOD(AskWrite)(
      const wchar_t *srcPath,
      Int32 srcIsFolder,
      const FILETIME *srcTime,
      const UInt64 *srcSize,
      const wchar_t *destPathRequest,
      BSTR *destPathResult,
      Int32 *writeAnswer);
  STDMETHOD(ShowMessage)(const wchar_t *message);
  STDMETHOD(SetCurrentFilePath)(const wchar_t *filePath);
  STDMETHOD(SetNumFiles)(UInt64 numFiles);
  INTERFACE_IFolderExtractToStreamCallback(;)
  STDMETHOD(SetRatioInfo)(const UInt64 *inSize, const UInt64 *outSize);
  #endif

  // ICryptoGetTextPassword
  #ifndef _NO_CRYPTO
  STDMETHOD(CryptoGetTextPassword)(BSTR *password);
  #endif

private:
  UString _currentArchivePath;
  bool _needWriteArchivePath;

  UString _currentFilePath;
  bool _isFolder;

  bool _isAltStream;
  UInt64 _curSize;
  bool _curSizeDefined;
  UString _filePath;
  // bool _extractMode;
  // bool _testMode;
  bool _newVirtFileWasAdded;
  bool _needUpdateStat;


  HRESULT SetCurrentFilePath2(const wchar_t *filePath);
  void AddError_Message(LPCWSTR message);

  #ifndef _SFX
  bool _hashStreamWasUsed;
  COutStreamWithHash *_hashStreamSpec;
  CMyComPtr<ISequentialOutStream> _hashStream;
  IHashCalc *_hashCalc; // it's for stat in Test operation
  #endif

public:

  #ifndef _SFX
  CVirtFileSystem *VirtFileSystemSpec;
  CMyComPtr<ISequentialOutStream> VirtFileSystem;
  #endif

  bool ProcessAltStreams;

  bool StreamMode;

  CProgressDialog *ProgressDialog;
  #ifndef _SFX
  UInt64 NumFolders;
  UInt64 NumFiles;
  bool NeedAddFile;
  #endif
  UInt32 NumArchiveErrors;
  bool ThereAreMessageErrors;
  NExtract::NOverwriteMode::EEnum OverwriteMode;

  #ifndef _NO_CRYPTO
  bool PasswordIsDefined;
  bool PasswordWasAsked;
  UString Password;
  #endif


  UString _lang_Extracting;
  UString _lang_Testing;
  UString _lang_Skipping;
  UString _lang_Reading;
  UString _lang_Empty;

  bool _totalFilesDefined;
  bool _totalBytesDefined;
  bool MultiArcMode;

  CExtractCallbackImp():
    #ifndef _SFX
    _hashCalc(NULL),
    #endif
    ProcessAltStreams(true),
    StreamMode(false),
    OverwriteMode(NExtract::NOverwriteMode::kAsk),
    #ifndef _NO_CRYPTO
    PasswordIsDefined(false),
    PasswordWasAsked(false),
    #endif
    _totalFilesDefined(false),
    _totalBytesDefined(false),
    MultiArcMode(false)
    {}
   
  ~CExtractCallbackImp();
  void Init();

  #ifndef _SFX
  void SetHashCalc(IHashCalc *hashCalc) { _hashCalc = hashCalc; }

  void SetHashMethods(IHashCalc *hash)
  {
    if (!hash)
      return;
    _hashStreamSpec = new COutStreamWithHash;
    _hashStream = _hashStreamSpec;
    _hashStreamSpec->_hash = hash;
  }
  #endif

  bool IsOK() const { return NumArchiveErrors == 0 && !ThereAreMessageErrors; }
};

#endif

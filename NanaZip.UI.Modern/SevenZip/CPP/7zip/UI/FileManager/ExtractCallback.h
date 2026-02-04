// ExtractCallback.h

#ifndef __EXTRACT_CALLBACK_H
#define __EXTRACT_CALLBACK_H

#include "../../../../C/Alloc.h"

#include "../../../Common/MyCom.h"
#include "../../../Common/StringConvert.h"

#ifndef _SFX
#include "../Agent/IFolderArchive.h"
#endif

// **************** NanaZip Modification Start ****************
#include "../Common/Extract.h"
// **************** NanaZip Modification End ****************
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

// **************** NanaZip Modification Start ****************
// Backported from 24.09.
class CGrowBuf
{
  Byte *_items;
  size_t _size;

  CLASS_NO_COPY(CGrowBuf);

public:
  void Free()
  {
    MyFree(_items);
    _items = NULL;
    _size = 0;
  }

  // newSize >= keepSize
  bool ReAlloc_KeepData(size_t newSize, size_t keepSize)
  {
    void *buf = NULL;
    if (newSize)
    {
      buf = MyAlloc(newSize);
      if (!buf)
        return false;
    }
    if (keepSize)
      memcpy(buf, _items, keepSize);
    MyFree(_items);
    _items = (Byte *)buf;
    _size = newSize;
    return true;
  }

  CGrowBuf(): _items(NULL), _size(0) {}
  ~CGrowBuf() { MyFree(_items); }

  operator Byte *() { return _items; }
  operator const Byte *() const { return _items; }
  size_t Size() const { return _size; }
};
// **************** NanaZip Modification End ****************

// **************** NanaZip Modification Start ****************
// Backported from 24.09.
struct CVirtFile
{
  CGrowBuf Data;

  UInt64 ExpectedSize; // size from props request. 0 if unknown
  size_t WrittenSize;  // size of written data in (Data) buffer
                       //   use (WrittenSize) only if (CVirtFileSystem::_newVirtFileStream_IsReadyToWrite == false)
  UString BaseName;    // original name of file inside archive,
                       // It's not path. So any path separators
                       // should be treated as part of name (or as incorrect chars)
  UString AltStreamName;

  bool CTime_Defined;
  bool ATime_Defined;
  bool MTime_Defined;
  bool Attrib_Defined;

  // bool IsDir;
  bool IsAltStream;
  bool ColonWasUsed;
  DWORD Attrib;

  FILETIME CTime;
  FILETIME ATime;
  FILETIME MTime;

  CVirtFile():
    CTime_Defined(false),
    ATime_Defined(false),
    MTime_Defined(false),
    Attrib_Defined(false),
    // IsDir(false),
    IsAltStream(false),
    ColonWasUsed(false)
    {}
};
// **************** NanaZip Modification End ****************


// **************** NanaZip Modification Start ****************
// Backported from 24.09.
/*
  We use CVirtFileSystem only for single file extraction:
  It supports the following cases and names:
     - "fileName" : single file
     - "fileName" item (main base file) and additional "fileName:altStream" items
     - "altStream" : single item without "fileName:" prefix.
  If file is flushed to disk, it uses Get_Correct_FsFile_Name(name).
*/

class CVirtFileSystem:
  public ISequentialOutStream,
  public CMyUnknownImp
{
  unsigned _numFlushed;
public:
  bool IsAltStreamFile; // in:
      // = true,  if extracting file is alt stream without "fileName:" prefix.
      // = false, if extracting file is normal file, but additional
      //          alt streams "fileName:altStream" items are possible.
private:
  bool _newVirtFileStream_IsReadyToWrite;    // it can non real file (if can't open alt stream)
  bool _needWriteToRealFile;  // we need real writing to open file.
  bool _wasSwitchedToFsMode;
  bool _altStream_NeedRestore_Attrib_bool;
  DWORD _altStream_NeedRestore_AttribVal;

  CMyComPtr2<ISequentialOutStream, COutFileStream> _outFileStream;
public:
  CObjectVector<CVirtFile> Files;
  size_t MaxTotalAllocSize; // remain size, including Files.Back()
  FString DirPrefix; // files will be flushed to this FS directory.
  UString FileName; // name of file that will be extracted.
                    // it can be name of alt stream without "fileName:" prefix, if (IsAltStreamFile == trye).
                    // we use that name to detect altStream part in "FileName:altStream".
  CByteBuffer ZoneBuf;
  int Index_of_MainExtractedFile_in_Files; // out: index in Files. == -1, if expected file was not extracted
  int Index_of_ZoneBuf_AltStream_in_Files; // out: index in Files. == -1, if no zonbuf alt stream


  CVirtFileSystem()
  {
    _numFlushed = 0;
    IsAltStreamFile = false;
    _newVirtFileStream_IsReadyToWrite = false;
    _needWriteToRealFile = false;
    _wasSwitchedToFsMode = false;
    _altStream_NeedRestore_Attrib_bool = false;
    MaxTotalAllocSize = (size_t)0 - 1;
    Index_of_MainExtractedFile_in_Files = -1;
    Index_of_ZoneBuf_AltStream_in_Files = -1;
  }

  bool WasStreamFlushedToFS() const { return _wasSwitchedToFsMode; }

  HRESULT CloseMemFile()
  {
    if (_wasSwitchedToFsMode)
      return FlushToDisk(true); // closeLast
    CVirtFile &file = Files.Back();
    if (file.Data.Size() != file.WrittenSize)
      file.Data.ReAlloc_KeepData(file.WrittenSize, file.WrittenSize);
    return S_OK;
  }

  HRESULT FlushToDisk(bool closeLast);

  // NanaZip: retained definitions.
  MY_UNKNOWN_IMP
  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
};
// **************** NanaZip Modification End ****************

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
  // **************** NanaZip Modification Start ****************
  CDecompressStat Stat;
  // **************** NanaZip Modification End ****************

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

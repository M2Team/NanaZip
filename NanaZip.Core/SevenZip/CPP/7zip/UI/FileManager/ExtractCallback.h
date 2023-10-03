// ExtractCallback.h

#ifndef ZIP7_INC_EXTRACT_CALLBACK_H
#define ZIP7_INC_EXTRACT_CALLBACK_H

#include "../../../../C/Alloc.h"

#include "../../../Common/MyCom.h"
#include "../../../Common/StringConvert.h"

#ifndef Z7_SFX
#include "../Agent/IFolderArchive.h"
#endif

#include "../Common/ArchiveExtractCallback.h"
#include "../Common/ArchiveOpenCallback.h"

#ifndef Z7_NO_CRYPTO
#include "../../IPassword.h"
#endif

#ifndef Z7_SFX
#include "IFolder.h"
#endif

#include "ProgressDialog2.h"

#ifdef Z7_LANG
// #include "LangUtils.h"
#endif

#ifndef Z7_SFX

class CGrowBuf
{
  Byte *_items;
  size_t _size;

  Z7_CLASS_NO_COPY(CGrowBuf)

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

  CGrowBuf(): _items(NULL), _size(0) {}
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


Z7_CLASS_IMP_NOQIB_1(
  CVirtFileSystem,
  ISequentialOutStream
)
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
};

#endif
  
class CExtractCallbackImp Z7_final:
  public IFolderArchiveExtractCallback,
  /* IExtractCallbackUI:
       before v23.00 : it         included IFolderArchiveExtractCallback
       since  v23.00 : it doesn't include  IFolderArchiveExtractCallback
  */
  public IExtractCallbackUI, // NON-COM interface since 23.00
  public IOpenCallbackUI,    // NON-COM interface
  public IFolderArchiveExtractCallback2,
 #ifndef Z7_SFX
  public IFolderOperationsExtractCallback,
  public IFolderExtractToStreamCallback,
  public ICompressProgressInfo,
 #endif
 #ifndef Z7_NO_CRYPTO
  public ICryptoGetTextPassword,
 #endif
  public CMyUnknownImp
{
  Z7_COM_QI_BEGIN2(IFolderArchiveExtractCallback)
  Z7_COM_QI_ENTRY(IFolderArchiveExtractCallback2)
 #ifndef Z7_SFX
  Z7_COM_QI_ENTRY(IFolderOperationsExtractCallback)
  Z7_COM_QI_ENTRY(IFolderExtractToStreamCallback)
  Z7_COM_QI_ENTRY(ICompressProgressInfo)
 #endif
 #ifndef Z7_NO_CRYPTO
  Z7_COM_QI_ENTRY(ICryptoGetTextPassword)
 #endif
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

  Z7_IFACE_IMP(IExtractCallbackUI)
  Z7_IFACE_IMP(IOpenCallbackUI)
  Z7_IFACE_COM7_IMP(IProgress)
  Z7_IFACE_COM7_IMP(IFolderArchiveExtractCallback)
  Z7_IFACE_COM7_IMP(IFolderArchiveExtractCallback2)
 #ifndef Z7_SFX
  Z7_IFACE_COM7_IMP(IFolderOperationsExtractCallback)
  Z7_IFACE_COM7_IMP(IFolderExtractToStreamCallback)
  Z7_IFACE_COM7_IMP(ICompressProgressInfo)
 #endif
 #ifndef Z7_NO_CRYPTO
  Z7_IFACE_COM7_IMP(ICryptoGetTextPassword)
 #endif


  UString _currentArchivePath;
  bool _needWriteArchivePath;

  bool _isFolder;
  UString _currentFilePath;
  UString _filePath;

 #ifndef Z7_SFX
  bool _needUpdateStat;
  bool _newVirtFileWasAdded;
  bool _isAltStream;
  bool _curSize_Defined;
  UInt64 _curSize;
  // bool _extractMode;
  // bool _testMode;
  bool _hashStreamWasUsed;
  COutStreamWithHash *_hashStreamSpec;
  CMyComPtr<ISequentialOutStream> _hashStream;
  IHashCalc *_hashCalc; // it's for stat in Test operation
 #endif

  HRESULT SetCurrentFilePath2(const wchar_t *filePath);
  void AddError_Message(LPCWSTR message);
  HRESULT MessageError(const char *message, const FString &path);
  void Add_ArchiveName_Error();

public:

  #ifndef Z7_SFX
  CVirtFileSystem *VirtFileSystemSpec;
  CMyComPtr<ISequentialOutStream> VirtFileSystem;
  #endif

  bool ProcessAltStreams;

  bool StreamMode;

  CProgressDialog *ProgressDialog;
  #ifndef Z7_SFX
  UInt64 NumFolders;
  UInt64 NumFiles;
  bool NeedAddFile;
  #endif
  UInt32 NumArchiveErrors;
  bool ThereAreMessageErrors;
  NExtract::NOverwriteMode::EEnum OverwriteMode;

  #ifndef Z7_NO_CRYPTO
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
    #ifndef Z7_SFX
    _hashCalc(NULL),
    #endif
    ProcessAltStreams(true),
    StreamMode(false),
    OverwriteMode(NExtract::NOverwriteMode::kAsk),
    #ifndef Z7_NO_CRYPTO
    PasswordIsDefined(false),
    PasswordWasAsked(false),
    #endif
    _totalFilesDefined(false),
    _totalBytesDefined(false),
    MultiArcMode(false)
    {}
   
  ~CExtractCallbackImp();
  void Init();

  #ifndef Z7_SFX
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

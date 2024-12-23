// ArchiveExtractCallback.h

#ifndef ZIP7_INC_ARCHIVE_EXTRACT_CALLBACK_H
#define ZIP7_INC_ARCHIVE_EXTRACT_CALLBACK_H

#include "../../../Common/MyCom.h"
#include "../../../Common/MyLinux.h"
#include "../../../Common/Wildcard.h"

#include "../../IPassword.h"

#include "../../Common/FileStreams.h"
#include "../../Common/ProgressUtils.h"
#include "../../Common/StreamObjects.h"

#include "../../Archive/IArchive.h"

#include "ExtractMode.h"
#include "IFileExtractCallback.h"
#include "OpenArchive.h"

#include "HashCalc.h"

#ifndef Z7_SFX

Z7_CLASS_IMP_NOQIB_1(
  COutStreamWithHash
  , ISequentialOutStream
)
  bool _calculate;
  CMyComPtr<ISequentialOutStream> _stream;
  UInt64 _size;
public:
  IHashCalc *_hash;

  void SetStream(ISequentialOutStream *stream) { _stream = stream; }
  void ReleaseStream() { _stream.Release(); }
  void Init(bool calculate = true)
  {
    InitCRC();
    _size = 0;
    _calculate = calculate;
  }
  void EnableCalc(bool calculate) { _calculate = calculate; }
  void InitCRC() { _hash->InitForNewFile(); }
  UInt64 GetSize() const { return _size; }
};

#endif

struct CExtractNtOptions
{
  CBoolPair NtSecurity;
  CBoolPair SymLinks;
  CBoolPair SymLinks_AllowDangerous;
  CBoolPair HardLinks;
  CBoolPair AltStreams;
  bool ReplaceColonForAltStream;
  bool WriteToAltStreamIfColon;

  bool ExtractOwner;

  bool PreAllocateOutFile;

  // used for hash arcs only, when we open external files
  bool PreserveATime;
  bool OpenShareForWrite;

  UInt64 MemLimit;

  CExtractNtOptions():
      ReplaceColonForAltStream(false),
      WriteToAltStreamIfColon(false),
      ExtractOwner(false),
      PreserveATime(false),
      OpenShareForWrite(false),
      MemLimit((UInt64)(Int64)-1)
  {
    SymLinks.Val = true;
    SymLinks_AllowDangerous.Val = false;
    HardLinks.Val = true;
    AltStreams.Val = true;

    PreAllocateOutFile =
      #ifdef _WIN32
        true;
      #else
        false;
      #endif
  }
};


#ifndef Z7_SFX
#ifndef UNDER_CE
#define SUPPORT_LINKS
#endif
#endif


#ifdef SUPPORT_LINKS

struct CHardLinkNode
{
  UInt64 StreamId;
  UInt64 INode;

  int Compare(const CHardLinkNode &a) const;
};

class CHardLinks
{
public:
  CRecordVector<CHardLinkNode> IDs;
  CObjectVector<FString> Links;

  void Clear()
  {
    IDs.Clear();
    Links.Clear();
  }

  void PrepareLinks()
  {
    while (Links.Size() < IDs.Size())
      Links.AddNew();
  }
};

#endif

#ifdef SUPPORT_ALT_STREAMS

struct CIndexToPathPair
{
  UInt32 Index;
  FString Path;

  CIndexToPathPair(UInt32 index): Index(index) {}
  CIndexToPathPair(UInt32 index, const FString &path): Index(index), Path(path) {}

  int Compare(const CIndexToPathPair &pair) const
  {
    return MyCompare(Index, pair.Index);
  }
};

#endif



struct CFiTimesCAM
{
  CFiTime CTime;
  CFiTime ATime;
  CFiTime MTime;

  bool CTime_Defined;
  bool ATime_Defined;
  bool MTime_Defined;

  bool IsSomeTimeDefined() const
  {
    return
      CTime_Defined |
      ATime_Defined |
      MTime_Defined;
  }
};

struct CDirPathTime: public CFiTimesCAM
{
  FString Path;
  
  bool SetDirTime() const;
};


#ifdef SUPPORT_LINKS

struct CLinkInfo
{
  // bool isCopyLink;
  bool isHardLink;
  bool isJunction;
  bool isRelative;
  bool isWSL;
  UString linkPath;

  bool IsSymLink() const { return !isHardLink; }

  CLinkInfo():
    // IsCopyLink(false),
    isHardLink(false),
    isJunction(false),
    isRelative(false),
    isWSL(false)
    {}

  void Clear()
  {
    // IsCopyLink = false;
    isHardLink = false;
    isJunction = false;
    isRelative = false;
    isWSL = false;
    linkPath.Empty();
  }

  bool Parse(const Byte *data, size_t dataSize, bool isLinuxData);
};

#endif // SUPPORT_LINKS


#ifndef _WIN32

struct COwnerInfo
{
  bool Id_Defined;
  UInt32 Id;
  AString Name;

  void Clear()
  {
    Id_Defined = false;
    Id = 0;
    Name.Empty();
  }
};

#endif


class CArchiveExtractCallback Z7_final:
  public IArchiveExtractCallback,
  public IArchiveExtractCallbackMessage2,
  public ICryptoGetTextPassword,
  public ICompressProgressInfo,
#ifndef Z7_SFX
  public IArchiveUpdateCallbackFile,
  public IArchiveGetDiskProperty,
  public IArchiveRequestMemoryUseCallback,
#endif
  public CMyUnknownImp
{
  /* IArchiveExtractCallback, */
  Z7_COM_QI_BEGIN2(IArchiveExtractCallbackMessage2)
  Z7_COM_QI_ENTRY(ICryptoGetTextPassword)
  Z7_COM_QI_ENTRY(ICompressProgressInfo)
#ifndef Z7_SFX
  Z7_COM_QI_ENTRY(IArchiveUpdateCallbackFile)
  Z7_COM_QI_ENTRY(IArchiveGetDiskProperty)
  Z7_COM_QI_ENTRY(IArchiveRequestMemoryUseCallback)
#endif
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

  Z7_IFACE_COM7_IMP(IProgress)
  Z7_IFACE_COM7_IMP(IArchiveExtractCallback)
  Z7_IFACE_COM7_IMP(IArchiveExtractCallbackMessage2)
  Z7_IFACE_COM7_IMP(ICryptoGetTextPassword)
  Z7_IFACE_COM7_IMP(ICompressProgressInfo)
#ifndef Z7_SFX
  Z7_IFACE_COM7_IMP(IArchiveUpdateCallbackFile)
  Z7_IFACE_COM7_IMP(IArchiveGetDiskProperty)
  Z7_IFACE_COM7_IMP(IArchiveRequestMemoryUseCallback)
#endif

  // bool Write_CTime;
  // bool Write_ATime;
  // bool Write_MTime;
  bool _stdOutMode;
  bool _testMode;
  bool _removePartsForAltStreams;
public:
  bool Is_elimPrefix_Mode;
private:

  const CArc *_arc;
  CExtractNtOptions _ntOptions;

  bool _encrypted;
  bool _isSplit;
  bool _curSize_Defined;
  bool _fileLength_WasSet;

  bool _isRenamed;
  bool _extractMode;
  // bool _is_SymLink_in_Data;
  bool _is_SymLink_in_Data_Linux; // false = WIN32, true = LINUX
  bool _needSetAttrib;
  bool _isSymLinkCreated;
  bool _itemFailure;
  bool _some_pathParts_wereRemoved;

  bool _multiArchives;
  bool _keepAndReplaceEmptyDirPrefixes; // replace them to "_";
#if defined(_WIN32) && !defined(UNDER_CE) && !defined(Z7_SFX)
  bool _saclEnabled;
#endif

  NExtract::NPathMode::EEnum _pathMode;
  NExtract::NOverwriteMode::EEnum _overwriteMode;

  CMyComPtr<IFolderArchiveExtractCallback> _extractCallback2;
  const NWildcard::CCensorNode *_wildcardCensor; // we need wildcard for single pass mode (stdin)
  // CMyComPtr<ICompressProgressInfo> _compressProgress;
  // CMyComPtr<IArchiveExtractCallbackMessage2> _callbackMessage;
  CMyComPtr<IFolderArchiveExtractCallback2> _folderArchiveExtractCallback2;
  CMyComPtr<ICryptoGetTextPassword> _cryptoGetTextPassword;

  FString _dirPathPrefix;
  FString _dirPathPrefix_Full;

  #ifndef Z7_SFX

  CMyComPtr<IFolderExtractToStreamCallback> ExtractToStreamCallback;
  CMyComPtr<IArchiveRequestMemoryUseCallback> _requestMemoryUseCallback;
  
  #endif

  CReadArcItem _item;
  FString _diskFilePath;

  struct CProcessedFileInfo
  {
    CArcTime CTime;
    CArcTime ATime;
    CArcTime MTime;
    UInt32 Attrib;
    bool Attrib_Defined;

   #ifndef _WIN32
    COwnerInfo Owner;
    COwnerInfo Group;
   #endif

    bool IsReparse() const
    {
      return (Attrib_Defined && (Attrib & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
    }
    
    bool IsLinuxSymLink() const
    {
      return (Attrib_Defined && MY_LIN_S_ISLNK(Attrib >> 16));
    }

    void SetFromPosixAttrib(UInt32 a)
    {
      // here we set only part of combined attribute required by SetFileAttrib() call
      #ifdef _WIN32
      // Windows sets FILE_ATTRIBUTE_NORMAL, if we try to set 0 as attribute.
      Attrib = MY_LIN_S_ISDIR(a) ?
          FILE_ATTRIBUTE_DIRECTORY :
          FILE_ATTRIBUTE_ARCHIVE;
      if ((a & 0222) == 0) // (& S_IWUSR) in p7zip
        Attrib |= FILE_ATTRIBUTE_READONLY;
      // 22.00 : we need type bits for (MY_LIN_S_IFLNK) for IsLinuxSymLink()
      a &= MY_LIN_S_IFMT;
      if (a == MY_LIN_S_IFLNK)
        Attrib |= (a << 16);
      #else
      Attrib = (a << 16) | FILE_ATTRIBUTE_UNIX_EXTENSION;
      #endif
      Attrib_Defined = true;
    }
  } _fi;

  UInt64 _position;
  UInt64 _curSize;
  UInt64 _fileLength_that_WasSet;
  UInt32 _index;

// #ifdef SUPPORT_ALT_STREAMS
#if defined(_WIN32) && !defined(UNDER_CE)
  DWORD _altStream_NeedRestore_AttribVal;
  FString _altStream_NeedRestore_Attrib_for_parentFsPath;
#endif
// #endif

  COutFileStream *_outFileStreamSpec;
  CMyComPtr<ISequentialOutStream> _outFileStream;

  CByteBuffer _outMemBuf;
  CBufPtrSeqOutStream *_bufPtrSeqOutStream_Spec;
  CMyComPtr<ISequentialOutStream> _bufPtrSeqOutStream;

 #ifndef Z7_SFX
  COutStreamWithHash *_hashStreamSpec;
  CMyComPtr<ISequentialOutStream> _hashStream;
  bool _hashStreamWasUsed;
  
  bool _use_baseParentFolder_mode;
  UInt32 _baseParentFolder;
 #endif

  UStringVector _removePathParts;

  UInt64 _packTotal;
  UInt64 _progressTotal;
  // bool _progressTotal_Defined;

  CObjectVector<CDirPathTime> _extractedFolders;
  
  #ifndef _WIN32
  // CObjectVector<NWindows::NFile::NDir::CDelayedSymLink> _delayedSymLinks;
  #endif

  void CreateComplexDirectory(const UStringVector &dirPathParts, FString &fullPath);
  HRESULT GetTime(UInt32 index, PROPID propID, CArcTime &ft);
  HRESULT GetUnpackSize();

  FString Hash_GetFullFilePath();

  void SetAttrib();

public:
  HRESULT SendMessageError(const char *message, const FString &path);
  HRESULT SendMessageError_with_Error(HRESULT errorCode, const char *message, const FString &path);
  HRESULT SendMessageError_with_LastError(const char *message, const FString &path);
  HRESULT SendMessageError2(HRESULT errorCode, const char *message, const FString &path1, const FString &path2);

#if defined(_WIN32) && !defined(UNDER_CE) && !defined(Z7_SFX)
  NExtract::NZoneIdMode::EEnum ZoneMode;
  CByteBuffer ZoneBuf;
#endif

  CMyComPtr2_Create<ICompressProgressInfo, CLocalProgress> LocalProgressSpec;

  UInt64 NumFolders;
  UInt64 NumFiles;
  UInt64 NumAltStreams;
  UInt64 UnpackSize;
  UInt64 AltStreams_UnpackSize;
  
  FString DirPathPrefix_for_HashFiles;

  CArchiveExtractCallback();

  void InitForMulti(bool multiArchives,
      NExtract::NPathMode::EEnum pathMode,
      NExtract::NOverwriteMode::EEnum overwriteMode,
      NExtract::NZoneIdMode::EEnum zoneMode,
      bool keepAndReplaceEmptyDirPrefixes)
  {
    _multiArchives = multiArchives;
    _pathMode = pathMode;
    _overwriteMode = overwriteMode;
#if defined(_WIN32) && !defined(UNDER_CE) && !defined(Z7_SFX)
     ZoneMode = zoneMode;
#else
     UNUSED_VAR(zoneMode)
#endif
    _keepAndReplaceEmptyDirPrefixes = keepAndReplaceEmptyDirPrefixes;
    NumFolders = NumFiles = NumAltStreams = UnpackSize = AltStreams_UnpackSize = 0;
  }

  #ifndef Z7_SFX

  void SetHashMethods(IHashCalc *hash)
  {
    if (!hash)
      return;
    _hashStreamSpec = new COutStreamWithHash;
    _hashStream = _hashStreamSpec;
    _hashStreamSpec->_hash = hash;
  }

  #endif

  void InitBeforeNewArchive();

  void Init(
      const CExtractNtOptions &ntOptions,
      const NWildcard::CCensorNode *wildcardCensor,
      const CArc *arc,
      IFolderArchiveExtractCallback *extractCallback2,
      bool stdOutMode, bool testMode,
      const FString &directoryPath,
      const UStringVector &removePathParts, bool removePartsForAltStreams,
      UInt64 packSize);


  #ifdef SUPPORT_LINKS

private:
  CHardLinks _hardLinks;
  CLinkInfo _link;

  // FString _copyFile_Path;
  // HRESULT MyCopyFile(ISequentialOutStream *outStream);
  HRESULT Link(const FString &fullProcessedPath);
  HRESULT ReadLink();

public:
  // call PrepareHardLinks() after Init()
  HRESULT PrepareHardLinks(const CRecordVector<UInt32> *realIndices);  // NULL means all items

  #endif


  #ifdef SUPPORT_ALT_STREAMS
  CObjectVector<CIndexToPathPair> _renamedFiles;
  #endif

  // call it after Init()

  #ifndef Z7_SFX
  void SetBaseParentFolderIndex(UInt32 indexInArc)
  {
    _baseParentFolder = indexInArc;
    _use_baseParentFolder_mode = true;
  }
  #endif

  HRESULT CloseArc();

private:
  void ClearExtractedDirsInfo()
  {
    _extractedFolders.Clear();
    #ifndef _WIN32
    // _delayedSymLinks.Clear();
    #endif
  }

  HRESULT Read_fi_Props();
  void CorrectPathParts();
  void GetFiTimesCAM(CFiTimesCAM &pt);
  void CreateFolders();
  
  HRESULT CheckExistFile(FString &fullProcessedPath, bool &needExit);
  HRESULT GetExtractStream(CMyComPtr<ISequentialOutStream> &outStreamLoc, bool &needExit);
  HRESULT GetItem(UInt32 index);

  HRESULT CloseFile();
  HRESULT CloseReparseAndFile();
  HRESULT CloseReparseAndFile2();
  HRESULT SetDirsTimes();

  const void *NtReparse_Data;
  UInt32 NtReparse_Size;

  #ifdef SUPPORT_LINKS
  HRESULT SetFromLinkPath(
      const FString &fullProcessedPath,
      const CLinkInfo &linkInfo,
      bool &linkWasSet);
  #endif
};


struct CArchiveExtractCallback_Closer
{
  CArchiveExtractCallback *_ref;
  
  CArchiveExtractCallback_Closer(CArchiveExtractCallback *ref): _ref(ref) {}
  
  HRESULT Close()
  {
    HRESULT res = S_OK;
    if (_ref)
    {
      res = _ref->CloseArc();
      _ref = NULL;
    }
    return res;
  }
  
  ~CArchiveExtractCallback_Closer()
  {
    Close();
  }
};


bool CensorNode_CheckPath(const NWildcard::CCensorNode &node, const CReadArcItem &item);

bool Is_ZoneId_StreamName(const wchar_t *s);
void ReadZoneFile_Of_BaseFile(CFSTR fileName, CByteBuffer &buf);
bool WriteZoneFile_To_BaseFile(CFSTR fileName, const CByteBuffer &buf);

#endif

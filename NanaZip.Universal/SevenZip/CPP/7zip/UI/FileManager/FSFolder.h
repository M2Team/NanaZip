// FSFolder.h

#ifndef ZIP7_INC_FS_FOLDER_H
#define ZIP7_INC_FS_FOLDER_H

#include "../../../Common/MyCom.h"
#include "../../../Common/MyBuffer.h"

#include "../../../Windows/FileFind.h"

#include "../../Archive/IArchive.h"

#include "IFolder.h"
#include "TextPairs.h"

namespace NFsFolder {

class CFSFolder;

#define FS_SHOW_LINKS_INFO
// #define FS_SHOW_CHANGE_TIME

struct CDirItem: public NWindows::NFile::NFind::CFileInfo
{
#ifndef UNDER_CE
  UInt64 PackSize;
#endif

#ifdef FS_SHOW_LINKS_INFO
  FILETIME ChangeTime;
  UInt64 FileIndex;
  UInt32 NumLinks;
  bool FileInfo_Defined;
  bool FileInfo_WasRequested;
  bool ChangeTime_Defined;
  bool ChangeTime_WasRequested;
#endif

#ifndef UNDER_CE
  bool PackSize_Defined;
#endif

  bool FolderStat_Defined;
  int Parent;

#ifndef UNDER_CE
  CByteBuffer Reparse;
#endif

  UInt64 NumFolders;
  UInt64 NumFiles;
};

/*
struct CAltStream
{
  UInt64 Size;
  UInt64 PackSize;
  bool PackSize_Defined;
  int Parent;
  UString Name;
};
*/

struct CFsFolderStat
{
  UInt64 NumFolders;
  UInt64 NumFiles;
  UInt64 Size;
  IProgress *Progress;
  FString Path;

  CFsFolderStat(): NumFolders(0), NumFiles(0), Size(0), Progress(NULL) {}
  CFsFolderStat(const FString &path, IProgress *progress Z7_lifetimebound = NULL):
      NumFolders(0), NumFiles(0), Size(0), Progress(progress), Path(path) {}

  HRESULT Enumerate();
};

class CFSFolder Z7_final:
  public IFolderFolder,
  public IArchiveGetRawProps,
  public IFolderCompare,
  #ifdef USE_UNICODE_FSTRING
  public IFolderGetItemName,
  #endif
  public IFolderWasChanged,
  public IFolderOperations,
  // public IFolderOperationsDeleteToRecycleBin,
  public IFolderCalcItemFullSize,
  public IFolderClone,
  public IFolderGetSystemIconIndex,
  public IFolderSetFlatMode,
  // public IFolderSetShowNtfsStreamsMode,
  public CMyUnknownImp
{
  Z7_COM_QI_BEGIN2(IFolderFolder)
    Z7_COM_QI_ENTRY(IArchiveGetRawProps)
    Z7_COM_QI_ENTRY(IFolderCompare)
    #ifdef USE_UNICODE_FSTRING
    Z7_COM_QI_ENTRY(IFolderGetItemName)
    #endif
    Z7_COM_QI_ENTRY(IFolderWasChanged)
    // Z7_COM_QI_ENTRY(IFolderOperationsDeleteToRecycleBin)
    Z7_COM_QI_ENTRY(IFolderOperations)
    Z7_COM_QI_ENTRY(IFolderCalcItemFullSize)
    Z7_COM_QI_ENTRY(IFolderClone)
    Z7_COM_QI_ENTRY(IFolderGetSystemIconIndex)
    Z7_COM_QI_ENTRY(IFolderSetFlatMode)
    // Z7_COM_QI_ENTRY(IFolderSetShowNtfsStreamsMode)
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

  Z7_IFACE_COM7_IMP(IFolderFolder)
  Z7_IFACE_COM7_IMP(IArchiveGetRawProps)
  Z7_IFACE_COM7_IMP(IFolderCompare)
  #ifdef USE_UNICODE_FSTRING
  Z7_IFACE_COM7_IMP(IFolderGetItemName)
  #endif
  Z7_IFACE_COM7_IMP(IFolderWasChanged)
  Z7_IFACE_COM7_IMP(IFolderOperations)
  Z7_IFACE_COM7_IMP(IFolderCalcItemFullSize)
  Z7_IFACE_COM7_IMP(IFolderClone)
  Z7_IFACE_COM7_IMP(IFolderGetSystemIconIndex)
  Z7_IFACE_COM7_IMP(IFolderSetFlatMode)
  // Z7_IFACE_COM7_IMP(IFolderSetShowNtfsStreamsMode)

  bool _flatMode;
  bool _commentsAreLoaded;
  // bool _scanAltStreams;

  FString _path;
  CObjectVector<CDirItem> Files;
  FStringVector Folders;
  // CObjectVector<CAltStream> Streams;
  // CMyComPtr<IFolderFolder> _parentFolder;

  CPairsStorage _comments;

  #ifdef _WIN32
  NWindows::NFile::NFind::CFindChangeNotification _findChangeNotification;
  #endif

  // HRESULT GetItemFullSize(unsigned index, UInt64 &size, IProgress *progress);
  void GetAbsPath(const wchar_t *name, FString &absPath);
  HRESULT BindToFolderSpec(CFSTR name, IFolderFolder **resultFolder);

  bool LoadComments();
  bool SaveComments();
  HRESULT LoadSubItems(int dirItem, const FString &path);

  #ifdef FS_SHOW_LINKS_INFO
  bool ReadFileInfo(CDirItem &di);
  void ReadChangeTime(CDirItem &di);
  #endif

public:
  HRESULT Init(const FString &path /* , IFolderFolder *parentFolder */);
  #if !defined(_WIN32) || defined(UNDER_CE)
  HRESULT InitToRoot() { return Init((FString) FSTRING_PATH_SEPARATOR /* , NULL */); }
  #endif

  CFSFolder():
    _flatMode(false),
    _commentsAreLoaded(false)
    // , _scanAltStreams(false)
    {}

  void GetFullPath(const CDirItem &item, FString &path) const
  {
    // FString prefix;
    // GetPrefix(item, prefix);
    path = _path;
    if (item.Parent >= 0)
      path += Folders[item.Parent];
    path += item.Name;
  }

  // void GetPrefix(const CDirItem &item, FString &prefix) const;

  FString GetRelPath(const CDirItem &item) const;

  void Clear()
  {
    Files.Clear();
    Folders.Clear();
    // Streams.Clear();
  }
};

struct CCopyStateIO
{
  IProgress *Progress;
  UInt64 TotalSize;
  UInt64 StartPos;
  UInt64 CurrentSize;
  bool DeleteSrcFile;

  int ErrorFileIndex;
  UString ErrorMessage;

  CCopyStateIO(): TotalSize(0), StartPos(0), DeleteSrcFile(false) {}

  HRESULT MyCopyFile(CFSTR inPath, CFSTR outPath, DWORD attrib = INVALID_FILE_ATTRIBUTES);
};

HRESULT SendLastErrorMessage(IFolderOperationsExtractCallback *callback, const FString &fileName);

/* destDirPrefix is allowed to be:
   "full_path\" or "full_path:" for alt streams */

HRESULT CopyFileSystemItems(
    const UStringVector &itemsPaths,
    const FString &destDirPrefix,
    bool moveMode,
    IFolderOperationsExtractCallback *callback);

}

#endif

// FSFolder.h

#ifndef __FS_FOLDER_H
#define __FS_FOLDER_H

#include "../../../Common/MyCom.h"
#include "../../../Common/MyBuffer.h"

#include "../../../Windows/FileFind.h"

#include "../../Archive/IArchive.h"

#include "IFolder.h"
#include "TextPairs.h"

namespace NFsFolder {

class CFSFolder;

#define FS_SHOW_LINKS_INFO

struct CDirItem: public NWindows::NFile::NFind::CFileInfo
{
  #ifndef UNDER_CE
  UInt64 PackSize;
  #endif

  #ifdef FS_SHOW_LINKS_INFO
  UInt64 FileIndex;
  UInt32 NumLinks;
  bool FileInfo_Defined;
  bool FileInfo_WasRequested;
  #endif

  #ifndef UNDER_CE
  bool PackSize_Defined;
  #endif

  bool FolderStat_Defined;

  #ifndef UNDER_CE
  CByteBuffer Reparse;
  #endif
  
  UInt64 NumFolders;
  UInt64 NumFiles;
  
  int Parent;
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
  CFsFolderStat(const FString &path, IProgress *progress = NULL):
      NumFolders(0), NumFiles(0), Size(0), Progress(progress), Path(path) {}

  HRESULT Enumerate();
};

class CFSFolder:
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
public:
  MY_QUERYINTERFACE_BEGIN2(IFolderFolder)
    MY_QUERYINTERFACE_ENTRY(IArchiveGetRawProps)
    MY_QUERYINTERFACE_ENTRY(IFolderCompare)
    #ifdef USE_UNICODE_FSTRING
    MY_QUERYINTERFACE_ENTRY(IFolderGetItemName)
    #endif
    MY_QUERYINTERFACE_ENTRY(IFolderWasChanged)
    // MY_QUERYINTERFACE_ENTRY(IFolderOperationsDeleteToRecycleBin)
    MY_QUERYINTERFACE_ENTRY(IFolderOperations)
    MY_QUERYINTERFACE_ENTRY(IFolderCalcItemFullSize)
    MY_QUERYINTERFACE_ENTRY(IFolderClone)
    MY_QUERYINTERFACE_ENTRY(IFolderGetSystemIconIndex)
    MY_QUERYINTERFACE_ENTRY(IFolderSetFlatMode)
    // MY_QUERYINTERFACE_ENTRY(IFolderSetShowNtfsStreamsMode)
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE


  INTERFACE_FolderFolder(;)
  INTERFACE_IArchiveGetRawProps(;)
  INTERFACE_FolderOperations(;)

  STDMETHOD_(Int32, CompareItems)(UInt32 index1, UInt32 index2, PROPID propID, Int32 propIsRaw);

  #ifdef USE_UNICODE_FSTRING
  INTERFACE_IFolderGetItemName(;)
  #endif
  STDMETHOD(WasChanged)(Int32 *wasChanged);
  STDMETHOD(Clone)(IFolderFolder **resultFolder);
  STDMETHOD(CalcItemFullSize)(UInt32 index, IProgress *progress);

  STDMETHOD(SetFlatMode)(Int32 flatMode);
  // STDMETHOD(SetShowNtfsStreamsMode)(Int32 showStreamsMode);

  STDMETHOD(GetSystemIconIndex)(UInt32 index, Int32 *iconIndex);

private:
  FString _path;
  
  CObjectVector<CDirItem> Files;
  FStringVector Folders;
  // CObjectVector<CAltStream> Streams;
  // CMyComPtr<IFolderFolder> _parentFolder;

  bool _commentsAreLoaded;
  CPairsStorage _comments;

  // bool _scanAltStreams;
  bool _flatMode;

  #ifdef _WIN32
  NWindows::NFile::NFind::CFindChangeNotification _findChangeNotification;
  #endif

  HRESULT GetItemsFullSize(const UInt32 *indices, UInt32 numItems, CFsFolderStat &stat);

  HRESULT GetItemFullSize(unsigned index, UInt64 &size, IProgress *progress);
  void GetAbsPath(const wchar_t *name, FString &absPath);
  HRESULT BindToFolderSpec(CFSTR name, IFolderFolder **resultFolder);

  bool LoadComments();
  bool SaveComments();
  HRESULT LoadSubItems(int dirItem, const FString &path);
  
  #ifdef FS_SHOW_LINKS_INFO
  bool ReadFileInfo(CDirItem &di);
  #endif

public:
  HRESULT Init(const FString &path /* , IFolderFolder *parentFolder */);
  #if !defined(_WIN32) || defined(UNDER_CE)
  HRESULT InitToRoot() { return Init((FString) FSTRING_PATH_SEPARATOR /* , NULL */); }
  #endif

  CFSFolder() : _flatMode(false)
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

}

#endif

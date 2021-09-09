// AltStreamsFolder.h

#ifndef __ALT_STREAMS_FOLDER_H
#define __ALT_STREAMS_FOLDER_H

#include "../../../Common/MyCom.h"

#include "../../../Windows/FileFind.h"

#include "../../Archive/IArchive.h"

#include "IFolder.h"

namespace NAltStreamsFolder {

class CAltStreamsFolder;

struct CAltStream
{
  UInt64 Size;
  UInt64 PackSize;
  bool PackSize_Defined;
  UString Name;
};


class CAltStreamsFolder:
  public IFolderFolder,
  public IFolderCompare,
  #ifdef USE_UNICODE_FSTRING
  public IFolderGetItemName,
  #endif
  public IFolderWasChanged,
  public IFolderOperations,
  // public IFolderOperationsDeleteToRecycleBin,
  public IFolderClone,
  public IFolderGetSystemIconIndex,
  public CMyUnknownImp
{
public:
  MY_QUERYINTERFACE_BEGIN2(IFolderFolder)
    MY_QUERYINTERFACE_ENTRY(IFolderCompare)
    #ifdef USE_UNICODE_FSTRING
    MY_QUERYINTERFACE_ENTRY(IFolderGetItemName)
    #endif
    MY_QUERYINTERFACE_ENTRY(IFolderWasChanged)
    // MY_QUERYINTERFACE_ENTRY(IFolderOperationsDeleteToRecycleBin)
    MY_QUERYINTERFACE_ENTRY(IFolderOperations)
    MY_QUERYINTERFACE_ENTRY(IFolderClone)
    MY_QUERYINTERFACE_ENTRY(IFolderGetSystemIconIndex)
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE


  INTERFACE_FolderFolder(;)
  INTERFACE_FolderOperations(;)

  STDMETHOD_(Int32, CompareItems)(UInt32 index1, UInt32 index2, PROPID propID, Int32 propIsRaw);

  #ifdef USE_UNICODE_FSTRING
  INTERFACE_IFolderGetItemName(;)
  #endif
  STDMETHOD(WasChanged)(Int32 *wasChanged);
  STDMETHOD(Clone)(IFolderFolder **resultFolder);

  STDMETHOD(GetSystemIconIndex)(UInt32 index, Int32 *iconIndex);

private:
  FString _pathBaseFile;  // folder
  FString _pathPrefix;    // folder:
  
  CObjectVector<CAltStream> Streams;
  // CMyComPtr<IFolderFolder> _parentFolder;

  NWindows::NFile::NFind::CFindChangeNotification _findChangeNotification;

  HRESULT GetItemFullSize(unsigned index, UInt64 &size, IProgress *progress);
  void GetAbsPath(const wchar_t *name, FString &absPath);

public:
  // path must be with ':' at tail
  HRESULT Init(const FString &path /* , IFolderFolder *parentFolder */);

  CAltStreamsFolder() {}

  void GetFullPath(const CAltStream &item, FString &path) const
  {
    path = _pathPrefix;
    path += us2fs(item.Name);
  }

  void Clear()
  {
    Streams.Clear();
  }
};

}

#endif

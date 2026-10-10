// AltStreamsFolder.h

#ifndef ZIP7_INC_ALT_STREAMS_FOLDER_H
#define ZIP7_INC_ALT_STREAMS_FOLDER_H

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


class CAltStreamsFolder Z7_final:
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
  Z7_COM_QI_BEGIN2(IFolderFolder)
    Z7_COM_QI_ENTRY(IFolderCompare)
    #ifdef USE_UNICODE_FSTRING
    Z7_COM_QI_ENTRY(IFolderGetItemName)
    #endif
    Z7_COM_QI_ENTRY(IFolderWasChanged)
    // Z7_COM_QI_ENTRY(IFolderOperationsDeleteToRecycleBin)
    Z7_COM_QI_ENTRY(IFolderOperations)
    Z7_COM_QI_ENTRY(IFolderClone)
    Z7_COM_QI_ENTRY(IFolderGetSystemIconIndex)
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

  Z7_IFACE_COM7_IMP(IFolderFolder)
  Z7_IFACE_COM7_IMP(IFolderCompare)
 #ifdef USE_UNICODE_FSTRING
  Z7_IFACE_COM7_IMP(IFolderGetItemName)
 #endif
  Z7_IFACE_COM7_IMP(IFolderWasChanged)
  Z7_IFACE_COM7_IMP(IFolderOperations)
  Z7_IFACE_COM7_IMP(IFolderClone)
  Z7_IFACE_COM7_IMP(IFolderGetSystemIconIndex)

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

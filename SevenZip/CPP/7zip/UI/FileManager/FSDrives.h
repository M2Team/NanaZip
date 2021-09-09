// FSDrives.h

#ifndef __FS_DRIVES_H
#define __FS_DRIVES_H

#include "../../../Common/MyCom.h"
#include "../../../Common/MyString.h"

#include "IFolder.h"

struct CDriveInfo
{
  FString Name;
  FString FullSystemName;
  UInt64 DriveSize;
  UInt64 FreeSpace;
  UInt64 ClusterSize;
  // UString Type;
  UString VolumeName;
  UString FileSystemName;
  UINT DriveType;

  bool KnownSize;
  bool KnownSizes;
  bool IsPhysicalDrive;

  FString GetDeviceFileIoName() const;
  CDriveInfo(): KnownSize(false), KnownSizes(false), IsPhysicalDrive(false) {}
};

class CFSDrives:
  public IFolderFolder,
  public IFolderOperations,
  public IFolderGetSystemIconIndex,
  public CMyUnknownImp
{
  CObjectVector<CDriveInfo> _drives;
  bool _volumeMode;
  bool _superMode;

  HRESULT BindToFolderSpec(CFSTR name, IFolderFolder **resultFolder);
  void AddExt(FString &s, unsigned index) const;
  HRESULT GetFileSize(unsigned index, UInt64 &fileSize) const;
public:
  MY_UNKNOWN_IMP2(IFolderGetSystemIconIndex, IFolderOperations)

  INTERFACE_FolderFolder(;)
  INTERFACE_FolderOperations(;)

  STDMETHOD(GetSystemIconIndex)(UInt32 index, Int32 *iconIndex);

  void Init(bool volMode = false, bool superMode = false)
  {
    _volumeMode = volMode;
    _superMode = superMode;
  }
};

#endif

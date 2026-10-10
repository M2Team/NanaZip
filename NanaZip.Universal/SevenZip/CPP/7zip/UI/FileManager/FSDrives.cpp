// FSDrives.cpp

#include "StdAfx.h"

#include "../../../../C/Alloc.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/Defs.h"
#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileIO.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/FileSystem.h"
#include "../../../Windows/PropVariant.h"

#include "../../PropID.h"

#include "FSDrives.h"
#include "FSFolder.h"
#include "LangUtils.h"
#include "SysIconUtils.h"

#include "resource.h"

using namespace NWindows;
using namespace NFile;
using namespace NFind;

static const char * const kVolPrefix   = "\\\\.\\";
static const char * const kSuperPrefix = "\\\\?\\";

FString CDriveInfo::GetDeviceFileIoName() const
{
  FString f (kVolPrefix);
  f += Name;
  return f;
}

struct CPhysTempBuffer
{
  void *buffer;
  CPhysTempBuffer(): buffer(NULL) {}
  ~CPhysTempBuffer() { MidFree(buffer); }
};

static HRESULT CopyFileSpec(CFSTR fromPath, CFSTR toPath,
    bool writeToDisk, UInt64 fileSize,
    UInt32 bufferSize, UInt64 progressStart, IProgress *progress)
{
  NIO::CInFile inFile;
  if (!inFile.Open(fromPath))
    return GetLastError_noZero_HRESULT();
  if (fileSize == (UInt64)(Int64)-1)
  {
    if (!inFile.GetLength(fileSize))
      return GetLastError_noZero_HRESULT();
  }

  NIO::COutFile outFile;
  if (writeToDisk)
  {
    if (!outFile.Open(toPath, FILE_SHARE_WRITE, OPEN_EXISTING, 0))
      return GetLastError_noZero_HRESULT();
  }
  else
    if (!outFile.Create_ALWAYS(toPath))
      return GetLastError_noZero_HRESULT();

  CPhysTempBuffer tempBuffer;
  tempBuffer.buffer = MidAlloc(bufferSize);
  if (!tempBuffer.buffer)
    return E_OUTOFMEMORY;

  for (UInt64 pos = 0; pos < fileSize;)
  {
    {
      const UInt64 progressCur = progressStart + pos;
      RINOK(progress->SetCompleted(&progressCur))
    }
    const UInt64 rem = fileSize - pos;
    UInt32 curSize = (UInt32)MyMin(rem, (UInt64)bufferSize);
    UInt32 processedSize;
    if (!inFile.Read(tempBuffer.buffer, curSize, processedSize))
      return GetLastError_noZero_HRESULT();
    if (processedSize == 0)
      break;
    curSize = processedSize;
    if (writeToDisk)
    {
      const UInt32 kMask = 0x1FF;
      curSize = (curSize + kMask) & ~kMask;
      if (curSize > bufferSize)
        return E_FAIL;
    }
    if (!outFile.Write(tempBuffer.buffer, curSize, processedSize))
      return GetLastError_noZero_HRESULT();
    if (curSize != processedSize)
      return E_FAIL;
    pos += curSize;
  }

  return S_OK;
}

static const Byte kProps[] =
{
  kpidName,
  // kpidOutName,
  kpidTotalSize,
  kpidFreeSpace,
  kpidType,
  kpidVolumeName,
  kpidFileSystem,
  kpidClusterSize
};

static const char * const kDriveTypes[] =
{
    "Unknown"
  , "No Root Dir"
  , "Removable"
  , "Fixed"
  , "Remote"
  , "CD-ROM"
  , "RAM disk"
};

Z7_COM7F_IMF(CFSDrives::LoadItems())
{
  _drives.Clear();

  FStringVector driveStrings;
  MyGetLogicalDriveStrings(driveStrings);

  FOR_VECTOR (i, driveStrings)
  {
    CDriveInfo di;
    const FString &driveName = driveStrings[i];
    di.FullSystemName = driveName;
    if (!driveName.IsEmpty())
      di.Name.SetFrom(driveName, driveName.Len() - 1);
    di.ClusterSize = 0;
    di.DriveSize = 0;
    di.FreeSpace = 0;
    di.DriveType = NSystem::MyGetDriveType(driveName);
    bool needRead = true;

    if (di.DriveType == DRIVE_CDROM || di.DriveType == DRIVE_REMOVABLE)
    {
      /*
      DWORD dwSerialNumber;`
      if (!::GetVolumeInformation(di.FullSystemName,
          NULL, 0, &dwSerialNumber, NULL, NULL, NULL, 0))
      */
      {
        needRead = false;
      }
    }

    if (needRead)
    {
      DWORD volumeSerialNumber, maximumComponentLength, fileSystemFlags;
      NSystem::MyGetVolumeInformation(driveName,
          di.VolumeName,
          &volumeSerialNumber, &maximumComponentLength, &fileSystemFlags,
          di.FileSystemName);

      NSystem::MyGetDiskFreeSpace(driveName,
          di.ClusterSize, di.DriveSize, di.FreeSpace);
      di.KnownSizes = true;
      di.KnownSize = true;
    }

    _drives.Add(di);
  }

  if (_volumeMode)
  {
    // we must use IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS
    for (unsigned n = 0; n < 16; n++) // why 16 ?
    {
      FString name ("PhysicalDrive");
      name.Add_UInt32(n);
      FString fullPath (kVolPrefix);
      fullPath += name;
      CFileInfo fi;
      if (!fi.Find(fullPath))
        continue;

      CDriveInfo di;
      di.Name = name;
      // if (_volumeMode == true) we use CDriveInfo::FullSystemName only in GetSystemIconIndex().
      // And we need name without "\\\\.\\" prefix in GetSystemIconIndex().
      // So we don't set di.FullSystemName = fullPath;
      di.FullSystemName = name;
      di.ClusterSize = 0;
      di.DriveSize = fi.Size;
      di.FreeSpace = 0;
      di.DriveType = 0;
      di.IsPhysicalDrive = true;
      di.KnownSize = true;
      _drives.Add(di);
    }
  }

  return S_OK;
}

Z7_COM7F_IMF(CFSDrives::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _drives.Size();
  return S_OK;
}

Z7_COM7F_IMF(CFSDrives::GetProperty(UInt32 itemIndex, PROPID propID, PROPVARIANT *value))
{
  if (itemIndex >= _drives.Size())
    return E_INVALIDARG;
  NCOM::CPropVariant prop;
  const CDriveInfo &di = _drives[itemIndex];
  switch (propID)
  {
    case kpidIsDir:  prop = !_volumeMode; break;
    case kpidName:  prop = fs2us(di.Name); break;
    case kpidOutName:
      if (!di.Name.IsEmpty() && di.Name.Back() == ':')
      {
        FString s = di.Name;
        s.DeleteBack();
        AddExt(s, itemIndex);
        prop = fs2us(s);
      }
      break;

    case kpidTotalSize:   if (di.KnownSize) prop = di.DriveSize; break;
    case kpidFreeSpace:   if (di.KnownSizes) prop = di.FreeSpace; break;
    case kpidClusterSize: if (di.KnownSizes) prop = di.ClusterSize; break;
    case kpidType:
      if (di.DriveType < Z7_ARRAY_SIZE(kDriveTypes))
        prop = kDriveTypes[di.DriveType];
      break;
    case kpidVolumeName:  prop = di.VolumeName; break;
    case kpidFileSystem:  prop = di.FileSystemName; break;
  }
  prop.Detach(value);
  return S_OK;
}

HRESULT CFSDrives::BindToFolderSpec(CFSTR name, IFolderFolder **resultFolder)
{
  *resultFolder = NULL;
  if (_volumeMode)
    return S_OK;
  NFsFolder::CFSFolder *fsFolderSpec = new NFsFolder::CFSFolder;
  CMyComPtr<IFolderFolder> subFolder = fsFolderSpec;
  FString path;
  if (_superMode)
    path = kSuperPrefix;
  path += name;
  RINOK(fsFolderSpec->Init(path))
  *resultFolder = subFolder.Detach();
  return S_OK;
}

Z7_COM7F_IMF(CFSDrives::BindToFolder(UInt32 index, IFolderFolder **resultFolder))
{
  *resultFolder = NULL;
  if (index >= _drives.Size())
    return E_INVALIDARG;
  const CDriveInfo &di = _drives[index];
  /*
  if (_volumeMode)
  {
    *resultFolder = 0;
    CPhysDriveFolder *folderSpec = new CPhysDriveFolder;
    CMyComPtr<IFolderFolder> subFolder = folderSpec;
    RINOK(folderSpec->Init(di.Name));
    *resultFolder = subFolder.Detach();
    return S_OK;
  }
  */
  return BindToFolderSpec(di.FullSystemName, resultFolder);
}

Z7_COM7F_IMF(CFSDrives::BindToFolder(const wchar_t *name, IFolderFolder **resultFolder))
{
  return BindToFolderSpec(us2fs(name), resultFolder);
}

Z7_COM7F_IMF(CFSDrives::BindToParentFolder(IFolderFolder **resultFolder))
{
  *resultFolder = NULL;
  return S_OK;
}

IMP_IFolderFolder_Props(CFSDrives)

Z7_COM7F_IMF(CFSDrives::GetFolderProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidType: prop = "FSDrives"; break;
    case kpidPath:
      if (_volumeMode)
        prop = kVolPrefix;
      else if (_superMode)
        prop = kSuperPrefix;
      else
        prop = (UString)LangString(IDS_COMPUTER) + WCHAR_PATH_SEPARATOR;
      break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


Z7_COM7F_IMF(CFSDrives::GetSystemIconIndex(UInt32 index, Int32 *iconIndex))
{
  *iconIndex = -1;
  const CDriveInfo &di = _drives[index];
  return Shell_GetFileInfo_SysIconIndex_for_Path_return_HRESULT(
      di.FullSystemName,
      _volumeMode ?
          FILE_ATTRIBUTE_ARCHIVE:
          FILE_ATTRIBUTE_DIRECTORY,
      iconIndex);
}

void CFSDrives::AddExt(FString &s, unsigned index) const
{
  s.Add_Dot();
  const CDriveInfo &di = _drives[index];
  UString n = di.FileSystemName;
  n.MakeLower_Ascii();
  const char *ext;
  if (di.DriveType == DRIVE_CDROM)
    ext = "iso";
  else
  {
    unsigned i;
    for (i = 0; i < n.Len(); i++)
    {
      const wchar_t c = n[i];
      if (c < 'a' || c > 'z')
        break;
    }
    if (i != 0)
    {
      n.DeleteFrom(i);
      s += us2fs(n);
      return;
    }
    ext = "img";
  }
  /*
       if (n.IsPrefixedBy_Ascii_NoCase("NTFS")) ext = "ntfs";
  else if (n.IsPrefixedBy_Ascii_NoCase("UDF")) ext = "udf";
  else if (n.IsPrefixedBy_Ascii_NoCase("exFAT")) ext = "exfat";
  */
  s += ext;
}

HRESULT CFSDrives::GetFileSize(unsigned index, UInt64& fileSize) const
{
#ifdef Z7_DEVICE_FILE
  NIO::CInFile inFile;
  if (!inFile.Open(_drives[index].GetDeviceFileIoName()))
    return GetLastError_noZero_HRESULT();
  if (inFile.SizeDefined)
  {
    fileSize = inFile.Size;
    return S_OK;
  }
#else
  UNUSED_VAR(index)
#endif
  fileSize = 0;
  return E_FAIL;
}

Z7_COM7F_IMF(CFSDrives::CopyTo(Int32 moveMode, const UInt32 *indices, UInt32 numItems,
    Int32 /* includeAltStreams */, Int32 /* replaceAltStreamColon */,
    const wchar_t *path, IFolderOperationsExtractCallback *callback))
{
  if (numItems == 0)
    return S_OK;
  if (moveMode)
    return E_NOTIMPL;
  if (!_volumeMode)
    return E_NOTIMPL;

  UInt64 totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    const CDriveInfo &di = _drives[indices[i]];
    if (di.KnownSize)
      totalSize += di.DriveSize;
  }
  RINOK(callback->SetTotal(totalSize))
  RINOK(callback->SetNumFiles(numItems))

  const FString destPath = us2fs(path);
  if (destPath.IsEmpty())
    return E_INVALIDARG;

  const bool isAltDest = NName::IsAltPathPrefix(destPath);
  const bool isDirectPath = (!isAltDest && !IsPathSepar(destPath.Back()));

  if (isDirectPath)
  {
    if (numItems > 1)
      return E_INVALIDARG;
  }

  UInt64 completedSize = 0;
  RINOK(callback->SetCompleted(&completedSize))
  for (i = 0; i < numItems; i++)
  {
    const unsigned index = indices[i];
    const CDriveInfo &di = _drives[index];
    FString destPath2 = destPath;

    if (!isDirectPath)
    {
      FString destName = di.Name;
      if (!destName.IsEmpty() && destName.Back() == ':')
      {
        destName.DeleteBack();
        AddExt(destName, index);
      }
      destPath2 += destName;
    }

    const FString srcPath = di.GetDeviceFileIoName();

    UInt64 fileSize = 0;
    if (GetFileSize(index, fileSize) != S_OK)
    {
      return E_FAIL;
    }
    if (!di.KnownSize)
    {
      totalSize += fileSize;
      RINOK(callback->SetTotal(totalSize))
    }

    Int32 writeAskResult;
    CMyComBSTR destPathResult;
    RINOK(callback->AskWrite(fs2us(srcPath), BoolToInt(false), NULL, &fileSize,
        fs2us(destPath2), &destPathResult, &writeAskResult))

    if (!IntToBool(writeAskResult))
    {
      if (totalSize >= fileSize)
        totalSize -= fileSize;
      RINOK(callback->SetTotal(totalSize))
      continue;
    }

    RINOK(callback->SetCurrentFilePath(fs2us(srcPath)))

    const UInt32 kBufferSize = (4 << 20);
    const UInt32 bufferSize = (di.DriveType == DRIVE_REMOVABLE) ? (18 << 10) * 4 : kBufferSize;
    RINOK(CopyFileSpec(srcPath, us2fs(destPathResult), false, fileSize, bufferSize, completedSize, callback))
    completedSize += fileSize;
  }

  return S_OK;
}

Z7_COM7F_IMF(CFSDrives::CopyFrom(Int32 /* moveMode */, const wchar_t * /* fromFolderPath */,
    const wchar_t * const * /* itemsPaths */, UInt32 /* numItems */, IProgress * /* progress */))
{
  return E_NOTIMPL;
}

Z7_COM7F_IMF(CFSDrives::CopyFromFile(UInt32 /* index */, const wchar_t * /* fullFilePath */, IProgress * /* progress */))
{
  return E_NOTIMPL;
}

Z7_COM7F_IMF(CFSDrives::CreateFolder(const wchar_t * /* name */, IProgress * /* progress */))
{
  return E_NOTIMPL;
}

Z7_COM7F_IMF(CFSDrives::CreateFile(const wchar_t * /* name */, IProgress * /* progress */))
{
  return E_NOTIMPL;
}

Z7_COM7F_IMF(CFSDrives::Rename(UInt32 /* index */, const wchar_t * /* newName */, IProgress * /* progress */))
{
  return E_NOTIMPL;
}

Z7_COM7F_IMF(CFSDrives::Delete(const UInt32 * /* indices */, UInt32 /* numItems */, IProgress * /* progress */))
{
  return E_NOTIMPL;
}

Z7_COM7F_IMF(CFSDrives::SetProperty(UInt32 /* index */, PROPID /* propID */,
    const PROPVARIANT * /* value */, IProgress * /* progress */))
{
  return E_NOTIMPL;
}

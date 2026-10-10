// AltStreamsFolder.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/Wildcard.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileIO.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariant.h"

#include "../Common/ExtractingFilePath.h"

#include "../Agent/IFolderArchive.h"

#include "AltStreamsFolder.h"
#include "FSDrives.h"
#include "FSFolder.h"

#include "SysIconUtils.h"

using namespace NWindows;
using namespace NFile;
using namespace NFind;
using namespace NDir;
using namespace NName;

#ifndef USE_UNICODE_FSTRING
int CompareFileNames_ForFolderList(const FChar *s1, const FChar *s2);
#endif

#ifndef UNDER_CE

namespace NFsFolder
{
bool MyGetCompressedFileSizeW(CFSTR path, UInt64 &size);
}

#endif

namespace NAltStreamsFolder {

static const Byte kProps[] =
{
  kpidName,
  kpidSize,
  kpidPackSize
};

static unsigned GetFsParentPrefixSize(const FString &path)
{
  if (IsNetworkShareRootPath(path))
    return 0;
  const unsigned prefixSize = GetRootPrefixSize(path);
  if (prefixSize == 0 || prefixSize >= path.Len())
    return 0;
  FString parentPath = path;
  int pos = parentPath.ReverseFind_PathSepar();
  if (pos < 0)
    return 0;
  if (pos == (int)parentPath.Len() - 1)
  {
    parentPath.DeleteBack();
    pos = parentPath.ReverseFind_PathSepar();
    if (pos < 0)
      return 0;
  }
  if ((unsigned)pos + 1 < prefixSize)
    return 0;
  return (unsigned)pos + 1;
}

HRESULT CAltStreamsFolder::Init(const FString &path /* , IFolderFolder *parentFolder */)
{
   // _parentFolder = parentFolder;
   if (path.Back() != ':')
     return E_FAIL;

  _pathPrefix = path;
  _pathBaseFile = path;
  _pathBaseFile.DeleteBack();

  {
    CFileInfo fi;
    if (!fi.Find(_pathBaseFile))
      return GetLastError_noZero_HRESULT();
  }

  unsigned prefixSize = GetFsParentPrefixSize(_pathBaseFile);
  if (prefixSize == 0)
    return S_OK;
  FString parentPath = _pathBaseFile;
  parentPath.DeleteFrom(prefixSize);

  _findChangeNotification.FindFirst(parentPath, false,
        FILE_NOTIFY_CHANGE_FILE_NAME
      | FILE_NOTIFY_CHANGE_DIR_NAME
      | FILE_NOTIFY_CHANGE_ATTRIBUTES
      | FILE_NOTIFY_CHANGE_SIZE
      | FILE_NOTIFY_CHANGE_LAST_WRITE
      /*
      | FILE_NOTIFY_CHANGE_LAST_ACCESS
      | FILE_NOTIFY_CHANGE_CREATION
      | FILE_NOTIFY_CHANGE_SECURITY
      */
      );
  /*
  if (_findChangeNotification.IsHandleAllocated())
    return S_OK;
  return GetLastError();
  */
  return S_OK;
}

Z7_COM7F_IMF(CAltStreamsFolder::LoadItems())
{
  Int32 dummy;
  WasChanged(&dummy);
  Clear();

  CStreamEnumerator enumerator(_pathBaseFile);

  CStreamInfo si;
  for (;;)
  {
    bool found;
    if (!enumerator.Next(si, found))
    {
      // if (GetLastError() == ERROR_ACCESS_DENIED)
      //   break;
      // return E_FAIL;
      break;
    }
    if (!found)
      break;
    if (si.IsMainStream())
      continue;
    CAltStream ss;
    ss.Name = si.GetReducedName();
    if (!ss.Name.IsEmpty() && ss.Name[0] == ':')
      ss.Name.Delete(0);

    ss.Size = si.Size;
    ss.PackSize_Defined = false;
    ss.PackSize = si.Size;
    Streams.Add(ss);
  }

  return S_OK;
}

Z7_COM7F_IMF(CAltStreamsFolder::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = Streams.Size();
  return S_OK;
}

#ifdef USE_UNICODE_FSTRING

Z7_COM7F_IMF(CAltStreamsFolder::GetItemPrefix(UInt32 /* index */, const wchar_t **name, unsigned *len))
{
  *name = NULL;
  *len = 0;
  return S_OK;
}

Z7_COM7F_IMF(CAltStreamsFolder::GetItemName(UInt32 index, const wchar_t **name, unsigned *len))
{
  *name = NULL;
  *len = 0;
  {
    const CAltStream &ss = Streams[index];
    *name = ss.Name;
    *len = ss.Name.Len();
  }
  return S_OK;
}

Z7_COM7F_IMF2(UInt64, CAltStreamsFolder::GetItemSize(UInt32 index))
{
  return Streams[index].Size;
}

#endif


Z7_COM7F_IMF(CAltStreamsFolder::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  NCOM::CPropVariant prop;
  {
    CAltStream &ss = Streams[index];
    switch (propID)
    {
      case kpidIsDir: prop = false; break;
      case kpidIsAltStream: prop = true; break;
      case kpidName: prop = ss.Name; break;
      case kpidSize: prop = ss.Size; break;
      case kpidPackSize:
        #ifdef UNDER_CE
        prop = ss.Size;
        #else
        if (!ss.PackSize_Defined)
        {
          ss.PackSize_Defined = true;
          if (!NFsFolder::MyGetCompressedFileSizeW(_pathPrefix + us2fs(ss.Name), ss.PackSize))
            ss.PackSize = ss.Size;
        }
        prop = ss.PackSize;
        #endif
        break;
    }
  }

  prop.Detach(value);
  return S_OK;
}


// returns Position of extension including '.'

static inline const wchar_t *GetExtensionPtr(const UString &name)
{
  const int dotPos = name.ReverseFind_Dot();
  return name.Ptr(dotPos < 0 ? name.Len() : (unsigned)dotPos);
}

Z7_COM7F_IMF2(Int32, CAltStreamsFolder::CompareItems(UInt32 index1, UInt32 index2, PROPID propID, Int32 /* propIsRaw */))
{
  const CAltStream &ss1 = Streams[index1];
  const CAltStream &ss2 = Streams[index2];

  switch (propID)
  {
    case kpidName:
    {
      return CompareFileNames_ForFolderList(ss1.Name, ss2.Name);
      // return MyStringCompareNoCase(ss1.Name, ss2.Name);
    }
    case kpidSize:
      return MyCompare(ss1.Size, ss2.Size);
    case kpidPackSize:
    {
      #ifdef UNDER_CE
      return MyCompare(ss1.Size, ss2.Size);
      #else
      // PackSize can be undefined here
      return MyCompare(
          ss1.PackSize,
          ss2.PackSize);
      #endif
    }

    case kpidExtension:
      return CompareFileNames_ForFolderList(
          GetExtensionPtr(ss1.Name),
          GetExtensionPtr(ss2.Name));
  }

  return 0;
}

Z7_COM7F_IMF(CAltStreamsFolder::BindToFolder(UInt32 /* index */, IFolderFolder **resultFolder))
{
  *resultFolder = NULL;
  return E_INVALIDARG;
}

Z7_COM7F_IMF(CAltStreamsFolder::BindToFolder(const wchar_t * /* name */, IFolderFolder **resultFolder))
{
  *resultFolder = NULL;
  return E_INVALIDARG;
}

// static CFSTR const kSuperPrefix = FTEXT("\\\\?\\");

Z7_COM7F_IMF(CAltStreamsFolder::BindToParentFolder(IFolderFolder **resultFolder))
{
  *resultFolder = NULL;
  /*
  if (_parentFolder)
  {
    CMyComPtr<IFolderFolder> parentFolder = _parentFolder;
    *resultFolder = parentFolder.Detach();
    return S_OK;
  }
  */

  if (IsDriveRootPath_SuperAllowed(_pathBaseFile))
  {
    CFSDrives *drivesFolderSpec = new CFSDrives;
    CMyComPtr<IFolderFolder> drivesFolder = drivesFolderSpec;
    drivesFolderSpec->Init();
    *resultFolder = drivesFolder.Detach();
    return S_OK;
  }

  /*
  parentPath.DeleteFrom(pos + 1);

  if (parentPath == kSuperPrefix)
  {
    #ifdef UNDER_CE
    *resultFolder = 0;
    #else
    CFSDrives *drivesFolderSpec = new CFSDrives;
    CMyComPtr<IFolderFolder> drivesFolder = drivesFolderSpec;
    drivesFolderSpec->Init(false, true);
    *resultFolder = drivesFolder.Detach();
    #endif
    return S_OK;
  }

  FString parentPathReduced = parentPath.Left(pos);

  #ifndef UNDER_CE
  pos = parentPathReduced.ReverseFind_PathSepar();
  if (pos == 1)
  {
    if (!IS_PATH_SEPAR_CHAR(parentPath[0]))
      return E_FAIL;
    CNetFolder *netFolderSpec = new CNetFolder;
    CMyComPtr<IFolderFolder> netFolder = netFolderSpec;
    netFolderSpec->Init(fs2us(parentPath));
    *resultFolder = netFolder.Detach();
    return S_OK;
  }
  #endif

  CFSFolder *parentFolderSpec = new CFSFolder;
  CMyComPtr<IFolderFolder> parentFolder = parentFolderSpec;
  RINOK(parentFolderSpec->Init(parentPath, 0));
  *resultFolder = parentFolder.Detach();
  */

  return S_OK;
}

IMP_IFolderFolder_Props(CAltStreamsFolder)

Z7_COM7F_IMF(CAltStreamsFolder::GetFolderProperty(PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidType: prop = "AltStreamsFolder"; break;
    case kpidPath: prop = fs2us(_pathPrefix); break;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

Z7_COM7F_IMF(CAltStreamsFolder::WasChanged(Int32 *wasChanged))
{
  bool wasChangedMain = false;
  for (;;)
  {
    if (!_findChangeNotification.IsHandleAllocated())
    {
      *wasChanged = BoolToInt(false);
      return S_OK;
    }

    const DWORD waitResult = ::WaitForSingleObject(_findChangeNotification, 0);
    const bool wasChangedLoc = (waitResult == WAIT_OBJECT_0);
    if (wasChangedLoc)
    {
      _findChangeNotification.FindNext();
      wasChangedMain = true;
    }
    else
      break;
  }
  *wasChanged = BoolToInt(wasChangedMain);
  return S_OK;
}

Z7_COM7F_IMF(CAltStreamsFolder::Clone(IFolderFolder **resultFolder))
{
  CAltStreamsFolder *folderSpec = new CAltStreamsFolder;
  CMyComPtr<IFolderFolder> folderNew = folderSpec;
  folderSpec->Init(_pathPrefix);
  *resultFolder = folderNew.Detach();
  return S_OK;
}

void CAltStreamsFolder::GetAbsPath(const wchar_t *name, FString &absPath)
{
  absPath.Empty();
  if (!IsAbsolutePath(name))
    absPath += _pathPrefix;
  absPath += us2fs(name);
}



static HRESULT SendMessageError(IFolderOperationsExtractCallback *callback,
    const wchar_t *message, const FString &fileName)
{
  UString s = message;
  s += " : ";
  s += fs2us(fileName);
  return callback->ShowMessage(s);
}

static HRESULT SendMessageError(IFolderArchiveUpdateCallback *callback,
    const wchar_t *message, const FString &fileName)
{
  UString s = message;
  s += " : ";
  s += fs2us(fileName);
  return callback->UpdateErrorMessage(s);
}

static HRESULT SendMessageError(IFolderOperationsExtractCallback *callback,
    const char *message, const FString &fileName)
{
  return SendMessageError(callback, MultiByteToUnicodeString(message), fileName);
}

/*
static HRESULT SendMessageError(IFolderArchiveUpdateCallback *callback,
    const char *message, const FString &fileName)
{
  return SendMessageError(callback, MultiByteToUnicodeString(message), fileName);
}
*/

Z7_COM7F_IMF(CAltStreamsFolder::CreateFolder(const wchar_t * /* name */, IProgress * /* progress */))
{
  return E_NOTIMPL;
}

Z7_COM7F_IMF(CAltStreamsFolder::CreateFile(const wchar_t *name, IProgress * /* progress */))
{
  FString absPath;
  GetAbsPath(name, absPath);
  NIO::COutFile outFile;
  if (!outFile.Create_NEW(absPath))
    return GetLastError_noZero_HRESULT();
  return S_OK;
}

static UString GetLastErrorMessage()
{
  return NError::MyFormatMessage(GetLastError_noZero_HRESULT());
}

static HRESULT UpdateFile(NFsFolder::CCopyStateIO &state, CFSTR inPath, CFSTR outPath, IFolderArchiveUpdateCallback *callback)
{
  if (NFind::DoesFileOrDirExist(outPath))
  {
    RINOK(SendMessageError(callback, NError::MyFormatMessage(ERROR_ALREADY_EXISTS), FString(outPath)))
    CFileInfo fi;
    if (fi.Find(inPath))
    {
      if (state.TotalSize >= fi.Size)
        state.TotalSize -= fi.Size;
    }
    return S_OK;
  }

  {
    if (callback)
      RINOK(callback->CompressOperation(fs2us(inPath)))
    RINOK(state.MyCopyFile(inPath, outPath))
    if (state.ErrorFileIndex >= 0)
    {
      if (state.ErrorMessage.IsEmpty())
        state.ErrorMessage = GetLastErrorMessage();
      FString errorName;
      if (state.ErrorFileIndex == 0)
        errorName = inPath;
      else
        errorName = outPath;
      if (callback)
        RINOK(SendMessageError(callback, state.ErrorMessage, errorName))
    }
    if (callback)
      RINOK(callback->OperationResult(0))
  }

  return S_OK;
}

EXTERN_C_BEGIN

typedef struct
{
  // #if (_WIN32_WINNT >= _WIN32_WINNT_WIN10_RS1)
  union
  {
    BOOLEAN ReplaceIfExists;  // FileRenameInformation
    ULONG Flags;              // FileRenameInformationEx
  } DUMMYUNIONNAME;
  // #else
  // BOOLEAN ReplaceIfExists;
  // #endif
  HANDLE RootDirectory;
  ULONG FileNameLength;
  WCHAR FileName[1];
} Z7_WIN_FILE_RENAME_INFORMATION;

EXTERN_C_END

// static Func_NtSetInformationFile f_NtSetInformationFile;
// static bool g_NtSetInformationFile_WasRequested = false;
Z7_DIAGNOSTIC_IGNORE_CAST_FUNCTION

Z7_COM7F_IMF(CAltStreamsFolder::Rename(UInt32 index, const wchar_t *newName, IProgress *progress))
{
  const CAltStream &ss = Streams[index];
  const FString srcPath = _pathPrefix + us2fs(ss.Name);
  {
    NIO::CInFile inFile;
    if (inFile.Open_for_FileRenameInformation(srcPath))
    {
      UString destPath (':');
      destPath += newName;
      const ULONG len = (ULONG)sizeof(wchar_t) * destPath.Len();
      CByteBuffer buffer(sizeof(Z7_WIN_FILE_RENAME_INFORMATION) + len);
      // buffer is 4 bytes larger than required.
      Z7_WIN_FILE_RENAME_INFORMATION *fri = (Z7_WIN_FILE_RENAME_INFORMATION *)(void *)(Byte *)buffer;
      memset(fri, 0, sizeof(Z7_WIN_FILE_RENAME_INFORMATION));
      /* DOCS: If ReplaceIfExists is set to TRUE, the rename operation will succeed only
         if a stream with the same name does not exist or is a zero-length data stream. */
      fri->ReplaceIfExists = FALSE;
      fri->RootDirectory = NULL;
      fri->FileNameLength = len;
      memcpy(fri->FileName, destPath.Ptr(), len);
      const DWORD res = inFile.Call_NtSetInformationFile_return_WinError(
          fri, (ULONG)buffer.Size(), Z7_WIN_FileRenameInformation);
      return HRESULT_FROM_WIN32(res);
    }
  }

  CMyComPtr<IFolderArchiveUpdateCallback> callback;
  if (progress)
  {
    RINOK(progress->QueryInterface(IID_IFolderArchiveUpdateCallback, (void **)&callback))
  }

  if (callback)
  {
    RINOK(callback->SetNumFiles(1))
    RINOK(callback->SetTotal(ss.Size))
  }

  NFsFolder::CCopyStateIO state;
  state.Progress = progress;
  state.TotalSize = 0;
  state.DeleteSrcFile = true;

  const FString destPath = _pathPrefix + us2fs(newName);
  return UpdateFile(state, srcPath, destPath, callback);
}

Z7_COM7F_IMF(CAltStreamsFolder::Delete(const UInt32 *indices, UInt32 numItems,IProgress *progress))
{
  RINOK(progress->SetTotal(numItems))
  for (UInt32 i = 0; i < numItems; i++)
  {
    const CAltStream &ss = Streams[indices[i]];
    const FString fullPath = _pathPrefix + us2fs(ss.Name);
    const bool result = DeleteFileAlways(fullPath);
    if (!result)
      return GetLastError_noZero_HRESULT();
    const UInt64 completed = i;
    RINOK(progress->SetCompleted(&completed))
  }
  return S_OK;
}

Z7_COM7F_IMF(CAltStreamsFolder::SetProperty(UInt32 /* index */, PROPID /* propID */,
    const PROPVARIANT * /* value */, IProgress * /* progress */))
{
  return E_NOTIMPL;
}

Z7_COM7F_IMF(CAltStreamsFolder::GetSystemIconIndex(UInt32 index, Int32 *iconIndex))
{
  const CAltStream &ss = Streams[index];
  return Shell_GetFileInfo_SysIconIndex_for_Path_return_HRESULT(
      _pathPrefix + us2fs(ss.Name),
      FILE_ATTRIBUTE_ARCHIVE,
      iconIndex);
}

/*
Z7_CLASS_IMP_COM_1(
  CGetProp
  , IGetProp
)
public:
  // const CArc *Arc;
  // UInt32 IndexInArc;
  UString Name; // relative path
  UInt64 Size;
};

Z7_COM7F_IMF(CGetProp::GetProp(PROPID propID, PROPVARIANT *value))
{
  if (propID == kpidName)
  {
    COM_TRY_BEGIN
    NCOM::CPropVariant prop;
    prop = Name;
    prop.Detach(value);
    return S_OK;
    COM_TRY_END
  }
  if (propID == kpidSize)
  {
    NCOM::CPropVariant prop = Size;
    prop.Detach(value);
    return S_OK;
  }
  NCOM::CPropVariant prop;
  prop.Detach(value);
  return S_OK;
}
*/

static HRESULT CopyStream(
    NFsFolder::CCopyStateIO &state,
    const FString &srcPath,
    const CFileInfo &srcFileInfo,
    const CAltStream &srcAltStream,
    const FString &destPathSpec,
    IFolderOperationsExtractCallback *callback)
{
  FString destPath = destPathSpec;
  if (CompareFileNames(destPath, srcPath) == 0)
  {
    RINOK(SendMessageError(callback, "Cannot copy file onto itself", destPath))
    return E_ABORT;
  }

  Int32 writeAskResult;
  CMyComBSTR destPathResult;
  RINOK(callback->AskWrite(
      fs2us(srcPath),
      BoolToInt(false),
      &srcFileInfo.MTime, &srcAltStream.Size,
      fs2us(destPath),
      &destPathResult,
      &writeAskResult))

  if (IntToBool(writeAskResult))
  {
    RINOK(callback->SetCurrentFilePath(fs2us(srcPath)))
    FString destPathNew (us2fs((LPCOLESTR)destPathResult));
    RINOK(state.MyCopyFile(srcPath, destPathNew))
    if (state.ErrorFileIndex >= 0)
    {
      if (state.ErrorMessage.IsEmpty())
        state.ErrorMessage = GetLastErrorMessage();
      FString errorName;
      if (state.ErrorFileIndex == 0)
        errorName = srcPath;
      else
        errorName = destPathNew;
      RINOK(SendMessageError(callback, state.ErrorMessage, errorName))
      return E_ABORT;
    }
    state.StartPos += state.CurrentSize;
  }
  else
  {
    if (state.TotalSize >= srcAltStream.Size)
    {
      state.TotalSize -= srcAltStream.Size;
      RINOK(state.Progress->SetTotal(state.TotalSize))
    }
  }
  return S_OK;
}

Z7_COM7F_IMF(CAltStreamsFolder::CopyTo(Int32 moveMode, const UInt32 *indices, UInt32 numItems,
    Int32 /* includeAltStreams */, Int32 /* replaceAltStreamColon */,
    const wchar_t *path, IFolderOperationsExtractCallback *callback))
{
  if (numItems == 0)
    return S_OK;

  /*
  Z7_DECL_CMyComPtr_QI_FROM(
      IFolderExtractToStreamCallback,
      ExtractToStreamCallback, callback)
  if (ExtractToStreamCallback)
  {
    Int32 useStreams = 0;
    if (ExtractToStreamCallback->UseExtractToStream(&useStreams) != S_OK)
      useStreams = 0;
    if (useStreams == 0)
      ExtractToStreamCallback.Release();
  }
  */

  UInt64 totalSize = 0;
  {
    UInt32 i;
    for (i = 0; i < numItems; i++)
    {
      totalSize += Streams[indices[i]].Size;
    }
    RINOK(callback->SetTotal(totalSize))
    RINOK(callback->SetNumFiles(numItems))
  }

  /*
  if (ExtractToStreamCallback)
  {
    CGetProp *GetProp_Spec = new CGetProp;
    CMyComPtr<IGetProp> GetProp= GetProp_Spec;

    for (UInt32 i = 0; i < numItems; i++)
    {
      UInt32 index = indices[i];
      const CAltStream &ss = Streams[index];
      GetProp_Spec->Name = ss.Name;
      GetProp_Spec->Size = ss.Size;
      CMyComPtr<ISequentialOutStream> outStream;
      RINOK(ExtractToStreamCallback->GetStream7(GetProp_Spec->Name, BoolToInt(false), &outStream,
        NArchive::NExtract::NAskMode::kExtract, GetProp)); // isDir
      FString srcPath;
      GetFullPath(ss, srcPath);
      RINOK(ExtractToStreamCallback->PrepareOperation7(NArchive::NExtract::NAskMode::kExtract));
      RINOK(ExtractToStreamCallback->SetOperationResult7(NArchive::NExtract::NOperationResult::kOK, BoolToInt(false))); // _encrypted
      // RINOK(CopyStream(state, srcPath, fi, ss, destPath2, callback, completedSize));
    }
    return S_OK;
  }
  */

  FString destPath (us2fs(path));
  if (destPath.IsEmpty() /* && !ExtractToStreamCallback */)
    return E_INVALIDARG;

  const bool isAltDest = NName::IsAltPathPrefix(destPath);
  const bool isDirectPath = (!isAltDest && !IsPathSepar(destPath.Back()));

  if (isDirectPath)
  {
    if (numItems > 1)
      return E_INVALIDARG;
  }

  CFileInfo fi;
  if (!fi.Find(_pathBaseFile))
    return GetLastError_noZero_HRESULT();

  NFsFolder::CCopyStateIO state;
  state.Progress = callback;
  state.DeleteSrcFile = IntToBool(moveMode);
  state.TotalSize = totalSize;

  for (UInt32 i = 0; i < numItems; i++)
  {
    const UInt32 index = indices[i];
    const CAltStream &ss = Streams[index];
    FString destPath2 = destPath;
    if (!isDirectPath)
      destPath2 += us2fs(Get_Correct_FsFile_Name(ss.Name));
    FString srcPath;
    GetFullPath(ss, srcPath);
    RINOK(CopyStream(state, srcPath, fi, ss, destPath2, callback))
  }

  return S_OK;
}

Z7_COM7F_IMF(CAltStreamsFolder::CopyFrom(Int32 /* moveMode */, const wchar_t * /* fromFolderPath */,
    const wchar_t * const * /* itemsPaths */, UInt32 /* numItems */, IProgress * /* progress */))
{
  /*
  if (numItems == 0)
    return S_OK;

  CMyComPtr<IFolderArchiveUpdateCallback> callback;
  if (progress)
  {
    RINOK(progress->QueryInterface(IID_IFolderArchiveUpdateCallback, (void **)&callback));
  }

  if (CompareFileNames(fromFolderPath, fs2us(_pathPrefix)) == 0)
  {
    RINOK(SendMessageError(callback, "Cannot copy file onto itself", _pathPrefix));
    return E_ABORT;
  }

  if (callback)
    RINOK(callback->SetNumFiles(numItems));

  UInt64 totalSize = 0;

  UInt32 i;

  FString path;
  for (i = 0; i < numItems; i++)
  {
    path = us2fs(fromFolderPath);
    path += us2fs(itemsPaths[i]);

    CFileInfo fi;
    if (!fi.Find(path))
      return ::GetLastError();
    if (fi.IsDir())
      return E_NOTIMPL;
    totalSize += fi.Size;
  }

  RINOK(progress->SetTotal(totalSize));

  // UInt64 completedSize = 0;

  NFsFolder::CCopyStateIO state;
  state.Progress = progress;
  state.DeleteSrcFile = IntToBool(moveMode);
  state.TotalSize = totalSize;

  // we need to clear READ-ONLY of parent before creating alt stream
  {
    DWORD attrib = GetFileAttrib(_pathBaseFile);
    if (attrib != INVALID_FILE_ATTRIBUTES
        && (attrib & FILE_ATTRIBUTE_READONLY) != 0)
    {
      if (!SetFileAttrib(_pathBaseFile, attrib & ~FILE_ATTRIBUTE_READONLY))
      {
        if (callback)
        {
          RINOK(SendMessageError(callback, GetLastErrorMessage(), _pathBaseFile));
          return S_OK;
        }
        return Return_LastError_or_FAIL();
      }
    }
  }

  for (i = 0; i < numItems; i++)
  {
    path = us2fs(fromFolderPath);
    path += us2fs(itemsPaths[i]);

    FString destPath = _pathPrefix + us2fs(itemsPaths[i]);

    RINOK(UpdateFile(state, path, destPath, callback));
  }

  return S_OK;
  */
  return E_NOTIMPL;
}

Z7_COM7F_IMF(CAltStreamsFolder::CopyFromFile(UInt32 /* index */, const wchar_t * /* fullFilePath */, IProgress * /* progress */))
{
  return E_NOTIMPL;
}

}

// FSFolderCopy.cpp

#include "StdAfx.h"

#include "../../../Common/MyWindows.h"

#include <WinBase.h>

#include "../../../Common/Defs.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/Wildcard.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileName.h"

#include "../../Common/FilePathAutoRename.h"

#include "FSFolder.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;
using namespace NName;
using namespace NFind;

#ifndef _UNICODE
extern bool g_IsNT;
#endif

namespace NFsFolder {

HRESULT CCopyStateIO::MyCopyFile(CFSTR inPath, CFSTR outPath, DWORD attrib)
{
  ErrorFileIndex = -1;
  ErrorMessage.Empty();
  CurrentSize = 0;

  {
    const size_t kBufSize = 1 << 16;
    CByteArr buf(kBufSize);
    
    NIO::CInFile inFile;
    NIO::COutFile outFile;
    
    if (!inFile.Open(inPath))
    {
      ErrorFileIndex = 0;
      return S_OK;
    }
    
    if (!outFile.Create(outPath, true))
    {
      ErrorFileIndex = 1;
      return S_OK;
    }
    
    for (;;)
    {
      UInt32 num;
      if (!inFile.Read(buf, kBufSize, num))
      {
        ErrorFileIndex = 0;
        return S_OK;
      }
      if (num == 0)
        break;
      
      UInt32 written = 0;
      if (!outFile.Write(buf, num, written))
      {
        ErrorFileIndex = 1;
        return S_OK;
      }
      if (written != num)
      {
        ErrorMessage = "Write error";
        return S_OK;
      }
      CurrentSize += num;
      if (Progress)
      {
        UInt64 completed = StartPos + CurrentSize;
        RINOK(Progress->SetCompleted(&completed));
      }
    }
  }

  if (attrib != INVALID_FILE_ATTRIBUTES)
    SetFileAttrib(outPath, attrib);

  if (DeleteSrcFile)
  {
    if (!DeleteFileAlways(inPath))
    {
      ErrorFileIndex = 0;
      return S_OK;
    }
  }
  
  return S_OK;
}


/*
static bool IsItWindows2000orHigher()
{
  OSVERSIONINFO versionInfo;
  versionInfo.dwOSVersionInfoSize = sizeof(versionInfo);
  if (!::GetVersionEx(&versionInfo))
    return false;
  return (versionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) &&
      (versionInfo.dwMajorVersion >= 5);
}
*/

struct CProgressInfo
{
  UInt64 TotalSize;
  UInt64 StartPos;
  UInt64 FileSize;
  IProgress *Progress;
  HRESULT ProgressResult;

  void Init() { ProgressResult = S_OK; }
};

#ifndef PROGRESS_CONTINUE

#define PROGRESS_CONTINUE 0
#define PROGRESS_CANCEL 1

#define COPY_FILE_FAIL_IF_EXISTS 0x00000001

typedef
DWORD
(WINAPI* LPPROGRESS_ROUTINE)(
    LARGE_INTEGER TotalFileSize,
    LARGE_INTEGER TotalBytesTransferred,
    LARGE_INTEGER StreamSize,
    LARGE_INTEGER StreamBytesTransferred,
    DWORD dwStreamNumber,
    DWORD dwCallbackReason,
    HANDLE hSourceFile,
    HANDLE hDestinationFile,
    LPVOID lpData
    );

#endif

static DWORD CALLBACK CopyProgressRoutine(
  LARGE_INTEGER TotalFileSize,          // file size
  LARGE_INTEGER TotalBytesTransferred,  // bytes transferred
  LARGE_INTEGER /* StreamSize */,             // bytes in stream
  LARGE_INTEGER /* StreamBytesTransferred */, // bytes transferred for stream
  DWORD /* dwStreamNumber */,                 // current stream
  DWORD /* dwCallbackReason */,               // callback reason
  HANDLE /* hSourceFile */,                   // handle to source file
  HANDLE /* hDestinationFile */,              // handle to destination file
  LPVOID lpData                         // from CopyFileEx
)
{
  // StreamSize = StreamSize;
  // StreamBytesTransferred = StreamBytesTransferred;
  // dwStreamNumber = dwStreamNumber;
  // dwCallbackReason = dwCallbackReason;

  CProgressInfo &pi = *(CProgressInfo *)lpData;

  if ((UInt64)TotalFileSize.QuadPart > pi.FileSize)
  {
    pi.TotalSize += (UInt64)TotalFileSize.QuadPart - pi.FileSize;
    pi.FileSize = (UInt64)TotalFileSize.QuadPart;
    pi.ProgressResult = pi.Progress->SetTotal(pi.TotalSize);
  }
  UInt64 completed = pi.StartPos + TotalBytesTransferred.QuadPart;
  pi.ProgressResult = pi.Progress->SetCompleted(&completed);
  return (pi.ProgressResult == S_OK ? PROGRESS_CONTINUE : PROGRESS_CANCEL);
}

typedef BOOL (WINAPI * Func_CopyFileExA)(
    IN LPCSTR lpExistingFileName,
    IN LPCSTR lpNewFileName,
    IN LPPROGRESS_ROUTINE lpProgressRoutine OPTIONAL,
    IN LPVOID lpData OPTIONAL,
    IN LPBOOL pbCancel OPTIONAL,
    IN DWORD dwCopyFlags
    );

typedef BOOL (WINAPI * Func_CopyFileExW)(
    IN LPCWSTR lpExistingFileName,
    IN LPCWSTR lpNewFileName,
    IN LPPROGRESS_ROUTINE lpProgressRoutine OPTIONAL,
    IN LPVOID lpData OPTIONAL,
    IN LPBOOL pbCancel OPTIONAL,
    IN DWORD dwCopyFlags
    );

typedef BOOL (WINAPI * Func_MoveFileWithProgressW)(
    IN LPCWSTR lpExistingFileName,
    IN LPCWSTR lpNewFileName,
    IN LPPROGRESS_ROUTINE lpProgressRoutine OPTIONAL,
    IN LPVOID lpData OPTIONAL,
    IN DWORD dwFlags
    );

struct CCopyState
{
  CProgressInfo ProgressInfo;
  IFolderOperationsExtractCallback *Callback;
  bool MoveMode;
  bool UseReadWriteMode;

  Func_CopyFileExW my_CopyFileExW;
  #ifndef UNDER_CE
  Func_MoveFileWithProgressW my_MoveFileWithProgressW;
  #endif
  #ifndef _UNICODE
  Func_CopyFileExA my_CopyFileExA;
  #endif

  void Prepare();
  bool CopyFile_NT(const wchar_t *oldFile, const wchar_t *newFile);
  bool CopyFile_Sys(CFSTR oldFile, CFSTR newFile);
  bool MoveFile_Sys(CFSTR oldFile, CFSTR newFile);

  HRESULT CallProgress();

  bool IsCallbackProgressError() { return ProgressInfo.ProgressResult != S_OK; }
};

HRESULT CCopyState::CallProgress()
{
  return ProgressInfo.Progress->SetCompleted(&ProgressInfo.StartPos);
}

void CCopyState::Prepare()
{
  my_CopyFileExW = NULL;
  #ifndef UNDER_CE
  my_MoveFileWithProgressW = NULL;
  #endif
  #ifndef _UNICODE
  my_CopyFileExA = NULL;
  if (!g_IsNT)
  {
    my_CopyFileExA = (Func_CopyFileExA)::GetProcAddress(::GetModuleHandleA("kernel32.dll"), "CopyFileExA");
  }
  else
  #endif
  {
    HMODULE module = ::GetModuleHandleW(
      #ifdef UNDER_CE
        L"coredll.dll"
      #else
        L"kernel32.dll"
      #endif
        );
    my_CopyFileExW = (Func_CopyFileExW)My_GetProcAddress(module, "CopyFileExW");
    #ifndef UNDER_CE
    my_MoveFileWithProgressW = (Func_MoveFileWithProgressW)My_GetProcAddress(module, "MoveFileWithProgressW");
    #endif
  }
}

/* WinXP-64:
  CopyFileW(fromFile, toFile:altStream)
    OK                       - there are NO alt streams in fromFile
    ERROR_INVALID_PARAMETER  - there are    alt streams in fromFile
*/

bool CCopyState::CopyFile_NT(const wchar_t *oldFile, const wchar_t *newFile)
{
  BOOL cancelFlag = FALSE;
  if (my_CopyFileExW)
    return BOOLToBool(my_CopyFileExW(oldFile, newFile, CopyProgressRoutine,
        &ProgressInfo, &cancelFlag, COPY_FILE_FAIL_IF_EXISTS));
  return BOOLToBool(::CopyFileW(oldFile, newFile, TRUE));
}

bool CCopyState::CopyFile_Sys(CFSTR oldFile, CFSTR newFile)
{
  #ifndef _UNICODE
  if (!g_IsNT)
  {
    if (my_CopyFileExA)
    {
      BOOL cancelFlag = FALSE;
      if (my_CopyFileExA(fs2fas(oldFile), fs2fas(newFile),
          CopyProgressRoutine, &ProgressInfo, &cancelFlag, COPY_FILE_FAIL_IF_EXISTS))
        return true;
      if (::GetLastError() != ERROR_CALL_NOT_IMPLEMENTED)
        return false;
    }
    return BOOLToBool(::CopyFile(fs2fas(oldFile), fs2fas(newFile), TRUE));
  }
  else
  #endif
  {
    IF_USE_MAIN_PATH_2(oldFile, newFile)
    {
      if (CopyFile_NT(fs2us(oldFile), fs2us(newFile)))
        return true;
    }
    #ifdef WIN_LONG_PATH
    if (USE_SUPER_PATH_2)
    {
      if (IsCallbackProgressError())
        return false;
      UString superPathOld, superPathNew;
      if (!GetSuperPaths(oldFile, newFile, superPathOld, superPathNew, USE_MAIN_PATH_2))
        return false;
      if (CopyFile_NT(superPathOld, superPathNew))
        return true;
    }
    #endif
    return false;
  }
}

bool CCopyState::MoveFile_Sys(CFSTR oldFile, CFSTR newFile)
{
  #ifndef UNDER_CE
  // if (IsItWindows2000orHigher())
  // {
    if (my_MoveFileWithProgressW)
    {
      IF_USE_MAIN_PATH_2(oldFile, newFile)
      {
        if (my_MoveFileWithProgressW(fs2us(oldFile), fs2us(newFile), CopyProgressRoutine,
            &ProgressInfo, MOVEFILE_COPY_ALLOWED))
          return true;
      }
      #ifdef WIN_LONG_PATH
      if ((!(USE_MAIN_PATH_2) || ::GetLastError() != ERROR_CALL_NOT_IMPLEMENTED) && USE_SUPER_PATH_2)
      {
        if (IsCallbackProgressError())
          return false;
        UString superPathOld, superPathNew;
        if (!GetSuperPaths(oldFile, newFile, superPathOld, superPathNew, USE_MAIN_PATH_2))
          return false;
        if (my_MoveFileWithProgressW(superPathOld, superPathNew, CopyProgressRoutine,
            &ProgressInfo, MOVEFILE_COPY_ALLOWED))
          return true;
      }
      #endif
      if (::GetLastError() != ERROR_CALL_NOT_IMPLEMENTED)
        return false;
    }
  // }
  // else
  #endif
    return MyMoveFile(oldFile, newFile);
}

static HRESULT SendMessageError(IFolderOperationsExtractCallback *callback,
    const wchar_t *message, const FString &fileName)
{
  UString s = message;
  s += " : ";
  s += fs2us(fileName);
  return callback->ShowMessage(s);
}

static HRESULT SendMessageError(IFolderOperationsExtractCallback *callback,
    const char *message, const FString &fileName)
{
  return SendMessageError(callback, MultiByteToUnicodeString(message), fileName);
}

static DWORD Return_LastError_or_FAIL()
{
  DWORD errorCode = GetLastError();
  if (errorCode == 0)
    errorCode = (DWORD)E_FAIL;
  return errorCode;
}

static UString GetLastErrorMessage()
{
  return NError::MyFormatMessage(Return_LastError_or_FAIL());
}

HRESULT SendLastErrorMessage(IFolderOperationsExtractCallback *callback, const FString &fileName)
{
  return SendMessageError(callback, GetLastErrorMessage(), fileName);
}

static HRESULT CopyFile_Ask(
    CCopyState &state,
    const FString &srcPath,
    const CFileInfo &srcFileInfo,
    const FString &destPath)
{
  if (CompareFileNames(destPath, srcPath) == 0)
  {
    RINOK(SendMessageError(state.Callback,
        state.MoveMode ?
          "Cannot move file onto itself" :
          "Cannot copy file onto itself"
        , destPath));
    return E_ABORT;
  }

  Int32 writeAskResult;
  CMyComBSTR destPathResult;
  RINOK(state.Callback->AskWrite(
      fs2us(srcPath),
      BoolToInt(false),
      &srcFileInfo.MTime, &srcFileInfo.Size,
      fs2us(destPath),
      &destPathResult,
      &writeAskResult));
  
  if (IntToBool(writeAskResult))
  {
    FString destPathNew = us2fs((LPCOLESTR)destPathResult);
    RINOK(state.Callback->SetCurrentFilePath(fs2us(srcPath)));

    if (state.UseReadWriteMode)
    {
      NFsFolder::CCopyStateIO state2;
      state2.Progress = state.Callback;
      state2.DeleteSrcFile = state.MoveMode;
      state2.TotalSize = state.ProgressInfo.TotalSize;
      state2.StartPos = state.ProgressInfo.StartPos;

      RINOK(state2.MyCopyFile(srcPath, destPathNew, srcFileInfo.Attrib));
      
      if (state2.ErrorFileIndex >= 0)
      {
        if (state2.ErrorMessage.IsEmpty())
          state2.ErrorMessage = GetLastErrorMessage();
        FString errorName;
        if (state2.ErrorFileIndex == 0)
          errorName = srcPath;
        else
          errorName = destPathNew;
        RINOK(SendMessageError(state.Callback, state2.ErrorMessage, errorName));
        return E_ABORT;
      }
      state.ProgressInfo.StartPos += state2.CurrentSize;
    }
    else
    {
      state.ProgressInfo.FileSize = srcFileInfo.Size;
      bool res;
      if (state.MoveMode)
        res = state.MoveFile_Sys(srcPath, destPathNew);
      else
        res = state.CopyFile_Sys(srcPath, destPathNew);
      RINOK(state.ProgressInfo.ProgressResult);
      if (!res)
      {
        // GetLastError() is ERROR_REQUEST_ABORTED in case of PROGRESS_CANCEL.
        RINOK(SendMessageError(state.Callback, GetLastErrorMessage(), destPathNew));
        return E_ABORT;
      }
      state.ProgressInfo.StartPos += state.ProgressInfo.FileSize;
    }
  }
  else
  {
    if (state.ProgressInfo.TotalSize >= srcFileInfo.Size)
    {
      state.ProgressInfo.TotalSize -= srcFileInfo.Size;
      RINOK(state.ProgressInfo.Progress->SetTotal(state.ProgressInfo.TotalSize));
    }
  }
  return state.CallProgress();
}

static FString CombinePath(const FString &folderPath, const FString &fileName)
{
  FString s (folderPath);
  s.Add_PathSepar(); // FCHAR_PATH_SEPARATOR
  s += fileName;
  return s;
}

static bool IsDestChild(const FString &src, const FString &dest)
{
  unsigned len = src.Len();
  if (dest.Len() < len)
    return false;
  if (dest.Len() != len && dest[len] != FCHAR_PATH_SEPARATOR)
    return false;
  return CompareFileNames(dest.Left(len), src) == 0;
}

static HRESULT CopyFolder(
    CCopyState &state,
    const FString &srcPath,   // without TAIL separator
    const FString &destPath)  // without TAIL separator
{
  RINOK(state.CallProgress());

  if (IsDestChild(srcPath, destPath))
  {
    RINOK(SendMessageError(state.Callback,
        state.MoveMode ?
          "Cannot copy folder onto itself" :
          "Cannot move folder onto itself"
        , destPath));
    return E_ABORT;
  }

  if (state.MoveMode)
  {
    if (state.MoveFile_Sys(srcPath, destPath))
      return S_OK;

    // MSDN: MoveFile() fails for dirs on different volumes.
  }

  if (!CreateComplexDir(destPath))
  {
    RINOK(SendMessageError(state.Callback, "Cannot create folder", destPath));
    return E_ABORT;
  }

  CEnumerator enumerator;
  enumerator.SetDirPrefix(CombinePath(srcPath, FString()));
  
  for (;;)
  {
    NFind::CFileInfo fi;
    bool found;
    if (!enumerator.Next(fi, found))
    {
      SendLastErrorMessage(state.Callback, srcPath);
      return S_OK;
    }
    if (!found)
      break;
    const FString srcPath2 = CombinePath(srcPath, fi.Name);
    const FString destPath2 = CombinePath(destPath, fi.Name);
    if (fi.IsDir())
    {
      RINOK(CopyFolder(state, srcPath2, destPath2))
    }
    else
    {
      RINOK(CopyFile_Ask(state, srcPath2, fi, destPath2));
    }
  }

  if (state.MoveMode)
  {
    if (!RemoveDir(srcPath))
    {
      RINOK(SendMessageError(state.Callback, "Cannot remove folder", srcPath));
      return E_ABORT;
    }
  }
  
  return S_OK;
}

STDMETHODIMP CFSFolder::CopyTo(Int32 moveMode, const UInt32 *indices, UInt32 numItems,
    Int32 /* includeAltStreams */, Int32 /* replaceAltStreamColon */,
    const wchar_t *path, IFolderOperationsExtractCallback *callback)
{
  if (numItems == 0)
    return S_OK;

  FString destPath = us2fs(path);
  if (destPath.IsEmpty())
    return E_INVALIDARG;

  bool isAltDest = NName::IsAltPathPrefix(destPath);
  bool isDirectPath = (!isAltDest && !IsPathSepar(destPath.Back()));

  if (isDirectPath)
  {
    if (numItems > 1)
      return E_INVALIDARG;
  }

  CFsFolderStat stat;
  stat.Progress = callback;
  RINOK(GetItemsFullSize(indices, numItems, stat));

  if (stat.NumFolders != 0 && isAltDest)
    return E_NOTIMPL;

  RINOK(callback->SetTotal(stat.Size));
  RINOK(callback->SetNumFiles(stat.NumFiles));

  UInt64 completedSize = 0;
  RINOK(callback->SetCompleted(&completedSize));

  CCopyState state;
  state.ProgressInfo.TotalSize = stat.Size;
  state.ProgressInfo.StartPos = 0;
  state.ProgressInfo.Progress = callback;
  state.ProgressInfo.Init();
  state.Callback = callback;
  state.MoveMode = IntToBool(moveMode);
  state.UseReadWriteMode = isAltDest;
  state.Prepare();

  for (UInt32 i = 0; i < numItems; i++)
  {
    UInt32 index = indices[i];
    if (index >= (UInt32)Files.Size())
      continue;
    const CDirItem &fi = Files[index];
    FString destPath2 = destPath;
    if (!isDirectPath)
      destPath2 += fi.Name;
    FString srcPath;
    GetFullPath(fi, srcPath);
  
    if (fi.IsDir())
    {
      RINOK(CopyFolder(state, srcPath, destPath2));
    }
    else
    {
      RINOK(CopyFile_Ask(state, srcPath, fi, destPath2));
    }
  }
  return S_OK;
}

STDMETHODIMP CFSFolder::CopyFrom(Int32 /* moveMode */, const wchar_t * /* fromFolderPath */,
    const wchar_t * const * /* itemsPaths */, UInt32 /* numItems */, IProgress * /* progress */)
{
  /*
  UInt64 numFolders, numFiles, totalSize;
  numFiles = numFolders = totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    UString path = (UString)fromFolderPath + itemsPaths[i];

    CFileInfo fi;
    if (!FindFile(path, fi))
      return ::GetLastError();
    if (fi.IsDir())
    {
      UInt64 subFolders, subFiles, subSize;
      RINOK(GetFolderSize(CombinePath(path, fi.Name), subFolders, subFiles, subSize, progress));
      numFolders += subFolders;
      numFolders++;
      numFiles += subFiles;
      totalSize += subSize;
    }
    else
    {
      numFiles++;
      totalSize += fi.Size;
    }
  }
  RINOK(progress->SetTotal(totalSize));
  RINOK(callback->SetNumFiles(numFiles));
  for (i = 0; i < numItems; i++)
  {
    UString path = (UString)fromFolderPath + itemsPaths[i];
  }
  return S_OK;
  */
  return E_NOTIMPL;
}

STDMETHODIMP CFSFolder::CopyFromFile(UInt32 /* index */, const wchar_t * /* fullFilePath */, IProgress * /* progress */)
{
  return E_NOTIMPL;
}

}

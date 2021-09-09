// WorkDir.cpp

#include "StdAfx.h"

#include "../../../Common/StringConvert.h"
#include "../../../Common/Wildcard.h"

#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileName.h"

#include "WorkDir.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;

FString GetWorkDir(const NWorkDir::CInfo &workDirInfo, const FString &path, FString &fileName)
{
  NWorkDir::NMode::EEnum mode = workDirInfo.Mode;
  
  #if defined(_WIN32) && !defined(UNDER_CE)
  if (workDirInfo.ForRemovableOnly)
  {
    mode = NWorkDir::NMode::kCurrent;
    FString prefix = path.Left(3);
    if (prefix[1] == FTEXT(':') && prefix[2] == FTEXT('\\'))
    {
      UINT driveType = GetDriveType(GetSystemString(prefix, ::AreFileApisANSI() ? CP_ACP : CP_OEMCP));
      if (driveType == DRIVE_CDROM || driveType == DRIVE_REMOVABLE)
        mode = workDirInfo.Mode;
    }
    /*
    CParsedPath parsedPath;
    parsedPath.ParsePath(archiveName);
    UINT driveType = GetDriveType(parsedPath.Prefix);
    if ((driveType != DRIVE_CDROM) && (driveType != DRIVE_REMOVABLE))
      mode = NZipSettings::NWorkDir::NMode::kCurrent;
    */
  }
  #endif
  
  int pos = path.ReverseFind_PathSepar() + 1;
  fileName = path.Ptr((unsigned)pos);
  
  switch (mode)
  {
    case NWorkDir::NMode::kCurrent:
    {
      return path.Left((unsigned)pos);
    }
    case NWorkDir::NMode::kSpecified:
    {
      FString tempDir = workDirInfo.Path;
      NName::NormalizeDirPathPrefix(tempDir);
      return tempDir;
    }
    default:
    {
      FString tempDir;
      if (!MyGetTempPath(tempDir))
        throw 141717;
      return tempDir;
    }
  }
}

HRESULT CWorkDirTempFile::CreateTempFile(const FString &originalPath)
{
  NWorkDir::CInfo workDirInfo;
  workDirInfo.Load();
  FString namePart;
  FString workDir = GetWorkDir(workDirInfo, originalPath, namePart);
  CreateComplexDir(workDir);
  CTempFile tempFile;
  _outStreamSpec = new COutFileStream;
  OutStream = _outStreamSpec;
  if (!_tempFile.Create(workDir + namePart, &_outStreamSpec->File))
  {
    return GetLastError_noZero_HRESULT();
  }
  _originalPath = originalPath;
  return S_OK;
}

HRESULT CWorkDirTempFile::MoveToOriginal(bool deleteOriginal)
{
  OutStream.Release();
  if (!_tempFile.MoveTo(_originalPath, deleteOriginal))
  {
    return GetLastError_noZero_HRESULT();
  }
  return S_OK;
}

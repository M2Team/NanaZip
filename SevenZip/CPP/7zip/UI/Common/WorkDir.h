// WorkDir.h

#ifndef __WORK_DIR_H
#define __WORK_DIR_H

#include "../../../../../ThirdParty/LZMA/CPP/Windows/FileDir.h"

#include "../../../../../ThirdParty/LZMA/CPP/7zip/Common/FileStreams.h"

#include "ZipRegistry.h"

FString GetWorkDir(const NWorkDir::CInfo &workDirInfo, const FString &path, FString &fileName);

class CWorkDirTempFile
{
  FString _originalPath;
  NWindows::NFile::NDir::CTempFile _tempFile;
  COutFileStream *_outStreamSpec;
public:
  CMyComPtr<IOutStream> OutStream;

  HRESULT CreateTempFile(const FString &originalPath);
  HRESULT MoveToOriginal(bool deleteOriginal);
};

#endif

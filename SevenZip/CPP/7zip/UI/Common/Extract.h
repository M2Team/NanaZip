// Extract.h

#ifndef __EXTRACT_H
#define __EXTRACT_H

#include "../../../Windows/FileFind.h"

#include "../../Archive/IArchive.h"

#include "ArchiveExtractCallback.h"
#include "ArchiveOpenCallback.h"
#include "ExtractMode.h"
#include "Property.h"

#include "../Common/LoadCodecs.h"

struct CExtractOptionsBase
{
  CBoolPair ElimDup;

  bool ExcludeDirItems;
  bool ExcludeFileItems;

  bool PathMode_Force;
  bool OverwriteMode_Force;
  NExtract::NPathMode::EEnum PathMode;
  NExtract::NOverwriteMode::EEnum OverwriteMode;
  
  FString OutputDir;
  CExtractNtOptions NtOptions;
  UString HashDir;

  CExtractOptionsBase():
      ExcludeDirItems(false),
      ExcludeFileItems(false),
      PathMode_Force(false),
      OverwriteMode_Force(false),
      PathMode(NExtract::NPathMode::kFullPaths),
      OverwriteMode(NExtract::NOverwriteMode::kAsk)
      {}
};

struct CExtractOptions: public CExtractOptionsBase
{
  bool StdInMode;
  bool StdOutMode;
  bool YesToAll;
  bool TestMode;
  
  // bool ShowDialog;
  // bool PasswordEnabled;
  // UString Password;
  #ifndef _SFX
  CObjectVector<CProperty> Properties;
  #endif

  /*
  #ifdef EXTERNAL_CODECS
  CCodecs *Codecs;
  #endif
  */

  CExtractOptions():
      StdInMode(false),
      StdOutMode(false),
      YesToAll(false),
      TestMode(false)
      {}
};

struct CDecompressStat
{
  UInt64 NumArchives;
  UInt64 UnpackSize;
  UInt64 AltStreams_UnpackSize;
  UInt64 PackSize;
  UInt64 NumFolders;
  UInt64 NumFiles;
  UInt64 NumAltStreams;

  void Clear()
  {
    NumArchives = UnpackSize = AltStreams_UnpackSize = PackSize = NumFolders = NumFiles = NumAltStreams = 0;
  }
};

HRESULT Extract(
    // DECL_EXTERNAL_CODECS_LOC_VARS
    CCodecs *codecs,
    const CObjectVector<COpenType> &types,
    const CIntVector &excludedFormats,
    UStringVector &archivePaths, UStringVector &archivePathsFull,
    const NWildcard::CCensorNode &wildcardCensor,
    const CExtractOptions &options,
    IOpenCallbackUI *openCallback,
    IExtractCallbackUI *extractCallback,
    #ifndef _SFX
    IHashCalc *hash,
    #endif
    UString &errorMessage,
    CDecompressStat &st);

#endif

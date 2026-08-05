// Extract.h

#ifndef ZIP7_INC_EXTRACT_H
#define ZIP7_INC_EXTRACT_H

#include "../../../Windows/FileFind.h"

#include "../../Archive/IArchive.h"

#include "ArchiveExtractCallback.h"
#include "ArchiveOpenCallback.h"
#include "ExtractMode.h"
#include "Property.h"

#include "../Common/LoadCodecs.h"

namespace NExtractOutDirMode {
enum EEnum
{
  k_Direct = 0,
  k_AddArcName,
  k_ReplaceAsterisk
};
}

struct CExtractOptionsBase
{
  CBoolPair ElimDup;
  // **************** NanaZip Modification Start ****************
  CBoolPair SmartExtract;
  // **************** NanaZip Modification End ****************

  bool ExcludeDirItems;
  bool ExcludeFileItems;

  bool PathMode_Force;
  bool OverwriteMode_Force;
  NExtract::NPathMode::EEnum PathMode;
  NExtract::NOverwriteMode::EEnum OverwriteMode;
  NExtract::NZoneIdMode::EEnum ZoneMode;
  NExtractOutDirMode::EEnum OutDirMode;

  CExtractNtOptions NtOptions;
  
  FString OutputDir; // normalized : with path separator at the end
  UString HashDir;

  CExtractOptionsBase():
      ExcludeDirItems(false),
      ExcludeFileItems(false),
      PathMode_Force(false),
      OverwriteMode_Force(false),
      PathMode(NExtract::NPathMode::kFullPaths),
      OverwriteMode(NExtract::NOverwriteMode::kAsk),
      // **************** NanaZip Modification Start ****************
      // ZoneMode(NExtract::NZoneIdMode::kNone),
      ZoneMode(NExtract::NZoneIdMode::Default),
      // **************** NanaZip Modification End ****************
      OutDirMode(NExtractOutDirMode::k_ReplaceAsterisk)
      {}
};

struct CExtractOptions: public CExtractOptionsBase
{
  bool StdInMode;
  bool StdOutMode;
  bool YesToAll;
  bool TestMode;
  // **************** NanaZip Modification Start ****************
  CBoolPair OpenFolder;
  CBoolPair DeleteArchive;
  // **************** NanaZip Modification End ****************
  
  // bool ShowDialog;
  // bool PasswordEnabled;
  // UString Password;
  #ifndef Z7_SFX
  CObjectVector<CProperty> Properties;
  #endif

  /*
  #ifdef Z7_EXTERNAL_CODECS
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
  // **************** 7-Zip ZS Modification Start ****************
  FString FirstExtractedPath;
  // **************** 7-Zip ZS Modification End ****************
  // **************** NanaZip Modification Start ****************
  FString OutDir;
  /* Archives that were opened and decompressed without a fatal error, including
     every volume of a multi-volume archive. It is only filled when the caller
     asks for the source archives to be deleted, because the caller also needs
     its own error counters to decide whether deleting them is safe. */
  FStringVector ExtractedArcPaths;
  // **************** NanaZip Modification End ****************

  void Clear()
  {
    NumArchives = UnpackSize = AltStreams_UnpackSize = PackSize = NumFolders = NumFiles = NumAltStreams = 0;
    // **************** 7-Zip ZS Modification Start ****************
    FirstExtractedPath.Empty();
    // **************** 7-Zip ZS Modification End ****************
    // **************** NanaZip Modification Start ****************
    OutDir.Empty();
    ExtractedArcPaths.Clear();
    // **************** NanaZip Modification End ****************
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
    IFolderArchiveExtractCallback *faeCallback,
    #ifndef Z7_SFX
    IHashCalc *hash,
    #endif
    UString &errorMessage,
    CDecompressStat &st);

// **************** NanaZip Modification Start ****************
/* Removes a source archive after it has been extracted. The Recycle Bin is
   preferred because the operation is destructive and the user is not asked to
   confirm it, but the shell refuses paths that are longer than MAX_PATH and
   volumes such as network shares have no Recycle Bin at all, so a permanent
   delete is used as the fallback instead of silently keeping the archive. */
void DeleteExtractedArchives(const FStringVector &paths);
// **************** NanaZip Modification End ****************

#endif

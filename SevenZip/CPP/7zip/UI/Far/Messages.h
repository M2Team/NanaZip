// Far/Messages.h

#ifndef __7ZIP_FAR_MESSAGES_H
#define __7ZIP_FAR_MESSAGES_H

#include "../../PropID.h"

namespace NMessageID {

const unsigned k_Last_PropId_supported_by_plugin = kpidCopyLink;

enum EEnum
{
  kOk,
  kCancel,

  kWarning,
  kError,

  kArchiveType,

  kProperties,

  kYes,
  kNo,
  
  kGetPasswordTitle,
  kEnterPasswordForFile,

  kExtractTitle,
  kExtractTo,

  kExtractPathMode,
  kExtractPathFull,
  kExtractPathCurrent,
  kExtractPathNo,

  kExtractOwerwriteMode,
  kExtractOwerwriteAsk,
  kExtractOwerwritePrompt,
  kExtractOwerwriteSkip,
  kExtractOwerwriteAutoRename,
  kExtractOwerwriteAutoRenameExisting,

  kExtractFilesMode,
  kExtractFilesSelected,
  kExtractFilesAll,

  kExtractPassword,

  kExtractExtract,
  kExtractCancel,

  kExtractCanNotOpenOutputFile,

  kExtractUnsupportedMethod,
  kExtractCRCFailed,
  kExtractDataError,
  kExtractCRCFailedEncrypted,
  kExtractDataErrorEncrypted,

  kOverwriteTitle,
  kOverwriteMessage1,
  kOverwriteMessageWouldYouLike,
  kOverwriteMessageWithtTisOne,

  kOverwriteBytes,
  kOverwriteModifiedOn,

  kOverwriteYes,
  kOverwriteYesToAll,
  kOverwriteNo,
  kOverwriteNoToAll,
  kOverwriteAutoRename,
  kOverwriteCancel,

  kUpdateNotSupportedForThisArchive,

  kDeleteTitle,
  kDeleteFile,
  kDeleteFiles,
  kDeleteNumberOfFiles,
  kDeleteDelete,
  kDeleteCancel,

  kUpdateTitle,
  kUpdateAddToArchive,

  kUpdateMethod,
  kUpdateMethod_Store,
  kUpdateMethod_Fastest,
  kUpdateMethod_Fast,
  kUpdateMethod_Normal,
  kUpdateMethod_Maximum,
  kUpdateMethod_Ultra,

  kUpdateMode,
  kUpdateMode_Add,
  kUpdateMode_Update,
  kUpdateMode_Fresh,
  kUpdateMode_Sync,

  kUpdateAdd,
  kUpdateSelectArchiver,

  kUpdateSelectArchiverMenuTitle,

  // kArcReadFiles,
  
  kWaitTitle,
  
  kReading,
  kExtracting,
  kDeleting,
  kUpdating,
  
  // kReadingList,

  kMoveIsNotSupported,

  kOpenArchiveMenuString,
  kCreateArchiveMenuString,

  kConfigTitle,
  
  kConfigPluginEnabled,

  // ---------- IDs for Properies (kpid*) ----------
  kNoProperty,
  k_Last_MessageID_for_Property = kNoProperty + k_Last_PropId_supported_by_plugin
  // ----------
};

}

#endif

// IFolderArchive.h

#ifndef ZIP7_INC_IFOLDER_ARCHIVE_H
#define ZIP7_INC_IFOLDER_ARCHIVE_H

#include "../../../Common/MyString.h"

#include "../../Archive/IArchive.h"
#include "../../UI/Common/LoadCodecs.h"
#include "../../UI/FileManager/IFolder.h"

#include "../Common/ExtractMode.h"
#include "../Common/IFileExtractCallback.h"

Z7_PURE_INTERFACES_BEGIN

/* ---------- IArchiveFolder ----------
IArchiveFolder is implemented by CAgentFolder (Agent/Agent.h)
IArchiveFolder is used by:
  - FileManager/PanelCopy.cpp
      CPanel::CopyTo(), if (options->testMode)
  - FAR/PluginRead.cpp
      CPlugin::ExtractFiles
*/

#define Z7_IFACEM_IArchiveFolder(x) \
  x(Extract(const UInt32 *indices, UInt32 numItems, \
      Int32 includeAltStreams, \
      Int32 replaceAltStreamCharsMode, \
      NExtract::NPathMode::EEnum pathMode, \
      NExtract::NOverwriteMode::EEnum overwriteMode, \
      const wchar_t *path, Int32 testMode, \
      IFolderArchiveExtractCallback *extractCallback2)) \

Z7_IFACE_CONSTR_FOLDERARC(IArchiveFolder, 0x0D)


/* ---------- IInFolderArchive ----------
IInFolderArchive is implemented by CAgent (Agent/Agent.h)
IInFolderArchive Is used by FAR/Plugin
*/

#define Z7_IFACEM_IInFolderArchive(x) \
  x(Open(IInStream *inStream, const wchar_t *filePath, const wchar_t *arcFormat, BSTR *archiveTypeRes, IArchiveOpenCallback *openArchiveCallback)) \
  x(ReOpen(IArchiveOpenCallback *openArchiveCallback)) \
  x(Close()) \
  x(GetNumberOfProperties(UInt32 *numProperties)) \
  x(GetPropertyInfo(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType)) \
  x(BindToRootFolder(IFolderFolder **resultFolder)) \
  x(Extract(NExtract::NPathMode::EEnum pathMode, \
      NExtract::NOverwriteMode::EEnum overwriteMode, const wchar_t *path, \
      Int32 testMode, IFolderArchiveExtractCallback *extractCallback2)) \

Z7_IFACE_CONSTR_FOLDERARC(IInFolderArchive, 0x0E)

#define Z7_IFACEM_IFolderArchiveUpdateCallback(x) \
  x(CompressOperation(const wchar_t *name)) \
  x(DeleteOperation(const wchar_t *name)) \
  x(OperationResult(Int32 opRes)) \
  x(UpdateErrorMessage(const wchar_t *message)) \
  x(SetNumFiles(UInt64 numFiles)) \

Z7_IFACE_CONSTR_FOLDERARC_SUB(IFolderArchiveUpdateCallback, IProgress, 0x0B)

#define Z7_IFACEM_IOutFolderArchive(x) \
  x(SetFolder(IFolderFolder *folder)) \
  x(SetFiles(const wchar_t *folderPrefix, const wchar_t * const *names, UInt32 numNames)) \
  x(DeleteItems(ISequentialOutStream *outArchiveStream, \
      const UInt32 *indices, UInt32 numItems, IFolderArchiveUpdateCallback *updateCallback)) \
  x(DoOperation( \
      FStringVector *requestedPaths, \
      FStringVector *processedPaths, \
      CCodecs *codecs, int index, \
      ISequentialOutStream *outArchiveStream, const Byte *stateActions, const wchar_t *sfxModule, \
      IFolderArchiveUpdateCallback *updateCallback)) \
  x(DoOperation2( \
      FStringVector *requestedPaths, \
      FStringVector *processedPaths, \
      ISequentialOutStream *outArchiveStream, const Byte *stateActions, const wchar_t *sfxModule, \
      IFolderArchiveUpdateCallback *updateCallback)) \

Z7_IFACE_CONSTR_FOLDERARC(IOutFolderArchive, 0x0F)


#define Z7_IFACEM_IFolderArchiveUpdateCallback2(x) \
  x(OpenFileError(const wchar_t *path, HRESULT errorCode)) \
  x(ReadingFileError(const wchar_t *path, HRESULT errorCode)) \
  x(ReportExtractResult(Int32 opRes, Int32 isEncrypted, const wchar_t *path)) \
  x(ReportUpdateOperation(UInt32 notifyOp, const wchar_t *path, Int32 isDir)) \

Z7_IFACE_CONSTR_FOLDERARC(IFolderArchiveUpdateCallback2, 0x10)


#define Z7_IFACEM_IFolderScanProgress(x) \
  x(ScanError(const wchar_t *path, HRESULT errorCode)) \
  x(ScanProgress(UInt64 numFolders, UInt64 numFiles, UInt64 totalSize, const wchar_t *path, Int32 isDir)) \

Z7_IFACE_CONSTR_FOLDERARC(IFolderScanProgress, 0x11)


#define Z7_IFACEM_IFolderSetZoneIdMode(x) \
  x(SetZoneIdMode(NExtract::NZoneIdMode::EEnum zoneMode)) \

Z7_IFACE_CONSTR_FOLDERARC(IFolderSetZoneIdMode, 0x12)

#define Z7_IFACEM_IFolderSetZoneIdFile(x) \
  x(SetZoneIdFile(const Byte *data, UInt32 size)) \

Z7_IFACE_CONSTR_FOLDERARC(IFolderSetZoneIdFile, 0x13)


// if the caller calls Before_ArcReopen(), the callee must
// clear user break status, because the caller want to open archive still.
#define Z7_IFACEM_IFolderArchiveUpdateCallback_MoveArc(x) \
  x(MoveArc_Start(const wchar_t *srcTempPath, const wchar_t *destFinalPath, UInt64 size, Int32 updateMode)) \
  x(MoveArc_Progress(UInt64 totalSize, UInt64 currentSize)) \
  x(MoveArc_Finish()) \
  x(Before_ArcReopen()) \

Z7_IFACE_CONSTR_FOLDERARC(IFolderArchiveUpdateCallback_MoveArc, 0x14)

Z7_PURE_INTERFACES_END
#endif

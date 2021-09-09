// IFolderArchive.h

#ifndef __IFOLDER_ARCHIVE_H
#define __IFOLDER_ARCHIVE_H

#include "../../../Common/MyString.h"

#include "../../Archive/IArchive.h"
#include "../../UI/Common/LoadCodecs.h"
#include "../../UI/FileManager/IFolder.h"

#include "../Common/ExtractMode.h"
#include "../Common/IFileExtractCallback.h"

#define FOLDER_ARCHIVE_INTERFACE_SUB(i, base, x) DECL_INTERFACE_SUB(i, base, 0x01, x)
#define FOLDER_ARCHIVE_INTERFACE(i, x) FOLDER_ARCHIVE_INTERFACE_SUB(i, IUnknown, x)

/* ---------- IArchiveFolder ----------
IArchiveFolder is implemented by CAgentFolder (Agent/Agent.h)
IArchiveFolder is used by:
  - FileManager/PanelCopy.cpp
      CPanel::CopyTo(), if (options->testMode)
  - FAR/PluginRead.cpp
      CPlugin::ExtractFiles
*/

#define INTERFACE_IArchiveFolder(x) \
  STDMETHOD(Extract)(const UInt32 *indices, UInt32 numItems, \
      Int32 includeAltStreams, \
      Int32 replaceAltStreamCharsMode, \
      NExtract::NPathMode::EEnum pathMode, \
      NExtract::NOverwriteMode::EEnum overwriteMode, \
      const wchar_t *path, Int32 testMode, \
      IFolderArchiveExtractCallback *extractCallback2) x; \

FOLDER_ARCHIVE_INTERFACE(IArchiveFolder, 0x0D)
{
  INTERFACE_IArchiveFolder(PURE)
};


/* ---------- IInFolderArchive ----------
IInFolderArchive is implemented by CAgent (Agent/Agent.h)
IInFolderArchive Is used by FAR/Plugin
*/

#define INTERFACE_IInFolderArchive(x) \
  STDMETHOD(Open)(IInStream *inStream, const wchar_t *filePath, const wchar_t *arcFormat, BSTR *archiveTypeRes, IArchiveOpenCallback *openArchiveCallback) x; \
  STDMETHOD(ReOpen)(IArchiveOpenCallback *openArchiveCallback) x; \
  STDMETHOD(Close)() x; \
  STDMETHOD(GetNumberOfProperties)(UInt32 *numProperties) x; \
  STDMETHOD(GetPropertyInfo)(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType) x; \
  STDMETHOD(BindToRootFolder)(IFolderFolder **resultFolder) x; \
  STDMETHOD(Extract)(NExtract::NPathMode::EEnum pathMode, \
      NExtract::NOverwriteMode::EEnum overwriteMode, const wchar_t *path, \
      Int32 testMode, IFolderArchiveExtractCallback *extractCallback2) x; \

FOLDER_ARCHIVE_INTERFACE(IInFolderArchive, 0x0E)
{
  INTERFACE_IInFolderArchive(PURE)
};

#define INTERFACE_IFolderArchiveUpdateCallback(x) \
  STDMETHOD(CompressOperation)(const wchar_t *name) x; \
  STDMETHOD(DeleteOperation)(const wchar_t *name) x; \
  STDMETHOD(OperationResult)(Int32 opRes) x; \
  STDMETHOD(UpdateErrorMessage)(const wchar_t *message) x; \
  STDMETHOD(SetNumFiles)(UInt64 numFiles) x; \

FOLDER_ARCHIVE_INTERFACE_SUB(IFolderArchiveUpdateCallback, IProgress, 0x0B)
{
  INTERFACE_IFolderArchiveUpdateCallback(PURE)
};

#define INTERFACE_IOutFolderArchive(x) \
  STDMETHOD(SetFolder)(IFolderFolder *folder) x; \
  STDMETHOD(SetFiles)(const wchar_t *folderPrefix, const wchar_t * const *names, UInt32 numNames) x; \
  STDMETHOD(DeleteItems)(ISequentialOutStream *outArchiveStream, \
      const UInt32 *indices, UInt32 numItems, IFolderArchiveUpdateCallback *updateCallback) x; \
  STDMETHOD(DoOperation)( \
      FStringVector *requestedPaths, \
      FStringVector *processedPaths, \
      CCodecs *codecs, int index, \
      ISequentialOutStream *outArchiveStream, const Byte *stateActions, const wchar_t *sfxModule, \
      IFolderArchiveUpdateCallback *updateCallback) x; \
  STDMETHOD(DoOperation2)( \
      FStringVector *requestedPaths, \
      FStringVector *processedPaths, \
      ISequentialOutStream *outArchiveStream, const Byte *stateActions, const wchar_t *sfxModule, \
      IFolderArchiveUpdateCallback *updateCallback) x; \

FOLDER_ARCHIVE_INTERFACE(IOutFolderArchive, 0x0F)
{
  INTERFACE_IOutFolderArchive(PURE)
};


#define INTERFACE_IFolderArchiveUpdateCallback2(x) \
  STDMETHOD(OpenFileError)(const wchar_t *path, HRESULT errorCode) x; \
  STDMETHOD(ReadingFileError)(const wchar_t *path, HRESULT errorCode) x; \
  STDMETHOD(ReportExtractResult)(Int32 opRes, Int32 isEncrypted, const wchar_t *path) x; \
  STDMETHOD(ReportUpdateOperation)(UInt32 notifyOp, const wchar_t *path, Int32 isDir) x; \

FOLDER_ARCHIVE_INTERFACE(IFolderArchiveUpdateCallback2, 0x10)
{
  INTERFACE_IFolderArchiveUpdateCallback2(PURE)
};


#define INTERFACE_IFolderScanProgress(x) \
  STDMETHOD(ScanError)(const wchar_t *path, HRESULT errorCode) x; \
  STDMETHOD(ScanProgress)(UInt64 numFolders, UInt64 numFiles, UInt64 totalSize, const wchar_t *path, Int32 isDir) x; \

FOLDER_ARCHIVE_INTERFACE(IFolderScanProgress, 0x11)
{
  INTERFACE_IFolderScanProgress(PURE)
};

#endif

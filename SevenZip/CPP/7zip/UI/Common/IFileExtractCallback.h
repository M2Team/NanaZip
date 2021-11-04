// IFileExtractCallback.h

#ifndef __I_FILE_EXTRACT_CALLBACK_H
#define __I_FILE_EXTRACT_CALLBACK_H

#include "../../../Common/MyString.h"

#include "../../IDecl.h"

#include "LoadCodecs.h"
#include "OpenArchive.h"

namespace NOverwriteAnswer
{
  enum EEnum
  {
    kYes,
    kYesToAll,
    kNo,
    kNoToAll,
    kAutoRename,
    kCancel
  };
}


/* ---------- IFolderArchiveExtractCallback ----------
is implemented by
  Console/ExtractCallbackConsole.h  CExtractCallbackConsole
  FileManager/ExtractCallback.h     CExtractCallbackImp
  FAR/ExtractEngine.cpp             CExtractCallBackImp: (QueryInterface is not supported)

IID_IFolderArchiveExtractCallback is requested by:
  - Agent/ArchiveFolder.cpp
      CAgentFolder::CopyTo(..., IFolderOperationsExtractCallback *callback)
      is sent to IArchiveFolder::Extract()

  - FileManager/PanelCopy.cpp
      CPanel::CopyTo(), if (options->testMode)
      is sent to IArchiveFolder::Extract()

 IFolderArchiveExtractCallback is used by Common/ArchiveExtractCallback.cpp
*/

#define INTERFACE_IFolderArchiveExtractCallback(x) \
  STDMETHOD(AskOverwrite)( \
      const wchar_t *existName, const FILETIME *existTime, const UInt64 *existSize, \
      const wchar_t *newName, const FILETIME *newTime, const UInt64 *newSize, \
      Int32 *answer) x; \
  STDMETHOD(PrepareOperation)(const wchar_t *name, Int32 isFolder, Int32 askExtractMode, const UInt64 *position) x; \
  STDMETHOD(MessageError)(const wchar_t *message) x; \
  STDMETHOD(SetOperationResult)(Int32 opRes, Int32 encrypted) x; \

DECL_INTERFACE_SUB(IFolderArchiveExtractCallback, IProgress, 0x01, 0x07)
{
  INTERFACE_IFolderArchiveExtractCallback(PURE)
};

#define INTERFACE_IFolderArchiveExtractCallback2(x) \
  STDMETHOD(ReportExtractResult)(Int32 opRes, Int32 encrypted, const wchar_t *name) x; \

DECL_INTERFACE_SUB(IFolderArchiveExtractCallback2, IUnknown, 0x01, 0x08)
{
  INTERFACE_IFolderArchiveExtractCallback2(PURE)
};

/* ---------- IExtractCallbackUI ----------
is implemented by
  Console/ExtractCallbackConsole.h  CExtractCallbackConsole
  FileManager/ExtractCallback.h     CExtractCallbackImp
*/

#ifdef _NO_CRYPTO
  #define INTERFACE_IExtractCallbackUI_Crypto(x)
#else
  #define INTERFACE_IExtractCallbackUI_Crypto(x) \
  virtual HRESULT SetPassword(const UString &password) x;
#endif

#define INTERFACE_IExtractCallbackUI(x) \
  virtual HRESULT BeforeOpen(const wchar_t *name, bool testMode) x; \
  virtual HRESULT OpenResult(const CCodecs *codecs, const CArchiveLink &arcLink, const wchar_t *name, HRESULT result) x; \
  virtual HRESULT ThereAreNoFiles() x; \
  virtual HRESULT ExtractResult(HRESULT result) x; \
  INTERFACE_IExtractCallbackUI_Crypto(x)

struct IExtractCallbackUI: IFolderArchiveExtractCallback
{
  INTERFACE_IExtractCallbackUI(PURE)
};



#define INTERFACE_IGetProp(x) \
  STDMETHOD(GetProp)(PROPID propID, PROPVARIANT *value) x; \

DECL_INTERFACE_SUB(IGetProp, IUnknown, 0x01, 0x20)
{
  INTERFACE_IGetProp(PURE)
};

#define INTERFACE_IFolderExtractToStreamCallback(x) \
  STDMETHOD(UseExtractToStream)(Int32 *res) x; \
  STDMETHOD(GetStream7)(const wchar_t *name, Int32 isDir, ISequentialOutStream **outStream, Int32 askExtractMode, IGetProp *getProp) x; \
  STDMETHOD(PrepareOperation7)(Int32 askExtractMode) x; \
  STDMETHOD(SetOperationResult8)(Int32 resultEOperationResult, Int32 encrypted, UInt64 size) x; \

DECL_INTERFACE_SUB(IFolderExtractToStreamCallback, IUnknown, 0x01, 0x31)
{
  INTERFACE_IFolderExtractToStreamCallback(PURE)
};


#endif

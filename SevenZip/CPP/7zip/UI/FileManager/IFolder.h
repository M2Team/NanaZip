// IFolder.h

#ifndef __IFOLDER_H
#define __IFOLDER_H

#include "../../IProgress.h"
#include "../../IStream.h"

#define FOLDER_INTERFACE_SUB(i, b, x) DECL_INTERFACE_SUB(i, b, 8, x)
#define FOLDER_INTERFACE(i, x) FOLDER_INTERFACE_SUB(i, IUnknown, x)

namespace NPlugin
{
  enum
  {
    kName = 0,
    kType,
    kClassID,
    kOptionsClassID
  };
}

#define INTERFACE_FolderFolder(x) \
  STDMETHOD(LoadItems)() x; \
  STDMETHOD(GetNumberOfItems)(UInt32 *numItems) x; \
  STDMETHOD(GetProperty)(UInt32 itemIndex, PROPID propID, PROPVARIANT *value) x; \
  STDMETHOD(BindToFolder)(UInt32 index, IFolderFolder **resultFolder) x; \
  STDMETHOD(BindToFolder)(const wchar_t *name, IFolderFolder **resultFolder) x; \
  STDMETHOD(BindToParentFolder)(IFolderFolder **resultFolder) x; \
  STDMETHOD(GetNumberOfProperties)(UInt32 *numProperties) x; \
  STDMETHOD(GetPropertyInfo)(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType) x; \
  STDMETHOD(GetFolderProperty)(PROPID propID, PROPVARIANT *value) x; \

FOLDER_INTERFACE(IFolderFolder, 0x00)
{
  INTERFACE_FolderFolder(PURE)
};

/*
  IFolderAltStreams::
    BindToAltStreams((UInt32)(Int32)-1, ... ) means alt streams of that folder
*/

#define INTERFACE_FolderAltStreams(x) \
  STDMETHOD(BindToAltStreams)(UInt32 index, IFolderFolder **resultFolder) x; \
  STDMETHOD(BindToAltStreams)(const wchar_t *name, IFolderFolder **resultFolder) x; \
  STDMETHOD(AreAltStreamsSupported)(UInt32 index, Int32 *isSupported) x; \

FOLDER_INTERFACE(IFolderAltStreams, 0x17)
{
  INTERFACE_FolderAltStreams(PURE)
};

FOLDER_INTERFACE(IFolderWasChanged, 0x04)
{
  STDMETHOD(WasChanged)(Int32 *wasChanged) PURE;
};

FOLDER_INTERFACE_SUB(IFolderOperationsExtractCallback, IProgress, 0x0B)
{
  // STDMETHOD(SetTotalFiles)(UInt64 total) PURE;
  // STDMETHOD(SetCompletedFiles)(const UInt64 *completedValue) PURE;
  STDMETHOD(AskWrite)(
      const wchar_t *srcPath,
      Int32 srcIsFolder,
      const FILETIME *srcTime,
      const UInt64 *srcSize,
      const wchar_t *destPathRequest,
      BSTR *destPathResult,
      Int32 *writeAnswer) PURE;
  STDMETHOD(ShowMessage)(const wchar_t *message) PURE;
  STDMETHOD(SetCurrentFilePath)(const wchar_t *filePath) PURE;
  STDMETHOD(SetNumFiles)(UInt64 numFiles) PURE;
};

#define INTERFACE_FolderOperations(x) \
  STDMETHOD(CreateFolder)(const wchar_t *name, IProgress *progress) x; \
  STDMETHOD(CreateFile)(const wchar_t *name, IProgress *progress) x; \
  STDMETHOD(Rename)(UInt32 index, const wchar_t *newName, IProgress *progress) x; \
  STDMETHOD(Delete)(const UInt32 *indices, UInt32 numItems, IProgress *progress) x; \
  STDMETHOD(CopyTo)(Int32 moveMode, const UInt32 *indices, UInt32 numItems, \
      Int32 includeAltStreams, Int32 replaceAltStreamCharsMode, \
      const wchar_t *path, IFolderOperationsExtractCallback *callback) x; \
  STDMETHOD(CopyFrom)(Int32 moveMode, const wchar_t *fromFolderPath, \
      const wchar_t * const *itemsPaths, UInt32 numItems, IProgress *progress) x; \
  STDMETHOD(SetProperty)(UInt32 index, PROPID propID, const PROPVARIANT *value, IProgress *progress) x; \
  STDMETHOD(CopyFromFile)(UInt32 index, const wchar_t *fullFilePath, IProgress *progress) x; \

FOLDER_INTERFACE(IFolderOperations, 0x13)
{
  INTERFACE_FolderOperations(PURE)
};

/*
FOLDER_INTERFACE2(IFolderOperationsDeleteToRecycleBin, 0x06, 0x03)
{
  STDMETHOD(DeleteToRecycleBin)(const UInt32 *indices, UInt32 numItems, IProgress *progress) PURE;
};
*/

FOLDER_INTERFACE(IFolderGetSystemIconIndex, 0x07)
{
  STDMETHOD(GetSystemIconIndex)(UInt32 index, Int32 *iconIndex) PURE;
};

FOLDER_INTERFACE(IFolderGetItemFullSize, 0x08)
{
  STDMETHOD(GetItemFullSize)(UInt32 index, PROPVARIANT *value, IProgress *progress) PURE;
};

FOLDER_INTERFACE(IFolderCalcItemFullSize, 0x14)
{
  STDMETHOD(CalcItemFullSize)(UInt32 index, IProgress *progress) PURE;
};

FOLDER_INTERFACE(IFolderClone, 0x09)
{
  STDMETHOD(Clone)(IFolderFolder **resultFolder) PURE;
};

FOLDER_INTERFACE(IFolderSetFlatMode, 0x0A)
{
  STDMETHOD(SetFlatMode)(Int32 flatMode) PURE;
};

/*
FOLDER_INTERFACE(IFolderSetShowNtfsStreamsMode, 0xFA)
{
  STDMETHOD(SetShowNtfsStreamsMode)(Int32 showStreamsMode) PURE;
};
*/

#define INTERFACE_FolderProperties(x) \
  STDMETHOD(GetNumberOfFolderProperties)(UInt32 *numProperties) x; \
  STDMETHOD(GetFolderPropertyInfo)(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType) x; \

FOLDER_INTERFACE(IFolderProperties, 0x0E)
{
  INTERFACE_FolderProperties(PURE)
};

#define INTERFACE_IFolderArcProps(x) \
  STDMETHOD(GetArcNumLevels)(UInt32 *numLevels) x; \
  STDMETHOD(GetArcProp)(UInt32 level, PROPID propID, PROPVARIANT *value) x; \
  STDMETHOD(GetArcNumProps)(UInt32 level, UInt32 *numProps) x; \
  STDMETHOD(GetArcPropInfo)(UInt32 level, UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType) x; \
  STDMETHOD(GetArcProp2)(UInt32 level, PROPID propID, PROPVARIANT *value) x; \
  STDMETHOD(GetArcNumProps2)(UInt32 level, UInt32 *numProps) x; \
  STDMETHOD(GetArcPropInfo2)(UInt32 level, UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType) x; \

FOLDER_INTERFACE(IFolderArcProps, 0x10)
{
  INTERFACE_IFolderArcProps(PURE)
};

FOLDER_INTERFACE(IGetFolderArcProps, 0x11)
{
  STDMETHOD(GetFolderArcProps)(IFolderArcProps **object) PURE;
};

FOLDER_INTERFACE(IFolderCompare, 0x15)
{
  STDMETHOD_(Int32, CompareItems)(UInt32 index1, UInt32 index2, PROPID propID, Int32 propIsRaw) PURE;
};

#define INTERFACE_IFolderGetItemName(x) \
  STDMETHOD(GetItemName)(UInt32 index, const wchar_t **name, unsigned *len) x; \
  STDMETHOD(GetItemPrefix)(UInt32 index, const wchar_t **name, unsigned *len) x; \
  STDMETHOD_(UInt64, GetItemSize)(UInt32 index) x; \

FOLDER_INTERFACE(IFolderGetItemName, 0x16)
{
  INTERFACE_IFolderGetItemName(PURE)
};

#define FOLDER_MANAGER_INTERFACE(i, x)  DECL_INTERFACE(i, 9, x)

#define INTERFACE_IFolderManager(x) \
  STDMETHOD(OpenFolderFile)(IInStream *inStream, const wchar_t *filePath, const wchar_t *arcFormat, IFolderFolder **resultFolder, IProgress *progress) x; \
  STDMETHOD(GetExtensions)(BSTR *extensions) x; \
  STDMETHOD(GetIconPath)(const wchar_t *ext, BSTR *iconPath, Int32 *iconIndex) x; \
  
  // STDMETHOD(GetTypes)(BSTR *types) PURE;
  // STDMETHOD(CreateFolderFile)(const wchar_t *type, const wchar_t *filePath, IProgress *progress) PURE;
            
FOLDER_MANAGER_INTERFACE(IFolderManager, 0x05)
{
  INTERFACE_IFolderManager(PURE);
};

/*
#define IMP_IFolderFolder_GetProp(k) \
  (UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType) \
    { if (index >= ARRAY_SIZE(k)) return E_INVALIDARG; \
    const CMy_STATPROPSTG_2 &srcItem = k[index]; \
    *propID = srcItem.propid; *varType = srcItem.vt; *name = 0; return S_OK; } \

#define IMP_IFolderFolder_Props(c) \
  STDMETHODIMP c::GetNumberOfProperties(UInt32 *numProperties) \
    { *numProperties = ARRAY_SIZE(kProps); return S_OK; } \
  STDMETHODIMP c::GetPropertyInfo IMP_IFolderFolder_GetProp(kProps)
*/

#define IMP_IFolderFolder_GetProp(k) \
  (UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType) \
    { if (index >= ARRAY_SIZE(k)) return E_INVALIDARG; \
    *propID = k[index]; *varType = k7z_PROPID_To_VARTYPE[(unsigned)*propID]; *name = 0; return S_OK; } \

#define IMP_IFolderFolder_Props(c) \
  STDMETHODIMP c::GetNumberOfProperties(UInt32 *numProperties) \
    { *numProperties = ARRAY_SIZE(kProps); return S_OK; } \
  STDMETHODIMP c::GetPropertyInfo IMP_IFolderFolder_GetProp(kProps)


int CompareFileNames_ForFolderList(const wchar_t *s1, const wchar_t *s2);
// int CompareFileNames_ForFolderList(const FChar *s1, const FChar *s2);

#endif

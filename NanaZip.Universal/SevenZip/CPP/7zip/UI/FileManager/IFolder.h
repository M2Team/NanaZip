// IFolder.h

#ifndef ZIP7_INC_IFOLDER_H
#define ZIP7_INC_IFOLDER_H

#include "../../IProgress.h"
#include "../../IStream.h"

Z7_PURE_INTERFACES_BEGIN

#define Z7_IFACE_CONSTR_FOLDER_SUB(i, base, n) \
  Z7_DECL_IFACE_7ZIP_SUB(i, base, 8, n) \
  { Z7_IFACE_COM7_PURE(i) };

#define Z7_IFACE_CONSTR_FOLDER(i, n) \
        Z7_IFACE_CONSTR_FOLDER_SUB(i, IUnknown, n)

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

#define Z7_IFACEM_IFolderFolder(x) \
  x(LoadItems()) \
  x(GetNumberOfItems(UInt32 *numItems)) \
  x(GetProperty(UInt32 itemIndex, PROPID propID, PROPVARIANT *value)) \
  x(BindToFolder(UInt32 index, IFolderFolder **resultFolder)) \
  x(BindToFolder(const wchar_t *name, IFolderFolder **resultFolder)) \
  x(BindToParentFolder(IFolderFolder **resultFolder)) \
  x(GetNumberOfProperties(UInt32 *numProperties)) \
  x(GetPropertyInfo(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType)) \
  x(GetFolderProperty(PROPID propID, PROPVARIANT *value)) \

Z7_IFACE_CONSTR_FOLDER(IFolderFolder, 0x00)

/*
  IFolderAltStreams::
    BindToAltStreams((UInt32)(Int32)-1, ... ) means alt streams of that folder
*/

#define Z7_IFACEM_IFolderAltStreams(x) \
  x(BindToAltStreams(UInt32 index, IFolderFolder **resultFolder)) \
  x(BindToAltStreams(const wchar_t *name, IFolderFolder **resultFolder)) \
  x(AreAltStreamsSupported(UInt32 index, Int32 *isSupported)) \

Z7_IFACE_CONSTR_FOLDER(IFolderAltStreams, 0x17)

#define Z7_IFACEM_IFolderWasChanged(x) \
  x(WasChanged(Int32 *wasChanged))
Z7_IFACE_CONSTR_FOLDER(IFolderWasChanged, 0x04)

  /* x(SetTotalFiles(UInt64 total)) */ \
  /* x(SetCompletedFiles(const UInt64 *completedValue)) */ \
#define Z7_IFACEM_IFolderOperationsExtractCallback(x) \
  x(AskWrite( \
      const wchar_t *srcPath, \
      Int32 srcIsFolder, \
      const FILETIME *srcTime, \
      const UInt64 *srcSize, \
      const wchar_t *destPathRequest, \
      BSTR *destPathResult, \
      Int32 *writeAnswer)) \
  x(ShowMessage(const wchar_t *message)) \
  x(SetCurrentFilePath(const wchar_t *filePath)) \
  x(SetNumFiles(UInt64 numFiles)) \

Z7_IFACE_CONSTR_FOLDER_SUB(IFolderOperationsExtractCallback, IProgress, 0x0B)


#define Z7_IFACEM_IFolderOperations(x) \
  x(CreateFolder(const wchar_t *name, IProgress *progress)) \
  x(CreateFile(const wchar_t *name, IProgress *progress)) \
  x(Rename(UInt32 index, const wchar_t *newName, IProgress *progress)) \
  x(Delete(const UInt32 *indices, UInt32 numItems, IProgress *progress)) \
  x(CopyTo(Int32 moveMode, const UInt32 *indices, UInt32 numItems, \
      Int32 includeAltStreams, Int32 replaceAltStreamCharsMode, \
      const wchar_t *path, IFolderOperationsExtractCallback *callback)) \
  x(CopyFrom(Int32 moveMode, const wchar_t *fromFolderPath, \
      const wchar_t * const *itemsPaths, UInt32 numItems, IProgress *progress)) \
  x(SetProperty(UInt32 index, PROPID propID, const PROPVARIANT *value, IProgress *progress)) \
  x(CopyFromFile(UInt32 index, const wchar_t *fullFilePath, IProgress *progress)) \

Z7_IFACE_CONSTR_FOLDER(IFolderOperations, 0x13)

/*
FOLDER_INTERFACE2(IFolderOperationsDeleteToRecycleBin, 0x06, 0x03)
{
  x(DeleteToRecycleBin(const UInt32 *indices, UInt32 numItems, IProgress *progress)) \
};
*/

#define Z7_IFACEM_IFolderGetSystemIconIndex(x) \
  x(GetSystemIconIndex(UInt32 index, Int32 *iconIndex))
Z7_IFACE_CONSTR_FOLDER(IFolderGetSystemIconIndex, 0x07)

#define Z7_IFACEM_IFolderGetItemFullSize(x) \
  x(GetItemFullSize(UInt32 index, PROPVARIANT *value, IProgress *progress))
Z7_IFACE_CONSTR_FOLDER(IFolderGetItemFullSize, 0x08)

#define Z7_IFACEM_IFolderCalcItemFullSize(x) \
  x(CalcItemFullSize(UInt32 index, IProgress *progress))
Z7_IFACE_CONSTR_FOLDER(IFolderCalcItemFullSize, 0x14)

#define Z7_IFACEM_IFolderClone(x) \
  x(Clone(IFolderFolder **resultFolder))
Z7_IFACE_CONSTR_FOLDER(IFolderClone, 0x09)

#define Z7_IFACEM_IFolderSetFlatMode(x) \
  x(SetFlatMode(Int32 flatMode))
Z7_IFACE_CONSTR_FOLDER(IFolderSetFlatMode, 0x0A)

/*
#define Z7_IFACEM_IFolderSetShowNtfsStreamsMode(x) \
  x(SetShowNtfsStreamsMode(Int32 showStreamsMode))
Z7_IFACE_CONSTR_FOLDER(IFolderSetShowNtfsStreamsMode, 0xFA)
*/

#define Z7_IFACEM_IFolderProperties(x) \
  x(GetNumberOfFolderProperties(UInt32 *numProperties)) \
  x(GetFolderPropertyInfo(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType)) \

Z7_IFACE_CONSTR_FOLDER(IFolderProperties, 0x0E)

#define Z7_IFACEM_IFolderArcProps(x) \
  x(GetArcNumLevels(UInt32 *numLevels)) \
  x(GetArcProp(UInt32 level, PROPID propID, PROPVARIANT *value)) \
  x(GetArcNumProps(UInt32 level, UInt32 *numProps)) \
  x(GetArcPropInfo(UInt32 level, UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType)) \
  x(GetArcProp2(UInt32 level, PROPID propID, PROPVARIANT *value)) \
  x(GetArcNumProps2(UInt32 level, UInt32 *numProps)) \
  x(GetArcPropInfo2(UInt32 level, UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType)) \

Z7_IFACE_CONSTR_FOLDER(IFolderArcProps, 0x10)

#define Z7_IFACEM_IGetFolderArcProps(x) \
  x(GetFolderArcProps(IFolderArcProps **object))
Z7_IFACE_CONSTR_FOLDER(IGetFolderArcProps, 0x11)

#define Z7_IFACEM_IFolderCompare(x) \
  x##2(Int32, CompareItems(UInt32 index1, UInt32 index2, PROPID propID, Int32 propIsRaw))
Z7_IFACE_CONSTR_FOLDER(IFolderCompare, 0x15)

#define Z7_IFACEM_IFolderGetItemName(x) \
  x(GetItemName(UInt32 index, const wchar_t **name, unsigned *len)) \
  x(GetItemPrefix(UInt32 index, const wchar_t **name, unsigned *len)) \
  x##2(UInt64, GetItemSize(UInt32 index)) \

Z7_IFACE_CONSTR_FOLDER(IFolderGetItemName, 0x16)


#define Z7_IFACEM_IFolderManager(x) \
  x(OpenFolderFile(IInStream *inStream, const wchar_t *filePath, const wchar_t *arcFormat, IFolderFolder **resultFolder, IProgress *progress)) \
  x(GetExtensions(BSTR *extensions)) \
  x(GetIconPath(const wchar_t *ext, BSTR *iconPath, Int32 *iconIndex)) \
  
  // x(GetTypes(BSTR *types))
  // x(CreateFolderFile(const wchar_t *type, const wchar_t *filePath, IProgress *progress))
  
Z7_DECL_IFACE_7ZIP(IFolderManager, 9, 5)
  { Z7_IFACE_COM7_PURE(IFolderManager) };

/*
    const CMy_STATPROPSTG_2 &srcItem = k[index]; \
    *propID = srcItem.propid; *varType = srcItem.vt; *name = 0; return S_OK; } \
*/
#define IMP_IFolderFolder_GetProp(fn, k) \
  Z7_COM7F_IMF(fn(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType)) \
    { if (index >= Z7_ARRAY_SIZE(k)) return E_INVALIDARG; \
    *propID = k[index]; *varType = k7z_PROPID_To_VARTYPE[(unsigned)*propID]; *name = NULL; return S_OK; } \

#define IMP_IFolderFolder_Props(c) \
  Z7_COM7F_IMF(c::GetNumberOfProperties(UInt32 *numProperties)) \
    { *numProperties = Z7_ARRAY_SIZE(kProps); return S_OK; } \
  IMP_IFolderFolder_GetProp(c::GetPropertyInfo, kProps)


int CompareFileNames_ForFolderList(const wchar_t *s1, const wchar_t *s2);
// int CompareFileNames_ForFolderList(const FChar *s1, const FChar *s2);

Z7_PURE_INTERFACES_END
#endif

// IArchive.h

#ifndef __IARCHIVE_H
#define __IARCHIVE_H

#include "../IProgress.h"
#include "../IStream.h"
#include "../PropID.h"

#define ARCHIVE_INTERFACE_SUB(i, base, x) DECL_INTERFACE_SUB(i, base, 6, x)
#define ARCHIVE_INTERFACE(i, x) ARCHIVE_INTERFACE_SUB(i, IUnknown, x)

/*
How the function in 7-Zip returns object for output parameter via pointer

1) The caller sets the value of variable before function call:
  PROPVARIANT  :  vt = VT_EMPTY
  BSTR         :  NULL
  IUnknown* and derived interfaces  :  NULL
  another scalar types  :  any non-initialized value is allowed

2) The callee in current 7-Zip code now can free input object for output parameter:
  PROPVARIANT   : the callee calls VariantClear(propvaiant_ptr) for input
                  value stored in variable
  another types : the callee ignores stored value.

3) The callee writes new value to variable for output parameter and
  returns execution to caller.

4) The caller must free or release object returned by the callee:
  PROPVARIANT   : VariantClear(&propvaiant)
  BSTR          : SysFreeString(bstr)
  IUnknown* and derived interfaces  :  if (ptr) ptr->Relase()
*/


namespace NFileTimeType
{
  enum EEnum
  {
    kWindows,
    kUnix,
    kDOS
  };
}

namespace NArcInfoFlags
{
  const UInt32 kKeepName        = 1 << 0;  // keep name of file in archive name
  const UInt32 kAltStreams      = 1 << 1;  // the handler supports alt streams
  const UInt32 kNtSecure        = 1 << 2;  // the handler supports NT security
  const UInt32 kFindSignature   = 1 << 3;  // the handler can find start of archive
  const UInt32 kMultiSignature  = 1 << 4;  // there are several signatures
  const UInt32 kUseGlobalOffset = 1 << 5;  // the seek position of stream must be set as global offset
  const UInt32 kStartOpen       = 1 << 6;  // call handler for each start position
  const UInt32 kPureStartOpen   = 1 << 7;  // call handler only for start of file
  const UInt32 kBackwardOpen    = 1 << 8;  // archive can be open backward
  const UInt32 kPreArc          = 1 << 9;  // such archive can be stored before real archive (like SFX stub)
  const UInt32 kSymLinks        = 1 << 10; // the handler supports symbolic links
  const UInt32 kHardLinks       = 1 << 11; // the handler supports hard links
  const UInt32 kByExtOnlyOpen   = 1 << 12; // call handler only if file extension matches
}

namespace NArchive
{
  namespace NHandlerPropID
  {
    enum
    {
      kName = 0,        // VT_BSTR
      kClassID,         // binary GUID in VT_BSTR
      kExtension,       // VT_BSTR
      kAddExtension,    // VT_BSTR
      kUpdate,          // VT_BOOL
      kKeepName,        // VT_BOOL
      kSignature,       // binary in VT_BSTR
      kMultiSignature,  // binary in VT_BSTR
      kSignatureOffset, // VT_UI4
      kAltStreams,      // VT_BOOL
      kNtSecure,        // VT_BOOL
      kFlags            // VT_UI4
      // kVersion          // VT_UI4 ((VER_MAJOR << 8) | VER_MINOR)
    };
  }

  namespace NExtract
  {
    namespace NAskMode
    {
      enum
      {
        kExtract = 0,
        kTest,
        kSkip
      };
    }
  
    namespace NOperationResult
    {
      enum
      {
        kOK = 0,
        kUnsupportedMethod,
        kDataError,
        kCRCError,
        kUnavailable,
        kUnexpectedEnd,
        kDataAfterEnd,
        kIsNotArc,
        kHeadersError,
        kWrongPassword
      };
    }
  }

  namespace NEventIndexType
  {
    enum
    {
      kNoIndex = 0,
      kInArcIndex,
      kBlockIndex,
      kOutArcIndex
    };
  }
  
  namespace NUpdate
  {
    namespace NOperationResult
    {
      enum
      {
        kOK = 0
        // , kError
      };
    }
  }
}

#define INTERFACE_IArchiveOpenCallback(x) \
  STDMETHOD(SetTotal)(const UInt64 *files, const UInt64 *bytes) x; \
  STDMETHOD(SetCompleted)(const UInt64 *files, const UInt64 *bytes) x; \

ARCHIVE_INTERFACE(IArchiveOpenCallback, 0x10)
{
  INTERFACE_IArchiveOpenCallback(PURE);
};

/*
IArchiveExtractCallback::

7-Zip doesn't call IArchiveExtractCallback functions
  GetStream()
  PrepareOperation()
  SetOperationResult()
from different threads simultaneously.
But 7-Zip can call functions for IProgress or ICompressProgressInfo functions
from another threads simultaneously with calls for IArchiveExtractCallback interface.

IArchiveExtractCallback::GetStream()
  UInt32 index - index of item in Archive
  Int32 askExtractMode  (Extract::NAskMode)
    if (askMode != NExtract::NAskMode::kExtract)
    {
      then the callee doesn't write data to stream: (*outStream == NULL)
    }
  
  Out:
      (*outStream == NULL) - for directories
      (*outStream == NULL) - if link (hard link or symbolic link) was created
      if (*outStream == NULL && askMode == NExtract::NAskMode::kExtract)
      {
        then the caller must skip extracting of that file.
      }

  returns:
    S_OK     : OK
    S_FALSE  : data error (for decoders)

if (IProgress::SetTotal() was called)
{
  IProgress::SetCompleted(completeValue) uses
    packSize   - for some stream formats (xz, gz, bz2, lzma, z, ppmd).
    unpackSize - for another formats.
}
else
{
  IProgress::SetCompleted(completeValue) uses packSize.
}

SetOperationResult()
  7-Zip calls SetOperationResult at the end of extracting,
  so the callee can close the file, set attributes, timestamps and security information.

  Int32 opRes (NExtract::NOperationResult)
*/

#define INTERFACE_IArchiveExtractCallback(x) \
  INTERFACE_IProgress(x) \
  STDMETHOD(GetStream)(UInt32 index, ISequentialOutStream **outStream, Int32 askExtractMode) x; \
  STDMETHOD(PrepareOperation)(Int32 askExtractMode) x; \
  STDMETHOD(SetOperationResult)(Int32 opRes) x; \

ARCHIVE_INTERFACE_SUB(IArchiveExtractCallback, IProgress, 0x20)
{
  INTERFACE_IArchiveExtractCallback(PURE)
};



/*
IArchiveExtractCallbackMessage can be requested from IArchiveExtractCallback object
  by Extract() or UpdateItems() functions to report about extracting errors
ReportExtractResult()
  UInt32 indexType (NEventIndexType)
  UInt32 index
  Int32 opRes (NExtract::NOperationResult)
*/

#define INTERFACE_IArchiveExtractCallbackMessage(x) \
  STDMETHOD(ReportExtractResult)(UInt32 indexType, UInt32 index, Int32 opRes) x; \

ARCHIVE_INTERFACE_SUB(IArchiveExtractCallbackMessage, IProgress, 0x21)
{
  INTERFACE_IArchiveExtractCallbackMessage(PURE)
};


#define INTERFACE_IArchiveOpenVolumeCallback(x) \
  STDMETHOD(GetProperty)(PROPID propID, PROPVARIANT *value) x; \
  STDMETHOD(GetStream)(const wchar_t *name, IInStream **inStream) x; \

ARCHIVE_INTERFACE(IArchiveOpenVolumeCallback, 0x30)
{
  INTERFACE_IArchiveOpenVolumeCallback(PURE);
};


ARCHIVE_INTERFACE(IInArchiveGetStream, 0x40)
{
  STDMETHOD(GetStream)(UInt32 index, ISequentialInStream **stream) PURE;
};


ARCHIVE_INTERFACE(IArchiveOpenSetSubArchiveName, 0x50)
{
  STDMETHOD(SetSubArchiveName)(const wchar_t *name) PURE;
};


/*
IInArchive::Open
    stream
      if (kUseGlobalOffset), stream current position can be non 0.
      if (!kUseGlobalOffset), stream current position is 0.
    if (maxCheckStartPosition == NULL), the handler can try to search archive start in stream
    if (*maxCheckStartPosition == 0), the handler must check only current position as archive start

IInArchive::Extract:
  indices must be sorted
  numItems = (UInt32)(Int32)-1 = 0xFFFFFFFF means "all files"
  testMode != 0 means "test files without writing to outStream"

IInArchive::GetArchiveProperty:
  kpidOffset  - start offset of archive.
      VT_EMPTY : means offset = 0.
      VT_UI4, VT_UI8, VT_I8 : result offset; negative values is allowed
  kpidPhySize - size of archive. VT_EMPTY means unknown size.
    kpidPhySize is allowed to be larger than file size. In that case it must show
    supposed size.

  kpidIsDeleted:
  kpidIsAltStream:
  kpidIsAux:
  kpidINode:
    must return VARIANT_TRUE (VT_BOOL), if archive can support that property in GetProperty.


Notes:
  Don't call IInArchive functions for same IInArchive object from different threads simultaneously.
  Some IInArchive handlers will work incorrectly in that case.
*/

#ifdef _MSC_VER
  #define MY_NO_THROW_DECL_ONLY throw()
#else
  #define MY_NO_THROW_DECL_ONLY
#endif

#define INTERFACE_IInArchive(x) \
  STDMETHOD(Open)(IInStream *stream, const UInt64 *maxCheckStartPosition, IArchiveOpenCallback *openCallback) MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(Close)() MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(GetNumberOfItems)(UInt32 *numItems) MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(GetProperty)(UInt32 index, PROPID propID, PROPVARIANT *value) MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(Extract)(const UInt32* indices, UInt32 numItems, Int32 testMode, IArchiveExtractCallback *extractCallback) MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(GetArchiveProperty)(PROPID propID, PROPVARIANT *value) MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(GetNumberOfProperties)(UInt32 *numProps) MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(GetPropertyInfo)(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType) MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(GetNumberOfArchiveProperties)(UInt32 *numProps) MY_NO_THROW_DECL_ONLY x; \
  STDMETHOD(GetArchivePropertyInfo)(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType) MY_NO_THROW_DECL_ONLY x; \

ARCHIVE_INTERFACE(IInArchive, 0x60)
{
  INTERFACE_IInArchive(PURE)
};

namespace NParentType
{
  enum
  {
    kDir = 0,
    kAltStream
  };
};

namespace NPropDataType
{
  const UInt32 kMask_ZeroEnd   = 1 << 4;
  // const UInt32 kMask_BigEndian = 1 << 5;
  const UInt32 kMask_Utf       = 1 << 6;
  const UInt32 kMask_Utf8  = kMask_Utf | 0;
  const UInt32 kMask_Utf16 = kMask_Utf | 1;
  // const UInt32 kMask_Utf32 = kMask_Utf | 2;

  const UInt32 kNotDefined = 0;
  const UInt32 kRaw = 1;

  const UInt32 kUtf8z  = kMask_Utf8  | kMask_ZeroEnd;
  const UInt32 kUtf16z = kMask_Utf16 | kMask_ZeroEnd;
};

// UTF string (pointer to wchar_t) with zero end and little-endian.
#define PROP_DATA_TYPE_wchar_t_PTR_Z_LE ((NPropDataType::kMask_Utf | NPropDataType::kMask_ZeroEnd) + (sizeof(wchar_t) >> 1))

/*
GetRawProp:
  Result:
    S_OK - even if property is not set
*/

#define INTERFACE_IArchiveGetRawProps(x) \
  STDMETHOD(GetParent)(UInt32 index, UInt32 *parent, UInt32 *parentType) x; \
  STDMETHOD(GetRawProp)(UInt32 index, PROPID propID, const void **data, UInt32 *dataSize, UInt32 *propType) x; \
  STDMETHOD(GetNumRawProps)(UInt32 *numProps) x; \
  STDMETHOD(GetRawPropInfo)(UInt32 index, BSTR *name, PROPID *propID) x;

ARCHIVE_INTERFACE(IArchiveGetRawProps, 0x70)
{
  INTERFACE_IArchiveGetRawProps(PURE)
};

#define INTERFACE_IArchiveGetRootProps(x) \
  STDMETHOD(GetRootProp)(PROPID propID, PROPVARIANT *value) x; \
  STDMETHOD(GetRootRawProp)(PROPID propID, const void **data, UInt32 *dataSize, UInt32 *propType) x; \
 
ARCHIVE_INTERFACE(IArchiveGetRootProps, 0x71)
{
  INTERFACE_IArchiveGetRootProps(PURE)
};

ARCHIVE_INTERFACE(IArchiveOpenSeq, 0x61)
{
  STDMETHOD(OpenSeq)(ISequentialInStream *stream) PURE;
};

/*
  OpenForSize
  Result:
    S_FALSE - is not archive
    ? - DATA error
*/
    
/*
const UInt32 kOpenFlags_RealPhySize = 1 << 0;
const UInt32 kOpenFlags_NoSeek = 1 << 1;
// const UInt32 kOpenFlags_BeforeExtract = 1 << 2;
*/

/*
Flags:
   0 - opens archive with IInStream, if IInStream interface is supported
     - if phySize is not available, it doesn't try to make full parse to get phySize
   kOpenFlags_NoSeek -  ArcOpen2 function doesn't use IInStream interface, even if it's available
   kOpenFlags_RealPhySize - the handler will try to get PhySize, even if it requires full decompression for file
   
  if handler is not allowed to use IInStream and the flag kOpenFlags_RealPhySize is not specified,
  the handler can return S_OK, but it doesn't check even Signature.
  So next Extract can be called for that sequential stream.
*/

/*
ARCHIVE_INTERFACE(IArchiveOpen2, 0x62)
{
  STDMETHOD(ArcOpen2)(ISequentialInStream *stream, UInt32 flags, IArchiveOpenCallback *openCallback) PURE;
};
*/

// ---------- UPDATE ----------

/*
GetUpdateItemInfo outs:
*newData  *newProps
   0        0      - Copy data and properties from archive
   0        1      - Copy data from archive, request new properties
   1        0      - that combination is unused now
   1        1      - Request new data and new properties. It can be used even for folders

  indexInArchive = -1 if there is no item in archive, or if it doesn't matter.


GetStream out:
  Result:
    S_OK:
      (*inStream == NULL) - only for directories
                          - the bug was fixed in 9.33: (*Stream == NULL) was in case of anti-file
      (*inStream != NULL) - for any file, even for empty file or anti-file
    S_FALSE - skip that file (don't add item to archive) - (client code can't open stream of that file by some reason)
      (*inStream == NULL)

The order of calling for hard links:
  - GetStream()
  - GetProperty(kpidHardLink)

SetOperationResult()
  Int32 opRes (NExtract::NOperationResult::kOK)
*/

#define INTERFACE_IArchiveUpdateCallback(x) \
  INTERFACE_IProgress(x); \
  STDMETHOD(GetUpdateItemInfo)(UInt32 index, Int32 *newData, Int32 *newProps, UInt32 *indexInArchive) x; \
  STDMETHOD(GetProperty)(UInt32 index, PROPID propID, PROPVARIANT *value) x; \
  STDMETHOD(GetStream)(UInt32 index, ISequentialInStream **inStream) x; \
  STDMETHOD(SetOperationResult)(Int32 operationResult) x; \

ARCHIVE_INTERFACE_SUB(IArchiveUpdateCallback, IProgress, 0x80)
{
  INTERFACE_IArchiveUpdateCallback(PURE);
};

#define INTERFACE_IArchiveUpdateCallback2(x) \
  INTERFACE_IArchiveUpdateCallback(x) \
  STDMETHOD(GetVolumeSize)(UInt32 index, UInt64 *size) x; \
  STDMETHOD(GetVolumeStream)(UInt32 index, ISequentialOutStream **volumeStream) x; \

ARCHIVE_INTERFACE_SUB(IArchiveUpdateCallback2, IArchiveUpdateCallback, 0x82)
{
  INTERFACE_IArchiveUpdateCallback2(PURE);
};

namespace NUpdateNotifyOp
{
  enum
  {
    kAdd = 0,
    kUpdate,
    kAnalyze,
    kReplicate,
    kRepack,
    kSkip,
    kDelete,
    kHeader

    // kNumDefined
  };
};

/*
IArchiveUpdateCallbackFile::ReportOperation
  UInt32 indexType (NEventIndexType)
  UInt32 index
  UInt32 notifyOp (NUpdateNotifyOp)
*/

#define INTERFACE_IArchiveUpdateCallbackFile(x) \
  STDMETHOD(GetStream2)(UInt32 index, ISequentialInStream **inStream, UInt32 notifyOp) x; \
  STDMETHOD(ReportOperation)(UInt32 indexType, UInt32 index, UInt32 notifyOp) x; \

ARCHIVE_INTERFACE(IArchiveUpdateCallbackFile, 0x83)
{
  INTERFACE_IArchiveUpdateCallbackFile(PURE);
};


/*
UpdateItems()
-------------

  outStream: output stream. (the handler) MUST support the case when
    Seek position in outStream is not ZERO.
    but the caller calls with empty outStream and seek position is ZERO??
 
  archives with stub:

  If archive is open and the handler and (Offset > 0), then the handler
  knows about stub size.
  UpdateItems():
  1) the handler MUST copy that stub to outStream
  2) the caller MUST NOT copy the stub to outStream, if
     "rsfx" property is set with SetProperties

  the handler must support the case where
    ISequentialOutStream *outStream
*/


#define INTERFACE_IOutArchive(x) \
  STDMETHOD(UpdateItems)(ISequentialOutStream *outStream, UInt32 numItems, IArchiveUpdateCallback *updateCallback) x; \
  STDMETHOD(GetFileTimeType)(UInt32 *type) x;

ARCHIVE_INTERFACE(IOutArchive, 0xA0)
{
  INTERFACE_IOutArchive(PURE)
};


/*
ISetProperties::SetProperties()
  PROPVARIANT values[i].vt:
    VT_EMPTY
    VT_BOOL
    VT_UI4   - if 32-bit number
    VT_UI8   - if 64-bit number
    VT_BSTR
*/

ARCHIVE_INTERFACE(ISetProperties, 0x03)
{
  STDMETHOD(SetProperties)(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps) PURE;
};

ARCHIVE_INTERFACE(IArchiveKeepModeForNextOpen, 0x04)
{
  STDMETHOD(KeepModeForNextOpen)() PURE;
};

/* Exe handler: the handler for executable format (PE, ELF, Mach-O).
   SFX archive: executable stub + some tail data.
     before 9.31: exe handler didn't parse SFX archives as executable format.
     for 9.31+: exe handler parses SFX archives as executable format, only if AllowTail(1) was called */

ARCHIVE_INTERFACE(IArchiveAllowTail, 0x05)
{
  STDMETHOD(AllowTail)(Int32 allowTail) PURE;
};


#define IMP_IInArchive_GetProp(k) \
  (UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType) \
    { if (index >= ARRAY_SIZE(k)) return E_INVALIDARG; \
    *propID = k[index]; *varType = k7z_PROPID_To_VARTYPE[(unsigned)*propID];  *name = 0; return S_OK; } \


struct CStatProp
{
  const char *Name;
  UInt32 PropID;
  VARTYPE vt;
};

namespace NWindows {
namespace NCOM {
// PropVariant.cpp
BSTR AllocBstrFromAscii(const char *s) throw();
}}

#define IMP_IInArchive_GetProp_WITH_NAME(k) \
  (UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType) \
    { if (index >= ARRAY_SIZE(k)) return E_INVALIDARG; \
    const CStatProp &prop = k[index]; \
    *propID = (PROPID)prop.PropID; *varType = prop.vt; \
    *name = NWindows::NCOM::AllocBstrFromAscii(prop.Name); return S_OK; } \

#define IMP_IInArchive_Props \
  STDMETHODIMP CHandler::GetNumberOfProperties(UInt32 *numProps) \
    { *numProps = ARRAY_SIZE(kProps); return S_OK; } \
  STDMETHODIMP CHandler::GetPropertyInfo IMP_IInArchive_GetProp(kProps)

#define IMP_IInArchive_Props_WITH_NAME \
  STDMETHODIMP CHandler::GetNumberOfProperties(UInt32 *numProps) \
    { *numProps = ARRAY_SIZE(kProps); return S_OK; } \
  STDMETHODIMP CHandler::GetPropertyInfo IMP_IInArchive_GetProp_WITH_NAME(kProps)


#define IMP_IInArchive_ArcProps \
  STDMETHODIMP CHandler::GetNumberOfArchiveProperties(UInt32 *numProps) \
    { *numProps = ARRAY_SIZE(kArcProps); return S_OK; } \
  STDMETHODIMP CHandler::GetArchivePropertyInfo IMP_IInArchive_GetProp(kArcProps)

#define IMP_IInArchive_ArcProps_WITH_NAME \
  STDMETHODIMP CHandler::GetNumberOfArchiveProperties(UInt32 *numProps) \
    { *numProps = ARRAY_SIZE(kArcProps); return S_OK; } \
  STDMETHODIMP CHandler::GetArchivePropertyInfo IMP_IInArchive_GetProp_WITH_NAME(kArcProps)

#define IMP_IInArchive_ArcProps_NO_Table \
  STDMETHODIMP CHandler::GetNumberOfArchiveProperties(UInt32 *numProps) \
    { *numProps = 0; return S_OK; } \
  STDMETHODIMP CHandler::GetArchivePropertyInfo(UInt32, BSTR *, PROPID *, VARTYPE *) \
    { return E_NOTIMPL; } \

#define IMP_IInArchive_ArcProps_NO \
  IMP_IInArchive_ArcProps_NO_Table \
  STDMETHODIMP CHandler::GetArchiveProperty(PROPID, PROPVARIANT *value) \
    { value->vt = VT_EMPTY; return S_OK; }



#define k_IsArc_Res_NO   0
#define k_IsArc_Res_YES  1
#define k_IsArc_Res_NEED_MORE 2
// #define k_IsArc_Res_YES_LOW_PROB 3

#define API_FUNC_IsArc EXTERN_C UInt32 WINAPI
#define API_FUNC_static_IsArc extern "C" { static UInt32 WINAPI

extern "C"
{
  typedef HRESULT (WINAPI *Func_CreateObject)(const GUID *clsID, const GUID *iid, void **outObject);

  typedef UInt32 (WINAPI *Func_IsArc)(const Byte *p, size_t size);
  typedef HRESULT (WINAPI *Func_GetIsArc)(UInt32 formatIndex, Func_IsArc *isArc);

  typedef HRESULT (WINAPI *Func_GetNumberOfFormats)(UInt32 *numFormats);
  typedef HRESULT (WINAPI *Func_GetHandlerProperty)(PROPID propID, PROPVARIANT *value);
  typedef HRESULT (WINAPI *Func_GetHandlerProperty2)(UInt32 index, PROPID propID, PROPVARIANT *value);

  typedef HRESULT (WINAPI *Func_SetCaseSensitive)(Int32 caseSensitive);
  typedef HRESULT (WINAPI *Func_SetLargePageMode)();

  typedef IOutArchive * (*Func_CreateOutArchive)();
  typedef IInArchive * (*Func_CreateInArchive)();
}

#endif

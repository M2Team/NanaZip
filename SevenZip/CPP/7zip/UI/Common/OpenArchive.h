// OpenArchive.h

#ifndef __OPEN_ARCHIVE_H
#define __OPEN_ARCHIVE_H

#include "../../../Windows/PropVariant.h"

#include "ArchiveOpenCallback.h"
#include "LoadCodecs.h"
#include "Property.h"

#ifndef _SFX

#define SUPPORT_ALT_STREAMS

#endif

HRESULT Archive_GetItemBoolProp(IInArchive *arc, UInt32 index, PROPID propID, bool &result) throw();
HRESULT Archive_IsItem_Dir(IInArchive *arc, UInt32 index, bool &result) throw();
HRESULT Archive_IsItem_Aux(IInArchive *arc, UInt32 index, bool &result) throw();
HRESULT Archive_IsItem_AltStream(IInArchive *arc, UInt32 index, bool &result) throw();
HRESULT Archive_IsItem_Deleted(IInArchive *arc, UInt32 index, bool &deleted) throw();

#ifdef SUPPORT_ALT_STREAMS
int FindAltStreamColon_in_Path(const wchar_t *path);
#endif

/*
struct COptionalOpenProperties
{
  UString FormatName;
  CObjectVector<CProperty> Props;
};
*/

#ifdef _SFX
#define OPEN_PROPS_DECL
#else
#define OPEN_PROPS_DECL const CObjectVector<CProperty> *props;
// #define OPEN_PROPS_DECL , const CObjectVector<COptionalOpenProperties> *props
#endif

struct COpenSpecFlags
{
  // bool CanReturnFull;
  bool CanReturnFrontal;
  bool CanReturnTail;
  bool CanReturnMid;

  bool CanReturn_NonStart() const { return CanReturnTail || CanReturnMid; }

  COpenSpecFlags():
    // CanReturnFull(true),
    CanReturnFrontal(false),
    CanReturnTail(false),
    CanReturnMid(false)
    {}
};

struct COpenType
{
  int FormatIndex;

  COpenSpecFlags SpecForcedType;
  COpenSpecFlags SpecMainType;
  COpenSpecFlags SpecWrongExt;
  COpenSpecFlags SpecUnknownExt;

  bool Recursive;

  bool CanReturnArc;
  bool CanReturnParser;
  bool EachPos;

  // bool SkipSfxStub;
  // bool ExeAsUnknown;

  bool ZerosTailIsAllowed;

  bool MaxStartOffset_Defined;
  UInt64 MaxStartOffset;

  const COpenSpecFlags &GetSpec(bool isForced, bool isMain, bool isUnknown) const
  {
    return isForced ? SpecForcedType : (isMain ? SpecMainType : (isUnknown ? SpecUnknownExt : SpecWrongExt));
  }

  COpenType():
      FormatIndex(-1),
      Recursive(true),
      CanReturnArc(true),
      CanReturnParser(false),
      EachPos(false),
      // SkipSfxStub(true),
      // ExeAsUnknown(true),
      ZerosTailIsAllowed(false),
      MaxStartOffset_Defined(false),
      MaxStartOffset(0)
  {
    SpecForcedType.CanReturnFrontal = true;
    SpecForcedType.CanReturnTail = true;
    SpecForcedType.CanReturnMid = true;

    SpecMainType.CanReturnFrontal = true;

    SpecUnknownExt.CanReturnTail = true; // for sfx
    SpecUnknownExt.CanReturnMid = true;
    SpecUnknownExt.CanReturnFrontal = true; // for alt streams of sfx with pad

    // ZerosTailIsAllowed = true;
  }
};

struct COpenOptions
{
  CCodecs *codecs;
  COpenType openType;
  const CObjectVector<COpenType> *types;
  const CIntVector *excludedFormats;

  IInStream *stream;
  ISequentialInStream *seqStream;
  IArchiveOpenCallback *callback;
  COpenCallbackImp *callbackSpec; // it's used for SFX only
  OPEN_PROPS_DECL
  // bool openOnlySpecifiedByExtension,

  bool stdInMode;
  UString filePath;

  COpenOptions():
      codecs(NULL),
      types(NULL),
      excludedFormats(NULL),
      stream(NULL),
      seqStream(NULL),
      callback(NULL),
      callbackSpec(NULL),
      stdInMode(false)
    {}

};

UInt32 GetOpenArcErrorFlags(const NWindows::NCOM::CPropVariant &prop, bool *isDefinedProp = NULL);

struct CArcErrorInfo
{
  bool ThereIsTail;
  bool UnexpecedEnd;
  bool IgnoreTail; // all are zeros
  // bool NonZerosTail;
  bool ErrorFlags_Defined;
  UInt32 ErrorFlags;
  UInt32 WarningFlags;
  int ErrorFormatIndex; // - 1 means no Error.
                        // if FormatIndex == ErrorFormatIndex, the archive is open with offset
  UInt64 TailSize;

  /* if CArc is Open OK with some format:
        - ErrorFormatIndex shows error format index, if extension is incorrect
        - other variables show message and warnings of archive that is open */
  
  UString ErrorMessage;
  UString WarningMessage;

  // call IsArc_After_NonOpen only if Open returns S_FALSE
  bool IsArc_After_NonOpen() const
  {
    return (ErrorFlags_Defined && (ErrorFlags & kpv_ErrorFlags_IsNotArc) == 0);
  }


  CArcErrorInfo():
      ThereIsTail(false),
      UnexpecedEnd(false),
      IgnoreTail(false),
      // NonZerosTail(false),
      ErrorFlags_Defined(false),
      ErrorFlags(0),
      WarningFlags(0),
      ErrorFormatIndex(-1),
      TailSize(0)
    {}

  void ClearErrors();

  void ClearErrors_Full()
  {
    ErrorFormatIndex = -1;
    ClearErrors();
  }

  bool IsThereErrorOrWarning() const
  {
    return ErrorFlags != 0
        || WarningFlags != 0
        || NeedTailWarning()
        || UnexpecedEnd
        || !ErrorMessage.IsEmpty()
        || !WarningMessage.IsEmpty();
  }

  bool AreThereErrors() const { return ErrorFlags != 0 || UnexpecedEnd; }
  bool AreThereWarnings() const { return WarningFlags != 0 || NeedTailWarning(); }

  bool NeedTailWarning() const { return !IgnoreTail && ThereIsTail; }

  UInt32 GetWarningFlags() const
  {
    UInt32 a = WarningFlags;
    if (NeedTailWarning() && (ErrorFlags & kpv_ErrorFlags_DataAfterEnd) == 0)
      a |= kpv_ErrorFlags_DataAfterEnd;
    return a;
  }

  UInt32 GetErrorFlags() const
  {
    UInt32 a = ErrorFlags;
    if (UnexpecedEnd)
      a |= kpv_ErrorFlags_UnexpectedEnd;
    return a;
  }
};

struct CReadArcItem
{
  UString Path;            // Path from root (including alt stream name, if alt stream)
  UStringVector PathParts; // without altStream name, path from root or from _baseParentFolder, if _use_baseParentFolder_mode

  #ifdef SUPPORT_ALT_STREAMS
  UString MainPath;
                /* MainPath = Path for non-AltStream,
                   MainPath = Path of parent, if there is parent for AltStream. */
  UString AltStreamName;
  bool IsAltStream;
  bool WriteToAltStreamIfColon;
  #endif

  bool IsDir;
  bool MainIsDir;
  UInt32 ParentIndex; // use it, if IsAltStream

  #ifndef _SFX
  bool _use_baseParentFolder_mode;
  int _baseParentFolder;
  #endif

  CReadArcItem()
  {
    #ifdef SUPPORT_ALT_STREAMS
    WriteToAltStreamIfColon = false;
    #endif

    #ifndef _SFX
    _use_baseParentFolder_mode = false;
    _baseParentFolder = -1;
    #endif
  }
};

class CArc
{
  HRESULT PrepareToOpen(const COpenOptions &op, unsigned formatIndex, CMyComPtr<IInArchive> &archive);
  HRESULT CheckZerosTail(const COpenOptions &op, UInt64 offset);
  HRESULT OpenStream2(const COpenOptions &options);

  #ifndef _SFX
  // parts.Back() can contain alt stream name "nams:AltName"
  HRESULT GetItemPathToParent(UInt32 index, UInt32 parent, UStringVector &parts) const;
  #endif

public:
  CMyComPtr<IInArchive> Archive;
  CMyComPtr<IInStream> InStream;
          // we use InStream in 2 cases (ArcStreamOffset != 0):
          // 1) if we use additional cache stream
          // 2) we reopen sfx archive with CTailInStream
  
  CMyComPtr<IArchiveGetRawProps> GetRawProps;
  CMyComPtr<IArchiveGetRootProps> GetRootProps;

  CArcErrorInfo ErrorInfo; // for OK archives
  CArcErrorInfo NonOpen_ErrorInfo; // ErrorInfo for mainArchive (false OPEN)

  UString Path;
  UString filePath;
  UString DefaultName;
  int FormatIndex; // - 1 means Parser.
  UInt32 SubfileIndex; // (UInt32)(Int32)-1; means no subfile
  FILETIME MTime;
  bool MTimeDefined;
  
  Int64 Offset; // it's offset of start of archive inside stream that is open by Archive Handler
  UInt64 PhySize;
  // UInt64 OkPhySize;
  bool PhySizeDefined;
  // bool OkPhySize_Defined;
  UInt64 FileSize;
  UInt64 AvailPhySize; // PhySize, but it's reduced if exceed end of file
  // bool offsetDefined;

  UInt64 GetEstmatedPhySize() const { return PhySizeDefined ? PhySize : FileSize; }

  UInt64 ArcStreamOffset; // offset of stream that is open by Archive Handler
  Int64 GetGlobalOffset() const { return (Int64)ArcStreamOffset + Offset; } // it's global offset of archive

  // AString ErrorFlagsText;

  bool IsParseArc;

  bool IsTree;
  bool IsReadOnly;
  
  bool Ask_Deleted;
  bool Ask_AltStream;
  bool Ask_Aux;
  bool Ask_INode;

  bool IgnoreSplit; // don't try split handler

  // void Set_ErrorFlagsText();

  CArc():
    MTimeDefined(false),
    IsTree(false),
    IsReadOnly(false),
    Ask_Deleted(false),
    Ask_AltStream(false),
    Ask_Aux(false),
    Ask_INode(false),
    IgnoreSplit(false)
    {}

  HRESULT ReadBasicProps(IInArchive *archive, UInt64 startPos, HRESULT openRes);

  // ~CArc();

  HRESULT Close()
  {
    InStream.Release();
    return Archive->Close();
  }

  HRESULT GetItemPath(UInt32 index, UString &result) const;
  HRESULT GetDefaultItemPath(UInt32 index, UString &result) const;
  
  // GetItemPath2 adds [DELETED] dir prefix for deleted items.
  HRESULT GetItemPath2(UInt32 index, UString &result) const;

  HRESULT GetItem(UInt32 index, CReadArcItem &item) const;
  
  HRESULT GetItemSize(UInt32 index, UInt64 &size, bool &defined) const;
  HRESULT GetItemMTime(UInt32 index, FILETIME &ft, bool &defined) const;
  HRESULT IsItemAnti(UInt32 index, bool &result) const
    { return Archive_GetItemBoolProp(Archive, index, kpidIsAnti, result); }


  HRESULT OpenStream(const COpenOptions &options);
  HRESULT OpenStreamOrFile(COpenOptions &options);

  HRESULT ReOpen(const COpenOptions &options);
  
  HRESULT CreateNewTailStream(CMyComPtr<IInStream> &stream);
};

struct CArchiveLink
{
  CObjectVector<CArc> Arcs;
  UStringVector VolumePaths;
  UInt64 VolumesSize;
  bool IsOpen;

  bool PasswordWasAsked;
  // UString Password;

  // int NonOpenErrorFormatIndex; // - 1 means no Error.
  UString NonOpen_ArcPath;

  CArcErrorInfo NonOpen_ErrorInfo;

  // UString ErrorsText;
  // void Set_ErrorsText();

  CArchiveLink():
      VolumesSize(0),
      IsOpen(false),
      PasswordWasAsked(false)
      {}

  void KeepModeForNextOpen();
  HRESULT Close();
  void Release();
  ~CArchiveLink() { Release(); }

  const CArc *GetArc() const { return &Arcs.Back(); }
  IInArchive *GetArchive() const { return Arcs.Back().Archive; }
  IArchiveGetRawProps *GetArchiveGetRawProps() const { return Arcs.Back().GetRawProps; }
  IArchiveGetRootProps *GetArchiveGetRootProps() const { return Arcs.Back().GetRootProps; }

  /*
  Open() opens archive and COpenOptions::callback
  Open2() uses COpenCallbackImp that implements Volumes and password callback
  Open3() calls Open2() and callbackUI->Open_Finished();
  Open_Strict() returns S_FALSE also in case, if there is non-open expected nested archive.
  */

  HRESULT Open(COpenOptions &options);
  HRESULT Open2(COpenOptions &options, IOpenCallbackUI *callbackUI);
  HRESULT Open3(COpenOptions &options, IOpenCallbackUI *callbackUI);

  HRESULT Open_Strict(COpenOptions &options, IOpenCallbackUI *callbackUI)
  {
    HRESULT result = Open3(options, callbackUI);
    if (result == S_OK && NonOpen_ErrorInfo.ErrorFormatIndex >= 0)
      result = S_FALSE;
    return result;
  }

  HRESULT ReOpen(COpenOptions &options);
};

bool ParseOpenTypes(CCodecs &codecs, const UString &s, CObjectVector<COpenType> &types);


struct CDirPathSortPair
{
  unsigned Len;
  unsigned Index;

  void SetNumSlashes(const FChar *s);
  
  int Compare(const CDirPathSortPair &a) const
  {
    // We need sorting order where parent items will be after child items
    if (Len < a.Len) return 1;
    if (Len > a.Len) return -1;
    if (Index < a.Index) return -1;
    if (Index > a.Index) return 1;
    return 0;
  }
};

#endif

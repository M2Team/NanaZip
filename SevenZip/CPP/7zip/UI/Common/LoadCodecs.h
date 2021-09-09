// LoadCodecs.h

#ifndef __LOAD_CODECS_H
#define __LOAD_CODECS_H

/*
Client application uses LoadCodecs.* to load plugins to
CCodecs object, that contains 3 lists of plugins:
  1) Formats - internal and external archive handlers
  2) Codecs  - external codecs
  3) Hashers - external hashers

EXTERNAL_CODECS
---------------

  if EXTERNAL_CODECS is defined, then the code tries to load external
  plugins from DLL files (shared libraries).

  There are two types of executables in 7-Zip:
  
  1) Executable that uses external plugins must be compiled
     with EXTERNAL_CODECS defined:
       - 7z.exe, 7zG.exe, 7zFM.exe
    
     Note: EXTERNAL_CODECS is used also in CPP/7zip/Common/CreateCoder.h
           that code is used in plugin module (7z.dll).
  
  2) Standalone modules are compiled without EXTERNAL_CODECS:
    - SFX modules: 7z.sfx, 7zCon.sfx
    - standalone versions of console 7-Zip: 7za.exe, 7zr.exe

  if EXTERNAL_CODECS is defined, CCodecs class implements interfaces:
    - ICompressCodecsInfo : for Codecs
    - IHashers            : for Hashers
  
  The client application can send CCodecs object to each plugin module.
  And plugin module can use ICompressCodecsInfo or IHashers interface to access
  another plugins.

  There are 2 ways to send (ICompressCodecsInfo * compressCodecsInfo) to plugin
    1) for old versions:
        a) request ISetCompressCodecsInfo from created archive handler.
        b) call ISetCompressCodecsInfo::SetCompressCodecsInfo(compressCodecsInfo)
    2) for new versions:
        a) request "SetCodecs" function from DLL file
        b) call SetCodecs(compressCodecsInfo) function from DLL file
*/

#include "../../../Common/MyBuffer.h"
#include "../../../Common/MyCom.h"
#include "../../../Common/MyString.h"
#include "../../../Common/ComTry.h"

#ifdef EXTERNAL_CODECS
#include "../../../Windows/DLL.h"
#endif

#include "../../ICoder.h"

#include "../../Archive/IArchive.h"


#ifdef EXTERNAL_CODECS

struct CDllCodecInfo
{
  unsigned LibIndex;
  UInt32 CodecIndex;
  bool EncoderIsAssigned;
  bool DecoderIsAssigned;
  CLSID Encoder;
  CLSID Decoder;
};

struct CDllHasherInfo
{
  unsigned LibIndex;
  UInt32 HasherIndex;
};

#endif

struct CArcExtInfo
{
  UString Ext;
  UString AddExt;
  
  CArcExtInfo() {}
  CArcExtInfo(const UString &ext): Ext(ext) {}
  CArcExtInfo(const UString &ext, const UString &addExt): Ext(ext), AddExt(addExt) {}
};


struct CArcInfoEx
{
  UInt32 Flags;
  
  Func_CreateInArchive CreateInArchive;
  Func_IsArc IsArcFunc;

  UString Name;
  CObjectVector<CArcExtInfo> Exts;
  
  #ifndef _SFX
    Func_CreateOutArchive CreateOutArchive;
    bool UpdateEnabled;
    bool NewInterface;
    // UInt32 Version;
    UInt32 SignatureOffset;
    CObjectVector<CByteBuffer> Signatures;
    #ifdef NEW_FOLDER_INTERFACE
      UStringVector AssociateExts;
    #endif
  #endif
  
  #ifdef EXTERNAL_CODECS
    int LibIndex;
    UInt32 FormatIndex;
    CLSID ClassID;
  #endif

  bool Flags_KeepName() const { return (Flags & NArcInfoFlags::kKeepName) != 0; }
  bool Flags_FindSignature() const { return (Flags & NArcInfoFlags::kFindSignature) != 0; }

  bool Flags_AltStreams() const { return (Flags & NArcInfoFlags::kAltStreams) != 0; }
  bool Flags_NtSecure() const { return (Flags & NArcInfoFlags::kNtSecure) != 0; }
  bool Flags_SymLinks() const { return (Flags & NArcInfoFlags::kSymLinks) != 0; }
  bool Flags_HardLinks() const { return (Flags & NArcInfoFlags::kHardLinks) != 0; }

  bool Flags_UseGlobalOffset() const { return (Flags & NArcInfoFlags::kUseGlobalOffset) != 0; }
  bool Flags_StartOpen() const { return (Flags & NArcInfoFlags::kStartOpen) != 0; }
  bool Flags_BackwardOpen() const { return (Flags & NArcInfoFlags::kBackwardOpen) != 0; }
  bool Flags_PreArc() const { return (Flags & NArcInfoFlags::kPreArc) != 0; }
  bool Flags_PureStartOpen() const { return (Flags & NArcInfoFlags::kPureStartOpen) != 0; }
  bool Flags_ByExtOnlyOpen() const { return (Flags & NArcInfoFlags::kByExtOnlyOpen) != 0; }

  UString GetMainExt() const
  {
    if (Exts.IsEmpty())
      return UString();
    return Exts[0].Ext;
  }
  int FindExtension(const UString &ext) const;
  
  /*
  UString GetAllExtensions() const
  {
    UString s;
    for (int i = 0; i < Exts.Size(); i++)
    {
      if (i > 0)
        s += ' ';
      s += Exts[i].Ext;
    }
    return s;
  }
  */

  void AddExts(const UString &ext, const UString &addExt);

  bool IsSplit() const { return StringsAreEqualNoCase_Ascii(Name, "Split"); }
  // bool IsRar() const { return StringsAreEqualNoCase_Ascii(Name, "Rar"); }

  CArcInfoEx():
      Flags(0),
      CreateInArchive(NULL),
      IsArcFunc(NULL)
      #ifndef _SFX
      , CreateOutArchive(NULL)
      , UpdateEnabled(false)
      , NewInterface(false)
      // , Version(0)
      , SignatureOffset(0)
      #endif
      #ifdef EXTERNAL_CODECS
      , LibIndex(-1)
      #endif
  {}
};

#ifdef NEW_FOLDER_INTERFACE

struct CCodecIcons
{
  struct CIconPair
  {
    UString Ext;
    int IconIndex;
  };
  CObjectVector<CIconPair> IconPairs;

  void LoadIcons(HMODULE m);
  bool FindIconIndex(const UString &ext, int &iconIndex) const;
};

#endif

#ifdef EXTERNAL_CODECS

struct CCodecLib
  #ifdef NEW_FOLDER_INTERFACE
    : public CCodecIcons
  #endif
{
  NWindows::NDLL::CLibrary Lib;
  FString Path;
  
  Func_CreateObject CreateObject;
  Func_GetMethodProperty GetMethodProperty;
  Func_CreateDecoder CreateDecoder;
  Func_CreateEncoder CreateEncoder;
  Func_SetCodecs SetCodecs;

  CMyComPtr<IHashers> ComHashers;
  
  #ifdef NEW_FOLDER_INTERFACE
  void LoadIcons() { CCodecIcons::LoadIcons((HMODULE)Lib); }
  #endif
  
  CCodecLib():
      CreateObject(NULL),
      GetMethodProperty(NULL),
      CreateDecoder(NULL),
      CreateEncoder(NULL),
      SetCodecs(NULL)
      {}
};

#endif

struct CCodecError
{
  FString Path;
  HRESULT ErrorCode;
  AString Message;
  CCodecError(): ErrorCode(0) {}
};

class CCodecs:
  #ifdef EXTERNAL_CODECS
    public ICompressCodecsInfo,
    public IHashers,
  #else
    public IUnknown,
  #endif
  public CMyUnknownImp
{
  CLASS_NO_COPY(CCodecs);
public:
  #ifdef EXTERNAL_CODECS
  
  CObjectVector<CCodecLib> Libs;
  FString MainDll_ErrorPath;
  CObjectVector<CCodecError> Errors;
  
  void AddLastError(const FString &path);
  void CloseLibs();

  class CReleaser
  {
    CLASS_NO_COPY(CReleaser);

    /* CCodecsReleaser object releases CCodecs links.
         1) CCodecs is COM object that is deleted when all links to that object will be released/
         2) CCodecs::Libs[i] can hold (ICompressCodecsInfo *) link to CCodecs object itself.
       To break that reference loop, we must close all CCodecs::Libs in CCodecsReleaser desttructor. */

    CCodecs *_codecs;
      
    public:
    CReleaser(): _codecs(NULL) {}
    void Set(CCodecs *codecs) { _codecs = codecs; }
    ~CReleaser() { if (_codecs) _codecs->CloseLibs(); }
  };

  bool NeedSetLibCodecs; // = false, if we don't need to set codecs for archive handler via ISetCompressCodecsInfo

  HRESULT LoadCodecs();
  HRESULT LoadFormats();
  HRESULT LoadDll(const FString &path, bool needCheckDll, bool *loadedOK = NULL);
  HRESULT LoadDllsFromFolder(const FString &folderPrefix);

  HRESULT CreateArchiveHandler(const CArcInfoEx &ai, bool outHandler, void **archive) const
  {
    return Libs[(unsigned)ai.LibIndex].CreateObject(&ai.ClassID, outHandler ? &IID_IOutArchive : &IID_IInArchive, (void **)archive);
  }
  
  #endif

  #ifdef NEW_FOLDER_INTERFACE
  CCodecIcons InternalIcons;
  #endif

  CObjectVector<CArcInfoEx> Formats;
  
  #ifdef EXTERNAL_CODECS
  CRecordVector<CDllCodecInfo> Codecs;
  CRecordVector<CDllHasherInfo> Hashers;
  #endif

  bool CaseSensitiveChange;
  bool CaseSensitive;

  CCodecs():
      #ifdef EXTERNAL_CODECS
      NeedSetLibCodecs(true),
      #endif
      CaseSensitiveChange(false),
      CaseSensitive(false)
      {}

  ~CCodecs()
  {
    // OutputDebugStringA("~CCodecs");
  }
 
  const wchar_t *GetFormatNamePtr(int formatIndex) const
  {
    return formatIndex < 0 ? L"#" : (const wchar_t *)Formats[(unsigned)formatIndex].Name;
  }

  HRESULT Load();

  #ifndef _SFX
  int FindFormatForArchiveName(const UString &arcPath) const;
  int FindFormatForExtension(const UString &ext) const;
  int FindFormatForArchiveType(const UString &arcType) const;
  bool FindFormatForArchiveType(const UString &arcType, CIntVector &formatIndices) const;
  #endif

  #ifdef EXTERNAL_CODECS

  MY_UNKNOWN_IMP2(ICompressCodecsInfo, IHashers)
    
  STDMETHOD(GetNumMethods)(UInt32 *numMethods);
  STDMETHOD(GetProperty)(UInt32 index, PROPID propID, PROPVARIANT *value);
  STDMETHOD(CreateDecoder)(UInt32 index, const GUID *iid, void **coder);
  STDMETHOD(CreateEncoder)(UInt32 index, const GUID *iid, void **coder);

  STDMETHOD_(UInt32, GetNumHashers)();
  STDMETHOD(GetHasherProp)(UInt32 index, PROPID propID, PROPVARIANT *value);
  STDMETHOD(CreateHasher)(UInt32 index, IHasher **hasher);

  #else

  MY_UNKNOWN_IMP

  #endif // EXTERNAL_CODECS

  
  #ifdef EXTERNAL_CODECS

  int GetCodec_LibIndex(UInt32 index) const;
  bool GetCodec_DecoderIsAssigned(UInt32 index) const;
  bool GetCodec_EncoderIsAssigned(UInt32 index) const;
  UInt32 GetCodec_NumStreams(UInt32 index);
  HRESULT GetCodec_Id(UInt32 index, UInt64 &id);
  AString GetCodec_Name(UInt32 index);

  int GetHasherLibIndex(UInt32 index);
  UInt64 GetHasherId(UInt32 index);
  AString GetHasherName(UInt32 index);
  UInt32 GetHasherDigestSize(UInt32 index);

  void GetCodecsErrorMessage(UString &s);

  #endif

  HRESULT CreateInArchive(unsigned formatIndex, CMyComPtr<IInArchive> &archive) const
  {
    const CArcInfoEx &ai = Formats[formatIndex];
    #ifdef EXTERNAL_CODECS
    if (ai.LibIndex < 0)
    #endif
    {
      COM_TRY_BEGIN
      archive = ai.CreateInArchive();
      return S_OK;
      COM_TRY_END
    }
    #ifdef EXTERNAL_CODECS
    return CreateArchiveHandler(ai, false, (void **)&archive);
    #endif
  }
  
  #ifndef _SFX

  HRESULT CreateOutArchive(unsigned formatIndex, CMyComPtr<IOutArchive> &archive) const
  {
    const CArcInfoEx &ai = Formats[formatIndex];
    #ifdef EXTERNAL_CODECS
    if (ai.LibIndex < 0)
    #endif
    {
      COM_TRY_BEGIN
      archive = ai.CreateOutArchive();
      return S_OK;
      COM_TRY_END
    }
    
    #ifdef EXTERNAL_CODECS
    return CreateArchiveHandler(ai, true, (void **)&archive);
    #endif
  }
  
  int FindOutFormatFromName(const UString &name) const
  {
    FOR_VECTOR (i, Formats)
    {
      const CArcInfoEx &arc = Formats[i];
      if (!arc.UpdateEnabled)
        continue;
      if (arc.Name.IsEqualTo_NoCase(name))
        return (int)i;
    }
    return -1;
  }

  #endif // _SFX
};

#ifdef EXTERNAL_CODECS
  #define CREATE_CODECS_OBJECT \
    CCodecs *codecs = new CCodecs; \
    CExternalCodecs __externalCodecs; \
    __externalCodecs.GetCodecs = codecs; \
    __externalCodecs.GetHashers = codecs; \
    CCodecs::CReleaser codecsReleaser; \
    codecsReleaser.Set(codecs);
#else
  #define CREATE_CODECS_OBJECT \
    CCodecs *codecs = new CCodecs; \
    CMyComPtr<IUnknown> __codecsRef = codecs;
#endif
  
#endif

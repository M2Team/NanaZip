// HashCalc.h

#ifndef __HASH_CALC_H
#define __HASH_CALC_H

#include "../../../Common/UTFConvert.h"
#include "../../../Common/Wildcard.h"

#include "../../Common/CreateCoder.h"
#include "../../Common/MethodProps.h"

#include "DirItem.h"
#include "IFileExtractCallback.h"

const unsigned k_HashCalc_DigestSize_Max = 64;
const unsigned k_HashCalc_ExtraSize = 8;
const unsigned k_HashCalc_NumGroups = 4;

enum
{
  k_HashCalc_Index_Current,
  k_HashCalc_Index_DataSum,
  k_HashCalc_Index_NamesSum,
  k_HashCalc_Index_StreamsSum
};

struct CHasherState
{
  CMyComPtr<IHasher> Hasher;
  AString Name;
  UInt32 DigestSize;
  UInt64 NumSums[k_HashCalc_NumGroups];
  Byte Digests[k_HashCalc_NumGroups][k_HashCalc_DigestSize_Max + k_HashCalc_ExtraSize];

  void InitDigestGroup(unsigned groupIndex)
  {
    NumSums[groupIndex] = 0;
    memset(Digests[groupIndex], 0, sizeof(Digests[groupIndex]));
  }

  const Byte *GetExtraData_for_Group(unsigned groupIndex) const
  {
    return Digests[groupIndex] + k_HashCalc_DigestSize_Max;
  }

  unsigned GetNumExtraBytes_for_Group(unsigned groupIndex) const
  {
    const Byte *p = GetExtraData_for_Group(groupIndex);
    // we use little-endian to read extra bytes
    for (unsigned i = k_HashCalc_ExtraSize; i != 0; i--)
      if (p[i - 1] != 0)
        return i;
    return 0;
  }

  void AddDigest(unsigned groupIndex, const Byte *data);

  void WriteToString(unsigned digestIndex, char *s) const;
};



struct IHashCalc
{
  virtual void InitForNewFile() = 0;
  virtual void Update(const void *data, UInt32 size) = 0;
  virtual void SetSize(UInt64 size) = 0;
  virtual void Final(bool isDir, bool isAltStream, const UString &path) = 0;
};

struct CHashBundle: public IHashCalc
{
  CObjectVector<CHasherState> Hashers;

  UInt64 NumDirs;
  UInt64 NumFiles;
  UInt64 NumAltStreams;
  UInt64 FilesSize;
  UInt64 AltStreamsSize;
  UInt64 NumErrors;

  UInt64 CurSize;

  UString MainName;
  UString FirstFileName;

  HRESULT SetMethods(DECL_EXTERNAL_CODECS_LOC_VARS const UStringVector &methods);
  
  // void Init() {}
  CHashBundle()
  {
    NumDirs = NumFiles = NumAltStreams = FilesSize = AltStreamsSize = NumErrors = 0;
  }

  virtual ~CHashBundle() {};

  void InitForNewFile();
  void Update(const void *data, UInt32 size);
  void SetSize(UInt64 size);
  void Final(bool isDir, bool isAltStream, const UString &path);
};

#define INTERFACE_IHashCallbackUI(x) \
  INTERFACE_IDirItemsCallback(x) \
  virtual HRESULT StartScanning() x; \
  virtual HRESULT FinishScanning(const CDirItemsStat &st) x; \
  virtual HRESULT SetNumFiles(UInt64 numFiles) x; \
  virtual HRESULT SetTotal(UInt64 size) x; \
  virtual HRESULT SetCompleted(const UInt64 *completeValue) x; \
  virtual HRESULT CheckBreak() x; \
  virtual HRESULT BeforeFirstFile(const CHashBundle &hb) x; \
  virtual HRESULT GetStream(const wchar_t *name, bool isFolder) x; \
  virtual HRESULT OpenFileError(const FString &path, DWORD systemError) x; \
  virtual HRESULT SetOperationResult(UInt64 fileSize, const CHashBundle &hb, bool showHash) x; \
  virtual HRESULT AfterLastFile(CHashBundle &hb) x; \

struct IHashCallbackUI: public IDirItemsCallback
{
  INTERFACE_IHashCallbackUI(=0)
};


struct CHashOptionsLocal
{
  CBoolPair HashMode_Zero;
  CBoolPair HashMode_Tag;
  CBoolPair HashMode_Dirs;
  CBoolPair HashMode_OnlyHash;
  
  void Init_HashOptionsLocal()
  {
    HashMode_Zero.Init();
    HashMode_Tag.Init();
    HashMode_Dirs.Init();
    HashMode_OnlyHash.Init();
    // HashMode_Dirs = true; // for debug
  }

  CHashOptionsLocal()
  {
    Init_HashOptionsLocal();
  }
  
  bool ParseFlagCharOption(wchar_t c, bool val)
  {
    c = MyCharLower_Ascii(c);
         if (c == 'z') HashMode_Zero.SetVal_as_Defined(val);
    else if (c == 't') HashMode_Tag.SetVal_as_Defined(val);
    else if (c == 'd') HashMode_Dirs.SetVal_as_Defined(val);
    else if (c == 'h') HashMode_OnlyHash.SetVal_as_Defined(val);
    else return false;
    return true;
  }

  bool ParseString(const UString &s)
  {
    for (unsigned i = 0; i < s.Len();)
    {
      const wchar_t c = s[i++];
      bool val = true;
      if (i < s.Len())
      {
        const wchar_t next  = s[i];
        if (next == '-')
        {
          val = false;
          i++;
        }
      }
      if (!ParseFlagCharOption(c, val))
        return false;
    }
    return true;
  }
};


struct CHashOptions
  // : public CHashOptionsLocal
{
  UStringVector Methods;
  // UString HashFilePath;

  bool PreserveATime;
  bool OpenShareForWrite;
  bool StdInMode;
  bool AltStreamsMode;
  CBoolPair SymLinks;

  NWildcard::ECensorPathMode PathMode;

  CHashOptions():
      PreserveATime(false),
      OpenShareForWrite(false),
      StdInMode(false),
      AltStreamsMode(false),
      PathMode(NWildcard::k_RelatPath) {};
};


HRESULT HashCalc(
    DECL_EXTERNAL_CODECS_LOC_VARS
    const NWildcard::CCensor &censor,
    const CHashOptions &options,
    AString &errorInfo,
    IHashCallbackUI *callback);



#ifndef _SFX

namespace NHash {

struct CHashPair
{
  CByteBuffer Hash;
  char Mode;
  bool IsBSD;
  bool Size_from_Arc_Defined;
  bool Size_from_Disk_Defined;
  AString Method;
  AString Name;
  
  AString FullLine;
  AString HashString;
  // unsigned HashLengthInBits;

  // AString MethodName;
  UInt64 Size_from_Arc;
  UInt64 Size_from_Disk;

  bool IsDir() const;

  void Get_UString_Path(UString &path) const
  {
    path.Empty();
    if (!ConvertUTF8ToUnicode(Name, path))
      return;
  }

  bool ParseCksum(const char *s);
  bool Parse(const char *s);

  bool IsSupportedMode() const
  {
    return Mode != 'U' && Mode != '^';
  }

  CHashPair():
      Mode(0)
      , IsBSD(false)
      , Size_from_Arc_Defined(false)
      , Size_from_Disk_Defined(false)
      // , HashLengthInBits(0)
      , Size_from_Arc(0)
      , Size_from_Disk(0)
    {}
};


class CHandler:
  public IInArchive,
  public IArchiveGetRawProps,
  // public IGetArchiveHashHandler,
  public IOutArchive,
  public ISetProperties,
  public CMyUnknownImp
{
  bool _isArc;
  UInt64 _phySize;
  CObjectVector<CHashPair> HashPairs;
  UString _nameExtenstion;
  // UString _method_fromName;
  AString _pgpMethod;
  bool _is_CksumMode;
  bool _is_PgpMethod;
  bool _is_ZeroMode;
  bool _are_there_Tags;
  bool _are_there_Dirs;
  bool _hashSize_Defined;
  unsigned _hashSize;

  bool _crcSize_WasSet;
  UInt32 _crcSize;
  UStringVector _methods;

  void ClearVars();

  void InitProps()
  {
    _crcSize_WasSet = false;
    _crcSize = 4;
    _methods.Clear();
    _options.Init_HashOptionsLocal();
  }

  CHashOptionsLocal _options;

  bool CanUpdate() const
  {
    if (!_isArc || _is_PgpMethod || _is_CksumMode)
      return false;
    return true;

  }

  HRESULT SetProperty(const wchar_t *nameSpec, const PROPVARIANT &value);

public:

  CHandler();

  MY_UNKNOWN_IMP4(
      IInArchive,
      IArchiveGetRawProps,
      IOutArchive,
      ISetProperties
      /*, IGetArchiveHashHandler */
      )
  INTERFACE_IInArchive(;)
  INTERFACE_IOutArchive(;)
  INTERFACE_IArchiveGetRawProps(;)
  // STDMETHOD(GetArchiveHashHandler)(CHandler **handler);
  STDMETHOD(SetProperties)(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps);
};

}

void Codecs_AddHashArcHandler(CCodecs *codecs);

#endif


#endif

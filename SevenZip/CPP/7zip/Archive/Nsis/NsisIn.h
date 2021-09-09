// NsisIn.h

#ifndef __ARCHIVE_NSIS_IN_H
#define __ARCHIVE_NSIS_IN_H

#include "../../../../C/CpuArch.h"

#include "../../../Common/DynLimBuf.h"
#include "../../../Common/MyBuffer.h"
#include "../../../Common/MyCom.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/UTFConvert.h"

#include "NsisDecode.h"

/* If NSIS_SCRIPT is defined, it will decompile NSIS script to [NSIS].nsi file.
   The code is much larger in that case. */
 
// #define NSIS_SCRIPT

namespace NArchive {
namespace NNsis {

const size_t kScriptSizeLimit = 1 << 27;

const unsigned kSignatureSize = 16;
extern const Byte kSignature[kSignatureSize];
#define NSIS_SIGNATURE { 0xEF, 0xBE, 0xAD, 0xDE, 'N', 'u', 'l', 'l', 's', 'o', 'f', 't', 'I', 'n', 's', 't' }

const UInt32 kFlagsMask = 0xF;
namespace NFlags
{
  const UInt32 kUninstall = 1;
  const UInt32 kSilent = 2;
  const UInt32 kNoCrc = 4;
  const UInt32 kForceCrc = 8;
  // NSISBI fork flags:
  const UInt32 k_BI_LongOffset = 16;
  const UInt32 k_BI_ExternalFileSupport = 32;
  const UInt32 k_BI_ExternalFile = 64;
  const UInt32 k_BI_IsStubInstaller = 128;
}

struct CFirstHeader
{
  UInt32 Flags;
  UInt32 HeaderSize;
  UInt32 ArcSize;

  bool ThereIsCrc() const
  {
    return
        (Flags & NFlags::kForceCrc) != 0 ||
        (Flags & NFlags::kNoCrc) == 0;
  }

  UInt32 GetDataSize() const { return ArcSize - (ThereIsCrc() ? 4 : 0); }
};


struct CBlockHeader
{
  UInt32 Offset;
  UInt32 Num;

  void Parse(const Byte *p, unsigned bhoSize);
};

struct CItem
{
  bool IsCompressed;
  bool Size_Defined;
  bool CompressedSize_Defined;
  bool EstimatedSize_Defined;
  bool Attrib_Defined;
  bool IsUninstaller;
  // bool UseFilter;
  
  UInt32 Attrib;
  UInt32 Pos;
  UInt32 Size;
  UInt32 CompressedSize;
  UInt32 EstimatedSize;
  UInt32 DictionarySize;
  UInt32 PatchSize; // for Uninstaller.exe
  int Prefix; // - 1 means no prefix

  FILETIME MTime;
  AString NameA;
  UString NameU;
  
  CItem():
      IsCompressed(true),
      Size_Defined(false),
      CompressedSize_Defined(false),
      EstimatedSize_Defined(false),
      Attrib_Defined(false),
      IsUninstaller(false),
      // UseFilter(false),
      Attrib(0),
      Pos(0),
      Size(0),
      CompressedSize(0),
      EstimatedSize(0),
      DictionarySize(1),
      PatchSize(0),
      Prefix(-1)
  {
    MTime.dwLowDateTime = 0;
    MTime.dwHighDateTime = 0;
  }

  /*
  bool IsINSTDIR() const
  {
    return (PrefixA.Len() >= 3 || PrefixU.Len() >= 3);
  }
  */
};

enum ENsisType
{
  k_NsisType_Nsis2,
  k_NsisType_Nsis3,
  k_NsisType_Park1, // Park 2.46.1-
  k_NsisType_Park2, // Park 2.46.2  : GetFontVersion
  k_NsisType_Park3  // Park 2.46.3+ : GetFontName
};

#ifdef NSIS_SCRIPT

struct CSection
{
  UInt32 InstallTypes; // bits set for each of the different install_types, if any.
  UInt32 Flags; // SF_* - defined above
  UInt32 StartCmdIndex; // code;
  UInt32 NumCommands; // code_size;
  UInt32 SizeKB;
  UInt32 Name;

  void Parse(const Byte *data);
};

struct CLicenseFile
{
  UInt32 Offset;
  UInt32 Size;
  AString Name;
  CByteBuffer Text;
};

#endif

class CInArchive
{
public:
  #ifdef NSIS_SCRIPT
  CDynLimBuf Script;
  #endif
  CByteBuffer _data;
  CObjectVector<CItem> Items;
  bool IsUnicode;
  bool Is64Bit;
private:
  UInt32 _stringsPos;     // relative to _data
  UInt32 NumStringChars;
  size_t _size;           // it's Header Size

  AString Raw_AString;
  UString Raw_UString;

  ENsisType NsisType;
  bool IsNsis200; // NSIS 2.03 and before
  bool IsNsis225; // NSIS 2.25 and before
  bool LogCmdIsEnabled;
  int BadCmd; // -1: no bad command; in another cases lowest bad command id

  bool IsPark() const { return NsisType >= k_NsisType_Park1; }

  UInt64 _fileSize;
  
  bool _headerIsCompressed;
  UInt32 _nonSolidStartOffset;

  #ifdef NSIS_SCRIPT
  
  CByteBuffer strUsed;

  CBlockHeader bhPages;
  CBlockHeader bhSections;
  CBlockHeader bhCtlColors;
  CBlockHeader bhData;
  UInt32 AfterHeaderSize;
  CByteBuffer _afterHeader;

  UInt32 SectionSize;
  const Byte *_mainLang;
  UInt32 _numLangStrings;
  AString LangComment;
  CRecordVector<UInt32> langStrIDs;
  UInt32 numOnFunc;
  UInt32 onFuncOffset;
  // CRecordVector<UInt32> OnFuncs;
  unsigned _numRootLicenses;
  CRecordVector<UInt32> noParseStringIndexes;
  AString _tempString_for_GetVar;
  AString _tempString_for_AddFuncName;
  AString _tempString;

  #endif


public:
  CMyComPtr<IInStream> _stream; // it's limited stream that contains only NSIS archive
  UInt64 StartOffset;           // offset in original stream.
  UInt64 DataStreamOffset;      // = sizeof(FirstHeader) = offset of Header in _stream

  bool IsArc;

  CDecoder Decoder;
  CByteBuffer ExeStub;
  CFirstHeader FirstHeader;
  NMethodType::EEnum Method;
  UInt32 DictionarySize;
  bool IsSolid;
  bool UseFilter;
  bool FilterFlag;
  
  bool IsInstaller;
  AString Name;
  AString BrandingText;
  UStringVector UPrefixes;
  AStringVector APrefixes;

  #ifdef NSIS_SCRIPT
  CObjectVector<CLicenseFile> LicenseFiles;
  #endif

private:
  void GetShellString(AString &s, unsigned index1, unsigned index2);
  void GetNsisString_Raw(const Byte *s);
  void GetNsisString_Unicode_Raw(const Byte *s);
  void ReadString2_Raw(UInt32 pos);
  bool IsGoodString(UInt32 param) const;
  bool AreTwoParamStringsEqual(UInt32 param1, UInt32 param2) const;

  void Add_LangStr(AString &res, UInt32 id);

  #ifdef NSIS_SCRIPT

  void Add_UInt(UInt32 v);
  void AddLicense(UInt32 param, Int32 langID);

  void Add_LangStr_Simple(UInt32 id);
  void Add_FuncName(const UInt32 *labels, UInt32 index);
  void AddParam_Func(const UInt32 *labels, UInt32 index);
  void Add_LabelName(UInt32 index);

  void Add_Color2(UInt32 v);
  void Add_ColorParam(UInt32 v);
  void Add_Color(UInt32 index);

  void Add_ButtonID(UInt32 buttonID);

  void Add_ShowWindow_Cmd(UInt32 cmd);
  void Add_TypeFromList(const char * const *table, unsigned tableSize, UInt32 type);
  void Add_ExecFlags(UInt32 flagsType);
  void Add_SectOp(UInt32 opType);

  void Add_Var(UInt32 index);
  void AddParam_Var(UInt32 value);
  void AddParam_UInt(UInt32 value);

  void Add_GotoVar(UInt32 param);
  void Add_GotoVar1(UInt32 param);
  void Add_GotoVars2(const UInt32 *params);


 
  bool PrintSectionBegin(const CSection &sect, unsigned index);
  void PrintSectionEnd();

  void GetNsisString(AString &res, const Byte *s);
  void GetNsisString_Unicode(AString &res, const Byte *s);
  UInt32 GetNumUsedVars() const;
  void ReadString2(AString &s, UInt32 pos);

  void MessageBox_MB_Part(UInt32 param);
  void AddParam(UInt32 pos);
  void AddOptionalParam(UInt32 pos);
  void AddParams(const UInt32 *params, unsigned num);
  void AddPageOption1(UInt32 param, const char *name);
  void AddPageOption(const UInt32 *params, unsigned num, const char *name);
  void AddOptionalParams(const UInt32 *params, unsigned num);
  void AddRegRoot(UInt32 value);

 
  void ClearLangComment();
  void Separator();
  void Space();
  void Tab();
  void Tab(bool commented);
  void BigSpaceComment();
  void SmallSpaceComment();
  void AddCommentAndString(const char *s);
  void AddError(const char *s);
  void AddErrorLF(const char *s);
  void CommentOpen();
  void CommentClose();
  void AddLF();
  void AddQuotes();
  void TabString(const char *s);
  void AddStringLF(const char *s);
  void NewLine();
  void PrintNumComment(const char *name, UInt32 value);
  void Add_QuStr(const AString &s);
  void SpaceQuStr(const AString &s);
  bool CompareCommands(const Byte *rawCmds, const Byte *sequence, size_t numCommands);

  #endif

  #ifdef NSIS_SCRIPT
  unsigned GetNumSupportedCommands() const;
  #endif

  UInt32 GetCmd(UInt32 a);
  void FindBadCmd(const CBlockHeader &bh, const Byte *);
  void DetectNsisType(const CBlockHeader &bh, const Byte *);

  HRESULT ReadEntries(const CBlockHeader &bh);
  HRESULT SortItems();
  HRESULT Parse();
  HRESULT Open2(const Byte *data, size_t size);
  void Clear2();

  void GetVar2(AString &res, UInt32 index);
  void GetVar(AString &res, UInt32 index);
  Int32 GetVarIndex(UInt32 strPos) const;
  Int32 GetVarIndex(UInt32 strPos, UInt32 &resOffset) const;
  Int32 GetVarIndexFinished(UInt32 strPos, Byte endChar, UInt32 &resOffset) const;
  bool IsVarStr(UInt32 strPos, UInt32 varIndex) const;
  bool IsAbsolutePathVar(UInt32 strPos) const;
  void SetItemName(CItem &item, UInt32 strPos);

public:
  HRESULT Open(IInStream *inStream, const UInt64 *maxCheckStartPosition);
  AString GetFormatDescription() const;
  HRESULT InitDecoder()
  {
    bool useFilter;
    return Decoder.Init(_stream, useFilter);
  }

  HRESULT SeekTo(UInt64 pos)
  {
    return _stream->Seek(pos, STREAM_SEEK_SET, NULL);
  }

  HRESULT SeekTo_DataStreamOffset()
  {
    return SeekTo(DataStreamOffset);
  }

  HRESULT SeekToNonSolidItem(unsigned index)
  {
    return SeekTo(GetPosOfNonSolidItem(index));
  }

  void Clear();

  bool IsDirectString_Equal(UInt32 offset, const char *s) const;
  /*
  UInt64 GetDataPos(unsigned index)
  {
    const CItem &item = Items[index];
    return GetOffset() + FirstHeader.HeaderSize + item.Pos;
  }
  */

  UInt64 GetPosOfSolidItem(unsigned index) const
  {
    const CItem &item = Items[index];
    return 4 + (UInt64)FirstHeader.HeaderSize + item.Pos;
  }
  
  UInt64 GetPosOfNonSolidItem(unsigned index) const
  {
    const CItem &item = Items[index];
    return DataStreamOffset + _nonSolidStartOffset + 4 + item.Pos;
  }

  void Release()
  {
    Decoder.Release();
  }

  bool IsTruncated() const { return (_fileSize - StartOffset < FirstHeader.ArcSize); }

  UString GetReducedName(unsigned index) const
  {
    const CItem &item = Items[index];

    UString s;
    if (item.Prefix >= 0)
    {
      if (IsUnicode)
        s = UPrefixes[item.Prefix];
      else
        s = MultiByteToUnicodeString(APrefixes[item.Prefix]);
      if (s.Len() > 0)
        if (s.Back() != L'\\')
          s += '\\';
    }

    if (IsUnicode)
    {
      s += item.NameU;
      if (item.NameU.IsEmpty())
        s += "file";
    }
    else
    {
      s += MultiByteToUnicodeString(item.NameA);
      if (item.NameA.IsEmpty())
        s += "file";
    }
    
    const char * const kRemoveStr = "$INSTDIR\\";
    if (s.IsPrefixedBy_Ascii_NoCase(kRemoveStr))
    {
      s.Delete(0, MyStringLen(kRemoveStr));
      if (s[0] == L'\\')
        s.DeleteFrontal(1);
    }
    if (item.IsUninstaller && ExeStub.Size() == 0)
      s += ".nsis";
    return s;
  }

  UString ConvertToUnicode(const AString &s) const;

  CInArchive()
    #ifdef NSIS_SCRIPT
      : Script(kScriptSizeLimit)
    #endif
    {}
};

}}
  
#endif

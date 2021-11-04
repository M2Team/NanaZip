// ArchiveExtractCallback.cpp

#include "StdAfx.h"

#undef sprintf
#undef printf

// #include <stdio.h>
// #include "../../../../C/CpuTicks.h"

#include "../../../../C/Alloc.h"
#include "../../../../C/CpuArch.h"


#include "../../../Common/ComTry.h"
#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/UTFConvert.h"
#include "../../../Common/Wildcard.h"

#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/PropVariant.h"
#include "../../../Windows/PropVariantConv.h"

#if defined(_WIN32) && !defined(UNDER_CE)  && !defined(_SFX)
#define _USE_SECURITY_CODE
#include "../../../Windows/SecurityUtils.h"
#endif

#include "../../Common/FilePathAutoRename.h"
#include "../../Common/StreamUtils.h"

#include "../Common/ExtractingFilePath.h"
#include "../Common/PropIDUtils.h"

#include "ArchiveExtractCallback.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;

static const char * const kCantAutoRename = "Cannot create file with auto name";
static const char * const kCantRenameFile = "Cannot rename existing file";
static const char * const kCantDeleteOutputFile = "Cannot delete output file";
static const char * const kCantDeleteOutputDir = "Cannot delete output folder";
static const char * const kCantOpenOutFile = "Cannot open output file";
static const char * const kCantOpenInFile = "Cannot open input file";
static const char * const kCantSetFileLen = "Cannot set length for output file";
#ifdef SUPPORT_LINKS
static const char * const kCantCreateHardLink = "Cannot create hard link";
static const char * const kCantCreateSymLink = "Cannot create symbolic link";
#endif

#ifndef _SFX

STDMETHODIMP COutStreamWithHash::Write(const void *data, UInt32 size, UInt32 *processedSize)
{
  HRESULT result = S_OK;
  if (_stream)
    result = _stream->Write(data, size, &size);
  if (_calculate)
    _hash->Update(data, size);
  _size += size;
  if (processedSize)
    *processedSize = size;
  return result;
}

#endif // _SFX


#ifdef _USE_SECURITY_CODE
bool InitLocalPrivileges();
bool InitLocalPrivileges()
{
  NSecurity::CAccessToken token;
  if (!token.OpenProcessToken(GetCurrentProcess(),
      TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES))
    return false;

  TOKEN_PRIVILEGES tp;
 
  tp.PrivilegeCount = 1;
  tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  
  if  (!::LookupPrivilegeValue(NULL, SE_SECURITY_NAME, &tp.Privileges[0].Luid))
    return false;
  if (!token.AdjustPrivileges(&tp))
    return false;
  return (GetLastError() == ERROR_SUCCESS);
}
#endif // _USE_SECURITY_CODE


#ifdef SUPPORT_LINKS

int CHardLinkNode::Compare(const CHardLinkNode &a) const
{
  if (StreamId < a.StreamId) return -1;
  if (StreamId > a.StreamId) return 1;
  return MyCompare(INode, a.INode);
}

static HRESULT Archive_Get_HardLinkNode(IInArchive *archive, UInt32 index, CHardLinkNode &h, bool &defined)
{
  h.INode = 0;
  h.StreamId = (UInt64)(Int64)-1;
  defined = false;
  {
    NCOM::CPropVariant prop;
    RINOK(archive->GetProperty(index, kpidINode, &prop));
    if (!ConvertPropVariantToUInt64(prop, h.INode))
      return S_OK;
  }
  {
    NCOM::CPropVariant prop;
    RINOK(archive->GetProperty(index, kpidStreamId, &prop));
    ConvertPropVariantToUInt64(prop, h.StreamId);
  }
  defined = true;
  return S_OK;
}


HRESULT CArchiveExtractCallback::PrepareHardLinks(const CRecordVector<UInt32> *realIndices)
{
  _hardLinks.Clear();

  if (!_arc->Ask_INode)
    return S_OK;
  
  IInArchive *archive = _arc->Archive;
  CRecordVector<CHardLinkNode> &hardIDs = _hardLinks.IDs;

  {
    UInt32 numItems;
    if (realIndices)
      numItems = realIndices->Size();
    else
    {
      RINOK(archive->GetNumberOfItems(&numItems));
    }

    for (UInt32 i = 0; i < numItems; i++)
    {
      CHardLinkNode h;
      bool defined;
      UInt32 realIndex = realIndices ? (*realIndices)[i] : i;

      RINOK(Archive_Get_HardLinkNode(archive, realIndex, h, defined));
      if (defined)
      {
        bool isAltStream = false;
        RINOK(Archive_IsItem_AltStream(archive, realIndex, isAltStream));
        if (!isAltStream)
          hardIDs.Add(h);
      }
    }
  }
  
  hardIDs.Sort2();
  
  {
    // we keep only items that have 2 or more items
    unsigned k = 0;
    unsigned numSame = 1;
    for (unsigned i = 1; i < hardIDs.Size(); i++)
    {
      if (hardIDs[i].Compare(hardIDs[i - 1]) != 0)
        numSame = 1;
      else if (++numSame == 2)
      {
        if (i - 1 != k)
          hardIDs[k] = hardIDs[i - 1];
        k++;
      }
    }
    hardIDs.DeleteFrom(k);
  }
  
  _hardLinks.PrepareLinks();
  return S_OK;
}

#endif // SUPPORT_LINKS


CArchiveExtractCallback::CArchiveExtractCallback():
    _arc(NULL),
    WriteCTime(true),
    WriteATime(true),
    WriteMTime(true),
    _multiArchives(false)
{
  LocalProgressSpec = new CLocalProgress();
  _localProgress = LocalProgressSpec;

  #ifdef _USE_SECURITY_CODE
  _saclEnabled = InitLocalPrivileges();
  #endif
}


void CArchiveExtractCallback::Init(
    const CExtractNtOptions &ntOptions,
    const NWildcard::CCensorNode *wildcardCensor,
    const CArc *arc,
    IFolderArchiveExtractCallback *extractCallback2,
    bool stdOutMode, bool testMode,
    const FString &directoryPath,
    const UStringVector &removePathParts, bool removePartsForAltStreams,
    UInt64 packSize)
{
  ClearExtractedDirsInfo();
  _outFileStream.Release();
  _bufPtrSeqOutStream.Release();
  
  #ifdef SUPPORT_LINKS
  _hardLinks.Clear();
  #endif

  #ifdef SUPPORT_ALT_STREAMS
  _renamedFiles.Clear();
  #endif

  _ntOptions = ntOptions;
  _wildcardCensor = wildcardCensor;

  _stdOutMode = stdOutMode;
  _testMode = testMode;
  
  // _progressTotal = 0;
  // _progressTotal_Defined = false;
  
  _packTotal = packSize;
  _progressTotal = packSize;
  _progressTotal_Defined = true;

  _extractCallback2 = extractCallback2;
  _compressProgress.Release();
  _extractCallback2.QueryInterface(IID_ICompressProgressInfo, &_compressProgress);
  _extractCallback2.QueryInterface(IID_IArchiveExtractCallbackMessage, &_callbackMessage);
  _extractCallback2.QueryInterface(IID_IFolderArchiveExtractCallback2, &_folderArchiveExtractCallback2);

  #ifndef _SFX

  _extractCallback2.QueryInterface(IID_IFolderExtractToStreamCallback, &ExtractToStreamCallback);
  if (ExtractToStreamCallback)
  {
    Int32 useStreams = 0;
    if (ExtractToStreamCallback->UseExtractToStream(&useStreams) != S_OK)
      useStreams = 0;
    if (useStreams == 0)
      ExtractToStreamCallback.Release();
  }
  
  #endif

  LocalProgressSpec->Init(extractCallback2, true);
  LocalProgressSpec->SendProgress = false;
 
  _removePathParts = removePathParts;
  _removePartsForAltStreams = removePartsForAltStreams;

  #ifndef _SFX
  _baseParentFolder = (UInt32)(Int32)-1;
  _use_baseParentFolder_mode = false;
  #endif

  _arc = arc;
  _dirPathPrefix = directoryPath;
  _dirPathPrefix_Full = directoryPath;
  #if defined(_WIN32) && !defined(UNDER_CE)
  if (!NName::IsAltPathPrefix(_dirPathPrefix))
  #endif
  {
    NName::NormalizeDirPathPrefix(_dirPathPrefix);
    NDir::MyGetFullPathName(directoryPath, _dirPathPrefix_Full);
    NName::NormalizeDirPathPrefix(_dirPathPrefix_Full);
  }
}


STDMETHODIMP CArchiveExtractCallback::SetTotal(UInt64 size)
{
  COM_TRY_BEGIN
  _progressTotal = size;
  _progressTotal_Defined = true;
  if (!_multiArchives && _extractCallback2)
    return _extractCallback2->SetTotal(size);
  return S_OK;
  COM_TRY_END
}


static void NormalizeVals(UInt64 &v1, UInt64 &v2)
{
  const UInt64 kMax = (UInt64)1 << 31;
  while (v1 > kMax)
  {
    v1 >>= 1;
    v2 >>= 1;
  }
}


static UInt64 MyMultDiv64(UInt64 unpCur, UInt64 unpTotal, UInt64 packTotal)
{
  NormalizeVals(packTotal, unpTotal);
  NormalizeVals(unpCur, unpTotal);
  if (unpTotal == 0)
    unpTotal = 1;
  return unpCur * packTotal / unpTotal;
}


STDMETHODIMP CArchiveExtractCallback::SetCompleted(const UInt64 *completeValue)
{
  COM_TRY_BEGIN
  
  if (!_extractCallback2)
    return S_OK;

  UInt64 packCur;
  if (_multiArchives)
  {
    packCur = LocalProgressSpec->InSize;
    if (completeValue && _progressTotal_Defined)
      packCur += MyMultDiv64(*completeValue, _progressTotal, _packTotal);
    completeValue = &packCur;
  }
  return _extractCallback2->SetCompleted(completeValue);
 
  COM_TRY_END
}


STDMETHODIMP CArchiveExtractCallback::SetRatioInfo(const UInt64 *inSize, const UInt64 *outSize)
{
  COM_TRY_BEGIN
  return _localProgress->SetRatioInfo(inSize, outSize);
  COM_TRY_END
}


void CArchiveExtractCallback::CreateComplexDirectory(const UStringVector &dirPathParts, FString &fullPath)
{
  bool isAbsPath = false;
  
  if (!dirPathParts.IsEmpty())
  {
    const UString &s = dirPathParts[0];
    if (s.IsEmpty())
      isAbsPath = true;
    #if defined(_WIN32) && !defined(UNDER_CE)
    else
    {
      if (NName::IsDrivePath2(s))
        isAbsPath = true;
    }
    #endif
  }
  
  if (_pathMode == NExtract::NPathMode::kAbsPaths && isAbsPath)
    fullPath.Empty();
  else
    fullPath = _dirPathPrefix;

  FOR_VECTOR (i, dirPathParts)
  {
    if (i != 0)
      fullPath.Add_PathSepar();
    const UString &s = dirPathParts[i];
    fullPath += us2fs(s);
    #if defined(_WIN32) && !defined(UNDER_CE)
    if (_pathMode == NExtract::NPathMode::kAbsPaths)
      if (i == 0 && s.Len() == 2 && NName::IsDrivePath2(s))
        continue;
    #endif
    CreateDir(fullPath);
  }
}


HRESULT CArchiveExtractCallback::GetTime(UInt32 index, PROPID propID, FILETIME &filetime, bool &filetimeIsDefined)
{
  filetimeIsDefined = false;
  filetime.dwLowDateTime = 0;
  filetime.dwHighDateTime = 0;
  NCOM::CPropVariant prop;
  RINOK(_arc->Archive->GetProperty(index, propID, &prop));
  if (prop.vt == VT_FILETIME)
  {
    filetime = prop.filetime;
    filetimeIsDefined = (filetime.dwHighDateTime != 0 || filetime.dwLowDateTime != 0);
  }
  else if (prop.vt != VT_EMPTY)
    return E_FAIL;
  return S_OK;
}

HRESULT CArchiveExtractCallback::GetUnpackSize()
{
  return _arc->GetItemSize(_index, _curSize, _curSizeDefined);
}

static void AddPathToMessage(UString &s, const FString &path)
{
  s += " : ";
  s += fs2us(path);
}

HRESULT CArchiveExtractCallback::SendMessageError(const char *message, const FString &path)
{
  UString s (message);
  AddPathToMessage(s, path);
  return _extractCallback2->MessageError(s);
}

HRESULT CArchiveExtractCallback::SendMessageError_with_LastError(const char *message, const FString &path)
{
  DWORD errorCode = GetLastError();
  UString s (message);
  if (errorCode != 0)
  {
    s += " : ";
    s += NError::MyFormatMessage(errorCode);
  }
  AddPathToMessage(s, path);
  return _extractCallback2->MessageError(s);
}

HRESULT CArchiveExtractCallback::SendMessageError2(HRESULT errorCode, const char *message, const FString &path1, const FString &path2)
{
  UString s (message);
  if (errorCode != 0)
  {
    s += " : ";
    s += NError::MyFormatMessage(errorCode);
  }
  AddPathToMessage(s, path1);
  AddPathToMessage(s, path2);
  return _extractCallback2->MessageError(s);
}

#ifndef _SFX

STDMETHODIMP CGetProp::GetProp(PROPID propID, PROPVARIANT *value)
{
  /*
  if (propID == kpidName)
  {
    COM_TRY_BEGIN
    NCOM::CPropVariant prop = Name;
    prop.Detach(value);
    return S_OK;
    COM_TRY_END
  }
  */
  return Arc->Archive->GetProperty(IndexInArc, propID, value);
}

#endif // _SFX


#ifdef SUPPORT_LINKS

static UString GetDirPrefixOf(const UString &src)
{
  UString s (src);
  if (!s.IsEmpty())
  {
    if (IsPathSepar(s.Back()))
      s.DeleteBack();
    int pos = s.ReverseFind_PathSepar();
    s.DeleteFrom((unsigned)(pos + 1));
  }
  return s;
}

#endif // SUPPORT_LINKS

struct CLinkLevelsInfo
{
  bool IsAbsolute;
  int LowLevel;
  int FinalLevel;

  void Parse(const UString &path);
};

void CLinkLevelsInfo::Parse(const UString &path)
{
  IsAbsolute = NName::IsAbsolutePath(path);

  LowLevel = 0;
  FinalLevel = 0;

  UStringVector parts;
  SplitPathToParts(path, parts);
  int level = 0;
  
  FOR_VECTOR (i, parts)
  {
    const UString &s = parts[i];
    if (s.IsEmpty())
    {
      if (i == 0)
        IsAbsolute = true;
      continue;
    }
    if (s == L".")
      continue;
    if (s == L"..")
    {
      level--;
      if (LowLevel > level)
        LowLevel = level;
    }
    else
      level++;
  }
  
  FinalLevel = level;
}


bool IsSafePath(const UString &path);
bool IsSafePath(const UString &path)
{
  CLinkLevelsInfo levelsInfo;
  levelsInfo.Parse(path);
  return !levelsInfo.IsAbsolute
      && levelsInfo.LowLevel >= 0
      && levelsInfo.FinalLevel > 0;
}


bool CensorNode_CheckPath2(const NWildcard::CCensorNode &node, const CReadArcItem &item, bool &include);
bool CensorNode_CheckPath2(const NWildcard::CCensorNode &node, const CReadArcItem &item, bool &include)
{
  bool found = false;

  // CheckPathVect() doesn't check path to Parent nodes
  if (node.CheckPathVect(item.PathParts, !item.MainIsDir, include))
  {
    if (!include)
      return true;
    
    #ifdef SUPPORT_ALT_STREAMS
    if (!item.IsAltStream)
      return true;
    #endif
    
    found = true;
  }
  
  #ifdef SUPPORT_ALT_STREAMS

  if (!item.IsAltStream)
    return false;
  
  UStringVector pathParts2 = item.PathParts;
  if (pathParts2.IsEmpty())
    pathParts2.AddNew();
  UString &back = pathParts2.Back();
  back += ':';
  back += item.AltStreamName;
  bool include2;
  
  if (node.CheckPathVect(pathParts2,
      true, // isFile,
      include2))
  {
    include = include2;
    return true;
  }

  #endif // SUPPORT_ALT_STREAMS

  return found;
}


bool CensorNode_CheckPath(const NWildcard::CCensorNode &node, const CReadArcItem &item)
{
  bool include;
  if (CensorNode_CheckPath2(node, item, include))
    return include;
  return false;
}


static FString MakePath_from_2_Parts(const FString &prefix, const FString &path)
{
  FString s (prefix);
  #if defined(_WIN32) && !defined(UNDER_CE)
  if (!path.IsEmpty() && path[0] == ':' && !prefix.IsEmpty() && IsPathSepar(prefix.Back()))
  {
    if (!NName::IsDriveRootPath_SuperAllowed(prefix))
      s.DeleteBack();
  }
  #endif
  s += path;
  return s;
}



#ifdef SUPPORT_LINKS

/*
struct CTempMidBuffer
{
  void *Buf;

  CTempMidBuffer(size_t size): Buf(NULL) { Buf = ::MidAlloc(size); }
  ~CTempMidBuffer() { ::MidFree(Buf); }
};

HRESULT CArchiveExtractCallback::MyCopyFile(ISequentialOutStream *outStream)
{
  const size_t kBufSize = 1 << 16;
  CTempMidBuffer buf(kBufSize);
  if (!buf.Buf)
    return E_OUTOFMEMORY;
  
  NIO::CInFile inFile;
  NIO::COutFile outFile;
  
  if (!inFile.Open(_CopyFile_Path))
    return SendMessageError_with_LastError("Open error", _CopyFile_Path);
    
  for (;;)
  {
    UInt32 num;
    
    if (!inFile.Read(buf.Buf, kBufSize, num))
      return SendMessageError_with_LastError("Read error", _CopyFile_Path);
      
    if (num == 0)
      return S_OK;
      
      
    RINOK(WriteStream(outStream, buf.Buf, num));
  }
}
*/


HRESULT CArchiveExtractCallback::ReadLink()
{
  IInArchive *archive = _arc->Archive;
  const UInt32 index = _index;

  {
    NCOM::CPropVariant prop;
    RINOK(archive->GetProperty(index, kpidHardLink, &prop));
    if (prop.vt == VT_BSTR)
    {
      _link.isHardLink = true;
      // _link.isCopyLink = false;
      _link.isRelative = false; // RAR5, TAR: hard links are from root folder of archive
      _link.linkPath.SetFromBstr(prop.bstrVal);
    }
    else if (prop.vt != VT_EMPTY)
      return E_FAIL;
  }
  
  /*
  {
    NCOM::CPropVariant prop;
    RINOK(archive->GetProperty(index, kpidCopyLink, &prop));
    if (prop.vt == VT_BSTR)
    {
      _link.isHardLink = false;
      _link.isCopyLink = true;
      _link.isRelative = false; // RAR5: copy links are from root folder of archive
      _link.linkPath.SetFromBstr(prop.bstrVal);
    }
    else if (prop.vt != VT_EMPTY)
      return E_FAIL;
  }
  */

  {
    NCOM::CPropVariant prop;
    RINOK(archive->GetProperty(index, kpidSymLink, &prop));
    if (prop.vt == VT_BSTR)
    {
      _link.isHardLink = false;
      // _link.isCopyLink = false;
      _link.isRelative = true; // RAR5, TAR: symbolic links can be relative
      _link.linkPath.SetFromBstr(prop.bstrVal);
    }
    else if (prop.vt != VT_EMPTY)
      return E_FAIL;
  }

  NtReparse_Data = NULL;
  NtReparse_Size = 0;

  if (_link.linkPath.IsEmpty() && _arc->GetRawProps)
  {
    const void *data;
    UInt32 dataSize;
    UInt32 propType;
    
    _arc->GetRawProps->GetRawProp(_index, kpidNtReparse, &data, &dataSize, &propType);
    
    // if (dataSize == 1234567) // for debug: unpacking without reparse
    if (dataSize != 0)
    {
      if (propType != NPropDataType::kRaw)
        return E_FAIL;
  
      #ifdef _WIN32

      NtReparse_Data = data;
      NtReparse_Size = dataSize;

      CReparseAttr reparse;
      bool isOkReparse = reparse.Parse((const Byte *)data, dataSize);
      if (isOkReparse)
      {
        _link.isHardLink = false;
        // _link.isCopyLink = false;
        _link.linkPath = reparse.GetPath();
        _link.isJunction = reparse.IsMountPoint();

        if (reparse.IsSymLink_WSL())
        {
          _link.isWSL = true;
          _link.isRelative = reparse.IsRelative_WSL();
        }
        else
          _link.isRelative = reparse.IsRelative_Win();

        #ifndef _WIN32
        _link.linkPath.Replace(L'\\', WCHAR_PATH_SEPARATOR);
        #endif
      }
      #endif
    }
  }

  if (_link.linkPath.IsEmpty())
    return S_OK;

  {
    #ifdef _WIN32
    _link.linkPath.Replace(L'/', WCHAR_PATH_SEPARATOR);
    #endif

    // rar5 uses "\??\" prefix for absolute links
    if (_link.linkPath.IsPrefixedBy(WSTRING_PATH_SEPARATOR L"??" WSTRING_PATH_SEPARATOR))
    {
      _link.isRelative = false;
      _link.linkPath.DeleteFrontal(4);
    }
    
    for (;;)
    // while (NName::IsAbsolutePath(linkPath))
    {
      unsigned n = NName::GetRootPrefixSize(_link.linkPath);
      if (n == 0)
        break;
      _link.isRelative = false;
      _link.linkPath.DeleteFrontal(n);
    }
  }

  if (_link.linkPath.IsEmpty())
    return S_OK;

  if (!_link.isRelative && _removePathParts.Size() != 0)
  {
    UStringVector pathParts;
    SplitPathToParts(_link.linkPath, pathParts);
    bool badPrefix = false;
    FOR_VECTOR (i, _removePathParts)
    {
      if (CompareFileNames(_removePathParts[i], pathParts[i]) != 0)
      {
        badPrefix = true;
        break;
      }
    }
    if (!badPrefix)
      pathParts.DeleteFrontal(_removePathParts.Size());
    _link.linkPath = MakePathFromParts(pathParts);
  }

  /*
  if (!_link.linkPath.IsEmpty())
  {
    printf("\n_link %s to -> %s\n", GetOemString(_item.Path).Ptr(), GetOemString(_link.linkPath).Ptr());
  }
  */

  return S_OK;
}

#endif // SUPPORT_LINKS



HRESULT CArchiveExtractCallback::Read_fi_Props()
{
  IInArchive *archive = _arc->Archive;
  const UInt32 index = _index;

  _fi.AttribDefined = false;

  {
    NCOM::CPropVariant prop;
    RINOK(archive->GetProperty(index, kpidPosixAttrib, &prop));
    if (prop.vt == VT_UI4)
    {
      _fi.SetFromPosixAttrib(prop.ulVal);
    }
    else if (prop.vt != VT_EMPTY)
      return E_FAIL;
  }
  
  {
    NCOM::CPropVariant prop;
    RINOK(archive->GetProperty(index, kpidAttrib, &prop));
    if (prop.vt == VT_UI4)
    {
      _fi.Attrib = prop.ulVal;
      _fi.AttribDefined = true;
    }
    else if (prop.vt != VT_EMPTY)
      return E_FAIL;
  }

  RINOK(GetTime(index, kpidCTime, _fi.CTime, _fi.CTimeDefined));
  RINOK(GetTime(index, kpidATime, _fi.ATime, _fi.ATimeDefined));
  RINOK(GetTime(index, kpidMTime, _fi.MTime, _fi.MTimeDefined));
  return S_OK;
}



void CArchiveExtractCallback::CorrectPathParts()
{
  UStringVector &pathParts = _item.PathParts;

  #ifdef SUPPORT_ALT_STREAMS
  if (!_item.IsAltStream
      || !pathParts.IsEmpty()
      || !(_removePartsForAltStreams || _pathMode == NExtract::NPathMode::kNoPathsAlt))
  #endif
    Correct_FsPath(_pathMode == NExtract::NPathMode::kAbsPaths, _keepAndReplaceEmptyDirPrefixes, pathParts, _item.MainIsDir);
  
  #ifdef SUPPORT_ALT_STREAMS
    
  if (_item.IsAltStream)
  {
    UString s (_item.AltStreamName);
    Correct_AltStream_Name(s);
    bool needColon = true;
    
    if (pathParts.IsEmpty())
    {
      pathParts.AddNew();
      if (_removePartsForAltStreams || _pathMode == NExtract::NPathMode::kNoPathsAlt)
        needColon = false;
    }
    #ifdef _WIN32
    else if (_pathMode == NExtract::NPathMode::kAbsPaths &&
        NWildcard::GetNumPrefixParts_if_DrivePath(pathParts) == pathParts.Size())
      pathParts.AddNew();
    #endif
    
    UString &name = pathParts.Back();
    if (needColon)
      name += (char)(_ntOptions.ReplaceColonForAltStream ? '_' : ':');
    name += s;
  }
    
  #endif // SUPPORT_ALT_STREAMS
}



void CArchiveExtractCallback::CreateFolders()
{
  // 21.04 : we don't change original (_item.PathParts) here
  UStringVector pathParts = _item.PathParts;

  if (!_item.IsDir)
  {
    if (!pathParts.IsEmpty())
      pathParts.DeleteBack();
  }
    
  if (pathParts.IsEmpty())
    return;

  FString fullPathNew;
  CreateComplexDirectory(pathParts, fullPathNew);
        
  if (!_item.IsDir)
    return;

  CDirPathTime &pt = _extractedFolders.AddNew();
  
  pt.CTime = _fi.CTime;
  pt.CTimeDefined = (WriteCTime && _fi.CTimeDefined);
  
  pt.ATime = _fi.ATime;
  pt.ATimeDefined = (WriteATime && _fi.ATimeDefined);
  
  pt.MTimeDefined = false;
  
  if (WriteMTime)
  {
    if (_fi.MTimeDefined)
    {
      pt.MTime = _fi.MTime;
      pt.MTimeDefined = true;
    }
    else if (_arc->MTimeDefined)
    {
      pt.MTime = _arc->MTime;
      pt.MTimeDefined = true;
    }
  }
  
  pt.Path = fullPathNew;
  pt.SetDirTime();
}



/*
  CheckExistFile(fullProcessedPath)
    it can change: fullProcessedPath, _isRenamed, _overwriteMode
  (needExit = true) means that we must exit GetStream() even for S_OK result.
*/

HRESULT CArchiveExtractCallback::CheckExistFile(FString &fullProcessedPath, bool &needExit)
{
  needExit = true; // it was set already before

  NFind::CFileInfo fileInfo;

  if (fileInfo.Find(fullProcessedPath))
  {
    if (_overwriteMode == NExtract::NOverwriteMode::kSkip)
      return S_OK;
    
    if (_overwriteMode == NExtract::NOverwriteMode::kAsk)
    {
      int slashPos = fullProcessedPath.ReverseFind_PathSepar();
      FString realFullProcessedPath (fullProcessedPath.Left((unsigned)(slashPos + 1)) + fileInfo.Name);
  
      /* (fileInfo) can be symbolic link.
         we can show final file properties here. */

      Int32 overwriteResult;
      RINOK(_extractCallback2->AskOverwrite(
          fs2us(realFullProcessedPath), &fileInfo.MTime, &fileInfo.Size, _item.Path,
          _fi.MTimeDefined ? &_fi.MTime : NULL,
          _curSizeDefined ? &_curSize : NULL,
          &overwriteResult))
          
      switch (overwriteResult)
      {
        case NOverwriteAnswer::kCancel:
          return E_ABORT;
        case NOverwriteAnswer::kNo:
          return S_OK;
        case NOverwriteAnswer::kNoToAll:
          _overwriteMode = NExtract::NOverwriteMode::kSkip;
          return S_OK;
    
        case NOverwriteAnswer::kYes:
          break;
        case NOverwriteAnswer::kYesToAll:
          _overwriteMode = NExtract::NOverwriteMode::kOverwrite;
          break;
        case NOverwriteAnswer::kAutoRename:
          _overwriteMode = NExtract::NOverwriteMode::kRename;
          break;
        default:
          return E_FAIL;
      }
    } // NExtract::NOverwriteMode::kAsk

    if (_overwriteMode == NExtract::NOverwriteMode::kRename)
    {
      if (!AutoRenamePath(fullProcessedPath))
      {
        RINOK(SendMessageError(kCantAutoRename, fullProcessedPath));
        return E_FAIL;
      }
      _isRenamed = true;
    }
    else if (_overwriteMode == NExtract::NOverwriteMode::kRenameExisting)
    {
      FString existPath (fullProcessedPath);
      if (!AutoRenamePath(existPath))
      {
        RINOK(SendMessageError(kCantAutoRename, fullProcessedPath));
        return E_FAIL;
      }
      // MyMoveFile can rename folders. So it's OK to use it for folders too
      if (!MyMoveFile(fullProcessedPath, existPath))
      {
        HRESULT errorCode = GetLastError_noZero_HRESULT();
        RINOK(SendMessageError2(errorCode, kCantRenameFile, existPath, fullProcessedPath));
        return E_FAIL;
      }
    }
    else // not Rename*
    {
      if (fileInfo.IsDir())
      {
        // do we need to delete all files in folder?
        if (!RemoveDir(fullProcessedPath))
        {
          RINOK(SendMessageError_with_LastError(kCantDeleteOutputDir, fullProcessedPath));
          return S_OK;
        }
      }
      else // fileInfo is not Dir
      {
        if (NFind::DoesFileExist_Raw(fullProcessedPath))
          if (!DeleteFileAlways(fullProcessedPath))
            if (GetLastError() != ERROR_FILE_NOT_FOUND) // check it in linux
            {
              RINOK(SendMessageError_with_LastError(kCantDeleteOutputFile, fullProcessedPath));
              return S_OK;
              // return E_FAIL;
            }
      } // fileInfo is not Dir
    } // not Rename*
  }
  else // not Find(fullProcessedPath)
  {
    #if defined(_WIN32) && !defined(UNDER_CE)
    // we need to clear READ-ONLY of parent before creating alt stream
    int colonPos = NName::FindAltStreamColon(fullProcessedPath);
    if (colonPos >= 0 && fullProcessedPath[(unsigned)colonPos + 1] != 0)
    {
      FString parentFsPath (fullProcessedPath);
      parentFsPath.DeleteFrom((unsigned)colonPos);
      NFind::CFileInfo parentFi;
      if (parentFi.Find(parentFsPath))
      {
        if (parentFi.IsReadOnly())
          SetFileAttrib(parentFsPath, parentFi.Attrib & ~(DWORD)FILE_ATTRIBUTE_READONLY);
      }
    }
    #endif // defined(_WIN32) && !defined(UNDER_CE)
  }
  
  needExit = false;
  return S_OK;
}






HRESULT CArchiveExtractCallback::GetExtractStream(CMyComPtr<ISequentialOutStream> &outStreamLoc, bool &needExit)
{
  needExit = true;
    
  RINOK(Read_fi_Props());

  #ifdef SUPPORT_LINKS
  IInArchive *archive = _arc->Archive;
  #endif

  const UInt32 index = _index;

  bool isAnti = false;
  RINOK(_arc->IsItemAnti(index, isAnti));

  CorrectPathParts();
  UString processedPath (MakePathFromParts(_item.PathParts));
  
  if (!isAnti)
  {
    // 21.04: CreateFolders doesn't change (_item.PathParts)
    CreateFolders();
  }
  
  FString fullProcessedPath (us2fs(processedPath));
  if (_pathMode != NExtract::NPathMode::kAbsPaths
      || !NName::IsAbsolutePath(processedPath))
  {
    fullProcessedPath = MakePath_from_2_Parts(_dirPathPrefix, fullProcessedPath);
  }

  #ifdef SUPPORT_ALT_STREAMS
  if (_item.IsAltStream && _item.ParentIndex != (UInt32)(Int32)-1)
  {
    int renIndex = _renamedFiles.FindInSorted(CIndexToPathPair(_item.ParentIndex));
    if (renIndex >= 0)
    {
      const CIndexToPathPair &pair = _renamedFiles[(unsigned)renIndex];
      fullProcessedPath = pair.Path;
      fullProcessedPath += ':';
      UString s (_item.AltStreamName);
      Correct_AltStream_Name(s);
      fullProcessedPath += us2fs(s);
    }
  }
  #endif // SUPPORT_ALT_STREAMS

  if (_item.IsDir)
  {
    _diskFilePath = fullProcessedPath;
    if (isAnti)
      RemoveDir(_diskFilePath);
    #ifdef SUPPORT_LINKS
    if (_link.linkPath.IsEmpty())
    #endif
      return S_OK;
  }
  else if (!_isSplit)
  {
    RINOK(CheckExistFile(fullProcessedPath, needExit));
    if (needExit)
      return S_OK;
    needExit = true;
  }
  
  _diskFilePath = fullProcessedPath;
    

  if (isAnti)
  {
    needExit = false;
    return S_OK;
  }

  // not anti

  #ifdef SUPPORT_LINKS
  
  if (!_link.linkPath.IsEmpty())
  {
    #ifndef UNDER_CE
    {
      bool linkWasSet = false;
      RINOK(SetFromLinkPath(fullProcessedPath, _link, linkWasSet));
    }
    #endif // UNDER_CE

    // if (_CopyFile_Path.IsEmpty())
    {
      needExit = false;
      return S_OK;
    }
  }

  if (!_hardLinks.IDs.IsEmpty() && !_item.IsAltStream)
  {
    CHardLinkNode h;
    bool defined;
    RINOK(Archive_Get_HardLinkNode(archive, index, h, defined));
    if (defined)
    {
      int linkIndex = _hardLinks.IDs.FindInSorted2(h);
      if (linkIndex >= 0)
      {
        FString &hl = _hardLinks.Links[(unsigned)linkIndex];
        if (hl.IsEmpty())
          hl = fullProcessedPath;
        else
        {
          if (!MyCreateHardLink(fullProcessedPath, hl))
          {
            HRESULT errorCode = GetLastError_noZero_HRESULT();
            RINOK(SendMessageError2(errorCode, kCantCreateHardLink, fullProcessedPath, hl));
            return S_OK;
          }
          
          needExit = false;
          return S_OK;
        }
      }
    }
  }
  
  #endif // SUPPORT_LINKS


  // ---------- CREATE WRITE FILE -----

  _outFileStreamSpec = new COutFileStream;
  CMyComPtr<ISequentialOutStream> outFileStream_Loc(_outFileStreamSpec);
  
  if (!_outFileStreamSpec->Open(fullProcessedPath, _isSplit ? OPEN_ALWAYS: CREATE_ALWAYS))
  {
    // if (::GetLastError() != ERROR_FILE_EXISTS || !isSplit)
    {
      RINOK(SendMessageError_with_LastError(kCantOpenOutFile, fullProcessedPath));
      return S_OK;
    }
  }
  
  _fileWasExtracted = true;

  if (_curSizeDefined && _curSize > 0 && _curSize < (1 << 12))
  {
    if (_fi.IsLinuxSymLink())
    {
      _is_SymLink_in_Data = true;
      _is_SymLink_in_Data_Linux = true;
    }
    else if (_fi.IsReparse())
    {
      _is_SymLink_in_Data = true;
      _is_SymLink_in_Data_Linux = false;
    }
  }
  if (_is_SymLink_in_Data)
  {
    _outMemBuf.Alloc((size_t)_curSize);
    _bufPtrSeqOutStream_Spec = new CBufPtrSeqOutStream;
    _bufPtrSeqOutStream = _bufPtrSeqOutStream_Spec;
    _bufPtrSeqOutStream_Spec->Init(_outMemBuf, _outMemBuf.Size());
    outStreamLoc = _bufPtrSeqOutStream;
  }
  else // not reprase
  {
    if (_ntOptions.PreAllocateOutFile && !_isSplit && _curSizeDefined && _curSize > (1 << 12))
    {
      // UInt64 ticks = GetCpuTicks();
      _fileLength_that_WasSet = _curSize;
      bool res = _outFileStreamSpec->File.SetLength(_curSize);
      _fileLengthWasSet = res;
      
      // ticks = GetCpuTicks() - ticks;
      // printf("\nticks = %10d\n", (unsigned)ticks);
      if (!res)
      {
        RINOK(SendMessageError_with_LastError(kCantSetFileLen, fullProcessedPath));
      }
      
      /*
      _outFileStreamSpec->File.Close();
      ticks = GetCpuTicks() - ticks;
      printf("\nticks = %10d\n", (unsigned)ticks);
      return S_FALSE;
      */
      
      /*
      File.SetLength() on FAT (xp64): is fast, but then File.Close() can be slow,
      if we don't write any data.
      File.SetLength() for remote share file (exFAT) can be slow in some cases,
      and the Windows can return "network error" after 1 minute,
      while remote file still can grow.
      We need some way to detect such bad cases and disable PreAllocateOutFile mode.
      */
      
      res = _outFileStreamSpec->SeekToBegin_bool();
      if (!res)
      {
        RINOK(SendMessageError_with_LastError("Cannot seek to begin of file", fullProcessedPath));
      }
    } // PreAllocateOutFile
    
    #ifdef SUPPORT_ALT_STREAMS
    if (_isRenamed && !_item.IsAltStream)
    {
      CIndexToPathPair pair(index, fullProcessedPath);
      unsigned oldSize = _renamedFiles.Size();
      unsigned insertIndex = _renamedFiles.AddToUniqueSorted(pair);
      if (oldSize == _renamedFiles.Size())
        _renamedFiles[insertIndex].Path = fullProcessedPath;
    }
    #endif // SUPPORT_ALT_STREAMS
    
    if (_isSplit)
    {
      RINOK(_outFileStreamSpec->Seek((Int64)_position, STREAM_SEEK_SET, NULL));
    }
    outStreamLoc = outFileStream_Loc;
  } // if not reprase

  _outFileStream = outFileStream_Loc;
      
  needExit = false;
  return S_OK;
}



HRESULT CArchiveExtractCallback::GetItem(UInt32 index)
{
  #ifndef _SFX
  _item._use_baseParentFolder_mode = _use_baseParentFolder_mode;
  if (_use_baseParentFolder_mode)
  {
    _item._baseParentFolder = (int)_baseParentFolder;
    if (_pathMode == NExtract::NPathMode::kFullPaths ||
        _pathMode == NExtract::NPathMode::kAbsPaths)
      _item._baseParentFolder = -1;
  }
  #endif // _SFX

  #ifdef SUPPORT_ALT_STREAMS
  _item.WriteToAltStreamIfColon = _ntOptions.WriteToAltStreamIfColon;
  #endif

  return _arc->GetItem(index, _item);
}


STDMETHODIMP CArchiveExtractCallback::GetStream(UInt32 index, ISequentialOutStream **outStream, Int32 askExtractMode)
{
  COM_TRY_BEGIN

  *outStream = NULL;

  #ifndef _SFX
  if (_hashStream)
    _hashStreamSpec->ReleaseStream();
  _hashStreamWasUsed = false;
  #endif

  _outFileStream.Release();
  _bufPtrSeqOutStream.Release();

  _encrypted = false;
  _position = 0;
  _isSplit = false;
  
  _curSize = 0;
  _curSizeDefined = false;
  _fileLengthWasSet = false;
  _fileLength_that_WasSet = 0;
  _index = index;

  _diskFilePath.Empty();

  _isRenamed = false;
  // _fi.Clear();
  _is_SymLink_in_Data = false;
  _is_SymLink_in_Data_Linux = false;

  _fileWasExtracted = false;

  #ifdef SUPPORT_LINKS
  // _CopyFile_Path.Empty();
  _link.Clear();
  #endif

  IInArchive *archive = _arc->Archive;

  RINOK(GetItem(index));

  {
    NCOM::CPropVariant prop;
    RINOK(archive->GetProperty(index, kpidPosition, &prop));
    if (prop.vt != VT_EMPTY)
    {
      if (prop.vt != VT_UI8)
        return E_FAIL;
      _position = prop.uhVal.QuadPart;
      _isSplit = true;
    }
  }

  #ifdef SUPPORT_LINKS
  RINOK(ReadLink());
  #endif // SUPPORT_LINKS
  
  
  RINOK(Archive_GetItemBoolProp(archive, index, kpidEncrypted, _encrypted));

  RINOK(GetUnpackSize());

  #ifdef SUPPORT_ALT_STREAMS
  if (!_ntOptions.AltStreams.Val && _item.IsAltStream)
    return S_OK;
  #endif // SUPPORT_ALT_STREAMS

  // we can change (_item.PathParts) in this function
  UStringVector &pathParts = _item.PathParts;

  if (_wildcardCensor)
  {
    if (!CensorNode_CheckPath(*_wildcardCensor, _item))
      return S_OK;
  }

  #ifndef _SFX
  if (_use_baseParentFolder_mode)
  {
    if (!pathParts.IsEmpty())
    {
      unsigned numRemovePathParts = 0;
      
      #ifdef SUPPORT_ALT_STREAMS
      if (_pathMode == NExtract::NPathMode::kNoPathsAlt && _item.IsAltStream)
        numRemovePathParts = pathParts.Size();
      else
      #endif
      if (_pathMode == NExtract::NPathMode::kNoPaths ||
          _pathMode == NExtract::NPathMode::kNoPathsAlt)
        numRemovePathParts = pathParts.Size() - 1;
      pathParts.DeleteFrontal(numRemovePathParts);
    }
  }
  else
  #endif // _SFX
  {
    if (pathParts.IsEmpty())
    {
      if (_item.IsDir)
        return S_OK;
      /*
      #ifdef SUPPORT_ALT_STREAMS
      if (!_item.IsAltStream)
      #endif
        return E_FAIL;
      */
    }

    unsigned numRemovePathParts = 0;
    
    switch (_pathMode)
    {
      case NExtract::NPathMode::kFullPaths:
      case NExtract::NPathMode::kCurPaths:
      {
        if (_removePathParts.IsEmpty())
          break;
        bool badPrefix = false;
        
        if (pathParts.Size() < _removePathParts.Size())
          badPrefix = true;
        else
        {
          if (pathParts.Size() == _removePathParts.Size())
          {
            if (_removePartsForAltStreams)
            {
              #ifdef SUPPORT_ALT_STREAMS
              if (!_item.IsAltStream)
              #endif
                badPrefix = true;
            }
            else
            {
              if (!_item.MainIsDir)
                badPrefix = true;
            }
          }
          
          if (!badPrefix)
          FOR_VECTOR (i, _removePathParts)
          {
            if (CompareFileNames(_removePathParts[i], pathParts[i]) != 0)
            {
              badPrefix = true;
              break;
            }
          }
        }
        
        if (badPrefix)
        {
          if (askExtractMode == NArchive::NExtract::NAskMode::kExtract && !_testMode)
            return E_FAIL;
        }
        else
          numRemovePathParts = _removePathParts.Size();
        break;
      }
      
      case NExtract::NPathMode::kNoPaths:
      {
        if (!pathParts.IsEmpty())
          numRemovePathParts = pathParts.Size() - 1;
        break;
      }
      case NExtract::NPathMode::kNoPathsAlt:
      {
        #ifdef SUPPORT_ALT_STREAMS
        if (_item.IsAltStream)
          numRemovePathParts = pathParts.Size();
        else
        #endif
        if (!pathParts.IsEmpty())
          numRemovePathParts = pathParts.Size() - 1;
        break;
      }
      /*
      case NExtract::NPathMode::kFullPaths:
      case NExtract::NPathMode::kAbsPaths:
        break;
      */
      default:
        break;
    }
    
    pathParts.DeleteFrontal(numRemovePathParts);
  }

  
  #ifndef _SFX

  if (ExtractToStreamCallback)
  {
    if (!GetProp)
    {
      GetProp_Spec = new CGetProp;
      GetProp = GetProp_Spec;
    }
    GetProp_Spec->Arc = _arc;
    GetProp_Spec->IndexInArc = index;
    UString name (MakePathFromParts(pathParts));
    
    #ifdef SUPPORT_ALT_STREAMS
    if (_item.IsAltStream)
    {
      if (!pathParts.IsEmpty() || (!_removePartsForAltStreams && _pathMode != NExtract::NPathMode::kNoPathsAlt))
        name += ':';
      name += _item.AltStreamName;
    }
    #endif

    return ExtractToStreamCallback->GetStream7(name, BoolToInt(_item.IsDir), outStream, askExtractMode, GetProp);
  }

  #endif // _SFX


  CMyComPtr<ISequentialOutStream> outStreamLoc;

  if (askExtractMode == NArchive::NExtract::NAskMode::kExtract && !_testMode)
  {
    if (_stdOutMode)
      outStreamLoc = new CStdOutFileStream;
    else
    {
      bool needExit = true;
      RINOK(GetExtractStream(outStreamLoc, needExit));
      if (needExit)
        return S_OK;
    }
  }

  #ifndef _SFX
  if (_hashStream)
  {
    if (askExtractMode == NArchive::NExtract::NAskMode::kExtract ||
        askExtractMode == NArchive::NExtract::NAskMode::kTest)
    {
      _hashStreamSpec->SetStream(outStreamLoc);
      outStreamLoc = _hashStream;
      _hashStreamSpec->Init(true);
      _hashStreamWasUsed = true;
    }
  }
  #endif // _SFX

  if (outStreamLoc)
  {
    /*
    #ifdef SUPPORT_LINKS
    if (!_CopyFile_Path.IsEmpty())
    {
      RINOK(PrepareOperation(askExtractMode));
      RINOK(MyCopyFile(outStreamLoc));
      return SetOperationResult(NArchive::NExtract::NOperationResult::kOK);
    }
    if (_link.isCopyLink && _testMode)
      return S_OK;
    #endif
    */
    *outStream = outStreamLoc.Detach();
  }
  
  return S_OK;

  COM_TRY_END
}











STDMETHODIMP CArchiveExtractCallback::PrepareOperation(Int32 askExtractMode)
{
  COM_TRY_BEGIN

  #ifndef _SFX
  if (ExtractToStreamCallback)
    return ExtractToStreamCallback->PrepareOperation7(askExtractMode);
  #endif
  
  _extractMode = false;
  
  switch (askExtractMode)
  {
    case NArchive::NExtract::NAskMode::kExtract:
      if (_testMode)
        askExtractMode = NArchive::NExtract::NAskMode::kTest;
      else
        _extractMode = true;
      break;
  };
  
  return _extractCallback2->PrepareOperation(_item.Path, BoolToInt(_item.IsDir),
      askExtractMode, _isSplit ? &_position: 0);
  
  COM_TRY_END
}





HRESULT CArchiveExtractCallback::CloseFile()
{
  if (!_outFileStream)
    return S_OK;
  
  HRESULT hres = S_OK;
  
  const UInt64 processedSize = _outFileStreamSpec->ProcessedSize;
  if (_fileLengthWasSet && _fileLength_that_WasSet > processedSize)
  {
    bool res = _outFileStreamSpec->File.SetLength(processedSize);
    _fileLengthWasSet = res;
    if (!res)
    {
      HRESULT hres2 = SendMessageError_with_LastError(kCantSetFileLen, us2fs(_item.Path));
      if (hres == S_OK)
        hres = hres2;
    }
  }

  _curSize = processedSize;
  _curSizeDefined = true;

  // #ifdef _WIN32
  _outFileStreamSpec->SetTime(
      (WriteCTime && _fi.CTimeDefined) ? &_fi.CTime : NULL,
      (WriteATime && _fi.ATimeDefined) ? &_fi.ATime : NULL,
      (WriteMTime && _fi.MTimeDefined) ? &_fi.MTime : (_arc->MTimeDefined ? &_arc->MTime : NULL));
  // #endif

  RINOK(_outFileStreamSpec->Close());
  _outFileStream.Release();
  return hres;
}


#ifdef SUPPORT_LINKS


HRESULT CArchiveExtractCallback::SetFromLinkPath(
    const FString &fullProcessedPath,
    const CLinkInfo &linkInfo,
    bool &linkWasSet)
{
  linkWasSet = false;
  if (!_ntOptions.SymLinks.Val && !linkInfo.isHardLink)
    return S_OK;

  UString relatPath;

  /* if (linkInfo.isRelative)
       linkInfo.linkPath is final link path that must be stored to file link field
     else
       linkInfo.linkPath is path from root of archive. So we must add _dirPathPrefix_Full before linkPath.
  */
     
  if (linkInfo.isRelative)
    relatPath = GetDirPrefixOf(_item.Path);
  relatPath += linkInfo.linkPath;
  
  if (!IsSafePath(relatPath))
  {
    return SendMessageError2(
          0, // errorCode
          "Dangerous link path was ignored",
          us2fs(_item.Path),
          us2fs(linkInfo.linkPath)); // us2fs(relatPath)
  }

  FString existPath;
  if (linkInfo.isHardLink /* || linkInfo.IsCopyLink */ || !linkInfo.isRelative)
  {
    if (!NName::GetFullPath(_dirPathPrefix_Full, us2fs(relatPath), existPath))
    {
      RINOK(SendMessageError("Incorrect path", us2fs(relatPath)));
    }
  }
  else
  {
    existPath = us2fs(linkInfo.linkPath);
    // printf("\nlinkPath = : %s\n", GetOemString(linkInfo.linkPath).Ptr());
  }
    
  if (existPath.IsEmpty())
    return SendMessageError("Empty link", fullProcessedPath);

  if (linkInfo.isHardLink /* || linkInfo.IsCopyLink */)
  {
    // if (linkInfo.isHardLink)
    {
      if (!MyCreateHardLink(fullProcessedPath, existPath))
      {
        HRESULT errorCode = GetLastError_noZero_HRESULT();
        RINOK(SendMessageError2(errorCode, kCantCreateHardLink, fullProcessedPath, existPath));
      }
      linkWasSet = true;
      return S_OK;
    }
    /*
    // IsCopyLink
    {
      NFind::CFileInfo fi;
      if (!fi.Find(existPath))
      {
        RINOK(SendMessageError2("Cannot find the file for copying", existPath, fullProcessedPath));
      }
      else
      {
        if (_curSizeDefined && _curSize == fi.Size)
          _CopyFile_Path = existPath;
        else
        {
          RINOK(SendMessageError2("File size collision for file copying", existPath, fullProcessedPath));
        }
        // RINOK(MyCopyFile(existPath, fullProcessedPath));
      }
    }
    */
  }

  // is Symbolic link

  /*
  if (_item.IsDir && !isRelative)
  {
    // Windows before Vista doesn't support symbolic links.
    // we could convert such symbolic links to Junction Points
    // isJunction = true;
    // convertToAbs = true;
  }
  */

  if (!_ntOptions.SymLinks_AllowDangerous.Val)
  {
    #ifdef _WIN32
    if (_item.IsDir)
    #endif
    if (linkInfo.isRelative)
      {
        CLinkLevelsInfo levelsInfo;
        levelsInfo.Parse(linkInfo.linkPath);
        if (levelsInfo.FinalLevel < 1 || levelsInfo.IsAbsolute)
        {
          return SendMessageError2(
            0, // errorCode
            "Dangerous symbolic link path was ignored",
            us2fs(_item.Path),
            us2fs(linkInfo.linkPath));
        }
      }
  }

  
  #ifdef _WIN32
  
  CByteBuffer data;
  // printf("\nFillLinkData(): %s\n", GetOemString(existPath).Ptr());
  if (!FillLinkData(data, fs2us(existPath), !linkInfo.isJunction, linkInfo.isWSL))
    return SendMessageError("Cannot fill link data", us2fs(_item.Path));

  /*
  if (NtReparse_Size != data.Size() || memcmp(NtReparse_Data, data, data.Size()) != 0)
  {
    SendMessageError("reconstructed Reparse is different", fs2us(existPath));
  }
  */
  
  CReparseAttr attr;
  if (!attr.Parse(data, data.Size()))
  {
    RINOK(SendMessageError("Internal error for symbolic link file", us2fs(_item.Path)));
    return S_OK;
  }
  if (!NFile::NIO::SetReparseData(fullProcessedPath, _item.IsDir, data, (DWORD)data.Size()))
  {
    RINOK(SendMessageError_with_LastError(kCantCreateSymLink, fullProcessedPath));
    return S_OK;
  }
  linkWasSet = true;

  return S_OK;
  
  
  #else // ! _WIN32

  if (!NFile::NIO::SetSymLink(fullProcessedPath, existPath))
  {
    RINOK(SendMessageError_with_LastError(kCantCreateSymLink, fullProcessedPath));
    return S_OK;
  }
  linkWasSet = true;

  return S_OK;

  #endif // ! _WIN32
}


bool CLinkInfo::Parse(const Byte *data, size_t dataSize, bool isLinuxData)
{
  // this->isLinux = isLinuxData;
  
  if (isLinuxData)
  {
    isJunction = false;
    isHardLink = false;
    AString utf;
    if (dataSize >= (1 << 12))
      return false;
    utf.SetFrom_CalcLen((const char *)data, (unsigned)dataSize);
    UString u;
    if (!ConvertUTF8ToUnicode(utf, u))
      return false;
    linkPath = u;
    
    // in linux symbolic data: we expect that linux separator '/' is used
    // if windows link was created, then we also must use linux separator
    if (u.IsEmpty())
      return false;
    wchar_t c = u[0];
    isRelative = !IS_PATH_SEPAR(c);
    return true;
  }

  CReparseAttr reparse;
  if (!reparse.Parse(data, dataSize))
    return false;
  isHardLink = false;
  // isCopyLink = false;
  linkPath = reparse.GetPath();
  isJunction = reparse.IsMountPoint();
  
  if (reparse.IsSymLink_WSL())
  {
    isWSL = true;
    isRelative = reparse.IsRelative_WSL();
  }
  else
    isRelative = reparse.IsRelative_Win();
    
  // FIXME !!!
  #ifndef _WIN32
  linkPath.Replace(L'\\', WCHAR_PATH_SEPARATOR);
  #endif
  
  return true;
}
    
#endif // SUPPORT_LINKS


HRESULT CArchiveExtractCallback::CloseReparseAndFile()
{
  HRESULT res = S_OK;

  #ifdef SUPPORT_LINKS

  size_t reparseSize = 0;
  bool repraseMode = false;
  bool needSetReparse = false;
  CLinkInfo linkInfo;
  
  if (_bufPtrSeqOutStream)
  {
    repraseMode = true;
    reparseSize = _bufPtrSeqOutStream_Spec->GetPos();
    if (_curSizeDefined && reparseSize == _outMemBuf.Size())
    {
      /*
      CReparseAttr reparse;
      DWORD errorCode = 0;
      needSetReparse = reparse.Parse(_outMemBuf, reparseSize, errorCode);
      if (needSetReparse)
      {
        UString linkPath = reparse.GetPath();
        #ifndef _WIN32
        linkPath.Replace(L'\\', WCHAR_PATH_SEPARATOR);
        #endif
      }
      */
      needSetReparse = linkInfo.Parse(_outMemBuf, reparseSize, _is_SymLink_in_Data_Linux);
      if (!needSetReparse)
        res = SendMessageError_with_LastError("Incorrect reparse stream", us2fs(_item.Path));
    }
    else
    {
      res = SendMessageError_with_LastError("Unknown reparse stream", us2fs(_item.Path));
    }
    if (!needSetReparse && _outFileStream)
    {
      HRESULT res2 = WriteStream(_outFileStream, _outMemBuf, reparseSize);
      if (res == S_OK)
        res = res2;
    }
    _bufPtrSeqOutStream.Release();
  }

  #endif // SUPPORT_LINKS


  HRESULT res2 = CloseFile();

  if (res == S_OK)
    res = res2;

  RINOK(res);

  #ifdef SUPPORT_LINKS
  if (repraseMode)
  {
    _curSize = reparseSize;
    _curSizeDefined = true;
    
    #ifdef SUPPORT_LINKS
    if (needSetReparse)
    {
      // in Linux   : we must delete empty file before symbolic link creation
      // in Windows : we can create symbolic link even without file deleting
      if (!DeleteFileAlways(_diskFilePath))
      {
        RINOK(SendMessageError_with_LastError("can't delete file", _diskFilePath));
      }
      {
        bool linkWasSet = false;
        RINOK(SetFromLinkPath(_diskFilePath, linkInfo, linkWasSet));
        if (!linkWasSet)
          _fileWasExtracted = false;
      }
      /*
      if (!NFile::NIO::SetReparseData(_diskFilePath, _item.IsDir, ))
      {
        res = SendMessageError_with_LastError(kCantCreateSymLink, _diskFilePath);
      }
      */
    }
    #endif
  }
  #endif
  return res;
}



STDMETHODIMP CArchiveExtractCallback::SetOperationResult(Int32 opRes)
{
  COM_TRY_BEGIN

  #ifndef _SFX
  if (ExtractToStreamCallback)
  {
    GetUnpackSize();
    return ExtractToStreamCallback->SetOperationResult8(opRes, BoolToInt(_encrypted), _curSize);
  }
  #endif

  #ifndef _SFX

  if (_hashStreamWasUsed)
  {
    _hashStreamSpec->_hash->Final(_item.IsDir,
        #ifdef SUPPORT_ALT_STREAMS
          _item.IsAltStream
        #else
          false
        #endif
        , _item.Path);
    _curSize = _hashStreamSpec->GetSize();
    _curSizeDefined = true;
    _hashStreamSpec->ReleaseStream();
    _hashStreamWasUsed = false;
  }

  #endif // _SFX

  RINOK(CloseReparseAndFile());
  
  #ifdef _USE_SECURITY_CODE
  if (!_stdOutMode && _extractMode && _ntOptions.NtSecurity.Val && _arc->GetRawProps)
  {
    const void *data;
    UInt32 dataSize;
    UInt32 propType;
    _arc->GetRawProps->GetRawProp(_index, kpidNtSecure, &data, &dataSize, &propType);
    if (dataSize != 0)
    {
      if (propType != NPropDataType::kRaw)
        return E_FAIL;
      if (CheckNtSecure((const Byte *)data, dataSize))
      {
        SECURITY_INFORMATION securInfo = DACL_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION;
        if (_saclEnabled)
          securInfo |= SACL_SECURITY_INFORMATION;
        ::SetFileSecurityW(fs2us(_diskFilePath), securInfo, (PSECURITY_DESCRIPTOR)(void *)(const Byte *)(data));
      }
    }
  }
  #endif // _USE_SECURITY_CODE

  if (!_curSizeDefined)
    GetUnpackSize();
  
  if (_curSizeDefined)
  {
    #ifdef SUPPORT_ALT_STREAMS
    if (_item.IsAltStream)
      AltStreams_UnpackSize += _curSize;
    else
    #endif
      UnpackSize += _curSize;
  }
    
  if (_item.IsDir)
    NumFolders++;
  #ifdef SUPPORT_ALT_STREAMS
  else if (_item.IsAltStream)
    NumAltStreams++;
  #endif
  else
    NumFiles++;

  if (_fileWasExtracted)
  if (!_stdOutMode && _extractMode && _fi.AttribDefined)
  {
    bool res = SetFileAttrib_PosixHighDetect(_diskFilePath, _fi.Attrib);
    if (!res)
    {
      // do we need error message here in Windows and in posix?
      SendMessageError_with_LastError("Cannot set file attribute", _diskFilePath);
    }
  }
  
  RINOK(_extractCallback2->SetOperationResult(opRes, BoolToInt(_encrypted)));
  
  return S_OK;
  
  COM_TRY_END
}



STDMETHODIMP CArchiveExtractCallback::ReportExtractResult(UInt32 indexType, UInt32 index, Int32 opRes)
{
  if (_folderArchiveExtractCallback2)
  {
    bool isEncrypted = false;
    UString s;
    
    if (indexType == NArchive::NEventIndexType::kInArcIndex && index != (UInt32)(Int32)-1)
    {
      CReadArcItem item;
      RINOK(_arc->GetItem(index, item));
      s = item.Path;
      RINOK(Archive_GetItemBoolProp(_arc->Archive, index, kpidEncrypted, isEncrypted));
    }
    else
    {
      s = '#';
      s.Add_UInt32(index);
      // if (indexType == NArchive::NEventIndexType::kBlockIndex) {}
    }
    
    return _folderArchiveExtractCallback2->ReportExtractResult(opRes, isEncrypted, s);
  }

  return S_OK;
}


STDMETHODIMP CArchiveExtractCallback::CryptoGetTextPassword(BSTR *password)
{
  COM_TRY_BEGIN
  if (!_cryptoGetTextPassword)
  {
    RINOK(_extractCallback2.QueryInterface(IID_ICryptoGetTextPassword,
        &_cryptoGetTextPassword));
  }
  return _cryptoGetTextPassword->CryptoGetTextPassword(password);
  COM_TRY_END
}


// ---------- HASH functions ----------

FString CArchiveExtractCallback::Hash_GetFullFilePath()
{
  // this function changes _item.PathParts.
  CorrectPathParts();
  const UStringVector &pathParts = _item.PathParts;
  const UString processedPath (MakePathFromParts(pathParts));
  FString fullProcessedPath (us2fs(processedPath));
  if (_pathMode != NExtract::NPathMode::kAbsPaths
      || !NName::IsAbsolutePath(processedPath))
  {
    fullProcessedPath = MakePath_from_2_Parts(
        DirPathPrefix_for_HashFiles,
        // _dirPathPrefix,
        fullProcessedPath);
  }
  return fullProcessedPath;
}


STDMETHODIMP CArchiveExtractCallback::GetDiskProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  if (propID == kpidSize)
  {
    RINOK(GetItem(index));
    const FString fullProcessedPath = Hash_GetFullFilePath();
    NFile::NFind::CFileInfo fi;
    if (fi.Find_FollowLink(fullProcessedPath))
      if (!fi.IsDir())
        prop = (UInt64)fi.Size;
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


STDMETHODIMP CArchiveExtractCallback::GetStream2(UInt32 index, ISequentialInStream **inStream, UInt32 mode)
{
  COM_TRY_BEGIN
  *inStream = NULL;
  // if (index != _index) return E_FAIL;
  if (mode != NUpdateNotifyOp::kHashRead)
    return E_FAIL;

  RINOK(GetItem(index));
  const FString fullProcessedPath = Hash_GetFullFilePath();

  CInFileStream *inStreamSpec = new CInFileStream;
  CMyComPtr<ISequentialInStream> inStreamRef = inStreamSpec;
  inStreamSpec->File.PreserveATime = _ntOptions.PreserveATime;
  if (!inStreamSpec->OpenShared(fullProcessedPath, _ntOptions.OpenShareForWrite))
  {
    RINOK(SendMessageError_with_LastError(kCantOpenInFile, fullProcessedPath));
    return S_OK;
  }
  *inStream = inStreamRef.Detach();
  return S_OK;
  COM_TRY_END
}


STDMETHODIMP CArchiveExtractCallback::ReportOperation(
    UInt32 /* indexType */, UInt32 /* index */, UInt32 /* op */)
{
  // COM_TRY_BEGIN
  return S_OK;
  // COM_TRY_END
}


// ------------ After Extracting functions ------------

void CDirPathSortPair::SetNumSlashes(const FChar *s)
{
  for (unsigned numSlashes = 0;;)
  {
    FChar c = *s++;
    if (c == 0)
    {
      Len = numSlashes;
      return;
    }
    if (IS_PATH_SEPAR(c))
      numSlashes++;
  }
}


bool CDirPathTime::SetDirTime() const
{
  return NDir::SetDirTime(Path,
      CTimeDefined ? &CTime : NULL,
      ATimeDefined ? &ATime : NULL,
      MTimeDefined ? &MTime : NULL);
}


HRESULT CArchiveExtractCallback::SetDirsTimes()
{
  if (!_arc)
    return S_OK;

  CRecordVector<CDirPathSortPair> pairs;
  pairs.ClearAndSetSize(_extractedFolders.Size());
  unsigned i;
  
  for (i = 0; i < _extractedFolders.Size(); i++)
  {
    CDirPathSortPair &pair = pairs[i];
    pair.Index = i;
    pair.SetNumSlashes(_extractedFolders[i].Path);
  }
  
  pairs.Sort2();
  
  HRESULT res = S_OK;

  for (i = 0; i < pairs.Size(); i++)
  {
    const CDirPathTime &dpt = _extractedFolders[pairs[i].Index];
    if (!dpt.SetDirTime())
    {
      // result = E_FAIL;
      // do we need error message here in Windows and in posix?
      // SendMessageError_with_LastError("Cannot set directory time", dpt.Path);
    }
  }

  /*
  #ifndef _WIN32
  for (i = 0; i < _delayedSymLinks.Size(); i++)
  {
    const CDelayedSymLink &link = _delayedSymLinks[i];
    if (!link.Create())
    {
      if (res == S_OK)
        res = GetLastError_noZero_HRESULT();
      // res = E_FAIL;
      // do we need error message here in Windows and in posix?
      SendMessageError_with_LastError("Cannot create Symbolic Link", link._source);
    }
  }
  #endif // _WIN32
  */

  ClearExtractedDirsInfo();
  return res;
}


HRESULT CArchiveExtractCallback::CloseArc()
{
  HRESULT res = CloseReparseAndFile();
  HRESULT res2 = SetDirsTimes();
  if (res == S_OK)
    res = res2;
  _arc = NULL;
  return res;
}

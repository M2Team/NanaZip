// LoadCodecs.cpp

/*
EXTERNAL_CODECS
---------------
  CCodecs::Load() tries to detect the directory with plugins.
  It stops the checking, if it can find any of the following items:
    - 7z.dll file
    - "Formats" subdir
    - "Codecs"  subdir
  The order of check:
    1) directory of client executable
    2) WIN32: directory for REGISTRY item [HKEY_*\Software\7-Zip\Path**]
       The order for HKEY_* : Path** :
         - HKEY_CURRENT_USER  : PathXX
         - HKEY_LOCAL_MACHINE : PathXX
         - HKEY_CURRENT_USER  : Path
         - HKEY_LOCAL_MACHINE : Path
       PathXX is Path32 in 32-bit code
       PathXX is Path64 in 64-bit code


EXPORT_CODECS
-------------
  if (EXTERNAL_CODECS) is defined, then the code exports internal
  codecs of client from CCodecs object to external plugins.
  7-Zip doesn't use that feature. 7-Zip uses the scheme:
    - client application without internal plugins.
    - 7z.dll module contains all (or almost all) plugins.
      7z.dll can use codecs from another plugins, if required.
*/


#include "StdAfx.h"

#include "../../../../C/7zVersion.h"

#include "../../../Common/MyCom.h"
#include "../../../Common/StringToInt.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileIO.h"
#include "../../../Windows/PropVariant.h"

#include "LoadCodecs.h"

using namespace NWindows;

#ifdef NEW_FOLDER_INTERFACE
#include "../../../Common/StringToInt.h"
#endif

#include "../../ICoder.h"
#include "../../Common/RegisterArc.h"

#ifdef EXTERNAL_CODECS

// #define EXPORT_CODECS

#endif

#ifdef NEW_FOLDER_INTERFACE
extern HINSTANCE g_hInstance;
#include "../../../Windows/ResourceString.h"
static const UINT kIconTypesResId = 100;
#endif

#ifdef EXTERNAL_CODECS

#include "../../../Windows/FileFind.h"
#include "../../../Windows/DLL.h"

#ifdef _WIN32
#include "../../../Windows/FileName.h"
#include "../../../Windows/Registry.h"
#endif

using namespace NFile;


#define kCodecsFolderName FTEXT("Codecs")
#define kFormatsFolderName FTEXT("Formats")


static CFSTR const kMainDll =
  #ifdef _WIN32
    FTEXT("7z.dll");
  #else
    FTEXT("7z.so");
  #endif


#ifdef _WIN32

static LPCTSTR const kRegistryPath = TEXT("Software") TEXT(STRING_PATH_SEPARATOR) TEXT("7-zip");
static LPCWSTR const kProgramPathValue = L"Path";
static LPCWSTR const kProgramPath2Value = L"Path"
  #ifdef _WIN64
  L"64";
  #else
  L"32";
  #endif

static bool ReadPathFromRegistry(HKEY baseKey, LPCWSTR value, FString &path)
{
  NRegistry::CKey key;
  if (key.Open(baseKey, kRegistryPath, KEY_READ) == ERROR_SUCCESS)
  {
    UString pathU;
    if (key.QueryValue(value, pathU) == ERROR_SUCCESS)
    {
      path = us2fs(pathU);
      NName::NormalizeDirPathPrefix(path);
      return NFind::DoesFileExist_Raw(path + kMainDll);
    }
  }
  return false;
}

#endif // _WIN32

#endif // EXTERNAL_CODECS


static const unsigned kNumArcsMax = 64;
static unsigned g_NumArcs = 0;
static const CArcInfo *g_Arcs[kNumArcsMax];

void RegisterArc(const CArcInfo *arcInfo) throw()
{
  if (g_NumArcs < kNumArcsMax)
  {
    g_Arcs[g_NumArcs] = arcInfo;
    g_NumArcs++;
  }
}

static void SplitString(const UString &srcString, UStringVector &destStrings)
{
  destStrings.Clear();
  UString s;
  unsigned len = srcString.Len();
  if (len == 0)
    return;
  for (unsigned i = 0; i < len; i++)
  {
    wchar_t c = srcString[i];
    if (c == L' ')
    {
      if (!s.IsEmpty())
      {
        destStrings.Add(s);
        s.Empty();
      }
    }
    else
      s += c;
  }
  if (!s.IsEmpty())
    destStrings.Add(s);
}

int CArcInfoEx::FindExtension(const UString &ext) const
{
  FOR_VECTOR (i, Exts)
    if (ext.IsEqualTo_NoCase(Exts[i].Ext))
      return (int)i;
  return -1;
}

void CArcInfoEx::AddExts(const UString &ext, const UString &addExt)
{
  UStringVector exts, addExts;
  SplitString(ext, exts);
  SplitString(addExt, addExts);
  FOR_VECTOR (i, exts)
  {
    CArcExtInfo extInfo;
    extInfo.Ext = exts[i];
    if (i < addExts.Size())
    {
      extInfo.AddExt = addExts[i];
      if (extInfo.AddExt == L"*")
        extInfo.AddExt.Empty();
    }
    Exts.Add(extInfo);
  }
}

#ifndef _SFX

static bool ParseSignatures(const Byte *data, unsigned size, CObjectVector<CByteBuffer> &signatures)
{
  signatures.Clear();
  while (size > 0)
  {
    unsigned len = *data++;
    size--;
    if (len > size)
      return false;
    signatures.AddNew().CopyFrom(data, len);
    data += len;
    size -= len;
  }
  return true;
}

#endif // _SFX

// #include <stdio.h>

#ifdef EXTERNAL_CODECS

static FString GetBaseFolderPrefixFromRegistry()
{
  FString moduleFolderPrefix = NDLL::GetModuleDirPrefix();
  
  #ifdef _WIN32
  if (   !NFind::DoesFileOrDirExist(moduleFolderPrefix + kMainDll)
      && !NFind::DoesFileOrDirExist(moduleFolderPrefix + kCodecsFolderName)
      && !NFind::DoesFileOrDirExist(moduleFolderPrefix + kFormatsFolderName))
  {
    FString path;
    if (ReadPathFromRegistry(HKEY_CURRENT_USER,  kProgramPath2Value, path)) return path;
    if (ReadPathFromRegistry(HKEY_LOCAL_MACHINE, kProgramPath2Value, path)) return path;
    if (ReadPathFromRegistry(HKEY_CURRENT_USER,  kProgramPathValue,  path)) return path;
    if (ReadPathFromRegistry(HKEY_LOCAL_MACHINE, kProgramPathValue,  path)) return path;
  }
  #endif
  
  // printf("\nmoduleFolderPrefix = %s\n", (const char *)GetAnsiString(moduleFolderPrefix));
  return moduleFolderPrefix;
}


static HRESULT GetCoderClass(Func_GetMethodProperty getMethodProperty, UInt32 index,
    PROPID propId, CLSID &clsId, bool &isAssigned)
{
  NCOM::CPropVariant prop;
  isAssigned = false;
  RINOK(getMethodProperty(index, propId, &prop));
  if (prop.vt == VT_BSTR)
  {
    if (::SysStringByteLen(prop.bstrVal) != sizeof(GUID))
      return E_FAIL;
    isAssigned = true;
    clsId = *(const GUID *)(const void *)prop.bstrVal;
  }
  else if (prop.vt != VT_EMPTY)
    return E_FAIL;
  return S_OK;
}

#define MY_GET_FUNC(dest, type, func)  *(void **)(&dest) = (func);
// #define MY_GET_FUNC(dest, type, func)  dest = (type)(func);

HRESULT CCodecs::LoadCodecs()
{
  CCodecLib &lib = Libs.Back();

  MY_GET_FUNC (lib.CreateDecoder,     Func_CreateDecoder,     lib.Lib.GetProc("CreateDecoder"));
  MY_GET_FUNC (lib.CreateEncoder,     Func_CreateEncoder,     lib.Lib.GetProc("CreateEncoder"));
  MY_GET_FUNC (lib.GetMethodProperty, Func_GetMethodProperty, lib.Lib.GetProc("GetMethodProperty"));

  if (lib.GetMethodProperty)
  {
    UInt32 numMethods = 1;
    Func_GetNumberOfMethods getNumberOfMethods;
    MY_GET_FUNC (getNumberOfMethods, Func_GetNumberOfMethods, lib.Lib.GetProc("GetNumberOfMethods"));
    if (getNumberOfMethods)
    {
      RINOK(getNumberOfMethods(&numMethods));
    }
    for (UInt32 i = 0; i < numMethods; i++)
    {
      CDllCodecInfo info;
      info.LibIndex = Libs.Size() - 1;
      info.CodecIndex = i;
      RINOK(GetCoderClass(lib.GetMethodProperty, i, NMethodPropID::kEncoder, info.Encoder, info.EncoderIsAssigned));
      RINOK(GetCoderClass(lib.GetMethodProperty, i, NMethodPropID::kDecoder, info.Decoder, info.DecoderIsAssigned));
      Codecs.Add(info);
    }
  }

  Func_GetHashers getHashers;
  MY_GET_FUNC (getHashers, Func_GetHashers, lib.Lib.GetProc("GetHashers"));
  if (getHashers)
  {
    RINOK(getHashers(&lib.ComHashers));
    if (lib.ComHashers)
    {
      UInt32 numMethods = lib.ComHashers->GetNumHashers();
      for (UInt32 i = 0; i < numMethods; i++)
      {
        CDllHasherInfo info;
        info.LibIndex = Libs.Size() - 1;
        info.HasherIndex = i;
        Hashers.Add(info);
      }
    }
  }
  
  return S_OK;
}

static HRESULT GetProp(
    Func_GetHandlerProperty getProp,
    Func_GetHandlerProperty2 getProp2,
    UInt32 index, PROPID propID, NCOM::CPropVariant &prop)
{
  if (getProp2)
    return getProp2(index, propID, &prop);;
  return getProp(propID, &prop);
}

static HRESULT GetProp_Bool(
    Func_GetHandlerProperty getProp,
    Func_GetHandlerProperty2 getProp2,
    UInt32 index, PROPID propID, bool &res)
{
  res = false;
  NCOM::CPropVariant prop;
  RINOK(GetProp(getProp, getProp2, index, propID, prop));
  if (prop.vt == VT_BOOL)
    res = VARIANT_BOOLToBool(prop.boolVal);
  else if (prop.vt != VT_EMPTY)
    return E_FAIL;
  return S_OK;
}

static HRESULT GetProp_UInt32(
    Func_GetHandlerProperty getProp,
    Func_GetHandlerProperty2 getProp2,
    UInt32 index, PROPID propID, UInt32 &res, bool &defined)
{
  res = 0;
  defined = false;
  NCOM::CPropVariant prop;
  RINOK(GetProp(getProp, getProp2, index, propID, prop));
  if (prop.vt == VT_UI4)
  {
    res = prop.ulVal;
    defined = true;
  }
  else if (prop.vt != VT_EMPTY)
    return E_FAIL;
  return S_OK;
}

static HRESULT GetProp_String(
    Func_GetHandlerProperty getProp,
    Func_GetHandlerProperty2 getProp2,
    UInt32 index, PROPID propID, UString &res)
{
  res.Empty();
  NCOM::CPropVariant prop;
  RINOK(GetProp(getProp, getProp2, index, propID, prop));
  if (prop.vt == VT_BSTR)
    res.SetFromBstr(prop.bstrVal);
  else if (prop.vt != VT_EMPTY)
    return E_FAIL;
  return S_OK;
}

static HRESULT GetProp_RawData(
    Func_GetHandlerProperty getProp,
    Func_GetHandlerProperty2 getProp2,
    UInt32 index, PROPID propID, CByteBuffer &bb)
{
  bb.Free();
  NCOM::CPropVariant prop;
  RINOK(GetProp(getProp, getProp2, index, propID, prop));
  if (prop.vt == VT_BSTR)
  {
    UINT len = ::SysStringByteLen(prop.bstrVal);
    bb.CopyFrom((const Byte *)prop.bstrVal, len);
  }
  else if (prop.vt != VT_EMPTY)
    return E_FAIL;
  return S_OK;
}

static const UInt32 kArcFlagsPars[] =
{
  NArchive::NHandlerPropID::kKeepName, NArcInfoFlags::kKeepName,
  NArchive::NHandlerPropID::kAltStreams, NArcInfoFlags::kAltStreams,
  NArchive::NHandlerPropID::kNtSecure, NArcInfoFlags::kNtSecure
};

HRESULT CCodecs::LoadFormats()
{
  const NDLL::CLibrary &lib = Libs.Back().Lib;
  
  Func_GetHandlerProperty getProp = NULL;
  Func_GetHandlerProperty2 getProp2;
  MY_GET_FUNC (getProp2, Func_GetHandlerProperty2, lib.GetProc("GetHandlerProperty2"));
  Func_GetIsArc getIsArc;
  MY_GET_FUNC (getIsArc, Func_GetIsArc, lib.GetProc("GetIsArc"));
  
  UInt32 numFormats = 1;

  if (getProp2)
  {
    Func_GetNumberOfFormats getNumberOfFormats;
    MY_GET_FUNC (getNumberOfFormats, Func_GetNumberOfFormats, lib.GetProc("GetNumberOfFormats"));
    if (getNumberOfFormats)
    {
      RINOK(getNumberOfFormats(&numFormats));
    }
  }
  else
  {
    MY_GET_FUNC (getProp, Func_GetHandlerProperty, lib.GetProc("GetHandlerProperty"));
    if (!getProp)
      return S_OK;
  }
  
  for (UInt32 i = 0; i < numFormats; i++)
  {
    CArcInfoEx item;
    item.LibIndex = (int)(Libs.Size() - 1);
    item.FormatIndex = i;

    RINOK(GetProp_String(getProp, getProp2, i, NArchive::NHandlerPropID::kName, item.Name));

    {
      NCOM::CPropVariant prop;
      if (GetProp(getProp, getProp2, i, NArchive::NHandlerPropID::kClassID, prop) != S_OK)
        continue;
      if (prop.vt != VT_BSTR)
        continue;
      if (::SysStringByteLen(prop.bstrVal) != sizeof(GUID))
        return E_FAIL;
      item.ClassID = *(const GUID *)(const void *)prop.bstrVal;
      prop.Clear();
    }

    UString ext, addExt;
    RINOK(GetProp_String(getProp, getProp2, i, NArchive::NHandlerPropID::kExtension, ext));
    RINOK(GetProp_String(getProp, getProp2, i, NArchive::NHandlerPropID::kAddExtension, addExt));
    item.AddExts(ext, addExt);

    GetProp_Bool(getProp, getProp2, i, NArchive::NHandlerPropID::kUpdate, item.UpdateEnabled);
    bool flags_Defined = false;
    RINOK(GetProp_UInt32(getProp, getProp2, i, NArchive::NHandlerPropID::kFlags, item.Flags, flags_Defined));
    item.NewInterface = flags_Defined;
    if (!flags_Defined) // && item.UpdateEnabled
    {
      // support for DLL version before 9.31:
      for (unsigned j = 0; j < ARRAY_SIZE(kArcFlagsPars); j += 2)
      {
        bool val = false;
        GetProp_Bool(getProp, getProp2, i, kArcFlagsPars[j], val);
        if (val)
          item.Flags |= kArcFlagsPars[j + 1];
      }
    }
    
    CByteBuffer sig;
    RINOK(GetProp_RawData(getProp, getProp2, i, NArchive::NHandlerPropID::kSignature, sig));
    if (sig.Size() != 0)
      item.Signatures.Add(sig);
    else
    {
      RINOK(GetProp_RawData(getProp, getProp2, i, NArchive::NHandlerPropID::kMultiSignature, sig));
      ParseSignatures(sig, (unsigned)sig.Size(), item.Signatures);
    }

    bool signatureOffset_Defined;
    RINOK(GetProp_UInt32(getProp, getProp2, i, NArchive::NHandlerPropID::kSignatureOffset, item.SignatureOffset, signatureOffset_Defined));
    
    // bool version_Defined;
    // RINOK(GetProp_UInt32(getProp, getProp2, i, NArchive::NHandlerPropID::kVersion, item.Version, version_Defined));

    if (getIsArc)
      getIsArc(i, &item.IsArcFunc);

    Formats.Add(item);
  }
  return S_OK;
}

#ifdef _7ZIP_LARGE_PAGES
extern "C"
{
  extern SIZE_T g_LargePageSize;
}
#endif


void CCodecs::AddLastError(const FString &path)
{
  HRESULT res = GetLastError_noZero_HRESULT();
  CCodecError &error = Errors.AddNew();
  error.Path = path;
  error.ErrorCode = res;
}

HRESULT CCodecs::LoadDll(const FString &dllPath, bool needCheckDll, bool *loadedOK)
{
  if (loadedOK)
    *loadedOK = false;

  // needCheckDll = 1;

  #ifdef _WIN32
  if (needCheckDll)
  {
    NDLL::CLibrary lib;
    if (!lib.LoadEx(dllPath, LOAD_LIBRARY_AS_DATAFILE))
    {
      /* if is not win32
      //  %1 is not a valid Win32 application.
      //  #define ERROR_BAD_EXE_FORMAT             193L
      */
      // return GetLastError_noZero_HRESULT();
      DWORD lastError = GetLastError();
      if (lastError != ERROR_BAD_EXE_FORMAT)
      {
        CCodecError &error = Errors.AddNew();
        error.Path = dllPath;
        error.Message = "cannot load file as datafile library";
        error.ErrorCode = HRESULT_FROM_WIN32(lastError);
      }
      return S_OK;
    }
  }
  #else
  UNUSED_VAR(needCheckDll)
  #endif
  
  Libs.AddNew();
  CCodecLib &lib = Libs.Back();
  lib.Path = dllPath;
  bool used = false;
  // HRESULT res = S_OK;
  
  if (lib.Lib.Load(dllPath))
  {
    if (loadedOK)
      *loadedOK = true;
    #ifdef NEW_FOLDER_INTERFACE
    lib.LoadIcons();
    #endif

    /*
    {
      Func_LibStartup _LibStartup;
      MY_GET_FUNC (_LibStartup, Func_LibStartup, lib.Lib.GetProc("LibStartup"));
      if (_LibStartup)
      {
        HRESULT res = _LibStartup();
        if (res != 0)
        {
          CCodecError &error = Errors.AddNew();
          error.Path = dllPath;
          error.ErrorCode = res;
        }
      }
    }
    */

    #ifdef _7ZIP_LARGE_PAGES
    if (g_LargePageSize != 0)
    {
      Func_SetLargePageMode setLargePageMode;
      MY_GET_FUNC (setLargePageMode, Func_SetLargePageMode, lib.Lib.GetProc("SetLargePageMode"));
      if (setLargePageMode)
        setLargePageMode();
    }
    #endif

    if (CaseSensitiveChange)
    {
      Func_SetCaseSensitive setCaseSensitive;
      MY_GET_FUNC (setCaseSensitive, Func_SetCaseSensitive, lib.Lib.GetProc("SetCaseSensitive"));
      if (setCaseSensitive)
        setCaseSensitive(CaseSensitive ? 1 : 0);
    }

    MY_GET_FUNC (lib.CreateObject, Func_CreateObject, lib.Lib.GetProc("CreateObject"));
    {
      unsigned startSize = Codecs.Size() + Hashers.Size();
      HRESULT res = LoadCodecs();
      if (startSize != Codecs.Size() + Hashers.Size())
        used = true;
      if (res == S_OK && lib.CreateObject)
      {
        startSize = Formats.Size();
        res = LoadFormats();
        if (startSize != Formats.Size())
          used = true;
      }
      if (res != S_OK)
      {
        CCodecError &error = Errors.AddNew();
        error.Path = dllPath;
        error.ErrorCode = res;
      }
    }
    // plugins can use non-7-zip dlls, so we silently ignore non7zip DLLs
    /*
    if (!used)
    {
      CCodecError &error = Errors.AddNew();
      error.Path = dllPath;
      error.Message = "no 7-Zip code";
    }
    */
  }
  else
  {
    AddLastError(dllPath);
  }
  
  if (!used)
    Libs.DeleteBack();

  return S_OK;
}

HRESULT CCodecs::LoadDllsFromFolder(const FString &folderPath)
{
  if (!NFile::NFind::DoesDirExist_FollowLink(folderPath))
  // if (!NFile::NFind::DoesDirExist(folderPath))
  {
    // AddLastError(folderPath);
    return S_OK;
  }

  FString folderPrefix = folderPath;
  folderPrefix.Add_PathSepar();

  NFile::NFind::CEnumerator enumerator;
  enumerator.SetDirPrefix(folderPrefix);
  NFile::NFind::CDirEntry fi;
  for (;;)
  {
    bool found;
    if (!enumerator.Next(fi, found))
    {
      // it can be wrong Symbolic link to folder here
      AddLastError(folderPath);
      break;
      // return GetLastError_noZero_HRESULT();
    }
    if (!found)
      break;
    if (fi.IsDir())
      continue;
    RINOK(LoadDll(folderPrefix + fi.Name, true));
  }
  return S_OK;
}

void CCodecs::CloseLibs()
{
  // OutputDebugStringA("~CloseLibs start");
  /*
  WIN32: FreeLibrary() (CLibrary::Free()) function doesn't work as expected,
  if it's called from another FreeLibrary() call.
  So we need to call FreeLibrary() before global destructors.
  
  Also we free global links from DLLs to object of this module before CLibrary::Free() call.
  */
  
  FOR_VECTOR(i, Libs)
  {
    const CCodecLib &lib = Libs[i];
    if (lib.SetCodecs)
      lib.SetCodecs(NULL);
  }
  
  // OutputDebugStringA("~CloseLibs after SetCodecs");
  Libs.Clear();
  // OutputDebugStringA("~CloseLibs end");
}

#endif // EXTERNAL_CODECS


HRESULT CCodecs::Load()
{
  #ifdef NEW_FOLDER_INTERFACE
  InternalIcons.LoadIcons(g_hInstance);
  #endif

  Formats.Clear();
  
  #ifdef EXTERNAL_CODECS
    Errors.Clear();
    MainDll_ErrorPath.Empty();
    Codecs.Clear();
    Hashers.Clear();
  #endif
  
  for (UInt32 i = 0; i < g_NumArcs; i++)
  {
    const CArcInfo &arc = *g_Arcs[i];
    CArcInfoEx item;
    
    item.Name = arc.Name;
    item.CreateInArchive = arc.CreateInArchive;
    item.IsArcFunc = arc.IsArc;
    item.Flags = arc.Flags;
  
    {
      UString e, ae;
      if (arc.Ext)
        e = arc.Ext;
      if (arc.AddExt)
        ae = arc.AddExt;
      item.AddExts(e, ae);
    }

    #ifndef _SFX

    item.CreateOutArchive = arc.CreateOutArchive;
    item.UpdateEnabled = (arc.CreateOutArchive != NULL);
    item.SignatureOffset = arc.SignatureOffset;
    // item.Version = MY_VER_MIX;
    item.NewInterface = true;
    
    if (arc.IsMultiSignature())
      ParseSignatures(arc.Signature, arc.SignatureSize, item.Signatures);
    else
      item.Signatures.AddNew().CopyFrom(arc.Signature, arc.SignatureSize);
    
    #endif

    Formats.Add(item);
  }
  
  // printf("\nLoad codecs \n");

  #ifdef EXTERNAL_CODECS
    const FString baseFolder = GetBaseFolderPrefixFromRegistry();
    {
      bool loadedOK;
      RINOK(LoadDll(baseFolder + kMainDll, false, &loadedOK));
      if (!loadedOK)
        MainDll_ErrorPath = kMainDll;
    }
    RINOK(LoadDllsFromFolder(baseFolder + kCodecsFolderName));
    RINOK(LoadDllsFromFolder(baseFolder + kFormatsFolderName));

  NeedSetLibCodecs = true;
    
  if (Libs.Size() == 0)
    NeedSetLibCodecs = false;
  else if (Libs.Size() == 1)
  {
    // we don't need to set ISetCompressCodecsInfo, if all arcs and codecs are in one external module.
    #ifndef EXPORT_CODECS
    if (g_NumArcs == 0)
      NeedSetLibCodecs = false;
    #endif
  }

  if (NeedSetLibCodecs)
  {
    /* 15.00: now we call global function in DLL: SetCompressCodecsInfo(c)
       old versions called only ISetCompressCodecsInfo::SetCompressCodecsInfo(c) for each archive handler */

    FOR_VECTOR(i, Libs)
    {
      CCodecLib &lib = Libs[i];
      MY_GET_FUNC (lib.SetCodecs, Func_SetCodecs, lib.Lib.GetProc("SetCodecs"));
      if (lib.SetCodecs)
      {
        RINOK(lib.SetCodecs(this));
      }
    }
  }

  #endif

  return S_OK;
}

#ifndef _SFX

int CCodecs::FindFormatForArchiveName(const UString &arcPath) const
{
  int dotPos = arcPath.ReverseFind_Dot();
  if (dotPos <= arcPath.ReverseFind_PathSepar())
    return -1;
  const UString ext = arcPath.Ptr((unsigned)(dotPos + 1));
  if (ext.IsEmpty())
    return -1;
  if (ext.IsEqualTo_Ascii_NoCase("exe"))
    return -1;
  FOR_VECTOR (i, Formats)
  {
    const CArcInfoEx &arc = Formats[i];
    /*
    if (!arc.UpdateEnabled)
      continue;
    */
    if (arc.FindExtension(ext) >= 0)
      return (int)i;
  }
  return -1;
}

int CCodecs::FindFormatForExtension(const UString &ext) const
{
  if (ext.IsEmpty())
    return -1;
  FOR_VECTOR (i, Formats)
    if (Formats[i].FindExtension(ext) >= 0)
      return (int)i;
  return -1;
}

int CCodecs::FindFormatForArchiveType(const UString &arcType) const
{
  FOR_VECTOR (i, Formats)
    if (Formats[i].Name.IsEqualTo_NoCase(arcType))
      return (int)i;
  return -1;
}

bool CCodecs::FindFormatForArchiveType(const UString &arcType, CIntVector &formatIndices) const
{
  formatIndices.Clear();
  for (unsigned pos = 0; pos < arcType.Len();)
  {
    int pos2 = arcType.Find(L'.', pos);
    if (pos2 < 0)
      pos2 = (int)arcType.Len();
    const UString name = arcType.Mid(pos, (unsigned)pos2 - pos);
    if (name.IsEmpty())
      return false;
    int index = FindFormatForArchiveType(name);
    if (index < 0 && name != L"*")
    {
      formatIndices.Clear();
      return false;
    }
    formatIndices.Add(index);
    pos = (unsigned)pos2 + 1;
  }
  return true;
}

#endif // _SFX


#ifdef NEW_FOLDER_INTERFACE

void CCodecIcons::LoadIcons(HMODULE m)
{
  UString iconTypes;
  MyLoadString(m, kIconTypesResId, iconTypes);
  UStringVector pairs;
  SplitString(iconTypes, pairs);
  FOR_VECTOR (i, pairs)
  {
    const UString &s = pairs[i];
    int pos = s.Find(L':');
    CIconPair iconPair;
    iconPair.IconIndex = -1;
    if (pos < 0)
      pos = (int)s.Len();
    else
    {
      const UString num = s.Ptr((unsigned)pos + 1);
      if (!num.IsEmpty())
      {
        const wchar_t *end;
        iconPair.IconIndex = (int)ConvertStringToUInt32(num, &end);
        if (*end != 0)
          continue;
      }
    }
    iconPair.Ext = s.Left((unsigned)pos);
    IconPairs.Add(iconPair);
  }
}

bool CCodecIcons::FindIconIndex(const UString &ext, int &iconIndex) const
{
  iconIndex = -1;
  FOR_VECTOR (i, IconPairs)
  {
    const CIconPair &pair = IconPairs[i];
    if (ext.IsEqualTo_NoCase(pair.Ext))
    {
      iconIndex = pair.IconIndex;
      return true;
    }
  }
  return false;
}

#endif // NEW_FOLDER_INTERFACE


#ifdef EXTERNAL_CODECS

// #define EXPORT_CODECS

#ifdef EXPORT_CODECS

extern unsigned g_NumCodecs;
STDAPI CreateDecoder(UInt32 index, const GUID *iid, void **outObject);
STDAPI CreateEncoder(UInt32 index, const GUID *iid, void **outObject);
STDAPI GetMethodProperty(UInt32 codecIndex, PROPID propID, PROPVARIANT *value);
#define NUM_EXPORT_CODECS g_NumCodecs

extern unsigned g_NumHashers;
STDAPI CreateHasher(UInt32 index, IHasher **hasher);
STDAPI GetHasherProp(UInt32 codecIndex, PROPID propID, PROPVARIANT *value);
#define NUM_EXPORT_HASHERS g_NumHashers

#else // EXPORT_CODECS

#define NUM_EXPORT_CODECS 0
#define NUM_EXPORT_HASHERS 0

#endif // EXPORT_CODECS

STDMETHODIMP CCodecs::GetNumMethods(UInt32 *numMethods)
{
  *numMethods = NUM_EXPORT_CODECS
    #ifdef EXTERNAL_CODECS
    + Codecs.Size()
    #endif
    ;
  return S_OK;
}

STDMETHODIMP CCodecs::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
{
  #ifdef EXPORT_CODECS
  if (index < g_NumCodecs)
    return GetMethodProperty(index, propID, value);
  #endif

  #ifdef EXTERNAL_CODECS
  const CDllCodecInfo &ci = Codecs[index - NUM_EXPORT_CODECS];

  if (propID == NMethodPropID::kDecoderIsAssigned ||
      propID == NMethodPropID::kEncoderIsAssigned)
  {
    NCOM::CPropVariant prop;
    prop = (bool)((propID == NMethodPropID::kDecoderIsAssigned) ?
        ci.DecoderIsAssigned :
        ci.EncoderIsAssigned);
    prop.Detach(value);
    return S_OK;
  }
  const CCodecLib &lib = Libs[ci.LibIndex];
  return lib.GetMethodProperty(ci.CodecIndex, propID, value);
  #else
  return E_FAIL;
  #endif
}

STDMETHODIMP CCodecs::CreateDecoder(UInt32 index, const GUID *iid, void **coder)
{
  #ifdef EXPORT_CODECS
  if (index < g_NumCodecs)
    return CreateDecoder(index, iid, coder);
  #endif
  
  #ifdef EXTERNAL_CODECS
  const CDllCodecInfo &ci = Codecs[index - NUM_EXPORT_CODECS];
  if (ci.DecoderIsAssigned)
  {
    const CCodecLib &lib = Libs[ci.LibIndex];
    if (lib.CreateDecoder)
      return lib.CreateDecoder(ci.CodecIndex, iid, (void **)coder);
    if (lib.CreateObject)
      return lib.CreateObject(&ci.Decoder, iid, (void **)coder);
  }
  return S_OK;
  #else
  return E_FAIL;
  #endif
}

STDMETHODIMP CCodecs::CreateEncoder(UInt32 index, const GUID *iid, void **coder)
{
  #ifdef EXPORT_CODECS
  if (index < g_NumCodecs)
    return CreateEncoder(index, iid, coder);
  #endif

  #ifdef EXTERNAL_CODECS
  const CDllCodecInfo &ci = Codecs[index - NUM_EXPORT_CODECS];
  if (ci.EncoderIsAssigned)
  {
    const CCodecLib &lib = Libs[ci.LibIndex];
    if (lib.CreateEncoder)
      return lib.CreateEncoder(ci.CodecIndex, iid, (void **)coder);
    if (lib.CreateObject)
      return lib.CreateObject(&ci.Encoder, iid, (void **)coder);
  }
  return S_OK;
  #else
  return E_FAIL;
  #endif
}


STDMETHODIMP_(UInt32) CCodecs::GetNumHashers()
{
  return NUM_EXPORT_HASHERS
    #ifdef EXTERNAL_CODECS
    + Hashers.Size()
    #endif
    ;
}

STDMETHODIMP CCodecs::GetHasherProp(UInt32 index, PROPID propID, PROPVARIANT *value)
{
  #ifdef EXPORT_CODECS
  if (index < g_NumHashers)
    return ::GetHasherProp(index, propID, value);
  #endif

  #ifdef EXTERNAL_CODECS
  const CDllHasherInfo &ci = Hashers[index - NUM_EXPORT_HASHERS];
  return Libs[ci.LibIndex].ComHashers->GetHasherProp(ci.HasherIndex, propID, value);
  #else
  return E_FAIL;
  #endif
}

STDMETHODIMP CCodecs::CreateHasher(UInt32 index, IHasher **hasher)
{
  #ifdef EXPORT_CODECS
  if (index < g_NumHashers)
    return CreateHasher(index, hasher);
  #endif
  #ifdef EXTERNAL_CODECS
  const CDllHasherInfo &ci = Hashers[index - NUM_EXPORT_HASHERS];
  return Libs[ci.LibIndex].ComHashers->CreateHasher(ci.HasherIndex, hasher);
  #else
  return E_FAIL;
  #endif
}

int CCodecs::GetCodec_LibIndex(UInt32 index) const
{
  #ifdef EXPORT_CODECS
  if (index < g_NumCodecs)
    return -1;
  #endif
  
  #ifdef EXTERNAL_CODECS
  const CDllCodecInfo &ci = Codecs[index - NUM_EXPORT_CODECS];
  return (int)ci.LibIndex;
  #else
  return -1;
  #endif
}

int CCodecs::GetHasherLibIndex(UInt32 index)
{
  #ifdef EXPORT_CODECS
  if (index < g_NumHashers)
    return -1;
  #endif
  
  #ifdef EXTERNAL_CODECS
  const CDllHasherInfo &ci = Hashers[index - NUM_EXPORT_HASHERS];
  return (int)ci.LibIndex;
  #else
  return -1;
  #endif
}

bool CCodecs::GetCodec_DecoderIsAssigned(UInt32 index) const
{
  #ifdef EXPORT_CODECS
  if (index < g_NumCodecs)
  {
    NCOM::CPropVariant prop;
    if (GetProperty(index, NMethodPropID::kDecoderIsAssigned, &prop) == S_OK)
    {
      if (prop.vt == VT_BOOL)
        return VARIANT_BOOLToBool(prop.boolVal);
    }
    return false;
  }
  #endif
  
  #ifdef EXTERNAL_CODECS
  return Codecs[index - NUM_EXPORT_CODECS].DecoderIsAssigned;
  #else
  return false;
  #endif
}

bool CCodecs::GetCodec_EncoderIsAssigned(UInt32 index) const
{
  #ifdef EXPORT_CODECS
  if (index < g_NumCodecs)
  {
    NCOM::CPropVariant prop;
    if (GetProperty(index, NMethodPropID::kEncoderIsAssigned, &prop) == S_OK)
    {
      if (prop.vt == VT_BOOL)
        return VARIANT_BOOLToBool(prop.boolVal);
    }
    return false;
  }
  #endif
  
  #ifdef EXTERNAL_CODECS
  return Codecs[index - NUM_EXPORT_CODECS].EncoderIsAssigned;
  #else
  return false;
  #endif
}

UInt32 CCodecs::GetCodec_NumStreams(UInt32 index)
{
  NCOM::CPropVariant prop;
  if (GetProperty(index, NMethodPropID::kPackStreams, &prop) != S_OK)
    return 0;
  if (prop.vt == VT_UI4)
    return (UInt32)prop.ulVal;
  if (prop.vt == VT_EMPTY)
    return 1;
  return 0;
}

HRESULT CCodecs::GetCodec_Id(UInt32 index, UInt64 &id)
{
  NCOM::CPropVariant prop;
  RINOK(GetProperty(index, NMethodPropID::kID, &prop));
  if (prop.vt != VT_UI8)
    return E_INVALIDARG;
  id = prop.uhVal.QuadPart;
  return S_OK;
}

AString CCodecs::GetCodec_Name(UInt32 index)
{
  AString s;
  NCOM::CPropVariant prop;
  if (GetProperty(index, NMethodPropID::kName, &prop) == S_OK)
    if (prop.vt == VT_BSTR)
      s.SetFromWStr_if_Ascii(prop.bstrVal);
  return s;
}

UInt64 CCodecs::GetHasherId(UInt32 index)
{
  NCOM::CPropVariant prop;
  if (GetHasherProp(index, NMethodPropID::kID, &prop) != S_OK)
    return 0;
  if (prop.vt != VT_UI8)
    return 0;
  return prop.uhVal.QuadPart;
}

AString CCodecs::GetHasherName(UInt32 index)
{
  AString s;
  NCOM::CPropVariant prop;
  if (GetHasherProp(index, NMethodPropID::kName, &prop) == S_OK)
    if (prop.vt == VT_BSTR)
      s.SetFromWStr_if_Ascii(prop.bstrVal);
  return s;
}

UInt32 CCodecs::GetHasherDigestSize(UInt32 index)
{
  NCOM::CPropVariant prop;
  if (GetHasherProp(index, NMethodPropID::kDigestSize, &prop) != S_OK)
    return 0;
  if (prop.vt != VT_UI4)
    return 0;
  return prop.ulVal;
}

void CCodecs::GetCodecsErrorMessage(UString &s)
{
  s.Empty();
  FOR_VECTOR (i, Errors)
  {
    const CCodecError &ce = Errors[i];
    s += "Codec Load Error: ";
    s += fs2us(ce.Path);
    if (ce.ErrorCode != 0)
    {
      s += " : ";
      s += NWindows::NError::MyFormatMessage(ce.ErrorCode);
    }
    if (!ce.Message.IsEmpty())
    {
      s += " : ";
      s += ce.Message;
    }
    s.Add_LF();
  }
}

#endif // EXTERNAL_CODECS

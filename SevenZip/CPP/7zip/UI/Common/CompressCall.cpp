// CompressCall.cpp

#include "StdAfx.h"

#include <wchar.h>

#include "../../../Common/IntToString.h"
#include "../../../Common/MyCom.h"
#include "../../../Common/Random.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileMapping.h"
#include "../../../Windows/MemoryLock.h"
#include "../../../Windows/ProcessUtils.h"
#include "../../../Windows/Synchronization.h"

#include "../FileManager/StringUtils.h"
#include "../FileManager/RegistryUtils.h"

#include "ZipRegistry.h"
#include "CompressCall.h"

using namespace NWindows;

#define MY_TRY_BEGIN try {

#define MY_TRY_FINISH } \
  catch(...) { ErrorMessageHRESULT(E_FAIL); return E_FAIL; }

#define MY_TRY_FINISH_VOID } \
  catch(...) { ErrorMessageHRESULT(E_FAIL); }

#define k7zGui  "NanaZipG.exe"

// 21.07 : we can disable wildcard
// #define ISWITCH_NO_WILDCARD_POSTFIX "w-"
#define ISWITCH_NO_WILDCARD_POSTFIX

#define kShowDialogSwitch  " -ad"
#define kEmailSwitch  " -seml."
#define kArchiveTypeSwitch  " -t"
#define kIncludeSwitch  " -i" ISWITCH_NO_WILDCARD_POSTFIX
#define kArcIncludeSwitches  " -an -ai" ISWITCH_NO_WILDCARD_POSTFIX
#define kHashIncludeSwitches  kIncludeSwitch
#define kStopSwitchParsing  " --"

static NCompression::CInfo m_RegistryInfo;
extern HWND g_HWND;

UString GetQuotedString(const UString &s)
{
  UString s2 ('\"');
  s2 += s;
  s2 += '\"';
  return s2;
}

static void ErrorMessage(LPCWSTR message)
{
  MessageBoxW(g_HWND, message, L"NanaZip", MB_ICONERROR | MB_OK);
}

static void ErrorMessageHRESULT(HRESULT res, LPCWSTR s = NULL)
{
  UString s2 = NError::MyFormatMessage(res);
  if (s)
  {
    s2.Add_LF();
    s2 += s;
  }
  ErrorMessage(s2);
}

static HRESULT Call7zGui(const UString &params,
    // LPCWSTR curDir,
    bool waitFinish,
    NSynchronization::CBaseEvent *event)
{
  UString imageName = fs2us(NWindows::NDLL::GetModuleDirPrefix());
  imageName += k7zGui;

  CProcess process;
  const WRes wres = process.Create(imageName, params, NULL); // curDir);
  if (wres != 0)
  {
    HRESULT hres = HRESULT_FROM_WIN32(wres);
    ErrorMessageHRESULT(hres, imageName);
    return hres;
  }
  if (waitFinish)
    process.Wait();
  else if (event != NULL)
  {
    HANDLE handles[] = { process, *event };
    ::WaitForMultipleObjects(ARRAY_SIZE(handles), handles, FALSE, INFINITE);
  }
  return S_OK;
}

static void AddLagePagesSwitch(UString &params)
{
  if (ReadLockMemoryEnable())
  #ifndef UNDER_CE
  if (NSecurity::Get_LargePages_RiskLevel() == 0)
  #endif
    params += " -slp";
}

class CRandNameGenerator
{
  CRandom _random;
public:
  CRandNameGenerator() { _random.Init(); }
  void GenerateName(UString &s, const char *prefix)
  {
    s += prefix;
    s.Add_UInt32((UInt32)(unsigned)_random.Generate());
  }
};

static HRESULT CreateMap(const UStringVector &names,
    CFileMapping &fileMapping, NSynchronization::CManualResetEvent &event,
    UString &params)
{
  size_t totalSize = 1;
  {
    FOR_VECTOR (i, names)
      totalSize += (names[i].Len() + 1);
  }
  totalSize *= sizeof(wchar_t);

  CRandNameGenerator random;

  UString mappingName;
  for (;;)
  {
    random.GenerateName(mappingName, "7zMap");
    const WRes wres = fileMapping.Create(PAGE_READWRITE, totalSize, GetSystemString(mappingName));
    if (fileMapping.IsCreated() && wres == 0)
      break;
    if (wres != ERROR_ALREADY_EXISTS)
      return HRESULT_FROM_WIN32(wres);
    fileMapping.Close();
  }

  UString eventName;
  for (;;)
  {
    random.GenerateName(eventName, "7zEvent");
    const WRes wres = event.CreateWithName(false, GetSystemString(eventName));
    if (event.IsCreated() && wres == 0)
      break;
    if (wres != ERROR_ALREADY_EXISTS)
      return HRESULT_FROM_WIN32(wres);
    event.Close();
  }

  params += '#';
  params += mappingName;
  params += ':';
  char temp[32];
  ConvertUInt64ToString(totalSize, temp);
  params += temp;

  params += ':';
  params += eventName;

  LPVOID data = fileMapping.Map(FILE_MAP_WRITE, 0, totalSize);
  if (!data)
    return E_FAIL;
  CFileUnmapper unmapper(data);
  {
    wchar_t *cur = (wchar_t *)data;
    *cur++ = 0; // it means wchar_t strings (UTF-16 in WIN32)
    FOR_VECTOR (i, names)
    {
      const UString &s = names[i];
      unsigned len = s.Len() + 1;
      wmemcpy(cur, (const wchar_t *)s, len);
      cur += len;
    }
  }
  return S_OK;
}

int FindRegistryFormat(const UString &name)
{
  FOR_VECTOR (i, m_RegistryInfo.Formats)
  {
    const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[i];
    if (name.IsEqualTo_NoCase(GetUnicodeString(fo.FormatID)))
      return i;
  }
  return -1;
}

int FindRegistryFormatAlways(const UString &name)
{
  int index = FindRegistryFormat(name);
  if (index < 0)
  {
    NCompression::CFormatOptions fo;
    fo.FormatID = GetSystemString(name);
    index = m_RegistryInfo.Formats.Add(fo);
  }
  return index;
}

HRESULT CompressFiles(
    const UString &arcPathPrefix,
    const UString &arcName,
    const UString &arcType,
    bool addExtension,
    const UStringVector &names,
    bool email, bool showDialog, bool waitFinish)
{
  MY_TRY_BEGIN
  UString params ('a');

  CFileMapping fileMapping;
  NSynchronization::CManualResetEvent event;
  params += kIncludeSwitch;
  RINOK(CreateMap(names, fileMapping, event, params));

  if (!arcType.IsEmpty() && arcType == L"7z")
  {
    int index;
    params += kArchiveTypeSwitch;
    params += arcType;
    m_RegistryInfo.Load();
    index = FindRegistryFormatAlways(arcType);
    if (index >= 0)
    {
      char temp[64];
      const NCompression::CFormatOptions &fo = m_RegistryInfo.Formats[index];

      if (!fo.Method.IsEmpty())
      {
        params += " -m0=";
        params += fo.Method;
      }

      if (fo.Level)
      {
        params += " -mx=";
        ConvertUInt32ToString(fo.Level, temp);
        params += temp;
      }

      if (fo.Dictionary)
      {
        params += " -md=";
        ConvertUInt32ToString(fo.Dictionary, temp);
        params += temp;
        params += "b";
      }

      if (fo.BlockLogSize)
      {
        params += " -ms=";
        ConvertUInt64ToString(1ULL << fo.BlockLogSize, temp);
        params += temp;
        params += "b";
      }

      if (fo.NumThreads)
      {
        params += " -mmt=";
        ConvertUInt32ToString(fo.NumThreads, temp);
        params += temp;
      }

      if (!fo.Options.IsEmpty())
      {
        UStringVector strings;
        SplitString(fo.Options, strings);
        FOR_VECTOR (i, strings)
        {
          params += " -m";
          params += strings[i];
        }
      }
    }
  }

  if (email)
    params += kEmailSwitch;

  if (showDialog)
    params += kShowDialogSwitch;

  AddLagePagesSwitch(params);

  if (arcName.IsEmpty())
    params += " -an";

  if (addExtension)
    params += " -saa";
  else
    params += " -sae";

  params += kStopSwitchParsing;
  params.Add_Space();

  if (!arcName.IsEmpty())
  {
    params += GetQuotedString(
    // #ifdef UNDER_CE
      arcPathPrefix +
    // #endif
    arcName);
  }

  return Call7zGui(params,
      // (arcPathPrefix.IsEmpty()? 0: (LPCWSTR)arcPathPrefix),
      waitFinish, &event);
  MY_TRY_FINISH
}

static void ExtractGroupCommand(const UStringVector &arcPaths, UString &params, bool isHash)
{
  AddLagePagesSwitch(params);
  params += (isHash ? kHashIncludeSwitches : kArcIncludeSwitches);
  CFileMapping fileMapping;
  NSynchronization::CManualResetEvent event;
  HRESULT result = CreateMap(arcPaths, fileMapping, event, params);
  if (result == S_OK)
    result = Call7zGui(params, false, &event);
  if (result != S_OK)
    ErrorMessageHRESULT(result);
}

void ExtractArchives(const UStringVector &arcPaths, const UString &outFolder, bool showDialog, bool elimDup)
{
  MY_TRY_BEGIN
  UString params ('x');
  if (!outFolder.IsEmpty())
  {
    params += " -o";
    params += GetQuotedString(outFolder);
  }
  if (elimDup)
    params += " -spe";
  if (showDialog)
    params += kShowDialogSwitch;
  ExtractGroupCommand(arcPaths, params, false);
  MY_TRY_FINISH_VOID
}


void TestArchives(const UStringVector &arcPaths, bool hashMode)
{
  MY_TRY_BEGIN
  UString params ('t');
  if (hashMode)
  {
    params += kArchiveTypeSwitch;
    params += "hash";
  }
  ExtractGroupCommand(arcPaths, params, false);
  MY_TRY_FINISH_VOID
}


void CalcChecksum(const UStringVector &paths,
    const UString &methodName,
    const UString &arcPathPrefix,
    const UString &arcFileName)
{
  MY_TRY_BEGIN

  if (!arcFileName.IsEmpty())
  {
    CompressFiles(
      arcPathPrefix,
      arcFileName,
      UString("hash"),
      false, // addExtension,
      paths,
      false, // email,
      false, // showDialog,
      false  // waitFinish
      );
    return;
  }

  UString params ('h');
  if (!methodName.IsEmpty())
  {
    params += " -scrc";
    params += methodName;
    /*
    if (!arcFileName.IsEmpty())
    {
      // not used alternate method of generating file
      params += " -scrf=";
      params += GetQuotedString(arcPathPrefix + arcFileName);
    }
    */
  }
  ExtractGroupCommand(paths, params, true);
  MY_TRY_FINISH_VOID
}

void Benchmark(bool totalMode)
{
  MY_TRY_BEGIN
  UString params ('b');
  if (totalMode)
    params += " -mm=*";
  AddLagePagesSwitch(params);
  HRESULT result = Call7zGui(params, false, NULL);
  if (result != S_OK)
    ErrorMessageHRESULT(result);
  MY_TRY_FINISH_VOID
}

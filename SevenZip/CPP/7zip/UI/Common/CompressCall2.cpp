// CompressCall2.cpp

#include "StdAfx.h"

#include "../../../Common/MyException.h"

#include "../../UI/Common/EnumDirItems.h"

#include "../../UI/FileManager/LangUtils.h"

#include "../../UI/GUI/BenchmarkDialog.h"
#include "../../UI/GUI/ExtractGUI.h"
#include "../../UI/GUI/UpdateGUI.h"
#include "../../UI/GUI/HashGUI.h"

#include "../../UI/GUI/ExtractRes.h"

#include "CompressCall.h"

extern HWND g_HWND;

#define MY_TRY_BEGIN  HRESULT result; try {
#define MY_TRY_FINISH } \
  catch(CSystemException &e) { result = e.ErrorCode; } \
  catch(UString &s) { ErrorMessage(s); result = E_FAIL; } \
  catch(...) { result = E_FAIL; } \
  if (result != S_OK && result != E_ABORT) \
    ErrorMessageHRESULT(result);

static void ThrowException_if_Error(HRESULT res)
{
  if (res != S_OK)
    throw CSystemException(res);
}

#ifdef EXTERNAL_CODECS

#define CREATE_CODECS \
  CCodecs *codecs = new CCodecs; \
  CMyComPtr<ICompressCodecsInfo> compressCodecsInfo = codecs; \
  ThrowException_if_Error(codecs->Load());

#define LOAD_EXTERNAL_CODECS \
    CExternalCodecs __externalCodecs; \
    __externalCodecs.GetCodecs = codecs; \
    __externalCodecs.GetHashers = codecs; \
    ThrowException_if_Error(__externalCodecs.Load());

#else

#define CREATE_CODECS \
  CCodecs *codecs = new CCodecs; \
  CMyComPtr<IUnknown> compressCodecsInfo = codecs; \
  ThrowException_if_Error(codecs->Load());

#define LOAD_EXTERNAL_CODECS

#endif



 
UString GetQuotedString(const UString &s)
{
  UString s2 ('\"');
  s2 += s;
  s2 += '\"';
  return s2;
}

static void ErrorMessage(LPCWSTR message)
{
  MessageBoxW(g_HWND, message, L"7-Zip", MB_ICONERROR);
}

static void ErrorMessageHRESULT(HRESULT res)
{
  ErrorMessage(HResultToMessage(res));
}

static void ErrorLangMessage(UINT resourceID)
{
  ErrorMessage(LangString(resourceID));
}

HRESULT CompressFiles(
    const UString &arcPathPrefix,
    const UString &arcName,
    const UString &arcType,
    bool addExtension,
    const UStringVector &names,
    bool email, bool showDialog, bool /* waitFinish */)
{
  MY_TRY_BEGIN
  
  CREATE_CODECS

  CUpdateCallbackGUI callback;
  
  callback.Init();

  CUpdateOptions uo;
  uo.EMailMode = email;
  uo.SetActionCommand_Add();

  uo.ArcNameMode = (addExtension ? k_ArcNameMode_Add : k_ArcNameMode_Exact);

  CObjectVector<COpenType> formatIndices;
  if (!ParseOpenTypes(*codecs, arcType, formatIndices))
  {
    ErrorLangMessage(IDS_UNSUPPORTED_ARCHIVE_TYPE);
    return E_FAIL;
  }
  const UString arcPath = arcPathPrefix + arcName;
  if (!uo.InitFormatIndex(codecs, formatIndices, arcPath) ||
      !uo.SetArcPath(codecs, arcPath))
  {
    ErrorLangMessage(IDS_UPDATE_NOT_SUPPORTED);
    return E_FAIL;
  }

  NWildcard::CCensor censor;
  FOR_VECTOR (i, names)
  {
    censor.AddPreItem(names[i]);
  }

  bool messageWasDisplayed = false;

  result = UpdateGUI(codecs,
      formatIndices, arcPath,
      censor, uo, showDialog, messageWasDisplayed, &callback, g_HWND);
  
  if (result != S_OK)
  {
    if (result != E_ABORT && messageWasDisplayed)
      return E_FAIL;
    throw CSystemException(result);
  }
  if (callback.FailedFiles.Size() > 0)
  {
    if (!messageWasDisplayed)
      throw CSystemException(E_FAIL);
    return E_FAIL;
  }
  
  MY_TRY_FINISH
  return S_OK;
}


static HRESULT ExtractGroupCommand(const UStringVector &arcPaths,
    bool showDialog, const UString &outFolder, bool testMode, bool elimDup = false)
{
  MY_TRY_BEGIN
  
  CREATE_CODECS

  CExtractOptions eo;
  eo.OutputDir = us2fs(outFolder);
  eo.TestMode = testMode;
  eo.ElimDup.Val = elimDup;
  eo.ElimDup.Def = elimDup;

  CExtractCallbackImp *ecs = new CExtractCallbackImp;
  CMyComPtr<IFolderArchiveExtractCallback> extractCallback = ecs;
  
  ecs->Init();
  
  // eo.CalcCrc = options.CalcCrc;
  
  UStringVector arcPathsSorted;
  UStringVector arcFullPathsSorted;
  {
    NWildcard::CCensor arcCensor;
    FOR_VECTOR (i, arcPaths)
    {
      arcCensor.AddPreItem(arcPaths[i]);
    }
    arcCensor.AddPathsToCensor(NWildcard::k_RelatPath);
    CDirItemsStat st;
    EnumerateDirItemsAndSort(arcCensor, NWildcard::k_RelatPath, UString(),
        arcPathsSorted, arcFullPathsSorted,
        st,
        NULL // &scan: change it!!!!
        );
  }
  
  CObjectVector<COpenType> formatIndices;
  
  NWildcard::CCensor censor;
  {
    censor.AddPreItem_Wildcard();
  }
  
  censor.AddPathsToCensor(NWildcard::k_RelatPath);

  bool messageWasDisplayed = false;

  ecs->MultiArcMode = (arcPathsSorted.Size() > 1);

  result = ExtractGUI(codecs,
      formatIndices, CIntVector(),
      arcPathsSorted, arcFullPathsSorted,
      censor.Pairs.Front().Head, eo, NULL, showDialog, messageWasDisplayed, ecs, g_HWND);

  if (result != S_OK)
  {
    if (result != E_ABORT && messageWasDisplayed)
      return E_FAIL;
    throw CSystemException(result);
  }
  return ecs->IsOK() ? S_OK : E_FAIL;
  
  MY_TRY_FINISH
  return result;
}

void ExtractArchives(const UStringVector &arcPaths, const UString &outFolder, bool showDialog, bool elimDup)
{
  ExtractGroupCommand(arcPaths, showDialog, outFolder, false, elimDup);
}

void TestArchives(const UStringVector &arcPaths)
{
  ExtractGroupCommand(arcPaths, true, UString(), true);
}

void CalcChecksum(const UStringVector &paths, const UString &methodName)
{
  MY_TRY_BEGIN
  
  CREATE_CODECS
  LOAD_EXTERNAL_CODECS

  NWildcard::CCensor censor;
  FOR_VECTOR (i, paths)
  {
    censor.AddPreItem(paths[i]);
  }

  censor.AddPathsToCensor(NWildcard::k_RelatPath);
  bool messageWasDisplayed = false;

  CHashOptions options;
  options.Methods.Add(methodName);

  result = HashCalcGUI(EXTERNAL_CODECS_VARS_L censor, options, messageWasDisplayed);
  if (result != S_OK)
  {
    if (result != E_ABORT && messageWasDisplayed)
      return; //  E_FAIL;
    throw CSystemException(result);
  }
  
  MY_TRY_FINISH
  return; //  result;
}

void Benchmark(bool totalMode)
{
  MY_TRY_BEGIN
  
  CREATE_CODECS
  LOAD_EXTERNAL_CODECS
  
  CObjectVector<CProperty> props;
  if (totalMode)
  {
    CProperty prop;
    prop.Name = "m";
    prop.Value = "*";
    props.Add(prop);
  }
  result = Benchmark(
      EXTERNAL_CODECS_VARS_L
      props,
      k_NumBenchIterations_Default,
      g_HWND);
  
  MY_TRY_FINISH
}

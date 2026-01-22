// ExtractGUI.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/Thread.h"

#include "../FileManager/ExtractCallback.h"
#include "../FileManager/FormatUtils.h"
#include "../FileManager/LangUtils.h"
#include "../FileManager/resourceGui.h"
#include "../FileManager/OverwriteDialogRes.h"

#include "../Common/ArchiveExtractCallback.h"
#include "../Common/PropIDUtils.h"

#include "../Explorer/MyMessages.h"

#include "resource2.h"
#include "ExtractRes.h"

#include "ExtractDialog.h"
#include "ExtractGUI.h"
#include "HashGUI.h"

#include "../FileManager/PropertyNameRes.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;

static const wchar_t * const kIncorrectOutDir = L"Incorrect output directory path";

#ifndef Z7_SFX

static void AddValuePair(UString &s, UINT resourceID, UInt64 value, bool addColon = true)
{
  AddLangString(s, resourceID);
  if (addColon)
    s.Add_Colon();
  s.Add_Space();
  s.Add_UInt64(value);
  s.Add_LF();
}

static void AddSizePair(UString &s, UINT resourceID, UInt64 value)
{
  AddLangString(s, resourceID);
  s += ": ";
  AddSizeValue(s, value);
  s.Add_LF();
}

#endif

class CThreadExtracting: public CProgressThreadVirt
{
  HRESULT ProcessVirt() Z7_override;
public:
  /*
  #ifdef Z7_EXTERNAL_CODECS
  const CExternalCodecs *externalCodecs;
  #endif
  */

  CCodecs *codecs;
  CExtractCallbackImp *ExtractCallbackSpec;
  const CObjectVector<COpenType> *FormatIndices;
  const CIntVector *ExcludedFormatIndices;

  UStringVector *ArchivePaths;
  UStringVector *ArchivePathsFull;
  const NWildcard::CCensorNode *WildcardCensor;
  const CExtractOptions *Options;

  #ifndef Z7_SFX
  CHashBundle *HashBundle;
  virtual void ProcessWasFinished_GuiVirt() Z7_override;
  #endif

  CMyComPtr<IFolderArchiveExtractCallback> FolderArchiveExtractCallback;
  UString Title;

  CPropNameValPairs Pairs;

  // **************** 7-Zip ZS Modification Start ****************
#ifndef Z7_SFX
  FString FirstExtractedPath;
#endif
  // **************** 7-Zip ZS Modification End ****************
};


#ifndef Z7_SFX
void CThreadExtracting::ProcessWasFinished_GuiVirt()
{
  if (HashBundle && !Pairs.IsEmpty())
    ShowHashResults(Pairs, *this);
}
#endif

HRESULT CThreadExtracting::ProcessVirt()
{
  CDecompressStat Stat;
  
  #ifndef Z7_SFX
  /*
  if (HashBundle)
    HashBundle->Init();
  */
  #endif

  HRESULT res = Extract(
      /*
      #ifdef Z7_EXTERNAL_CODECS
      externalCodecs,
      #endif
      */
      codecs,
      *FormatIndices, *ExcludedFormatIndices,
      *ArchivePaths, *ArchivePathsFull,
      *WildcardCensor, *Options,
      ExtractCallbackSpec, ExtractCallbackSpec, FolderArchiveExtractCallback,
      #ifndef Z7_SFX
        HashBundle,
      #endif
      FinalMessage.ErrorMessage.Message, Stat);
  
  #ifndef Z7_SFX
  if (res == S_OK && ExtractCallbackSpec->IsOK())
  {
    // **************** 7-Zip ZS Modification Start ****************
    FirstExtractedPath = Stat.FirstExtractedPath;
    // **************** 7-Zip ZS Modification End ****************
    if (HashBundle)
    {
      AddValuePair(Pairs, IDS_ARCHIVES_COLON, Stat.NumArchives);
      AddSizeValuePair(Pairs, IDS_PROP_PACKED_SIZE, Stat.PackSize);
      AddHashBundleRes(Pairs, *HashBundle);
    }
    else if (Options->TestMode)
    {
      UString s;
    
      AddValuePair(s, IDS_ARCHIVES_COLON, Stat.NumArchives, false);
      AddSizePair(s, IDS_PROP_PACKED_SIZE, Stat.PackSize);

      if (Stat.NumFolders != 0)
        AddValuePair(s, IDS_PROP_FOLDERS, Stat.NumFolders);
      AddValuePair(s, IDS_PROP_FILES, Stat.NumFiles);
      AddSizePair(s, IDS_PROP_SIZE, Stat.UnpackSize);
      if (Stat.NumAltStreams != 0)
      {
        s.Add_LF();
        AddValuePair(s, IDS_PROP_NUM_ALT_STREAMS, Stat.NumAltStreams);
        AddSizePair(s, IDS_PROP_ALT_STREAMS_SIZE, Stat.AltStreams_UnpackSize);
      }
      s.Add_LF();
      AddLangString(s, IDS_MESSAGE_NO_ERRORS);
      FinalMessage.OkMessage.Title = Title;
      FinalMessage.OkMessage.Message = s;
    }
  }
  #endif

  return res;
}

// **************** 7-Zip ZS Modification Start ****************
#ifndef Z7_SFX
#include <shlobj_core.h>
static void BrowseToPath(
    bool explore,
    UString &path)
{
  if (explore /* || (GetFileAttributes(path.Ptr()) & FILE_ATTRIBUTE_DIRECTORY)*/) {
    ShellExecute(NULL, L"explore", path.Ptr(), NULL, NULL, SW_SHOW);
  } else {
  #if (NTDDI_VERSION >= NTDDI_WINXP)
    LPITEMIDLIST pidl = ILCreateFromPath(path.Ptr());
    if (pidl) {
      SHOpenFolderAndSelectItems(pidl,0,0,0);
      ILFree(pidl);
    }
  #else
    UString args = L"/n,/select,\"" + path + L"\"";
    ShellExecute(NULL, L"open", L"explorer.exe", args.Ptr(), NULL, SW_SHOW);
  #endif
  }
}
#endif
// **************** 7-Zip ZS Modification End ****************

HRESULT ExtractGUI(
    // DECL_EXTERNAL_CODECS_LOC_VARS
    CCodecs *codecs,
    const CObjectVector<COpenType> &formatIndices,
    const CIntVector &excludedFormatIndices,
    UStringVector &archivePaths,
    UStringVector &archivePathsFull,
    const NWildcard::CCensorNode &wildcardCensor,
    CExtractOptions &options,
    #ifndef Z7_SFX
    CHashBundle *hb,
    #endif
    bool showDialog,
    bool &messageWasDisplayed,
    CExtractCallbackImp *extractCallback,
    HWND hwndParent)
{
  messageWasDisplayed = false;

  CThreadExtracting extracter;
  /*
  #ifdef Z7_EXTERNAL_CODECS
  extracter.externalCodecs = _externalCodecs;
  #endif
  */
  extracter.codecs = codecs;
  extracter.FormatIndices = &formatIndices;
  extracter.ExcludedFormatIndices = &excludedFormatIndices;

  // **************** 7-Zip ZS Modification Start ****************
#ifndef Z7_SFX
  bool OpnTrgFold = false;
#endif
  // **************** 7-Zip ZS Modification End ****************
  if (!options.TestMode)
  {
    FString outputDir = options.OutputDir;
    #ifndef UNDER_CE
    if (outputDir.IsEmpty())
      GetCurrentDir(outputDir);
    #endif
    if (showDialog)
    {
      CExtractDialog dialog;
      FString outputDirFull;
      if (!MyGetFullPathName(outputDir, outputDirFull))
      {
        ShowErrorMessage(kIncorrectOutDir);
        messageWasDisplayed = true;
        return E_FAIL;
      }
      NName::NormalizeDirPathPrefix(outputDirFull);

      dialog.DirPath = fs2us(outputDirFull);

      dialog.OverwriteMode = options.OverwriteMode;
      dialog.OverwriteMode_Force = options.OverwriteMode_Force;
      dialog.PathMode = options.PathMode;
      dialog.PathMode_Force = options.PathMode_Force;
      dialog.ElimDup = options.ElimDup;

      if (archivePathsFull.Size() == 1)
        dialog.ArcPath = archivePathsFull[0];

      #ifndef Z7_SFX
      // dialog.AltStreams = options.NtOptions.AltStreams;
      dialog.NtSecurity = options.NtOptions.NtSecurity;
      if (extractCallback->PasswordIsDefined)
        dialog.Password = extractCallback->Password;
      #endif

      if (dialog.Create(hwndParent) != IDOK)
        return E_ABORT;

      outputDir = us2fs(dialog.DirPath);

      options.OverwriteMode = dialog.OverwriteMode;
      options.PathMode = dialog.PathMode;
      options.ElimDup = dialog.ElimDup;
      
      #ifndef Z7_SFX
      // **************** 7-Zip ZS Modification Start ****************
      OpnTrgFold = dialog.OpnTrgFold.Val;
      // **************** 7-Zip ZS Modification End ****************
      // options.NtOptions.AltStreams = dialog.AltStreams;
      options.NtOptions.NtSecurity = dialog.NtSecurity;
      extractCallback->Password = dialog.Password;
      extractCallback->PasswordIsDefined = !dialog.Password.IsEmpty();
      #endif
    }
    // **************** 7-Zip ZS Modification Start ****************
    #ifndef Z7_SFX
    else if (!options.OutputDir.IsEmpty()) // don't open target folder if extract here
    {
      // load setting "open target folder" from registry saved by previous dialog
      NExtract::CInfo _info;
      _info.Load();
      OpnTrgFold = _info.OpnTrgFold.Val;
    }
    #endif
    // **************** 7-Zip ZS Modification End ****************
    if (!MyGetFullPathName(outputDir, options.OutputDir))
    {
      ShowErrorMessage(kIncorrectOutDir);
      messageWasDisplayed = true;
      return E_FAIL;
    }
    NName::NormalizeDirPathPrefix(options.OutputDir);
    
    /*
    if (!CreateComplexDirectory(options.OutputDir))
    {
      UString s = GetUnicodeString(NError::MyFormatMessage(GetLastError()));
      UString s2 = MyFormatNew(IDS_CANNOT_CREATE_FOLDER,
      #ifdef Z7_LANG
      0x02000603,
      #endif
      options.OutputDir);
      s2.Add_LF();
      s2 += s;
      MyMessageBox(s2);
      return E_FAIL;
    }
    */
  }
  
  UString title = LangString(options.TestMode ? IDS_PROGRESS_TESTING : IDS_PROGRESS_EXTRACTING);

  extracter.Title = title;
  extracter.ExtractCallbackSpec = extractCallback;
  extracter.ExtractCallbackSpec->ProgressDialog = &extracter;
  extracter.FolderArchiveExtractCallback = extractCallback;
  extracter.ExtractCallbackSpec->Init();

  extracter.CompressingMode = false;

  extracter.ArchivePaths = &archivePaths;
  extracter.ArchivePathsFull = &archivePathsFull;
  extracter.WildcardCensor = &wildcardCensor;
  extracter.Options = &options;
  #ifndef Z7_SFX
  extracter.HashBundle = hb;
  #endif

  extracter.IconID = IDI_ICON;

  RINOK(extracter.Create(title, hwndParent))
  messageWasDisplayed = extracter.ThreadFinishedOK && extracter.MessagesDisplayed;
  // **************** 7-Zip ZS Modification Start ****************
#ifndef Z7_SFX
  // browse/navigate to target path:
  if (OpnTrgFold && extracter.Result == S_OK) {
    // obtain path (directory or file) from first extracted:
    UString extrPath = extracter.FirstExtractedPath;
    if (extrPath.IsEmpty()) {
      extrPath = options.OutputDir;
    }
    else
    if (!options.OutputDir.IsEmpty()) {
      // first subpath relative selected in dialog or given by options.OutputDir:
      UString outDir = options.OutputDir;
      if (outDir.Back() != WCHAR_PATH_SEPARATOR)
        outDir += WCHAR_PATH_SEPARATOR;
      extrPath = extracter.FirstExtractedPath;
      if (extrPath.IsPrefixedBy(outDir)) {
        int subIdx = extrPath.Find(WCHAR_PATH_SEPARATOR, outDir.Len());
        if (subIdx != -1) {
          extrPath = extrPath.Left(subIdx-1);
        }
      }
    }
    if (!extrPath.IsEmpty()) {
      BrowseToPath(0 /* showDialog */, extrPath);
    }
  }
#endif
  // **************** 7-Zip ZS Modification End ****************
  return extracter.Result;
}

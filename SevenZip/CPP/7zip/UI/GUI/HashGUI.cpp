// HashGUI.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/ErrorMsg.h"

#include "../FileManager/FormatUtils.h"
#include "../FileManager/LangUtils.h"
#include "../FileManager/ListViewDialog.h"
#include "../FileManager/OverwriteDialogRes.h"
#include "../FileManager/ProgressDialog2.h"
#include "../FileManager/ProgressDialog2Res.h"
#include "../FileManager/PropertyNameRes.h"
#include "../FileManager/resourceGui.h"

#include "HashGUI.h"

using namespace NWindows;



class CHashCallbackGUI: public CProgressThreadVirt, public IHashCallbackUI
{
  UInt64 NumFiles;
  bool _curIsFolder;
  UString FirstFileName;
  // UString MainPath;

  CPropNameValPairs PropNameValPairs;

  HRESULT ProcessVirt();
  virtual void ProcessWasFinished_GuiVirt();

public:
  const NWildcard::CCensor *censor;
  const CHashOptions *options;

  DECL_EXTERNAL_CODECS_LOC_VARS2;

  CHashCallbackGUI() {}
  ~CHashCallbackGUI() { }

  INTERFACE_IHashCallbackUI(;)

  void AddErrorMessage(DWORD systemError, const wchar_t *name)
  {
    Sync.AddError_Code_Name(systemError, name);
  }
};


void AddValuePair(CPropNameValPairs &pairs, UINT resourceID, UInt64 value)
{
  CProperty &pair = pairs.AddNew();
  AddLangString(pair.Name, resourceID);
  char sz[32];
  ConvertUInt64ToString(value, sz);
  pair.Value = sz;
}


void AddSizeValue(UString &s, UInt64 value)
{
  {
    wchar_t sz[32];
    ConvertUInt64ToString(value, sz);
    s += MyFormatNew(IDS_FILE_SIZE, sz);
  }
  if (value >= (1 << 10))
  {
    char c;
          if (value >= ((UInt64)10 << 30)) { value >>= 30; c = 'G'; }
    else  if (value >=         (10 << 20)) { value >>= 20; c = 'M'; }
    else                                   { value >>= 10; c = 'K'; }
    char sz[32];
    ConvertUInt64ToString(value, sz);
    s += " (";
    s += sz;
    s += " ";
    s += (wchar_t)c;
    s += "iB)";
  }
}

void AddSizeValuePair(CPropNameValPairs &pairs, UINT resourceID, UInt64 value)
{
  CProperty &pair = pairs.AddNew();
  LangString(resourceID, pair.Name);
  AddSizeValue(pair.Value, value);
}


HRESULT CHashCallbackGUI::StartScanning()
{
  CProgressSync &sync = Sync;
  sync.Set_Status(LangString(IDS_SCANNING));
  return CheckBreak();
}

HRESULT CHashCallbackGUI::ScanProgress(const CDirItemsStat &st, const FString &path, bool isDir)
{
  return Sync.ScanProgress(st.NumFiles, st.GetTotalBytes(), path, isDir);
}

HRESULT CHashCallbackGUI::ScanError(const FString &path, DWORD systemError)
{
  AddErrorMessage(systemError, fs2us(path));
  return CheckBreak();
}

HRESULT CHashCallbackGUI::FinishScanning(const CDirItemsStat &st)
{
  return ScanProgress(st, FString(), false);
}

HRESULT CHashCallbackGUI::CheckBreak()
{
  return Sync.CheckStop();
}

HRESULT CHashCallbackGUI::SetNumFiles(UInt64 numFiles)
{
  CProgressSync &sync = Sync;
  sync.Set_NumFilesTotal(numFiles);
  return CheckBreak();
}

HRESULT CHashCallbackGUI::SetTotal(UInt64 size)
{
  CProgressSync &sync = Sync;
  sync.Set_NumBytesTotal(size);
  return CheckBreak();
}

HRESULT CHashCallbackGUI::SetCompleted(const UInt64 *completed)
{
  return Sync.Set_NumBytesCur(completed);
}

HRESULT CHashCallbackGUI::BeforeFirstFile(const CHashBundle & /* hb */)
{
  return S_OK;
}

HRESULT CHashCallbackGUI::GetStream(const wchar_t *name, bool isFolder)
{
  if (NumFiles == 0)
    FirstFileName = name;
  _curIsFolder = isFolder;
  CProgressSync &sync = Sync;
  sync.Set_FilePath(name, isFolder);
  return CheckBreak();
}

HRESULT CHashCallbackGUI::OpenFileError(const FString &path, DWORD systemError)
{
  // if (systemError == ERROR_SHARING_VIOLATION)
  {
    AddErrorMessage(systemError, fs2us(path));
    return S_FALSE;
  }
  // return systemError;
}

HRESULT CHashCallbackGUI::SetOperationResult(UInt64 /* fileSize */, const CHashBundle & /* hb */, bool /* showHash */)
{
  CProgressSync &sync = Sync;
  if (!_curIsFolder)
    NumFiles++;
  sync.Set_NumFilesCur(NumFiles);
  return CheckBreak();
}

static void AddHashString(CProperty &s, const CHasherState &h, unsigned digestIndex)
{
  char temp[k_HashCalc_DigestSize_Max * 2 + 4];
  AddHashHexToString(temp, h.Digests[digestIndex], h.DigestSize);
  s.Value = temp;
}

static void AddHashResString(CPropNameValPairs &s, const CHasherState &h, unsigned digestIndex, UInt32 resID)
{
  CProperty &pair = s.AddNew();
  UString &s2 = pair.Name;
  LangString(resID, s2);
  UString name (h.Name);
  s2.Replace(L"CRC", name);
  s2.Replace(L":", L"");
  AddHashString(pair, h, digestIndex);
}


void AddHashBundleRes(CPropNameValPairs &s, const CHashBundle &hb)
{
  if (hb.NumErrors != 0)
    AddValuePair(s, IDS_PROP_NUM_ERRORS, hb.NumErrors);

  if (hb.NumFiles == 1 && hb.NumDirs == 0 && !hb.FirstFileName.IsEmpty())
  {
    CProperty &pair = s.AddNew();
    LangString(IDS_PROP_NAME, pair.Name);
    pair.Value = hb.FirstFileName;
  }
  else
  {
    if (!hb.MainName.IsEmpty())
    {
      CProperty &pair = s.AddNew();
      LangString(IDS_PROP_NAME, pair.Name);
      pair.Value = hb.MainName;
    }
    if (hb.NumDirs != 0)
      AddValuePair(s, IDS_PROP_FOLDERS, hb.NumDirs);
    AddValuePair(s, IDS_PROP_FILES, hb.NumFiles);
  }

  AddSizeValuePair(s, IDS_PROP_SIZE, hb.FilesSize);

  if (hb.NumAltStreams != 0)
  {
    AddValuePair(s, IDS_PROP_NUM_ALT_STREAMS, hb.NumAltStreams);
    AddSizeValuePair(s, IDS_PROP_ALT_STREAMS_SIZE, hb.AltStreamsSize);
  }

  FOR_VECTOR (i, hb.Hashers)
  {
    const CHasherState &h = hb.Hashers[i];
    if (hb.NumFiles == 1 && hb.NumDirs == 0)
    {
      CProperty &pair = s.AddNew();
      pair.Name += h.Name;
      AddHashString(pair, h, k_HashCalc_Index_DataSum);
    }
    else
    {
      AddHashResString(s, h, k_HashCalc_Index_DataSum, IDS_CHECKSUM_CRC_DATA);
      AddHashResString(s, h, k_HashCalc_Index_NamesSum, IDS_CHECKSUM_CRC_DATA_NAMES);
    }
    if (hb.NumAltStreams != 0)
    {
      AddHashResString(s, h, k_HashCalc_Index_StreamsSum, IDS_CHECKSUM_CRC_STREAMS_NAMES);
    }
  }
}


void AddHashBundleRes(UString &s, const CHashBundle &hb)
{
  CPropNameValPairs pairs;
  AddHashBundleRes(pairs, hb);
  
  FOR_VECTOR (i, pairs)
  {
    const CProperty &pair = pairs[i];
    s += pair.Name;
    s += ": ";
    s += pair.Value;
    s.Add_LF();
  }

  if (hb.NumErrors == 0 && hb.Hashers.IsEmpty())
  {
    s.Add_LF();
    AddLangString(s, IDS_MESSAGE_NO_ERRORS);
    s.Add_LF();
  }
}


HRESULT CHashCallbackGUI::AfterLastFile(CHashBundle &hb)
{
  hb.FirstFileName = FirstFileName;
  // MainPath
  AddHashBundleRes(PropNameValPairs, hb);
  
  CProgressSync &sync = Sync;
  sync.Set_NumFilesCur(hb.NumFiles);

  // CProgressMessageBoxPair &pair = GetMessagePair(hb.NumErrors != 0);
  // pair.Message = s;
  // LangString(IDS_CHECKSUM_INFORMATION, pair.Title);

  return S_OK;
}


HRESULT CHashCallbackGUI::ProcessVirt()
{
  NumFiles = 0;
  AString errorInfo;
  HRESULT res = HashCalc(EXTERNAL_CODECS_LOC_VARS
      *censor, *options, errorInfo, this);
  return res;
}


HRESULT HashCalcGUI(
    DECL_EXTERNAL_CODECS_LOC_VARS
    const NWildcard::CCensor &censor,
    const CHashOptions &options,
    bool &messageWasDisplayed)
{
  CHashCallbackGUI t;
  #ifdef EXTERNAL_CODECS
  t.__externalCodecs = __externalCodecs;
  #endif
  t.censor = &censor;
  t.options = &options;

  t.ShowCompressionInfo = false;

  const UString title = LangString(IDS_CHECKSUM_CALCULATING);

  t.MainTitle = "7-Zip"; // LangString(IDS_APP_TITLE);
  t.MainAddTitle = title;
  t.MainAddTitle.Add_Space();

  RINOK(t.Create(title));
  messageWasDisplayed = t.ThreadFinishedOK && t.MessagesDisplayed;
  return S_OK;
}


void ShowHashResults(const CPropNameValPairs &propPairs, HWND hwnd)
{
  CListViewDialog lv;
  
  FOR_VECTOR (i, propPairs)
  {
    const CProperty &pair = propPairs[i];
    lv.Strings.Add(pair.Name);
    lv.Values.Add(pair.Value);
  }
  
  lv.Title = LangString(IDS_CHECKSUM_INFORMATION);
  lv.DeleteIsAllowed = true;
  lv.SelectFirst = false;
  lv.NumColumns = 2;
  
  lv.Create(hwnd);
}


void ShowHashResults(const CHashBundle &hb, HWND hwnd)
{
  CPropNameValPairs propPairs;
  AddHashBundleRes(propPairs, hb);
  ShowHashResults(propPairs, hwnd);
}


void CHashCallbackGUI::ProcessWasFinished_GuiVirt()
{
  ShowHashResults(PropNameValPairs, *this);
}

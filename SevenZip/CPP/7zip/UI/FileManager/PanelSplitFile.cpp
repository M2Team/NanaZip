// PanelSplitFile.cpp

#include "StdAfx.h"

#include "../../../Common/IntToString.h"

#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileName.h"

#include "../GUI/ExtractRes.h"

#include "resource.h"

#include "App.h"
#include "CopyDialog.h"
#include "FormatUtils.h"
#include "LangUtils.h"
#include "SplitDialog.h"
#include "SplitUtils.h"

#include "PropertyNameRes.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;

static const char * const g_Message_FileWriteError = "File write error";

struct CVolSeqName
{
  UString UnchangedPart;
  UString ChangedPart;
  CVolSeqName(): ChangedPart("000") {};

  void SetNumDigits(UInt64 numVolumes)
  {
    ChangedPart = "000";
    while (numVolumes > 999)
    {
      numVolumes /= 10;
      ChangedPart += '0';
    }
  }

  bool ParseName(const UString &name)
  {
    if (name.Len() < 2)
      return false;
    if (name.Back() != L'1' || name[name.Len() - 2] != L'0')
      return false;

    unsigned pos = name.Len() - 2;
    for (; pos > 0 && name[pos - 1] == '0'; pos--);
    UnchangedPart.SetFrom(name, pos);
    ChangedPart = name.Ptr(pos);
    return true;
  }

  UString GetNextName();
};


UString CVolSeqName::GetNextName()
{
  for (int i = (int)ChangedPart.Len() - 1; i >= 0; i--)
  {
    wchar_t c = ChangedPart[i];
    if (c != L'9')
    {
      ChangedPart.ReplaceOneCharAtPos(i, (wchar_t)(c + 1));
      break;
    }
    ChangedPart.ReplaceOneCharAtPos(i, L'0');
    if (i == 0)
      ChangedPart.InsertAtFront(L'1');
  }
  return UnchangedPart + ChangedPart;
}

class CThreadSplit: public CProgressThreadVirt
{
  HRESULT ProcessVirt();
public:
  FString FilePath;
  FString VolBasePath;
  UInt64 NumVolumes;
  CRecordVector<UInt64> VolumeSizes;
};


class CPreAllocOutFile
{
  UInt64 _preAllocSize;
public:
  NIO::COutFile File;
  UInt64 Written;
  
  CPreAllocOutFile(): _preAllocSize(0), Written(0) {}
  
  ~CPreAllocOutFile()
  {
    SetCorrectFileLength();
  }

  void PreAlloc(UInt64 preAllocSize)
  {
    _preAllocSize = 0;
    if (File.SetLength(preAllocSize))
      _preAllocSize = preAllocSize;
    File.SeekToBegin();
  }

  bool Write(const void *data, UInt32 size, UInt32 &processedSize) throw()
  {
    bool res = File.Write(data, size, processedSize);
    Written += processedSize;
    return res;
  }

  void Close()
  {
    SetCorrectFileLength();
    Written = 0;
    _preAllocSize = 0;
    File.Close();
  }

  void SetCorrectFileLength()
  {
    if (Written < _preAllocSize)
    {
      File.SetLength(Written);
      _preAllocSize = 0;
    }
  }
};


static const UInt32 kBufSize = (1 << 20);

HRESULT CThreadSplit::ProcessVirt()
{
  NIO::CInFile inFile;
  if (!inFile.Open(FilePath))
    return GetLastError();

  CPreAllocOutFile outFile;
  
  CMyBuffer buffer;
  if (!buffer.Allocate(kBufSize))
    return E_OUTOFMEMORY;
  
  CVolSeqName seqName;
  seqName.SetNumDigits(NumVolumes);
  
  UInt64 length;
  if (!inFile.GetLength(length))
    return GetLastError();
  
  CProgressSync &sync = Sync;
  sync.Set_NumBytesTotal(length);
  
  UInt64 pos = 0;
  UInt64 prev = 0;
  UInt64 numFiles = 0;
  unsigned volIndex = 0;

  for (;;)
  {
    UInt64 volSize;
    if (volIndex < VolumeSizes.Size())
      volSize = VolumeSizes[volIndex];
    else
      volSize = VolumeSizes.Back();
    
    UInt32 needSize = kBufSize;
    {
      const UInt64 rem = volSize - outFile.Written;
      if (needSize > rem)
        needSize = (UInt32)rem;
    }
    UInt32 processedSize;
    if (!inFile.Read(buffer, needSize, processedSize))
      return GetLastError();
    if (processedSize == 0)
      return S_OK;
    needSize = processedSize;
  
    if (outFile.Written == 0)
    {
      FString name = VolBasePath;
      name += '.';
      name += us2fs(seqName.GetNextName());
      sync.Set_FilePath(fs2us(name));
      if (!outFile.File.Create(name, false))
      {
        HRESULT res = GetLastError();
        AddErrorPath(name);
        return res;
      }
      UInt64 expectSize = volSize;
      if (pos < length)
      {
        const UInt64 rem = length - pos;
        if (expectSize > rem)
          expectSize = rem;
      }
      outFile.PreAlloc(expectSize);
    }
    
    if (!outFile.Write(buffer, needSize, processedSize))
      return GetLastError();
    if (needSize != processedSize)
      throw g_Message_FileWriteError;
    
    pos += processedSize;
    
    if (outFile.Written == volSize)
    {
      outFile.Close();
      sync.Set_NumFilesCur(++numFiles);
      if (volIndex < VolumeSizes.Size())
        volIndex++;
    }

    if (pos - prev >= ((UInt32)1 << 22) || outFile.Written == 0)
    {
      RINOK(sync.Set_NumBytesCur(pos));
      prev = pos;
    }
  }
}


void CApp::Split()
{
  int srcPanelIndex = GetFocusedPanelIndex();
  CPanel &srcPanel = Panels[srcPanelIndex];
  if (!srcPanel.Is_IO_FS_Folder())
  {
    srcPanel.MessageBox_Error_UnsupportOperation();
    return;
  }
  CRecordVector<UInt32> indices;
  srcPanel.GetOperatedItemIndices(indices);
  if (indices.IsEmpty())
    return;
  if (indices.Size() != 1)
  {
    srcPanel.MessageBox_Error_LangID(IDS_SELECT_ONE_FILE);
    return;
  }
  int index = indices[0];
  if (srcPanel.IsItem_Folder(index))
  {
    srcPanel.MessageBox_Error_LangID(IDS_SELECT_ONE_FILE);
    return;
  }
  const UString itemName = srcPanel.GetItemName(index);

  UString srcPath = srcPanel.GetFsPath() + srcPanel.GetItemPrefix(index);
  UString path = srcPath;
  unsigned destPanelIndex = (NumPanels <= 1) ? srcPanelIndex : (1 - srcPanelIndex);
  CPanel &destPanel = Panels[destPanelIndex];
  if (NumPanels > 1)
    if (destPanel.IsFSFolder())
      path = destPanel.GetFsPath();
  CSplitDialog splitDialog;
  splitDialog.FilePath = srcPanel.GetItemRelPath(index);
  splitDialog.Path = path;
  if (splitDialog.Create(srcPanel.GetParent()) != IDOK)
    return;

  NFind::CFileInfo fileInfo;
  if (!fileInfo.Find(us2fs(srcPath + itemName)))
  {
    srcPanel.MessageBox_Error(L"Cannot find file");
    return;
  }
  if (fileInfo.Size <= splitDialog.VolumeSizes.Front())
  {
    srcPanel.MessageBox_Error_LangID(IDS_SPLIT_VOL_MUST_BE_SMALLER);
    return;
  }
  const UInt64 numVolumes = GetNumberOfVolumes(fileInfo.Size, splitDialog.VolumeSizes);
  if (numVolumes >= 100)
  {
    wchar_t s[32];
    ConvertUInt64ToString(numVolumes, s);
    if (::MessageBoxW(srcPanel, MyFormatNew(IDS_SPLIT_CONFIRM_MESSAGE, s),
        LangString(IDS_SPLIT_CONFIRM_TITLE),
        MB_YESNOCANCEL | MB_ICONQUESTION) != IDYES)
      return;
  }

  path = splitDialog.Path;
  NName::NormalizeDirPathPrefix(path);
  if (!CreateComplexDir(us2fs(path)))
  {
    DWORD lastError = ::GetLastError();
    srcPanel.MessageBox_Error_2Lines_Message_HRESULT(MyFormatNew(IDS_CANNOT_CREATE_FOLDER, path), lastError);
    return;
  }

  {
  CThreadSplit spliter;
  spliter.NumVolumes = numVolumes;

  CProgressDialog &progressDialog = spliter;

  UString progressWindowTitle ("7-Zip"); // LangString(IDS_APP_TITLE, 0x03000000);
  UString title = LangString(IDS_SPLITTING);

  progressDialog.ShowCompressionInfo = false;

  progressDialog.MainWindow = _window;
  progressDialog.MainTitle = progressWindowTitle;
  progressDialog.MainAddTitle = title;
  progressDialog.MainAddTitle.Add_Space();
  progressDialog.Sync.Set_TitleFileName(itemName);


  spliter.FilePath = us2fs(srcPath + itemName);
  spliter.VolBasePath = us2fs(path + srcPanel.GetItemName_for_Copy(index));
  spliter.VolumeSizes = splitDialog.VolumeSizes;
  
  // if (splitDialog.VolumeSizes.Size() == 0) return;

  // CPanel::CDisableTimerProcessing disableTimerProcessing1(srcPanel);
  // CPanel::CDisableTimerProcessing disableTimerProcessing2(destPanel);

  if (spliter.Create(title, _window) != 0)
    return;
  }
  RefreshTitleAlways();


  // disableNotify.Restore();
  // disableNotify.Restore();
  // srcPanel.SetFocusToList();
  // srcPanel.RefreshListCtrlSaveFocused();
}


class CThreadCombine: public CProgressThreadVirt
{
  HRESULT ProcessVirt();
public:
  FString InputDirPrefix;
  FStringVector Names;
  FString OutputPath;
  UInt64 TotalSize;
};

HRESULT CThreadCombine::ProcessVirt()
{
  NIO::COutFile outFile;
  if (!outFile.Create(OutputPath, false))
  {
    HRESULT res = GetLastError();
    AddErrorPath(OutputPath);
    return res;
  }
  
  CProgressSync &sync = Sync;
  sync.Set_NumBytesTotal(TotalSize);
  
  CMyBuffer bufferObject;
  if (!bufferObject.Allocate(kBufSize))
    return E_OUTOFMEMORY;
  Byte *buffer = (Byte *)(void *)bufferObject;
  UInt64 pos = 0;
  FOR_VECTOR (i, Names)
  {
    NIO::CInFile inFile;
    const FString nextName = InputDirPrefix + Names[i];
    if (!inFile.Open(nextName))
    {
      HRESULT res = GetLastError();
      AddErrorPath(nextName);
      return res;
    }
    sync.Set_FilePath(fs2us(nextName));
    for (;;)
    {
      UInt32 processedSize;
      if (!inFile.Read(buffer, kBufSize, processedSize))
      {
        HRESULT res = GetLastError();
        AddErrorPath(nextName);
        return res;
      }
      if (processedSize == 0)
        break;
      UInt32 needSize = processedSize;
      if (!outFile.Write(buffer, needSize, processedSize))
      {
        HRESULT res = GetLastError();
        AddErrorPath(OutputPath);
        return res;
      }
      if (needSize != processedSize)
        throw g_Message_FileWriteError;
      pos += processedSize;
      RINOK(sync.Set_NumBytesCur(pos));
    }
  }
  return S_OK;
}

extern void AddValuePair2(UString &s, UINT resourceID, UInt64 num, UInt64 size);

static void AddInfoFileName(UString &dest, const UString &name)
{
  dest += "\n  ";
  dest += name;
}

void CApp::Combine()
{
  int srcPanelIndex = GetFocusedPanelIndex();
  CPanel &srcPanel = Panels[srcPanelIndex];
  if (!srcPanel.IsFSFolder())
  {
    srcPanel.MessageBox_Error_LangID(IDS_OPERATION_IS_NOT_SUPPORTED);
    return;
  }
  CRecordVector<UInt32> indices;
  srcPanel.GetOperatedItemIndices(indices);
  if (indices.IsEmpty())
    return;
  int index = indices[0];
  if (indices.Size() != 1 || srcPanel.IsItem_Folder(index))
  {
    srcPanel.MessageBox_Error_LangID(IDS_COMBINE_SELECT_ONE_FILE);
    return;
  }
  const UString itemName = srcPanel.GetItemName(index);

  UString srcPath = srcPanel.GetFsPath() + srcPanel.GetItemPrefix(index);
  UString path = srcPath;
  unsigned destPanelIndex = (NumPanels <= 1) ? srcPanelIndex : (1 - srcPanelIndex);
  CPanel &destPanel = Panels[destPanelIndex];
  if (NumPanels > 1)
    if (destPanel.IsFSFolder())
      path = destPanel.GetFsPath();

  CVolSeqName volSeqName;
  if (!volSeqName.ParseName(itemName))
  {
    srcPanel.MessageBox_Error_LangID(IDS_COMBINE_CANT_DETECT_SPLIT_FILE);
    return;
  }
  
  {
  CThreadCombine combiner;
  
  UString nextName = itemName;
  combiner.TotalSize = 0;
  for (;;)
  {
    NFind::CFileInfo fileInfo;
    if (!fileInfo.Find(us2fs(srcPath + nextName)) || fileInfo.IsDir())
      break;
    combiner.Names.Add(us2fs(nextName));
    combiner.TotalSize += fileInfo.Size;
    nextName = volSeqName.GetNextName();
  }
  if (combiner.Names.Size() == 1)
  {
    srcPanel.MessageBox_Error_LangID(IDS_COMBINE_CANT_FIND_MORE_THAN_ONE_PART);
    return;
  }
  
  if (combiner.TotalSize == 0)
  {
    srcPanel.MessageBox_Error(L"No data");
    return;
  }
  
  UString info;
  AddValuePair2(info, IDS_PROP_FILES, combiner.Names.Size(), combiner.TotalSize);
  
  info.Add_LF();
  info += srcPath;
  
  unsigned i;
  for (i = 0; i < combiner.Names.Size() && i < 2; i++)
    AddInfoFileName(info, fs2us(combiner.Names[i]));
  if (i != combiner.Names.Size())
  {
    if (i + 1 != combiner.Names.Size())
      AddInfoFileName(info, L"...");
    AddInfoFileName(info, fs2us(combiner.Names.Back()));
  }
  
  {
    CCopyDialog copyDialog;
    copyDialog.Value = path;
    LangString(IDS_COMBINE, copyDialog.Title);
    copyDialog.Title.Add_Space();
    copyDialog.Title += srcPanel.GetItemRelPath(index);
    LangString(IDS_COMBINE_TO, copyDialog.Static);
    copyDialog.Info = info;
    if (copyDialog.Create(srcPanel.GetParent()) != IDOK)
      return;
    path = copyDialog.Value;
  }

  NName::NormalizeDirPathPrefix(path);
  if (!CreateComplexDir(us2fs(path)))
  {
    DWORD lastError = ::GetLastError();
    srcPanel.MessageBox_Error_2Lines_Message_HRESULT(MyFormatNew(IDS_CANNOT_CREATE_FOLDER, path), lastError);
    return;
  }
  
  UString outName = volSeqName.UnchangedPart;
  while (!outName.IsEmpty())
  {
    if (outName.Back() != L'.')
      break;
    outName.DeleteBack();
  }
  if (outName.IsEmpty())
    outName = "file";
  
  NFind::CFileInfo fileInfo;
  UString destFilePath = path + outName;
  combiner.OutputPath = us2fs(destFilePath);
  if (fileInfo.Find(combiner.OutputPath))
  {
    srcPanel.MessageBox_Error(MyFormatNew(IDS_FILE_EXIST, destFilePath));
    return;
  }
  
    CProgressDialog &progressDialog = combiner;
    progressDialog.ShowCompressionInfo = false;
  
    UString progressWindowTitle ("7-Zip"); // LangString(IDS_APP_TITLE, 0x03000000);
    UString title = LangString(IDS_COMBINING);
    
    progressDialog.MainWindow = _window;
    progressDialog.MainTitle = progressWindowTitle;
    progressDialog.MainAddTitle = title;
    progressDialog.MainAddTitle.Add_Space();
    
    combiner.InputDirPrefix = us2fs(srcPath);
    
    // CPanel::CDisableTimerProcessing disableTimerProcessing1(srcPanel);
    // CPanel::CDisableTimerProcessing disableTimerProcessing2(destPanel);
    
    if (combiner.Create(title, _window) != 0)
      return;
  }
  RefreshTitleAlways();

  // disableNotify.Restore();
  // disableNotify.Restore();
  // srcPanel.SetFocusToList();
  // srcPanel.RefreshListCtrlSaveFocused();
}

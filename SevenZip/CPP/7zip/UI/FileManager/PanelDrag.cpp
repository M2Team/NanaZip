// PanelDrag.cpp

#include "StdAfx.h"

#ifdef UNDER_CE
#include <winuserm.h>
#endif

#include "../../../Common/StringConvert.h"
#include "../../../Common/Wildcard.h"

#include "../../../Windows/MemoryGlobal.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/Shell.h"

#include "../Common/ArchiveName.h"
#include "../Common/CompressCall.h"
#include "../Common/ExtractingFilePath.h"

#include "MessagesDialog.h"

#include "App.h"
#include "EnumFormatEtc.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;

#ifndef _UNICODE
extern bool g_IsNT;
#endif

#define kTempDirPrefix FTEXT("7zE")

static LPCTSTR const kSvenZipSetFolderFormat = TEXT("7-Zip::SetTargetFolder");

////////////////////////////////////////////////////////

class CDataObject:
  public IDataObject,
  public CMyUnknownImp
{
private:
  FORMATETC m_Etc;
  UINT m_SetFolderFormat;

public:
  MY_UNKNOWN_IMP1_MT(IDataObject)

  STDMETHODIMP GetData(LPFORMATETC pformatetcIn, LPSTGMEDIUM medium);
  STDMETHODIMP GetDataHere(LPFORMATETC pformatetc, LPSTGMEDIUM medium);
  STDMETHODIMP QueryGetData(LPFORMATETC pformatetc );

  STDMETHODIMP GetCanonicalFormatEtc ( LPFORMATETC /* pformatetc */, LPFORMATETC pformatetcOut)
    { pformatetcOut->ptd = NULL; return ResultFromScode(E_NOTIMPL); }

  STDMETHODIMP SetData(LPFORMATETC etc, STGMEDIUM *medium, BOOL release);
  STDMETHODIMP EnumFormatEtc(DWORD drection, LPENUMFORMATETC *enumFormatEtc);
  
  STDMETHODIMP DAdvise(FORMATETC * /* etc */, DWORD /* advf */, LPADVISESINK /* pAdvSink */, DWORD * /* pdwConnection */)
    { return OLE_E_ADVISENOTSUPPORTED; }
  STDMETHODIMP DUnadvise(DWORD /* dwConnection */) { return OLE_E_ADVISENOTSUPPORTED; }
  STDMETHODIMP EnumDAdvise( LPENUMSTATDATA * /* ppenumAdvise */) { return OLE_E_ADVISENOTSUPPORTED; }

  CDataObject();

  NMemory::CGlobal hGlobal;
  UString Path;
};

CDataObject::CDataObject()
{
  m_SetFolderFormat = RegisterClipboardFormat(kSvenZipSetFolderFormat);
  m_Etc.cfFormat = CF_HDROP;
  m_Etc.ptd = NULL;
  m_Etc.dwAspect = DVASPECT_CONTENT;
  m_Etc.lindex = -1;
  m_Etc.tymed = TYMED_HGLOBAL;
}

STDMETHODIMP CDataObject::SetData(LPFORMATETC etc, STGMEDIUM *medium, BOOL /* release */)
{
  if (etc->cfFormat == m_SetFolderFormat
      && etc->tymed == TYMED_HGLOBAL
      && etc->dwAspect == DVASPECT_CONTENT
      && medium->tymed == TYMED_HGLOBAL)
  {
    Path.Empty();
    if (!medium->hGlobal)
      return S_OK;
    size_t size = GlobalSize(medium->hGlobal) / sizeof(wchar_t);
    const wchar_t *src = (const wchar_t *)GlobalLock(medium->hGlobal);
    if (src)
    {
      for (size_t i = 0; i < size; i++)
      {
        wchar_t c = src[i];
        if (c == 0)
          break;
        Path += c;
      }
      GlobalUnlock(medium->hGlobal);
      return S_OK;
    }
  }
  return E_NOTIMPL;
}

static HGLOBAL DuplicateGlobalMem(HGLOBAL srcGlobal)
{
  SIZE_T size = GlobalSize(srcGlobal);
  const void *src = GlobalLock(srcGlobal);
  if (!src)
    return 0;
  HGLOBAL destGlobal = GlobalAlloc(GHND | GMEM_SHARE, size);
  if (destGlobal)
  {
    void *dest = GlobalLock(destGlobal);
    if (!dest)
    {
      GlobalFree(destGlobal);
      destGlobal = 0;
    }
    else
    {
      memcpy(dest, src, size);
      GlobalUnlock(destGlobal);
    }
  }
  GlobalUnlock(srcGlobal);
  return destGlobal;
}

STDMETHODIMP CDataObject::GetData(LPFORMATETC etc, LPSTGMEDIUM medium)
{
  RINOK(QueryGetData(etc));
  medium->tymed = m_Etc.tymed;
  medium->pUnkForRelease = 0;
  medium->hGlobal = DuplicateGlobalMem(hGlobal);
  if (!medium->hGlobal)
    return E_OUTOFMEMORY;
  return S_OK;
}

STDMETHODIMP CDataObject::GetDataHere(LPFORMATETC /* etc */, LPSTGMEDIUM /* medium */)
{
  // Seems Windows doesn't call it, so we will not implement it.
  return E_UNEXPECTED;
}


STDMETHODIMP CDataObject::QueryGetData(LPFORMATETC etc)
{
  if ((m_Etc.tymed & etc->tymed) &&
       m_Etc.cfFormat == etc->cfFormat &&
       m_Etc.dwAspect == etc->dwAspect)
    return S_OK;
  return DV_E_FORMATETC;
}

STDMETHODIMP CDataObject::EnumFormatEtc(DWORD direction, LPENUMFORMATETC FAR* enumFormatEtc)
{
  if (direction != DATADIR_GET)
    return E_NOTIMPL;
  return CreateEnumFormatEtc(1, &m_Etc, enumFormatEtc);
}

////////////////////////////////////////////////////////

class CDropSource:
  public IDropSource,
  public CMyUnknownImp
{
  DWORD m_Effect;
public:
  MY_UNKNOWN_IMP1_MT(IDropSource)
  STDMETHOD(QueryContinueDrag)(BOOL escapePressed, DWORD keyState);
  STDMETHOD(GiveFeedback)(DWORD effect);


  bool NeedExtract;
  CPanel *Panel;
  CRecordVector<UInt32> Indices;
  UString Folder;
  CDataObject *DataObjectSpec;
  CMyComPtr<IDataObject> DataObject;
  
  bool NeedPostCopy;
  HRESULT Result;
  UStringVector Messages;

  CDropSource(): m_Effect(DROPEFFECT_NONE), Panel(NULL), NeedPostCopy(false), Result(S_OK) {}
};

STDMETHODIMP CDropSource::QueryContinueDrag(BOOL escapePressed, DWORD keyState)
{
  if (escapePressed == TRUE)
    return DRAGDROP_S_CANCEL;
  if ((keyState & MK_LBUTTON) == 0)
  {
    if (m_Effect == DROPEFFECT_NONE)
      return DRAGDROP_S_CANCEL;
    Result = S_OK;
    bool needExtract = NeedExtract;
    // MoveMode = (((keyState & MK_SHIFT) != 0) && MoveIsAllowed);
    if (!DataObjectSpec->Path.IsEmpty())
    {
      needExtract = false;
      NeedPostCopy = true;
    }
    if (needExtract)
    {
      CCopyToOptions options;
      options.folder = Folder;

      // 15.13: fixed problem with mouse cursor for password window.
      // DoDragDrop() probably calls SetCapture() to some hidden window.
      // But it's problem, if we show some modal window, like MessageBox.
      // So we return capture to our window.
      // If you know better way to solve the problem, please notify 7-Zip developer.
      
      // MessageBoxW(*Panel, L"test", L"test", 0);

      /* HWND oldHwnd = */ SetCapture(*Panel);

      Result = Panel->CopyTo(options, Indices, &Messages);

      // do we need to restore capture?
      // ReleaseCapture();
      // oldHwnd = SetCapture(oldHwnd);

      if (Result != S_OK || !Messages.IsEmpty())
        return DRAGDROP_S_CANCEL;
    }
    return DRAGDROP_S_DROP;
  }
  return S_OK;
}

STDMETHODIMP CDropSource::GiveFeedback(DWORD effect)
{
  m_Effect = effect;
  return DRAGDROP_S_USEDEFAULTCURSORS;
}

static bool CopyNamesToHGlobal(NMemory::CGlobal &hgDrop, const UStringVector &names)
{
  size_t totalLen = 1;

  #ifndef _UNICODE
  if (!g_IsNT)
  {
    AStringVector namesA;
    unsigned i;
    for (i = 0; i < names.Size(); i++)
      namesA.Add(GetSystemString(names[i]));
    for (i = 0; i < names.Size(); i++)
      totalLen += namesA[i].Len() + 1;
    
    if (!hgDrop.Alloc(GHND | GMEM_SHARE, totalLen * sizeof(CHAR) + sizeof(DROPFILES)))
      return false;
    
    NMemory::CGlobalLock dropLock(hgDrop);
    DROPFILES* dropFiles = (DROPFILES*)dropLock.GetPointer();
    if (!dropFiles)
      return false;
    dropFiles->fNC = FALSE;
    dropFiles->pt.x = 0;
    dropFiles->pt.y = 0;
    dropFiles->pFiles = sizeof(DROPFILES);
    dropFiles->fWide = FALSE;
    CHAR *p = (CHAR *)((BYTE *)dropFiles + sizeof(DROPFILES));
    for (i = 0; i < names.Size(); i++)
    {
      const AString &s = namesA[i];
      unsigned fullLen = s.Len() + 1;
      MyStringCopy(p, (const char *)s);
      p += fullLen;
      totalLen -= fullLen;
    }
    *p = 0;
  }
  else
  #endif
  {
    unsigned i;
    for (i = 0; i < names.Size(); i++)
      totalLen += names[i].Len() + 1;
    
    if (!hgDrop.Alloc(GHND | GMEM_SHARE, totalLen * sizeof(WCHAR) + sizeof(DROPFILES)))
      return false;
    
    NMemory::CGlobalLock dropLock(hgDrop);
    DROPFILES* dropFiles = (DROPFILES*)dropLock.GetPointer();
    if (!dropFiles)
      return false;
    dropFiles->fNC = FALSE;
    dropFiles->pt.x = 0;
    dropFiles->pt.y = 0;
    dropFiles->pFiles = sizeof(DROPFILES);
    dropFiles->fWide = TRUE;
    WCHAR *p = (WCHAR *) (void *) ((BYTE *)dropFiles + sizeof(DROPFILES));
    for (i = 0; i < names.Size(); i++)
    {
      const UString &s = names[i];
      unsigned fullLen = s.Len() + 1;
      MyStringCopy(p, (const WCHAR *)s);
      p += fullLen;
      totalLen -= fullLen;
    }
    *p = 0;
  }
  return true;
}

void CPanel::OnDrag(LPNMLISTVIEW /* nmListView */)
{
  if (!DoesItSupportOperations())
    return;

  CDisableTimerProcessing disableTimerProcessing2(*this);
  
  CRecordVector<UInt32> indices;
  GetOperatedItemIndices(indices);
  if (indices.Size() == 0)
    return;

  // CSelectedState selState;
  // SaveSelectedState(selState);

  // FString dirPrefix2;
  FString dirPrefix;
  CTempDir tempDirectory;

  bool isFSFolder = IsFSFolder();
  if (isFSFolder)
    dirPrefix = us2fs(GetFsPath());
  else
  {
    if (!tempDirectory.Create(kTempDirPrefix))
    {
      MessageBox_Error(L"Can't create temp folder");
      return;
    }
    dirPrefix = tempDirectory.GetPath();
    // dirPrefix2 = dirPrefix;
    NFile::NName::NormalizeDirPathPrefix(dirPrefix);
  }

  CDataObject *dataObjectSpec = new CDataObject;
  CMyComPtr<IDataObject> dataObject = dataObjectSpec;

  {
    UStringVector names;

    // names variable is     USED for drag and drop from 7-zip to Explorer or to 7-zip archive folder.
    // names variable is NOT USED for drag and drop from 7-zip to 7-zip File System folder.

    FOR_VECTOR (i, indices)
    {
      UInt32 index = indices[i];
      UString s;
      if (isFSFolder)
        s = GetItemRelPath(index);
      else
      {
        s = GetItemName(index);
        /*
        // We use (keepAndReplaceEmptyPrefixes = true) in CAgentFolder::Extract
        // So the following code is not required.
        // Maybe we also can change IFolder interface and send some flag also.
  
        if (s.IsEmpty())
        {
          // Correct_FsFile_Name("") returns "_".
          // If extracting code removes empty folder prefixes from path (as it was in old version),
          // Explorer can't find "_" folder in temp folder.
          // We can ask Explorer to copy parent temp folder "7zE" instead.

          names.Clear();
          names.Add(dirPrefix2);
          break;
        }
        */
        s = Get_Correct_FsFile_Name(s);
      }
      names.Add(fs2us(dirPrefix) + s);
    }
    if (!CopyNamesToHGlobal(dataObjectSpec->hGlobal, names))
      return;
  }

  CDropSource *dropSourceSpec = new CDropSource;
  CMyComPtr<IDropSource> dropSource = dropSourceSpec;
  dropSourceSpec->NeedExtract = !isFSFolder;
  dropSourceSpec->Panel = this;
  dropSourceSpec->Indices = indices;
  dropSourceSpec->Folder = fs2us(dirPrefix);
  dropSourceSpec->DataObjectSpec = dataObjectSpec;
  dropSourceSpec->DataObject = dataObjectSpec;

 
  /*
  CTime - file creation timestamp.
  There are two operations in Windows with Drag and Drop:
    COPY_OPERATION - icon with    Plus sign - CTime will be set as current_time.
    MOVE_OPERATION - icon without Plus sign - CTime will be preserved

  Note: if we call DoDragDrop() with (effectsOK = DROPEFFECT_MOVE), then
        it will use MOVE_OPERATION and CTime will be preserved.
        But MoveFile() function doesn't preserve CTime, if different volumes are used.
        Why it's so?
        Does DoDragDrop() use some another function (not MoveFile())?

  if (effectsOK == DROPEFFECT_COPY) it works as COPY_OPERATION
    
  if (effectsOK == DROPEFFECT_MOVE) drag works as MOVE_OPERATION

  if (effectsOK == (DROPEFFECT_COPY | DROPEFFECT_MOVE))
  {
    if we drag file to same volume, then Windows suggests:
       CTRL      - COPY_OPERATION
       [default] - MOVE_OPERATION
    
    if we drag file to another volume, then Windows suggests
       [default] - COPY_OPERATION
       SHIFT     - MOVE_OPERATION
  }

  We want to use MOVE_OPERATION for extracting from archive (open in 7-Zip) to Explorer:
  It has the following advantages:
    1) it uses fast MOVE_OPERATION instead of slow COPY_OPERATION and DELETE, if same volume.
    2) it preserved CTime

  Some another programs support only COPY_OPERATION.
  So we can use (DROPEFFECT_COPY | DROPEFFECT_MOVE)

  Also another program can return from DoDragDrop() before
  files using. But we delete temp folder after DoDragDrop(),
  and another program can't open input files in that case.

  We create objects:
    IDropSource *dropSource
    IDataObject *dataObject
  if DropTarget is 7-Zip window, then 7-Zip's
    IDropTarget::DragOver() sets Path in IDataObject.
  and
    IDropSource::QueryContinueDrag() sets NeedPostCopy, if Path is not epmty.
  So we can detect destination path after DoDragDrop().
  Now we don't know any good way to detect destination path for D&D to Explorer.
  */

  bool moveIsAllowed = isFSFolder;
  /*
  DWORD effectsOK = DROPEFFECT_COPY;
  if (moveIsAllowed)
    effectsOK |= DROPEFFECT_MOVE;
  */

  // 18.04: was changed
  DWORD effectsOK = DROPEFFECT_MOVE | DROPEFFECT_COPY;

  DWORD effect;
  _panelCallback->DragBegin();
  
  HRESULT res = DoDragDrop(dataObject, dropSource, effectsOK, &effect);
  
  _panelCallback->DragEnd();
  bool canceled = (res == DRAGDROP_S_CANCEL);
  
  CDisableNotify disableNotify(*this);
  
  if (res == DRAGDROP_S_DROP)
  {
    res = dropSourceSpec->Result;
    if (dropSourceSpec->NeedPostCopy)
      if (!dataObjectSpec->Path.IsEmpty())
      {
        NFile::NName::NormalizeDirPathPrefix(dataObjectSpec->Path);
        CCopyToOptions options;
        options.folder = dataObjectSpec->Path;
        // if MOVE is not allowed, we just use COPY operation
        options.moveMode = (effect == DROPEFFECT_MOVE && moveIsAllowed);
        res = CopyTo(options, indices, &dropSourceSpec->Messages);
      }
    /*
    if (effect == DROPEFFECT_MOVE)
      RefreshListCtrl(selState);
    */
  }
  else
  {
    // we ignore E_UNEXPECTED that is returned if we drag file to printer
    if (res != DRAGDROP_S_CANCEL && res != S_OK
        && res != E_UNEXPECTED)
      MessageBox_Error_HRESULT(res);

    res = dropSourceSpec->Result;
  }

  if (!dropSourceSpec->Messages.IsEmpty())
  {
    CMessagesDialog messagesDialog;
    messagesDialog.Messages = &dropSourceSpec->Messages;
    messagesDialog.Create((*this));
  }
  
  if (res != S_OK && res != E_ABORT)
  {
    // we restore Notify before MessageBox_Error_HRESULT. So we will se files selection
    disableNotify.Restore();
    // SetFocusToList();
    MessageBox_Error_HRESULT(res);
  }
  if (res == S_OK && dropSourceSpec->Messages.IsEmpty() && !canceled)
    KillSelection();
}

void CDropTarget::QueryGetData(IDataObject *dataObject)
{
  FORMATETC etc = { CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
  m_DropIsAllowed = (dataObject->QueryGetData(&etc) == S_OK);

}

static void MySetDropHighlighted(HWND hWnd, int index, bool enable)
{
  LVITEM item;
  item.mask = LVIF_STATE;
  item.iItem = index;
  item.iSubItem = 0;
  item.state = enable ? LVIS_DROPHILITED : 0;
  item.stateMask = LVIS_DROPHILITED;
  item.pszText = 0;
  ListView_SetItem(hWnd, &item);
}

void CDropTarget::RemoveSelection()
{
  if (m_SelectionIndex >= 0 && m_Panel)
    MySetDropHighlighted(m_Panel->_listView, m_SelectionIndex, false);
  m_SelectionIndex = -1;
}

#ifdef UNDER_CE
#define ChildWindowFromPointEx(hwndParent, pt, uFlags) ChildWindowFromPoint(hwndParent, pt)
#endif

void CDropTarget::PositionCursor(POINTL ptl)
{
  m_SubFolderIndex = -1;
  POINT pt;
  pt.x = ptl.x;
  pt.y = ptl.y;

  RemoveSelection();
  m_IsAppTarget = true;
  m_Panel = NULL;

  m_PanelDropIsAllowed = true;
  if (!m_DropIsAllowed)
    return;
  {
    POINT pt2 = pt;
    App->_window.ScreenToClient(&pt2);
    for (unsigned i = 0; i < kNumPanelsMax; i++)
      if (App->IsPanelVisible(i))
        if (App->Panels[i].IsEnabled())
          if (ChildWindowFromPointEx(App->_window, pt2,
              CWP_SKIPINVISIBLE | CWP_SKIPDISABLED) == (HWND)App->Panels[i])
          {
            m_Panel = &App->Panels[i];
            m_IsAppTarget = false;
            if ((int)i == SrcPanelIndex)
            {
              m_PanelDropIsAllowed = false;
              return;
            }
            break;
          }
    if (m_IsAppTarget)
    {
      if (TargetPanelIndex >= 0)
        m_Panel = &App->Panels[TargetPanelIndex];
      return;
    }
  }

  /*
  m_PanelDropIsAllowed = m_Panel->DoesItSupportOperations();
  if (!m_PanelDropIsAllowed)
    return;
  */

  if (!m_Panel->IsFsOrPureDrivesFolder())
    return;

  if (WindowFromPoint(pt) != (HWND)m_Panel->_listView)
    return;

  LVHITTESTINFO info;
  m_Panel->_listView.ScreenToClient(&pt);
  info.pt = pt;
  int index = ListView_HitTest(m_Panel->_listView, &info);
  if (index < 0)
    return;
  int realIndex = m_Panel->GetRealItemIndex(index);
  if (realIndex == kParentIndex)
    return;
  if (!m_Panel->IsItem_Folder(realIndex))
    return;
  m_SubFolderIndex = realIndex;
  m_SubFolderName = m_Panel->GetItemName(m_SubFolderIndex);
  MySetDropHighlighted(m_Panel->_listView, index, true);
  m_SelectionIndex = index;
}

bool CDropTarget::IsFsFolderPath() const
{
  if (!m_IsAppTarget && m_Panel)
    return (m_Panel->IsFSFolder() || (m_Panel->IsFSDrivesFolder() && m_SelectionIndex >= 0));
  return false;
}

static void ReadUnicodeStrings(const wchar_t *p, size_t size, UStringVector &names)
{
  names.Clear();
  UString name;
  for (;size > 0; size--)
  {
    wchar_t c = *p++;
    if (c == 0)
    {
      if (name.IsEmpty())
        break;
      names.Add(name);
      name.Empty();
    }
    else
      name += c;
  }
}

static void ReadAnsiStrings(const char *p, size_t size, UStringVector &names)
{
  names.Clear();
  AString name;
  for (;size > 0; size--)
  {
    char c = *p++;
    if (c == 0)
    {
      if (name.IsEmpty())
        break;
      names.Add(GetUnicodeString(name));
      name.Empty();
    }
    else
      name += c;
  }
}

static void GetNamesFromDataObject(IDataObject *dataObject, UStringVector &names)
{
  names.Clear();
  FORMATETC etc = { CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
  STGMEDIUM medium;
  HRESULT res = dataObject->GetData(&etc, &medium);
  if (res != S_OK)
    return;
  if (medium.tymed != TYMED_HGLOBAL)
    return;
  {
    NMemory::CGlobal global;
    global.Attach(medium.hGlobal);
    size_t blockSize = GlobalSize(medium.hGlobal);
    NMemory::CGlobalLock dropLock(medium.hGlobal);
    const DROPFILES* dropFiles = (DROPFILES*)dropLock.GetPointer();
    if (!dropFiles)
      return;
    if (blockSize < dropFiles->pFiles)
      return;
    size_t size = blockSize - dropFiles->pFiles;
    const void *namesData = (const Byte *)dropFiles + dropFiles->pFiles;
    if (dropFiles->fWide)
      ReadUnicodeStrings((const wchar_t *)namesData, size / sizeof(wchar_t), names);
    else
      ReadAnsiStrings((const char *)namesData, size, names);
  }
}

bool CDropTarget::IsItSameDrive() const
{
  if (!m_Panel)
    return false;
  if (!IsFsFolderPath())
    return false;

  UString drive;
  
  if (m_Panel->IsFSFolder())
  {
    drive = m_Panel->GetDriveOrNetworkPrefix();
    if (drive.IsEmpty())
      return false;
  }
  else if (m_Panel->IsFSDrivesFolder() && m_SelectionIndex >= 0)
  {
    drive = m_SubFolderName;
    drive.Add_PathSepar();
  }
  else
    return false;

  if (m_SourcePaths.Size() == 0)
    return false;
  
  FOR_VECTOR (i, m_SourcePaths)
  {
    if (!m_SourcePaths[i].IsPrefixedBy_NoCase(drive))
      return false;
  }
  
  return true;
}


/*
  There are 2 different actions, when we drag to 7-Zip:
  1) Drag from any external program except of Explorer to "7-Zip" FS folder.
     We want to create new archive for that operation.
  2) all another operation work as usual file COPY/MOVE
    - Drag from "7-Zip" FS to "7-Zip" FS.
        COPY/MOVE are supported.
    - Drag to open archive in 7-Zip.
        We want to update archive.
        We replace COPY to MOVE.
    - Drag from "7-Zip" archive to "7-Zip" FS.
        We replace COPY to MOVE.
*/

DWORD CDropTarget::GetEffect(DWORD keyState, POINTL /* pt */, DWORD allowedEffect)
{
  if (!m_DropIsAllowed || !m_PanelDropIsAllowed)
    return DROPEFFECT_NONE;

  if (!IsFsFolderPath() || !m_SetPathIsOK)
    allowedEffect &= ~DROPEFFECT_MOVE;

  DWORD effect = 0;
  
  if (keyState & MK_CONTROL)
    effect = allowedEffect & DROPEFFECT_COPY;
  else if (keyState & MK_SHIFT)
    effect = allowedEffect & DROPEFFECT_MOVE;
  
  if (effect == 0)
  {
    if (allowedEffect & DROPEFFECT_COPY)
    effect = DROPEFFECT_COPY;
    if (allowedEffect & DROPEFFECT_MOVE)
    {
      if (IsItSameDrive())
        effect = DROPEFFECT_MOVE;
    }
  }
  if (effect == 0)
    return DROPEFFECT_NONE;
  return effect;
}

UString CDropTarget::GetTargetPath() const
{
  if (!IsFsFolderPath())
    return UString();
  UString path = m_Panel->GetFsPath();
  if (m_SubFolderIndex >= 0 && !m_SubFolderName.IsEmpty())
  {
    path += m_SubFolderName;
    path.Add_PathSepar();
  }
  return path;
}

bool CDropTarget::SetPath(bool enablePath) const
{
  UINT setFolderFormat = RegisterClipboardFormat(kSvenZipSetFolderFormat);
  
  FORMATETC etc = { (CLIPFORMAT)setFolderFormat, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
  STGMEDIUM medium;
  medium.tymed = etc.tymed;
  medium.pUnkForRelease = 0;
  UString path;
  if (enablePath)
    path = GetTargetPath();
  size_t size = path.Len() + 1;
  medium.hGlobal = GlobalAlloc(GHND | GMEM_SHARE, size * sizeof(wchar_t));
  if (!medium.hGlobal)
    return false;
  wchar_t *dest = (wchar_t *)GlobalLock(medium.hGlobal);
  if (!dest)
  {
    GlobalUnlock(medium.hGlobal);
    return false;
  }
  MyStringCopy(dest, (const wchar_t *)path);
  GlobalUnlock(medium.hGlobal);
  bool res = m_DataObject->SetData(&etc, &medium, FALSE) == S_OK;
  GlobalFree(medium.hGlobal);
  return res;
}

bool CDropTarget::SetPath()
{
  m_SetPathIsOK = SetPath(m_DropIsAllowed && m_PanelDropIsAllowed && IsFsFolderPath());
  return m_SetPathIsOK;
}

STDMETHODIMP CDropTarget::DragEnter(IDataObject * dataObject, DWORD keyState,
      POINTL pt, DWORD *effect)
{
  GetNamesFromDataObject(dataObject, m_SourcePaths);
  QueryGetData(dataObject);
  m_DataObject = dataObject;
  return DragOver(keyState, pt, effect);
}


STDMETHODIMP CDropTarget::DragOver(DWORD keyState, POINTL pt, DWORD *effect)
{
  PositionCursor(pt);
  SetPath();
  *effect = GetEffect(keyState, pt, *effect);
  return S_OK;
}


STDMETHODIMP CDropTarget::DragLeave()
{
  RemoveSelection();
  SetPath(false);
  m_DataObject.Release();
  return S_OK;
}

// We suppose that there was ::DragOver for same POINTL_pt before ::Drop
// So SetPath() is same as in Drop.

STDMETHODIMP CDropTarget::Drop(IDataObject *dataObject, DWORD keyState,
      POINTL pt, DWORD * effect)
{
  QueryGetData(dataObject);
  PositionCursor(pt);
  m_DataObject = dataObject;
  bool needDrop = true;
  if (m_DropIsAllowed && m_PanelDropIsAllowed)
    if (IsFsFolderPath())
      needDrop = !SetPath();
  *effect = GetEffect(keyState, pt, *effect);
  if (m_DropIsAllowed && m_PanelDropIsAllowed)
  {
    if (needDrop)
    {
      UString path = GetTargetPath();
      if (m_IsAppTarget && m_Panel)
        if (m_Panel->IsFSFolder())
          path = m_Panel->GetFsPath();
      m_Panel->DropObject(dataObject, path);
    }
  }
  RemoveSelection();
  m_DataObject.Release();
  return S_OK;
}

void CPanel::DropObject(IDataObject *dataObject, const UString &folderPath)
{
  UStringVector names;
  GetNamesFromDataObject(dataObject, names);
  CompressDropFiles(names, folderPath);
}

/*
void CPanel::CompressDropFiles(HDROP dr)
{
  UStringVector fileNames;
  {
    NShell::CDrop drop(true);
    drop.Attach(dr);
    drop.QueryFileNames(fileNames);
  }
  CompressDropFiles(fileNamesUnicode);
}
*/

static bool IsFolderInTemp(const FString &path)
{
  FString tempPath;
  if (!MyGetTempPath(tempPath))
    return false;
  if (tempPath.IsEmpty())
    return false;
  unsigned len = tempPath.Len();
  if (path.Len() < len)
    return false;
  return CompareFileNames(path.Left(len), tempPath) == 0;
}

static bool AreThereNamesFromTemp(const UStringVector &fileNames)
{
  FString tempPathF;
  if (!MyGetTempPath(tempPathF))
    return false;
  UString tempPath = fs2us(tempPathF);
  if (tempPath.IsEmpty())
    return false;
  FOR_VECTOR (i, fileNames)
    if (fileNames[i].IsPrefixedBy_NoCase(tempPath))
      return true;
  return false;
}

void CPanel::CompressDropFiles(const UStringVector &fileNames, const UString &folderPath)
{
  if (fileNames.Size() == 0)
    return;
  bool createNewArchive = true;
  if (!IsFSFolder())
    createNewArchive = !DoesItSupportOperations();

  if (createNewArchive)
  {
    UString folderPath2 = folderPath;
    if (folderPath2.IsEmpty())
    {
      FString folderPath2F;
      GetOnlyDirPrefix(us2fs(fileNames.Front()), folderPath2F);
      folderPath2 = fs2us(folderPath2F);
      if (IsFolderInTemp(folderPath2F))
        folderPath2 = ROOT_FS_FOLDER;
    }
    
    const UString arcName = CreateArchiveName(fileNames);
    
    CompressFiles(folderPath2, arcName, L"",
      true, // addExtension
      fileNames,
      false, // email
      true, // showDialog
      AreThereNamesFromTemp(fileNames) // waitFinish
      );
  }
  else
    CopyFromAsk(fileNames);
}

// PanelDrag.cpp

#include "StdAfx.h"

#ifdef UNDER_CE
#include <winuserm.h>
#endif

#include "../../../../C/7zVersion.h"
#include "../../../../C/CpuArch.h"

#include "../../../Common/StringConvert.h"
#include "../../../Common/Wildcard.h"

#include "../../../Windows/COM.h"
#include "../../../Windows/MemoryGlobal.h"
#include "../../../Windows/Menu.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/Shell.h"

#include "../Common/ArchiveName.h"
#include "../Common/CompressCall.h"
#include "../Common/ExtractingFilePath.h"

#include "MessagesDialog.h"

#include "App.h"
#include "EnumFormatEtc.h"
#include "FormatUtils.h"
#include "LangUtils.h"

#include "resource.h"
#include "../Explorer/resource.h"

using namespace NWindows;
using namespace NFile;
using namespace NDir;

#ifndef _UNICODE
extern bool g_IsNT;
#endif

#define PRF(x)
#define PRF_W(x)
// #define PRF2(x)
#define PRF3(x)
#define PRF3_W(x)
#define PRF4(x)
// #define PRF4(x) OutputDebugStringA(x)
// #define PRF4_W(x) OutputDebugStringW(x)

// #define SHOW_DEBUG_DRAG

#ifdef SHOW_DEBUG_DRAG

#define PRF_(x) { x; }

static void Print_Point(const char *name, DWORD keyState, const POINTL &pt, DWORD effect)
{
  AString s (name);
  s += " x=";  s.Add_UInt32((unsigned)pt.x);
  s += " y=";  s.Add_UInt32((unsigned)pt.y);
  s += " k=";  s.Add_UInt32(keyState);
  s += " e=";  s.Add_UInt32(effect);
  PRF4(s);
}

#else

#define PRF_(x)

#endif


#define kTempDirPrefix  FTEXT("7zE")

// **************** NanaZip Modification Start ****************
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
// all versions: k_Format_7zip_SetTargetFolder format to transfer folder path from target to source
static LPCTSTR const k_Format_7zip_SetTargetFolder = TEXT("7-Zip::SetTargetFolder");
// new v23 formats:
static LPCTSTR const k_Format_7zip_SetTransfer = TEXT("7-Zip::SetTransfer");
static LPCTSTR const k_Format_7zip_GetTransfer = TEXT("7-Zip::GetTransfer");
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
// all versions: k_Format_7zip_SetTargetFolder format to transfer folder path from target to source
static LPCTSTR const k_Format_7zip_SetTargetFolder = TEXT("NanaZip::SetTargetFolder");
// new v23 formats:
static LPCTSTR const k_Format_7zip_SetTransfer = TEXT("NanaZip::SetTransfer");
static LPCTSTR const k_Format_7zip_GetTransfer = TEXT("NanaZip::GetTransfer");
// **************** NanaZip Modification End ****************

/*
  Win10: clipboard formats.
  There are about 16K free ids (formats) per system that can be
  registered with RegisterClipboardFormat() with different names.
  Probably that 16K ids space is common for ids registering for both
  formats: RegisterClipboardFormat(), and registered window classes:
  RegisterClass(). But ids for window classes will be deleted from
  the list after process finishing. And registered clipboard
  formats probably will be deleted from the list only after reboot.
*/

// static bool const g_CreateArchive_for_Drag_from_7zip = false;
// static bool const g_CreateArchive_for_Drag_from_Explorer = true;
    // = false; // for debug

/*
How DoDragDrop() works:
{
  IDropSource::QueryContinueDrag()  (keyState & MK_LBUTTON) != 0
  IDropTarget::Enter()
    IDropSource::GiveFeedback()
  IDropTarget::DragOver()
    IDropSource::GiveFeedback()

  for()
  {
    IDropSource::QueryContinueDrag()  (keyState & MK_LBUTTON) != 0
    IDropTarget::DragOver()           (keyState & MK_LBUTTON) != 0
      IDropSource::GiveFeedback()
  }

  {
    // DoDragDrop() in Win10 before calling // QueryContinueDrag()
    // with (*(keyState & MK_LBUTTON) == 0) probably calls:
    //   1) IDropTarget::DragOver() with same point values (x,y), but (keyState & MK_LBUTTON) != 0)
    //   2) IDropSource::GiveFeedback().
    // so DropSource can know exact GiveFeedback(effect) mode just before LBUTTON releasing.

    if (IDropSource::QueryContinueDrag() for (keyState & MK_LBUTTON) == 0
      returns DRAGDROP_S_DROP), it will call
    IDropTarget::Drop()
  }
  or
  {
    IDropSource::QueryContinueDrag()
    IDropTarget::DragLeave()
    IDropSource::GiveFeedback(0)
  }
  or
  {
    if (IDropSource::QueryContinueDrag()
      returns DRAGDROP_S_CANCEL)
    IDropTarget::DragLeave()
  }
}
*/


// ---------- CDropTarget ----------

static const UInt32 k_Struct_Id_SetTranfer = 2;  // it's our selected id
static const UInt32 k_Struct_Id_GetTranfer = 3;  // it's our selected id

static const UInt64 k_Program_Id = 1; // "7-Zip"

enum E_Program_ISA
{
  k_Program_ISA_x86   = 2,
  k_Program_ISA_x64   = 3,
  k_Program_ISA_armt  = 4,
  k_Program_ISA_arm64 = 5,
  k_Program_ISA_arm32 = 6,
  k_Program_ISA_ia64  = 9
};

#define k_Program_Ver ((MY_VER_MAJOR << 16) | MY_VER_MINOR)


// k_SourceFlags_* are flags that are sent from Source to Target

static const UInt32 k_SourceFlags_DoNotProcessInTarget = 1 << 1;
/* Do not process in Target. Source will process operation instead of Target.
   By default Target processes Drop opearation. */
// static const UInt32 k_SourceFlags_ProcessInTarget      = 1 << 2;

static const UInt32 k_SourceFlags_DoNotWaitFinish   = 1 << 3;
static const UInt32 k_SourceFlags_WaitFinish        = 1 << 4;
/* usually Source needs WaitFinish, if temp files were created. */

static const UInt32 k_SourceFlags_TempFiles         = 1 << 6;
static const UInt32 k_SourceFlags_NamesAreParent    = 1 << 7;
/* if returned path list for GetData(CF_HDROP) contains
   path of parent temp folder instead of final paths of items
   that will be extracted later from archive */

static const UInt32 k_SourceFlags_SetTargetFolder   = 1 << 8;
/* SetData::("SetTargetFolder") was called (with empty or non-empty string) */

static const UInt32 k_SourceFlags_SetTargetFolder_NonEmpty  = 1 << 9;
/* SetData::("SetTargetFolder") was called with non-empty string */

static const UInt32 k_SourceFlags_NeedExtractOpToFs = 1 << 10;

static const UInt32 k_SourceFlags_Copy_WasCalled = 1 << 11;

static const UInt32 k_SourceFlags_LeftButton        = 1 << 14;
static const UInt32 k_SourceFlags_RightButton       = 1 << 15;


static const UInt32 k_TargetFlags_WasCanceled = 1 << 0;
static const UInt32 k_TargetFlags_MustBeProcessedBySource = 1 << 1;
static const UInt32 k_TargetFlags_WasProcessed    = 1 << 2;
static const UInt32 k_TargetFlags_DoNotWaitFinish = 1 << 3;
static const UInt32 k_TargetFlags_WaitFinish      = 1 << 4;
static const UInt32 k_TargetFlags_MenuWasShown    = 1 << 16;

struct CDataObject_TransferBase
{
  UInt32 Struct_Id;
  UInt32 Struct_Size;

  UInt64 Program_Id;
  UInt32 Program_Ver_Main;
  UInt32 Program_Ver_Build;
  UInt32 Program_ISA;
  UInt32 Program_Flags;

  UInt32 ProcessId;
  UInt32 _reserved1[7];

protected:
  void Init_Program();
};


void CDataObject_TransferBase::Init_Program()
{
  Program_Id = k_Program_Id;
  Program_ISA =
    #if defined(MY_CPU_AMD64)
      k_Program_ISA_x64
    #elif defined(MY_CPU_X86)
      k_Program_ISA_x86
    #elif defined(MY_CPU_ARM64)
      k_Program_ISA_arm64
    #elif defined(MY_CPU_ARM32)
      k_Program_ISA_arm32
    #elif defined(MY_CPU_ARMT) || defined(MY_CPU_ARM)
      k_Program_ISA_armt
    #elif defined(MY_CPU_IA64)
      k_Program_ISA_ia64
    #else
      0
    #endif
      ;
  Program_Flags = sizeof(size_t);
  Program_Ver_Main = k_Program_Ver;
  // Program_Ver_Build = 0;
  ProcessId = GetCurrentProcessId();
}


#if defined(__GNUC__) && !defined(__clang__)
/* 'void* memset(void*, int, size_t)' clearing an object
    of non-trivial type 'struct CDataObject_SetTransfer' */
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif


struct CDataObject_GetTransfer:
public CDataObject_TransferBase
{
  UInt32 Flags;

  UInt32 _reserved2[11];

  CDataObject_GetTransfer()
  {
    memset(this, 0, sizeof(*this));
    Init_Program();
    Struct_Id = k_Struct_Id_GetTranfer;
    Struct_Size = sizeof(*this);
  }

  bool Check() const
  {
    return Struct_Size >= sizeof(*this) && Struct_Id == k_Struct_Id_GetTranfer;
  }
};


enum Enum_FolderType
{
  k_FolderType_None,
  k_FolderType_Unknown = 1,
  k_FolderType_Fs = 2,
  k_FolderType_AltStreams = 3,
  k_FolderType_Archive = 4
};

struct CTargetTransferInfo
{
  UInt32 Flags;
  UInt32 FuncType;

  UInt32 KeyState;
  UInt32 OkEffects;
  POINTL Point;

  UInt32 Cmd_Effect;
  UInt32 Cmd_Type;
  UInt32 FolderType;
  UInt32 _reserved3[3];

  CTargetTransferInfo()
  {
    memset(this, 0, sizeof(*this));
  }
};

struct CDataObject_SetTransfer:
public CDataObject_TransferBase
{
  CTargetTransferInfo Target;

  void Init()
  {
    memset(this, 0, sizeof(*this));
    Init_Program();
    Struct_Id = k_Struct_Id_SetTranfer;
    Struct_Size = sizeof(*this);
  }

  bool Check() const
  {
    return Struct_Size >= sizeof(*this) && Struct_Id == k_Struct_Id_SetTranfer;
  }
};





enum Enum_DragTargetMode
{
  k_DragTargetMode_None   = 0,
  k_DragTargetMode_Leave  = 1,
  k_DragTargetMode_Enter  = 2,
  k_DragTargetMode_Over   = 3,
  k_DragTargetMode_Drop_Begin = 4,
  k_DragTargetMode_Drop_End   = 5
};


// ---- menu ----

namespace NDragMenu {

enum Enum_CmdId
{
  k_None          = 0,
  k_Cancel        = 1,
  k_Copy_Base     = 2, // to fs
  k_Copy_ToArc    = 3,
  k_AddToArc      = 4
  /*
  k_OpenArc       = 8,
  k_TestArc       = 9,
  k_ExtractFiles  = 10,
  k_ExtractHere   = 11
  */
};

struct CCmdLangPair
{
  unsigned CmdId_and_Flags;
  unsigned LangId;
};

static const UInt32 k_MenuFlags_CmdMask = (1 << 7) - 1;
static const UInt32 k_MenuFlag_Copy = 1 << 14;
static const UInt32 k_MenuFlag_Move = 1 << 15;
// #define IDS_CANCEL (IDCANCEL + 400)
#define IDS_CANCEL 402

static const CCmdLangPair g_Pairs[] =
{
  { k_Copy_Base  | k_MenuFlag_Copy,  IDS_COPY },
  { k_Copy_Base  | k_MenuFlag_Move,  IDS_MOVE },
  { k_Copy_ToArc | k_MenuFlag_Copy,  IDS_COPY_TO },
  // { k_Copy_ToArc | k_MenuFlag_Move,  IDS_MOVE_TO }, // IDS_CONTEXT_COMPRESS_TO
  // { k_OpenArc,      IDS_CONTEXT_OPEN },
  // { k_ExtractFiles, IDS_CONTEXT_EXTRACT },
  // { k_ExtractHere,  IDS_CONTEXT_EXTRACT_HERE },
  // { k_TestArc,      IDS_CONTEXT_TEST },
  { k_AddToArc   | k_MenuFlag_Copy,  IDS_CONTEXT_COMPRESS },
  { k_Cancel, IDS_CANCEL }
};

}


class CDropTarget Z7_final:
  public IDropTarget,
  public CMyUnknownImp
{
  Z7_COM_UNKNOWN_IMP_1_MT(IDropTarget)
  STDMETHOD(DragEnter)(IDataObject *dataObject, DWORD keyState, POINTL pt, DWORD *effect) Z7_override;
  STDMETHOD(DragOver)(DWORD keyState, POINTL pt, DWORD *effect) Z7_override;
  STDMETHOD(DragLeave)() Z7_override;
  STDMETHOD(Drop)(IDataObject *dataObject, DWORD keyState, POINTL pt, DWORD *effect) Z7_override;

  bool m_IsRightButton;
  bool m_GetTransfer_WasSuccess;
  bool m_DropIsAllowed;      // = true, if data IDataObject can return CF_HDROP (so we can get list of paths)
  bool m_PanelDropIsAllowed; // = false, if current target_panel is source_panel.
                             // check it only if m_DropIsAllowed == true
                             // we use it to show icon effect that drop is not allowed here.

  CMyComPtr<IDataObject> m_DataObject; // we set it in DragEnter()
  UStringVector m_SourcePaths;

  // int m_DropHighlighted_SelectionIndex;
  // int m_SubFolderIndex;      // realIndex of item in m_Panel list (if drop cursor to that item)
  // UString m_DropHighlighted_SubFolderName;   // name of folder in m_Panel list (if drop cursor to that folder)

  CPanel *m_Panel;
  bool m_IsAppTarget;        // true, if we want to drop to app window (not to panel)

  bool m_TargetPath_WasSent_ToDataObject;           // true, if TargetPath was sent
  bool m_TargetPath_NonEmpty_WasSent_ToDataObject;  // true, if non-empty TargetPath was sent
  bool m_Transfer_WasSent_ToDataObject;  // true, if Transfer was sent
  UINT m_Format_7zip_SetTargetFolder;
  UINT m_Format_7zip_SetTransfer;
  UINT m_Format_7zip_GetTransfer;

  UInt32 m_ProcessId; // for sending

  bool IsItSameDrive() const;

  // void Try_QueryGetData(IDataObject *dataObject);
  void LoadNames_From_DataObject(IDataObject *dataObject);

  UInt32 GetFolderType() const;
  bool IsFsFolderPath() const;
  DWORD GetEffect(DWORD keyState, POINTL pt, DWORD allowedEffect) const;
  void RemoveSelection();
  void PositionCursor(const POINTL &ptl);
  UString GetTargetPath() const;
  bool SendToSource_TargetPath_enable(IDataObject *dataObject, bool enablePath);
  bool SendToSource_UInt32(IDataObject *dataObject, UINT format, UInt32 value);
  bool SendToSource_TransferInfo(IDataObject *dataObject,
      const CTargetTransferInfo &info);
  void SendToSource_auto(IDataObject *dataObject,
      const CTargetTransferInfo &info);
  void SendToSource_Drag(CTargetTransferInfo &info)
  {
    SendToSource_auto(m_DataObject, info);
  }

  void ClearState();

public:
  CDropTarget();

  CApp *App;
  int SrcPanelIndex;     // index of D&D source_panel
  int TargetPanelIndex;  // what panel to use as target_panel of Application
};




// ---------- CDataObject ----------

/*
  Some programs (like Sticky Notes in Win10) do not like
  virtual non-existing items (files/dirs) in CF_HDROP format.
  So we use two versions of CF_HDROP data:
    m_hGlobal_HDROP_Pre   : the list contains only destination path of temp directory.
        That directory later will be filled with extracted items.
    m_hGlobal_HDROP_Final : the list contains paths of all root items that
        will be created in temp directory by archive extraction operation,
        or the list of existing fs items, if source is filesystem directory.

  The DRAWBACK: some programs (like Edge in Win10) can use names from IDataObject::GetData()
  call that was called before IDropSource::QueryContinueDrag() where we set (UseFinalGlobal = true)
  So such programs will use non-relevant m_hGlobal_HDROP_Pre item,
  instead of m_hGlobal_HDROP_Final items.
*/

class CDataObject Z7_final:
  public IDataObject,
  public CMyUnknownImp
{
  Z7_COM_UNKNOWN_IMP_1_MT(IDataObject)

  Z7_COMWF_B GetData(LPFORMATETC pformatetcIn, LPSTGMEDIUM medium) Z7_override;
  Z7_COMWF_B GetDataHere(LPFORMATETC pformatetc, LPSTGMEDIUM medium) Z7_override;
  Z7_COMWF_B QueryGetData(LPFORMATETC pformatetc) Z7_override;

  Z7_COMWF_B GetCanonicalFormatEtc(LPFORMATETC /* pformatetc */, LPFORMATETC pformatetcOut) Z7_override
  {
    if (!pformatetcOut)
      return E_INVALIDARG;
    pformatetcOut->ptd = NULL;
    return E_NOTIMPL;
  }

  Z7_COMWF_B SetData(LPFORMATETC etc, STGMEDIUM *medium, BOOL release) Z7_override;
  Z7_COMWF_B EnumFormatEtc(DWORD drection, LPENUMFORMATETC *enumFormatEtc) Z7_override;

  Z7_COMWF_B DAdvise(FORMATETC * /* etc */, DWORD /* advf */, LPADVISESINK /* pAdvSink */, DWORD * /* pdwConnection */) Z7_override
    { return OLE_E_ADVISENOTSUPPORTED; }
  Z7_COMWF_B DUnadvise(DWORD /* dwConnection */) Z7_override
    { return OLE_E_ADVISENOTSUPPORTED; }
  Z7_COMWF_B EnumDAdvise(LPENUMSTATDATA *ppenumAdvise) Z7_override
  {
    if (ppenumAdvise)
      *ppenumAdvise = NULL;
    return OLE_E_ADVISENOTSUPPORTED;
  }

  bool m_PerformedDropEffect_WasSet;
  bool m_LogicalPerformedDropEffect_WasSet;
  bool m_DestDirPrefix_FromTarget_WasSet;
public:
  bool m_Transfer_WasSet;
private:
  // GetData formats (source to target):
  FORMATETC m_Etc;
  // UINT m_Format_FileOpFlags;
  // UINT m_Format_PreferredDropEffect;

  // SetData() formats (target to source):
  // 7-Zip's format:
  UINT m_Format_7zip_SetTargetFolder;
  UINT m_Format_7zip_SetTransfer;
  UINT m_Format_7zip_GetTransfer; // for GetData()

  UINT m_Format_PerformedDropEffect;
  UINT m_Format_LogicalPerformedDropEffect;
  UINT m_Format_DisableDragText;
  UINT m_Format_IsShowingLayered;
  UINT m_Format_IsShowingText;
  UINT m_Format_DropDescription;
  UINT m_Format_TargetCLSID;

  DWORD m_PerformedDropEffect;
  DWORD m_LogicalPerformedDropEffect;

  void CopyFromPanelTo_Folder();
  HRESULT SetData2(const FORMATETC *formatetc, const STGMEDIUM *medium);

public:
  bool IsRightButton;
  bool IsTempFiles;

  bool UsePreGlobal;
  bool DoNotProcessInTarget;

  bool NeedCall_Copy;
  bool Copy_WasCalled;

  NMemory::CGlobal m_hGlobal_HDROP_Pre;
  NMemory::CGlobal m_hGlobal_HDROP_Final;
  // NMemory::CGlobal m_hGlobal_FileOpFlags;
  // NMemory::CGlobal m_hGlobal_PreferredDropEffect;

  CPanel *Panel;
  CRecordVector<UInt32> Indices;

  UString SrcDirPrefix_Temp; // FS directory with source files or Temp
  UString DestDirPrefix_FromTarget;
  /* destination Path that was sent by Target via SetData().
     it can be altstreams prefix.
     if (!DestDirPrefix_FromTarget.IsEmpty()) m_Panel->CompressDropFiles() was not called by Target.
     So we must do drop actions in Source */
  HRESULT Copy_HRESULT;
  UStringVector Messages;

  CDataObject();
public:
  CDataObject_SetTransfer m_Transfer;
};


// for old mingw:
#ifndef CFSTR_LOGICALPERFORMEDDROPEFFECT
#define CFSTR_LOGICALPERFORMEDDROPEFFECT    TEXT("Logical Performed DropEffect")
#endif
#ifndef CFSTR_TARGETCLSID
#define CFSTR_TARGETCLSID                   TEXT("TargetCLSID")                         // HGLOBAL with a CLSID of the drop target
#endif



CDataObject::CDataObject()
{
  // GetData formats (source to target):
  // and we use CF_HDROP format to transfer file paths from source to target:
  m_Etc.cfFormat = CF_HDROP;
  m_Etc.ptd = NULL;
  m_Etc.dwAspect = DVASPECT_CONTENT;
  m_Etc.lindex = -1;
  m_Etc.tymed = TYMED_HGLOBAL;

  // m_Format_FileOpFlags          = RegisterClipboardFormat(TEXT("FileOpFlags"));
  // m_Format_PreferredDropEffect  = RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT); // "Preferred DropEffect"

  // SetData() formats (target to source):
  m_Format_7zip_SetTargetFolder = RegisterClipboardFormat(k_Format_7zip_SetTargetFolder);
  m_Format_7zip_SetTransfer     = RegisterClipboardFormat(k_Format_7zip_SetTransfer);
  m_Format_7zip_GetTransfer     = RegisterClipboardFormat(k_Format_7zip_GetTransfer);

  m_Format_PerformedDropEffect  = RegisterClipboardFormat(CFSTR_PERFORMEDDROPEFFECT); // "Performed DropEffect"
  m_Format_LogicalPerformedDropEffect = RegisterClipboardFormat(CFSTR_LOGICALPERFORMEDDROPEFFECT); // "Logical Performed DropEffect"
  m_Format_DisableDragText      = RegisterClipboardFormat(TEXT("DisableDragText"));
  m_Format_IsShowingLayered     = RegisterClipboardFormat(TEXT("IsShowingLayered"));
  m_Format_IsShowingText        = RegisterClipboardFormat(TEXT("IsShowingText"));
  m_Format_DropDescription      = RegisterClipboardFormat(TEXT("DropDescription"));
  m_Format_TargetCLSID          = RegisterClipboardFormat(CFSTR_TARGETCLSID);

  m_PerformedDropEffect = 0;
  m_LogicalPerformedDropEffect = 0;

  m_PerformedDropEffect_WasSet = false;
  m_LogicalPerformedDropEffect_WasSet = false;

  m_DestDirPrefix_FromTarget_WasSet = false;
  m_Transfer_WasSet = false;

  IsRightButton = false;
  IsTempFiles = false;

  UsePreGlobal = false;
  DoNotProcessInTarget = false;

  NeedCall_Copy = false;
  Copy_WasCalled = false;

  Copy_HRESULT = S_OK;
}



void CDataObject::CopyFromPanelTo_Folder()
{
  try
  {
    CCopyToOptions options;
    options.folder = SrcDirPrefix_Temp;
    /* 15.13: fixed problem with mouse cursor for password window.
       DoDragDrop() probably calls SetCapture() to some hidden window.
       But it's problem, if we show some modal window, like MessageBox.
       So we return capture to our window.
       If you know better way to solve the problem, please notify 7-Zip developer.
    */
    // MessageBoxW(*Panel, L"test", L"test", 0);
    /* HWND oldHwnd = */ SetCapture(*Panel);
    Copy_WasCalled = true;
    Copy_HRESULT = E_FAIL;
    Copy_HRESULT = Panel->CopyTo(options, Indices, &Messages);
    // do we need to restore capture?
    // ReleaseCapture();
    // oldHwnd = SetCapture(oldHwnd);
  }
  catch(...)
  {
    Copy_HRESULT = E_FAIL;
  }
}


#ifdef SHOW_DEBUG_DRAG

static void PrintFormat2(AString &s, unsigned format)
{
  s += " ";
  s += "= format=";
  s.Add_UInt32(format);
  s += " ";
  const int k_len = 512;
  CHAR temp[k_len];
  if (GetClipboardFormatNameA(format, temp, k_len) && strlen(temp) != 0)
    s += temp;
}

static void PrintFormat(const char *title, unsigned format)
{
  AString s (title);
  PrintFormat2(s, format);
  PRF4(s);
}

static void PrintFormat_AndData(const char *title, unsigned format, const void *data, size_t size)
{
  AString s (title);
  PrintFormat2(s, format);
  s += " size=";
  s.Add_UInt32((UInt32)size);
  for (size_t i = 0; i < size && i < 16; i++)
  {
    s += " ";
    s.Add_UInt32(((const Byte *)data)[i]);
  }
  PRF4(s);
}

static void PrintFormat_GUIDToStringW(const void *p)
{
  const GUID *guid = (const GUID *)p;
  UString s;
  const unsigned kSize = 48;
  StringFromGUID2(*guid, s.GetBuf(kSize), kSize);
  s.ReleaseBuf_CalcLen(kSize);
  PRF3_W(s);
}

// Vista
typedef enum
{
  MY_DROPIMAGE_INVALID  = -1,                // no image preference (use default)
  MY_DROPIMAGE_NONE     = 0,                 // red "no" circle
  MY_DROPIMAGE_COPY     = DROPEFFECT_COPY,   // plus for copy
  MY_DROPIMAGE_MOVE     = DROPEFFECT_MOVE,   // movement arrow for move
  MY_DROPIMAGE_LINK     = DROPEFFECT_LINK,   // link arrow for link
  MY_DROPIMAGE_LABEL    = 6,                 // tag icon to indicate metadata will be changed
  MY_DROPIMAGE_WARNING  = 7,                 // yellow exclamation, something is amiss with the operation
  MY_DROPIMAGE_NOIMAGE  = 8                  // no image at all
} MY_DROPIMAGETYPE;

typedef struct {
  MY_DROPIMAGETYPE type;
  WCHAR szMessage[MAX_PATH];
  WCHAR szInsert[MAX_PATH];
} MY_DROPDESCRIPTION;

#endif


/*
IDataObject::SetData(LPFORMATETC etc, STGMEDIUM *medium, BOOL release)
======================================================================

  Main purpose of CDataObject is to transfer data from source to target
  of drag and drop operation.
  But also CDataObject can be used to transfer data in backward direction
  from target to source (even if target and source are different processes).
  There are some predefined Explorer's formats to transfer some data from target to source.
  And 7-Zip uses 7-Zip's format k_Format_7zip_SetTargetFolder to transfer
  destination directory path from target to source.

  Our CDataObject::SetData() function here is used only to transfer data from target to source.
  Usual source_to_target data is filled to m_hGlobal_* objects directly without SetData() calling.

The main problem of SetData() is ownership of medium for (release == TRUE) case.

SetData(,, release = TRUE) from different processes (DropSource and DropTarget)
===============================================================================
{
  MS DOCs about (STGMEDIUM *medium) ownership:
    The data object called does not take ownership of the data
    until it has successfully received it and no error code is returned.

  Each of processes (Source and Target) has own copy of medium allocated.
  Windows code creates proxy IDataObject object in Target process to transferr
  SetData() call between Target and Source processes via special proxies:
    DropTarget ->
    proxy_DataObject_in_Target ->
    proxy_in_Source ->
    DataObject_in_Source
  when Target calls SetData() with proxy_DataObject_in_Target,
  the system and proxy_in_Source
   - allocates proxy-medium-in-Source process
   - copies medium data from Target to that proxy-medium-in-Source
   - sends proxy-medium-in-Source to DataObject_in_Source->SetData().

  after returning from SetData() to Target process:
    Win10 proxy_DataObject_in_Target releases original medium in Target process,
    only if SetData() in Source returns S_OK. It's consistent with DOCs above.

  for unsupported cfFormat:
  [DropSource is 7-Zip 22.01 (old) : (etc->cfFormat != m_Format_7zip_SetTargetFolder && release == TRUE)]
  (DropSource is WinRAR case):
  Source doesn't release medium and returns error (for example, E_NOTIMPL)
  {
    Then Win10 proxy_in_Source also doesn't release proxy-medium-in-Source.
    So there is memory leak in Source process.
    Probably Win10 proxy_in_Source tries to avoid possible double releasing
    that can be more fatal than memory leak.

    Then Win10 proxy_DataObject_in_Target also doesn't release
    original medium, that was allocated by DropTarget.
    So if DropTarget also doesn't release medium, there is memory leak in
    DropTarget process too.
    DropTarget is Win10-Explorer probably doesn't release medium in that case.
  }

  [DropSource is 7-Zip 22.01 (old) : (etc->cfFormat == m_Format_7zip_SetTargetFolder && release == TRUE)]
  DropSource returns S_OK and doesn't release medium:
  {
    then there is memory leak in DropSource process only.
  }

  (DropSource is 7-Zip v23 (new)):
  (DropSource is Win10-Explorer case)
  {
    Win10-Explorer-DropSource probably always releases medium,
    and then it always returns S_OK.
    So Win10 proxy_DataObject_in_Target also releases
    original medium, that was allocated by DropTarget.
    So there is no memory leak in Source and Target processes.
  }

  if (DropTarget is Win10-Explorer)
  {
    Explorer Target uses SetData(,, (release = TRUE)) and
    Explorer Target probably doesn't free memory after SetData(),
      even if SetData(,, (release = TRUE)) returns E_NOTIMPL;
  }

  if (DropSource is Win10-Explorer)
  {
    (release == FALSE) doesn't work, and SetData() returns E_NOTIMPL;
    (release == TRUE)  works, and SetData() returns S_OK, and
                       it returns S_OK even for formats unsupported by Explorer.
  }

  To be more compatible with DOCs and Win10-Explorer and to avoid memory leaks,
  we use the following scheme for our IDataObject::SetData(,, release == TRUE)
  in DropSource code:
  if (release == TRUE) { our SetData() always releases medium
      with ReleaseStgMedium() and returns S_OK; }
  The DRAWBACK of that scheme:
    The caller always receives S_OK,
    so the caller doesn't know about any error in SetData() in that case.

for 7zip-Target to 7zip-Source calls:
  we use (release == FALSE)
  So we avoid (release == TRUE) memory leak problems,
  and we can get real return code from SetData().

for 7zip-Target to Explorer-Source calls:
  we use (release == TRUE).
  beacuse Explorer-Source doesn't accept (release == FALSE).
}
*/

/*
https://github.com/MicrosoftDocs/win32/blob/docs/desktop-src/shell/datascenarios.md
CFSTR_PERFORMEDDROPEFFECT:
  is used by the target to inform the data object through its
  IDataObject::SetData method of the outcome of a data transfer.
CFSTR_PREFERREDDROPEFFECT:
  is used by the source to specify whether its preferred method of data transfer is move or copy.
*/

Z7_COMWF_B CDataObject::SetData(LPFORMATETC etc, STGMEDIUM *medium, BOOL release)
{
  try {
  const HRESULT hres = SetData2(etc, medium);
  // PrintFormat(release ? "SetData RELEASE=TRUE" : "SetData RELEASE=FALSE" , etc->cfFormat);
  if (release)
  {
    /*
    const DWORD tymed = medium->tymed;
    IUnknown *pUnkForRelease = medium->pUnkForRelease;
    */
    // medium->tymed = NULL; // for debug
    // return E_NOTIMPL;  // for debug
    ReleaseStgMedium(medium);
    /* ReleaseStgMedium() will change STGMEDIUM::tymed to (TYMED_NULL = 0).
       but we also can clear (medium.hGlobal = NULL),
       to prevent some incorrect releasing, if the caller will try to release the data  */
    /*
    if (medium->tymed == TYMED_NULL && tymed == TYMED_HGLOBAL && !pUnkForRelease)
      medium->hGlobal = NULL;
    */
    // do we need return S_OK; for (tymed != TYMED_HGLOBAL) cases ?
    /* we return S_OK here to shows that we take ownership of the data in (medium),
       so the caller will not try to release (medium) */
    return S_OK; // to be more compatible with Win10-Explorer and DOCs.
  }
  return hres;
  } catch(...) { return E_FAIL; }
}



HRESULT CDataObject::SetData2(const FORMATETC *etc, const STGMEDIUM *medium)
{
  // PRF3("== CDataObject::SetData()");

  HRESULT hres = S_OK;

  if (etc->cfFormat == 0)
    return DV_E_FORMATETC;
  if (etc->tymed != TYMED_HGLOBAL)
    return E_NOTIMPL; // DV_E_TYMED;
  if (etc->dwAspect != DVASPECT_CONTENT)
    return E_NOTIMPL; // DV_E_DVASPECT;
  if (medium->tymed != TYMED_HGLOBAL)
    return E_NOTIMPL; // DV_E_TYMED;

  if (!medium->hGlobal)
    return S_OK;

  if (etc->cfFormat == m_Format_7zip_SetTargetFolder)
  {
    DestDirPrefix_FromTarget.Empty();
    m_DestDirPrefix_FromTarget_WasSet = true;
  }
  else if (etc->cfFormat == m_Format_7zip_SetTransfer)
    m_Transfer_WasSet = false;

  const size_t size = GlobalSize(medium->hGlobal);
  // GlobalLock() can return NULL, if memory block has a zero size
  if (size == 0)
    return S_OK;
  const void *src = (const Byte *)GlobalLock(medium->hGlobal);
  if (!src)
    return E_FAIL;

  PRF_(PrintFormat_AndData("SetData", etc->cfFormat, src, size))

  if (etc->cfFormat == m_Format_7zip_SetTargetFolder)
  {
    /* this is our registered k_Format_7zip_SetTargetFolder format.
       so it's call from 7-zip's CDropTarget */
    /* 7-zip's CDropTarget calls SetData() for m_Format_7zip_SetTargetFolder
       with (release == FALSE) */
    const size_t num = size / sizeof(wchar_t);
    if (size != num * sizeof(wchar_t))
      return E_FAIL;
    // if (num == 0) return S_OK;
    // GlobalLock() can return NULL, if memory block has a zero-byte size
    const wchar_t *s = (const wchar_t *)src;
    UString &dest = DestDirPrefix_FromTarget;
    for (size_t i = 0; i < num; i++)
    {
      const wchar_t c = s[i];
      if (c == 0)
        break;
      dest += c;
    }
    // PRF_(PrintFormat_AndData("SetData", etc->cfFormat, src, size))
    PRF3_W(DestDirPrefix_FromTarget);
  }
  else if (etc->cfFormat == m_Format_7zip_SetTransfer)
  {
    /* 7-zip's CDropTarget calls SetData() for m_Format_7zip_SetTransfer
       with (release == FALSE) */
    if (size < sizeof(CDataObject_SetTransfer))
      return E_FAIL;
    const CDataObject_SetTransfer *t = (const CDataObject_SetTransfer *)src;
    if (!t->Check())
      return E_FAIL;
    m_Transfer = *t;
    if (t->Target.FuncType != k_DragTargetMode_Leave)
      m_Transfer_WasSet = true;
    bool needProcessBySource = !DestDirPrefix_FromTarget.IsEmpty();
    if (t->Target.FuncType == k_DragTargetMode_Drop_Begin)
    {
      if (t->Target.Cmd_Type != NDragMenu::k_Copy_Base
          // || t->Target.Cmd_Effect != DROPEFFECT_COPY
          )
        needProcessBySource = false;
    }
    if (t->Target.FuncType == k_DragTargetMode_Drop_End)
    {
      if (t->Target.Flags & k_TargetFlags_MustBeProcessedBySource)
        needProcessBySource = true;
      else if (t->Target.Flags & k_TargetFlags_WasProcessed)
        needProcessBySource = false;
    }
    DoNotProcessInTarget = needProcessBySource;
  }
  else
  {
    // SetData() from Explorer Target:
    if (etc->cfFormat == m_Format_PerformedDropEffect)
    {
      m_PerformedDropEffect_WasSet = false;
      if (size == sizeof(DWORD))
      {
        m_PerformedDropEffect = *(const DWORD *)src;
        m_PerformedDropEffect_WasSet = true;
      }
    }
    else if (etc->cfFormat == m_Format_LogicalPerformedDropEffect)
    {
      m_LogicalPerformedDropEffect_WasSet = false;
      if (size == sizeof(DWORD))
      {
        m_LogicalPerformedDropEffect = *(const DWORD *)src;
        m_LogicalPerformedDropEffect_WasSet = true;
      }
    }
    else if (etc->cfFormat == m_Format_DropDescription)
    {
      // drop description contains only name of dest folder without full path
      #ifdef SHOW_DEBUG_DRAG
      if (size == sizeof(MY_DROPDESCRIPTION))
      {
        // const MY_DROPDESCRIPTION *s = (const MY_DROPDESCRIPTION *)src;
        // PRF3_W(s->szMessage);
        // PRF3_W(s->szInsert);
      }
      #endif
    }
    else if (etc->cfFormat == m_Format_TargetCLSID)
    {
      // it's called after call QueryContinueDrag() (keyState & MK_LBUTTON) == 0
      // Shell File System Folder (explorer) guid: F3364BA0-65B9-11CE-A9BA-00AA004AE837
      #ifdef SHOW_DEBUG_DRAG
      if (size == 16)
      {
        PrintFormat_GUIDToStringW((const Byte *)src);
      }
      #endif
    }
    else if (etc->cfFormat == m_Format_DisableDragText)
    {
      // (size == 4) (UInt32 value)
      //    value==0 : if drag to folder item or folder
      //    value==1 : if drag to file or non list_view */
    }
    else if (
        etc->cfFormat == m_Format_IsShowingLayered ||
        etc->cfFormat == m_Format_IsShowingText)
    {
      // (size == 4) (UInt32 value) value==0 :
    }
    else
      hres = DV_E_FORMATETC;
    // hres = E_NOTIMPL; // for debug
    // hres = DV_E_FORMATETC; // for debug
  }

  GlobalUnlock(medium->hGlobal);
  return hres;
}



static HGLOBAL DuplicateGlobalMem(HGLOBAL srcGlobal)
{
  /* GlobalSize() returns 0: If the specified handle
     is not valid or if the object has been discarded */
  const SIZE_T size = GlobalSize(srcGlobal);
  if (size == 0)
    return NULL;
  // GlobalLock() can return NULL, if memory block has a zero-byte size
  const void *src = GlobalLock(srcGlobal);
  if (!src)
    return NULL;
  HGLOBAL destGlobal = GlobalAlloc(GHND | GMEM_SHARE, size);
  if (destGlobal)
  {
    void *dest = GlobalLock(destGlobal);
    if (!dest)
    {
      GlobalFree(destGlobal);
      destGlobal = NULL;
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


static bool Medium_CopyFrom(LPSTGMEDIUM medium, const void *data, size_t size)
{
  medium->tymed = TYMED_NULL;
  medium->pUnkForRelease = NULL;
  medium->hGlobal = NULL;
  const HGLOBAL global = GlobalAlloc(GHND | GMEM_SHARE, size);
  if (!global)
    return false;
  void *dest = GlobalLock(global);
  if (!dest)
  {
    GlobalFree(global);
    return false;
  }
  memcpy(dest, data, size);
  GlobalUnlock(global);
  medium->hGlobal = global;
  medium->tymed = TYMED_HGLOBAL;
  return true;
}


Z7_COMWF_B CDataObject::GetData(LPFORMATETC etc, LPSTGMEDIUM medium)
{
  try {
  PRF_(PrintFormat("-- GetData", etc->cfFormat))

  medium->tymed = TYMED_NULL;
  medium->pUnkForRelease = NULL;
  medium->hGlobal = NULL;

  if (NeedCall_Copy && !Copy_WasCalled)
    CopyFromPanelTo_Folder();

  // PRF3("+ CDataObject::GetData");
  // PrintFormat(etc->cfFormat);
  HGLOBAL global;
  RINOK(QueryGetData(etc))

  /*
  if (etc->cfFormat == m_Format_FileOpFlags)
    global = m_hGlobal_FileOpFlags;
  else if (etc->cfFormat == m_Format_PreferredDropEffect)
  {
    // Explorer requests PreferredDropEffect only if Move/Copy selection is possible:
    //   Shift is not pressed and Ctrl is not pressed
    PRF3("------ CDataObject::GetData() PreferredDropEffect");
    global = m_hGlobal_PreferredDropEffect;
  }
  else
  */
  if (etc->cfFormat == m_Etc.cfFormat) // CF_HDROP
    global = UsePreGlobal ? m_hGlobal_HDROP_Pre : m_hGlobal_HDROP_Final;
  else if (etc->cfFormat == m_Format_7zip_GetTransfer)
  {
    CDataObject_GetTransfer transfer;
    if (m_DestDirPrefix_FromTarget_WasSet)
    {
      transfer.Flags |= k_SourceFlags_SetTargetFolder;
    }
    if (!DestDirPrefix_FromTarget.IsEmpty())
    {
      transfer.Flags |= k_SourceFlags_SetTargetFolder_NonEmpty;
    }
    if (IsTempFiles)
    {
      transfer.Flags |= k_SourceFlags_TempFiles;
      transfer.Flags |= k_SourceFlags_WaitFinish;
      transfer.Flags |= k_SourceFlags_NeedExtractOpToFs;
      if (UsePreGlobal)
        transfer.Flags |= k_SourceFlags_NamesAreParent;
    }
    else
      transfer.Flags |= k_SourceFlags_DoNotWaitFinish;

    if (IsRightButton)
      transfer.Flags |= k_SourceFlags_RightButton;
    else
      transfer.Flags |= k_SourceFlags_LeftButton;

    if (DoNotProcessInTarget)
      transfer.Flags |= k_SourceFlags_DoNotProcessInTarget;
    if (Copy_WasCalled)
      transfer.Flags |= k_SourceFlags_Copy_WasCalled;

    if (Medium_CopyFrom(medium, &transfer, sizeof(transfer)))
      return S_OK;
    return E_OUTOFMEMORY;
  }
  else
    return DV_E_FORMATETC;

  if (!global)
    return DV_E_FORMATETC;
  medium->tymed = m_Etc.tymed;
  medium->hGlobal = DuplicateGlobalMem(global);
  if (!medium->hGlobal)
    return E_OUTOFMEMORY;
  return S_OK;
  } catch(...) { return E_FAIL; }
}

Z7_COMWF_B CDataObject::GetDataHere(LPFORMATETC /* etc */, LPSTGMEDIUM /* medium */)
{
  PRF3("CDataObject::GetDataHere()");
  // Seems Windows doesn't call it, so we will not implement it.
  return E_UNEXPECTED;
}


/*
  IDataObject::QueryGetData() Determines whether the data object is capable of
  rendering the data as specified. Objects attempting a paste or drop
  operation can call this method before calling IDataObject::GetData
  to get an indication of whether the operation may be successful.

  The client of a data object calls QueryGetData to determine whether
  passing the specified FORMATETC structure to a subsequent call to
  IDataObject::GetData is likely to be successful.

  we check Try_QueryGetData with CF_HDROP
*/

Z7_COMWF_B CDataObject::QueryGetData(LPFORMATETC etc)
{
  PRF3("-- CDataObject::QueryGetData()");
  if (    etc->cfFormat == m_Etc.cfFormat // CF_HDROP
      ||  etc->cfFormat == m_Format_7zip_GetTransfer
      // || (etc->cfFormat == m_Format_FileOpFlags && (HGLOBAL)m_hGlobal_FileOpFlags)
      // || (etc->cfFormat == m_Format_PreferredDropEffect && (HGLOBAL)m_hGlobal_PreferredDropEffect)
      )
  {
  }
  else
    return DV_E_FORMATETC;
  if (etc->dwAspect != m_Etc.dwAspect)
    return DV_E_DVASPECT;
  /* GetData(): It is possible to specify more than one medium by using the Boolean OR
     operator, allowing the method to choose the best medium among those specified. */
  if ((etc->tymed & m_Etc.tymed) == 0)
    return DV_E_TYMED;
  return S_OK;
}

Z7_COMWF_B CDataObject::EnumFormatEtc(DWORD direction, LPENUMFORMATETC FAR* enumFormatEtc)
{
  // we don't enumerate for DATADIR_SET. Seems it can work without it.
  if (direction != DATADIR_GET)
    return E_NOTIMPL;
  // we don't enumerate for m_Format_FileOpFlags also. Seems it can work without it.
  return CreateEnumFormatEtc(1, &m_Etc, enumFormatEtc);
}



////////////////////////////////////////////////////////

class CDropSource Z7_final:
  public IDropSource,
  public CMyUnknownImp
{
  Z7_COM_UNKNOWN_IMP_1_MT(IDropSource)
  STDMETHOD(QueryContinueDrag)(BOOL escapePressed, DWORD keyState) Z7_override;
  STDMETHOD(GiveFeedback)(DWORD effect) Z7_override;

  DWORD m_Effect;
public:
  CDataObject *DataObjectSpec;
  CMyComPtr<IDataObject> DataObject;

  HRESULT DragProcessing_HRESULT;
  bool DragProcessing_WasFinished;

  CDropSource():
      m_Effect(DROPEFFECT_NONE),
      // Panel(NULL),
      DragProcessing_HRESULT(S_OK),
      DragProcessing_WasFinished(false)
      {}
};

// static bool g_Debug = 0;


Z7_COMWF_B CDropSource::QueryContinueDrag(BOOL escapePressed, DWORD keyState)
{
  // try {

  /* Determines whether a drag-and-drop operation should be continued, canceled, or completed.
     escapePressed : Indicates whether the Esc key has been pressed
       since the previous call to QueryContinueDrag
       or to DoDragDrop if this is the first call to QueryContinueDrag:
         TRUE  : the end user has pressed the escape key;
         FALSE : it has not been pressed.
     keyState : The current state of the keyboard modifier keys on the keyboard.
      Possible values can be a combination of any of the flags:
      MK_CONTROL, MK_SHIFT, MK_ALT, MK_BUTTON, MK_LBUTTON, MK_MBUTTON, and MK_RBUTTON.
  */
  #ifdef SHOW_DEBUG_DRAG
  {
    AString s ("CDropSource::QueryContinueDrag()");
    s.Add_Space();
    s += "keystate=";
    s.Add_UInt32(keyState);
    PRF4(s);
  }
  #endif

  /*
  if ((keyState & MK_LBUTTON) == 0)
  {
    // PRF4("CDropSource::QueryContinueDrag() (keyState & MK_LBUTTON) == 0");
    g_Debug = true;
  }
  else
  {
    // PRF4("CDropSource::QueryContinueDrag() (keyState & MK_LBUTTON) != 0");
  }
  */

  if (escapePressed)
  {
    // The drag operation should be canceled with no drop operation occurring.
    DragProcessing_WasFinished = true;
    DragProcessing_HRESULT = DRAGDROP_S_CANCEL;
    return DRAGDROP_S_CANCEL;
  }

  if (DragProcessing_WasFinished)
    return DragProcessing_HRESULT;

  if ((keyState & MK_RBUTTON) != 0)
  {
    if (!DataObjectSpec->IsRightButton)
    {
      DragProcessing_WasFinished = true;
      DragProcessing_HRESULT = DRAGDROP_S_CANCEL;
      return DRAGDROP_S_CANCEL;
    }
    return S_OK;
  }

  if ((keyState & MK_LBUTTON) != 0)
  {
    if (DataObjectSpec->IsRightButton)
    {
      DragProcessing_WasFinished = true;
      DragProcessing_HRESULT = DRAGDROP_S_CANCEL;
      return DRAGDROP_S_CANCEL;
    }
    /* The drag operation should continue. This result occurs if no errors are detected,
       the mouse button starting the drag-and-drop operation has not been released,
       and the Esc key has not been detected. */
    return S_OK;
  }
  {
    // the mouse button starting the drag-and-drop operation has been released.

    /* Win10 probably calls DragOver()/GiveFeedback() just before LBUTTON releasing.
       so m_Effect is effect returned by DropTarget::DragOver()
       just before LBUTTON releasing.
       So here we can use Effect sent to last GiveFeedback() */

    if (m_Effect == DROPEFFECT_NONE)
    {
      DragProcessing_WasFinished = true;
      DragProcessing_HRESULT = DRAGDROP_S_CANCEL;
      // Drop target cannot accept the data. So we cancel drag and drop
      // maybe return DRAGDROP_S_DROP also OK here ?
      // return DRAGDROP_S_DROP; // for debug
      return DRAGDROP_S_CANCEL;
    }

    // we switch to real names for items that will be created in temp folder
    DataObjectSpec->UsePreGlobal = false;
    DataObjectSpec->Copy_HRESULT = S_OK;
    // MoveMode = (((keyState & MK_SHIFT) != 0) && MoveIsAllowed);
    /*
    if (DataObjectSpec->IsRightButton)
      return DRAGDROP_S_DROP;
    */

    if (DataObjectSpec->IsTempFiles)
    {
      if (!DataObjectSpec->DestDirPrefix_FromTarget.IsEmpty())
      {
        /* we know the destination Path.
           So we can copy or extract items later in Source with simpler code. */
        DataObjectSpec->DoNotProcessInTarget = true;
        // return DRAGDROP_S_CANCEL;
      }
      else
      {
        DataObjectSpec->NeedCall_Copy = true;
        /*
        if (Copy_HRESULT != S_OK || !Messages.IsEmpty())
        {
          DragProcessing_WasFinished = true;
          DragProcessing_HRESULT = DRAGDROP_S_CANCEL;
          return DRAGDROP_S_CANCEL;
        }
        */
      }
    }
    DragProcessing_HRESULT = DRAGDROP_S_DROP;
    DragProcessing_WasFinished = true;
    return DRAGDROP_S_DROP;
  }
  // } catch(...) { return E_FAIL; }
}


Z7_COMWF_B CDropSource::GiveFeedback(DWORD effect)
{
  // PRF3("CDropSource::GiveFeedback");
  /* Enables a source application to give visual feedback to the end user
     during a drag-and-drop operation by providing the DoDragDrop function
     with an enumeration value specifying the visual effect.
  in (effect):
     The DROPEFFECT value returned by the most recent call to
        IDropTarget::DragEnter,
        IDropTarget::DragOver,
     or DROPEFFECT_NONE after IDropTarget::DragLeave.
    0: DROPEFFECT_NONE
    1: DROPEFFECT_COPY
    2: DROPEFFECT_MOVE
    4: DROPEFFECT_LINK
    0x80000000: DROPEFFECT_SCROLL
    The dwEffect parameter can include DROPEFFECT_SCROLL, indicating that the
    source should put up the drag-scrolling variation of the appropriate pointer.
  */
  m_Effect = effect;

 #ifdef SHOW_DEBUG_DRAG
  AString w ("GiveFeedback effect=");
  if (effect & DROPEFFECT_SCROLL)
    w += " SCROLL ";
  w.Add_UInt32(effect & ~DROPEFFECT_SCROLL);
  // if (g_Debug)
  PRF4(w);
 #endif

  /* S_OK : no special drag and drop cursors.
            Maybe it's for case where we created custom custom cursors.
     DRAGDROP_S_USEDEFAULTCURSORS: Indicates successful completion of the method,
       and requests OLE to update the cursor using the OLE-provided default cursors. */
  // return S_OK; // for debug
  return DRAGDROP_S_USEDEFAULTCURSORS;
}



/*
static bool Global_SetUInt32(NMemory::CGlobal &hg, const UInt32 v)
{
  if (!hg.Alloc(GHND | GMEM_SHARE, sizeof(v)))
    return false;
  NMemory::CGlobalLock dropLock(hg);
  *(UInt32 *)dropLock.GetPointer() = v;
  return true;
}
*/

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
    for (i = 0; i < namesA.Size(); i++)
      totalLen += namesA[i].Len() + 1;

    if (!hgDrop.Alloc(GHND | GMEM_SHARE, totalLen * sizeof(CHAR) + sizeof(DROPFILES)))
      return false;

    NMemory::CGlobalLock dropLock(hgDrop);
    DROPFILES *dropFiles = (DROPFILES *)dropLock.GetPointer();
    if (!dropFiles)
      return false;
    dropFiles->fNC = FALSE;
    dropFiles->pt.x = 0;
    dropFiles->pt.y = 0;
    dropFiles->pFiles = sizeof(DROPFILES);
    dropFiles->fWide = FALSE;
    CHAR *p = (CHAR *) (void *) ((BYTE *)dropFiles + sizeof(DROPFILES));
    for (i = 0; i < namesA.Size(); i++)
    {
      const AString &s = namesA[i];
      const unsigned fullLen = s.Len() + 1;
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
    DROPFILES *dropFiles = (DROPFILES *)dropLock.GetPointer();
    if (!dropFiles)
      return false;
    /* fNC:
        TRUE  : pt specifies the screen coordinates of a point in a window's nonclient area.
        FALSE : pt specifies the client coordinates of a point in the client area.
    */
    dropFiles->fNC = FALSE;
    dropFiles->pt.x = 0;
    dropFiles->pt.y = 0;
    dropFiles->pFiles = sizeof(DROPFILES);
    dropFiles->fWide = TRUE;
    WCHAR *p = (WCHAR *) (void *) ((BYTE *)dropFiles + sizeof(DROPFILES));
    for (i = 0; i < names.Size(); i++)
    {
      const UString &s = names[i];
      const unsigned fullLen = s.Len() + 1;
      MyStringCopy(p, (const WCHAR *)s);
      p += fullLen;
      totalLen -= fullLen;
    }
    *p = 0;
  }
  // if (totalLen != 1) return false;
  return true;
}


void CPanel::OnDrag(LPNMLISTVIEW /* nmListView */, bool isRightButton)
{
  PRF("CPanel::OnDrag");
  if (!DoesItSupportOperations())
    return;

  CDisableTimerProcessing disableTimerProcessing2(*this);

  CRecordVector<UInt32> indices;
  Get_ItemIndices_Operated(indices);
  if (indices.Size() == 0)
    return;

  // CSelectedState selState;
  // SaveSelectedState(selState);

  const bool isFSFolder = IsFSFolder();
  // why we don't allow drag with rightButton from archive?
  if (!isFSFolder && isRightButton)
    return;

  UString dirPrefix;
  CTempDir tempDirectory;

  CDataObject *dataObjectSpec = new CDataObject;
  CMyComPtr<IDataObject> dataObject = dataObjectSpec;
  dataObjectSpec->IsRightButton = isRightButton;

  {
    /* we can change confirmation mode and another options.
       Explorer target requests that FILEOP_FLAGS value. */
    /*
    const FILEOP_FLAGS fopFlags =
        FOF_NOCONFIRMATION
      | FOF_NOCONFIRMMKDIR
      | FOF_NOERRORUI
      | FOF_SILENT;
      // | FOF_SIMPLEPROGRESS; // it doesn't work as expected in Win10
    Global_SetUInt32(dataObjectSpec->m_hGlobal_FileOpFlags, fopFlags);
    // dataObjectSpec->m_hGlobal_FileOpFlags.Free(); // for debug : disable these options
    */
  }
  {
    /* we can change Preferred DropEffect.
       Explorer target requests that FILEOP_FLAGS value. */
    /*
    const DWORD effect = DROPEFFECT_MOVE; // DROPEFFECT_COPY;
    Global_SetUInt32(dataObjectSpec->m_hGlobal_PreferredDropEffect, effect);
    */
  }
  if (isFSFolder)
  {
    dirPrefix = GetFsPath(); // why this in 22.01 ?
    dataObjectSpec->UsePreGlobal = false;
    // dataObjectSpec->IsTempFiles = false;
  }
  else
  {
    if (!tempDirectory.Create(kTempDirPrefix))
    {
      MessageBox_Error(L"Can't create temp folder");
      return;
    }
    dirPrefix = fs2us(tempDirectory.GetPath());
    {
      UStringVector names;
      names.Add(dirPrefix);
      dataObjectSpec->IsTempFiles = true;
      dataObjectSpec->UsePreGlobal = true;
      if (!CopyNamesToHGlobal(dataObjectSpec->m_hGlobal_HDROP_Pre, names))
        return;
    }
    NFile::NName::NormalizeDirPathPrefix(dirPrefix);
    /*
    {
      FString path2 = dirPrefix;
      path2 += "1.txt";
      CopyFileW(L"C:\\1\\1.txt", path2, FALSE);
    }
    */
  }

  {
    UStringVector names;
    // names variable is     USED for drag and drop from 7-zip to Explorer or to 7-zip archive folder.
    // names variable is NOT USED for drag and drop from 7-zip to 7-zip File System folder.
    FOR_VECTOR (i, indices)
    {
      const UInt32 index = indices[i];
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
      names.Add(dirPrefix + s);
    }
    if (!CopyNamesToHGlobal(dataObjectSpec->m_hGlobal_HDROP_Final, names))
      return;
  }

  CDropSource *dropSourceSpec = new CDropSource;
  CMyComPtr<IDropSource> dropSource = dropSourceSpec;
  dataObjectSpec->Panel = this;
  dataObjectSpec->Indices = indices;
  dataObjectSpec->SrcDirPrefix_Temp = dirPrefix;

  dropSourceSpec->DataObjectSpec = dataObjectSpec;
  dropSourceSpec->DataObject = dataObjectSpec;


  /*
  CTime - file creation timestamp.
  There are two operations in Windows with Drag and Drop:
    COPY_OPERATION : icon with    Plus sign : CTime will be set as current_time.
    MOVE_OPERATION : icon without Plus sign : CTime will be preserved.

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
    2) it preserves CTime

  Some another programs support only COPY_OPERATION.
  So we can use (DROPEFFECT_COPY | DROPEFFECT_MOVE)

  Also another program can return from DoDragDrop() before
  files using. But we delete temp folder after DoDragDrop(),
  and another program can't open input files in that case.

  We create objects:
    IDropSource *dropSource
    IDataObject *dataObject
  if DropTarget is 7-Zip window, then 7-Zip's
    IDropTarget::DragOver() sets DestDirPrefix_FromTarget in IDataObject.
  and
    IDropSource::QueryContinueDrag() sets DoNotProcessInTarget, if DestDirPrefix_FromTarget is not empty.
  So we can detect destination path after DoDragDrop().
  Now we don't know any good way to detect destination path for D&D to Explorer.
  */

  /*
  DWORD effectsOK = DROPEFFECT_COPY;
  if (moveIsAllowed)
    effectsOK |= DROPEFFECT_MOVE;
  */
  const bool moveIsAllowed = isFSFolder;
  _panelCallback->DragBegin();
  PRF("=== DoDragDrop()");
  DWORD effect = 0;
  // 18.04: was changed
  const DWORD effectsOK = DROPEFFECT_MOVE | DROPEFFECT_COPY;
  // effectsOK |= (1 << 8); // for debug
  HRESULT res = ::DoDragDrop(dataObject, dropSource, effectsOK, &effect);
  PRF("=== After DoDragDrop()");
  _panelCallback->DragEnd();

  /*
    Win10 drag and drop to Explorer:
      DoDragDrop() output variables:
       for MOVE operation:
       {
         effect == DROPEFFECT_NONE;
         dropSourceSpec->m_PerformedDropEffect == DROPEFFECT_MOVE;
       }
       for COPY operation:
       {
         effect == DROPEFFECT_COPY;
         dropSourceSpec->m_PerformedDropEffect == DROPEFFECT_COPY;
       }
    DOCs: The source inspects the two values that can be returned by the target.
       If both are set to DROPEFFECT_MOVE, it completes the unoptimized move
       by deleting the original data. Otherwise, the target did an optimized
       move and the original data has been deleted.

    We didn't see "unoptimized move" case (two values of DROPEFFECT_MOVE),
    where we still need to delete source files.
    So we don't delete files after DoDragDrop().

    Also DOCs say for "optimized move":
      The target also calls the data object's IDataObject::SetData method and passes
      it a CFSTR_PERFORMEDDROPEFFECT format identifier set to DROPEFFECT_NONE.
    but actually in Win10 we always have
      (dropSourceSpec->m_PerformedDropEffect == DROPEFFECT_MOVE)
    for any MOVE operation.
  */

  const bool canceled = (res == DRAGDROP_S_CANCEL);

  CDisableNotify disableNotify(*this);

  if (res == DRAGDROP_S_DROP)
  {
    /* DRAGDROP_S_DROP is returned. It means that
         - IDropTarget::Drop() was called,
         - IDropTarget::Drop() returned (ret_code >= 0)
    */
    res = dataObjectSpec->Copy_HRESULT;
    bool need_Process = dataObjectSpec->DoNotProcessInTarget;
    if (dataObjectSpec->m_Transfer_WasSet)
    {
      if (dataObjectSpec->m_Transfer.Target.FuncType == k_DragTargetMode_Drop_End)
      {
        if (dataObjectSpec->m_Transfer.Target.Flags & k_TargetFlags_MustBeProcessedBySource)
          need_Process = true;
      }
    }

    if (need_Process)
      if (!dataObjectSpec->DestDirPrefix_FromTarget.IsEmpty())
      {
        if (!NFile::NName::IsAltStreamPrefixWithColon(dataObjectSpec->DestDirPrefix_FromTarget))
          NFile::NName::NormalizeDirPathPrefix(dataObjectSpec->DestDirPrefix_FromTarget);
        CCopyToOptions options;
        options.folder = dataObjectSpec->DestDirPrefix_FromTarget;
        // if MOVE is not allowed, we just use COPY operation
        /* it was 7-zip's Target that set non-empty dataObjectSpec->DestDirPrefix_FromTarget.
           it means that target didn't completed operation,
           and we can use (effect) value returned by target via DoDragDrop().
           as indicator of type of operation
        */
        // options.moveMode = (moveIsAllowed && effect == DROPEFFECT_MOVE) // before v23.00:
        options.moveMode = moveIsAllowed;
        if (moveIsAllowed)
        {
          if (dataObjectSpec->m_Transfer_WasSet)
            options.moveMode = (
              dataObjectSpec->m_Transfer.Target.Cmd_Effect == DROPEFFECT_MOVE);
          else
            options.moveMode = (effect == DROPEFFECT_MOVE);
            // we expect (DROPEFFECT_MOVE) as indicator of move operation for Drag&Drop MOVE ver 22.01.
        }
        res = CopyTo(options, indices, &dataObjectSpec->Messages);
      }
    /*
    if (effect & DROPEFFECT_MOVE)
      RefreshListCtrl(selState);
    */
  }
  else
  {
    // we ignore E_UNEXPECTED that is returned if we drag file to printer
    if (res != DRAGDROP_S_CANCEL
        && res != S_OK
        && res != E_UNEXPECTED)
      MessageBox_Error_HRESULT(res);
    res = dataObjectSpec->Copy_HRESULT;
  }

  if (!dataObjectSpec->Messages.IsEmpty())
  {
    CMessagesDialog messagesDialog;
    messagesDialog.Messages = &dataObjectSpec->Messages;
    messagesDialog.Create((*this));
  }

  if (res != S_OK && res != E_ABORT)
  {
    // we restore Notify before MessageBox_Error_HRESULT. So we will see files selection
    disableNotify.Restore();
    // SetFocusToList();
    MessageBox_Error_HRESULT(res);
  }
  if (res == S_OK && dataObjectSpec->Messages.IsEmpty() && !canceled)
    KillSelection();
}





CDropTarget::CDropTarget():
      m_IsRightButton(false),
      m_GetTransfer_WasSuccess(false),
      m_DropIsAllowed(false),
      m_PanelDropIsAllowed(false),
      // m_DropHighlighted_SelectionIndex(-1),
      // m_SubFolderIndex(-1),
      m_Panel(NULL),
      m_IsAppTarget(false),
      m_TargetPath_WasSent_ToDataObject(false),
      m_TargetPath_NonEmpty_WasSent_ToDataObject(false),
      m_Transfer_WasSent_ToDataObject(false),
      App(NULL),
      SrcPanelIndex(-1),
      TargetPanelIndex(-1)
{
  m_Format_7zip_SetTargetFolder = RegisterClipboardFormat(k_Format_7zip_SetTargetFolder);
  m_Format_7zip_SetTransfer     = RegisterClipboardFormat(k_Format_7zip_SetTransfer);
  m_Format_7zip_GetTransfer     = RegisterClipboardFormat(k_Format_7zip_GetTransfer);

  m_ProcessId = GetCurrentProcessId();
  // m_TransactionId = ((UInt64)m_ProcessId << 32) + 1;
  // ClearState();
}

// clear internal state
void CDropTarget::ClearState()
{
  m_DataObject.Release();
  m_SourcePaths.Clear();

  m_IsRightButton = false;

  m_GetTransfer_WasSuccess = false;
  m_DropIsAllowed = false;

  m_PanelDropIsAllowed = false;
  // m_SubFolderIndex = -1;
  // m_DropHighlighted_SubFolderName.Empty();
  m_Panel = NULL;
  m_IsAppTarget = false;
  m_TargetPath_WasSent_ToDataObject = false;
  m_TargetPath_NonEmpty_WasSent_ToDataObject = false;
  m_Transfer_WasSent_ToDataObject = false;
}

/*
  IDataObject::QueryGetData() Determines whether the data object is capable of
  rendering the data as specified. Objects attempting a paste or drop
  operation can call this method before calling IDataObject::GetData
  to get an indication of whether the operation may be successful.

  The client of a data object calls QueryGetData to determine whether
  passing the specified FORMATETC structure to a subsequent call to
  IDataObject::GetData is likely to be successful.

  We check Try_QueryGetData with CF_HDROP
*/
/*
void CDropTarget::Try_QueryGetData(IDataObject *dataObject)
{
  FORMATETC etc = { CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
  m_DropIsAllowed = (dataObject->QueryGetData(&etc) == S_OK);
}
*/

static void ListView_SetItemState_DropHighlighted(
    NControl::CListView &listView, int index, bool highlighted)
{
  // LVIS_DROPHILITED : The item is highlighted as a drag-and-drop target
  /*
  LVITEM item;
  item.mask = LVIF_STATE;
  item.iItem = index;
  item.iSubItem = 0;
  item.state = enable ? LVIS_DROPHILITED : 0;
  item.stateMask = LVIS_DROPHILITED;
  item.pszText = NULL;
  listView.SetItem(&item);
  */
  listView.SetItemState(index, highlighted ? LVIS_DROPHILITED : 0, LVIS_DROPHILITED);
}

// Removes DropHighlighted state in ListView item, if it was set before
void CDropTarget::RemoveSelection()
{
  if (m_Panel)
  {
    m_Panel->m_DropHighlighted_SubFolderName.Empty();
    if (m_Panel->m_DropHighlighted_SelectionIndex >= 0)
    {
      ListView_SetItemState_DropHighlighted(m_Panel->_listView,
          m_Panel->m_DropHighlighted_SelectionIndex, false);
      m_Panel->m_DropHighlighted_SelectionIndex = -1;
    }
  }
}

#ifdef UNDER_CE
#define ChildWindowFromPointEx(hwndParent, pt, uFlags) ChildWindowFromPoint(hwndParent, pt)
#endif


/*
   PositionCursor() function sets m_Panel under cursor drop, and
   m_SubFolderIndex/m_DropHighlighted_SubFolderName, if drop to some folder in Panel list.
*/
/*
PositionCursor() uses as input variables:
  m_DropIsAllowed must be set before PositionCursor()
  if (m_DropHighlighted_SelectionIndex >= 0 && m_Panel) it uses m_Panel and removes previous selection
PositionCursor() sets
  m_PanelDropIsAllowed
  m_Panel
  m_IsAppTarget
  m_SubFolderIndex
  m_DropHighlighted_SubFolderName
  m_DropHighlighted_SelectionIndex
*/
void CDropTarget::PositionCursor(const POINTL &ptl)
{
  RemoveSelection();

  // m_SubFolderIndex = -1;
  // m_DropHighlighted_SubFolderName.Empty();
  m_IsAppTarget = true;
  m_Panel = NULL;
  m_PanelDropIsAllowed = false;

  if (!m_DropIsAllowed)
    return;

  POINT pt;
  pt.x = ptl.x;
  pt.y = ptl.y;
  {
    POINT pt2 = pt;
    if (App->_window.ScreenToClient(&pt2))
      for (unsigned i = 0; i < kNumPanelsMax; i++)
        if (App->IsPanelVisible(i))
        {
          CPanel *panel = &App->Panels[i];
          if (panel->IsEnabled())
          if (::ChildWindowFromPointEx(App->_window, pt2,
              CWP_SKIPINVISIBLE | CWP_SKIPDISABLED) == (HWND)*panel)
          {
            m_Panel = panel;
            m_IsAppTarget = false;
            if ((int)i == SrcPanelIndex)
              return; // we don't allow to drop to source panel
            break;
          }
        }
  }

  m_PanelDropIsAllowed = true;

  if (!m_Panel)
  {
    if (TargetPanelIndex >= 0)
      m_Panel = &App->Panels[TargetPanelIndex];
    // we don't need to find item in panel
    return;
  }

  // we will try to find and highlight drop folder item in listView under cursor
  /*
  m_PanelDropIsAllowed = m_Panel->DoesItSupportOperations();
  if (!m_PanelDropIsAllowed)
    return;
  */
  /* now we don't allow drop to subfolder under cursor, if dest panel is archive.
     Another code must be fixed for that case, where we must use m_SubFolderIndex/m_DropHighlighted_SubFolderName */
  if (!m_Panel->IsFsOrPureDrivesFolder())
    return;

  if (::WindowFromPoint(pt) != (HWND)m_Panel->_listView)
    return;

  LVHITTESTINFO info;
  m_Panel->_listView.ScreenToClient(&pt);
  info.pt = pt;
  const int index = ListView_HitTest(m_Panel->_listView, &info);
  if (index < 0)
    return;
  const unsigned realIndex = m_Panel->GetRealItemIndex(index);
  if (realIndex == kParentIndex)
    return;
  if (!m_Panel->IsItem_Folder(realIndex))
    return;
  // m_SubFolderIndex = (int)realIndex;
  m_Panel->m_DropHighlighted_SubFolderName = m_Panel->GetItemName(realIndex);
  ListView_SetItemState_DropHighlighted(m_Panel->_listView, index, true);
  m_Panel->m_DropHighlighted_SelectionIndex = index;
}


/* returns true, if !m_IsAppTarget
   and target is FS folder or altStream folder
*/

UInt32 CDropTarget::GetFolderType() const
{
  if (m_IsAppTarget || !m_Panel)
    return k_FolderType_None;
  if (m_Panel->IsFSFolder() ||
      (m_Panel->IsFSDrivesFolder()
       && m_Panel->m_DropHighlighted_SelectionIndex >= 0))
    return k_FolderType_Fs;
  if (m_Panel->IsAltStreamsFolder())
    return k_FolderType_AltStreams;
  if (m_Panel->IsArcFolder())
    return k_FolderType_Archive;
  return k_FolderType_Unknown;
}

bool CDropTarget::IsFsFolderPath() const
{
  if (m_IsAppTarget || !m_Panel)
    return false;
  if (m_Panel->IsFSFolder())
    return true;
  if (m_Panel->IsAltStreamsFolder())
    return true;
  return m_Panel->IsFSDrivesFolder() &&
      m_Panel->m_DropHighlighted_SelectionIndex >= 0;
}


#define INIT_FORMATETC_HGLOBAL(type) { (type), NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL }

static bool DataObject_GetData_GetTransfer(IDataObject *dataObject,
    UINT a_Format_7zip_GetTransfer, CDataObject_GetTransfer &transfer)
{
  FORMATETC etc = INIT_FORMATETC_HGLOBAL((CLIPFORMAT)a_Format_7zip_GetTransfer);
  NCOM::CStgMedium medium;
  const HRESULT res = dataObject->GetData(&etc, &medium);
  if (res != S_OK)
    return false;
  if (medium.tymed != TYMED_HGLOBAL)
    return false;
  const size_t size = GlobalSize(medium.hGlobal);
  if (size < sizeof(transfer))
    return false;
  NMemory::CGlobalLock dropLock(medium.hGlobal);
  const CDataObject_GetTransfer *t = (const CDataObject_GetTransfer *)dropLock.GetPointer();
  if (!t)
    return false;
  if (!t->Check()) // isSetData
    return false;
  transfer = *t;
  return true;
}

/*
  returns true, if all m_SourcePaths[] items are same drive
  as destination drop path in m_Panel
*/
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
  else if (m_Panel->IsFSDrivesFolder()
      && m_Panel->m_DropHighlighted_SelectionIndex >= 0)
  {
    drive = m_Panel->m_DropHighlighted_SubFolderName;
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
  1) if target panel is "7-Zip" FS and any of the 2 cases:
     - Drag from any non "7-Zip" program;
     or
     - Drag from "7-Zip" to non-panel area of "7-Zip".
     We want to create new archive for that operation with "Add to Archive" window.
  2) all another operations work as usual file COPY/MOVE
    - Drag from "7-Zip" FS to "7-Zip" FS.
        COPY/MOVE are supported.
    - Drag to open archive in 7-Zip.
        We want to update archive.
        We replace COPY to MOVE.
    - Drag from "7-Zip" archive to "7-Zip" FS.
        We replace COPY to MOVE.
*/

// we try to repeat Explorer's effects.
// out: 0 - means that use default effect
static DWORD GetEffect_ForKeys(DWORD keyState)
{
  if (keyState & MK_CONTROL)
  {
    if (keyState & MK_ALT)
      return 0;
    if (keyState & MK_SHIFT)
      return DROPEFFECT_LINK; // CONTROL + SHIFT
    return DROPEFFECT_COPY;   // CONTROL
  }
  // no CONTROL
  if (keyState & MK_SHIFT)
  {
    if (keyState & MK_ALT)
      return 0;
    return DROPEFFECT_MOVE; // SHIFT
  }
  // no CONTROL, no SHIFT
  if (keyState & MK_ALT)
    return DROPEFFECT_LINK; // ALT
  return 0;
}


/* GetEffect() uses m_TargetPath_WasSentToDataObject
   to disale MOVE operation, if Source is not 7-Zip
*/
DWORD CDropTarget::GetEffect(DWORD keyState, POINTL /* pt */, DWORD allowedEffect) const
{
  // (DROPEFFECT_NONE == 0)
  if (!m_DropIsAllowed || !m_PanelDropIsAllowed)
    return 0;
  if (!IsFsFolderPath() || !m_TargetPath_WasSent_ToDataObject)
  {
    // we don't allow MOVE, if Target is archive or Source is not 7-Zip
    // disabled for debug:
    // allowedEffect &= ~DROPEFFECT_MOVE;
  }
  DWORD effect;
  {
    effect = GetEffect_ForKeys(keyState);
    if (effect == DROPEFFECT_LINK)
      return 0;
    effect &= allowedEffect;
  }
  if (effect == 0)
  {
    if (allowedEffect & DROPEFFECT_COPY)
      effect = DROPEFFECT_COPY;
    if (allowedEffect & DROPEFFECT_MOVE)
    {
      /* MOVE operation can be optimized. So MOVE is preferred way
         for default action, if Source and Target are at same drive */
      if (IsItSameDrive())
        effect = DROPEFFECT_MOVE;
    }
  }
  return effect;
}


/* returns:
    - target folder path prefix, if target is FS folder
    - empty string, if target is not FS folder
*/
UString CDropTarget::GetTargetPath() const
{
  if (!IsFsFolderPath())
    return UString();
  UString path = m_Panel->GetFsPath();
  if (/* m_SubFolderIndex >= 0 && */
      !m_Panel->m_DropHighlighted_SubFolderName.IsEmpty())
  {
    path += m_Panel->m_DropHighlighted_SubFolderName;
    path.Add_PathSepar();
  }
  return path;
}


/*
if IDropSource is Win10-Explorer
--------------------------------
  As in MS DOCs:
  The source inspects the two (effect) values that can be returned by the target:
    1) SetData(CFSTR_PERFORMEDDROPEFFECT)
    2) returned value  (*effect) by
        CDropTarget::Drop(IDataObject *dataObject, DWORD keyState,
        POINTL pt, DWORD *effect)
  If both are set to DROPEFFECT_MOVE, Explorer completes the unoptimized move by deleting
  the original data.
  // Otherwise, the target did an optimized move and the original data has been deleted.
*/


/*
  Send targetPath from target to dataObject (to Source)
  input: set (enablePath = false) to send empty path
  returns true,  if SetData()         returns S_OK : (source is 7-zip)
  returns false, if SetData() doesn't return  S_OK : (source is Explorer)
*/
bool CDropTarget::SendToSource_TargetPath_enable(IDataObject *dataObject, bool enablePath)
{
  m_TargetPath_NonEmpty_WasSent_ToDataObject = false;
  UString path;
  if (enablePath)
    path = GetTargetPath();
  PRF("CDropTarget::SetPath");
  PRF_W(path);
  if (!dataObject || m_Format_7zip_SetTargetFolder == 0)
    return false;
  FORMATETC etc = INIT_FORMATETC_HGLOBAL((CLIPFORMAT)m_Format_7zip_SetTargetFolder);
  STGMEDIUM medium;
  medium.tymed = etc.tymed;
  medium.pUnkForRelease = NULL;
  const size_t num = path.Len() + 1; // + (1 << 19) // for debug
  medium.hGlobal = GlobalAlloc(GHND | GMEM_SHARE, num * sizeof(wchar_t));
  if (!medium.hGlobal)
    return false;
  // Sleep(1000);
  wchar_t *dest = (wchar_t *)GlobalLock(medium.hGlobal);
  // Sleep(1000);
  bool res = false;
  if (dest)
  {
    MyStringCopy(dest, (const wchar_t *)path);
    GlobalUnlock(medium.hGlobal);
    // OutputDebugString("m_DataObject->SetData");
    const BOOL release = FALSE; // that way is more simple for correct releasing.
        // TRUE; // for debug : is not good for some cases.
    /* If DropSource is Win10-Explorer, dataObject->SetData() returns E_NOTIMPL; */
    const HRESULT hres = dataObject->SetData(&etc, &medium, release);
    // Sleep(1000);
    res = (hres == S_OK);
  }

  ReleaseStgMedium(&medium);
  if (res && !path.IsEmpty())
    m_TargetPath_NonEmpty_WasSent_ToDataObject = true;
  // Sleep(1000);
  return res;
}


void CDropTarget::SendToSource_auto(IDataObject *dataObject,
    const CTargetTransferInfo &info)
{
  /* we try to send target path to Source.
     If Source is 7-Zip, then it will accept k_Format_7zip_SetTargetFolder.
     That sent path will be non-Empty, if this target is FS folder and drop is allowed */
  bool need_Send = false;
  if (   info.FuncType == k_DragTargetMode_Enter
      || info.FuncType == k_DragTargetMode_Over
      || (info.FuncType == k_DragTargetMode_Drop_Begin
        // && targetOp_Cmd != NDragMenu::k_None
        && info.Cmd_Type != NDragMenu::k_Cancel))
  // if (!g_CreateArchive_for_Drag_from_7zip)
      need_Send = m_DropIsAllowed && m_PanelDropIsAllowed && IsFsFolderPath();
  m_TargetPath_WasSent_ToDataObject = SendToSource_TargetPath_enable(dataObject, need_Send);
  SendToSource_TransferInfo(dataObject, info);
}


bool CDropTarget::SendToSource_TransferInfo(IDataObject *dataObject,
    const CTargetTransferInfo &info)
{
  m_Transfer_WasSent_ToDataObject = false;
  PRF("CDropTarget::SendToSource_TransferInfo");

  if (!dataObject || m_Format_7zip_SetTransfer == 0)
    return false;
  FORMATETC etc = INIT_FORMATETC_HGLOBAL((CLIPFORMAT)m_Format_7zip_SetTransfer);
  STGMEDIUM medium;
  medium.tymed = etc.tymed;
  medium.pUnkForRelease = NULL;
  CDataObject_SetTransfer transfer;
  const size_t size = sizeof(transfer); // + (1 << 19) // for debug
  // OutputDebugString("GlobalAlloc");
  medium.hGlobal = GlobalAlloc(GHND | GMEM_SHARE, size);
  // Sleep(1000);
  if (!medium.hGlobal)
    return false;
  // OutputDebugString("GlobalLock");
  void *dest = (wchar_t *)GlobalLock(medium.hGlobal);
  // Sleep(1000);
  bool res = false;
  if (dest)
  {
    transfer.Init();
    transfer.Target = info;

    memcpy(dest, &transfer, sizeof(transfer));
    GlobalUnlock(medium.hGlobal);
    // OutputDebugString("m_DataObject->SetData");
    const BOOL release = FALSE; // that way is more simple for correct releasing.
        // TRUE; // for debug : is not good for some cases
    const HRESULT hres = dataObject->SetData(&etc, &medium, release);
    res = (hres == S_OK);
  }

  ReleaseStgMedium(&medium);
  if (res)
    m_Transfer_WasSent_ToDataObject = true;
  return res;
}


bool CDropTarget::SendToSource_UInt32(IDataObject *dataObject, UINT format, UInt32 value)
{
  PRF("CDropTarget::Send_UInt32 (Performed)");

  if (!dataObject || format == 0)
    return false;
  FORMATETC etc = INIT_FORMATETC_HGLOBAL((CLIPFORMAT)format);
  STGMEDIUM medium;
  medium.tymed = etc.tymed;
  medium.pUnkForRelease = NULL;
  const size_t size = 4;
  medium.hGlobal = GlobalAlloc(GHND | GMEM_SHARE, size);
  if (!medium.hGlobal)
    return false;
  void *dest = GlobalLock(medium.hGlobal);
  bool res = false;
  if (dest)
  {
    *(UInt32 *)dest = value;
    GlobalUnlock(medium.hGlobal);
    // OutputDebugString("m_DataObject->SetData");
    const BOOL release = TRUE;
        // FALSE; // for debug
    /* If DropSource is Win10-Explorer, then (release == FALSE) doesn't work
       and dataObject->SetData() returns E_NOTIMPL;
       So we use release = TRUE; here */
    const HRESULT hres = dataObject->SetData(&etc, &medium, release);
    // we return here without calling ReleaseStgMedium().
    return (hres == S_OK);
    // Sleep(1000);
    /*
      if (we use release = TRUE), we expect that
          - SetData() will release medium, and
          - SetData() will set STGMEDIUM::tymed to (TYMED_NULL = 0).
      but some "incorrect" SetData() implementations can keep STGMEDIUM::tymed unchanged.
      And it's not safe to call ReleaseStgMedium() here for that case,
      because DropSource also could release medium.
      We can reset (medium.tymed = TYMED_NULL) manually here to disable
      unsafe medium releasing in ReleaseStgMedium().
    */
    /*
    if (release)
    {
      medium.tymed = TYMED_NULL;
      medium.pUnkForRelease = NULL;
      medium.hGlobal = NULL;
    }
    res = (hres == S_OK);
    */
  }
  ReleaseStgMedium(&medium);
  return res;
}


void CDropTarget::LoadNames_From_DataObject(IDataObject *dataObject)
{
  // "\\\\.\\" prefix is possible for long names
  m_DropIsAllowed = NShell::DataObject_GetData_HDROP_or_IDLIST_Names(dataObject, m_SourcePaths) == S_OK;
}


Z7_COMWF_B CDropTarget::DragEnter(IDataObject *dataObject, DWORD keyState, POINTL pt, DWORD *effect)
{
  /* *(effect):
       - on input  : value of the dwOKEffects parameter of the DoDragDrop() function.
       - on return : must contain one of the DROPEFFECT flags, which indicates
                     what the result of the drop operation would be.
     (pt): the current cursor coordinates in screen coordinates.
  */
  PRF_(Print_Point("CDropTarget::DragEnter", keyState, pt, *effect))
  try {

  if ((keyState & (MK_RBUTTON | MK_MBUTTON)) != 0)
    m_IsRightButton = true;

  LoadNames_From_DataObject(dataObject);
  // Try_QueryGetData(dataObject);
  // we will use (m_DataObject) later in DragOver() and DragLeave().
  m_DataObject = dataObject;
  // return DragOver(keyState, pt, effect);
  PositionCursor(pt);
  CTargetTransferInfo target;
  target.FuncType = k_DragTargetMode_Enter;
  target.KeyState = keyState;
  target.Point = pt;
  target.OkEffects = *effect;
  SendToSource_Drag(target);

  CDataObject_GetTransfer transfer;
  m_GetTransfer_WasSuccess = DataObject_GetData_GetTransfer(
      dataObject, m_Format_7zip_GetTransfer, transfer);
  if (m_GetTransfer_WasSuccess)
  {
    if (transfer.Flags & k_SourceFlags_LeftButton)
      m_IsRightButton = false;
    else if (transfer.Flags & k_SourceFlags_RightButton)
      m_IsRightButton = true;
  }

  *effect = GetEffect(keyState, pt, *effect);
  return S_OK;
  } catch(...) { return E_FAIL; }
}


Z7_COMWF_B CDropTarget::DragOver(DWORD keyState, POINTL pt, DWORD *effect)
{
  PRF_(Print_Point("CDropTarget::DragOver", keyState, pt, *effect))
  /*
    For efficiency reasons, a data object is not passed in IDropTarget::DragOver.
    The data object passed in the most recent call to IDropTarget::DragEnter
    is available and can be used.

    When IDropTarget::DragOver has completed its operation, the DoDragDrop
    function calls IDropSource::GiveFeedback so the source application can display
    the appropriate visual feedback to the user.
  */
  /*
    we suppose that it's unexpected that (keyState) shows that mouse
    button is not pressed, because such cases will be processed by
    IDropSource::QueryContinueDrag() that returns DRAGDROP_S_DROP or DRAGDROP_S_CANCEL.
    So DragOver() will not be called.
  */

  if ((keyState & MK_LBUTTON) == 0)
  {
    PRF4("CDropTarget::DragOver() (keyState & MK_LBUTTON) == 0");
    // g_Debug = true;
  }

  try {
  /* we suppose that source names were not changed after DragEnter()
     so we don't request GetNames_From_DataObject() for each call of DragOver() */
  PositionCursor(pt);
  CTargetTransferInfo target;
  target.FuncType = k_DragTargetMode_Over;
  target.KeyState = keyState;
  target.Point = pt;
  target.OkEffects = *effect;
  SendToSource_Drag(target);
  *effect = GetEffect(keyState, pt, *effect);
  // *effect = 1 << 8; // for debug
  return S_OK;
  } catch(...) { return E_FAIL; }
}


Z7_COMWF_B CDropTarget::DragLeave()
{
  PRF4("CDropTarget::DragLeave");
  try {
  RemoveSelection();
  // we send empty TargetPath to 7-Zip Source to clear value of TargetPath that was sent before

  CTargetTransferInfo target;
  target.FuncType = k_DragTargetMode_Leave;
  /*
  target.KeyState = 0;
  target.Point = pt;
  pt.x = 0; // -1
  pt.y = 0; // -1
  target.Effect = 0;
  */
  SendToSource_Drag(target);
  ClearState();
  return S_OK;
  } catch(...) { return E_FAIL; }
}


static unsigned Drag_OnContextMenu(int xPos, int yPos, UInt32 cmdFlags);

/*
  We suppose that there was DragEnter/DragOver for same (POINTL pt) before Drop().
  But we can work without DragEnter/DragOver too.
*/
Z7_COMWF_B CDropTarget::Drop(IDataObject *dataObject, DWORD keyState,
    POINTL pt, DWORD *effect)
{
  PRF_(Print_Point("CDropTarget::Drop", keyState, pt, *effect))
  /* Drop() is called after SourceDrop::QueryContinueDrag() returned DRAGDROP_S_DROP.
     So it's possible that Source have done some operations already.
  */
  HRESULT hres = S_OK;
  bool needDrop_by_Source = false;
  DWORD opEffect = DROPEFFECT_NONE;

  try {
  // we don't need m_DataObject reference anymore, because we use local (dataObject)
  m_DataObject.Release();

  /* in normal case : we called LoadNames_From_DataObject() in DragEnter() already.
     But if by some reason DragEnter() was not called,
     we need to call LoadNames_From_DataObject() before PositionCursor().
  */
  if (!m_DropIsAllowed) LoadNames_From_DataObject(dataObject);
  PositionCursor(pt);

  CPanel::CDisableTimerProcessing2 disableTimerProcessing(m_Panel);
  // CDisableNotify disableNotify2(m_Panel);

  UInt32 cmd = NDragMenu::k_None;
  UInt32 cmdEffect = DROPEFFECT_NONE;
  bool menu_WasShown = false;
  if (m_IsRightButton && m_Panel)
  {
    UInt32 flagsMask;
    if (m_Panel->IsArcFolder())
      flagsMask = (UInt32)1 << NDragMenu::k_Copy_ToArc;
    else
    {
      flagsMask = (UInt32)1 << NDragMenu::k_AddToArc;
      if (IsFsFolderPath())
        flagsMask |= (UInt32)1 << NDragMenu::k_Copy_Base;
    }
    // flagsMask |= (UInt32)1 << NDragMenu::k_Cancel;
    const UInt32 cmd32 = Drag_OnContextMenu(pt.x, pt.y, flagsMask);
    cmd = cmd32 & NDragMenu::k_MenuFlags_CmdMask;
    if (cmd32 & NDragMenu::k_MenuFlag_Copy)
      cmdEffect = DROPEFFECT_COPY;
    else if (cmd32 & NDragMenu::k_MenuFlag_Move)
      cmdEffect = DROPEFFECT_MOVE;
    opEffect = cmdEffect;
    menu_WasShown = true;
  }
  else
  {
    opEffect = GetEffect(keyState, pt, *effect);
    if (m_IsAppTarget)
      cmd = NDragMenu::k_AddToArc;
    else if (m_Panel)
    {
      if (IsFsFolderPath())
      {
        const bool is7zip = m_TargetPath_WasSent_ToDataObject;
        bool createNewArchive = false;
        if (is7zip)
          createNewArchive = false; // g_CreateArchive_for_Drag_from_7zip;
        else
          createNewArchive = true; // g_CreateArchive_for_Drag_from_Explorer;

        if (createNewArchive)
          cmd = NDragMenu::k_AddToArc;
        else
        {
          if (opEffect != 0)
            cmd = NDragMenu::k_Copy_Base;
          cmdEffect = opEffect;
        }
      }
      else
      {
        /* if we are inside open archive:
           if archive support operations         -> we will call operations
           if archive doesn't support operations -> we will create new archove
        */
        if (m_Panel->IsArcFolder()
            || m_Panel->DoesItSupportOperations())
        {
          cmd = NDragMenu::k_Copy_ToArc;
          // we don't want move to archive operation here.
          // so we force to DROPEFFECT_COPY.
          if (opEffect != DROPEFFECT_NONE)
            opEffect = DROPEFFECT_COPY;
          cmdEffect = opEffect;
        }
        else
          cmd = NDragMenu::k_AddToArc;
      }
    }
  }

  if (cmd == 0)
    cmd = NDragMenu::k_AddToArc;

  if (cmd == NDragMenu::k_AddToArc)
  {
    opEffect = DROPEFFECT_COPY;
    cmdEffect = DROPEFFECT_COPY;
  }

  if (m_Panel)
  if (cmd == NDragMenu::k_Copy_ToArc)
  {
    const UString title = LangString(IDS_CONFIRM_FILE_COPY);
    UString s = LangString(cmdEffect == DROPEFFECT_MOVE ?
        IDS_MOVE_TO : IDS_COPY_TO);
    s.Add_LF();
    // s += "\'";
    s += m_Panel->_currentFolderPrefix;
    // s += "\'";
    s.Add_LF();
    AddLangString(s, IDS_WANT_TO_COPY_FILES);
    s += " ?";
    const int res = ::MessageBoxW(*m_Panel, s, title, MB_YESNOCANCEL | MB_ICONQUESTION);
    if (res != IDYES)
      cmd = NDragMenu::k_Cancel;
  }

  CTargetTransferInfo target;
  target.FuncType = k_DragTargetMode_Drop_Begin;
  target.KeyState = keyState;
  target.Point = pt;
  target.OkEffects = *effect;
  target.Flags = 0;

  target.Cmd_Effect = cmdEffect;
  target.Cmd_Type = cmd;
  target.FolderType = GetFolderType();

  if (cmd == NDragMenu::k_Cancel)
    target.Flags |= k_TargetFlags_WasCanceled;
  if (menu_WasShown)
    target.Flags |= k_TargetFlags_MenuWasShown;

  SendToSource_auto(dataObject, target);

  CDataObject_GetTransfer transfer;
  m_GetTransfer_WasSuccess = DataObject_GetData_GetTransfer(
      dataObject, m_Format_7zip_GetTransfer, transfer);

  /* The Source (for example, 7-zip) could change file names when drop was confirmed.
     So we must reload source file paths here */
  if (cmd != NDragMenu::k_Cancel)
    LoadNames_From_DataObject(dataObject);

  if (cmd == NDragMenu::k_Cancel)
  {
    opEffect = DROPEFFECT_NONE;
    cmdEffect = DROPEFFECT_NONE;
  }
  else
  {
    if (m_GetTransfer_WasSuccess)
      needDrop_by_Source = ((transfer.Flags & k_SourceFlags_DoNotProcessInTarget) != 0);
    if (!needDrop_by_Source)
    {
      bool moveMode = (cmdEffect == DROPEFFECT_MOVE);
      bool needDrop = false;
      if (m_IsRightButton && m_Panel)
        needDrop = true;
      if (m_DropIsAllowed && m_PanelDropIsAllowed)
      {
        /* if non-empty TargetPath was sent successfully to DataObject,
           then the Source is 7-Zip, and that 7zip-Source can copy to FS operation.
           So we can disable Drop operation here for such case.
        */
        needDrop_by_Source = (cmd != NDragMenu::k_AddToArc
            && m_TargetPath_WasSent_ToDataObject
            && m_TargetPath_NonEmpty_WasSent_ToDataObject);
        needDrop = !(needDrop_by_Source);
      }
      if (needDrop)
      {
        UString path = GetTargetPath();
        if (m_IsAppTarget && m_Panel)
          if (m_Panel->IsFSFolder())
            path = m_Panel->GetFsPath();

        UInt32 sourceFlags = 0;
        if (m_GetTransfer_WasSuccess)
          sourceFlags = transfer.Flags;

        if (menu_WasShown)
          target.Flags |= k_TargetFlags_MenuWasShown;

        target.Flags |= k_TargetFlags_WasProcessed;

        RemoveSelection();
        // disableTimerProcessing.Restore();
        m_Panel->CompressDropFiles(m_SourcePaths, path,
            (cmd == NDragMenu::k_AddToArc),  // createNewArchive,
            moveMode, sourceFlags,
            target.Flags
            );
      }
    }
  } // end of if (cmd != NDragMenu::k_Cancel)
  {
    /* note that, if (we send CFSTR_PERFORMEDDROPEFFECT as DROPEFFECT_MOVE
            and Drop() returns (*effect == DROPEFFECT_MOVE), then
       Win10-Explorer-Source will try to remove files just after Drop() exit.
       But our CompressFiles() could be run without waiting finishing.
       DOCs say, that we must send CFSTR_PERFORMEDDROPEFFECT
         - DROPEFFECT_NONE : for   optimized move
         - DROPEFFECT_MOVE : for unoptimized move.
       But actually Win10-Explorer-Target sends (DROPEFFECT_MOVE) for move operation.
       And it still works as in optimized mode, because "unoptimized" deleting by Source will be performed
       if both conditions are met:
          1) DROPEFFECT_MOVE is sent to (CFSTR_PERFORMEDDROPEFFECT) and
          2) (*effect == DROPEFFECT_MOVE) is returend by Drop().
       We don't want to send DROPEFFECT_MOVE here to protect from
       deleting file by Win10-Explorer.
       We are not sure that allfile fieree processed by move.
    */

    // for debug: we test the case when source tries to delete original files
    // bool res;
    // only CFSTR_PERFORMEDDROPEFFECT affects file removing in Win10-Explorer.
    // res = SendToSource_UInt32(dataObject, RegisterClipboardFormat(CFSTR_LOGICALPERFORMEDDROPEFFECT), DROPEFFECT_MOVE); // for debug
    /* res = */ SendToSource_UInt32(dataObject,
        RegisterClipboardFormat(CFSTR_PERFORMEDDROPEFFECT),
        cmd == NDragMenu::k_Cancel ? DROPEFFECT_NONE : DROPEFFECT_COPY);
    // res = res;
  }
  RemoveSelection();

  target.FuncType = k_DragTargetMode_Drop_End;
  target.Cmd_Type = cmd;
  if (needDrop_by_Source)
    target.Flags |= k_TargetFlags_MustBeProcessedBySource;

  SendToSource_TransferInfo(dataObject, target);
  } catch(...) { hres = E_FAIL; }

  ClearState();
  // *effect |= (1 << 10); // for debug
  // *effect = DROPEFFECT_COPY; // for debug

  /*
    if we return (*effect == DROPEFFECT_MOVE) here,
    Explorer-Source at some conditions can treat it as (unoptimized move) mode,
    and Explorer-Source will remove source files after DoDragDrop()
    in that (unoptimized move) mode.
    We want to avoid such (unoptimized move) cases.
    So we don't return (*effect == DROPEFFECT_MOVE), here if Source is not 7-Zip.
    If source is 7-Zip that will do acual opeartion, then we can return DROPEFFECT_MOVE.
  */
  if (hres != S_OK || (opEffect == DROPEFFECT_MOVE && !needDrop_by_Source))
  {
    // opEffect = opEffect;
    // opEffect = DROPEFFECT_NONE; // for debug disabled
  }

  *effect = opEffect;
  /* if (hres <  0), DoDragDrop() also will return (hres).
     if (hres >= 0), DoDragDrop() will return DRAGDROP_S_DROP;
  */
  return hres;
}



// ---------- CPanel ----------


static bool Is_Path1_Prefixed_by_Path2(const UString &path, const UString &prefix)
{
  const unsigned len = prefix.Len();
  if (path.Len() < len)
    return false;
  return CompareFileNames(path.Left(len), prefix) == 0;
}

static bool IsFolderInTemp(const UString &path)
{
  FString tempPathF;
  if (!MyGetTempPath(tempPathF))
    return false;
  const UString tempPath = fs2us(tempPathF);
  if (tempPath.IsEmpty())
    return false;
  return Is_Path1_Prefixed_by_Path2(path, tempPath);
}

static bool AreThereNamesFromTemp(const UStringVector &filePaths)
{
  FString tempPathF;
  if (!MyGetTempPath(tempPathF))
    return false;
  const UString tempPath = fs2us(tempPathF);
  if (tempPath.IsEmpty())
    return false;
  FOR_VECTOR (i, filePaths)
    if (Is_Path1_Prefixed_by_Path2(filePaths[i], tempPath))
      return true;
  return false;
}


/*
  empty folderPath means create new Archive to path of first fileName.
  createNewArchive == true : show "Add to archive ..." dialog with external program
    folderPath.IsEmpty()  : create archive in folder of filePaths[0].
  createNewArchive == false :
    folderPath.IsEmpty()  : copy to archive folder that is open in panel
    !folderPath.IsEmpty()  : CopyFsItems() to folderPath.
*/
void CPanel::CompressDropFiles(
    const UStringVector &filePaths,
    const UString &folderPath,
    bool createNewArchive,
    bool moveMode,
    UInt32 sourceFlags,
    UInt32 &targetFlags
    )
{
  if (filePaths.Size() == 0)
    return;
  // createNewArchive = false; // for debug

  if (createNewArchive)
  {
    UString folderPath2 = folderPath;
    // folderPath2.Empty(); // for debug
    if (folderPath2.IsEmpty())
    {
      {
        FString folderPath2F;
        GetOnlyDirPrefix(us2fs(filePaths.Front()), folderPath2F);
        folderPath2 = fs2us(folderPath2F);
      }
      if (IsFolderInTemp(folderPath2))
      {
        /* we don't want archive to be created in temp directory.
           so we change the path to root folder (non-temp) */
        folderPath2 = ROOT_FS_FOLDER;
      }
    }

    UString arcName_base;
    const UString arcName = CreateArchiveName(filePaths,
        false, // isHash
        NULL,  // CFileInfo *fi
        arcName_base);

    bool needWait;
    if (sourceFlags & k_SourceFlags_WaitFinish)
      needWait = true;
    else if (sourceFlags & k_SourceFlags_DoNotWaitFinish)
      needWait = false;
    else if (sourceFlags & k_SourceFlags_TempFiles)
      needWait = true;
    else
      needWait = AreThereNamesFromTemp(filePaths);

    targetFlags |= (needWait ?
        k_TargetFlags_WaitFinish :
        k_TargetFlags_DoNotWaitFinish);

    CompressFiles(folderPath2, arcName,
        L"",      // arcType
        true,     // addExtension
        filePaths,
        false,    // email
        true,     // showDialog
        needWait);
  }
  else
  {
    targetFlags |= k_TargetFlags_WaitFinish;
    if (!folderPath.IsEmpty())
    {
      CCopyToOptions options;
      options.moveMode = moveMode;
      options.folder = folderPath;
      options.showErrorMessages = true; // showErrorMessages is not used for this operation
      options.NeedRegistryZone = false;
      options.ZoneIdMode = NExtract::NZoneIdMode::kNone;
      // maybe we need more options here: FIXME
      /* HRESULT hres = */ CopyFsItems(options,
          filePaths,
          NULL // UStringVector *messages
          );
      // hres = hres;
    }
    else
    {
      CopyFromNoAsk(moveMode, filePaths);
    }
  }
}



static unsigned Drag_OnContextMenu(int xPos, int yPos, UInt32 cmdFlags)
{
  CMenu menu;
  CMenuDestroyer menuDestroyer(menu);
  /*
    Esc key in shown menu doesn't work if we call Drag_OnContextMenu from ::Drop().
    We call SetFocus() tp solve that problem.
    But the focus will be changed to Target Window after Drag and Drop.
    Is it OK to use SetFocus() here ?
    Is there another way to enable Esc key ?
  */
  // _listView.SetFocus(); // for debug
  ::SetFocus(g_HWND);
  menu.CreatePopup();
  /*
  int defaultCmd; // = NDragMenu::k_Move;
  defaultCmd = NDragMenu::k_None;
  */
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(NDragMenu::g_Pairs); i++)
  {
    const NDragMenu::CCmdLangPair &pair = NDragMenu::g_Pairs[i];
    const UInt32 cmdAndFlags = pair.CmdId_and_Flags;
    const UInt32 cmdId = cmdAndFlags & NDragMenu::k_MenuFlags_CmdMask;
    if (cmdId != NDragMenu::k_Cancel)
    if ((cmdFlags & ((UInt32)1 << cmdId)) == 0)
      continue;
    const UINT flags = MF_STRING;
    /*
    if (prop.IsVisible)
      flags |= MF_CHECKED;
    if (i == 0)
      flags |= MF_GRAYED;
    */
    // MF_DEFAULT doesn't work
    // if (i == 2) flags |= MF_DEFAULT;
    // if (i == 4) flags |= MF_HILITE;
    // if (cmd == defaultCmd) flags |= MF_HILITE;
    UString name = LangString(pair.LangId);
    if (name.IsEmpty())
    {
      if (cmdId == NDragMenu::k_Cancel)
        name = "Cancel";
      else
        name.Add_UInt32(pair.LangId);
    }
    if (cmdId == NDragMenu::k_Copy_ToArc)
    {
      // UString destPath = _currentFolderPrefix;
      /*
      UString destPath = LangString(IDS_CONTEXT_ARCHIVE);
      name = MyFormatNew(name, destPath);
      */
      name.Add_Space();
      AddLangString(name, IDS_CONTEXT_ARCHIVE);
    }
    if (cmdId == NDragMenu::k_Cancel)
      menu.AppendItem(MF_SEPARATOR, 0, (LPCTSTR)NULL);
    menu.AppendItem(flags, cmdAndFlags, name);
  }
  /*
  if (defaultCmd != 0)
    SetMenuDefaultItem(menu, (unsigned)defaultCmd,
        FALSE); // byPos
  */
  int menuResult = menu.Track(
      TPM_LEFTALIGN | TPM_RETURNCMD | TPM_NONOTIFY,
      xPos, yPos,
      g_HWND
      // _listView // for debug
      );
  /* menu.Track() return value is zero, if the user cancels
     the menu without making a selection, or if an error occurs */
  if (menuResult <= 0)
    menuResult = NDragMenu::k_Cancel;
  return (unsigned)menuResult;
}



void CApp::CreateDragTarget()
{
  _dropTargetSpec = new CDropTarget();
  _dropTarget = _dropTargetSpec;
  _dropTargetSpec->App = (this);
}

void CApp::SetFocusedPanel(unsigned index)
{
  LastFocusedPanel = index;
  _dropTargetSpec->TargetPanelIndex = (int)LastFocusedPanel;
}

void CApp::DragBegin(unsigned panelIndex)
{
  _dropTargetSpec->TargetPanelIndex = (int)(NumPanels > 1 ? 1 - panelIndex : panelIndex);
  _dropTargetSpec->SrcPanelIndex = (int)panelIndex;
}

void CApp::DragEnd()
{
  _dropTargetSpec->TargetPanelIndex = (int)LastFocusedPanel;
  _dropTargetSpec->SrcPanelIndex = -1;
}

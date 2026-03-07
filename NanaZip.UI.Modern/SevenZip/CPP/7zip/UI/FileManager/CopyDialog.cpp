// CopyDialog.cpp

#include "StdAfx.h"

#include "../../../Windows/FileName.h"

#include "../../../Windows/Control/Static.h"

#include "BrowseDialog.h"
#include "CopyDialog.h"

#ifdef LANG
#include "LangUtils.h"
#endif

// **************** NanaZip Modification Start ****************
#include <NanaZip.Modern.h>
// **************** NanaZip Modification End ****************

using namespace NWindows;

// **************** NanaZip Modification Start ****************
INT_PTR CCopyDialog::Create(HWND parentWindow)
{
    ::K7ModernShowCopyLocationDialog(
        parentWindow,
        this->Title.Ptr(),
        this->Static.Ptr(),
        this->Info.Ptr(),
        this->Value.Ptr(),
        &CCopyDialog::ModernWindowHandler,
        this);

    return this->m_ReturnCode;
}

bool CCopyDialog::ModernMessageRouter(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    if (WM_COMMAND == uMsg)
    {
        int Code = HIWORD(wParam);
        int ItemID = LOWORD(wParam);
        if (BN_CLICKED == Code)
        {
            switch (ItemID)
            {
            case K7_COPY_LOCATION_DIALOG_RESULT_OK:
                this->ModernOK();
                return true;
            }
        }
    }

    return false;
}

void CCopyDialog::ModernOK()
{
    Value = ::K7ModernGetCopyLocationDialogPath(
        m_WindowHandle);
    m_ReturnCode = IDOK;
}

LRESULT CALLBACK CCopyDialog::ModernWindowHandler(
    _In_ HWND hWnd,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam,
    _In_ UINT_PTR uIdSubclass,
    _In_ DWORD_PTR dwRefData)
{
    UNREFERENCED_PARAMETER(uIdSubclass);

    CCopyDialog* Instance =
        reinterpret_cast<CCopyDialog*>(dwRefData);

    if (Instance)
    {
        if (!Instance->m_FirstRun)
        {
            Instance->m_FirstRun = true;
            Instance->m_WindowHandle = hWnd;
        }
        if (Instance->ModernMessageRouter(uMsg, wParam, lParam))
        {
            return 0;
        }
    }

    return ::DefSubclassProc(
        hWnd,
        uMsg,
        wParam,
        lParam);
}

#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
bool CCopyDialog::OnInit()
{
  #ifdef LANG
  LangSetDlgItems(*this, NULL, 0);
  #endif
  _path.Attach(GetItem(IDC_COPY));
  SetText(Title);

  NControl::CStatic staticContol;
  staticContol.Attach(GetItem(IDT_COPY));
  staticContol.SetText(Static);
  #ifdef UNDER_CE
  // we do it, since WinCE selects Value\something instead of Value !!!!
  _path.AddString(Value);
  #endif
  FOR_VECTOR (i, Strings)
    _path.AddString(Strings[i]);
  _path.SetText(Value);
  SetItemText(IDT_COPY_INFO, Info);
  NormalizeSize(true);
  return CModalDialog::OnInit();
}

bool CCopyDialog::OnSize(WPARAM /* wParam */, int xSize, int ySize)
{
  int mx, my;
  GetMargins(8, mx, my);
  int bx1, bx2, by;
  GetItemSizes(IDCANCEL, bx1, by);
  GetItemSizes(IDOK, bx2, by);
  int y = ySize - my - by;
  int x = xSize - mx - bx1;

  InvalidateRect(NULL);

  {
    RECT r;
    GetClientRectOfItem(IDB_COPY_SET_PATH, r);
    int bx = RECT_SIZE_X(r);
    MoveItem(IDB_COPY_SET_PATH, xSize - mx - bx, r.top, bx, RECT_SIZE_Y(r));
    ChangeSubWindowSizeX(_path, xSize - mx - mx - bx - mx);
  }

  {
    RECT r;
    GetClientRectOfItem(IDT_COPY_INFO, r);
    NControl::CStatic staticContol;
    staticContol.Attach(GetItem(IDT_COPY_INFO));
    int yPos = r.top;
    staticContol.Move(mx, yPos, xSize - mx * 2, y - 2 - yPos);
  }

  MoveItem(IDCANCEL, x, y, bx1, by);
  MoveItem(IDOK, x - mx - bx2, y, bx2, by);

  return false;
}

bool CCopyDialog::OnButtonClicked(int buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDB_COPY_SET_PATH:
      OnButtonSetPath();
      return true;
  }
  return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
}

void CCopyDialog::OnButtonSetPath()
{
  UString currentPath;
  _path.GetText(currentPath);

  const UString title = LangString(IDS_SET_FOLDER);

  UString resultPath;
  if (!MyBrowseForFolder(*this, title, currentPath, resultPath))
    return;
  NFile::NName::NormalizeDirPathPrefix(resultPath);
  _path.SetCurSel(-1);
  _path.SetText(resultPath);
}

void CCopyDialog::OnOK()
{
  _path.GetText(Value);
  CModalDialog::OnOK();
}
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
// **************** NanaZip Modification End ****************

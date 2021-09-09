// CopyDialog.cpp

#include "StdAfx.h"

#include "../../../Windows/FileName.h"

#include "../../../Windows/Control/Static.h"

#include "BrowseDialog.h"
#include "CopyDialog.h"

#ifdef LANG
#include "LangUtils.h"
#endif

using namespace NWindows;

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

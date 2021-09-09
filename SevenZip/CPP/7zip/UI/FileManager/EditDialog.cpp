// EditDialog.cpp

#include "StdAfx.h"

#include "EditDialog.h"

#ifdef LANG
#include "LangUtils.h"
#endif

bool CEditDialog::OnInit()
{
  #ifdef LANG
  LangSetDlgItems(*this, NULL, 0);
  #endif
  _edit.Attach(GetItem(IDE_EDIT));

  SetText(Title);
  _edit.SetText(Text);

  NormalizeSize();
  return CModalDialog::OnInit();
}

// #define MY_CLOSE_BUTTON__ID IDCANCEL
#define MY_CLOSE_BUTTON__ID IDCLOSE
 
bool CEditDialog::OnSize(WPARAM /* wParam */, int xSize, int ySize)
{
  int mx, my;
  GetMargins(8, mx, my);
  int bx1, by;
  GetItemSizes(MY_CLOSE_BUTTON__ID, bx1, by);

  // int bx2;
  // GetItemSizes(IDOK, bx2, by);

  int y = ySize - my - by;
  int x = xSize - mx - bx1;

  /*
  RECT rect;
  GetClientRect(&rect);
  rect.top = y - my;
  InvalidateRect(&rect);
  */
  InvalidateRect(NULL);

  MoveItem(MY_CLOSE_BUTTON__ID, x, y, bx1, by);
  // MoveItem(IDOK, x - mx - bx2, y, bx2, by);
  /*
  if (wParam == SIZE_MAXSHOW || wParam == SIZE_MAXIMIZED || wParam == SIZE_MAXHIDE)
    mx = 0;
  */
  _edit.Move(mx, my, xSize - mx * 2, y - my * 2);
  return false;
}

// OverwriteDialog.cpp

#include "StdAfx.h"

#include "../../../Common/StringConvert.h"

#include "../../../Windows/PropVariantConv.h"
#include "../../../Windows/ResourceString.h"

#include "../../../Windows/Control/Static.h"

#include "FormatUtils.h"
#include "LangUtils.h"
#include "OverwriteDialog.h"

#include "PropertyNameRes.h"

using namespace NWindows;

#ifdef LANG
static const UInt32 kLangIDs[] =
{
  IDT_OVERWRITE_HEADER,
  IDT_OVERWRITE_QUESTION_BEGIN,
  IDT_OVERWRITE_QUESTION_END,
  IDB_YES_TO_ALL,
  IDB_NO_TO_ALL,
  IDB_AUTO_RENAME
};
#endif

static const unsigned kCurrentFileNameSizeLimit = 82;
static const unsigned kCurrentFileNameSizeLimit2 = 30;

void COverwriteDialog::ReduceString(UString &s)
{
  unsigned size = _isBig ? kCurrentFileNameSizeLimit : kCurrentFileNameSizeLimit2;
  if (s.Len() > size)
  {
    s.Delete(size / 2, s.Len() - size);
    s.Insert(size / 2, L" ... ");
  }
  if (!s.IsEmpty() && s.Back() == ' ')
  {
    // s += (wchar_t)(0x2423);
    s.InsertAtFront(L'\"');
    s += L'\"';
  }
}

void COverwriteDialog::SetFileInfoControl(int textID, int iconID,
    const NOverwriteDialog::CFileInfo &fileInfo)
{
  UString sizeString;
  if (fileInfo.SizeIsDefined)
    sizeString = MyFormatNew(IDS_FILE_SIZE, NumberToString(fileInfo.Size));

  const UString &fileName = fileInfo.Name;
  int slashPos = fileName.ReverseFind_PathSepar();
  UString s1 = fileName.Left((unsigned)(slashPos + 1));
  UString s2 = fileName.Ptr((unsigned)(slashPos + 1));

  ReduceString(s1);
  ReduceString(s2);
  
  UString s = s1;
  s.Add_LF();
  s += s2;
  s.Add_LF();
  s += sizeString;
  s.Add_LF();

  if (fileInfo.TimeIsDefined)
  {
    AddLangString(s, IDS_PROP_MTIME);
    s += ": ";
    char t[32];
    ConvertUtcFileTimeToString(fileInfo.Time, t);
    s += t;
  }

  NControl::CDialogChildControl control;
  control.Init(*this, textID);
  control.SetText(s);

  SHFILEINFO shellFileInfo;
  if (::SHGetFileInfo(
      GetSystemString(fileInfo.Name), FILE_ATTRIBUTE_NORMAL, &shellFileInfo,
      sizeof(shellFileInfo), SHGFI_ICON | SHGFI_USEFILEATTRIBUTES | SHGFI_LARGEICON))
  {
    NControl::CStatic staticContol;
    staticContol.Attach(GetItem(iconID));
    staticContol.SetIcon(shellFileInfo.hIcon);
  }
}

bool COverwriteDialog::OnInit()
{
  #ifdef LANG
  LangSetWindowText(*this, IDD_OVERWRITE);
  LangSetDlgItems(*this, kLangIDs, ARRAY_SIZE(kLangIDs));
  #endif
  SetFileInfoControl(IDT_OVERWRITE_OLD_FILE_SIZE_TIME, IDI_OVERWRITE_OLD_FILE, OldFileInfo);
  SetFileInfoControl(IDT_OVERWRITE_NEW_FILE_SIZE_TIME, IDI_OVERWRITE_NEW_FILE, NewFileInfo);
  NormalizePosition();

  if (!ShowExtraButtons)
  {
    HideItem(IDB_YES_TO_ALL);
    HideItem(IDB_NO_TO_ALL);
    HideItem(IDB_AUTO_RENAME);
  }

  if (DefaultButton_is_NO)
  {
    PostMsg(DM_SETDEFID, IDNO);
    HWND h = GetItem(IDNO);
    PostMsg(WM_NEXTDLGCTL, (WPARAM)h, TRUE);
    // ::SetFocus(h);
  }

  return CModalDialog::OnInit();
}

bool COverwriteDialog::OnButtonClicked(int buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDYES:
    case IDNO:
    case IDB_YES_TO_ALL:
    case IDB_NO_TO_ALL:
    case IDB_AUTO_RENAME:
      End(buttonID);
      return true;
  }
  return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
}

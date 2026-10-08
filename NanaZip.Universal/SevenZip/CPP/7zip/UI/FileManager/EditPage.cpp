// EditPage.cpp

#include "StdAfx.h"

#include "EditPage.h"
#include "EditPageRes.h"

#include "BrowseDialog.h"
#include "HelpUtils.h"
#include "LangUtils.h"
#include "RegistryUtils.h"

using namespace NWindows;

#ifdef Z7_LANG
static const UInt32 kLangIDs[] =
{
  IDT_EDIT_EDITOR,
  IDT_EDIT_DIFF
};

static const UInt32 kLangIDs_Colon[] =
{
  IDT_EDIT_VIEWER
};
#endif

#define kEditTopic "FM/options.htm#editor"

bool CEditPage::OnInit()
{
  _initMode = true;

  #ifdef Z7_LANG
  LangSetDlgItems(*this, kLangIDs, Z7_ARRAY_SIZE(kLangIDs));
  LangSetDlgItems_Colon(*this, kLangIDs_Colon, Z7_ARRAY_SIZE(kLangIDs_Colon));
  #endif

  _ctrls[0].Ctrl = IDE_EDIT_VIEWER; _ctrls[0].Button = IDB_EDIT_VIEWER;
  _ctrls[1].Ctrl = IDE_EDIT_EDITOR; _ctrls[1].Button = IDB_EDIT_EDITOR;
  _ctrls[2].Ctrl = IDE_EDIT_DIFF;   _ctrls[2].Button = IDB_EDIT_DIFF;

  for (unsigned i = 0; i < 3; i++)
  {
    CEditPageCtrl &c = _ctrls[i];
    c.WasChanged = false;
    c.Edit.Attach(GetItem(c.Ctrl));
    UString path;
    if (i < 2)
      ReadRegEditor(i > 0, path);
    else
      ReadRegDiff(path);
    c.Edit.SetText(path);
  }

  _initMode = false;

  return CPropertyPage::OnInit();
}

LONG CEditPage::OnApply()
{
  for (unsigned i = 0; i < 3; i++)
  {
    CEditPageCtrl &c = _ctrls[i];
    if (c.WasChanged)
    {
      UString path;
      c.Edit.GetText(path);
      if (i < 2)
        SaveRegEditor(i > 0, path);
      else
        SaveRegDiff(path);
      c.WasChanged = false;
    }
  }

  return PSNRET_NOERROR;
}

// **************** NanaZip Modification Start ****************
// Removed from NanaZip.
#if 0 // ******** Annotated 7-Zip Mainline Source Code snippet Start ********
void CEditPage::OnNotifyHelp()
{
  ShowHelpWindow(kEditTopic);
}
#endif // ******** Annotated 7-Zip Mainline Source Code snippet End ********
// **************** NanaZip Modification End ****************

void SplitCmdLineSmart(const UString &cmd, UString &prg, UString &params);

static void Edit_BrowseForFile(NWindows::NControl::CEdit &edit, HWND hwnd)
{
  UString cmd;
  edit.GetText(cmd);

  UString param;
  UString prg;

  SplitCmdLineSmart(cmd, prg, param);

  CObjectVector<CBrowseFilterInfo> filters;
  CBrowseFilterInfo &bfi = filters.AddNew();
  bfi.Description = "*.exe";
  bfi.Masks.Add(UString("*.exe"));

  CBrowseInfo bi;
  bi.FilterIndex = 0;
  bi.FilePath = prg;
  bi.hwndOwner = hwnd;

  if (bi.BrowseForFile(filters))
  {
    cmd = bi.FilePath;
    cmd.Trim();
    /*
    if (!param.IsEmpty() && !resPath.IsEmpty())
    {
      cmd.InsertAtFront(L'\"');
      cmd += L'\"';
      cmd.Add_Space();
      cmd += param;
    }
    */

    edit.SetText(cmd);
    // Changed();
  }
}

bool CEditPage::OnButtonClicked(unsigned buttonID, HWND buttonHWND)
{
  for (unsigned i = 0; i < 3; i++)
  {
    CEditPageCtrl &c = _ctrls[i];
    if (buttonID == c.Button)
    {
      Edit_BrowseForFile(c.Edit, *this);
      return true;
    }
  }

  return CPropertyPage::OnButtonClicked(buttonID, buttonHWND);
}

bool CEditPage::OnCommand(unsigned code, unsigned itemID, LPARAM param)
{
  if (!_initMode && code == EN_CHANGE)
  {
    for (unsigned i = 0; i < 3; i++)
    {
      CEditPageCtrl &c = _ctrls[i];
      if (itemID == c.Ctrl)
      {
        c.WasChanged = true;
        Changed();
        return true;
      }
    }
  }

  return CPropertyPage::OnCommand(code, itemID, param);
}

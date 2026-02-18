// UpdateCallbackGUI2.cpp

#include "StdAfx.h"

#include "../FileManager/LangUtils.h"

// **************** NanaZip Modification Start ****************
#include "NanaZip.Modern.h"
#include <Mile.Helpers.h>
// **************** NanaZip Modification End ****************

#include "resource2.h"
#include "resource3.h"
#include "ExtractRes.h"

#include "UpdateCallbackGUI.h"

using namespace NWindows;

static const UINT k_UpdNotifyLangs[] =
{
  IDS_PROGRESS_ADD,
  IDS_PROGRESS_UPDATE,
  IDS_PROGRESS_ANALYZE,
  IDS_PROGRESS_REPLICATE,
  IDS_PROGRESS_REPACK,
  IDS_PROGRESS_SKIPPING,
  IDS_PROGRESS_DELETE,
  IDS_PROGRESS_HEADER
};

void CUpdateCallbackGUI2::Init()
{
  NumFiles = 0;

  _lang_Removing = LangString(IDS_PROGRESS_REMOVE);
  _lang_Ops.Clear();
  for (unsigned i = 0; i < ARRAY_SIZE(k_UpdNotifyLangs); i++)
    _lang_Ops.Add(LangString(k_UpdNotifyLangs[i]));
}

HRESULT CUpdateCallbackGUI2::SetOperation_Base(UInt32 notifyOp, const wchar_t *name, bool isDir)
{
  const UString *s = NULL;
  if (notifyOp < _lang_Ops.Size())
    s = &(_lang_Ops[(unsigned)notifyOp]);
  else
    s = &_emptyString;

  return ProgressDialog->Sync.Set_Status2(*s, name, isDir);
}


HRESULT CUpdateCallbackGUI2::ShowAskPasswordDialog()
{
  // **************** NanaZip Modification Start ****************
  ProgressDialog->WaitCreating();

  LPWSTR PasswordInput{ nullptr };

  if (::K7ModernShowPasswordDialog(static_cast<HWND>(*ProgressDialog), &PasswordInput) != IDOK)
  {
      return E_ABORT;
  }

  Password.SetFrom(PasswordInput, ::lstrlenW(PasswordInput));
  ::MileFreeMemory(PasswordInput);
  // **************** NanaZip Modification End ****************

  PasswordIsDefined = true;
  return S_OK;
}

// UpdateCallbackGUI2.cpp

#include "StdAfx.h"

#include "../FileManager/LangUtils.h"
#include "../FileManager/PasswordDialog.h"

#include "resource2.h"
#include "resource3.h"
#include "ExtractRes.h"
#include "../FileManager/resourceGui.h"

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

  LangString(IDS_PROGRESS_REMOVE, _lang_Removing);
  LangString(IDS_MOVING, _lang_Moving);
  _lang_Ops.Clear();
  for (unsigned i = 0; i < Z7_ARRAY_SIZE(k_UpdNotifyLangs); i++)
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
  CPasswordDialog dialog;
  ProgressDialog->WaitCreating();
  if (dialog.Create(*ProgressDialog) != IDOK)
    return E_ABORT;
  Password = dialog.Password;
  PasswordIsDefined = true;
  return S_OK;
}


HRESULT CUpdateCallbackGUI2::MoveArc_UpdateStatus()
{
  UString s;
  s.Add_UInt64(_arcMoving_percents);
  s.Add_Char('%');

  const bool totalDefined = (_arcMoving_total != 0 && _arcMoving_total != (UInt64)(Int64)-1);
  if (totalDefined || _arcMoving_current != 0)
  {
    s += " : ";
    s.Add_UInt64(_arcMoving_current >> 20);
    s += " MiB";
  }
  if (totalDefined)
  {
    s += " / ";
    s.Add_UInt64((_arcMoving_total + ((1 << 20) - 1)) >> 20);
    s += " MiB";
  }

  s += " : ";
  s += _lang_Moving;
  s += " : ";
  // s.Add_Char('\"');
  s += _arcMoving_name1;
  // s.Add_Char('\"');
  return ProgressDialog->Sync.Set_Status2(s, _arcMoving_name2,
      false); // isDir
}


HRESULT CUpdateCallbackGUI2::MoveArc_Start_Base(const wchar_t *srcTempPath, const wchar_t *destFinalPath, UInt64 totalSize, Int32 updateMode)
{
  _arcMoving_percents = 0;
  _arcMoving_total = totalSize;
  _arcMoving_current = 0;
  _arcMoving_updateMode = updateMode;
  _arcMoving_name1 = srcTempPath;
  _arcMoving_name2 = destFinalPath;
  return MoveArc_UpdateStatus();
}


HRESULT CUpdateCallbackGUI2::MoveArc_Progress_Base(UInt64 totalSize, UInt64 currentSize)
{
  _arcMoving_total = totalSize;
  _arcMoving_current = currentSize;
  UInt64 percents = 0;
  if (totalSize != 0)
  {
    if (totalSize < ((UInt64)1 << 57))
      percents = currentSize * 100 / totalSize;
    else
      percents = currentSize / (totalSize / 100);
  }
  if (percents == _arcMoving_percents)
    return ProgressDialog->Sync.CheckStop();
  // Sleep(300); // for debug
  _arcMoving_percents = percents;
  return MoveArc_UpdateStatus();
}


HRESULT CUpdateCallbackGUI2::MoveArc_Finish_Base()
{
  return ProgressDialog->Sync.Set_Status2(L"", L"", false);
}

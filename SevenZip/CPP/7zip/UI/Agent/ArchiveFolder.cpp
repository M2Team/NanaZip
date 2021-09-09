// Agent/ArchiveFolder.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"

#include "../Common/ArchiveExtractCallback.h"

#include "Agent.h"

/*
STDMETHODIMP CAgentFolder::SetReplaceAltStreamCharsMode(Int32 replaceAltStreamCharsMode)
{
  _replaceAltStreamCharsMode = replaceAltStreamCharsMode;
  return S_OK;
}
*/

STDMETHODIMP CAgentFolder::CopyTo(Int32 moveMode, const UInt32 *indices, UInt32 numItems,
    Int32 includeAltStreams, Int32 replaceAltStreamCharsMode,
    const wchar_t *path, IFolderOperationsExtractCallback *callback)
{
  if (moveMode)
    return E_NOTIMPL;
  COM_TRY_BEGIN
  CMyComPtr<IFolderArchiveExtractCallback> extractCallback2;
  {
    CMyComPtr<IFolderOperationsExtractCallback> callbackWrap = callback;
    RINOK(callbackWrap.QueryInterface(IID_IFolderArchiveExtractCallback, &extractCallback2));
  }
  NExtract::NPathMode::EEnum pathMode;
  if (!_flatMode)
    pathMode = NExtract::NPathMode::kCurPaths;
  else
    pathMode = (_proxy2 && _loadAltStreams) ?
      NExtract::NPathMode::kNoPathsAlt :
      NExtract::NPathMode::kNoPaths;

  return Extract(indices, numItems,
      includeAltStreams, replaceAltStreamCharsMode,
      pathMode, NExtract::NOverwriteMode::kAsk,
      path, BoolToInt(false), extractCallback2);
  COM_TRY_END
}

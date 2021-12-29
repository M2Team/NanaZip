// TarHandlerOut.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/Defs.h"
#include "../../../Common/MyLinux.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/UTFConvert.h"

#include "../../../Windows/PropVariant.h"
#include "../../../Windows/TimeUtils.h"

#include "TarHandler.h"
#include "TarUpdate.h"

using namespace NWindows;

namespace NArchive {
namespace NTar {

STDMETHODIMP CHandler::GetFileTimeType(UInt32 *type)
{
  *type = NFileTimeType::kUnix;
  return S_OK;
}

HRESULT GetPropString(IArchiveUpdateCallback *callback, UInt32 index, PROPID propId, AString &res,
    UINT codePage, unsigned utfFlags, bool convertSlash)
{
  NCOM::CPropVariant prop;
  RINOK(callback->GetProperty(index, propId, &prop));
  
  if (prop.vt == VT_BSTR)
  {
    UString s = prop.bstrVal;
    if (convertSlash)
      NItemName::ReplaceSlashes_OsToUnix(s);

    if (codePage == CP_UTF8)
    {
      ConvertUnicodeToUTF8_Flags(s, res, utfFlags);
      // if (!ConvertUnicodeToUTF8(s, res)) // return E_INVALIDARG;
    }
    else
      UnicodeStringToMultiByte2(res, s, codePage);
  }
  else if (prop.vt != VT_EMPTY)
    return E_INVALIDARG;

  return S_OK;
}


// sort old files with original order.

static int CompareUpdateItems(void *const *p1, void *const *p2, void *)
{
  const CUpdateItem &u1 = *(*((const CUpdateItem *const *)p1));
  const CUpdateItem &u2 = *(*((const CUpdateItem *const *)p2));
  if (!u1.NewProps)
  {
    if (u2.NewProps)
      return -1;
    return MyCompare(u1.IndexInArc, u2.IndexInArc);
  }
  if (!u2.NewProps)
    return 1;
  return MyCompare(u1.IndexInClient, u2.IndexInClient);
}


STDMETHODIMP CHandler::UpdateItems(ISequentialOutStream *outStream, UInt32 numItems,
    IArchiveUpdateCallback *callback)
{
  COM_TRY_BEGIN

  if ((_stream && (_error != k_ErrorType_OK || _warning /* || _isSparse */)) || _seqStream)
    return E_NOTIMPL;
  CObjectVector<CUpdateItem> updateItems;
  const UINT codePage = (_forceCodePage ? _specifiedCodePage : _openCodePage);
  const unsigned utfFlags = g_Unicode_To_UTF8_Flags;
  /*
  // for debug only:
  unsigned utfFlags = 0;
  utfFlags |= UTF_FLAG__TO_UTF8__EXTRACT_BMP_ESCAPE;
  utfFlags |= UTF_FLAG__TO_UTF8__SURROGATE_ERROR;
  */

  for (UInt32 i = 0; i < numItems; i++)
  {
    CUpdateItem ui;
    Int32 newData;
    Int32 newProps;
    UInt32 indexInArc;
    
    if (!callback)
      return E_FAIL;
    
    RINOK(callback->GetUpdateItemInfo(i, &newData, &newProps, &indexInArc));
    
    ui.NewProps = IntToBool(newProps);
    ui.NewData = IntToBool(newData);
    ui.IndexInArc = (int)indexInArc;
    ui.IndexInClient = i;

    if (IntToBool(newProps))
    {
      {
        NCOM::CPropVariant prop;
        RINOK(callback->GetProperty(i, kpidIsDir, &prop));
        if (prop.vt == VT_EMPTY)
          ui.IsDir = false;
        else if (prop.vt != VT_BOOL)
          return E_INVALIDARG;
        else
          ui.IsDir = (prop.boolVal != VARIANT_FALSE);
      }

      {
        NCOM::CPropVariant prop;
        RINOK(callback->GetProperty(i, kpidPosixAttrib, &prop));
        if (prop.vt == VT_EMPTY)
          ui.Mode =
                MY_LIN_S_IRWXO
              | MY_LIN_S_IRWXG
              | MY_LIN_S_IRWXU
              | (ui.IsDir ? MY_LIN_S_IFDIR : MY_LIN_S_IFREG);
        else if (prop.vt != VT_UI4)
          return E_INVALIDARG;
        else
          ui.Mode = prop.ulVal;
        // 21.07 : we clear high file type bits as GNU TAR.
        ui.Mode &= ~(UInt32)MY_LIN_S_IFMT;
      }

      {
        NCOM::CPropVariant prop;
        RINOK(callback->GetProperty(i, kpidMTime, &prop));
        if (prop.vt == VT_EMPTY)
          ui.MTime = 0;
        else if (prop.vt != VT_FILETIME)
          return E_INVALIDARG;
        else
          ui.MTime = NTime::FileTimeToUnixTime64(prop.filetime);
      }
      
      RINOK(GetPropString(callback, i, kpidPath, ui.Name, codePage, utfFlags, true));
      if (ui.IsDir && !ui.Name.IsEmpty() && ui.Name.Back() != '/')
        ui.Name += '/';
      RINOK(GetPropString(callback, i, kpidUser, ui.User, codePage, utfFlags, false));
      RINOK(GetPropString(callback, i, kpidGroup, ui.Group, codePage, utfFlags, false));
    }

    if (IntToBool(newData))
    {
      NCOM::CPropVariant prop;
      RINOK(callback->GetProperty(i, kpidSize, &prop));
      if (prop.vt != VT_UI8)
        return E_INVALIDARG;
      ui.Size = prop.uhVal.QuadPart;
      /*
      // now we support GNU extension for big files
      if (ui.Size >= ((UInt64)1 << 33))
        return E_INVALIDARG;
      */
    }
    
    updateItems.Add(ui);
  }
  
  if (_thereIsPaxExtendedHeader)
  {
    // we restore original order of files, if there is pax header block
    updateItems.Sort(CompareUpdateItems, NULL);
  }
  
  return UpdateArchive(_stream, outStream, _items, updateItems, codePage, utfFlags, callback);
  
  COM_TRY_END
}

}}

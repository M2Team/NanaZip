// TarHandlerOut.cpp

#include "StdAfx.h"

// #include <stdio.h>

#include "../../../Common/ComTry.h"
#include "../../../Common/MyLinux.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/TimeUtils.h"

#include "../Common/ItemNameUtils.h"

#include "TarHandler.h"
#include "TarUpdate.h"

using namespace NWindows;

namespace NArchive {
namespace NTar {

Z7_COM7F_IMF(CHandler::GetFileTimeType(UInt32 *type))
{
  UInt32 t = NFileTimeType::kUnix;
  const UInt32 prec = _handlerTimeOptions.Prec;
  if (prec != (UInt32)(Int32)-1)
  {
    t = NFileTimeType::kWindows;
    if (prec == k_PropVar_TimePrec_0 ||
        prec == k_PropVar_TimePrec_100ns)
      t = NFileTimeType::kWindows;
    else if (prec == k_PropVar_TimePrec_HighPrec)
      t = k_PropVar_TimePrec_1ns;
    else if (prec >= k_PropVar_TimePrec_Base)
      t = prec;
  }
  // 7-Zip before 22.00 fails, if unknown typeType.
  *type = t;
  return S_OK;
}


void Get_AString_From_UString(const UString &s, AString &res,
    UINT codePage, unsigned utfFlags)
{
  if (codePage == CP_UTF8)
    ConvertUnicodeToUTF8_Flags(s, res, utfFlags);
  else
    UnicodeStringToMultiByte2(res, s, codePage);
}


HRESULT GetPropString(IArchiveUpdateCallback *callback, UInt32 index, PROPID propId, AString &res,
    UINT codePage, unsigned utfFlags, bool convertSlash)
{
  NCOM::CPropVariant prop;
  RINOK(callback->GetProperty(index, propId, &prop))
  
  if (prop.vt == VT_BSTR)
  {
    UString s = prop.bstrVal;
    if (convertSlash)
      NItemName::ReplaceSlashes_OsToUnix(s);
    Get_AString_From_UString(s, res, codePage, utfFlags);
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


static HRESULT GetTime(UInt32 i, UInt32 pid, IArchiveUpdateCallback *callback,
    CPaxTime &pt)
{
  pt.Clear();
  NCOM::CPropVariant prop;
  RINOK(callback->GetProperty(i, pid, &prop))
  return Prop_To_PaxTime(prop, pt);
}


/*
static HRESULT GetDevice(IArchiveUpdateCallback *callback, UInt32 i,
    UInt32 &majo, UInt32 &mino, bool &majo_defined, bool &mino_defined)
{
  NWindows::NCOM::CPropVariant prop;
  RINOK(callback->GetProperty(i, kpidDevice, &prop));
  if (prop.vt == VT_EMPTY)
    return S_OK;
  if (prop.vt != VT_UI8)
    return E_INVALIDARG;
  {
    const UInt64 v = prop.uhVal.QuadPart;
    majo = MY_dev_major(v);
    mino = MY_dev_minor(v);
    majo_defined = true;
    mino_defined = true;
  }
  return S_OK;
}
*/

static HRESULT GetDevice(IArchiveUpdateCallback *callback, UInt32 i,
    UInt32 pid, UInt32 &id, bool &defined)
{
  defined = false;
  NWindows::NCOM::CPropVariant prop;
  RINOK(callback->GetProperty(i, pid, &prop))
  if (prop.vt == VT_EMPTY)
    return S_OK;
  if (prop.vt == VT_UI4)
  {
    id = prop.ulVal;
    defined = true;
    return S_OK;
  }
  return E_INVALIDARG;
}


static HRESULT GetUser(IArchiveUpdateCallback *callback, UInt32 i,
    UInt32 pidName, UInt32 pidId, AString &name, UInt32 &id,
    UINT codePage, unsigned utfFlags)
{
  // printf("\ncallback->GetProperty(i, pidId, &prop))\n");
   
  bool isSet = false;
  {
    NWindows::NCOM::CPropVariant prop;
    RINOK(callback->GetProperty(i, pidId, &prop))
    if (prop.vt == VT_UI4)
    {
      isSet = true;
      id = prop.ulVal;
      // printf("\ncallback->GetProperty(i, pidId, &prop)); = %d \n", (unsigned)id);
      name.Empty();
    }
    else if (prop.vt != VT_EMPTY)
      return E_INVALIDARG;
  }
  {
    NWindows::NCOM::CPropVariant prop;
    RINOK(callback->GetProperty(i, pidName, &prop))
    if (prop.vt == VT_BSTR)
    {
      const UString s = prop.bstrVal;
      Get_AString_From_UString(s, name, codePage, utfFlags);
      if (!isSet)
        id = 0;
    }
    else if (prop.vt == VT_UI4)
    {
      id = prop.ulVal;
      name.Empty();
    }
    else if (prop.vt != VT_EMPTY)
      return E_INVALIDARG;
  }
  return S_OK;
}



Z7_COM7F_IMF(CHandler::UpdateItems(ISequentialOutStream *outStream, UInt32 numItems,
    IArchiveUpdateCallback *callback))
{
  COM_TRY_BEGIN

  if ((_stream && (_arc._error != k_ErrorType_OK || _arc._is_Warning
      /* || _isSparse */
      )) || _seqStream)
    return E_NOTIMPL;
  CObjectVector<CUpdateItem> updateItems;
  const UINT codePage = (_forceCodePage ? _specifiedCodePage : _openCodePage);
  const unsigned utfFlags = g_Unicode_To_UTF8_Flags;
  /*
  // for debug only:
  unsigned utfFlags = 0;
  utfFlags |= Z7_UTF_FLAG_TO_UTF8_EXTRACT_BMP_ESCAPE;
  utfFlags |= Z7_UTF_FLAG_TO_UTF8_SURROGATE_ERROR;
  */

  for (UInt32 i = 0; i < numItems; i++)
  {
    CUpdateItem ui;
    Int32 newData;
    Int32 newProps;
    UInt32 indexInArc;
    
    if (!callback)
      return E_FAIL;
    
    RINOK(callback->GetUpdateItemInfo(i, &newData, &newProps, &indexInArc))
    
    ui.NewProps = IntToBool(newProps);
    ui.NewData = IntToBool(newData);
    ui.IndexInArc = (int)indexInArc;
    ui.IndexInClient = i;

    if (IntToBool(newProps))
    {
      {
        NCOM::CPropVariant prop;
        RINOK(callback->GetProperty(i, kpidIsDir, &prop))
        if (prop.vt == VT_EMPTY)
          ui.IsDir = false;
        else if (prop.vt != VT_BOOL)
          return E_INVALIDARG;
        else
          ui.IsDir = (prop.boolVal != VARIANT_FALSE);
      }

      {
        NCOM::CPropVariant prop;
        RINOK(callback->GetProperty(i, kpidPosixAttrib, &prop))
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
        // we will clear it later
        // ui.Mode &= ~(UInt32)MY_LIN_S_IFMT;
      }

      if (_handlerTimeOptions.Write_MTime.Val)
        RINOK(GetTime(i, kpidMTime, callback, ui.PaxTimes.MTime))
      if (_handlerTimeOptions.Write_ATime.Val)
        RINOK(GetTime(i, kpidATime, callback, ui.PaxTimes.ATime))
      if (_handlerTimeOptions.Write_CTime.Val)
        RINOK(GetTime(i, kpidCTime, callback, ui.PaxTimes.CTime))

      RINOK(GetPropString(callback, i, kpidPath, ui.Name, codePage, utfFlags, true))
      if (ui.IsDir && !ui.Name.IsEmpty() && ui.Name.Back() != '/')
        ui.Name += '/';
      // ui.Name += '/'; // for debug

      if (_posixMode)
      {
        RINOK(GetDevice(callback, i, kpidDeviceMajor, ui.DeviceMajor, ui.DeviceMajor_Defined))
        RINOK(GetDevice(callback, i, kpidDeviceMinor, ui.DeviceMinor, ui.DeviceMinor_Defined))
      }

      RINOK(GetUser(callback, i, kpidUser,  kpidUserId,  ui.User,  ui.UID, codePage, utfFlags))
      RINOK(GetUser(callback, i, kpidGroup, kpidGroupId, ui.Group, ui.GID, codePage, utfFlags))
    }

    if (IntToBool(newData))
    {
      NCOM::CPropVariant prop;
      RINOK(callback->GetProperty(i, kpidSize, &prop))
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
  
  if (_arc._are_Pax_Items)
  {
    // we restore original order of files, if there are pax items
    updateItems.Sort(CompareUpdateItems, NULL);
  }

  CUpdateOptions options;

  options.CodePage = codePage;
  options.UtfFlags = utfFlags;
  options.PosixMode = _posixMode;
  
  options.Write_MTime = _handlerTimeOptions.Write_MTime;
  options.Write_ATime = _handlerTimeOptions.Write_ATime;
  options.Write_CTime = _handlerTimeOptions.Write_CTime;
  
  // options.TimeOptions = TimeOptions;
  
  const UInt32 prec = _handlerTimeOptions.Prec;
  if (prec != (UInt32)(Int32)-1)
  {
    unsigned numDigits = 0;
    if (prec == 0)
      numDigits = 7;
    else if (prec == k_PropVar_TimePrec_HighPrec
          || prec >= k_PropVar_TimePrec_1ns)
      numDigits = 9;
    else if (prec >= k_PropVar_TimePrec_Base)
      numDigits = prec - k_PropVar_TimePrec_Base;
    options.TimeOptions.NumDigitsMax = numDigits;
    // options.TimeOptions.RemoveZeroMode =
        // k_PaxTimeMode_DontRemoveZero; // pure for debug
        // k_PaxTimeMode_RemoveZero_if_PureSecondOnly; // optimized code
        // k_PaxTimeMode_RemoveZero_Always; // original pax code
  }

  return UpdateArchive(_stream, outStream, _items, updateItems,
      options, callback);
  
  COM_TRY_END
}

}}

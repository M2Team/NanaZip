// ZipHandlerOut.cpp

#include "StdAfx.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/StringToInt.h"

#include "../../../Windows/PropVariant.h"
#include "../../../Windows/TimeUtils.h"

#include "../../IPassword.h"

#include "../../Common/OutBuffer.h"

#include "../../Crypto/WzAes.h"

#include "../Common/ItemNameUtils.h"
#include "../Common/ParseProperties.h"

#include "ZipHandler.h"
#include "ZipUpdate.h"

using namespace NWindows;
using namespace NCOM;
using namespace NTime;

namespace NArchive {
namespace NZip {

STDMETHODIMP CHandler::GetFileTimeType(UInt32 *timeType)
{
  *timeType = NFileTimeType::kDOS;
  return S_OK;
}

static bool IsSimpleAsciiString(const wchar_t *s)
{
  for (;;)
  {
    wchar_t c = *s++;
    if (c == 0)
      return true;
    if (c < 0x20 || c > 0x7F)
      return false;
  }
}


static int FindZipMethod(const char *s, const char * const *names, unsigned num)
{
  for (unsigned i = 0; i < num; i++)
  {
    const char *name = names[i];
    if (name && StringsAreEqualNoCase_Ascii(s, name))
      return (int)i;
  }
  return -1;
}

static int FindZipMethod(const char *s)
{
  int k = FindZipMethod(s, kMethodNames1, kNumMethodNames1);
  if (k >= 0)
    return k;
  k = FindZipMethod(s, kMethodNames2, kNumMethodNames2);
  if (k >= 0)
    return (int)kMethodNames2Start + k;
  return -1;
}


#define COM_TRY_BEGIN2 try {
#define COM_TRY_END2 } \
catch(const CSystemException &e) { return e.ErrorCode; } \
catch(...) { return E_OUTOFMEMORY; }

static HRESULT GetTime(IArchiveUpdateCallback *callback, unsigned index, PROPID propID, FILETIME &filetime)
{
  filetime.dwHighDateTime = filetime.dwLowDateTime = 0;
  NCOM::CPropVariant prop;
  RINOK(callback->GetProperty(index, propID, &prop));
  if (prop.vt == VT_FILETIME)
    filetime = prop.filetime;
  else if (prop.vt != VT_EMPTY)
    return E_INVALIDARG;
  return S_OK;
}


STDMETHODIMP CHandler::UpdateItems(ISequentialOutStream *outStream, UInt32 numItems,
    IArchiveUpdateCallback *callback)
{
  COM_TRY_BEGIN2
  
  if (m_Archive.IsOpen())
  {
    if (!m_Archive.CanUpdate())
      return E_NOTIMPL;
  }

  CObjectVector<CUpdateItem> updateItems;
  updateItems.ClearAndReserve(numItems);

  bool thereAreAesUpdates = false;
  UInt64 largestSize = 0;
  bool largestSizeDefined = false;

  #ifdef _WIN32
  const UINT oemCP = GetOEMCP();
  #endif

  UString name;
  CUpdateItem ui;

  for (UInt32 i = 0; i < numItems; i++)
  {
    Int32 newData;
    Int32 newProps;
    UInt32 indexInArc;
    
    if (!callback)
      return E_FAIL;
    
    RINOK(callback->GetUpdateItemInfo(i, &newData, &newProps, &indexInArc));
    
    name.Empty();
    ui.Clear();

    ui.NewProps = IntToBool(newProps);
    ui.NewData = IntToBool(newData);
    ui.IndexInArc = (int)indexInArc;
    ui.IndexInClient = i;
    
    bool existInArchive = (indexInArc != (UInt32)(Int32)-1);
    if (existInArchive)
    {
      const CItemEx &inputItem = m_Items[indexInArc];
      if (inputItem.IsAesEncrypted())
        thereAreAesUpdates = true;
      if (!IntToBool(newProps))
        ui.IsDir = inputItem.IsDir();
      // ui.IsAltStream = inputItem.IsAltStream();
    }

    if (IntToBool(newProps))
    {
      {
        NCOM::CPropVariant prop;
        RINOK(callback->GetProperty(i, kpidAttrib, &prop));
        if (prop.vt == VT_EMPTY)
          ui.Attrib = 0;
        else if (prop.vt != VT_UI4)
          return E_INVALIDARG;
        else
          ui.Attrib = prop.ulVal;
      }

      {
        NCOM::CPropVariant prop;
        RINOK(callback->GetProperty(i, kpidPath, &prop));
        if (prop.vt == VT_EMPTY)
        {
          // name.Empty();
        }
        else if (prop.vt != VT_BSTR)
          return E_INVALIDARG;
        else
          name = prop.bstrVal;
      }

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

      /*
      {
        bool isAltStream = false;
        {
          NCOM::CPropVariant prop;
          RINOK(callback->GetProperty(i, kpidIsAltStream, &prop));
          if (prop.vt == VT_BOOL)
            isAltStream = (prop.boolVal != VARIANT_FALSE);
          else if (prop.vt != VT_EMPTY)
            return E_INVALIDARG;
        }
      
        if (isAltStream)
        {
          if (ui.IsDir)
            return E_INVALIDARG;
          int delim = name.ReverseFind(L':');
          if (delim >= 0)
          {
            name.Delete(delim, 1);
            name.Insert(delim, UString(k_SpecName_NTFS_STREAM));
            ui.IsAltStream = true;
          }
        }
      }
      */

      {
        CPropVariant prop;
        RINOK(callback->GetProperty(i, kpidTimeType, &prop));
        if (prop.vt == VT_UI4)
          ui.NtfsTimeIsDefined = (prop.ulVal == NFileTimeType::kWindows);
        else
          ui.NtfsTimeIsDefined = m_WriteNtfsTimeExtra;
      }
      RINOK(GetTime(callback, i, kpidMTime, ui.Ntfs_MTime));
      RINOK(GetTime(callback, i, kpidATime, ui.Ntfs_ATime));
      RINOK(GetTime(callback, i, kpidCTime, ui.Ntfs_CTime));

      {
        FILETIME localFileTime = { 0, 0 };
        if (ui.Ntfs_MTime.dwHighDateTime != 0 ||
            ui.Ntfs_MTime.dwLowDateTime != 0)
          if (!FileTimeToLocalFileTime(&ui.Ntfs_MTime, &localFileTime))
            return E_INVALIDARG;
        FileTimeToDosTime(localFileTime, ui.Time);
      }

      NItemName::ReplaceSlashes_OsToUnix(name);
      
      bool needSlash = ui.IsDir;
      const wchar_t kSlash = L'/';
      if (!name.IsEmpty())
      {
        if (name.Back() == kSlash)
        {
          if (!ui.IsDir)
            return E_INVALIDARG;
          needSlash = false;
        }
      }
      if (needSlash)
        name += kSlash;

      const UINT codePage = _forceCodePage ? _specifiedCodePage : CP_OEMCP;
      bool tryUtf8 = true;

      /*
        Windows 10 allows users to set UTF-8 in Region Settings via option:
        "Beta: Use Unicode UTF-8 for worldwide language support"
        In that case Windows uses CP_UTF8 when we use CP_OEMCP.
        21.02 fixed:
          we set UTF-8 mark for non-latin files for such UTF-8 mode in Windows.
          we write additional Info-Zip Utf-8 FileName Extra for non-latin names/
      */

      if ((codePage != CP_UTF8) &&
        #ifdef _WIN32
          (m_ForceLocal || !m_ForceUtf8) && (oemCP != CP_UTF8)
        #else
          (m_ForceLocal && !m_ForceUtf8)
        #endif
        )
      {
        bool defaultCharWasUsed;
        ui.Name = UnicodeStringToMultiByte(name, codePage, '_', defaultCharWasUsed);
        tryUtf8 = (!m_ForceLocal && (defaultCharWasUsed ||
          MultiByteToUnicodeString(ui.Name, codePage) != name));
      }

      const bool isNonLatin = !name.IsAscii();

      if (tryUtf8)
      {
        ui.IsUtf8 = isNonLatin;
        ConvertUnicodeToUTF8(name, ui.Name);

        #ifndef _WIN32
        if (ui.IsUtf8 && !CheckUTF8_AString(ui.Name))
        {
          // if it's non-Windows and there are non-UTF8 characters we clear UTF8-flag
          ui.IsUtf8 = false;
        }
        #endif
      }
      else if (isNonLatin)
        Convert_Unicode_To_UTF8_Buf(name, ui.Name_Utf);

      if (ui.Name.Len() >= (1 << 16)
          || ui.Name_Utf.Size() >= (1 << 16) - 128)
        return E_INVALIDARG;

      {
        NCOM::CPropVariant prop;
        RINOK(callback->GetProperty(i, kpidComment, &prop));
        if (prop.vt == VT_EMPTY)
        {
          // ui.Comment.Free();
        }
        else if (prop.vt != VT_BSTR)
          return E_INVALIDARG;
        else
        {
          UString s = prop.bstrVal;
          AString a;
          if (ui.IsUtf8)
            ConvertUnicodeToUTF8(s, a);
          else
          {
            bool defaultCharWasUsed;
            a = UnicodeStringToMultiByte(s, codePage, '_', defaultCharWasUsed);
          }
          if (a.Len() >= (1 << 16))
            return E_INVALIDARG;
          ui.Comment.CopyFrom((const Byte *)(const char *)a, a.Len());
        }
      }


      /*
      if (existInArchive)
      {
        const CItemEx &itemInfo = m_Items[indexInArc];
        // ui.Commented = itemInfo.IsCommented();
        ui.Commented = false;
        if (ui.Commented)
        {
          ui.CommentRange.Position = itemInfo.GetCommentPosition();
          ui.CommentRange.Size  = itemInfo.CommentSize;
        }
      }
      else
        ui.Commented = false;
      */
    }
    
    
    if (IntToBool(newData))
    {
      UInt64 size = 0;
      if (!ui.IsDir)
      {
        NCOM::CPropVariant prop;
        RINOK(callback->GetProperty(i, kpidSize, &prop));
        if (prop.vt != VT_UI8)
          return E_INVALIDARG;
        size = prop.uhVal.QuadPart;
        if (largestSize < size)
          largestSize = size;
        largestSizeDefined = true;
      }
      ui.Size = size;
    }

    updateItems.Add(ui);
  }


  CMyComPtr<ICryptoGetTextPassword2> getTextPassword;
  {
    CMyComPtr<IArchiveUpdateCallback> udateCallBack2(callback);
    udateCallBack2.QueryInterface(IID_ICryptoGetTextPassword2, &getTextPassword);
  }
  CCompressionMethodMode options;
  (CBaseProps &)options = _props;
  options._dataSizeReduce = largestSize;
  options._dataSizeReduceDefined = largestSizeDefined;

  options.PasswordIsDefined = false;
  options.Password.Wipe_and_Empty();
  if (getTextPassword)
  {
    CMyComBSTR_Wipe password;
    Int32 passwordIsDefined;
    RINOK(getTextPassword->CryptoGetTextPassword2(&passwordIsDefined, &password));
    options.PasswordIsDefined = IntToBool(passwordIsDefined);
    if (options.PasswordIsDefined)
    {
      if (!m_ForceAesMode)
        options.IsAesMode = thereAreAesUpdates;

      if (!IsSimpleAsciiString(password))
        return E_INVALIDARG;
      if (password)
        UnicodeStringToMultiByte2(options.Password, (LPCOLESTR)password, CP_OEMCP);
      if (options.IsAesMode)
      {
        if (options.Password.Len() > NCrypto::NWzAes::kPasswordSizeMax)
          return E_INVALIDARG;
      }
    }
  }

  
  int mainMethod = m_MainMethod;
  
  if (mainMethod < 0)
  {
    if (!_props._methods.IsEmpty())
    {
      const AString &methodName = _props._methods.Front().MethodName;
      if (!methodName.IsEmpty())
      {
        mainMethod = FindZipMethod(methodName);
        if (mainMethod < 0)
        {
          CMethodId methodId;
          UInt32 numStreams;
          if (FindMethod_Index(EXTERNAL_CODECS_VARS methodName, true,
              methodId, numStreams) < 0)
            return E_NOTIMPL;
          if (numStreams != 1)
            return E_NOTIMPL;
          if (methodId == kMethodId_BZip2)
            mainMethod = NFileHeader::NCompressionMethod::kBZip2;
          else
          {
            if (methodId < kMethodId_ZipBase)
              return E_NOTIMPL;
            methodId -= kMethodId_ZipBase;
            if (methodId > 0xFF)
              return E_NOTIMPL;
            mainMethod = (int)methodId;
          }
        }
      }
    }
  }

  if (mainMethod < 0)
    mainMethod = (Byte)(((_props.GetLevel() == 0) ?
        NFileHeader::NCompressionMethod::kStore :
        NFileHeader::NCompressionMethod::kDeflate));
  else
    mainMethod = (Byte)mainMethod;
  
  options.MethodSequence.Add((Byte)mainMethod);
  
  if (mainMethod != NFileHeader::NCompressionMethod::kStore)
    options.MethodSequence.Add(NFileHeader::NCompressionMethod::kStore);

  return Update(
      EXTERNAL_CODECS_VARS
      m_Items, updateItems, outStream,
      m_Archive.IsOpen() ? &m_Archive : NULL, _removeSfxBlock,
      options, callback);
 
  COM_TRY_END2
}



STDMETHODIMP CHandler::SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps)
{
  InitMethodProps();
  
  for (UInt32 i = 0; i < numProps; i++)
  {
    UString name = names[i];
    name.MakeLower_Ascii();
    if (name.IsEmpty())
      return E_INVALIDARG;

    const PROPVARIANT &prop = values[i];

    if (name.IsEqualTo_Ascii_NoCase("em"))
    {
      if (prop.vt != VT_BSTR)
        return E_INVALIDARG;
      {
        const wchar_t *m = prop.bstrVal;
        if (IsString1PrefixedByString2_NoCase_Ascii(m, "aes"))
        {
          m += 3;
          if (StringsAreEqual_Ascii(m, "128"))
            _props.AesKeyMode = 1;
          else if (StringsAreEqual_Ascii(m, "192"))
            _props.AesKeyMode = 2;
          else if (StringsAreEqual_Ascii(m, "256") || m[0] == 0)
            _props.AesKeyMode = 3;
          else
            return E_INVALIDARG;
          _props.IsAesMode = true;
          m_ForceAesMode = true;
        }
        else if (StringsAreEqualNoCase_Ascii(m, "ZipCrypto"))
        {
          _props.IsAesMode = false;
          m_ForceAesMode = true;
        }
        else
          return E_INVALIDARG;
      }
    }
    else if (name.IsEqualTo("tc"))
    {
      RINOK(PROPVARIANT_to_bool(prop, m_WriteNtfsTimeExtra));
    }
    else if (name.IsEqualTo("cl"))
    {
      RINOK(PROPVARIANT_to_bool(prop, m_ForceLocal));
      if (m_ForceLocal)
        m_ForceUtf8 = false;
    }
    else if (name.IsEqualTo("cu"))
    {
      RINOK(PROPVARIANT_to_bool(prop, m_ForceUtf8));
      if (m_ForceUtf8)
        m_ForceLocal = false;
    }
    else if (name.IsEqualTo("cp"))
    {
      UInt32 cp = CP_OEMCP;
      RINOK(ParsePropToUInt32(L"", prop, cp));
      _forceCodePage = true;
      _specifiedCodePage = cp;
    }
    else if (name.IsEqualTo("rsfx"))
    {
      RINOK(PROPVARIANT_to_bool(prop, _removeSfxBlock));
    }
    else
    {
      if (name.IsEqualTo_Ascii_NoCase("m") && prop.vt == VT_UI4)
      {
        UInt32 id = prop.ulVal;
        if (id > 0xFF)
          return E_INVALIDARG;
        m_MainMethod = (int)id;
      }
      else
      {
        RINOK(_props.SetProperty(name, prop));
      }
      // RINOK(_props.MethodInfo.ParseParamsFromPROPVARIANT(name, prop));
    }
  }

  _props._methods.DeleteFrontal(_props.GetNumEmptyMethods());
  if (_props._methods.Size() > 1)
    return E_INVALIDARG;
  if (_props._methods.Size() == 1)
  {
    const AString &methodName = _props._methods[0].MethodName;

    if (!methodName.IsEmpty())
    {
      const char *end;
      UInt32 id = ConvertStringToUInt32(methodName, &end);
      if (*end == 0 && id <= 0xFF)
        m_MainMethod = (int)id;
      else if (methodName.IsEqualTo_Ascii_NoCase("Copy")) // it's alias for "Store"
        m_MainMethod = 0;
    }
  }
  
  return S_OK;
}

}}

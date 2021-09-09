// NSisHandler.cpp

#include "StdAfx.h"

#include "../../../../C/CpuArch.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/IntToString.h"

#include "../../../Windows/PropVariant.h"

#include "../../Common/ProgressUtils.h"
#include "../../Common/StreamUtils.h"

#include "../Common/ItemNameUtils.h"

#include "NsisHandler.h"

#define Get32(p) GetUi32(p)

using namespace NWindows;

namespace NArchive {
namespace NNsis {

#define kBcjMethod "BCJ"
#define kUnknownMethod "Unknown"

static const char * const kMethods[] =
{
    "Copy"
  , "Deflate"
  , "BZip2"
  , "LZMA"
};

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidPackSize,
  kpidMTime,
  kpidAttrib,
  kpidMethod,
  kpidSolid,
  kpidOffset
};

static const Byte kArcProps[] =
{
  kpidMethod,
  kpidSolid,
  kpidBit64,
  kpidHeadersSize,
  kpidEmbeddedStubSize,
  kpidSubType
  // kpidCodePage
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps


static AString UInt32ToString(UInt32 val)
{
  char s[16];
  ConvertUInt32ToString(val, s);
  return (AString)s;
}

static AString GetStringForSizeValue(UInt32 val)
{
  for (int i = 31; i >= 0; i--)
    if (((UInt32)1 << i) == val)
      return UInt32ToString(i);
  char c = 'b';
  if      ((val & ((1 << 20) - 1)) == 0) { val >>= 20; c = 'm'; }
  else if ((val & ((1 << 10) - 1)) == 0) { val >>= 10; c = 'k'; }
  return UInt32ToString(val) + c;
}

static AString GetMethod(bool useFilter, NMethodType::EEnum method, UInt32 dict)
{
  AString s;
  if (useFilter)
  {
    s += kBcjMethod;
    s.Add_Space();
  }
  s += ((unsigned)method < ARRAY_SIZE(kMethods)) ? kMethods[(unsigned)method] : kUnknownMethod;
  if (method == NMethodType::kLZMA)
  {
    s += ':';
    s += GetStringForSizeValue(dict);
  }
  return s;
}

/*
AString CHandler::GetMethod(NMethodType::EEnum method, bool useItemFilter, UInt32 dictionary) const
{
  AString s;
  if (_archive.IsSolid && _archive.UseFilter || !_archive.IsSolid && useItemFilter)
  {
    s += kBcjMethod;
    s.Add_Space();
  }
  s += (method < ARRAY_SIZE(kMethods)) ? kMethods[method] : kUnknownMethod;
  if (method == NMethodType::kLZMA)
  {
    s += ':';
    s += GetStringForSizeValue(_archive.IsSolid ? _archive.DictionarySize: dictionary);
  }
  return s;
}
*/

STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  switch (propID)
  {
    // case kpidCodePage: if (_archive.IsUnicode) prop = "UTF-16"; break;
    case kpidSubType:
    {
      AString s (_archive.GetFormatDescription());
      if (!_archive.IsInstaller)
      {
        s.Add_Space_if_NotEmpty();
        s += "(Uninstall)";
      }
      if (!s.IsEmpty())
        prop = s;
      break;
    }

    case kpidBit64: if (_archive.Is64Bit) prop = true; break;
    case kpidMethod: prop = _methodString; break;
    case kpidSolid: prop = _archive.IsSolid; break;
    case kpidOffset: prop = _archive.StartOffset; break;
    case kpidPhySize: prop = (UInt64)((UInt64)_archive.ExeStub.Size() + _archive.FirstHeader.ArcSize); break;
    case kpidEmbeddedStubSize: prop = (UInt64)_archive.ExeStub.Size(); break;
    case kpidHeadersSize: prop = _archive.FirstHeader.HeaderSize; break;
    
    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_archive.IsArc) v |= kpv_ErrorFlags_IsNotArc;
      if (_archive.IsTruncated()) v |= kpv_ErrorFlags_UnexpectedEnd;
      prop = v;
      break;
    }
    
    case kpidName:
    {
      AString s;
      
      #ifdef NSIS_SCRIPT
        if (!_archive.Name.IsEmpty())
          s = _archive.Name;
        if (!_archive.IsInstaller)
        {
          if (!s.IsEmpty())
            s += '.';
          s += "Uninstall";
        }
      #endif

      if (s.IsEmpty())
        s = _archive.IsInstaller ? "Install" : "Uninstall";
      s += (_archive.ExeStub.Size() == 0) ? ".nsis" : ".exe";

      prop = _archive.ConvertToUnicode(s);
      break;
    }
    
    #ifdef NSIS_SCRIPT
    case kpidShortComment:
    {
      if (!_archive.BrandingText.IsEmpty())
        prop = _archive.ConvertToUnicode(_archive.BrandingText);
      break;
    }
    #endif
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


STDMETHODIMP CHandler::Open(IInStream *stream, const UInt64 *maxCheckStartPosition, IArchiveOpenCallback * /* openArchiveCallback */)
{
  COM_TRY_BEGIN
  Close();
  {
    if (_archive.Open(stream, maxCheckStartPosition) != S_OK)
      return S_FALSE;
    {
      UInt32 dict = _archive.DictionarySize;
      if (!_archive.IsSolid)
      {
        FOR_VECTOR (i, _archive.Items)
        {
          const CItem &item = _archive.Items[i];
          if (item.DictionarySize > dict)
            dict = item.DictionarySize;
        }
      }
      _methodString = GetMethod(_archive.UseFilter, _archive.Method, dict);
    }
  }
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::Close()
{
  _archive.Clear();
  _archive.Release();
  return S_OK;
}

STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = _archive.Items.Size()
  #ifdef NSIS_SCRIPT
    + 1 + _archive.LicenseFiles.Size();
  #endif
  ;
  return S_OK;
}

bool CHandler::GetUncompressedSize(unsigned index, UInt32 &size) const
{
  size = 0;
  const CItem &item = _archive.Items[index];
  if (item.Size_Defined)
    size = item.Size;
  else if (_archive.IsSolid && item.EstimatedSize_Defined)
    size = item.EstimatedSize;
  else
    return false;
  return true;
}

bool CHandler::GetCompressedSize(unsigned index, UInt32 &size) const
{
  size = 0;
  const CItem &item = _archive.Items[index];
  if (item.CompressedSize_Defined)
    size = item.CompressedSize;
  else
  {
    if (_archive.IsSolid)
    {
      if (index == 0)
        size = _archive.FirstHeader.GetDataSize();
      else
        return false;
    }
    else
    {
      if (!item.IsCompressed)
        size = item.Size;
      else
        return false;
    }
  }
  return true;
}


STDMETHODIMP CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  #ifdef NSIS_SCRIPT
  if (index >= (UInt32)_archive.Items.Size())
  {
    if (index == (UInt32)_archive.Items.Size())
    {
      switch (propID)
      {
        case kpidPath: prop = "[NSIS].nsi"; break;
        case kpidSize:
        case kpidPackSize: prop = (UInt64)_archive.Script.Len(); break;
        case kpidSolid: prop = false; break;
      }
    }
    else
    {
      const CLicenseFile &lic = _archive.LicenseFiles[index - (_archive.Items.Size() + 1)];
      switch (propID)
      {
        case kpidPath: prop = lic.Name; break;
        case kpidSize:
        case kpidPackSize: prop = (UInt64)lic.Size; break;
        case kpidSolid: prop = false; break;
      }
    }
  }
  else
  #endif
  {
    const CItem &item = _archive.Items[index];
    switch (propID)
    {
      case kpidOffset: prop = item.Pos; break;
      case kpidPath:
      {
        UString s = NItemName::WinPathToOsPath(_archive.GetReducedName(index));
        if (!s.IsEmpty())
          prop = (const wchar_t *)s;
        break;
      }
      case kpidSize:
      {
        UInt32 size;
        if (GetUncompressedSize(index, size))
          prop = (UInt64)size;
        break;
      }
      case kpidPackSize:
      {
        UInt32 size;
        if (GetCompressedSize(index, size))
          prop = (UInt64)size;
        break;
      }
      case kpidMTime:
      {
        if (item.MTime.dwHighDateTime > 0x01000000 &&
            item.MTime.dwHighDateTime < 0xFF000000)
          prop = item.MTime;
        break;
      }
      case kpidAttrib:
      {
        if (item.Attrib_Defined)
          prop = item.Attrib;
        break;
      }
      
      case kpidMethod:
        if (_archive.IsSolid)
          prop = _methodString;
        else
          prop = GetMethod(_archive.UseFilter, item.IsCompressed ? _archive.Method :
              NMethodType::kCopy, item.DictionarySize);
        break;
      
      case kpidSolid:  prop = _archive.IsSolid; break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


static bool UninstallerPatch(const Byte *p, size_t size, CByteBuffer &dest)
{
  for (;;)
  {
    if (size < 4)
      return false;
    UInt32 len = Get32(p);
    if (len == 0)
      return size == 4;
    if (size < 8)
      return false;
    UInt32 offs = Get32(p + 4);
    p += 8;
    size -= 8;
    if (size < len || offs > dest.Size() || len > dest.Size() - offs)
      return false;
    memcpy(dest + offs, p, len);
    p += len;
    size -= len;
  }
}


STDMETHODIMP CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback)
{
  COM_TRY_BEGIN
  bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    GetNumberOfItems(&numItems);
  if (numItems == 0)
    return S_OK;
  
  UInt64 totalSize = 0;
  UInt64 solidPosMax = 0;

  UInt32 i;
  for (i = 0; i < numItems; i++)
  {
    UInt32 index = (allFilesMode ? i : indices[i]);
    
    #ifdef NSIS_SCRIPT
    if (index >= _archive.Items.Size())
    {
      if (index == _archive.Items.Size())
        totalSize += _archive.Script.Len();
      else
        totalSize += _archive.LicenseFiles[index - (_archive.Items.Size() + 1)].Size;
    }
    else
    #endif
    {
      UInt32 size;
      if (_archive.IsSolid)
      {
        GetUncompressedSize(index, size);
        UInt64 pos = (UInt64)_archive.GetPosOfSolidItem(index) + size;
        if (solidPosMax < pos)
          solidPosMax = pos;
      }
      else
      {
        GetCompressedSize(index, size);
        totalSize += size;
      }
    }
  }

  extractCallback->SetTotal(totalSize + solidPosMax);

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, !_archive.IsSolid);

  if (_archive.IsSolid)
  {
    RINOK(_archive.SeekTo_DataStreamOffset());
    RINOK(_archive.InitDecoder());
    _archive.Decoder.StreamPos = 0;
  }

  /* We use tempBuf for solid archives, if there is duplicate item.
     We don't know uncompressed size for non-solid archives, so we can't
     allocate exact buffer.
     We use tempBuf also for first part (EXE stub) of unistall.exe
     and tempBuf2 is used for second part (NSIS script). */

  CByteBuffer tempBuf;
  CByteBuffer tempBuf2;
  
  /* tempPos is pos in uncompressed stream of previous item for solid archive, that
     was written to tempBuf  */
  UInt64 tempPos = (UInt64)(Int64)-1;
  
  /* prevPos is pos in uncompressed stream of previous item for solid archive.
     It's used for test mode (where we don't need to test same file second time */
  UInt64 prevPos =  (UInt64)(Int64)-1;
  
  // if there is error in solid archive, we show error for all subsequent files
  bool solidDataError = false;

  UInt64 curTotalPacked = 0, curTotalUnpacked = 0;
  UInt32 curPacked = 0;
  UInt64 curUnpacked = 0;

  for (i = 0; i < numItems; i++,
      curTotalPacked += curPacked,
      curTotalUnpacked += curUnpacked)
  {
    lps->InSize = curTotalPacked;
    lps->OutSize = curTotalUnpacked;
    if (_archive.IsSolid)
      lps->OutSize += _archive.Decoder.StreamPos;

    curPacked = 0;
    curUnpacked = 0;
    RINOK(lps->SetCur());

    // RINOK(extractCallback->SetCompleted(&currentTotalSize));
    CMyComPtr<ISequentialOutStream> realOutStream;
    Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;
    const UInt32 index = allFilesMode ? i : indices[i];

    RINOK(extractCallback->GetStream(index, &realOutStream, askMode));

    bool dataError = false;

    #ifdef NSIS_SCRIPT
    if (index >= (UInt32)_archive.Items.Size())
    {
      const void *data;
      size_t size;
      if (index == (UInt32)_archive.Items.Size())
      {
        data = (const Byte *)_archive.Script;
        size = _archive.Script.Len();
      }
      else
      {
        CLicenseFile &lic = _archive.LicenseFiles[index - (_archive.Items.Size() + 1)];
        if (lic.Text.Size() != 0)
          data = lic.Text;
        else
          data = _archive._data + lic.Offset;
        size = lic.Size;
      }
      curUnpacked = size;
      if (!testMode && !realOutStream)
        continue;
      RINOK(extractCallback->PrepareOperation(askMode));
      if (realOutStream)
        RINOK(WriteStream(realOutStream, data, size));
    }
    else
    #endif
    {
      const CItem &item = _archive.Items[index];
      
      if (!_archive.IsSolid)
        GetCompressedSize(index, curPacked);
      
      if (!testMode && !realOutStream)
        continue;
      
      RINOK(extractCallback->PrepareOperation(askMode));
      
      dataError = solidDataError;

      bool needDecompress = !solidDataError;
      if (needDecompress)
      {
        if (testMode && _archive.IsSolid && _archive.GetPosOfSolidItem(index) == prevPos)
          needDecompress = false;
      }

      if (needDecompress)
      {
        bool writeToTemp = false;
        bool readFromTemp = false;

        if (!_archive.IsSolid)
        {
          RINOK(_archive.SeekToNonSolidItem(index));
        }
        else
        {
          UInt64 pos = _archive.GetPosOfSolidItem(index);
          if (pos < _archive.Decoder.StreamPos)
          {
            if (pos != tempPos)
              solidDataError = dataError = true;
            readFromTemp = true;
          }
          else
          {
            HRESULT res = _archive.Decoder.SetToPos(pos, progress);
            if (res != S_OK)
            {
              if (res != S_FALSE)
                return res;
              solidDataError = dataError = true;
            }
            else if (!testMode && i + 1 < numItems)
            {
              UInt32 next = allFilesMode ? i + 1 : indices[i + 1];
              if (next < _archive.Items.Size())
              {
                UInt64 nextPos = _archive.GetPosOfSolidItem(next);
                if (nextPos == pos)
                {
                  writeToTemp = true;
                  tempPos = pos;
                }
              }
            }
          }
          prevPos = pos;
        }

        if (!dataError)
        {
          // UInt32 unpackSize = 0;
          // bool unpackSize_Defined = false;
          bool writeToTemp1 = writeToTemp;
          if (item.IsUninstaller)
          {
            // unpackSize = item.PatchSize;
            // unpackSize_Defined = true;
            if (!readFromTemp)
              writeToTemp = true;
            writeToTemp1 = writeToTemp;
            if (_archive.ExeStub.Size() == 0)
            {
              if (writeToTemp1 && !readFromTemp)
                tempBuf.Free();
              writeToTemp1 = false;
            }
          }

          if (readFromTemp)
          {
            if (realOutStream && !item.IsUninstaller)
              RINOK(WriteStream(realOutStream, tempBuf, tempBuf.Size()));
          }
          else
          {
            UInt32 curUnpacked32 = 0;
            HRESULT res = _archive.Decoder.Decode(
                writeToTemp1 ? &tempBuf : NULL,
                item.IsUninstaller, item.PatchSize,
                item.IsUninstaller ? NULL : (ISequentialOutStream *)realOutStream,
                progress,
                curPacked, curUnpacked32);
            curUnpacked = curUnpacked32;
            if (_archive.IsSolid)
              curUnpacked = 0;
            if (res != S_OK)
            {
              if (res != S_FALSE)
                return res;
              dataError = true;
              if (_archive.IsSolid)
                solidDataError = true;
            }
          }
        }
        
        if (!dataError && item.IsUninstaller)
        {
          if (_archive.ExeStub.Size() != 0)
          {
            CByteBuffer destBuf = _archive.ExeStub;
            dataError = !UninstallerPatch(tempBuf, tempBuf.Size(), destBuf);
           
            if (realOutStream)
              RINOK(WriteStream(realOutStream, destBuf, destBuf.Size()));
          }
          
          if (readFromTemp)
          {
            if (realOutStream)
              RINOK(WriteStream(realOutStream, tempBuf2, tempBuf2.Size()));
          }
          else
          {
            UInt32 curPacked2 = 0;
            UInt32 curUnpacked2 = 0;

            if (!_archive.IsSolid)
            {
              RINOK(_archive.SeekTo(_archive.GetPosOfNonSolidItem(index) + 4 + curPacked ));
            }

            HRESULT res = _archive.Decoder.Decode(
                writeToTemp ? &tempBuf2 : NULL,
                false, 0,
                realOutStream,
                progress,
                curPacked2, curUnpacked2);
            curPacked += curPacked2;
            if (!_archive.IsSolid)
              curUnpacked += curUnpacked2;
            if (res != S_OK)
            {
              if (res != S_FALSE)
                return res;
              dataError = true;
              if (_archive.IsSolid)
                solidDataError = true;
            }
          }
        }
      }
    }
    realOutStream.Release();
    RINOK(extractCallback->SetOperationResult(dataError ?
        NExtract::NOperationResult::kDataError :
        NExtract::NOperationResult::kOK));
  }
  return S_OK;
  COM_TRY_END
}

}}

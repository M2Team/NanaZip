// WimHandler.cpp

#include "StdAfx.h"

#include "../../../../C/CpuArch.h"

#include "../../../Common/ComTry.h"
#include "../../../Common/IntToString.h"

#include "../../Common/MethodProps.h"
#include "../../Common/ProgressUtils.h"
#include "../../Common/StreamUtils.h"

#include "WimHandler.h"

#define Get16(p) GetUi16(p)
#define Get32(p) GetUi32(p)
#define Get64(p) GetUi64(p)

using namespace NWindows;

namespace NArchive {
namespace NWim {

#define FILES_DIR_NAME "[DELETED]"

// #define WIM_DETAILS

static const Byte kProps[] =
{
  kpidPath,
  kpidIsDir,
  kpidSize,
  kpidPackSize,
  kpidMTime,
  kpidCTime,
  kpidATime,
  kpidAttrib,
  kpidMethod,
  kpidSolid,
  kpidShortName,
  kpidINode,
  kpidLinks,
  kpidIsAltStream,
  kpidNumAltStreams,
  
  #ifdef WIM_DETAILS
  , kpidVolume
  , kpidOffset
  #endif
};

enum
{
  kpidNumImages = kpidUserDefined,
  kpidBootImage
};

static const CStatProp kArcProps[] =
{
  { NULL, kpidSize, VT_UI8},
  { NULL, kpidPackSize, VT_UI8},
  { NULL, kpidMethod, VT_BSTR},
  { NULL, kpidClusterSize, VT_UI4},
  { NULL, kpidCTime, VT_FILETIME},
  { NULL, kpidMTime, VT_FILETIME},
  { NULL, kpidComment, VT_BSTR},
  { NULL, kpidUnpackVer, VT_BSTR},
  { NULL, kpidIsVolume, VT_BOOL},
  { NULL, kpidVolume, VT_UI4},
  { NULL, kpidNumVolumes, VT_UI4},
  { "Images", kpidNumImages, VT_UI4},
  { "Boot Image", kpidBootImage, VT_UI4}
};


static const char * const k_Methods[] =
{
    "Copy"
  , "XPress"
  , "LZX"
  , "LZMS"
};



IMP_IInArchive_Props
IMP_IInArchive_ArcProps_WITH_NAME

static void AddErrorMessage(AString &s, const char *message)
{
  if (!s.IsEmpty())
    s += ". ";
  s += message;
}


STDMETHODIMP CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;

  const CImageInfo *image = NULL;
  if (_xmls.Size() == 1)
  {
    const CWimXml &xml = _xmls[0];
    if (xml.Images.Size() == 1)
      image = &xml.Images[0];
  }

  switch (propID)
  {
    case kpidPhySize:  prop = _phySize; break;
    case kpidSize: prop = _db.GetUnpackSize(); break;
    case kpidPackSize: prop = _db.GetPackSize(); break;
    
    case kpidCTime:
      if (_xmls.Size() == 1)
      {
        const CWimXml &xml = _xmls[0];
        int index = -1;
        FOR_VECTOR (i, xml.Images)
        {
          const CImageInfo &image2 = xml.Images[i];
          if (image2.CTimeDefined)
            if (index < 0 || ::CompareFileTime(&image2.CTime, &xml.Images[index].CTime) < 0)
              index = i;
        }
        if (index >= 0)
          prop = xml.Images[index].CTime;
      }
      break;

    case kpidMTime:
      if (_xmls.Size() == 1)
      {
        const CWimXml &xml = _xmls[0];
        int index = -1;
        FOR_VECTOR (i, xml.Images)
        {
          const CImageInfo &image2 = xml.Images[i];
          if (image2.MTimeDefined)
            if (index < 0 || ::CompareFileTime(&image2.MTime, &xml.Images[index].MTime) > 0)
              index = i;
        }
        if (index >= 0)
          prop = xml.Images[index].MTime;
      }
      break;

    case kpidComment:
      if (image)
      {
        if (_xmlInComments)
        {
          UString s;
          _xmls[0].ToUnicode(s);
          prop = s;
        }
        else if (image->NameDefined)
          prop = image->Name;
      }
      break;

    case kpidUnpackVer:
    {
      UInt32 ver1 = _version >> 16;
      UInt32 ver2 = (_version >> 8) & 0xFF;
      UInt32 ver3 = (_version) & 0xFF;

      AString res;
      res.Add_UInt32(ver1);
      res += '.';
      res.Add_UInt32(ver2);
      if (ver3 != 0)
      {
        res += '.';
        res.Add_UInt32(ver3);
      }
      prop = res;
      break;
    }

    case kpidIsVolume:
      if (_xmls.Size() > 0)
      {
        UInt16 volIndex = _xmls[0].VolIndex;
        if (volIndex < _volumes.Size())
          prop = (_volumes[volIndex].Header.NumParts > 1);
      }
      break;
    case kpidVolume:
      if (_xmls.Size() > 0)
      {
        UInt16 volIndex = _xmls[0].VolIndex;
        if (volIndex < _volumes.Size())
          prop = (UInt32)_volumes[volIndex].Header.PartNumber;
      }
      break;
    case kpidNumVolumes: if (_volumes.Size() > 0) prop = (UInt32)(_volumes.Size() - 1); break;
    
    case kpidClusterSize:
      if (_xmls.Size() > 0)
      {
        UInt16 volIndex = _xmls[0].VolIndex;
        if (volIndex < _volumes.Size())
        {
          const CHeader &h = _volumes[volIndex].Header;
          prop = (UInt32)1 << h.ChunkSizeBits;
        }
      }
      break;

    case kpidName:
      if (_firstVolumeIndex >= 0)
      {
        const CHeader &h = _volumes[_firstVolumeIndex].Header;
        if (GetUi32(h.Guid) != 0)
        {
          char temp[64];
          RawLeGuidToString(h.Guid, temp);
          temp[8] = 0; // for reduced GUID
          AString s (temp);
          const char *ext = ".wim";
          if (h.NumParts != 1)
          {
            s += '_';
            if (h.PartNumber != 1)
              s.Add_UInt32(h.PartNumber);
            ext = ".swm";
          }
          s += ext;
          prop = s;
        }
      }
      break;

    case kpidExtension:
      if (_firstVolumeIndex >= 0)
      {
        const CHeader &h = _volumes[_firstVolumeIndex].Header;
        if (h.NumParts > 1)
        {
          AString s;
          if (h.PartNumber != 1)
          {
            s.Add_UInt32(h.PartNumber);
            s += '.';
          }
          s += "swm";
          prop = s;
        }
      }
      break;

    case kpidNumImages: prop = (UInt32)_db.Images.Size(); break;
    case kpidBootImage: if (_bootIndex != 0) prop = (UInt32)_bootIndex; break;
    
    case kpidMethod:
    {
      UInt32 methodUnknown = 0;
      UInt32 methodMask = 0;
      unsigned chunkSizeBits = 0;
      
      {
        FOR_VECTOR (i, _xmls)
        {
          const CHeader &header = _volumes[_xmls[i].VolIndex].Header;
          unsigned method = header.GetMethod();
          if (method < ARRAY_SIZE(k_Methods))
            methodMask |= ((UInt32)1 << method);
          else
            methodUnknown = method;
          if (chunkSizeBits < header.ChunkSizeBits)
            chunkSizeBits = header.ChunkSizeBits;
        }
      }

      AString res;

      unsigned numMethods = 0;

      for (unsigned i = 0; i < ARRAY_SIZE(k_Methods); i++)
      {
        if (methodMask & ((UInt32)1 << i))
        {
          res.Add_Space_if_NotEmpty();
          res += k_Methods[i];
          numMethods++;
        }
      }

      if (methodUnknown != 0)
      {
        res.Add_Space_if_NotEmpty();
        res.Add_UInt32(methodUnknown);
        numMethods++;
      }

      if (numMethods == 1 && chunkSizeBits != 0)
      {
        res += ':';
        res.Add_UInt32((UInt32)chunkSizeBits);
      }

      prop = res;
      break;
    }
    
    case kpidIsTree: prop = true; break;
    case kpidIsAltStream: prop = _db.ThereAreAltStreams; break;
    case kpidIsAux: prop = true; break;
    // WIM uses special prefix to represent deleted items
    // case kpidIsDeleted: prop = _db.ThereAreDeletedStreams; break;
    case kpidINode: prop = true; break;

    case kpidErrorFlags:
    {
      UInt32 flags = 0;
      if (!_isArc) flags |= kpv_ErrorFlags_IsNotArc;
      if (_db.HeadersError) flags |= kpv_ErrorFlags_HeadersError;
      if (_unsupported) flags |= kpv_ErrorFlags_UnsupportedMethod;
      prop = flags;
      break;
    }

    case kpidWarning:
    {
      AString s;
      if (_xmlError)
        AddErrorMessage(s, "XML error");
      if (_db.RefCountError)
        AddErrorMessage(s, "Some files have incorrect reference count");
      if (!s.IsEmpty())
        prop = s;
      break;
    }

    case kpidReadOnly:
    {
      bool readOnly = !IsUpdateSupported();
      if (readOnly)
        prop = readOnly;
      break;
    }
  }

  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

static void GetFileTime(const Byte *p, NCOM::CPropVariant &prop)
{
  prop.vt = VT_FILETIME;
  prop.filetime.dwLowDateTime = Get32(p);
  prop.filetime.dwHighDateTime = Get32(p + 4);
}


static void MethodToProp(int method, int chunksSizeBits, NCOM::CPropVariant &prop)
{
  if (method >= 0)
  {
    char temp[32];
    
    if ((unsigned)method < ARRAY_SIZE(k_Methods))
      strcpy(temp, k_Methods[(unsigned)method]);
    else
      ConvertUInt32ToString((UInt32)(unsigned)method, temp);
    
    if (chunksSizeBits >= 0)
    {
      size_t pos = strlen(temp);
      temp[pos++] = ':';
      ConvertUInt32ToString((unsigned)chunksSizeBits, temp + pos);
    }
    
    prop = temp;
  }
}


STDMETHODIMP CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value)
{
  COM_TRY_BEGIN
  NCOM::CPropVariant prop;

  if (index < _db.SortedItems.Size())
  {
    unsigned realIndex = _db.SortedItems[index];
    const CItem &item = _db.Items[realIndex];
    const CStreamInfo *si = NULL;
    const CVolume *vol = NULL;
    if (item.StreamIndex >= 0)
    {
      si = &_db.DataStreams[item.StreamIndex];
      vol = &_volumes[si->PartNumber];
    }

    const CItem *mainItem = &item;
    if (item.IsAltStream)
      mainItem = &_db.Items[item.Parent];
    const Byte *metadata = NULL;
    if (mainItem->ImageIndex >= 0)
      metadata = _db.Images[mainItem->ImageIndex].Meta + mainItem->Offset;

    switch (propID)
    {
      case kpidPath:
        if (item.ImageIndex >= 0)
          _db.GetItemPath(realIndex, _showImageNumber, prop);
        else
        {
          /*
          while (s.Len() < _nameLenForStreams)
            s = '0' + s;
          */
          /*
          if (si->Resource.IsFree())
            s = (AString)("[Free]" STRING_PATH_SEPARATOR) + sz;
          else
          */
          AString s (FILES_DIR_NAME STRING_PATH_SEPARATOR);
          s.Add_UInt32(item.StreamIndex);
          prop = s;
        }
        break;
      
      case kpidName:
        if (item.ImageIndex >= 0)
          _db.GetItemName(realIndex, prop);
        else
        {
          char sz[16];
          ConvertUInt32ToString(item.StreamIndex, sz);
          /*
          AString s = sz;
          while (s.Len() < _nameLenForStreams)
            s = '0' + s;
          */
          prop = sz;
        }
        break;

      case kpidShortName:
        if (item.ImageIndex >= 0 && !item.IsAltStream)
          _db.GetShortName(realIndex, prop);
        break;

      case kpidPackSize:
      {
        if (si)
        {
          if (!si->Resource.IsSolidSmall())
            prop = si->Resource.PackSize;
          else
          {
            if (si->Resource.SolidIndex >= 0)
            {
              const CSolid &ss = _db.Solids[(unsigned)si->Resource.SolidIndex];
              if (ss.FirstSmallStream == item.StreamIndex)
                prop = _db.DataStreams[ss.StreamIndex].Resource.PackSize;
            }
          }
        }
        else if (!item.IsDir)
          prop = (UInt64)0;

        break;
      }

      case kpidSize:
      {
        if (si)
        {
          if (si->Resource.IsSolid())
          {
            if (si->Resource.IsSolidBig())
            {
              if (si->Resource.SolidIndex >= 0)
              {
                const CSolid &ss = _db.Solids[(unsigned)si->Resource.SolidIndex];
                prop = ss.UnpackSize;
              }
            }
            else
              prop = si->Resource.PackSize;
          }
          else
            prop = si->Resource.UnpackSize;
        }
        else if (!item.IsDir)
          prop = (UInt64)0;

        break;
      }
      
      case kpidIsDir: prop = item.IsDir; break;
      case kpidIsAltStream: prop = item.IsAltStream; break;
      case kpidNumAltStreams:
      {
        if (!item.IsAltStream && mainItem->HasMetadata())
        {
          UInt32 dirRecordSize = _db.IsOldVersion ? kDirRecordSizeOld : kDirRecordSize;
          UInt32 numAltStreams = Get16(metadata + dirRecordSize - 6);
          if (numAltStreams != 0)
          {
            if (!item.IsDir)
              numAltStreams--;
            prop = numAltStreams;
          }
        }
        break;
      }

      case kpidAttrib:
        if (!item.IsAltStream && mainItem->ImageIndex >= 0)
        {
          /*
          if (fileNameLen == 0 && isDir && !item.HasStream())
            item.Attrib = 0x10; // some swm archives have system/hidden attributes for root
          */
          prop = (UInt32)Get32(metadata + 8);
        }
        break;
      case kpidCTime: if (mainItem->HasMetadata()) GetFileTime(metadata + (_db.IsOldVersion ? 0x18: 0x28), prop); break;
      case kpidATime: if (mainItem->HasMetadata()) GetFileTime(metadata + (_db.IsOldVersion ? 0x20: 0x30), prop); break;
      case kpidMTime: if (mainItem->HasMetadata()) GetFileTime(metadata + (_db.IsOldVersion ? 0x28: 0x38), prop); break;

      case kpidINode:
        if (mainItem->HasMetadata() && !_isOldVersion)
        {
          UInt32 attrib = (UInt32)Get32(metadata + 8);
          if ((attrib & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
          {
            // we don't know about that field in OLD WIM format
            unsigned offset = 0x58; // (_db.IsOldVersion ? 0x30: 0x58);
            UInt64 val = Get64(metadata + offset);
            if (val != 0)
              prop = val;
          }
        }
        break;

      case kpidStreamId:
        if (item.StreamIndex >= 0)
          prop = (UInt32)item.StreamIndex;
        break;

      case kpidMethod:
          if (si)
          {
            const CResource &r = si->Resource;
            if (r.IsSolid())
            {
              if (r.SolidIndex >= 0)
              {
                CSolid &ss = _db.Solids[r.SolidIndex];
                MethodToProp(ss.Method, ss.ChunkSizeBits, prop);
              }
            }
            else
            {
              int method = 0;
              int chunkSizeBits = -1;
              if (r.IsCompressed())
              {
                method = vol->Header.GetMethod();
                chunkSizeBits = vol->Header.ChunkSizeBits;
              }
              MethodToProp(method, chunkSizeBits, prop);
            }
          }
          break;

      case kpidSolid: if (si) prop = si->Resource.IsSolid(); break;
      case kpidLinks: if (si) prop = (UInt32)si->RefCount; break;
      #ifdef WIM_DETAILS
      case kpidVolume: if (si) prop = (UInt32)si->PartNumber; break;
      case kpidOffset: if (si)  prop = (UInt64)si->Resource.Offset; break;
      #endif
    }
  }
  else
  {
    index -= _db.SortedItems.Size();
    if (index < _numXmlItems)
    {
      switch (propID)
      {
        case kpidPath:
        case kpidName: prop = _xmls[index].FileName; break;
        case kpidIsDir: prop = false; break;
        case kpidPackSize:
        case kpidSize: prop = (UInt64)_xmls[index].Data.Size(); break;
        case kpidMethod: /* prop = k_Method_Copy; */ break;
      }
    }
    else
    {
      index -= _numXmlItems;
      switch (propID)
      {
        case kpidPath:
        case kpidName:
          if (index < (UInt32)_db.VirtualRoots.Size())
            prop = _db.Images[_db.VirtualRoots[index]].RootName;
          else
            prop = FILES_DIR_NAME;
          break;
        case kpidIsDir: prop = true; break;
        case kpidIsAux: prop = true; break;
      }
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}

STDMETHODIMP CHandler::GetRootProp(PROPID propID, PROPVARIANT *value)
{
  // COM_TRY_BEGIN
  NCOM::CPropVariant prop;
  if (_db.Images.Size() != 0 && _db.NumExcludededItems != 0)
  {
    const CImage &image = _db.Images[_db.IndexOfUserImage];
    const CItem &item = _db.Items[image.StartItem];
    if (!item.IsDir || item.ImageIndex != _db.IndexOfUserImage)
      return E_FAIL;
    const Byte *metadata = image.Meta + item.Offset;

    switch (propID)
    {
      case kpidIsDir: prop = true; break;
      case kpidAttrib: prop = (UInt32)Get32(metadata + 8); break;
      case kpidCTime: GetFileTime(metadata + (_db.IsOldVersion ? 0x18: 0x28), prop); break;
      case kpidATime: GetFileTime(metadata + (_db.IsOldVersion ? 0x20: 0x30), prop); break;
      case kpidMTime: GetFileTime(metadata + (_db.IsOldVersion ? 0x28: 0x38), prop); break;
    }
  }
  prop.Detach(value);
  return S_OK;
  // COM_TRY_END
}

HRESULT CHandler::GetSecurity(UInt32 realIndex, const void **data, UInt32 *dataSize, UInt32 *propType)
{
  const CItem &item = _db.Items[realIndex];
  if (item.IsAltStream || item.ImageIndex < 0)
    return S_OK;
  const CImage &image = _db.Images[item.ImageIndex];
  const Byte *metadata = image.Meta + item.Offset;
  UInt32 securityId = Get32(metadata + 0xC);
  if (securityId == (UInt32)(Int32)-1)
    return S_OK;
  if (securityId >= (UInt32)image.SecurOffsets.Size())
    return E_FAIL;
  UInt32 offs = image.SecurOffsets[securityId];
  UInt32 len = image.SecurOffsets[securityId + 1] - offs;
  const CByteBuffer &buf = image.Meta;
  if (offs <= buf.Size() && buf.Size() - offs >= len)
  {
    *data = buf + offs;
    *dataSize = len;
    *propType = NPropDataType::kRaw;
  }
  return S_OK;
}

STDMETHODIMP CHandler::GetRootRawProp(PROPID propID, const void **data, UInt32 *dataSize, UInt32 *propType)
{
  *data = 0;
  *dataSize = 0;
  *propType = 0;
  if (propID == kpidNtSecure && _db.Images.Size() != 0 && _db.NumExcludededItems != 0)
  {
    const CImage &image = _db.Images[_db.IndexOfUserImage];
    const CItem &item = _db.Items[image.StartItem];
    if (!item.IsDir || item.ImageIndex != _db.IndexOfUserImage)
      return E_FAIL;
    return GetSecurity(image.StartItem, data, dataSize, propType);
  }
  return S_OK;
}

static const Byte kRawProps[] =
{
  kpidSha1,
  kpidNtReparse,
  kpidNtSecure
};


STDMETHODIMP CHandler::GetNumRawProps(UInt32 *numProps)
{
  *numProps = ARRAY_SIZE(kRawProps);
  return S_OK;
}

STDMETHODIMP CHandler::GetRawPropInfo(UInt32 index, BSTR *name, PROPID *propID)
{
  *propID = kRawProps[index];
  *name = 0;
  return S_OK;
}

STDMETHODIMP CHandler::GetParent(UInt32 index, UInt32 *parent, UInt32 *parentType)
{
  *parentType = NParentType::kDir;
  *parent = (UInt32)(Int32)-1;
  if (index >= _db.SortedItems.Size())
    return S_OK;

  const CItem &item = _db.Items[_db.SortedItems[index]];
  
  if (item.ImageIndex >= 0)
  {
    *parentType = item.IsAltStream ? NParentType::kAltStream : NParentType::kDir;
    if (item.Parent >= 0)
    {
      if (_db.ExludedItem != item.Parent)
        *parent = _db.Items[item.Parent].IndexInSorted;
    }
    else
    {
      CImage &image = _db.Images[item.ImageIndex];
      if (image.VirtualRootIndex >= 0)
        *parent = _db.SortedItems.Size() + _numXmlItems + image.VirtualRootIndex;
    }
  }
  else
    *parent = _db.SortedItems.Size() + _numXmlItems + _db.VirtualRoots.Size();
  return S_OK;
}

STDMETHODIMP CHandler::GetRawProp(UInt32 index, PROPID propID, const void **data, UInt32 *dataSize, UInt32 *propType)
{
  *data = NULL;
  *dataSize = 0;
  *propType = 0;

  if (propID == kpidName)
  {
    if (index < _db.SortedItems.Size())
    {
      const CItem &item = _db.Items[_db.SortedItems[index]];
      if (item.ImageIndex < 0)
        return S_OK;
      const CImage &image = _db.Images[item.ImageIndex];
      *propType = NPropDataType::kUtf16z;
      if (image.NumEmptyRootItems != 0 && item.Parent < 0)
      {
        const CByteBuffer &buf = _db.Images[item.ImageIndex].RootNameBuf;
        *data = (void *)(const Byte *)buf;
        *dataSize = (UInt32)buf.Size();
        return S_OK;
      }
      const Byte *meta = image.Meta + item.Offset +
          (item.IsAltStream ?
          (_isOldVersion ? 0x10 : 0x24) :
          (_isOldVersion ? kDirRecordSizeOld - 2 : kDirRecordSize - 2));
      *data = (const void *)(meta + 2);
      *dataSize = (UInt32)Get16(meta) + 2;
      return S_OK;
    }
    {
      index -= _db.SortedItems.Size();
      if (index < _numXmlItems)
        return S_OK;
      index -= _numXmlItems;
      if (index >= (UInt32)_db.VirtualRoots.Size())
        return S_OK;
      const CByteBuffer &buf = _db.Images[_db.VirtualRoots[index]].RootNameBuf;
      *data = (void *)(const Byte *)buf;
      *dataSize = (UInt32)buf.Size();
      *propType = NPropDataType::kUtf16z;
      return S_OK;
    }
  }

  if (index >= _db.SortedItems.Size())
    return S_OK;

  unsigned index2 = _db.SortedItems[index];
  
  if (propID == kpidNtSecure)
  {
    return GetSecurity(index2, data, dataSize, propType);
  }
  
  const CItem &item = _db.Items[index2];
  if (propID == kpidSha1)
  {
    if (item.StreamIndex >= 0)
      *data = _db.DataStreams[item.StreamIndex].Hash;
    else
    {
      if (_isOldVersion)
        return S_OK;
      const Byte *sha1 = _db.Images[item.ImageIndex].Meta + item.Offset + (item.IsAltStream ? 0x10 : 0x40);
      if (IsEmptySha(sha1))
        return S_OK;
      *data = sha1;
    }
    *dataSize = kHashSize;
    *propType = NPropDataType::kRaw;
    return S_OK;
  }
  
  if (propID == kpidNtReparse && !_isOldVersion)
  {
    // we don't know about Reparse field in OLD WIM format

    if (item.StreamIndex < 0)
      return S_OK;
    if (index2 >= _db.ItemToReparse.Size())
      return S_OK;
    int reparseIndex = _db.ItemToReparse[index2];
    if (reparseIndex < 0)
      return S_OK;
    const CByteBuffer &buf = _db.ReparseItems[reparseIndex];
    if (buf.Size() == 0)
      return S_OK;
    *data = buf;
    *dataSize = (UInt32)buf.Size();
    *propType = NPropDataType::kRaw;
    return S_OK;
  }

  return S_OK;
}

class CVolumeName
{
  UString _before;
  UString _after;
public:
  void InitName(const UString &name)
  {
    int dotPos = name.ReverseFind_Dot();
    if (dotPos < 0)
      dotPos = name.Len();
    _before = name.Left(dotPos);
    _after = name.Ptr(dotPos);
  }

  UString GetNextName(UInt32 index) const
  {
    UString s = _before;
    s.Add_UInt32(index);
    s += _after;
    return s;
  }
};

STDMETHODIMP CHandler::Open(IInStream *inStream, const UInt64 *, IArchiveOpenCallback *callback)
{
  COM_TRY_BEGIN

  Close();
  {
    CMyComPtr<IArchiveOpenVolumeCallback> openVolumeCallback;
    
    CVolumeName seqName;
    if (callback)
      callback->QueryInterface(IID_IArchiveOpenVolumeCallback, (void **)&openVolumeCallback);

    UInt32 numVolumes = 1;
    
    for (UInt32 i = 1; i <= numVolumes; i++)
    {
      CMyComPtr<IInStream> curStream;
      
      if (i == 1)
        curStream = inStream;
      else
      {
        if (!openVolumeCallback)
          continue;
        const UString fullName = seqName.GetNextName(i);
        const HRESULT result = openVolumeCallback->GetStream(fullName, &curStream);
        if (result == S_FALSE)
          continue;
        if (result != S_OK)
          return result;
        if (!curStream)
          break;
      }
      
      CHeader header;
      HRESULT res = NWim::ReadHeader(curStream, header, _phySize);
      
      if (res != S_OK)
      {
        if (i != 1 && res == S_FALSE)
          continue;
        return res;
      }
      
      _isArc = true;
      _bootIndex = header.BootIndex;
      _version = header.Version;
      _isOldVersion = header.IsOldVersion();
      if (_firstVolumeIndex >= 0)
        if (!header.AreFromOnArchive(_volumes[_firstVolumeIndex].Header))
          break;
      if (_volumes.Size() > header.PartNumber && _volumes[header.PartNumber].Stream)
        break;
      CWimXml xml;
      xml.VolIndex = header.PartNumber;
      res = _db.OpenXml(curStream, header, xml.Data);
      
      if (res == S_OK)
      {
        if (!xml.Parse())
          _xmlError = true;

        if (xml.IsEncrypted)
        {
          _unsupported = true;
          return S_FALSE;
        }

        UInt64 totalFiles = xml.GetTotalFilesAndDirs() + xml.Images.Size();
        totalFiles += 16 + xml.Images.Size() * 4; // we reserve some additional items
        if (totalFiles >= ((UInt32)1 << 30))
          totalFiles = 0;
        res = _db.Open(curStream, header, (unsigned)totalFiles, callback);
      }
      
      if (res != S_OK)
      {
        if (i != 1 && res == S_FALSE)
          continue;
        return res;
      }
      
      while (_volumes.Size() <= header.PartNumber)
        _volumes.AddNew();
      CVolume &volume = _volumes[header.PartNumber];
      volume.Header = header;
      volume.Stream = curStream;
      
      _firstVolumeIndex = header.PartNumber;
      
      if (_xmls.IsEmpty() || xml.Data != _xmls[0].Data)
      {
        xml.FileName = '[';
        xml.FileName.Add_UInt32(xml.VolIndex);
        xml.FileName += "].xml";
        _xmls.Add(xml);
      }
      
      if (i == 1)
      {
        if (header.PartNumber != 1)
          break;
        if (!openVolumeCallback)
          break;
        numVolumes = header.NumParts;
        {
          NCOM::CPropVariant prop;
          RINOK(openVolumeCallback->GetProperty(kpidName, &prop));
          if (prop.vt != VT_BSTR)
            break;
          seqName.InitName(prop.bstrVal);
        }
      }
    }

    RINOK(_db.FillAndCheck(_volumes));
    int defaultImageIndex = (int)_defaultImageNumber - 1;
    
    bool showImageNumber = (_db.Images.Size() != 1 && defaultImageIndex < 0);
    if (!showImageNumber && _set_use_ShowImageNumber)
      showImageNumber = _set_showImageNumber;

    if (!showImageNumber && _keepMode_ShowImageNumber)
      showImageNumber = true;

    _showImageNumber = showImageNumber;

    RINOK(_db.GenerateSortedItems(defaultImageIndex, showImageNumber));
    RINOK(_db.ExtractReparseStreams(_volumes, callback));

    /*
    wchar_t sz[16];
    ConvertUInt32ToString(_db.DataStreams.Size(), sz);
    _nameLenForStreams = MyStringLen(sz);
    */

    _xmlInComments = !_showImageNumber;
    _numXmlItems = (_xmlInComments ? 0 : _xmls.Size());
    _numIgnoreItems = _db.ThereAreDeletedStreams ? 1 : 0;
  }
  return S_OK;
  COM_TRY_END
}


STDMETHODIMP CHandler::Close()
{
  _firstVolumeIndex = -1;
  _phySize = 0;
  _db.Clear();
  _volumes.Clear();
  _xmls.Clear();
  // _nameLenForStreams = 0;
  _xmlInComments = false;
  _numXmlItems = 0;
  _numIgnoreItems = 0;
  _xmlError = false;
  _isArc = false;
  _unsupported = false;
  return S_OK;
}


STDMETHODIMP CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback)
{
  COM_TRY_BEGIN
  bool allFilesMode = (numItems == (UInt32)(Int32)-1);

  if (allFilesMode)
    numItems = _db.SortedItems.Size() + _numXmlItems + _db.VirtualRoots.Size() + _numIgnoreItems;
  if (numItems == 0)
    return S_OK;

  UInt32 i;
  UInt64 totalSize = 0;

  for (i = 0; i < numItems; i++)
  {
    UInt32 index = allFilesMode ? i : indices[i];
    if (index < _db.SortedItems.Size())
    {
      int streamIndex = _db.Items[_db.SortedItems[index]].StreamIndex;
      if (streamIndex >= 0)
      {
        const CStreamInfo &si = _db.DataStreams[streamIndex];
        totalSize += _db.Get_UnpackSize_of_Resource(si.Resource);
      }
    }
    else
    {
      index -= _db.SortedItems.Size();
      if (index < (UInt32)_numXmlItems)
        totalSize += _xmls[index].Data.Size();
    }
  }

  RINOK(extractCallback->SetTotal(totalSize));

  UInt64 currentTotalUnPacked = 0;
  UInt64 currentItemUnPacked;
  
  int prevSuccessStreamIndex = -1;

  CUnpacker unpacker;

  CLocalProgress *lps = new CLocalProgress;
  CMyComPtr<ICompressProgressInfo> progress = lps;
  lps->Init(extractCallback, false);

  for (i = 0;; i++,
      currentTotalUnPacked += currentItemUnPacked)
  {
    currentItemUnPacked = 0;

    lps->InSize = unpacker.TotalPacked;
    lps->OutSize = currentTotalUnPacked;

    RINOK(lps->SetCur());

    if (i >= numItems)
      break;

    UInt32 index = allFilesMode ? i : indices[i];
    Int32 askMode = testMode ?
        NExtract::NAskMode::kTest :
        NExtract::NAskMode::kExtract;

    CMyComPtr<ISequentialOutStream> realOutStream;
    RINOK(extractCallback->GetStream(index, &realOutStream, askMode));

    if (index >= _db.SortedItems.Size())
    {
      if (!testMode && !realOutStream)
        continue;
      RINOK(extractCallback->PrepareOperation(askMode));
      index -= _db.SortedItems.Size();
      if (index < (UInt32)_numXmlItems)
      {
        const CByteBuffer &data = _xmls[index].Data;
        currentItemUnPacked = data.Size();
        if (realOutStream)
        {
          RINOK(WriteStream(realOutStream, (const Byte *)data, data.Size()));
          realOutStream.Release();
        }
      }
      RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK));
      continue;
    }

    const CItem &item = _db.Items[_db.SortedItems[index]];
    int streamIndex = item.StreamIndex;
    if (streamIndex < 0)
    {
      if (!item.IsDir)
        if (!testMode && !realOutStream)
          continue;
      RINOK(extractCallback->PrepareOperation(askMode));
      realOutStream.Release();
      RINOK(extractCallback->SetOperationResult(!item.IsDir && _db.ItemHasStream(item) ?
          NExtract::NOperationResult::kDataError :
          NExtract::NOperationResult::kOK));
      continue;
    }

    const CStreamInfo &si = _db.DataStreams[streamIndex];
    currentItemUnPacked = _db.Get_UnpackSize_of_Resource(si.Resource);
    // currentItemPacked = _db.Get_PackSize_of_Resource(streamIndex);

    if (!testMode && !realOutStream)
      continue;
    RINOK(extractCallback->PrepareOperation(askMode));
    Int32 opRes = NExtract::NOperationResult::kOK;
    
    if (streamIndex != prevSuccessStreamIndex || realOutStream)
    {
      Byte digest[kHashSize];
      const CVolume &vol = _volumes[si.PartNumber];
      bool needDigest = !si.IsEmptyHash();
      
      HRESULT res = unpacker.Unpack(vol.Stream, si.Resource, vol.Header, &_db,
          realOutStream, progress, needDigest ? digest : NULL);
      
      if (res == S_OK)
      {
        if (!needDigest || memcmp(digest, si.Hash, kHashSize) == 0)
          prevSuccessStreamIndex = streamIndex;
        else
          opRes = NExtract::NOperationResult::kCRCError;
      }
      else if (res == S_FALSE)
        opRes = NExtract::NOperationResult::kDataError;
      else if (res == E_NOTIMPL)
        opRes = NExtract::NOperationResult::kUnsupportedMethod;
      else
        return res;
    }
    
    realOutStream.Release();
    RINOK(extractCallback->SetOperationResult(opRes));
  }
  
  return S_OK;
  COM_TRY_END
}


STDMETHODIMP CHandler::GetNumberOfItems(UInt32 *numItems)
{
  *numItems = _db.SortedItems.Size() +
      _numXmlItems +
      _db.VirtualRoots.Size() +
      _numIgnoreItems;
  return S_OK;
}

CHandler::CHandler()
{
  _keepMode_ShowImageNumber = false;
  InitDefaults();
  _xmlError = false;
}

STDMETHODIMP CHandler::SetProperties(const wchar_t * const *names, const PROPVARIANT *values, UInt32 numProps)
{
  InitDefaults();

  for (UInt32 i = 0; i < numProps; i++)
  {
    UString name = names[i];
    name.MakeLower_Ascii();
    if (name.IsEmpty())
      return E_INVALIDARG;

    const PROPVARIANT &prop = values[i];

    if (name[0] == L'x')
    {
      // some clients write 'x' property. So we support it
      UInt32 level = 0;
      RINOK(ParsePropToUInt32(name.Ptr(1), prop, level));
    }
    else if (name.IsEqualTo("is"))
    {
      RINOK(PROPVARIANT_to_bool(prop, _set_showImageNumber));
      _set_use_ShowImageNumber = true;
    }
    else if (name.IsEqualTo("im"))
    {
      UInt32 image = 9;
      RINOK(ParsePropToUInt32(L"", prop, image));
      _defaultImageNumber = image;
    }
    else
      return E_INVALIDARG;
  }
  return S_OK;
}

STDMETHODIMP CHandler::KeepModeForNextOpen()
{
  _keepMode_ShowImageNumber = _showImageNumber;
  return S_OK;
}

}}

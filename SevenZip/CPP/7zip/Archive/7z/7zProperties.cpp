// 7zProperties.cpp

#include "StdAfx.h"

#include "7zProperties.h"
#include "7zHeader.h"
#include "7zHandler.h"

// #define _MULTI_PACK

namespace NArchive {
namespace N7z {

struct CPropMap
{
  UInt32 FilePropID;
  CStatProp StatProp;
};

static const CPropMap kPropMap[] =
{
  { NID::kName, { NULL, kpidPath, VT_BSTR } },
  { NID::kSize, { NULL, kpidSize, VT_UI8 } },
  { NID::kPackInfo, { NULL, kpidPackSize, VT_UI8 } },
  
  #ifdef _MULTI_PACK
  { 100, { "Pack0", kpidPackedSize0, VT_UI8 } },
  { 101, { "Pack1", kpidPackedSize1, VT_UI8 } },
  { 102, { "Pack2", kpidPackedSize2, VT_UI8 } },
  { 103, { "Pack3", kpidPackedSize3, VT_UI8 } },
  { 104, { "Pack4", kpidPackedSize4, VT_UI8 } },
  #endif

  { NID::kCTime, { NULL, kpidCTime, VT_FILETIME } },
  { NID::kMTime, { NULL, kpidMTime, VT_FILETIME } },
  { NID::kATime, { NULL, kpidATime, VT_FILETIME } },
  { NID::kWinAttrib, { NULL, kpidAttrib, VT_UI4 } },
  { NID::kStartPos, { NULL, kpidPosition, VT_UI8 } },

  { NID::kCRC, { NULL, kpidCRC, VT_UI4 } },
  
//  { NID::kIsAux, { NULL, kpidIsAux, VT_BOOL } },
  { NID::kAnti, { NULL, kpidIsAnti, VT_BOOL } }

  #ifndef _SFX
  ,
  { 97, { NULL, kpidEncrypted, VT_BOOL } },
  { 98, { NULL, kpidMethod, VT_BSTR } },
  { 99, { NULL, kpidBlock, VT_UI4 } }
  #endif
};

static void CopyOneItem(CRecordVector<UInt64> &src,
    CRecordVector<UInt64> &dest, UInt32 item)
{
  FOR_VECTOR (i, src)
    if (src[i] == item)
    {
      dest.Add(item);
      src.Delete(i);
      return;
    }
}

static void RemoveOneItem(CRecordVector<UInt64> &src, UInt32 item)
{
  FOR_VECTOR (i, src)
    if (src[i] == item)
    {
      src.Delete(i);
      return;
    }
}

static void InsertToHead(CRecordVector<UInt64> &dest, UInt32 item)
{
  FOR_VECTOR (i, dest)
    if (dest[i] == item)
    {
      dest.Delete(i);
      break;
    }
  dest.Insert(0, item);
}

#define COPY_ONE_ITEM(id) CopyOneItem(fileInfoPopIDs, _fileInfoPopIDs, NID::id);

void CHandler::FillPopIDs()
{
  _fileInfoPopIDs.Clear();

  #ifdef _7Z_VOL
  if (_volumes.Size() < 1)
    return;
  const CVolume &volume = _volumes.Front();
  const CArchiveDatabaseEx &_db = volume.Database;
  #endif

  CRecordVector<UInt64> fileInfoPopIDs = _db.ArcInfo.FileInfoPopIDs;

  RemoveOneItem(fileInfoPopIDs, NID::kEmptyStream);
  RemoveOneItem(fileInfoPopIDs, NID::kEmptyFile);
  /*
  RemoveOneItem(fileInfoPopIDs, NID::kParent);
  RemoveOneItem(fileInfoPopIDs, NID::kNtSecure);
  */

  COPY_ONE_ITEM(kName);
  COPY_ONE_ITEM(kAnti);
  COPY_ONE_ITEM(kSize);
  COPY_ONE_ITEM(kPackInfo);
  COPY_ONE_ITEM(kCTime);
  COPY_ONE_ITEM(kMTime);
  COPY_ONE_ITEM(kATime);
  COPY_ONE_ITEM(kWinAttrib);
  COPY_ONE_ITEM(kCRC);
  COPY_ONE_ITEM(kComment);

  _fileInfoPopIDs += fileInfoPopIDs;
 
  #ifndef _SFX
  _fileInfoPopIDs.Add(97);
  _fileInfoPopIDs.Add(98);
  _fileInfoPopIDs.Add(99);
  #endif

  #ifdef _MULTI_PACK
  _fileInfoPopIDs.Add(100);
  _fileInfoPopIDs.Add(101);
  _fileInfoPopIDs.Add(102);
  _fileInfoPopIDs.Add(103);
  _fileInfoPopIDs.Add(104);
  #endif

  #ifndef _SFX
  InsertToHead(_fileInfoPopIDs, NID::kMTime);
  InsertToHead(_fileInfoPopIDs, NID::kPackInfo);
  InsertToHead(_fileInfoPopIDs, NID::kSize);
  InsertToHead(_fileInfoPopIDs, NID::kName);
  #endif
}

STDMETHODIMP CHandler::GetNumberOfProperties(UInt32 *numProps)
{
  *numProps = _fileInfoPopIDs.Size();
  return S_OK;
}

STDMETHODIMP CHandler::GetPropertyInfo(UInt32 index, BSTR *name, PROPID *propID, VARTYPE *varType)
{
  if (index >= _fileInfoPopIDs.Size())
    return E_INVALIDARG;
  UInt64 id = _fileInfoPopIDs[index];
  for (unsigned i = 0; i < ARRAY_SIZE(kPropMap); i++)
  {
    const CPropMap &pr = kPropMap[i];
    if (pr.FilePropID == id)
    {
      const CStatProp &st = pr.StatProp;
      *propID = st.PropID;
      *varType = st.vt;
      /*
      if (st.lpwstrName)
        *name = ::SysAllocString(st.lpwstrName);
      else
      */
        *name = NULL;
      return S_OK;
    }
  }
  return E_INVALIDARG;
}

}}

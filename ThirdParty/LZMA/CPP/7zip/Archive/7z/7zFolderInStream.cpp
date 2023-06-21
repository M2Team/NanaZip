// 7zFolderInStream.cpp

#include "StdAfx.h"

#include "../../../Windows/TimeUtils.h"

#include "7zFolderInStream.h"

namespace NArchive {
namespace N7z {

void CFolderInStream::Init(IArchiveUpdateCallback *updateCallback,
    const UInt32 *indexes, unsigned numFiles)
{
  _updateCallback = updateCallback;
  _indexes = indexes;
  _numFiles = numFiles;
  
  Processed.ClearAndReserve(numFiles);
  CRCs.ClearAndReserve(numFiles);
  Sizes.ClearAndReserve(numFiles);

  if (Need_CTime) CTimes.ClearAndReserve(numFiles);
  if (Need_ATime) ATimes.ClearAndReserve(numFiles);
  if (Need_MTime) MTimes.ClearAndReserve(numFiles);
  if (Need_Attrib) Attribs.ClearAndReserve(numFiles);
  TimesDefined.ClearAndReserve(numFiles);
  
  _stream.Release();
}

HRESULT CFolderInStream::OpenStream()
{
  _pos = 0;
  _crc = CRC_INIT_VAL;
  _size_Defined = false;
  _times_Defined = false;
  _size = 0;
  FILETIME_Clear(_cTime);
  FILETIME_Clear(_aTime);
  FILETIME_Clear(_mTime);
  _attrib = 0;

  while (Processed.Size() < _numFiles)
  {
    CMyComPtr<ISequentialInStream> stream;
    const HRESULT result = _updateCallback->GetStream(_indexes[Processed.Size()], &stream);
    if (result != S_OK && result != S_FALSE)
      return result;

    _stream = stream;
    
    if (stream)
    {
      {
        CMyComPtr<IStreamGetProps> getProps;
        stream.QueryInterface(IID_IStreamGetProps, (void **)&getProps);
        if (getProps)
        {
          // access could be changed in first myx pass
          if (getProps->GetProps(&_size,
              Need_CTime ? &_cTime : NULL,
              Need_ATime ? &_aTime : NULL,
              Need_MTime ? &_mTime : NULL,
              Need_Attrib ? &_attrib : NULL)
              == S_OK)
          {
            _size_Defined = true;
            _times_Defined = true;
          }
          return S_OK;
        }
      }
      {
        CMyComPtr<IStreamGetSize> streamGetSize;
        stream.QueryInterface(IID_IStreamGetSize, &streamGetSize);
        if (streamGetSize)
        {
          if (streamGetSize->GetSize(&_size) == S_OK)
            _size_Defined = true;
        }
        return S_OK;
      }
    }
    
    RINOK(AddFileInfo(result == S_OK));
  }
  return S_OK;
}

static void AddFt(CRecordVector<UInt64> &vec, const FILETIME &ft)
{
  vec.AddInReserved(FILETIME_To_UInt64(ft));
}

/*
HRESULT ReportItemProps(IArchiveUpdateCallbackArcProp *reportArcProp,
    UInt32 index, UInt64 size, const UInt32 *crc)
{
  PROPVARIANT prop;
  prop.vt = VT_EMPTY;
  prop.wReserved1 = 0;
  
  NWindows::NCOM::PropVarEm_Set_UInt64(&prop, size);
  RINOK(reportArcProp->ReportProp(NEventIndexType::kOutArcIndex, index, kpidSize, &prop));
  if (crc)
  {
    NWindows::NCOM::PropVarEm_Set_UInt32(&prop, *crc);
    RINOK(reportArcProp->ReportProp(NEventIndexType::kOutArcIndex, index, kpidCRC, &prop));
  }
  return reportArcProp->ReportFinished(NEventIndexType::kOutArcIndex, index, NUpdate::NOperationResult::kOK);
}
*/

HRESULT CFolderInStream::AddFileInfo(bool isProcessed)
{
  // const UInt32 index = _indexes[Processed.Size()];
  Processed.AddInReserved(isProcessed);
  Sizes.AddInReserved(_pos);
  const UInt32 crc = CRC_GET_DIGEST(_crc);
  CRCs.AddInReserved(crc);
  TimesDefined.AddInReserved(_times_Defined);
  if (Need_CTime) AddFt(CTimes, _cTime);
  if (Need_ATime) AddFt(ATimes, _aTime);
  if (Need_MTime) AddFt(MTimes, _mTime);
  if (Need_Attrib) Attribs.AddInReserved(_attrib);
  /*
  if (isProcessed && _reportArcProp)
    RINOK(ReportItemProps(_reportArcProp, index, _pos, &crc))
  */
  return _updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK);
}

STDMETHODIMP CFolderInStream::Read(void *data, UInt32 size, UInt32 *processedSize)
{
  if (processedSize)
    *processedSize = 0;
  while (size != 0)
  {
    if (_stream)
    {
      UInt32 cur = size;
      const UInt32 kMax = (UInt32)1 << 20;
      if (cur > kMax)
        cur = kMax;
      RINOK(_stream->Read(data, cur, &cur));
      if (cur != 0)
      {
        _crc = CrcUpdate(_crc, data, cur);
        _pos += cur;
        if (processedSize)
          *processedSize = cur;
        return S_OK;
      }
      
      _stream.Release();
      RINOK(AddFileInfo(true));
    }
    
    if (Processed.Size() >= _numFiles)
      break;
    RINOK(OpenStream());
  }
  return S_OK;
}

STDMETHODIMP CFolderInStream::GetSubStreamSize(UInt64 subStream, UInt64 *value)
{
  *value = 0;
  if (subStream > Sizes.Size())
    return S_FALSE; // E_FAIL;
  
  unsigned index = (unsigned)subStream;
  if (index < Sizes.Size())
  {
    *value = Sizes[index];
    return S_OK;
  }
  
  if (!_size_Defined)
  {
    *value = _pos;
    return S_FALSE;
  }
  
  *value = (_pos > _size ? _pos : _size);
  return S_OK;
}

}}

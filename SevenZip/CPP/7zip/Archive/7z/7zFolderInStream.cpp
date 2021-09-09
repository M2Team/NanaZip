// 7zFolderInStream.cpp

#include "StdAfx.h"

#include "7zFolderInStream.h"

namespace NArchive {
namespace N7z {

void CFolderInStream::Init(IArchiveUpdateCallback *updateCallback,
    const UInt32 *indexes, unsigned numFiles)
{
  _updateCallback = updateCallback;
  _indexes = indexes;
  _numFiles = numFiles;
  _index = 0;
  
  Processed.ClearAndReserve(numFiles);
  CRCs.ClearAndReserve(numFiles);
  Sizes.ClearAndReserve(numFiles);
  
  _pos = 0;
  _crc = CRC_INIT_VAL;
  _size_Defined = false;
  _size = 0;

  _stream.Release();
}

HRESULT CFolderInStream::OpenStream()
{
  _pos = 0;
  _crc = CRC_INIT_VAL;
  _size_Defined = false;
  _size = 0;

  while (_index < _numFiles)
  {
    CMyComPtr<ISequentialInStream> stream;
    HRESULT result = _updateCallback->GetStream(_indexes[_index], &stream);
    if (result != S_OK)
    {
      if (result != S_FALSE)
        return result;
    }

    _stream = stream;
    
    if (stream)
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
    
    _index++;
    RINOK(_updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK));
    AddFileInfo(result == S_OK);
  }
  return S_OK;
}

void CFolderInStream::AddFileInfo(bool isProcessed)
{
  Processed.Add(isProcessed);
  Sizes.Add(_pos);
  CRCs.Add(CRC_GET_DIGEST(_crc));
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
      _index++;
      AddFileInfo(true);

      _pos = 0;
      _crc = CRC_INIT_VAL;
      _size_Defined = false;
      _size = 0;

      RINOK(_updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK));
    }
    
    if (_index >= _numFiles)
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

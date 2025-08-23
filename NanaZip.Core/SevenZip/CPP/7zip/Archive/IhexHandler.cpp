// IhexHandler.cpp

#include "StdAfx.h"

#include "../../../C/CpuArch.h"

#include "../../Common/ComTry.h"
#include "../../Common/DynamicBuffer.h"
#include "../../Common/IntToString.h"
#include "../../Common/StringToInt.h"

#include "../../Windows/PropVariant.h"

#include "../Common/InBuffer.h"
#include "../Common/ProgressUtils.h"
#include "../Common/RegisterArc.h"
#include "../Common/StreamUtils.h"

namespace NArchive {
namespace NIhex {

/* We still don't support files with custom record types: 20, 22: used by Samsung */

struct CBlock
{
  CByteDynamicBuffer Data;
  UInt32 Offset;
};


Z7_CLASS_IMP_CHandler_IInArchive_0

  bool _isArc;
  bool _needMoreInput;
  bool _dataError;
  
  UInt64 _phySize;

  CObjectVector<CBlock> _blocks;
};

static const Byte kProps[] =
{
  kpidPath,
  kpidSize,
  kpidVa
};

IMP_IInArchive_Props
IMP_IInArchive_ArcProps_NO_Table

Z7_COM7F_IMF(CHandler::GetNumberOfItems(UInt32 *numItems))
{
  *numItems = _blocks.Size();
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetArchiveProperty(PROPID propID, PROPVARIANT *value))
{
  NWindows::NCOM::CPropVariant prop;
  switch (propID)
  {
    case kpidPhySize: if (_phySize != 0) prop = _phySize; break;
    case kpidErrorFlags:
    {
      UInt32 v = 0;
      if (!_isArc) v |= kpv_ErrorFlags_IsNotArc;
      if (_needMoreInput) v |= kpv_ErrorFlags_UnexpectedEnd;
      if (_dataError) v |= kpv_ErrorFlags_DataError;
      prop = v;
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
}

Z7_COM7F_IMF(CHandler::GetProperty(UInt32 index, PROPID propID, PROPVARIANT *value))
{
  COM_TRY_BEGIN
  NWindows::NCOM::CPropVariant prop;
  const CBlock &block = _blocks[index];
  switch (propID)
  {
    case kpidSize: prop = (UInt64)block.Data.GetPos(); break;
    case kpidVa: prop = block.Offset; break;
    case kpidPath:
    {
      if (_blocks.Size() != 1)
      {
        char s[16];
        ConvertUInt32ToString(index, s);
        prop = s;
      }
      break;
    }
  }
  prop.Detach(value);
  return S_OK;
  COM_TRY_END
}


static int Parse(const Byte *p)
{
  unsigned v0 = p[0];  Z7_PARSE_HEX_DIGIT(v0, return -1;)
  unsigned v1 = p[1];  Z7_PARSE_HEX_DIGIT(v1, return -1;)
  return (int)((v0 << 4) | v1);
}
  
#define kType_Data 0
#define kType_Eof  1
#define kType_Seg  2
// #define kType_CsIp 3
#define kType_High 4
// #define kType_Ip32 5

#define kType_MAX  5

// we don't want to read files with big number of spaces between records
// it's our limitation (out of specification):
static const unsigned k_NumSpaces_LIMIT = 16 + 1;

#define IS_LINE_DELIMITER(c) (/* (c) == 0 || */ (c) == 10 || (c) == 13)

API_FUNC_static_IsArc IsArc_Ihex(const Byte *p, size_t size)
{
  if (size < 1)
    return k_IsArc_Res_NEED_MORE;
  if (p[0] != ':')
    return k_IsArc_Res_NO;
  p++;
  size--;

  const unsigned kNumLinesToCheck = 3; // 1 line is OK also, but we don't want false detection

  for (unsigned j = 0; j < kNumLinesToCheck; j++)
  {
    if (size < 4 * 2)
      return k_IsArc_Res_NEED_MORE;
    
    const int num = Parse(p);
    if (num < 0)
      return k_IsArc_Res_NO;
    
    const int type = Parse(p + 6);
    if (type < 0 || type > kType_MAX)
      return k_IsArc_Res_NO;
    
    unsigned numChars = ((unsigned)num + 5) * 2;
    unsigned sum = 0;
    
    for (unsigned i = 0; i < numChars; i += 2)
    {
      if (i + 2 > size)
        return k_IsArc_Res_NEED_MORE;
      int v = Parse(p + i);
      if (v < 0)
        return k_IsArc_Res_NO;
      sum += (unsigned)v;
    }
    
    if (sum & 0xFF)
      return k_IsArc_Res_NO;

    if (type == kType_Data)
    {
      // we don't want to open :0000000000 files
      if (num == 0)
        return k_IsArc_Res_NO;
    }
    else
    {
      if (type == kType_Eof)
      {
        if (num != 0)
          return k_IsArc_Res_NO;
        return k_IsArc_Res_YES;
      }
      if (p[2] != 0 ||
          p[3] != 0 ||
          p[4] != 0 ||
          p[5] != 0)
        return k_IsArc_Res_NO;
      if (type == kType_Seg || type == kType_High)
      {
        if (num != 2)
          return k_IsArc_Res_NO;
      }
      else
      {
        if (num != 4)
          return k_IsArc_Res_NO;
      }
    }
    
    p += numChars;
    size -= numChars;
    
    unsigned numSpaces = k_NumSpaces_LIMIT;
    for (;;)
    {
      if (size == 0)
        return k_IsArc_Res_NEED_MORE;
      const Byte b = *p++;
      size--;
      if (b == ':')
        break;
      if (--numSpaces == 0 || !IS_LINE_DELIMITER(b))
        return k_IsArc_Res_NO;
    }
  }
  
  return k_IsArc_Res_YES;
}
}

Z7_COM7F_IMF(CHandler::Open(IInStream *stream, const UInt64 *, IArchiveOpenCallback *openCallback))
{
  COM_TRY_BEGIN
  {
  Close();
  try
  {
    const unsigned kStartSize = (2 + (256 + 5) + 2) * 2;
    Byte temp[kStartSize];
    {
      size_t size = kStartSize;
      RINOK(ReadStream(stream, temp, &size))
      const UInt32 isArcRes = IsArc_Ihex(temp, size);
      if (isArcRes == k_IsArc_Res_NO)
        return S_FALSE;
      if (isArcRes == k_IsArc_Res_NEED_MORE && size != kStartSize)
        return S_FALSE;
    }
    _isArc = true;

    RINOK(InStream_SeekToBegin(stream))
    CInBuffer s;
    if (!s.Create(1 << 15))
      return E_OUTOFMEMORY;
    s.SetStream(stream);
    s.Init();
    {
      Byte b;
      if (!s.ReadByte(b))
      {
        _needMoreInput = true;
        return S_FALSE;
      }
      if (b != ':')
      {
        _dataError = true;
        return S_FALSE;
      }
    }

    UInt32 globalOffset = 0;
    const UInt32 k_progressStep = 1 << 24;
    UInt64 progressNext = k_progressStep;

    for (;;)
    {
      if (s.ReadBytes(temp, 2) != 2)
      {
        _needMoreInput = true;
        return S_FALSE;
      }
      const int num = Parse(temp);
      if (num < 0)
      {
        _dataError = true;
        return S_FALSE;
      }
      {
        const size_t numPairs = (unsigned)num + 4;
        const size_t numBytes = numPairs * 2;
        if (s.ReadBytes(temp, numBytes) != numBytes)
        {
          _needMoreInput = true;
          return S_FALSE;
        }
        unsigned sum = (unsigned)num;
        for (size_t i = 0; i < numPairs; i++)
        {
          const int a = Parse(temp + i * 2);
          if (a < 0)
          {
            _dataError = true;
            return S_FALSE;
          }
          temp[i] = (Byte)a;
          sum += (unsigned)a;
        }
        if (sum & 0xFF)
        {
          _dataError = true;
          return S_FALSE;
        }
      }

      const unsigned type = temp[2];
      if (type > kType_MAX)
      {
        _dataError = true;
        return S_FALSE;
      }

      const UInt32 a = GetBe16(temp);

      if (type == kType_Data)
      {
        if (num == 0)
        {
          // we don't want to open :0000000000 files
          // maybe it can mean EOF in old-style files?
          _dataError = true;
          return S_FALSE;
        }
        // if (num != 0)
        {
          const UInt32 offs = globalOffset + a;
          CBlock *block = NULL;
          if (!_blocks.IsEmpty())
          {
            block = &_blocks.Back();
            if (block->Offset + block->Data.GetPos() != offs)
              block = NULL;
          }
          if (!block)
          {
            block = &_blocks.AddNew();
            block->Offset = offs;
          }
          block->Data.AddData(temp + 3, (unsigned)num);
        }
      }
      else
      {
        if (a != 0) // from description: the address field is typically 0.
        {
          _dataError = true;
          return S_FALSE;
        }
        if (type == kType_Eof)
        {
          if (num != 0)
          {
            _dataError = true;
            return S_FALSE;
          }
          _phySize = s.GetProcessedSize();
          Byte b;
          if (s.ReadByte(b))
          {
            if (b == 10)
              _phySize++;
            else if (b == 13)
            {
              _phySize++;
              if (s.ReadByte(b))
              {
                if (b == 10)
                  _phySize++;
              }
            }
          }
          return S_OK;
        }
  
        if (type == kType_Seg || type == kType_High)
        {
          if (num != 2)
          {
            _dataError = true;
            return S_FALSE;
          }
          // here we use optimization trick for num shift calculation: (type == kType_Seg ? 4 : 16)
          globalOffset = (UInt32)GetBe16(temp + 3) << (1u << type);
        }
        else
        {
          if (num != 4)
          {
            _dataError = true;
            return S_FALSE;
          }
        }
      }

      if (openCallback)
      {
        const UInt64 processed = s.GetProcessedSize();
        if (processed >= progressNext)
        {
          progressNext = processed + k_progressStep;
          const UInt64 numFiles = _blocks.Size();
          RINOK(openCallback->SetCompleted(&numFiles, &processed))
        }
      }

      unsigned numSpaces = k_NumSpaces_LIMIT;
      for (;;)
      {
        Byte b;
        if (!s.ReadByte(b))
        {
          _needMoreInput = true;
          return S_FALSE;
        }
        if (b == ':')
          break;
        if (--numSpaces == 0 || !IS_LINE_DELIMITER(b))
        {
          _dataError = true;
          return S_FALSE;
        }
      }
    }
  }
  catch(const CInBufferException &e) { return e.ErrorCode; }
  }
  COM_TRY_END
}


Z7_COM7F_IMF(CHandler::Close())
{
  _phySize = 0;
  _isArc = false;
  _needMoreInput = false;
  _dataError = false;
  _blocks.Clear();
  return S_OK;
}


Z7_COM7F_IMF(CHandler::Extract(const UInt32 *indices, UInt32 numItems,
    Int32 testMode, IArchiveExtractCallback *extractCallback))
{
  COM_TRY_BEGIN
  const bool allFilesMode = (numItems == (UInt32)(Int32)-1);
  if (allFilesMode)
    numItems = _blocks.Size();
  if (numItems == 0)
    return S_OK;

  UInt64 totalSize = 0;
  UInt32 i;
  for (i = 0; i < numItems; i++)
    totalSize += _blocks[allFilesMode ? i : indices[i]].Data.GetPos();
  RINOK(extractCallback->SetTotal(totalSize))

  CMyComPtr2_Create<ICompressProgressInfo, CLocalProgress> lps;
  lps->Init(extractCallback, false);

  for (i = 0;; i++)
  {
    lps->InSize = lps->OutSize;
    RINOK(lps->SetCur())
    if (i >= numItems)
      break;
    const UInt32 index = allFilesMode ? i : indices[i];
    const CByteDynamicBuffer &data = _blocks[index].Data;
    lps->OutSize += data.GetPos();
    {
      CMyComPtr<ISequentialOutStream> realOutStream;
      const Int32 askMode = testMode ?
          NExtract::NAskMode::kTest :
          NExtract::NAskMode::kExtract;
      RINOK(extractCallback->GetStream(index, &realOutStream, askMode))
      if (!testMode && !realOutStream)
        continue;
      RINOK(extractCallback->PrepareOperation(askMode))
      if (realOutStream)
        RINOK(WriteStream(realOutStream, (const Byte *)data, data.GetPos()))
    }
    RINOK(extractCallback->SetOperationResult(NExtract::NOperationResult::kOK))
  }
  
  return S_OK;
  COM_TRY_END
}

// k_Signature: { ':' }

REGISTER_ARC_I_NO_SIG(
  "IHex", "ihex", NULL, 0xCD,
  0,
  NArcInfoFlags::kStartOpen,
  IsArc_Ihex)

}}

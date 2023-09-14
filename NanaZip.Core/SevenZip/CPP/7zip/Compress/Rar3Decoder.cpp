// Rar3Decoder.cpp
// According to unRAR license, this code may not be used to develop
// a program that creates RAR archives

/* This code uses Carryless rangecoder (1999): Dmitry Subbotin : Public domain */
 
#include "StdAfx.h"

#include "../../../C/Alloc.h"

#include "../Common/StreamUtils.h"

#include "Rar3Decoder.h"

namespace NCompress {
namespace NRar3 {

static const UInt32 kNumAlignReps = 15;

static const UInt32 kSymbolReadTable = 256;
static const UInt32 kSymbolRep = 259;

static const Byte kDistDirectBits[kDistTableSize] =
  {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15,
  16,16,16,16,16,16,16,16,16,16,16,16,16,16,
  18,18,18,18,18,18,18,18,18,18,18,18};

static const Byte kLen2DistStarts[kNumLen2Symbols] = {0,4,8,16,32,64,128,192};
static const Byte kLen2DistDirectBits[kNumLen2Symbols] = {2,2,3, 4, 5, 6,  6,  6};

static const UInt32 kDistLimit3 = 0x2000 - 2;
static const UInt32 kDistLimit4 = 0x40000 - 2;

static const UInt32 kNormalMatchMinLen = 3;

static const UInt32 kVmDataSizeMax = 1 << 16;
static const UInt32 kVmCodeSizeMax = 1 << 16;

extern "C" {

static Byte Wrap_ReadByte(IByteInPtr pp) throw()
{
  CByteIn *p = Z7_CONTAINER_FROM_VTBL_CLS(pp, CByteIn, IByteIn_obj);
  return p->BitDecoder.Stream.ReadByte();
}

static Byte Wrap_ReadBits8(IByteInPtr pp) throw()
{
  CByteIn *p = Z7_CONTAINER_FROM_VTBL_CLS(pp, CByteIn, IByteIn_obj);
  return (Byte)p->BitDecoder.ReadByteFromAligned();
}

}


CDecoder::CDecoder():
  _window(NULL),
  _winPos(0),
  _wrPtr(0),
  _lzSize(0),
  _writtenFileSize(0),
  _vmData(NULL),
  _vmCode(NULL),
  _isSolid(false),
  _solidAllowed(false)
{
  Ppmd7_Construct(&_ppmd);
  
  UInt32 start = 0;
  for (UInt32 i = 0; i < kDistTableSize; i++)
  {
    kDistStart[i] = start;
    start += ((UInt32)1 << kDistDirectBits[i]);
  }
}

CDecoder::~CDecoder()
{
  InitFilters();
  ::MidFree(_vmData);
  ::MidFree(_window);
  Ppmd7_Free(&_ppmd, &g_BigAlloc);
}

HRESULT CDecoder::WriteDataToStream(const Byte *data, UInt32 size)
{
  return WriteStream(_outStream, data, size);
}

HRESULT CDecoder::WriteData(const Byte *data, UInt32 size)
{
  HRESULT res = S_OK;
  if (_writtenFileSize < _unpackSize)
  {
    UInt32 curSize = size;
    UInt64 remain = _unpackSize - _writtenFileSize;
    if (remain < curSize)
      curSize = (UInt32)remain;
    res = WriteDataToStream(data, curSize);
  }
  _writtenFileSize += size;
  return res;
}

HRESULT CDecoder::WriteArea(UInt32 startPtr, UInt32 endPtr)
{
  if (startPtr <= endPtr)
    return WriteData(_window + startPtr, endPtr - startPtr);
  RINOK(WriteData(_window + startPtr, kWindowSize - startPtr))
  return WriteData(_window, endPtr);
}

void CDecoder::ExecuteFilter(unsigned tempFilterIndex, NVm::CBlockRef &outBlockRef)
{
  CTempFilter *tempFilter = _tempFilters[tempFilterIndex];
  tempFilter->InitR[6] = (UInt32)_writtenFileSize;
  NVm::SetValue32(&tempFilter->GlobalData[0x24], (UInt32)_writtenFileSize);
  NVm::SetValue32(&tempFilter->GlobalData[0x28], (UInt32)(_writtenFileSize >> 32));
  CFilter *filter = _filters[tempFilter->FilterIndex];
  if (!filter->IsSupported)
    _unsupportedFilter = true;
  if (!_vm.Execute(filter, tempFilter, outBlockRef, filter->GlobalData))
    _unsupportedFilter = true;
  delete tempFilter;
  _tempFilters[tempFilterIndex] = NULL;
  _numEmptyTempFilters++;
}

HRESULT CDecoder::WriteBuf()
{
  UInt32 writtenBorder = _wrPtr;
  UInt32 writeSize = (_winPos - writtenBorder) & kWindowMask;
  FOR_VECTOR (i, _tempFilters)
  {
    CTempFilter *filter = _tempFilters[i];
    if (!filter)
      continue;
    if (filter->NextWindow)
    {
      filter->NextWindow = false;
      continue;
    }
    UInt32 blockStart = filter->BlockStart;
    UInt32 blockSize = filter->BlockSize;
    if (((blockStart - writtenBorder) & kWindowMask) < writeSize)
    {
      if (writtenBorder != blockStart)
      {
        RINOK(WriteArea(writtenBorder, blockStart))
        writtenBorder = blockStart;
        writeSize = (_winPos - writtenBorder) & kWindowMask;
      }
      if (blockSize <= writeSize)
      {
        UInt32 blockEnd = (blockStart + blockSize) & kWindowMask;
        if (blockStart < blockEnd || blockEnd == 0)
          _vm.SetMemory(0, _window + blockStart, blockSize);
        else
        {
          UInt32 tailSize = kWindowSize - blockStart;
          _vm.SetMemory(0, _window + blockStart, tailSize);
          _vm.SetMemory(tailSize, _window, blockEnd);
        }
        NVm::CBlockRef outBlockRef;
        ExecuteFilter(i, outBlockRef);
        while (i + 1 < _tempFilters.Size())
        {
          CTempFilter *nextFilter = _tempFilters[i + 1];
          if (!nextFilter
              || nextFilter->BlockStart != blockStart
              || nextFilter->BlockSize != outBlockRef.Size
              || nextFilter->NextWindow)
            break;
          _vm.SetMemory(0, _vm.GetDataPointer(outBlockRef.Offset), outBlockRef.Size);
          ExecuteFilter(++i, outBlockRef);
        }
        WriteDataToStream(_vm.GetDataPointer(outBlockRef.Offset), outBlockRef.Size);
        _writtenFileSize += outBlockRef.Size;
        writtenBorder = blockEnd;
        writeSize = (_winPos - writtenBorder) & kWindowMask;
      }
      else
      {
        for (unsigned j = i; j < _tempFilters.Size(); j++)
        {
          CTempFilter *filter2 = _tempFilters[j];
          if (filter2 && filter2->NextWindow)
            filter2->NextWindow = false;
        }
        _wrPtr = writtenBorder;
        return S_OK; // check it
      }
    }
  }
      
  _wrPtr = _winPos;
  return WriteArea(writtenBorder, _winPos);
}

void CDecoder::InitFilters()
{
  _lastFilter = 0;
  _numEmptyTempFilters = 0;
  unsigned i;
  for (i = 0; i < _tempFilters.Size(); i++)
    delete _tempFilters[i];
  _tempFilters.Clear();
  for (i = 0; i < _filters.Size(); i++)
    delete _filters[i];
  _filters.Clear();
}

static const unsigned MAX_UNPACK_FILTERS = 8192;

bool CDecoder::AddVmCode(UInt32 firstByte, UInt32 codeSize)
{
  CMemBitDecoder inp;
  inp.Init(_vmData, codeSize);

  UInt32 filterIndex;
  
  if (firstByte & 0x80)
  {
    filterIndex = inp.ReadEncodedUInt32();
    if (filterIndex == 0)
      InitFilters();
    else
      filterIndex--;
  }
  else
    filterIndex = _lastFilter;
  
  if (filterIndex > (UInt32)_filters.Size())
    return false;
  _lastFilter = filterIndex;
  bool newFilter = (filterIndex == (UInt32)_filters.Size());

  CFilter *filter;
  if (newFilter)
  {
    // check if too many filters
    if (filterIndex > MAX_UNPACK_FILTERS)
      return false;
    filter = new CFilter;
    _filters.Add(filter);
  }
  else
  {
    filter = _filters[filterIndex];
    filter->ExecCount++;
  }

  if (_numEmptyTempFilters != 0)
  {
    unsigned num = _tempFilters.Size();
    CTempFilter **tempFilters = &_tempFilters.Front();
    
    unsigned w = 0;
    for (unsigned i = 0; i < num; i++)
    {
      CTempFilter *tf = tempFilters[i];
      if (tf)
        tempFilters[w++] = tf;
    }

    _tempFilters.DeleteFrom(w);
    _numEmptyTempFilters = 0;
  }
  
  if (_tempFilters.Size() > MAX_UNPACK_FILTERS)
    return false;
  CTempFilter *tempFilter = new CTempFilter;
  _tempFilters.Add(tempFilter);
  tempFilter->FilterIndex = filterIndex;
 
  UInt32 blockStart = inp.ReadEncodedUInt32();
  if (firstByte & 0x40)
    blockStart += 258;
  tempFilter->BlockStart = (blockStart + _winPos) & kWindowMask;
  if (firstByte & 0x20)
    filter->BlockSize = inp.ReadEncodedUInt32();
  tempFilter->BlockSize = filter->BlockSize;
  tempFilter->NextWindow = _wrPtr != _winPos && ((_wrPtr - _winPos) & kWindowMask) <= blockStart;

  memset(tempFilter->InitR, 0, sizeof(tempFilter->InitR));
  tempFilter->InitR[3] = NVm::kGlobalOffset;
  tempFilter->InitR[4] = tempFilter->BlockSize;
  tempFilter->InitR[5] = filter->ExecCount;
  if (firstByte & 0x10)
  {
    UInt32 initMask = inp.ReadBits(NVm::kNumGpRegs);
    for (unsigned i = 0; i < NVm::kNumGpRegs; i++)
      if (initMask & (1 << i))
        tempFilter->InitR[i] = inp.ReadEncodedUInt32();
  }

  bool isOK = true;
  if (newFilter)
  {
    UInt32 vmCodeSize = inp.ReadEncodedUInt32();
    if (vmCodeSize >= kVmCodeSizeMax || vmCodeSize == 0)
      return false;
    for (UInt32 i = 0; i < vmCodeSize; i++)
      _vmCode[i] = (Byte)inp.ReadBits(8);
    isOK = filter->PrepareProgram(_vmCode, vmCodeSize);
  }

  {
    Byte *globalData = &tempFilter->GlobalData[0];
    for (unsigned i = 0; i < NVm::kNumGpRegs; i++)
      NVm::SetValue32(&globalData[i * 4], tempFilter->InitR[i]);
    NVm::SetValue32(&globalData[NVm::NGlobalOffset::kBlockSize], tempFilter->BlockSize);
    NVm::SetValue32(&globalData[NVm::NGlobalOffset::kBlockPos], 0); // It was commented. why?
    NVm::SetValue32(&globalData[NVm::NGlobalOffset::kExecCount], filter->ExecCount);
  }

  if (firstByte & 8)
  {
    UInt32 dataSize = inp.ReadEncodedUInt32();
    if (dataSize > NVm::kGlobalSize - NVm::kFixedGlobalSize)
      return false;
    CRecordVector<Byte> &globalData = tempFilter->GlobalData;
    unsigned requiredSize = (unsigned)(dataSize + NVm::kFixedGlobalSize);
    if (globalData.Size() < requiredSize)
      globalData.ChangeSize_KeepData(requiredSize);
    Byte *dest = &globalData[NVm::kFixedGlobalSize];
    for (UInt32 i = 0; i < dataSize; i++)
      dest[i] = (Byte)inp.ReadBits(8);
  }
  
  return isOK;
}

bool CDecoder::ReadVmCodeLZ()
{
  UInt32 firstByte = ReadBits(8);
  UInt32 len = (firstByte & 7) + 1;
  if (len == 7)
    len = ReadBits(8) + 7;
  else if (len == 8)
    len = ReadBits(16);
  if (len > kVmDataSizeMax)
    return false;
  for (UInt32 i = 0; i < len; i++)
    _vmData[i] = (Byte)ReadBits(8);
  return AddVmCode(firstByte, len);
}


// int CDecoder::DecodePpmSymbol() { return Ppmd7a_DecodeSymbol(&_ppmd); }
#define DecodePpmSymbol() Ppmd7a_DecodeSymbol(&_ppmd)


bool CDecoder::ReadVmCodePPM()
{
  const int firstByte = DecodePpmSymbol();
  if (firstByte < 0)
    return false;
  UInt32 len = (firstByte & 7) + 1;
  if (len == 7)
  {
    const int b1 = DecodePpmSymbol();
    if (b1 < 0)
      return false;
    len = (unsigned)b1 + 7;
  }
  else if (len == 8)
  {
    const int b1 = DecodePpmSymbol();
    if (b1 < 0)
      return false;
    const int b2 = DecodePpmSymbol();
    if (b2 < 0)
      return false;
    len = (unsigned)b1 * 256 + (unsigned)b2;
  }
  if (len > kVmDataSizeMax)
    return false;
  if (InputEofError_Fast())
    return false;
  for (UInt32 i = 0; i < len; i++)
  {
    const int b = DecodePpmSymbol();
    if (b < 0)
      return false;
    _vmData[i] = (Byte)b;
  }
  return AddVmCode((unsigned)firstByte, len);
}

#define RIF(x) { if (!(x)) return S_FALSE; }

UInt32 CDecoder::ReadBits(unsigned numBits) { return m_InBitStream.BitDecoder.ReadBits(numBits); }

// ---------- PPM ----------

HRESULT CDecoder::InitPPM()
{
  unsigned maxOrder = (unsigned)ReadBits(7);

  const bool reset = ((maxOrder & 0x20) != 0);
  UInt32 maxMB = 0;
  if (reset)
    maxMB = (Byte)Wrap_ReadBits8(&m_InBitStream.IByteIn_obj);
  else
  {
    if (PpmError || !Ppmd7_WasAllocated(&_ppmd))
      return S_FALSE;
  }
  if (maxOrder & 0x40)
    PpmEscChar = (Byte)Wrap_ReadBits8(&m_InBitStream.IByteIn_obj);

  _ppmd.rc.dec.Stream = &m_InBitStream.IByteIn_obj;
  m_InBitStream.IByteIn_obj.Read = Wrap_ReadBits8;

  Ppmd7a_RangeDec_Init(&_ppmd.rc.dec);

  m_InBitStream.IByteIn_obj.Read = Wrap_ReadByte;

  if (reset)
  {
    PpmError = true;
    maxOrder = (maxOrder & 0x1F) + 1;
    if (maxOrder > 16)
      maxOrder = 16 + (maxOrder - 16) * 3;
    if (maxOrder == 1)
    {
      Ppmd7_Free(&_ppmd, &g_BigAlloc);
      return S_FALSE;
    }
    if (!Ppmd7_Alloc(&_ppmd, (maxMB + 1) << 20, &g_BigAlloc))
      return E_OUTOFMEMORY;
    Ppmd7_Init(&_ppmd, maxOrder);
    PpmError = false;
  }
  return S_OK;
}


HRESULT CDecoder::DecodePPM(Int32 num, bool &keepDecompressing)
{
  keepDecompressing = false;
  if (PpmError)
    return S_FALSE;
  do
  {
    if (((_wrPtr - _winPos) & kWindowMask) < 260 && _wrPtr != _winPos)
    {
      RINOK(WriteBuf())
      if (_writtenFileSize > _unpackSize)
      {
        keepDecompressing = false;
        return S_OK;
      }
    }
    if (InputEofError_Fast())
      return false;
    const int c = DecodePpmSymbol();
    if (c < 0)
    {
      PpmError = true;
      return S_FALSE;
    }
    if (c == PpmEscChar)
    {
      const int nextCh = DecodePpmSymbol();
      if (nextCh < 0)
      {
        PpmError = true;
        return S_FALSE;
      }
      if (nextCh == 0)
        return ReadTables(keepDecompressing);
      if (nextCh == 2 || nextCh == -1)
        return S_OK;
      if (nextCh == 3)
      {
        if (!ReadVmCodePPM())
        {
          PpmError = true;
          return S_FALSE;
        }
        continue;
      }
      if (nextCh == 4 || nextCh == 5)
      {
        UInt32 dist = 0;
        UInt32 len = 4;
        if (nextCh == 4)
        {
          for (int i = 0; i < 3; i++)
          {
            const int c2 = DecodePpmSymbol();
            if (c2 < 0)
            {
              PpmError = true;
              return S_FALSE;
            }
            dist = (dist << 8) + (Byte)c2;
          }
          dist++;
          len += 28;
        }
        const int c2 = DecodePpmSymbol();
        if (c2 < 0)
        {
          PpmError = true;
          return S_FALSE;
        }
        len += (unsigned)c2;
        if (dist >= _lzSize)
          return S_FALSE;
        CopyBlock(dist, len);
        num -= (Int32)len;
        continue;
      }
    }
    PutByte((Byte)c);
    num--;
  }
  while (num >= 0);
  keepDecompressing = true;
  return S_OK;
}

// ---------- LZ ----------

HRESULT CDecoder::ReadTables(bool &keepDecompressing)
{
  keepDecompressing = true;
  m_InBitStream.BitDecoder.AlignToByte();
  if (ReadBits(1) != 0)
  {
    _lzMode = false;
    return InitPPM();
  }

  TablesRead = false;
  TablesOK = false;

  _lzMode = true;
  PrevAlignBits = 0;
  PrevAlignCount = 0;

  Byte levelLevels[kLevelTableSize];
  Byte lens[kTablesSizesSum];

  if (ReadBits(1) == 0)
    memset(m_LastLevels, 0, kTablesSizesSum);

  unsigned i;

  for (i = 0; i < kLevelTableSize; i++)
  {
    const UInt32 len = ReadBits(4);
    if (len == 15)
    {
      UInt32 zeroCount = ReadBits(4);
      if (zeroCount != 0)
      {
        zeroCount += 2;
        while (zeroCount-- > 0 && i < kLevelTableSize)
          levelLevels[i++]=0;
        i--;
        continue;
      }
    }
    levelLevels[i] = (Byte)len;
  }
  
  RIF(m_LevelDecoder.Build(levelLevels))
  
  i = 0;
  
  do
  {
    const UInt32 sym = m_LevelDecoder.Decode(&m_InBitStream.BitDecoder);
    if (sym < 16)
    {
      lens[i] = Byte((sym + m_LastLevels[i]) & 15);
      i++;
    }
    else if (sym > kLevelTableSize)
      return S_FALSE;
    else
    {
      unsigned num = ((sym - 16) & 1) * 4;
      num += num + 3 + (unsigned)ReadBits(num + 3);
      num += i;
      if (num > kTablesSizesSum)
        num = kTablesSizesSum;
      Byte v = 0;
      if (sym < 16 + 2)
      {
        if (i == 0)
          return S_FALSE;
        v = lens[(size_t)i - 1];
      }
      do
        lens[i++] = v;
      while (i < num);
    }
  }
  while (i < kTablesSizesSum);

  if (InputEofError())
    return S_FALSE;

  TablesRead = true;

  // original code has check here:
  /*
  if (InAddr > ReadTop)
  {
    keepDecompressing = false;
    return true;
  }
  */

  RIF(m_MainDecoder.Build(&lens[0]))
  RIF(m_DistDecoder.Build(&lens[kMainTableSize]))
  RIF(m_AlignDecoder.Build(&lens[kMainTableSize + kDistTableSize]))
  RIF(m_LenDecoder.Build(&lens[kMainTableSize + kDistTableSize + kAlignTableSize]))

  memcpy(m_LastLevels, lens, kTablesSizesSum);

  TablesOK = true;

  return S_OK;
}

/*
class CCoderReleaser
{
  CDecoder *m_Coder;
public:
  CCoderReleaser(CDecoder *coder): m_Coder(coder) {}
  ~CCoderReleaser()
  {
    m_Coder->ReleaseStreams();
  }
};
*/

HRESULT CDecoder::ReadEndOfBlock(bool &keepDecompressing)
{
  if (ReadBits(1) == 0)
  {
    // new file
    keepDecompressing = false;
    TablesRead = (ReadBits(1) == 0);
    return S_OK;
  }
  TablesRead = false;
  return ReadTables(keepDecompressing);
}


HRESULT CDecoder::DecodeLZ(bool &keepDecompressing)
{
  UInt32 rep0 = _reps[0];
  UInt32 rep1 = _reps[1];
  UInt32 rep2 = _reps[2];
  UInt32 rep3 = _reps[3];
  UInt32 len = _lastLength;
  for (;;)
  {
    if (((_wrPtr - _winPos) & kWindowMask) < 260 && _wrPtr != _winPos)
    {
      RINOK(WriteBuf())
      if (_writtenFileSize > _unpackSize)
      {
        keepDecompressing = false;
        return S_OK;
      }
    }
    
    if (InputEofError_Fast())
      return S_FALSE;

    UInt32 sym = m_MainDecoder.Decode(&m_InBitStream.BitDecoder);
    if (sym < 256)
    {
      PutByte((Byte)sym);
      continue;
    }
    else if (sym == kSymbolReadTable)
    {
      RINOK(ReadEndOfBlock(keepDecompressing))
      break;
    }
    else if (sym == 257)
    {
      if (!ReadVmCodeLZ())
        return S_FALSE;
      continue;
    }
    else if (sym == 258)
    {
      if (len == 0)
        return S_FALSE;
    }
    else if (sym < kSymbolRep + 4)
    {
      if (sym != kSymbolRep)
      {
        UInt32 dist;
        if (sym == kSymbolRep + 1)
          dist = rep1;
        else
        {
          if (sym == kSymbolRep + 2)
            dist = rep2;
          else
          {
            dist = rep3;
            rep3 = rep2;
          }
          rep2 = rep1;
        }
        rep1 = rep0;
        rep0 = dist;
      }

      const UInt32 sym2 = m_LenDecoder.Decode(&m_InBitStream.BitDecoder);
      if (sym2 >= kLenTableSize)
        return S_FALSE;
      len = 2 + sym2;
      if (sym2 >= 8)
      {
        unsigned num = (sym2 >> 2) - 1;
        len = 2 + ((4 + (sym2 & 3)) << num) + m_InBitStream.BitDecoder.ReadBits_upto8(num);
      }
    }
    else
    {
      rep3 = rep2;
      rep2 = rep1;
      rep1 = rep0;
      if (sym < 271)
      {
        sym -= 263;
        rep0 = kLen2DistStarts[sym] + m_InBitStream.BitDecoder.ReadBits_upto8(kLen2DistDirectBits[sym]);
        len = 2;
      }
      else if (sym < 299)
      {
        sym -= 271;
        len = kNormalMatchMinLen + sym;
        if (sym >= 8)
        {
          const unsigned num = (sym >> 2) - 1;
          len = kNormalMatchMinLen + ((4 + (sym & 3)) << num) + m_InBitStream.BitDecoder.ReadBits_upto8(num);
        }
        const UInt32 sym2 = m_DistDecoder.Decode(&m_InBitStream.BitDecoder);
        if (sym2 >= kDistTableSize)
          return S_FALSE;
        rep0 = kDistStart[sym2];
        unsigned numBits = kDistDirectBits[sym2];
        if (sym2 >= (kNumAlignBits * 2) + 2)
        {
          if (numBits > kNumAlignBits)
            rep0 += (m_InBitStream.BitDecoder.ReadBits(numBits - kNumAlignBits) << kNumAlignBits);
          if (PrevAlignCount > 0)
          {
            PrevAlignCount--;
            rep0 += PrevAlignBits;
          }
          else
          {
            const UInt32 sym3 = m_AlignDecoder.Decode(&m_InBitStream.BitDecoder);
            if (sym3 < (1 << kNumAlignBits))
            {
              rep0 += sym3;
              PrevAlignBits = sym3;
            }
            else if (sym3 == (1 << kNumAlignBits))
            {
              PrevAlignCount = kNumAlignReps;
              rep0 += PrevAlignBits;
            }
            else
              return S_FALSE;
          }
        }
        else
          rep0 += m_InBitStream.BitDecoder.ReadBits_upto8(numBits);
        len += ((UInt32)(kDistLimit4 - rep0) >> 31) + ((UInt32)(kDistLimit3 - rep0) >> 31);
      }
      else
        return S_FALSE;
    }
    if (rep0 >= _lzSize)
      return S_FALSE;
    CopyBlock(rep0, len);
  }
  _reps[0] = rep0;
  _reps[1] = rep1;
  _reps[2] = rep2;
  _reps[3] = rep3;
  _lastLength = len;

  return S_OK;
}


HRESULT CDecoder::CodeReal(ICompressProgressInfo *progress)
{
  _writtenFileSize = 0;
  _unsupportedFilter = false;
  
  if (!_isSolid)
  {
    _lzSize = 0;
    _winPos = 0;
    _wrPtr = 0;
    for (unsigned i = 0; i < kNumReps; i++)
      _reps[i] = 0;
    _lastLength = 0;
    memset(m_LastLevels, 0, kTablesSizesSum);
    TablesRead = false;
    PpmEscChar = 2;
    PpmError = true;
    InitFilters();
    // _errorMode = false;
  }

  /*
  if (_errorMode)
    return S_FALSE;
  */

  if (!_isSolid || !TablesRead)
  {
    bool keepDecompressing;
    RINOK(ReadTables(keepDecompressing))
    if (!keepDecompressing)
    {
      _solidAllowed = true;
      return S_OK;
    }
  }

  for (;;)
  {
    bool keepDecompressing;
    if (_lzMode)
    {
      if (!TablesOK)
        return S_FALSE;
      RINOK(DecodeLZ(keepDecompressing))
    }
    else
    {
      RINOK(DecodePPM(1 << 18, keepDecompressing))
    }

    if (InputEofError())
      return S_FALSE;

    const UInt64 packSize = m_InBitStream.BitDecoder.GetProcessedSize();
    RINOK(progress->SetRatioInfo(&packSize, &_writtenFileSize))
    if (!keepDecompressing)
      break;
  }

  _solidAllowed = true;

  RINOK(WriteBuf())
  const UInt64 packSize = m_InBitStream.BitDecoder.GetProcessedSize();
  RINOK(progress->SetRatioInfo(&packSize, &_writtenFileSize))
  if (_writtenFileSize < _unpackSize)
    return S_FALSE;

  if (_unsupportedFilter)
    return E_NOTIMPL;

  return S_OK;
}

Z7_COM7F_IMF(CDecoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress))
{
  try
  {
    if (!inSize)
      return E_INVALIDARG;

    if (_isSolid && !_solidAllowed)
      return S_FALSE;
    _solidAllowed = false;

    if (!_vmData)
    {
      _vmData = (Byte *)::MidAlloc(kVmDataSizeMax + kVmCodeSizeMax);
      if (!_vmData)
        return E_OUTOFMEMORY;
      _vmCode = _vmData + kVmDataSizeMax;
    }
    
    if (!_window)
    {
      _window = (Byte *)::MidAlloc(kWindowSize);
      if (!_window)
        return E_OUTOFMEMORY;
    }
    if (!m_InBitStream.BitDecoder.Create(1 << 20))
      return E_OUTOFMEMORY;
    if (!_vm.Create())
      return E_OUTOFMEMORY;

    
    m_InBitStream.BitDecoder.SetStream(inStream);
    m_InBitStream.BitDecoder.Init();
    _outStream = outStream;
   
    // CCoderReleaser coderReleaser(this);
    _unpackSize = outSize ? *outSize : (UInt64)(Int64)-1;
    return CodeReal(progress);
  }
  catch(const CInBufferException &e)  { /* _errorMode = true; */ return e.ErrorCode; }
  catch(...) { /* _errorMode = true; */ return S_FALSE; }
  // CNewException is possible here. But probably CNewException is caused
  // by error in data stream.
}

Z7_COM7F_IMF(CDecoder::SetDecoderProperties2(const Byte *data, UInt32 size))
{
  if (size < 1)
    return E_INVALIDARG;
  _isSolid = ((data[0] & 1) != 0);
  return S_OK;
}

}}

// BZip2Encoder.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"
#include "../../../C/BwtSort.h"
#include "../../../C/HuffEnc.h"

#include "BZip2Crc.h"
#include "BZip2Encoder.h"
#include "Mtf8.h"

namespace NCompress {
namespace NBZip2 {

const unsigned kMaxHuffmanLenForEncoding = 16; // it must be < kMaxHuffmanLen = 20

static const UInt32 kBufferSize = (1 << 17);
static const unsigned kNumHuffPasses = 4;

bool CThreadInfo::Alloc()
{
  if (!m_BlockSorterIndex)
  {
    m_BlockSorterIndex = (UInt32 *)::BigAlloc(BLOCK_SORT_BUF_SIZE(kBlockSizeMax) * sizeof(UInt32));
    if (!m_BlockSorterIndex)
      return false;
  }

  if (!m_Block)
  {
    m_Block = (Byte *)::MidAlloc(kBlockSizeMax * 5 + kBlockSizeMax / 10 + (20 << 10));
    if (!m_Block)
      return false;
    m_MtfArray = m_Block + kBlockSizeMax;
    m_TempArray = m_MtfArray + kBlockSizeMax * 2 + 2;
  }
  return true;
}

void CThreadInfo::Free()
{
  ::BigFree(m_BlockSorterIndex);
  m_BlockSorterIndex = NULL;
  ::MidFree(m_Block);
  m_Block = NULL;
}

#ifndef Z7_ST

static THREAD_FUNC_DECL MFThread(void *threadCoderInfo)
{
  return ((CThreadInfo *)threadCoderInfo)->ThreadFunc();
}

HRESULT CThreadInfo::Create()
{
  WRes             wres = StreamWasFinishedEvent.Create();
  if (wres == 0) { wres = WaitingWasStartedEvent.Create();
  if (wres == 0) { wres = CanWriteEvent.Create();
  if (wres == 0)
  {
    if (Encoder->_props.Affinity != 0)
      wres = Thread.Create_With_Affinity(MFThread, this, (CAffinityMask)Encoder->_props.Affinity);
    else
      wres = Thread.Create(MFThread, this);
  }}}
  return HRESULT_FROM_WIN32(wres);
}

void CThreadInfo::FinishStream(bool needLeave)
{
  Encoder->StreamWasFinished = true;
  StreamWasFinishedEvent.Set();
  if (needLeave)
    Encoder->CS.Leave();
  Encoder->CanStartWaitingEvent.Lock();
  WaitingWasStartedEvent.Set();
}

THREAD_FUNC_RET_TYPE CThreadInfo::ThreadFunc()
{
  for (;;)
  {
    Encoder->CanProcessEvent.Lock();
    Encoder->CS.Enter();
    if (Encoder->CloseThreads)
    {
      Encoder->CS.Leave();
      return 0;
    }
    if (Encoder->StreamWasFinished)
    {
      FinishStream(true);
      continue;
    }
    HRESULT res = S_OK;
    bool needLeave = true;
    try
    {
      const UInt32 blockSize = Encoder->ReadRleBlock(m_Block);
      m_UnpackSize = Encoder->m_InStream.GetProcessedSize();
      m_BlockIndex = Encoder->NextBlockIndex;
      if (++Encoder->NextBlockIndex == Encoder->NumThreads)
        Encoder->NextBlockIndex = 0;
      if (blockSize == 0)
      {
        FinishStream(true);
        continue;
      }
      Encoder->CS.Leave();
      needLeave = false;
      res = EncodeBlock3(blockSize);
    }
    catch(const CInBufferException &e)  { res = e.ErrorCode; }
    catch(const COutBufferException &e) { res = e.ErrorCode; }
    catch(...) { res = E_FAIL; }
    if (res != S_OK)
    {
      Encoder->Result = res;
      FinishStream(needLeave);
      continue;
    }
  }
}

#endif

void CEncProps::Normalize(int level)
{
  if (level < 0) level = 5;
  if (level > 9) level = 9;
  
  if (NumPasses == (UInt32)(Int32)-1)
    NumPasses = (level >= 9 ? 7 : (level >= 7 ? 2 : 1));
  if (NumPasses < 1) NumPasses = 1;
  if (NumPasses > kNumPassesMax) NumPasses = kNumPassesMax;
  
  if (BlockSizeMult == (UInt32)(Int32)-1)
    BlockSizeMult = (level >= 5 ? 9 : (level >= 1 ? (unsigned)level * 2 - 1: 1));
  if (BlockSizeMult < kBlockSizeMultMin) BlockSizeMult = kBlockSizeMultMin;
  if (BlockSizeMult > kBlockSizeMultMax) BlockSizeMult = kBlockSizeMultMax;
}

CEncoder::CEncoder()
{
  _props.Normalize(-1);

  #ifndef Z7_ST
  ThreadsInfo = NULL;
  m_NumThreadsPrev = 0;
  NumThreads = 1;
  #endif
}

#ifndef Z7_ST
CEncoder::~CEncoder()
{
  Free();
}

HRESULT CEncoder::Create()
{
  {
    WRes             wres = CanProcessEvent.CreateIfNotCreated_Reset();
    if (wres == 0) { wres = CanStartWaitingEvent.CreateIfNotCreated_Reset(); }
    if (wres != 0)
      return HRESULT_FROM_WIN32(wres);
  }
  
  if (ThreadsInfo && m_NumThreadsPrev == NumThreads)
    return S_OK;
  try
  {
    Free();
    MtMode = (NumThreads > 1);
    m_NumThreadsPrev = NumThreads;
    ThreadsInfo = new CThreadInfo[NumThreads];
    if (!ThreadsInfo)
      return E_OUTOFMEMORY;
  }
  catch(...) { return E_OUTOFMEMORY; }
  for (UInt32 t = 0; t < NumThreads; t++)
  {
    CThreadInfo &ti = ThreadsInfo[t];
    ti.Encoder = this;
    if (MtMode)
    {
      HRESULT res = ti.Create();
      if (res != S_OK)
      {
        NumThreads = t;
        Free();
        return res;
      }
    }
  }
  return S_OK;
}

void CEncoder::Free()
{
  if (!ThreadsInfo)
    return;
  CloseThreads = true;
  CanProcessEvent.Set();
  for (UInt32 t = 0; t < NumThreads; t++)
  {
    CThreadInfo &ti = ThreadsInfo[t];
    if (MtMode)
      ti.Thread.Wait_Close();
    ti.Free();
  }
  delete []ThreadsInfo;
  ThreadsInfo = NULL;
}
#endif

UInt32 CEncoder::ReadRleBlock(Byte *buffer)
{
  UInt32 i = 0;
  Byte prevByte;
  if (m_InStream.ReadByte(prevByte))
  {
    NumBlocks++;
    const UInt32 blockSize = _props.BlockSizeMult * kBlockSizeStep - 1;
    unsigned numReps = 1;
    buffer[i++] = prevByte;
    while (i < blockSize) // "- 1" to support RLE
    {
      Byte b;
      if (!m_InStream.ReadByte(b))
        break;
      if (b != prevByte)
      {
        if (numReps >= kRleModeRepSize)
          buffer[i++] = (Byte)(numReps - kRleModeRepSize);
        buffer[i++] = b;
        numReps = 1;
        prevByte = b;
        continue;
      }
      numReps++;
      if (numReps <= kRleModeRepSize)
        buffer[i++] = b;
      else if (numReps == kRleModeRepSize + 255)
      {
        buffer[i++] = (Byte)(numReps - kRleModeRepSize);
        numReps = 0;
      }
    }
    // it's to support original BZip2 decoder
    if (numReps >= kRleModeRepSize)
      buffer[i++] = (Byte)(numReps - kRleModeRepSize);
  }
  return i;
}

void CThreadInfo::WriteBits2(UInt32 value, unsigned numBits) { m_OutStreamCurrent->WriteBits(value, numBits); }
void CThreadInfo::WriteByte2(Byte b) { WriteBits2(b, 8); }
void CThreadInfo::WriteBit2(Byte v) { WriteBits2(v, 1); }
void CThreadInfo::WriteCrc2(UInt32 v)
{
  for (unsigned i = 0; i < 4; i++)
    WriteByte2(((Byte)(v >> (24 - i * 8))));
}

void CEncoder::WriteBits(UInt32 value, unsigned numBits) { m_OutStream.WriteBits(value, numBits); }
void CEncoder::WriteByte(Byte b) { WriteBits(b, 8); }
// void CEncoder::WriteBit(Byte v) { WriteBits(v, 1); }
void CEncoder::WriteCrc(UInt32 v)
{
  for (unsigned i = 0; i < 4; i++)
    WriteByte(((Byte)(v >> (24 - i * 8))));
}


// blockSize > 0
void CThreadInfo::EncodeBlock(const Byte *block, UInt32 blockSize)
{
  WriteBit2(0); // Randomised = false
  
  {
    UInt32 origPtr = BlockSort(m_BlockSorterIndex, block, blockSize);
    // if (m_BlockSorterIndex[origPtr] != 0) throw 1;
    m_BlockSorterIndex[origPtr] = blockSize;
    WriteBits2(origPtr, kNumOrigBits);
  }

  CMtf8Encoder mtf;
  unsigned numInUse = 0;
  {
    Byte inUse[256];
    Byte inUse16[16];
    UInt32 i;
    for (i = 0; i < 256; i++)
      inUse[i] = 0;
    for (i = 0; i < 16; i++)
      inUse16[i] = 0;
    for (i = 0; i < blockSize; i++)
      inUse[block[i]] = 1;
    for (i = 0; i < 256; i++)
      if (inUse[i])
      {
        inUse16[i >> 4] = 1;
        mtf.Buf[numInUse++] = (Byte)i;
      }
    for (i = 0; i < 16; i++)
      WriteBit2(inUse16[i]);
    for (i = 0; i < 256; i++)
      if (inUse16[i >> 4])
        WriteBit2(inUse[i]);
  }
  unsigned alphaSize = numInUse + 2;

  Byte *mtfs = m_MtfArray;
  UInt32 mtfArraySize = 0;
  UInt32 symbolCounts[kMaxAlphaSize];
  {
    for (unsigned i = 0; i < kMaxAlphaSize; i++)
      symbolCounts[i] = 0;
  }

  {
    UInt32 rleSize = 0;
    UInt32 i = 0;
    const UInt32 *bsIndex = m_BlockSorterIndex;
    block--;
    do
    {
      unsigned pos = mtf.FindAndMove(block[bsIndex[i]]);
      if (pos == 0)
        rleSize++;
      else
      {
        while (rleSize != 0)
        {
          rleSize--;
          mtfs[mtfArraySize++] = (Byte)(rleSize & 1);
          symbolCounts[rleSize & 1]++;
          rleSize >>= 1;
        }
        if (pos >= 0xFE)
        {
          mtfs[mtfArraySize++] = 0xFF;
          mtfs[mtfArraySize++] = (Byte)(pos - 0xFE);
        }
        else
          mtfs[mtfArraySize++] = (Byte)(pos + 1);
        symbolCounts[(size_t)pos + 1]++;
      }
    }
    while (++i < blockSize);

    while (rleSize != 0)
    {
      rleSize--;
      mtfs[mtfArraySize++] = (Byte)(rleSize & 1);
      symbolCounts[rleSize & 1]++;
      rleSize >>= 1;
    }

    if (alphaSize < 256)
      mtfs[mtfArraySize++] = (Byte)(alphaSize - 1);
    else
    {
      mtfs[mtfArraySize++] = 0xFF;
      mtfs[mtfArraySize++] = (Byte)(alphaSize - 256);
    }
    symbolCounts[(size_t)alphaSize - 1]++;
  }

  UInt32 numSymbols = 0;
  {
    for (unsigned i = 0; i < kMaxAlphaSize; i++)
      numSymbols += symbolCounts[i];
  }

  unsigned bestNumTables = kNumTablesMin;
  UInt32 bestPrice = 0xFFFFFFFF;
  UInt32 startPos = m_OutStreamCurrent->GetPos();
  Byte startCurByte = m_OutStreamCurrent->GetCurByte();
  for (unsigned nt = kNumTablesMin; nt <= kNumTablesMax + 1; nt++)
  {
    unsigned numTables;

    if (m_OptimizeNumTables)
    {
      m_OutStreamCurrent->SetPos(startPos);
      m_OutStreamCurrent->SetCurState((startPos & 7), startCurByte);
      if (nt <= kNumTablesMax)
        numTables = nt;
      else
        numTables = bestNumTables;
    }
    else
    {
      if (numSymbols < 200)  numTables = 2;
      else if (numSymbols < 600) numTables = 3;
      else if (numSymbols < 1200) numTables = 4;
      else if (numSymbols < 2400) numTables = 5;
      else numTables = 6;
    }

    WriteBits2(numTables, kNumTablesBits);
    
    UInt32 numSelectors = (numSymbols + kGroupSize - 1) / kGroupSize;
    WriteBits2(numSelectors, kNumSelectorsBits);
    
    {
      UInt32 remFreq = numSymbols;
      unsigned gs = 0;
      unsigned t = numTables;
      do
      {
        UInt32 tFreq = remFreq / t;
        unsigned ge = gs;
        UInt32 aFreq = 0;
        while (aFreq < tFreq) //  && ge < alphaSize)
          aFreq += symbolCounts[ge++];
        
        if (ge > gs + 1 && t != numTables && t != 1 && (((numTables - t) & 1) == 1))
          aFreq -= symbolCounts[--ge];
        
        Byte *lens = Lens[(size_t)t - 1];
        unsigned i = 0;
        do
          lens[i] = (Byte)((i >= gs && i < ge) ? 0 : 1);
        while (++i < alphaSize);
        gs = ge;
        remFreq -= aFreq;
      }
      while (--t != 0);
    }
    
    
    for (unsigned pass = 0; pass < kNumHuffPasses; pass++)
    {
      {
        unsigned t = 0;
        do
          memset(Freqs[t], 0, sizeof(Freqs[t]));
        while (++t < numTables);
      }
      
      {
        UInt32 mtfPos = 0;
        UInt32 g = 0;
        do
        {
          UInt32 symbols[kGroupSize];
          unsigned i = 0;
          do
          {
            UInt32 symbol = mtfs[mtfPos++];
            if (symbol >= 0xFF)
              symbol += mtfs[mtfPos++];
            symbols[i] = symbol;
          }
          while (++i < kGroupSize && mtfPos < mtfArraySize);
          
          UInt32 bestPrice2 = 0xFFFFFFFF;
          unsigned t = 0;
          do
          {
            const Byte *lens = Lens[t];
            UInt32 price = 0;
            unsigned j = 0;
            do
              price += lens[symbols[j]];
            while (++j < i);
            if (price < bestPrice2)
            {
              m_Selectors[g] = (Byte)t;
              bestPrice2 = price;
            }
          }
          while (++t < numTables);
          UInt32 *freqs = Freqs[m_Selectors[g++]];
          unsigned j = 0;
          do
            freqs[symbols[j]]++;
          while (++j < i);
        }
        while (mtfPos < mtfArraySize);
      }
      
      unsigned t = 0;
      do
      {
        UInt32 *freqs = Freqs[t];
        unsigned i = 0;
        do
          if (freqs[i] == 0)
            freqs[i] = 1;
        while (++i < alphaSize);
        Huffman_Generate(freqs, Codes[t], Lens[t], kMaxAlphaSize, kMaxHuffmanLenForEncoding);
      }
      while (++t < numTables);
    }
    
    {
      Byte mtfSel[kNumTablesMax];
      {
        unsigned t = 0;
        do
          mtfSel[t] = (Byte)t;
        while (++t < numTables);
      }
      
      UInt32 i = 0;
      do
      {
        Byte sel = m_Selectors[i];
        unsigned pos;
        for (pos = 0; mtfSel[pos] != sel; pos++)
          WriteBit2(1);
        WriteBit2(0);
        for (; pos > 0; pos--)
          mtfSel[pos] = mtfSel[(size_t)pos - 1];
        mtfSel[0] = sel;
      }
      while (++i < numSelectors);
    }
    
    {
      unsigned t = 0;
      do
      {
        const Byte *lens = Lens[t];
        UInt32 len = lens[0];
        WriteBits2(len, kNumLevelsBits);
        unsigned i = 0;
        do
        {
          UInt32 level = lens[i];
          while (len != level)
          {
            WriteBit2(1);
            if (len < level)
            {
              WriteBit2(0);
              len++;
            }
            else
            {
              WriteBit2(1);
              len--;
            }
          }
          WriteBit2(0);
        }
        while (++i < alphaSize);
      }
      while (++t < numTables);
    }
    
    {
      UInt32 groupSize = 0;
      UInt32 groupIndex = 0;
      const Byte *lens = NULL;
      const UInt32 *codes = NULL;
      UInt32 mtfPos = 0;
      do
      {
        UInt32 symbol = mtfs[mtfPos++];
        if (symbol >= 0xFF)
          symbol += mtfs[mtfPos++];
        if (groupSize == 0)
        {
          groupSize = kGroupSize;
          unsigned t = m_Selectors[groupIndex++];
          lens = Lens[t];
          codes = Codes[t];
        }
        groupSize--;
        m_OutStreamCurrent->WriteBits(codes[symbol], lens[symbol]);
      }
      while (mtfPos < mtfArraySize);
    }

    if (!m_OptimizeNumTables)
      break;
    UInt32 price = m_OutStreamCurrent->GetPos() - startPos;
    if (price <= bestPrice)
    {
      if (nt == kNumTablesMax)
        break;
      bestPrice = price;
      bestNumTables = nt;
    }
  }
}

// blockSize > 0
UInt32 CThreadInfo::EncodeBlockWithHeaders(const Byte *block, UInt32 blockSize)
{
  WriteByte2(kBlockSig0);
  WriteByte2(kBlockSig1);
  WriteByte2(kBlockSig2);
  WriteByte2(kBlockSig3);
  WriteByte2(kBlockSig4);
  WriteByte2(kBlockSig5);

  CBZip2Crc crc;
  unsigned numReps = 0;
  Byte prevByte = block[0];
  UInt32 i = 0;
  do
  {
    Byte b = block[i];
    if (numReps == kRleModeRepSize)
    {
      for (; b > 0; b--)
        crc.UpdateByte(prevByte);
      numReps = 0;
      continue;
    }
    if (prevByte == b)
      numReps++;
    else
    {
      numReps = 1;
      prevByte = b;
    }
    crc.UpdateByte(b);
  }
  while (++i < blockSize);
  UInt32 crcRes = crc.GetDigest();
  WriteCrc2(crcRes);
  EncodeBlock(block, blockSize);
  return crcRes;
}

void CThreadInfo::EncodeBlock2(const Byte *block, UInt32 blockSize, UInt32 numPasses)
{
  UInt32 numCrcs = m_NumCrcs;
  bool needCompare = false;

  UInt32 startBytePos = m_OutStreamCurrent->GetBytePos();
  UInt32 startPos = m_OutStreamCurrent->GetPos();
  Byte startCurByte = m_OutStreamCurrent->GetCurByte();
  Byte endCurByte = 0;
  UInt32 endPos = 0;
  if (numPasses > 1 && blockSize >= (1 << 10))
  {
    UInt32 blockSize0 = blockSize / 2; // ????
    
    for (; (block[blockSize0] == block[(size_t)blockSize0 - 1]
            || block[(size_t)blockSize0 - 1] == block[(size_t)blockSize0 - 2])
          && blockSize0 < blockSize;
        blockSize0++);
    
    if (blockSize0 < blockSize)
    {
      EncodeBlock2(block, blockSize0, numPasses - 1);
      EncodeBlock2(block + blockSize0, blockSize - blockSize0, numPasses - 1);
      endPos = m_OutStreamCurrent->GetPos();
      endCurByte = m_OutStreamCurrent->GetCurByte();
      if ((endPos & 7) > 0)
        WriteBits2(0, 8 - (endPos & 7));
      m_OutStreamCurrent->SetCurState((startPos & 7), startCurByte);
      needCompare = true;
    }
  }

  UInt32 startBytePos2 = m_OutStreamCurrent->GetBytePos();
  UInt32 startPos2 = m_OutStreamCurrent->GetPos();
  UInt32 crcVal = EncodeBlockWithHeaders(block, blockSize);
  UInt32 endPos2 = m_OutStreamCurrent->GetPos();

  if (needCompare)
  {
    UInt32 size2 = endPos2 - startPos2;
    if (size2 < endPos - startPos)
    {
      UInt32 numBytes = m_OutStreamCurrent->GetBytePos() - startBytePos2;
      Byte *buffer = m_OutStreamCurrent->GetStream();
      for (UInt32 i = 0; i < numBytes; i++)
        buffer[startBytePos + i] = buffer[startBytePos2 + i];
      m_OutStreamCurrent->SetPos(startPos + endPos2 - startPos2);
      m_NumCrcs = numCrcs;
      m_CRCs[m_NumCrcs++] = crcVal;
    }
    else
    {
      m_OutStreamCurrent->SetPos(endPos);
      m_OutStreamCurrent->SetCurState((endPos & 7), endCurByte);
    }
  }
  else
  {
    m_NumCrcs = numCrcs;
    m_CRCs[m_NumCrcs++] = crcVal;
  }
}

HRESULT CThreadInfo::EncodeBlock3(UInt32 blockSize)
{
  CMsbfEncoderTemp outStreamTemp;
  outStreamTemp.SetStream(m_TempArray);
  outStreamTemp.Init();
  m_OutStreamCurrent = &outStreamTemp;

  m_NumCrcs = 0;

  EncodeBlock2(m_Block, blockSize, Encoder->_props.NumPasses);

  #ifndef Z7_ST
  if (Encoder->MtMode)
    Encoder->ThreadsInfo[m_BlockIndex].CanWriteEvent.Lock();
  #endif
  for (UInt32 i = 0; i < m_NumCrcs; i++)
    Encoder->CombinedCrc.Update(m_CRCs[i]);
  Encoder->WriteBytes(m_TempArray, outStreamTemp.GetPos(), outStreamTemp.GetCurByte());
  HRESULT res = S_OK;
  #ifndef Z7_ST
  if (Encoder->MtMode)
  {
    UInt32 blockIndex = m_BlockIndex + 1;
    if (blockIndex == Encoder->NumThreads)
      blockIndex = 0;

    if (Encoder->Progress)
    {
      const UInt64 packSize = Encoder->m_OutStream.GetProcessedSize();
      res = Encoder->Progress->SetRatioInfo(&m_UnpackSize, &packSize);
    }

    Encoder->ThreadsInfo[blockIndex].CanWriteEvent.Set();
  }
  #endif
  return res;
}

void CEncoder::WriteBytes(const Byte *data, UInt32 sizeInBits, Byte lastByte)
{
  UInt32 bytesSize = (sizeInBits >> 3);
  for (UInt32 i = 0; i < bytesSize; i++)
    m_OutStream.WriteBits(data[i], 8);
  WriteBits(lastByte, (sizeInBits & 7));
}


HRESULT CEncoder::CodeReal(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 * /* outSize */, ICompressProgressInfo *progress)
{
  NumBlocks = 0;
  #ifndef Z7_ST
  Progress = progress;
  RINOK(Create())
  for (UInt32 t = 0; t < NumThreads; t++)
  #endif
  {
    #ifndef Z7_ST
    CThreadInfo &ti = ThreadsInfo[t];
    if (MtMode)
    {
      WRes             wres = ti.StreamWasFinishedEvent.Reset();
      if (wres == 0) { wres = ti.WaitingWasStartedEvent.Reset();
      if (wres == 0) { wres = ti.CanWriteEvent.Reset(); }}
      if (wres != 0)
        return HRESULT_FROM_WIN32(wres);
    }
    #else
    CThreadInfo &ti = ThreadsInfo;
    ti.Encoder = this;
    #endif

    ti.m_OptimizeNumTables = _props.DoOptimizeNumTables();

    if (!ti.Alloc())
      return E_OUTOFMEMORY;
  }


  if (!m_InStream.Create(kBufferSize))
    return E_OUTOFMEMORY;
  if (!m_OutStream.Create(kBufferSize))
    return E_OUTOFMEMORY;


  m_InStream.SetStream(inStream);
  m_InStream.Init();

  m_OutStream.SetStream(outStream);
  m_OutStream.Init();

  CombinedCrc.Init();
  #ifndef Z7_ST
  NextBlockIndex = 0;
  StreamWasFinished = false;
  CloseThreads = false;
  CanStartWaitingEvent.Reset();
  #endif

  WriteByte(kArSig0);
  WriteByte(kArSig1);
  WriteByte(kArSig2);
  WriteByte((Byte)(kArSig3 + _props.BlockSizeMult));

  #ifndef Z7_ST

  if (MtMode)
  {
    ThreadsInfo[0].CanWriteEvent.Set();
    Result = S_OK;
    CanProcessEvent.Set();
    UInt32 t;
    for (t = 0; t < NumThreads; t++)
      ThreadsInfo[t].StreamWasFinishedEvent.Lock();
    CanProcessEvent.Reset();
    CanStartWaitingEvent.Set();
    for (t = 0; t < NumThreads; t++)
      ThreadsInfo[t].WaitingWasStartedEvent.Lock();
    CanStartWaitingEvent.Reset();
    RINOK(Result)
  }
  else
  #endif
  {
    for (;;)
    {
      CThreadInfo &ti =
      #ifndef Z7_ST
      ThreadsInfo[0];
      #else
      ThreadsInfo;
      #endif
      UInt32 blockSize = ReadRleBlock(ti.m_Block);
      if (blockSize == 0)
        break;
      RINOK(ti.EncodeBlock3(blockSize))
      if (progress)
      {
        const UInt64 unpackSize = m_InStream.GetProcessedSize();
        const UInt64 packSize = m_OutStream.GetProcessedSize();
        RINOK(progress->SetRatioInfo(&unpackSize, &packSize))
      }
    }
  }
  WriteByte(kFinSig0);
  WriteByte(kFinSig1);
  WriteByte(kFinSig2);
  WriteByte(kFinSig3);
  WriteByte(kFinSig4);
  WriteByte(kFinSig5);

  WriteCrc(CombinedCrc.GetDigest());
  RINOK(Flush())
  if (!m_InStream.WasFinished())
    return E_FAIL;
  return S_OK;
}

Z7_COM7F_IMF(CEncoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress))
{
  try { return CodeReal(inStream, outStream, inSize, outSize, progress); }
  catch(const CInBufferException &e) { return e.ErrorCode; }
  catch(const COutBufferException &e) { return e.ErrorCode; }
  catch(...) { return S_FALSE; }
}

Z7_COM7F_IMF(CEncoder::SetCoderProperties(const PROPID *propIDs, const PROPVARIANT *coderProps, UInt32 numProps))
{
  int level = -1;
  CEncProps props;
  for (UInt32 i = 0; i < numProps; i++)
  {
    const PROPVARIANT &prop = coderProps[i];
    PROPID propID = propIDs[i];

    if (propID == NCoderPropID::kAffinity)
    {
      if (prop.vt == VT_UI8)
        props.Affinity = prop.uhVal.QuadPart;
      else
        return E_INVALIDARG;
      continue;
    }

    if (propID >= NCoderPropID::kReduceSize)
      continue;
    if (prop.vt != VT_UI4)
      return E_INVALIDARG;
    UInt32 v = (UInt32)prop.ulVal;
    switch (propID)
    {
      case NCoderPropID::kNumPasses: props.NumPasses = v; break;
      case NCoderPropID::kDictionarySize: props.BlockSizeMult = v / kBlockSizeStep; break;
      case NCoderPropID::kLevel: level = (int)v; break;
      case NCoderPropID::kNumThreads:
      {
        #ifndef Z7_ST
        SetNumberOfThreads(v);
        #endif
        break;
      }
      default: return E_INVALIDARG;
    }
  }
  props.Normalize(level);
  _props = props;
  return S_OK;
}

#ifndef Z7_ST
Z7_COM7F_IMF(CEncoder::SetNumberOfThreads(UInt32 numThreads))
{
  const UInt32 kNumThreadsMax = 64;
  if (numThreads < 1) numThreads = 1;
  if (numThreads > kNumThreadsMax) numThreads = kNumThreadsMax;
  NumThreads = numThreads;
  return S_OK;
}
#endif

}}

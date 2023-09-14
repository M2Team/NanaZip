// BZip2Encoder.h

#ifndef ZIP7_INC_COMPRESS_BZIP2_ENCODER_H
#define ZIP7_INC_COMPRESS_BZIP2_ENCODER_H

#include "../../Common/Defs.h"
#include "../../Common/MyCom.h"

#ifndef Z7_ST
#include "../../Windows/Synchronization.h"
#include "../../Windows/Thread.h"
#endif

#include "../ICoder.h"

#include "../Common/InBuffer.h"
#include "../Common/OutBuffer.h"

#include "BitmEncoder.h"
#include "BZip2Const.h"
#include "BZip2Crc.h"

namespace NCompress {
namespace NBZip2 {

class CMsbfEncoderTemp
{
  UInt32 _pos;
  unsigned _bitPos;
  Byte _curByte;
  Byte *_buf;
public:
  void SetStream(Byte *buf) { _buf = buf;  }
  Byte *GetStream() const { return _buf; }

  void Init()
  {
    _pos = 0;
    _bitPos = 8;
    _curByte = 0;
  }

  void Flush()
  {
    if (_bitPos < 8)
      WriteBits(0, _bitPos);
  }

  void WriteBits(UInt32 value, unsigned numBits)
  {
    while (numBits > 0)
    {
      unsigned numNewBits = MyMin(numBits, _bitPos);
      numBits -= numNewBits;
      
      _curByte = (Byte)(_curByte << numNewBits);
      UInt32 newBits = value >> numBits;
      _curByte |= Byte(newBits);
      value -= (newBits << numBits);
      
      _bitPos -= numNewBits;
      
      if (_bitPos == 0)
      {
       _buf[_pos++] = _curByte;
        _bitPos = 8;
      }
    }
  }
  
  UInt32 GetBytePos() const { return _pos ; }
  UInt32 GetPos() const { return _pos * 8 + (8 - _bitPos); }
  Byte GetCurByte() const { return _curByte; }
  void SetPos(UInt32 bitPos)
  {
    _pos = bitPos >> 3;
    _bitPos = 8 - ((unsigned)bitPos & 7);
  }
  void SetCurState(unsigned bitPos, Byte curByte)
  {
    _bitPos = 8 - bitPos;
    _curByte = curByte;
  }
};

class CEncoder;

const unsigned kNumPassesMax = 10;

class CThreadInfo
{
public:
  Byte *m_Block;
private:
  Byte *m_MtfArray;
  Byte *m_TempArray;
  UInt32 *m_BlockSorterIndex;

  CMsbfEncoderTemp *m_OutStreamCurrent;

  Byte Lens[kNumTablesMax][kMaxAlphaSize];
  UInt32 Freqs[kNumTablesMax][kMaxAlphaSize];
  UInt32 Codes[kNumTablesMax][kMaxAlphaSize];

  Byte m_Selectors[kNumSelectorsMax];

  UInt32 m_CRCs[1 << kNumPassesMax];
  UInt32 m_NumCrcs;

  void WriteBits2(UInt32 value, unsigned numBits);
  void WriteByte2(Byte b);
  void WriteBit2(Byte v);
  void WriteCrc2(UInt32 v);

  void EncodeBlock(const Byte *block, UInt32 blockSize);
  UInt32 EncodeBlockWithHeaders(const Byte *block, UInt32 blockSize);
  void EncodeBlock2(const Byte *block, UInt32 blockSize, UInt32 numPasses);
public:
  bool m_OptimizeNumTables;
  CEncoder *Encoder;
 #ifndef Z7_ST
  NWindows::CThread Thread;

  NWindows::NSynchronization::CAutoResetEvent StreamWasFinishedEvent;
  NWindows::NSynchronization::CAutoResetEvent WaitingWasStartedEvent;

  // it's not member of this thread. We just need one event per thread
  NWindows::NSynchronization::CAutoResetEvent CanWriteEvent;

private:
  UInt32 m_BlockIndex;
  UInt64 m_UnpackSize;
public:
  Byte MtPad[1 << 8]; // It's pad for Multi-Threading. Must be >= Cache_Line_Size.
  HRESULT Create();
  void FinishStream(bool needLeave);
  THREAD_FUNC_RET_TYPE ThreadFunc();
 #endif

  CThreadInfo(): m_Block(NULL), m_BlockSorterIndex(NULL)  {}
  ~CThreadInfo() { Free(); }
  bool Alloc();
  void Free();

  HRESULT EncodeBlock3(UInt32 blockSize);
};

struct CEncProps
{
  UInt32 BlockSizeMult;
  UInt32 NumPasses;
  UInt64 Affinity;
  
  CEncProps()
  {
    BlockSizeMult = (UInt32)(Int32)-1;
    NumPasses = (UInt32)(Int32)-1;
    Affinity = 0;
  }
  void Normalize(int level);
  bool DoOptimizeNumTables() const { return NumPasses > 1; }
};

class CEncoder Z7_final:
  public ICompressCoder,
  public ICompressSetCoderProperties,
 #ifndef Z7_ST
  public ICompressSetCoderMt,
 #endif
  public CMyUnknownImp
{
  Z7_COM_QI_BEGIN2(ICompressCoder)
  Z7_COM_QI_ENTRY(ICompressSetCoderProperties)
 #ifndef Z7_ST
  Z7_COM_QI_ENTRY(ICompressSetCoderMt)
 #endif
  Z7_COM_QI_END
  Z7_COM_ADDREF_RELEASE

  Z7_IFACE_COM7_IMP(ICompressCoder)
  Z7_IFACE_COM7_IMP(ICompressSetCoderProperties)
 #ifndef Z7_ST
  Z7_IFACE_COM7_IMP(ICompressSetCoderMt)
 #endif

 #ifndef Z7_ST
  UInt32 m_NumThreadsPrev;
 #endif
public:
  CInBuffer m_InStream;
 #ifndef Z7_ST
  Byte MtPad[1 << 8]; // It's pad for Multi-Threading. Must be >= Cache_Line_Size.
 #endif
  CBitmEncoder<COutBuffer> m_OutStream;
  CEncProps _props;
  CBZip2CombinedCrc CombinedCrc;

 #ifndef Z7_ST
  CThreadInfo *ThreadsInfo;
  NWindows::NSynchronization::CManualResetEvent CanProcessEvent;
  NWindows::NSynchronization::CCriticalSection CS;
  UInt32 NumThreads;
  bool MtMode;
  UInt32 NextBlockIndex;

  bool CloseThreads;
  bool StreamWasFinished;
  NWindows::NSynchronization::CManualResetEvent CanStartWaitingEvent;

  HRESULT Result;
  ICompressProgressInfo *Progress;
 #else
  CThreadInfo ThreadsInfo;
 #endif

  UInt64 NumBlocks;

  UInt64 GetInProcessedSize() const { return m_InStream.GetProcessedSize(); }

  UInt32 ReadRleBlock(Byte *buf);
  void WriteBytes(const Byte *data, UInt32 sizeInBits, Byte lastByte);

  void WriteBits(UInt32 value, unsigned numBits);
  void WriteByte(Byte b);
  // void WriteBit(Byte v);
  void WriteCrc(UInt32 v);

 #ifndef Z7_ST
  HRESULT Create();
  void Free();
 #endif

public:
  CEncoder();
 #ifndef Z7_ST
  ~CEncoder();
 #endif

  HRESULT Flush() { return m_OutStream.Flush(); }
  
  HRESULT CodeReal(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
};

}}

#endif

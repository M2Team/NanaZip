// DeflateDecoder.h

#ifndef __DEFLATE_DECODER_H
#define __DEFLATE_DECODER_H

#include "../../Common/MyCom.h"

#include "../ICoder.h"

#include "../Common/InBuffer.h"

#include "BitlDecoder.h"
#include "DeflateConst.h"
#include "HuffmanDecoder.h"
#include "LzOutWindow.h"

namespace NCompress {
namespace NDeflate {
namespace NDecoder {

const int kLenIdFinished = -1;
const int kLenIdNeedInit = -2;

class CCoder:
  public ICompressCoder,
  public ICompressSetFinishMode,
  public ICompressGetInStreamProcessedSize,
  public ICompressReadUnusedFromInBuf,
  #ifndef NO_READ_FROM_CODER
  public ICompressSetInStream,
  public ICompressSetOutStreamSize,
  public ISequentialInStream,
  #endif
  public CMyUnknownImp
{
  CLzOutWindow m_OutWindowStream;
  CMyComPtr<ISequentialInStream> m_InStreamRef;
  NBitl::CDecoder<CInBuffer> m_InBitStream;
  NCompress::NHuffman::CDecoder<kNumHuffmanBits, kFixedMainTableSize> m_MainDecoder;
  NCompress::NHuffman::CDecoder<kNumHuffmanBits, kFixedDistTableSize> m_DistDecoder;
  NCompress::NHuffman::CDecoder7b<kLevelTableSize> m_LevelDecoder;

  UInt32 m_StoredBlockSize;

  UInt32 _numDistLevels;
  bool m_FinalBlock;
  bool m_StoredMode;

  bool _deflateNSIS;
  bool _deflate64Mode;
  bool _keepHistory;
  bool _needFinishInput;
  
  bool _needInitInStream;
  bool _needReadTable;
  Int32 _remainLen;
  UInt32 _rep0;

  bool _outSizeDefined;
  UInt64 _outSize;
  UInt64 _outStartPos;

  void SetOutStreamSizeResume(const UInt64 *outSize);
  UInt64 GetOutProcessedCur() const { return m_OutWindowStream.GetProcessedSize() - _outStartPos; }

  UInt32 ReadBits(unsigned numBits);

  bool DecodeLevels(Byte *levels, unsigned numSymbols);
  bool ReadTables();
  
  HRESULT Flush() { return m_OutWindowStream.Flush(); }
  class CCoderReleaser
  {
    CCoder *_coder;
  public:
    bool NeedFlush;
    CCoderReleaser(CCoder *coder): _coder(coder), NeedFlush(true) {}
    ~CCoderReleaser()
    {
      if (NeedFlush)
        _coder->Flush();
    }
  };
  friend class CCoderReleaser;

  HRESULT CodeSpec(UInt32 curSize, bool finishInputStream, UInt32 inputProgressLimit = 0);
public:
  bool ZlibMode;
  Byte ZlibFooter[4];

  CCoder(bool deflate64Mode);
  virtual ~CCoder() {};

  void SetNsisMode(bool nsisMode) { _deflateNSIS = nsisMode; }

  void Set_KeepHistory(bool keepHistory) { _keepHistory = keepHistory; }
  void Set_NeedFinishInput(bool needFinishInput) { _needFinishInput = needFinishInput; }

  bool IsFinished() const { return _remainLen == kLenIdFinished;; }
  bool IsFinalBlock() const { return m_FinalBlock; }

  HRESULT CodeReal(ISequentialOutStream *outStream, ICompressProgressInfo *progress);

  MY_QUERYINTERFACE_BEGIN2(ICompressCoder)
  MY_QUERYINTERFACE_ENTRY(ICompressSetFinishMode)
  MY_QUERYINTERFACE_ENTRY(ICompressGetInStreamProcessedSize)
  MY_QUERYINTERFACE_ENTRY(ICompressReadUnusedFromInBuf)

  #ifndef NO_READ_FROM_CODER
  MY_QUERYINTERFACE_ENTRY(ICompressSetInStream)
  MY_QUERYINTERFACE_ENTRY(ICompressSetOutStreamSize)
  MY_QUERYINTERFACE_ENTRY(ISequentialInStream)
  #endif

  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE


  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);

  STDMETHOD(SetFinishMode)(UInt32 finishMode);
  STDMETHOD(GetInStreamProcessedSize)(UInt64 *value);
  STDMETHOD(ReadUnusedFromInBuf)(void *data, UInt32 size, UInt32 *processedSize);

  STDMETHOD(SetInStream)(ISequentialInStream *inStream);
  STDMETHOD(ReleaseInStream)();
  STDMETHOD(SetOutStreamSize)(const UInt64 *outSize);
  
  #ifndef NO_READ_FROM_CODER
  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);
  #endif

  HRESULT CodeResume(ISequentialOutStream *outStream, const UInt64 *outSize, ICompressProgressInfo *progress);

  HRESULT InitInStream(bool needInit);

  void AlignToByte() { m_InBitStream.AlignToByte(); }
  Byte ReadAlignedByte();
  UInt32 ReadAligned_UInt16() // aligned for Byte range
  {
    UInt32 v = m_InBitStream.ReadAlignedByte();
    return v | ((UInt32)m_InBitStream.ReadAlignedByte() << 8);
  }
  bool InputEofError() const { return m_InBitStream.ExtraBitsWereRead(); }

  // size of used real data from input stream
  UInt64 GetStreamSize() const { return m_InBitStream.GetStreamSize(); }

  // size of virtual input stream processed
  UInt64 GetInputProcessedSize() const { return m_InBitStream.GetProcessedSize(); }
};

class CCOMCoder     : public CCoder { public: CCOMCoder(): CCoder(false) {} };
class CCOMCoder64   : public CCoder { public: CCOMCoder64(): CCoder(true) {} };

}}}

#endif

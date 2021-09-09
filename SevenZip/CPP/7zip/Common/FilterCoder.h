// FilterCoder.h

#ifndef __FILTER_CODER_H
#define __FILTER_CODER_H

#include "../../../C/Alloc.h"

#include "../../Common/MyCom.h"
#include "../ICoder.h"

#ifndef _NO_CRYPTO
#include "../IPassword.h"
#endif

#define MY_QUERYINTERFACE_ENTRY_AG(i, sub0, sub) else if (iid == IID_ ## i) \
  { if (!sub) RINOK(sub0->QueryInterface(IID_ ## i, (void **)&sub)) \
    *outObject = (void *)(i *)this; }


struct CAlignedMidBuffer
{
  Byte *_buf;

  CAlignedMidBuffer(): _buf(NULL) {}
  ~CAlignedMidBuffer();
  void AllocAligned(size_t size);
};

class CFilterCoder:
  public ICompressCoder,
  
  public ICompressSetOutStreamSize,
  public ICompressInitEncoder,
 
  public ICompressSetInStream,
  public ISequentialInStream,
  
  public ICompressSetOutStream,
  public ISequentialOutStream,
  public IOutStreamFinish,
  
  public ICompressSetBufSize,

  #ifndef _NO_CRYPTO
  public ICryptoSetPassword,
  public ICryptoProperties,
  #endif
  
  #ifndef EXTRACT_ONLY
  public ICompressSetCoderProperties,
  public ICompressWriteCoderProperties,
  // public ICryptoResetSalt,
  public ICryptoResetInitVector,
  #endif
  
  public ICompressSetDecoderProperties2,
  public CMyUnknownImp,
  public CAlignedMidBuffer
{
  UInt32 _bufSize;
  UInt32 _inBufSize;
  UInt32 _outBufSize;

  bool _encodeMode;
  bool _outSizeIsDefined;
  UInt64 _outSize;
  UInt64 _nowPos64;

  CMyComPtr<ISequentialInStream> _inStream;
  CMyComPtr<ISequentialOutStream> _outStream;
  UInt32 _bufPos;
  UInt32 _convPos;    // current pos in buffer for converted data
  UInt32 _convSize;   // size of converted data starting from _convPos
  
  void InitSpecVars()
  {
    _bufPos = 0;
    _convPos = 0;
    _convSize = 0;

    _outSizeIsDefined = false;
    _outSize = 0;
    _nowPos64 = 0;
  }

  HRESULT Alloc();
  HRESULT Init_and_Alloc();
  HRESULT Flush2();

  #ifndef _NO_CRYPTO
  CMyComPtr<ICryptoSetPassword> _SetPassword;
  CMyComPtr<ICryptoProperties> _CryptoProperties;
  #endif

  #ifndef EXTRACT_ONLY
  CMyComPtr<ICompressSetCoderProperties> _SetCoderProperties;
  CMyComPtr<ICompressWriteCoderProperties> _WriteCoderProperties;
  // CMyComPtr<ICryptoResetSalt> _CryptoResetSalt;
  CMyComPtr<ICryptoResetInitVector> _CryptoResetInitVector;
  #endif

  CMyComPtr<ICompressSetDecoderProperties2> _SetDecoderProperties2;

public:
  CMyComPtr<ICompressFilter> Filter;

  CFilterCoder(bool encodeMode);
  ~CFilterCoder();

  class C_InStream_Releaser
  {
  public:
    CFilterCoder *FilterCoder;
    C_InStream_Releaser(): FilterCoder(NULL) {}
    ~C_InStream_Releaser() { if (FilterCoder) FilterCoder->ReleaseInStream(); }
  };
  
  class C_OutStream_Releaser
  {
  public:
    CFilterCoder *FilterCoder;
    C_OutStream_Releaser(): FilterCoder(NULL) {}
    ~C_OutStream_Releaser() { if (FilterCoder) FilterCoder->ReleaseOutStream(); }
  };

  class C_Filter_Releaser
  {
  public:
    CFilterCoder *FilterCoder;
    C_Filter_Releaser(): FilterCoder(NULL) {}
    ~C_Filter_Releaser() { if (FilterCoder) FilterCoder->Filter.Release(); }
  };
  

  MY_QUERYINTERFACE_BEGIN2(ICompressCoder)

    MY_QUERYINTERFACE_ENTRY(ICompressSetOutStreamSize)
    MY_QUERYINTERFACE_ENTRY(ICompressInitEncoder)
    
    MY_QUERYINTERFACE_ENTRY(ICompressSetInStream)
    MY_QUERYINTERFACE_ENTRY(ISequentialInStream)
    
    MY_QUERYINTERFACE_ENTRY(ICompressSetOutStream)
    MY_QUERYINTERFACE_ENTRY(ISequentialOutStream)
    MY_QUERYINTERFACE_ENTRY(IOutStreamFinish)
    
    MY_QUERYINTERFACE_ENTRY(ICompressSetBufSize)

    #ifndef _NO_CRYPTO
    MY_QUERYINTERFACE_ENTRY_AG(ICryptoSetPassword, Filter, _SetPassword)
    MY_QUERYINTERFACE_ENTRY_AG(ICryptoProperties, Filter, _CryptoProperties)
    #endif

    #ifndef EXTRACT_ONLY
    MY_QUERYINTERFACE_ENTRY_AG(ICompressSetCoderProperties, Filter, _SetCoderProperties)
    MY_QUERYINTERFACE_ENTRY_AG(ICompressWriteCoderProperties, Filter, _WriteCoderProperties)
    // MY_QUERYINTERFACE_ENTRY_AG(ICryptoResetSalt, Filter, _CryptoResetSalt)
    MY_QUERYINTERFACE_ENTRY_AG(ICryptoResetInitVector, Filter, _CryptoResetInitVector)
    #endif

    MY_QUERYINTERFACE_ENTRY_AG(ICompressSetDecoderProperties2, Filter, _SetDecoderProperties2)
  MY_QUERYINTERFACE_END
  MY_ADDREF_RELEASE
  
  
  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
  
  STDMETHOD(SetOutStreamSize)(const UInt64 *outSize);
  STDMETHOD(InitEncoder)();

  STDMETHOD(SetInStream)(ISequentialInStream *inStream);
  STDMETHOD(ReleaseInStream)();
  STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);

  STDMETHOD(SetOutStream)(ISequentialOutStream *outStream);
  STDMETHOD(ReleaseOutStream)();
  STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);
  STDMETHOD(OutStreamFinish)();
  
  STDMETHOD(SetInBufSize)(UInt32 streamIndex, UInt32 size);
  STDMETHOD(SetOutBufSize)(UInt32 streamIndex, UInt32 size);

  #ifndef _NO_CRYPTO
  STDMETHOD(CryptoSetPassword)(const Byte *data, UInt32 size);

  STDMETHOD(SetKey)(const Byte *data, UInt32 size);
  STDMETHOD(SetInitVector)(const Byte *data, UInt32 size);
  #endif
  
  #ifndef EXTRACT_ONLY
  STDMETHOD(SetCoderProperties)(const PROPID *propIDs,
      const PROPVARIANT *properties, UInt32 numProperties);
  STDMETHOD(WriteCoderProperties)(ISequentialOutStream *outStream);
  // STDMETHOD(ResetSalt)();
  STDMETHOD(ResetInitVector)();
  #endif
  
  STDMETHOD(SetDecoderProperties2)(const Byte *data, UInt32 size);

  
  HRESULT Init_NoSubFilterInit();
};

#endif

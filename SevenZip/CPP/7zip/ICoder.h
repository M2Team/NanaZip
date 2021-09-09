// ICoder.h

#ifndef __ICODER_H
#define __ICODER_H

#include "IStream.h"

#define CODER_INTERFACE(i, x) DECL_INTERFACE(i, 4, x)

CODER_INTERFACE(ICompressProgressInfo, 0x04)
{
  STDMETHOD(SetRatioInfo)(const UInt64 *inSize, const UInt64 *outSize) PURE;
  
  /* (inSize) can be NULL, if unknown
     (outSize) can be NULL, if unknown

  returns:
    S_OK
    E_ABORT  : Break by user
    another error codes
  */
};

CODER_INTERFACE(ICompressCoder, 0x05)
{
  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize,
      ICompressProgressInfo *progress) PURE;
};

CODER_INTERFACE(ICompressCoder2, 0x18)
{
  STDMETHOD(Code)(ISequentialInStream * const *inStreams, const UInt64 * const *inSizes, UInt32 numInStreams,
      ISequentialOutStream * const *outStreams, const UInt64 * const *outSizes, UInt32 numOutStreams,
      ICompressProgressInfo *progress) PURE;
};

/*
  ICompressCoder::Code
  ICompressCoder2::Code
  
  returns:
    S_OK     : OK
    S_FALSE  : data error (for decoders)
    E_OUTOFMEMORY : memory allocation error
    E_NOTIMPL : unsupported encoding method (for decoders)
    another error code : some error. For example, it can be error code received from inStream or outStream function.
  
  Parameters:
    (inStream != NULL)
    (outStream != NULL)

    if (inSize != NULL)
    {
      Encoders in 7-Zip ignore (inSize).
      Decoder can use (*inSize) to check that stream was decoded correctly.
      Some decoder in 7-Zip check it, if (full_decoding mode was set via ICompressSetFinishMode)
    }

    If it's required to limit the reading from input stream (inStream), it can
      be done with ISequentialInStream implementation.

    if (outSize != NULL)
    {
      Encoders in 7-Zip ignore (outSize).
      Decoder unpacks no more than (*outSize) bytes.
    }
    
    (progress == NULL) is allowed.


  Decoding with Code() function
  -----------------------------
   
  You can request some interfaces before decoding
   - ICompressSetDecoderProperties2
   - ICompressSetFinishMode

  If you need to decode full stream:
  {
    1) try to set full_decoding mode with ICompressSetFinishMode::SetFinishMode(1);
    2) call the Code() function with specified (inSize) and (outSize), if these sizes are known.
  }

  If you need to decode only part of stream:
  {
    1) try to set partial_decoding mode with ICompressSetFinishMode::SetFinishMode(0);
    2) Call the Code() function with specified (inSize = NULL) and specified (outSize).
  }

  Encoding with Code() function
  -----------------------------
  
  You can request some interfaces :
  - ICompressSetCoderProperties   - use it before encoding to set properties
  - ICompressWriteCoderProperties - use it before or after encoding to request encoded properties.

  ICompressCoder2 is used when (numInStreams != 1 || numOutStreams != 1)
     The rules are similar to ICompressCoder rules
*/


namespace NCoderPropID
{
  enum EEnum
  {
    kDefaultProp = 0,
    kDictionarySize,    // VT_UI4
    kUsedMemorySize,    // VT_UI4
    kOrder,             // VT_UI4
    kBlockSize,         // VT_UI4 or VT_UI8
    kPosStateBits,      // VT_UI4
    kLitContextBits,    // VT_UI4
    kLitPosBits,        // VT_UI4
    kNumFastBytes,      // VT_UI4
    kMatchFinder,       // VT_BSTR
    kMatchFinderCycles, // VT_UI4
    kNumPasses,         // VT_UI4
    kAlgorithm,         // VT_UI4
    kNumThreads,        // VT_UI4
    kEndMarker,         // VT_BOOL
    kLevel,             // VT_UI4
    kReduceSize,        // VT_UI8 : it's estimated size of largest data stream that will be compressed
                        //   encoder can use this value to reduce dictionary size and allocate data buffers

    kExpectedDataSize,  // VT_UI8 : for ICompressSetCoderPropertiesOpt :
                        //   it's estimated size of current data stream
                        //   real data size can differ from that size
                        //   encoder can use this value to optimize encoder initialization

    kBlockSize2,        // VT_UI4 or VT_UI8
    kCheckSize,         // VT_UI4 : size of digest in bytes
    kFilter,            // VT_BSTR
    kMemUse,            // VT_UI8
    kAffinity           // VT_UI8
  };
}

CODER_INTERFACE(ICompressSetCoderPropertiesOpt, 0x1F)
{
  STDMETHOD(SetCoderPropertiesOpt)(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps) PURE;
};

CODER_INTERFACE(ICompressSetCoderProperties, 0x20)
{
  STDMETHOD(SetCoderProperties)(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps) PURE;
};

/*
CODER_INTERFACE(ICompressSetCoderProperties, 0x21)
{
  STDMETHOD(SetDecoderProperties)(ISequentialInStream *inStream) PURE;
};
*/

CODER_INTERFACE(ICompressSetDecoderProperties2, 0x22)
{
  /* returns:
    S_OK
    E_NOTIMP      : unsupported properties
    E_INVALIDARG  : incorrect (or unsupported) properties
    E_OUTOFMEMORY : memory allocation error
  */
  STDMETHOD(SetDecoderProperties2)(const Byte *data, UInt32 size) PURE;
};

CODER_INTERFACE(ICompressWriteCoderProperties, 0x23)
{
  STDMETHOD(WriteCoderProperties)(ISequentialOutStream *outStream) PURE;
};

CODER_INTERFACE(ICompressGetInStreamProcessedSize, 0x24)
{
  STDMETHOD(GetInStreamProcessedSize)(UInt64 *value) PURE;
};

CODER_INTERFACE(ICompressSetCoderMt, 0x25)
{
  STDMETHOD(SetNumberOfThreads)(UInt32 numThreads) PURE;
};

CODER_INTERFACE(ICompressSetFinishMode, 0x26)
{
  STDMETHOD(SetFinishMode)(UInt32 finishMode) PURE;

  /* finishMode:
    0 : partial decoding is allowed. It's default mode for ICompressCoder::Code(), if (outSize) is defined.
    1 : full decoding. The stream must be finished at the end of decoding. */
};

CODER_INTERFACE(ICompressGetInStreamProcessedSize2, 0x27)
{
  STDMETHOD(GetInStreamProcessedSize2)(UInt32 streamIndex, UInt64 *value) PURE;
};

CODER_INTERFACE(ICompressSetMemLimit, 0x28)
{
  STDMETHOD(SetMemLimit)(UInt64 memUsage) PURE;
};


/*
  ICompressReadUnusedFromInBuf is supported by ICoder object
  call ReadUnusedFromInBuf() after ICoder::Code(inStream, ...).
  ICoder::Code(inStream, ...) decodes data, and the ICoder object is allowed
  to read from inStream to internal buffers more data than minimal data required for decoding.
  So we can call ReadUnusedFromInBuf() from same ICoder object to read unused input
  data from the internal buffer.
  in ReadUnusedFromInBuf(): the Coder is not allowed to use (ISequentialInStream *inStream) object, that was sent to ICoder::Code().
*/

CODER_INTERFACE(ICompressReadUnusedFromInBuf, 0x29)
{
  STDMETHOD(ReadUnusedFromInBuf)(void *data, UInt32 size, UInt32 *processedSize) PURE;
};



CODER_INTERFACE(ICompressGetSubStreamSize, 0x30)
{
  STDMETHOD(GetSubStreamSize)(UInt64 subStream, UInt64 *value) PURE;

  /* returns:
    S_OK     : (*value) contains the size or estimated size (can be incorrect size)
    S_FALSE  : size is undefined
    E_NOTIMP : the feature is not implemented

  Let's (read_size) is size of data that was already read by ISequentialInStream::Read().
  The caller should call GetSubStreamSize() after each Read() and check sizes:
    if (start_of_subStream + *value < read_size)
    {
      // (*value) is correct, and it's allowed to call GetSubStreamSize() for next subStream:
      start_of_subStream += *value;
      subStream++;
    }
  */
};

CODER_INTERFACE(ICompressSetInStream, 0x31)
{
  STDMETHOD(SetInStream)(ISequentialInStream *inStream) PURE;
  STDMETHOD(ReleaseInStream)() PURE;
};

CODER_INTERFACE(ICompressSetOutStream, 0x32)
{
  STDMETHOD(SetOutStream)(ISequentialOutStream *outStream) PURE;
  STDMETHOD(ReleaseOutStream)() PURE;
};

/*
CODER_INTERFACE(ICompressSetInStreamSize, 0x33)
{
  STDMETHOD(SetInStreamSize)(const UInt64 *inSize) PURE;
};
*/

CODER_INTERFACE(ICompressSetOutStreamSize, 0x34)
{
  STDMETHOD(SetOutStreamSize)(const UInt64 *outSize) PURE;

  /* That function initializes decoder structures.
     Call this function only for stream version of decoder.
       if (outSize == NULL), then output size is unknown
       if (outSize != NULL), then the decoder must stop decoding after (*outSize) bytes. */
};

CODER_INTERFACE(ICompressSetBufSize, 0x35)
{
  STDMETHOD(SetInBufSize)(UInt32 streamIndex, UInt32 size) PURE;
  STDMETHOD(SetOutBufSize)(UInt32 streamIndex, UInt32 size) PURE;
};

CODER_INTERFACE(ICompressInitEncoder, 0x36)
{
  STDMETHOD(InitEncoder)() PURE;

  /* That function initializes encoder structures.
     Call this function only for stream version of encoder. */
};

CODER_INTERFACE(ICompressSetInStream2, 0x37)
{
  STDMETHOD(SetInStream2)(UInt32 streamIndex, ISequentialInStream *inStream) PURE;
  STDMETHOD(ReleaseInStream2)(UInt32 streamIndex) PURE;
};

/*
CODER_INTERFACE(ICompressSetOutStream2, 0x38)
{
  STDMETHOD(SetOutStream2)(UInt32 streamIndex, ISequentialOutStream *outStream) PURE;
  STDMETHOD(ReleaseOutStream2)(UInt32 streamIndex) PURE;
};

CODER_INTERFACE(ICompressSetInStreamSize2, 0x39)
{
  STDMETHOD(SetInStreamSize2)(UInt32 streamIndex, const UInt64 *inSize) PURE;
};
*/


/*
  ICompressFilter
  Filter() converts as most as possible bytes required for fast processing.
     Some filters have (smallest_fast_block).
     For example, (smallest_fast_block == 16) for AES CBC/CTR filters.
     If data stream is not finished, caller must call Filter() for larger block:
     where (size >= smallest_fast_block).
     if (size >= smallest_fast_block)
     {
       The filter can leave some bytes at the end of data without conversion:
       if there are data alignment reasons or speed reasons.
       The caller must read additional data from stream and call Filter() again.
     }
     If data stream was finished, caller can call Filter() for (size < smallest_fast_block)

     data : must be aligned for at least 16 bytes for some filters (AES)

     returns: (outSize):
       if (outSize == 0) : Filter have not converted anything.
           So the caller can stop processing, if data stream was finished.
       if (outSize <= size) : Filter have converted outSize bytes
       if (outSize >  size) : Filter have not converted anything.
           and it needs at least outSize bytes to convert one block
           (it's for crypto block algorithms).
*/

#define INTERFACE_ICompressFilter(x) \
  STDMETHOD(Init)() x; \
  STDMETHOD_(UInt32, Filter)(Byte *data, UInt32 size) x; \

CODER_INTERFACE(ICompressFilter, 0x40)
{
  INTERFACE_ICompressFilter(PURE);
};


CODER_INTERFACE(ICompressCodecsInfo, 0x60)
{
  STDMETHOD(GetNumMethods)(UInt32 *numMethods) PURE;
  STDMETHOD(GetProperty)(UInt32 index, PROPID propID, PROPVARIANT *value) PURE;
  STDMETHOD(CreateDecoder)(UInt32 index, const GUID *iid, void **coder) PURE;
  STDMETHOD(CreateEncoder)(UInt32 index, const GUID *iid, void **coder) PURE;
};

CODER_INTERFACE(ISetCompressCodecsInfo, 0x61)
{
  STDMETHOD(SetCompressCodecsInfo)(ICompressCodecsInfo *compressCodecsInfo) PURE;
};

CODER_INTERFACE(ICryptoProperties, 0x80)
{
  STDMETHOD(SetKey)(const Byte *data, UInt32 size) PURE;
  STDMETHOD(SetInitVector)(const Byte *data, UInt32 size) PURE;
};

/*
CODER_INTERFACE(ICryptoResetSalt, 0x88)
{
  STDMETHOD(ResetSalt)() PURE;
};
*/

CODER_INTERFACE(ICryptoResetInitVector, 0x8C)
{
  STDMETHOD(ResetInitVector)() PURE;

  /* Call ResetInitVector() only for encoding.
     Call ResetInitVector() before encoding and before WriteCoderProperties().
     Crypto encoder can create random IV in that function. */
};

CODER_INTERFACE(ICryptoSetPassword, 0x90)
{
  STDMETHOD(CryptoSetPassword)(const Byte *data, UInt32 size) PURE;
};

CODER_INTERFACE(ICryptoSetCRC, 0xA0)
{
  STDMETHOD(CryptoSetCRC)(UInt32 crc) PURE;
};


namespace NMethodPropID
{
  enum EEnum
  {
    kID,
    kName,
    kDecoder,
    kEncoder,
    kPackStreams,
    kUnpackStreams,
    kDescription,
    kDecoderIsAssigned,
    kEncoderIsAssigned,
    kDigestSize
  };
}


#define INTERFACE_IHasher(x) \
  STDMETHOD_(void, Init)() throw() x; \
  STDMETHOD_(void, Update)(const void *data, UInt32 size) throw() x; \
  STDMETHOD_(void, Final)(Byte *digest) throw() x; \
  STDMETHOD_(UInt32, GetDigestSize)() throw() x; \

CODER_INTERFACE(IHasher, 0xC0)
{
  INTERFACE_IHasher(PURE)
};

CODER_INTERFACE(IHashers, 0xC1)
{
  STDMETHOD_(UInt32, GetNumHashers)() PURE;
  STDMETHOD(GetHasherProp)(UInt32 index, PROPID propID, PROPVARIANT *value) PURE;
  STDMETHOD(CreateHasher)(UInt32 index, IHasher **hasher) PURE;
};

extern "C"
{
  typedef HRESULT (WINAPI *Func_GetNumberOfMethods)(UInt32 *numMethods);
  typedef HRESULT (WINAPI *Func_GetMethodProperty)(UInt32 index, PROPID propID, PROPVARIANT *value);
  typedef HRESULT (WINAPI *Func_CreateDecoder)(UInt32 index, const GUID *iid, void **outObject);
  typedef HRESULT (WINAPI *Func_CreateEncoder)(UInt32 index, const GUID *iid, void **outObject);

  typedef HRESULT (WINAPI *Func_GetHashers)(IHashers **hashers);
  
  typedef HRESULT (WINAPI *Func_SetCodecs)(ICompressCodecsInfo *compressCodecsInfo);
}

#endif

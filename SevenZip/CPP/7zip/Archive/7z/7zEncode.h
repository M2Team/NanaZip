// 7zEncode.h

#ifndef __7Z_ENCODE_H
#define __7Z_ENCODE_H

#include "7zCompressionMode.h"

#include "../Common/CoderMixer2.h"

#include "7zItem.h"

namespace NArchive {
namespace N7z {

class CMtEncMultiProgress:
  public ICompressProgressInfo,
  public CMyUnknownImp
{
  CMyComPtr<ICompressProgressInfo> _progress;
  #ifndef _7ZIP_ST
  NWindows::NSynchronization::CCriticalSection CriticalSection;
  #endif

public:
  UInt64 OutSize;

  CMtEncMultiProgress(): OutSize(0) {}

  void Init(ICompressProgressInfo *progress);

  void AddOutSize(UInt64 addOutSize)
  {
    #ifndef _7ZIP_ST
    NWindows::NSynchronization::CCriticalSectionLock lock(CriticalSection);
    #endif
    OutSize += addOutSize;
  }

  MY_UNKNOWN_IMP1(ICompressProgressInfo)

  STDMETHOD(SetRatioInfo)(const UInt64 *inSize, const UInt64 *outSize);
};

class CEncoder  MY_UNCOPYABLE
{
  #ifdef USE_MIXER_ST
    NCoderMixer2::CMixerST *_mixerST;
  #endif
  #ifdef USE_MIXER_MT
    NCoderMixer2::CMixerMT *_mixerMT;
  #endif
  
  NCoderMixer2::CMixer *_mixer;
  CMyComPtr<IUnknown> _mixerRef;

  CCompressionMethodMode _options;
  NCoderMixer2::CBindInfo _bindInfo;
  CRecordVector<CMethodId> _decompressionMethods;

  CRecordVector<UInt32> _SrcIn_to_DestOut;
  CRecordVector<UInt32> _SrcOut_to_DestIn;
  // CRecordVector<UInt32> _DestIn_to_SrcOut;
  CRecordVector<UInt32> _DestOut_to_SrcIn;

  void InitBindConv();
  void SetFolder(CFolder &folder);

  HRESULT CreateMixerCoder(DECL_EXTERNAL_CODECS_LOC_VARS
      const UInt64 *inSizeForReduce);

  bool _constructed;
public:

  CEncoder(const CCompressionMethodMode &options);
  ~CEncoder();
  HRESULT EncoderConstr();
  HRESULT Encode(
      DECL_EXTERNAL_CODECS_LOC_VARS
      ISequentialInStream *inStream,
      // const UInt64 *inStreamSize,
      const UInt64 *inSizeForReduce,
      CFolder &folderItem,
      CRecordVector<UInt64> &coderUnpackSizes,
      UInt64 &unpackSize,
      ISequentialOutStream *outStream,
      CRecordVector<UInt64> &packSizes,
      ICompressProgressInfo *compressProgress);
};

}}

#endif

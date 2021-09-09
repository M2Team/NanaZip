// PpmdEncoder.h

#ifndef __COMPRESS_PPMD_ENCODER_H
#define __COMPRESS_PPMD_ENCODER_H

#include "../../../C/Ppmd7.h"

#include "../../Common/MyCom.h"

#include "../ICoder.h"

#include "../Common/CWrappers.h"

namespace NCompress {
namespace NPpmd {

struct CEncProps
{
  UInt32 MemSize;
  UInt32 ReduceSize;
  int Order;
  
  CEncProps()
  {
    MemSize = (UInt32)(Int32)-1;
    ReduceSize = (UInt32)(Int32)-1;
    Order = -1;
  }
  void Normalize(int level);
};

class CEncoder :
  public ICompressCoder,
  public ICompressSetCoderProperties,
  public ICompressWriteCoderProperties,
  public CMyUnknownImp
{
  Byte *_inBuf;
  CByteOutBufWrap _outStream;
  CPpmd7 _ppmd;
  CEncProps _props;
public:
  MY_UNKNOWN_IMP3(
      ICompressCoder,
      ICompressSetCoderProperties,
      ICompressWriteCoderProperties)
  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
  STDMETHOD(SetCoderProperties)(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps);
  STDMETHOD(WriteCoderProperties)(ISequentialOutStream *outStream);
  CEncoder();
  ~CEncoder();
};

}}

#endif

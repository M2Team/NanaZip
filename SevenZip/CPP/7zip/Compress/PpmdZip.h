// PpmdZip.h

#ifndef __COMPRESS_PPMD_ZIP_H
#define __COMPRESS_PPMD_ZIP_H

#include "../../../C/Alloc.h"
#include "../../../C/Ppmd8.h"

#include "../../Common/MyCom.h"

#include "../ICoder.h"

#include "../Common/CWrappers.h"

namespace NCompress {
namespace NPpmdZip {

static const UInt32 kBufSize = (1 << 20);

struct CBuf
{
  Byte *Buf;
  
  CBuf(): Buf(NULL) {}
  ~CBuf() { ::MidFree(Buf); }
  bool Alloc()
  {
    if (!Buf)
      Buf = (Byte *)::MidAlloc(kBufSize);
    return (Buf != 0);
  }
};


class CDecoder :
  public ICompressCoder,
  public ICompressSetFinishMode,
  public ICompressGetInStreamProcessedSize,
  public CMyUnknownImp
{
  CByteInBufWrap _inStream;
  CBuf _outStream;
  CPpmd8 _ppmd;
  bool _fullFileMode;
public:
  MY_UNKNOWN_IMP2(
      ICompressSetFinishMode,
      ICompressGetInStreamProcessedSize)

  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
  STDMETHOD(SetFinishMode)(UInt32 finishMode);
  STDMETHOD(GetInStreamProcessedSize)(UInt64 *value);

  CDecoder(bool fullFileMode = true);
  ~CDecoder();
};


struct CEncProps
{
  UInt32 MemSizeMB;
  UInt32 ReduceSize;
  int Order;
  int Restor;
  
  CEncProps()
  {
    MemSizeMB = (UInt32)(Int32)-1;
    ReduceSize = (UInt32)(Int32)-1;
    Order = -1;
    Restor = -1;
  }
  void Normalize(int level);
};

class CEncoder :
  public ICompressCoder,
  public ICompressSetCoderProperties,
  public CMyUnknownImp
{
  CByteOutBufWrap _outStream;
  CBuf _inStream;
  CPpmd8 _ppmd;
  CEncProps _props;
public:
  MY_UNKNOWN_IMP1(ICompressSetCoderProperties)
  STDMETHOD(Code)(ISequentialInStream *inStream, ISequentialOutStream *outStream,
      const UInt64 *inSize, const UInt64 *outSize, ICompressProgressInfo *progress);
  STDMETHOD(SetCoderProperties)(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps);
  CEncoder();
  ~CEncoder();
};

}}

#endif

// HfsHandler.h

#ifndef ZIP7_INC_HFS_HANDLER_H
#define ZIP7_INC_HFS_HANDLER_H

#include "../../Windows/PropVariant.h"

#include "../Compress/LzfseDecoder.h"
#include "../Compress/ZlibDecoder.h"

namespace NArchive {
namespace NHfs {

static const UInt32 k_decmpfs_HeaderSize = 16;

struct CCompressHeader
{
  UInt64 UnpackSize;
  UInt32 Method;
  Byte DataPos;
  bool IsCorrect;
  bool IsSupported;
  bool IsResource;

  bool IsMethod_Compressed_Inline() const { return DataPos == k_decmpfs_HeaderSize; }
  bool IsMethod_Uncompressed_Inline()       const { return DataPos == k_decmpfs_HeaderSize + 1; }
  bool IsMethod_Resource() const { return IsResource; }

  void Parse(const Byte *p, size_t size);

  void Clear()
  {
    UnpackSize = 0;
    Method = 0;
    DataPos = 0;
    IsCorrect = false;
    IsSupported = false;
    IsResource = false;
  }

  CCompressHeader() { Clear(); }

  void MethodToProp(NWindows::NCOM::CPropVariant &prop) const;
};

void MethodsMaskToProp(UInt32 methodsMask, NWindows::NCOM::CPropVariant &prop);


class CDecoder
{
  NCompress::NZlib::CDecoder *_zlibDecoderSpec;
  CMyComPtr<ICompressCoder> _zlibDecoder;

  NCompress::NLzfse::CDecoder *_lzfseDecoderSpec;
  CMyComPtr<ICompressCoder> _lzfseDecoder;

  CByteBuffer _tableBuf;
  CByteBuffer _buf;

  HRESULT ExtractResourceFork_ZLIB(
      ISequentialInStream *inStream, ISequentialOutStream *realOutStream,
      UInt64 forkSize, UInt64 unpackSize,
      UInt64 progressStart, IArchiveExtractCallback *extractCallback);

  HRESULT ExtractResourceFork_LZFSE(
      ISequentialInStream *inStream, ISequentialOutStream *realOutStream,
      UInt64 forkSize, UInt64 unpackSize,
      UInt64 progressStart, IArchiveExtractCallback *extractCallback);

  HRESULT ExtractResourceFork_ZBM(
      ISequentialInStream *inStream, ISequentialOutStream *realOutStream,
      UInt64 forkSize, UInt64 unpackSize,
      UInt64 progressStart, IArchiveExtractCallback *extractCallback);

public:

  HRESULT Extract(
      ISequentialInStream *inStreamFork, ISequentialOutStream *realOutStream,
      UInt64 forkSize,
      const CCompressHeader &compressHeader,
      const CByteBuffer *data,
      UInt64 progressStart, IArchiveExtractCallback *extractCallback,
      int &opRes);

  CDecoder();
};

}}

#endif

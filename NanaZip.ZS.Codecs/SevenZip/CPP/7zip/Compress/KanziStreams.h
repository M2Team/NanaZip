// KanziStreams.h

#ifndef ZIP7_INC_COMPRESS_KANZI_STREAMS_H
#define ZIP7_INC_COMPRESS_KANZI_STREAMS_H

#include <streambuf>

#include "../ICoder.h"
#include "../Common/StreamUtils.h"

namespace NCompress {
namespace NKANZI {

struct CStreamError
{
  HRESULT Result;
  CStreamError(HRESULT result): Result(result) {}
};

class CInStreamBuf: public std::streambuf
{
  enum { kBufferSize = 1 << 15 };

  ISequentialInStream *_stream;
  UInt64 *_processed;
  HRESULT _result;
  char _buffer[kBufferSize];

public:
  CInStreamBuf(ISequentialInStream *stream, UInt64 *processed);
  HRESULT GetResult() const { return _result; }

protected:
  int_type underflow() Z7_override;
};

class COutStreamBuf: public std::streambuf
{
  ISequentialOutStream *_stream;
  ICompressProgressInfo *_progress;
  UInt64 *_processedIn;
  UInt64 *_processedOut;
  HRESULT _result;

public:
  COutStreamBuf(ISequentialOutStream *stream, ICompressProgressInfo *progress,
      UInt64 *processedIn, UInt64 *processedOut);
  HRESULT GetResult() const { return _result; }

protected:
  int_type overflow(int_type ch) Z7_override;
  std::streamsize xsputn(const char *s, std::streamsize count) Z7_override;
  int sync() Z7_override;
};

HRESULT WriteKanziOutput(ISequentialOutStream *stream, const void *data, size_t size,
    ICompressProgressInfo *progress, UInt64 *processedIn, UInt64 *processedOut);

}}

#endif

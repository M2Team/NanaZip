// KanziStreams.cpp

#include "StdAfx.h"

#include "KanziStreams.h"

#include <limits.h>

namespace NCompress {
namespace NKANZI {

CInStreamBuf::CInStreamBuf(ISequentialInStream *stream, UInt64 *processed):
  _stream(stream),
  _processed(processed),
  _result(S_OK)
{
  setg(_buffer, _buffer, _buffer);
}

CInStreamBuf::int_type CInStreamBuf::underflow()
{
  if (gptr() < egptr())
    return traits_type::to_int_type(*gptr());

  UInt32 processed = 0;
  const HRESULT res = _stream->Read(_buffer, kBufferSize, &processed);
  if (res != S_OK)
  {
    _result = res;
    throw CStreamError(res);
  }

  if (processed == 0)
    return traits_type::eof();

  *_processed += processed;
  setg(_buffer, _buffer, _buffer + processed);
  return traits_type::to_int_type(*gptr());
}

COutStreamBuf::COutStreamBuf(ISequentialOutStream *stream, ICompressProgressInfo *progress,
    UInt64 *processedIn, UInt64 *processedOut):
  _stream(stream),
  _progress(progress),
  _processedIn(processedIn),
  _processedOut(processedOut),
  _result(S_OK)
{
}

COutStreamBuf::int_type COutStreamBuf::overflow(int_type ch)
{
  if (traits_type::eq_int_type(ch, traits_type::eof()))
    return traits_type::not_eof(ch);

  const char c = traits_type::to_char_type(ch);
  return xsputn(&c, 1) == 1 ? ch : traits_type::eof();
}

std::streamsize COutStreamBuf::xsputn(const char *s, std::streamsize count)
{
  if (count <= 0)
    return 0;

  std::streamsize written = 0;
  while (written < count)
  {
    const std::streamsize remaining = count - written;
    const UInt32 todo = remaining > (std::streamsize)UINT_MAX ? (UInt32)UINT_MAX : (UInt32)remaining;
    UInt32 processed = 0;
    const HRESULT res = _stream->Write(s + written, todo, &processed);

    if (res != S_OK)
    {
      _result = res;
      throw CStreamError(res);
    }

    if (processed == 0)
    {
      _result = E_FAIL;
      throw CStreamError(E_FAIL);
    }

    written += processed;
    *_processedOut += processed;

    if (_progress)
    {
      const HRESULT progressRes = _progress->SetRatioInfo(_processedIn, _processedOut);
      if (progressRes != S_OK)
      {
        _result = progressRes;
        throw CStreamError(progressRes);
      }
    }
  }

  return written;
}

int COutStreamBuf::sync()
{
  return _result == S_OK ? 0 : -1;
}

HRESULT WriteKanziOutput(ISequentialOutStream *stream, const void *data, size_t size,
    ICompressProgressInfo *progress, UInt64 *processedIn, UInt64 *processedOut)
{
  const Byte *src = (const Byte *)data;
  size_t written = 0;

  while (written < size)
  {
    const size_t remaining = size - written;
    const UInt32 todo = remaining > (size_t)UINT_MAX ? (UInt32)UINT_MAX : (UInt32)remaining;
    UInt32 processed = 0;
    const HRESULT res = stream->Write(src + written, todo, &processed);

    if (res != S_OK)
      return res;

    if (processed == 0)
      return E_FAIL;

    written += processed;
    *processedOut += processed;

    if (progress)
    {
      RINOK(progress->SetRatioInfo(processedIn, processedOut))
    }
  }

  return S_OK;
}

}}

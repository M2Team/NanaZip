// KanziDecoder.cpp

#include "StdAfx.h"

#include "KanziDecoder.h"
#include "KanziStreams.h"

// **************** 7-Zip ZS Modification Start ****************
//#include "../../../C/kanzi/src/io/CompressedInputStream.hpp"
#include "../../../../Kanzi/io/CompressedInputStream.hpp"
// **************** 7-Zip ZS Modification End ****************

#include "../../Windows/System.h"

#include <istream>

namespace NCompress {
namespace NKANZI {

CDecoder::CDecoder():
  _processedIn(0),
  _processedInExact(0),
  _processedOut(0),
  _numThreads(NWindows::NSystem::GetNumberOfProcessors()),
  _finishMode(false)
{
  _props.Clear();
  if (_numThreads > kKanziMaxThreads)
    _numThreads = kKanziMaxThreads;
}

Z7_COM7F_IMF(CDecoder::SetDecoderProperties2(const Byte *props, UInt32 size))
{
  return ReadPropsFromBytes(_props, props, size) ? S_OK : E_NOTIMPL;
}

Z7_COM7F_IMF(CDecoder::SetFinishMode(UInt32 finishMode))
{
  _finishMode = (finishMode != 0);
  return S_OK;
}

Z7_COM7F_IMF(CDecoder::GetInStreamProcessedSize(UInt64 *value))
{
  if (!value)
    return E_INVALIDARG;
  *value = _processedInExact;
  return S_OK;
}

Z7_COM7F_IMF(CDecoder::SetNumberOfThreads(UInt32 numThreads))
{
  if (numThreads < 1)
    numThreads = 1;
  if (numThreads > kKanziMaxThreads)
    numThreads = kKanziMaxThreads;
  _numThreads = numThreads;
  return S_OK;
}

#if defined(_M_IX86) || defined(__i386__)
static UInt32 GetEffectiveDecodeThreads(const CProps &props, UInt32 numThreads)
{
  if (props.Level >= 9 && numThreads > 4)
    return 4;
  if (props.Level == 8 && numThreads > 32)
    return 32;
  return numThreads;
}
#else
static UInt32 GetEffectiveDecodeThreads(const CProps &, UInt32 numThreads)
{
  return numThreads;
}
#endif

Z7_COM7F_IMF(CDecoder::Code(ISequentialInStream *inStream,
    ISequentialOutStream *outStream, const UInt64 *inSize,
    const UInt64 *outSize, ICompressProgressInfo *progress))
{
  _processedIn = 0;
  _processedInExact = 0;
  _processedOut = 0;

  CInStreamBuf inBuf(inStream, &_processedIn);
  std::istream input(&inBuf);

  try
  {
    const UInt32 numThreads = GetEffectiveDecodeThreads(_props, _numThreads);
    kanzi::CompressedInputStream cis(input, (int)numThreads);
    char buffer[1 << 15];
    bool outputLimitReached = outSize && *outSize == 0;

    for (;;)
    {
      if (outputLimitReached)
      {
        if (_finishMode)
        {
          const int probe = cis.peek();
          if (probe != std::char_traits<char>::eof())
            return S_FALSE;
        }
        break;
      }

      size_t request = sizeof(buffer);
      if (outSize)
      {
        if (_processedOut >= *outSize)
          break;
        const UInt64 rem = *outSize - _processedOut;
        if (rem < request)
          request = (size_t)rem;
      }

      cis.read(buffer, (std::streamsize)request);
      const std::streamsize read = cis.gcount();
      // CInStreamBuf counts bytes fetched into its buffer; getRead() is the
      // Kanzi bit cursor and therefore excludes both layers of read-ahead.
      _processedInExact = cis.getRead();
      if (read > 0)
      {
        RINOK(WriteKanziOutput(outStream, buffer, (size_t)read,
            progress, &_processedIn, &_processedOut))
      }
      else if (cis)
        return S_FALSE;

      if (outSize && _processedOut >= *outSize)
        outputLimitReached = true;
      if (!cis)
        break;
    }

    cis.close();
    _processedInExact = cis.getRead();

    if (_finishMode && outSize && _processedOut != *outSize)
      return S_FALSE;

    if (_finishMode && inSize && _processedInExact != *inSize)
      return S_FALSE;
  }
  catch (const CStreamError &e)
  {
    return e.Result;
  }
  catch (...)
  {
    return E_FAIL;
  }

  return inBuf.GetResult();
}

}}

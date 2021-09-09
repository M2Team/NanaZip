// FindSignature.cpp

#include "StdAfx.h"

#include <string.h>

#include "../../../Common/MyBuffer.h"

#include "../../Common/StreamUtils.h"

#include "FindSignature.h"

HRESULT FindSignatureInStream(ISequentialInStream *stream,
    const Byte *signature, unsigned signatureSize,
    const UInt64 *limit, UInt64 &resPos)
{
  resPos = 0;
  CByteBuffer byteBuffer2(signatureSize);
  RINOK(ReadStream_FALSE(stream, byteBuffer2, signatureSize));

  if (memcmp(byteBuffer2, signature, signatureSize) == 0)
    return S_OK;

  const UInt32 kBufferSize = (1 << 16);
  CByteBuffer byteBuffer(kBufferSize);
  Byte *buffer = byteBuffer;
  UInt32 numPrevBytes = signatureSize - 1;
  memcpy(buffer, (const Byte *)byteBuffer2 + 1, numPrevBytes);
  resPos = 1;
  for (;;)
  {
    if (limit != NULL)
      if (resPos > *limit)
        return S_FALSE;
    do
    {
      UInt32 numReadBytes = kBufferSize - numPrevBytes;
      UInt32 processedSize;
      RINOK(stream->Read(buffer + numPrevBytes, numReadBytes, &processedSize));
      numPrevBytes += processedSize;
      if (processedSize == 0)
        return S_FALSE;
    }
    while (numPrevBytes < signatureSize);
    UInt32 numTests = numPrevBytes - signatureSize + 1;
    for (UInt32 pos = 0; pos < numTests; pos++)
    {
      Byte b = signature[0];
      for (; buffer[pos] != b && pos < numTests; pos++);
      if (pos == numTests)
        break;
      if (memcmp(buffer + pos, signature, signatureSize) == 0)
      {
        resPos += pos;
        return S_OK;
      }
    }
    resPos += numTests;
    numPrevBytes -= numTests;
    memmove(buffer, buffer + numTests, numPrevBytes);
  }
}

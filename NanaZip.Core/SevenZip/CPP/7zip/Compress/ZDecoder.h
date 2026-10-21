// ZDecoder.h

#ifndef ZIP7_INC_COMPRESS_Z_DECODER_H
#define ZIP7_INC_COMPRESS_Z_DECODER_H

#include "../ICoder.h"

namespace NCompress {
namespace NZ {

/*
  There is no end_of_payload_marker in Z-stream.
  The Z-decoder stops decoding upon reaching the end of the input stream
  or upon encountering an error.
*/

HRESULT Decode(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    ICompressProgressInfo *progress) throw();

/*
CheckStream() function:
in:
  if (size > real_size_of_zstream), CheckStream() may return (false),
     because there is no end_of_payload_marker in Z-stream.
     
return value:
  false : for any of the following cases:
    - (size < 3)
    - there is no correct Z-signature in Z-header (first 3 bytes)
    - unsupported properties in Z-header
    - a decoding error was detected in (data)
  
  true : size >= 3 and data[0 ... (size-1)] is part of a valid Z-stream,
         and no errors have been detected.
*/
const unsigned kRecommendedCheckSize = 64; // (size >= 64) ensures a low probability of false detection
bool CheckStream(const Byte *data, size_t size) throw();

}}

#endif

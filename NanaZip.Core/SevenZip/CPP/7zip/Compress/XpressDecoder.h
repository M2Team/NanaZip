// XpressDecoder.h

#ifndef ZIP7_INC_XPRESS_DECODER_H
#define ZIP7_INC_XPRESS_DECODER_H

#include "../../Common/MyWindows.h"

namespace NCompress {
namespace NXpress {

HRESULT Decode(const Byte *in, size_t inSize, Byte *out, size_t outSize);

}}

#endif

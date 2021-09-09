// XpressDecoder.h

#ifndef __XPRESS_DECODER_H
#define __XPRESS_DECODER_H

namespace NCompress {
namespace NXpress {

HRESULT Decode(const Byte *in, size_t inSize, Byte *out, size_t outSize);

}}

#endif

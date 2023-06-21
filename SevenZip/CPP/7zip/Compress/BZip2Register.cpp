// BZip2Register.cpp

#include "../../../../ThirdParty/LZMA/CPP/7zip/Compress/StdAfx.h"

#include "../../../../ThirdParty/LZMA/CPP/7zip/Common/RegisterCodec.h"

#include "BZip2Decoder.h"
#if !defined(EXTRACT_ONLY) && !defined(BZIP2_EXTRACT_ONLY)
#include "BZip2Encoder.h"
#endif

namespace NCompress {
namespace NBZip2 {

REGISTER_CODEC_CREATE(CreateDec, CDecoder)

#if !defined(EXTRACT_ONLY) && !defined(BZIP2_EXTRACT_ONLY)
REGISTER_CODEC_CREATE(CreateEnc, CEncoder)
#else
#define CreateEnc NULL
#endif

REGISTER_CODEC_2(BZip2, CreateDec, CreateEnc, 0x40202, "BZip2")

}}

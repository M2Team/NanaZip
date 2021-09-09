// RarCodecsRegister.cpp

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "Rar1Decoder.h"
#include "Rar2Decoder.h"
#include "Rar3Decoder.h"
#include "Rar5Decoder.h"

namespace NCompress {

#define CREATE_CODEC(x) REGISTER_CODEC_CREATE(CreateCodec ## x, NRar ## x::CDecoder())

CREATE_CODEC(1)
CREATE_CODEC(2)
CREATE_CODEC(3)
CREATE_CODEC(5)

#define RAR_CODEC(x, name) { CreateCodec ## x, NULL, 0x40300 + x, "Rar" name, 1, false }

REGISTER_CODECS_VAR
{
  RAR_CODEC(1, "1"),
  RAR_CODEC(2, "2"),
  RAR_CODEC(3, "3"),
  RAR_CODEC(5, "5"),
};

REGISTER_CODECS(Rar)

}

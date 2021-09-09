// BcjRegister.cpp

#include "StdAfx.h"

#include "../Common/RegisterCodec.h"

#include "BcjCoder.h"

namespace NCompress {
namespace NBcj {

REGISTER_FILTER_E(BCJ,
    CCoder(false),
    CCoder(true),
    0x3030103, "BCJ")

}}

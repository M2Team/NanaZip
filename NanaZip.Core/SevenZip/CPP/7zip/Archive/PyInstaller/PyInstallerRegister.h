#include "../../Common/RegisterArc.h"

#define PYINST_SIGNATURE { 'M', 'E', 'I', 0x0C, 0x0B, 0x0A, 0x0B, 0x0E }

const unsigned kSignatureSize = 8;
const Byte kSignature[kSignatureSize] = PYINST_SIGNATURE;

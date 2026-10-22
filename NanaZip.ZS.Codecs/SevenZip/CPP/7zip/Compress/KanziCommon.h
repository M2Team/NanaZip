// KanziCommon.h

#ifndef ZIP7_INC_COMPRESS_KANZI_COMMON_H
#define ZIP7_INC_COMPRESS_KANZI_COMMON_H

#include "../../Common/MyTypes.h"

namespace NCompress {
namespace NKANZI {

const Byte kKanziVersionMajor = 2;
const Byte kKanziVersionMinor = 6;
const Byte kKanziDefaultLevel = 3;
const UInt32 kKanziDefaultBlockSize = 4u << 20;
const UInt32 kKanziMinBlockSize = 1u << 10;
const UInt32 kKanziMaxBlockSize = 1u << 30;
const UInt32 kKanziMaxThreads = 64;

struct CProps
{
  Byte VersionMajor;
  Byte VersionMinor;
  Byte Level;
  Byte ChecksumBits;
  UInt32 BlockSize;

  void Clear()
  {
    VersionMajor = kKanziVersionMajor;
    VersionMinor = kKanziVersionMinor;
    Level = kKanziDefaultLevel;
    ChecksumBits = 0;
    BlockSize = kKanziDefaultBlockSize;
  }

  CProps() { Clear(); }
};

UInt32 GetDefaultBlockSize(Byte level);
void GetLevelParams(Byte level, const char *&transform, const char *&entropy);
void NormalizeProps(CProps &props);
void WritePropsToBytes(const CProps &props, Byte dest[8]);
bool ReadPropsFromBytes(CProps &props, const Byte *data, UInt32 size);

}}

#endif

// KanziCommon.cpp

#include "StdAfx.h"

#include "KanziCommon.h"

namespace NCompress {
namespace NKANZI {

UInt32 GetDefaultBlockSize(Byte level)
{
  switch (level)
  {
    case 6: return 8u << 20;
    case 7:
    case 8: return 16u << 20;
    case 9: return 32u << 20;
    default: return kKanziDefaultBlockSize;
  }
}

void GetLevelParams(Byte level, const char *&transform, const char *&entropy)
{
  switch (level)
  {
    case 0:
      transform = "NONE";
      entropy = "NONE";
      break;
    case 1:
      transform = "LZX";
      entropy = "NONE";
      break;
    case 2:
      transform = "DNA+LZ";
      entropy = "HUFFMAN";
      break;
    case 3:
      transform = "TEXT+UTF+PACK+MM+LZX";
      entropy = "HUFFMAN";
      break;
    case 4:
      transform = "TEXT+UTF+EXE+PACK+MM+ROLZ";
      entropy = "NONE";
      break;
    case 5:
      transform = "TEXT+UTF+BWT+RANK+ZRLT";
      entropy = "ANS0";
      break;
    case 6:
      transform = "TEXT+UTF+BWT+SRT+ZRLT";
      entropy = "FPAQ";
      break;
    case 7:
      transform = "LZP+TEXT+UTF+BWT+LZP";
      entropy = "CM";
      break;
    case 8:
      transform = "EXE+RLT+TEXT+UTF+DNA";
      entropy = "TPAQ";
      break;
    default:
      transform = "EXE+RLT+TEXT+UTF+DNA";
      entropy = "TPAQX";
      break;
  }
}

void NormalizeProps(CProps &props)
{
  if (props.Level > 9)
    props.Level = 9;

  if (props.ChecksumBits != 0 && props.ChecksumBits != 32 && props.ChecksumBits != 64)
    props.ChecksumBits = 0;

  if (props.BlockSize < kKanziMinBlockSize)
    props.BlockSize = kKanziMinBlockSize;
  else if (props.BlockSize > kKanziMaxBlockSize)
    props.BlockSize = kKanziMaxBlockSize;

  props.BlockSize = (props.BlockSize + 15) & ~(UInt32)15;
}

void WritePropsToBytes(const CProps &props, Byte dest[8])
{
  dest[0] = props.VersionMajor;
  dest[1] = props.VersionMinor;
  dest[2] = props.Level;
  dest[3] = props.ChecksumBits;
  dest[4] = (Byte)(props.BlockSize);
  dest[5] = (Byte)(props.BlockSize >> 8);
  dest[6] = (Byte)(props.BlockSize >> 16);
  dest[7] = (Byte)(props.BlockSize >> 24);
}

bool ReadPropsFromBytes(CProps &props, const Byte *data, UInt32 size)
{
  props.Clear();

  if (data == nullptr || size != 8)
    return false;

  props.VersionMajor = data[0];
  props.VersionMinor = data[1];
  props.Level = data[2];
  props.ChecksumBits = data[3];
  props.BlockSize =
      (UInt32)data[4]
    | ((UInt32)data[5] << 8)
    | ((UInt32)data[6] << 16)
    | ((UInt32)data[7] << 24);
  // Reject malformed properties instead of silently normalizing them.
  if (props.VersionMajor != kKanziVersionMajor
      || props.VersionMinor > kKanziVersionMinor
      || props.Level > 9
      || (props.ChecksumBits != 0 && props.ChecksumBits != 32 && props.ChecksumBits != 64)
      || props.BlockSize < kKanziMinBlockSize
      || props.BlockSize > kKanziMaxBlockSize
      || (props.BlockSize & 15) != 0)
    return false;

  return true;
}

}}

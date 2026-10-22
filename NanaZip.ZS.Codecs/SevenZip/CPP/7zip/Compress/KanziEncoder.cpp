// KanziEncoder.cpp

#include "StdAfx.h"

#include "KanziEncoder.h"
#include "KanziStreams.h"

// **************** 7-Zip ZS Modification Start ****************
//#include "../../../C/kanzi/src/io/CompressedOutputStream.hpp"
#include "../../../../Kanzi/io/CompressedOutputStream.hpp"
// **************** 7-Zip ZS Modification End ****************

#include "../../Windows/System.h"

#include <istream>
#include <ostream>

#ifndef Z7_EXTRACT_ONLY
namespace NCompress {
namespace NKANZI {

CEncoder::CEncoder():
  _processedIn(0),
  _processedOut(0),
  _inputSize(0),
  _numThreads(NWindows::NSystem::GetNumberOfProcessors()),
  _blockSizeWasSet(false)
{
  _props.Clear();
  if (_numThreads > kKanziMaxThreads)
    _numThreads = kKanziMaxThreads;
}

static HRESULT SetBlockSize(const PROPVARIANT &prop, UInt32 &blockSize)
{
  UInt64 v;
  if (prop.vt == VT_UI4)
    v = prop.ulVal;
  else if (prop.vt == VT_UI8)
    v = prop.uhVal.QuadPart;
  else
    return E_INVALIDARG;

  if (v > kKanziMaxBlockSize)
    v = kKanziMaxBlockSize;
  blockSize = (UInt32)v;
  return S_OK;
}

static HRESULT SetChecksum(const PROPVARIANT &prop, Byte &checksumBits)
{
  if (prop.vt != VT_UI4)
    return E_INVALIDARG;

  switch (prop.ulVal)
  {
    case 0: checksumBits = 0; return S_OK;
    case 4:
    case 32: checksumBits = 32; return S_OK;
    case 8:
    case 64: checksumBits = 64; return S_OK;
    default: return E_INVALIDARG;
  }
}

static UInt32 GetEffectiveBlockSize(const CProps &props, bool blockSizeWasSet)
{
  UInt32 blockSize = blockSizeWasSet ? props.BlockSize : GetDefaultBlockSize(props.Level);
  if (blockSize < kKanziMinBlockSize)
    blockSize = kKanziMinBlockSize;
  blockSize = (blockSize + 15) & ~(UInt32)15;
  return blockSize;
}

Z7_COM7F_IMF(CEncoder::SetCoderProperties(const PROPID *propIDs,
    const PROPVARIANT *coderProps, UInt32 numProps))
{
  _props.Clear();
  _inputSize = 0;
  _blockSizeWasSet = false;

  for (UInt32 i = 0; i < numProps; i++)
  {
    const PROPVARIANT &prop = coderProps[i];
    switch (propIDs[i])
    {
      case NCoderPropID::kLevel:
        if (prop.vt != VT_UI4 || prop.ulVal > 9)
          return E_INVALIDARG;
        _props.Level = (Byte)prop.ulVal;
        break;
      case NCoderPropID::kNumThreads:
        if (prop.vt != VT_UI4)
          return E_INVALIDARG;
        RINOK(SetNumberOfThreads(prop.ulVal))
        break;
      case NCoderPropID::kBlockSize:
      case NCoderPropID::kBlockSize2:
        RINOK(SetBlockSize(prop, _props.BlockSize))
        _blockSizeWasSet = true;
        break;
      case NCoderPropID::kCheckSize:
        RINOK(SetChecksum(prop, _props.ChecksumBits))
        break;
      default:
        break;
    }
  }

  NormalizeProps(_props);
  return S_OK;
}

Z7_COM7F_IMF(CEncoder::SetCoderPropertiesOpt(const PROPID *propIDs,
    const PROPVARIANT *coderProps, UInt32 numProps))
{
  for (UInt32 i = 0; i < numProps; i++)
  {
    if (propIDs[i] == NCoderPropID::kExpectedDataSize)
    {
      const PROPVARIANT &prop = coderProps[i];
      if (prop.vt == VT_UI8)
        _inputSize = prop.uhVal.QuadPart;
    }
  }
  return S_OK;
}

Z7_COM7F_IMF(CEncoder::WriteCoderProperties(ISequentialOutStream *outStream))
{
  CProps effectiveProps = _props;
  effectiveProps.BlockSize = GetEffectiveBlockSize(_props, _blockSizeWasSet);
  Byte propsBytes[8];
  WritePropsToBytes(effectiveProps, propsBytes);
  return WriteStream(outStream, propsBytes, sizeof(propsBytes));
}

Z7_COM7F_IMF(CEncoder::SetNumberOfThreads(UInt32 numThreads))
{
  if (numThreads < 1)
    numThreads = 1;
  if (numThreads > kKanziMaxThreads)
    numThreads = kKanziMaxThreads;
  _numThreads = numThreads;
  return S_OK;
}

Z7_COM7F_IMF(CEncoder::Code(ISequentialInStream *inStream,
    ISequentialOutStream *outStream, const UInt64 *inSize,
    const UInt64 * /* outSize */, ICompressProgressInfo *progress))
{
  _processedIn = 0;
  _processedOut = 0;

  UInt64 inputSize = _inputSize;
  _inputSize = 0;
  if (inSize)
    inputSize = *inSize;

  const char *transform;
  const char *entropy;
  GetLevelParams(_props.Level, transform, entropy);
  UInt32 blockSize = GetEffectiveBlockSize(_props, _blockSizeWasSet);

  CInStreamBuf inBuf(inStream, &_processedIn);
  COutStreamBuf outBuf(outStream, progress, &_processedIn, &_processedOut);
  std::istream input(&inBuf);
  std::ostream output(&outBuf);

  try
  {
    kanzi::Context ctx;
    ctx.putInt("jobs", (int)_numThreads);
    ctx.putString("entropy", entropy);
    ctx.putString("transform", transform);
    ctx.putInt("blockSize", (int)blockSize);
    ctx.putInt("checksum", (int)_props.ChecksumBits);
    if (inputSize != 0 && inputSize < ((UInt64)1 << 48))
      ctx.putLong("fileSize", (kanzi::int64)inputSize);

    kanzi::CompressedOutputStream cos(output, ctx, false);
    char buffer[1 << 15];
    for (;;)
    {
      input.read(buffer, sizeof(buffer));
      const std::streamsize read = input.gcount();
      if (read > 0)
        cos.write(buffer, read);
      if (!input)
        break;
    }
    cos.close();
  }
  catch (const CStreamError &e)
  {
    return e.Result;
  }
  catch (...)
  {
    return E_FAIL;
  }

  return outBuf.GetResult();
}

}}
#endif

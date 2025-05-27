// Lzma2Encoder.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"

#include "../Common/CWrappers.h"
#include "../Common/StreamUtils.h"
// **************** 7-Zip ZS Modification Start ****************
#include "../../Windows/System.h"
// **************** 7-Zip ZS Modification End ****************

#include "Lzma2Encoder.h"

namespace NCompress {

namespace NLzma {

HRESULT SetLzmaProp(PROPID propID, const PROPVARIANT &prop, CLzmaEncProps &ep);

}

namespace NLzma2 {

CEncoder::CEncoder()
{
  _encoder = NULL;
  _encoder = Lzma2Enc_Create(&g_AlignedAlloc, &g_BigAlloc);
  if (!_encoder)
    throw 1;
}

CEncoder::~CEncoder()
{
  if (_encoder)
    Lzma2Enc_Destroy(_encoder);
}


HRESULT SetLzma2Prop(PROPID propID, const PROPVARIANT &prop, CLzma2EncProps &lzma2Props);
HRESULT SetLzma2Prop(PROPID propID, const PROPVARIANT &prop, CLzma2EncProps &lzma2Props)
{
  switch (propID)
  {
    case NCoderPropID::kBlockSize:
    {
      if (prop.vt == VT_UI4)
        lzma2Props.blockSize = prop.ulVal;
      else if (prop.vt == VT_UI8)
        lzma2Props.blockSize = prop.uhVal.QuadPart;
      else
        return E_INVALIDARG;
      break;
    }
    case NCoderPropID::kNumThreads:
    // **************** 7-Zip ZS Modification Start ****************
    {
    // **************** 7-Zip ZS Modification End ****************
      if (prop.vt != VT_UI4)
        return E_INVALIDARG;
      // **************** 7-Zip ZS Modification Start ****************
      // lzma2Props.numTotalThreads = (int)(prop.ulVal);
      int v = (int)prop.ulVal;
      lzma2Props.numTotalThreads = v >= 0 ? (v ? v : 1) : NWindows::NSystem::GetNumberOfProcessors();;
      // **************** 7-Zip ZS Modification End ****************
      break;
    // **************** 7-Zip ZS Modification Start ****************
    }
    // **************** 7-Zip ZS Modification End ****************
    default:
      RINOK(NLzma::SetLzmaProp(propID, prop, lzma2Props.lzmaProps))
  }
  return S_OK;
}


Z7_COM7F_IMF(CEncoder::SetCoderProperties(const PROPID *propIDs,
    const PROPVARIANT *coderProps, UInt32 numProps))
{
  CLzma2EncProps lzma2Props;
  Lzma2EncProps_Init(&lzma2Props);

  for (UInt32 i = 0; i < numProps; i++)
  {
    RINOK(SetLzma2Prop(propIDs[i], coderProps[i], lzma2Props))
  }
  return SResToHRESULT(Lzma2Enc_SetProps(_encoder, &lzma2Props));
}


Z7_COM7F_IMF(CEncoder::SetCoderPropertiesOpt(const PROPID *propIDs,
    const PROPVARIANT *coderProps, UInt32 numProps))
{
  for (UInt32 i = 0; i < numProps; i++)
  {
    const PROPVARIANT &prop = coderProps[i];
    const PROPID propID = propIDs[i];
    if (propID == NCoderPropID::kExpectedDataSize)
      if (prop.vt == VT_UI8)
        Lzma2Enc_SetDataSize(_encoder, prop.uhVal.QuadPart);
  }
  return S_OK;
}


Z7_COM7F_IMF(CEncoder::WriteCoderProperties(ISequentialOutStream *outStream))
{
  const Byte prop = Lzma2Enc_WriteProperties(_encoder);
  return WriteStream(outStream, &prop, 1);
}


#define RET_IF_WRAP_ERROR(wrapRes, sRes, sResErrorCode) \
  if (wrapRes != S_OK /* && (sRes == SZ_OK || sRes == sResErrorCode) */) return wrapRes;

Z7_COM7F_IMF(CEncoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 * /* outSize */, ICompressProgressInfo *progress))
{
  CSeqInStreamWrap inWrap;
  CSeqOutStreamWrap outWrap;
  CCompressProgressWrap progressWrap;

  inWrap.Init(inStream);
  outWrap.Init(outStream);
  progressWrap.Init(progress);

  SRes res = Lzma2Enc_Encode2(_encoder,
      &outWrap.vt, NULL, NULL,
      &inWrap.vt, NULL, 0,
      progress ? &progressWrap.vt : NULL);

  RET_IF_WRAP_ERROR(inWrap.Res, res, SZ_ERROR_READ)
  RET_IF_WRAP_ERROR(outWrap.Res, res, SZ_ERROR_WRITE)
  RET_IF_WRAP_ERROR(progressWrap.Res, res, SZ_ERROR_PROGRESS)

  return SResToHRESULT(res);
}
  
}}

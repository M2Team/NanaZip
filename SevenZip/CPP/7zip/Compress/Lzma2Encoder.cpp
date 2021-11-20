// Lzma2Encoder.cpp

#include "StdAfx.h"

#include "../../../C/Alloc.h"

#include "../../../C/fast-lzma2/fl2_errors.h"

#include "../Common/CWrappers.h"
#include "../Common/StreamUtils.h"

#include "Lzma2Encoder.h"
#pragma warning(disable : 4127)

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
      if (prop.vt != VT_UI4)
        return E_INVALIDARG;
      lzma2Props.numTotalThreads = (int)(prop.ulVal);
      break;
    default:
      RINOK(NLzma::SetLzmaProp(propID, prop, lzma2Props.lzmaProps));
  }
  return S_OK;
}


STDMETHODIMP CEncoder::SetCoderProperties(const PROPID *propIDs,
    const PROPVARIANT *coderProps, UInt32 numProps)
{
  CLzma2EncProps lzma2Props;
  Lzma2EncProps_Init(&lzma2Props);

  for (UInt32 i = 0; i < numProps; i++)
  {
    RINOK(SetLzma2Prop(propIDs[i], coderProps[i], lzma2Props));
  }
  return SResToHRESULT(Lzma2Enc_SetProps(_encoder, &lzma2Props));
}


STDMETHODIMP CEncoder::SetCoderPropertiesOpt(const PROPID *propIDs,
    const PROPVARIANT *coderProps, UInt32 numProps)
{
  for (UInt32 i = 0; i < numProps; i++)
  {
    const PROPVARIANT &prop = coderProps[i];
    PROPID propID = propIDs[i];
    if (propID == NCoderPropID::kExpectedDataSize)
      if (prop.vt == VT_UI8)
        Lzma2Enc_SetDataSize(_encoder, prop.uhVal.QuadPart);
  }
  return S_OK;
}


STDMETHODIMP CEncoder::WriteCoderProperties(ISequentialOutStream *outStream)
{
  Byte prop = Lzma2Enc_WriteProperties(_encoder);
  return WriteStream(outStream, &prop, 1);
}


#define RET_IF_WRAP_ERROR(wrapRes, sRes, sResErrorCode) \
  if (wrapRes != S_OK /* && (sRes == SZ_OK || sRes == sResErrorCode) */) return wrapRes;

STDMETHODIMP CEncoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
    const UInt64 * /* inSize */, const UInt64 * /* outSize */, ICompressProgressInfo *progress)
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


// Fast LZMA2 encoder


static HRESULT TranslateError(size_t res)
{
  if (FL2_getErrorCode(res) == FL2_error_memory_allocation)
    return E_OUTOFMEMORY;
  return S_FALSE;
}

#define CHECK_S(f_) do { \
  size_t r_ = f_; \
  if (FL2_isError(r_)) \
    return TranslateError(r_); \
} while (false)

#define CHECK_H(f_) do { \
  HRESULT r_ = f_; \
  if (r_ != S_OK) \
    return r_; \
} while (false)

#define CHECK_P(f) if (FL2_isError(f)) return E_INVALIDARG;  /* check and convert error code */

#define MIN_BLOCK_SIZE (1U << 20)
#define MAX_BLOCK_SIZE (1U << 28)

CFastEncoder::FastLzma2::FastLzma2()
  : fcs(NULL),
  dict_pos(0)
{
}

CFastEncoder::FastLzma2::~FastLzma2()
{
  FL2_freeCCtx(fcs);
}

HRESULT CFastEncoder::FastLzma2::SetCoderProperties(const PROPID *propIDs, const PROPVARIANT *coderProps, UInt32 numProps)
{
  CLzma2EncProps lzma2Props;
  Lzma2EncProps_Init(&lzma2Props);

  for (UInt32 i = 0; i < numProps; i++)
  {
    RINOK(SetLzma2Prop(propIDs[i], coderProps[i], lzma2Props));
  }
  if (fcs == NULL) {
    fcs = FL2_createCStreamMt(lzma2Props.numTotalThreads, 1);
    if (fcs == NULL)
      return E_OUTOFMEMORY;
  }
  if (lzma2Props.lzmaProps.algo > 2) {
    if (lzma2Props.lzmaProps.algo > 3)
      return E_INVALIDARG;
    lzma2Props.lzmaProps.algo = 2;
    FL2_CCtx_setParameter(fcs, FL2_p_highCompression, 1);
    FL2_CCtx_setParameter(fcs, FL2_p_compressionLevel, lzma2Props.lzmaProps.level);
  }
  else {
    FL2_CCtx_setParameter(fcs, FL2_p_compressionLevel, lzma2Props.lzmaProps.level);
  }
  size_t dictSize = lzma2Props.lzmaProps.dictSize;
  if (!dictSize) {
    dictSize = (UInt32)FL2_CCtx_getParameter(fcs, FL2_p_dictionarySize);
  }
  UInt64 reduceSize = lzma2Props.lzmaProps.reduceSize;
  reduceSize += (reduceSize < (UInt64)-1); /* prevent extra buffer shift after read */
  dictSize = (UInt32)min(dictSize, reduceSize);
  dictSize = max(dictSize, FL2_DICTSIZE_MIN);
  CHECK_P(FL2_CCtx_setParameter(fcs, FL2_p_dictionarySize, dictSize));
  if (lzma2Props.lzmaProps.algo >= 0) {
    CHECK_P(FL2_CCtx_setParameter(fcs, FL2_p_strategy, (unsigned)lzma2Props.lzmaProps.algo));
  }
  if (lzma2Props.lzmaProps.fb > 0)
    CHECK_P(FL2_CCtx_setParameter(fcs, FL2_p_fastLength, lzma2Props.lzmaProps.fb));
  if (lzma2Props.lzmaProps.mc > 0)
    CHECK_P(FL2_CCtx_setParameter(fcs, FL2_p_hybridCycles, lzma2Props.lzmaProps.mc));
  if (lzma2Props.lzmaProps.lc >= 0)
    CHECK_P(FL2_CCtx_setParameter(fcs, FL2_p_literalCtxBits, lzma2Props.lzmaProps.lc));
  if (lzma2Props.lzmaProps.lp >= 0)
    CHECK_P(FL2_CCtx_setParameter(fcs, FL2_p_literalPosBits, lzma2Props.lzmaProps.lp));
  if (lzma2Props.lzmaProps.pb >= 0)
    CHECK_P(FL2_CCtx_setParameter(fcs, FL2_p_posBits, lzma2Props.lzmaProps.pb));
  if (lzma2Props.blockSize == 0)
    lzma2Props.blockSize = min(max(MIN_BLOCK_SIZE, dictSize * 4U), MAX_BLOCK_SIZE);
  else if (lzma2Props.blockSize == LZMA2_ENC_PROPS__BLOCK_SIZE__SOLID)
    lzma2Props.blockSize = 0;
  unsigned r = 0;
  if (lzma2Props.blockSize != 0) {
    r = 1;
    // Do not exceed the block size. TODO: the lib should support setting a value instead of a multiplier.
    while (r < FL2_RESET_INTERVAL_MAX && (r + 1) * (UInt64)dictSize <= lzma2Props.blockSize)
      ++r;
  }
  CHECK_P(FL2_CCtx_setParameter(fcs, FL2_p_resetInterval, r));
  FL2_CCtx_setParameter(fcs, FL2_p_omitProperties, 1);
  FL2_setCStreamTimeout(fcs, 500);
  return S_OK;
}

size_t CFastEncoder::FastLzma2::GetDictSize() const
{
  return FL2_CCtx_getParameter(fcs, FL2_p_dictionarySize);
}

HRESULT CFastEncoder::FastLzma2::Begin()
{
  CHECK_S(FL2_initCStream(fcs, 0));
  CHECK_S(FL2_getDictionaryBuffer(fcs, &dict));
  dict_pos = 0;
  return S_OK;
}

BYTE* CFastEncoder::FastLzma2::GetAvailableBuffer(unsigned long& size)
{
  size = static_cast<unsigned long>(dict.size - dict_pos);
  return reinterpret_cast<BYTE*>(dict.dst) + dict_pos;
}

HRESULT CFastEncoder::FastLzma2::WaitAndReport(size_t& res, ICompressProgressInfo *progress)
{
  while (FL2_isTimedOut(res)) {
    if (!UpdateProgress(progress))
      return S_FALSE;
    res = FL2_waitCStream(fcs);
  }
  CHECK_S(res);
  return S_OK;
}

HRESULT CFastEncoder::FastLzma2::AddByteCount(size_t count, ISequentialOutStream *outStream, ICompressProgressInfo *progress)
{
  dict_pos += count;
  if (dict_pos == dict.size) {
    size_t res = FL2_updateDictionary(fcs, dict_pos);
    CHECK_H(WaitAndReport(res, progress));
    if (res != 0)
      CHECK_H(WriteBuffers(outStream));
    res = FL2_getDictionaryBuffer(fcs, &dict);
    while (FL2_isTimedOut(res)) {
      if (!UpdateProgress(progress))
        return S_FALSE;
      res = FL2_getDictionaryBuffer(fcs, &dict);
    }
    CHECK_S(res);
    dict_pos = 0;
  }
  if (!UpdateProgress(progress))
    return S_FALSE;
  return S_OK;
}

bool CFastEncoder::FastLzma2::UpdateProgress(ICompressProgressInfo *progress)
{
  if (progress) {
    UInt64 outProcessed;
    UInt64 inProcessed = FL2_getCStreamProgress(fcs, &outProcessed);
    HRESULT err = progress->SetRatioInfo(&inProcessed, &outProcessed);
    if (err != S_OK) {
      FL2_cancelCStream(fcs);
      return false;
    }
  }
  return true;
}

HRESULT CFastEncoder::FastLzma2::WriteBuffers(ISequentialOutStream *outStream)
{
  size_t csize;
  for (;;) {
    FL2_cBuffer cbuf;
    do {
      csize = FL2_getNextCompressedBuffer(fcs, &cbuf);
    } while (FL2_isTimedOut(csize));
    CHECK_S(csize);
    if (csize == 0)
      break;
    HRESULT err = WriteStream(outStream, cbuf.src, cbuf.size);
    if (err != S_OK)
      return err;
  }
  return S_OK;
}

HRESULT CFastEncoder::FastLzma2::End(ISequentialOutStream *outStream, ICompressProgressInfo *progress)
{
  if (dict_pos) {
    size_t res = FL2_updateDictionary(fcs, dict_pos);
    CHECK_H(WaitAndReport(res, progress));
  }

  size_t res = FL2_endStream(fcs, nullptr);
  CHECK_H(WaitAndReport(res, progress));
  while (res) {
    CHECK_H(WriteBuffers(outStream));
    res = FL2_endStream(fcs, nullptr);
    CHECK_H(WaitAndReport(res, progress));
  }
  return S_OK;
}

void CFastEncoder::FastLzma2::Cancel()
{
  FL2_cancelCStream(fcs);
}

CFastEncoder::CFastEncoder()
{
}

CFastEncoder::~CFastEncoder()
{
}


STDMETHODIMP CFastEncoder::SetCoderProperties(const PROPID *propIDs,
  const PROPVARIANT *coderProps, UInt32 numProps)
{
  return _encoder.SetCoderProperties(propIDs, coderProps, numProps);
}


#define LZMA2_DIC_SIZE_FROM_PROP(p) (((UInt32)2 | ((p) & 1)) << ((p) / 2 + 11))

STDMETHODIMP CFastEncoder::WriteCoderProperties(ISequentialOutStream *outStream)
{
  Byte prop;
  unsigned i;
  size_t dictSize = _encoder.GetDictSize();
  for (i = 0; i < 40; i++)
    if (dictSize <= LZMA2_DIC_SIZE_FROM_PROP(i))
      break;
  prop = (Byte)i;
  return WriteStream(outStream, &prop, 1);
}


STDMETHODIMP CFastEncoder::Code(ISequentialInStream *inStream, ISequentialOutStream *outStream,
  const UInt64 * /* inSize */, const UInt64 * /* outSize */, ICompressProgressInfo *progress)
{
  CHECK_H(_encoder.Begin());
  size_t inSize;
  unsigned long dSize;
  do
  {
    BYTE* dict = _encoder.GetAvailableBuffer(dSize);

    inSize = dSize;
    HRESULT err = ReadStream(inStream, dict, &inSize);
    if (err != S_OK) {
      _encoder.Cancel();
      return err;
    }
    CHECK_H(_encoder.AddByteCount(inSize, outStream, progress));

  } while (inSize == dSize);

  CHECK_H(_encoder.End(outStream, progress));

  return S_OK;
}

}}

// DeltaFilter.cpp

#include "StdAfx.h"

#include "../../../C/Delta.h"

#include "../../Common/MyCom.h"

#include "../ICoder.h"

#include "../Common/RegisterCodec.h"

namespace NCompress {
namespace NDelta {

struct CDelta
{
  unsigned _delta;
  Byte _state[DELTA_STATE_SIZE];

  CDelta(): _delta(1) {}
  void DeltaInit() { Delta_Init(_state); }
};


#ifndef EXTRACT_ONLY

class CEncoder:
  public ICompressFilter,
  public ICompressSetCoderProperties,
  public ICompressWriteCoderProperties,
  CDelta,
  public CMyUnknownImp
{
public:
  MY_UNKNOWN_IMP3(ICompressFilter, ICompressSetCoderProperties, ICompressWriteCoderProperties)
  INTERFACE_ICompressFilter(;)
  STDMETHOD(SetCoderProperties)(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps);
  STDMETHOD(WriteCoderProperties)(ISequentialOutStream *outStream);
};

STDMETHODIMP CEncoder::Init()
{
  DeltaInit();
  return S_OK;
}

STDMETHODIMP_(UInt32) CEncoder::Filter(Byte *data, UInt32 size)
{
  Delta_Encode(_state, _delta, data, size);
  return size;
}

STDMETHODIMP CEncoder::SetCoderProperties(const PROPID *propIDs, const PROPVARIANT *props, UInt32 numProps)
{
  UInt32 delta = _delta;
  for (UInt32 i = 0; i < numProps; i++)
  {
    const PROPVARIANT &prop = props[i];
    PROPID propID = propIDs[i];
    if (propID >= NCoderPropID::kReduceSize)
      continue;
    if (prop.vt != VT_UI4)
      return E_INVALIDARG;
    switch (propID)
    {
      case NCoderPropID::kDefaultProp:
        delta = (UInt32)prop.ulVal;
        if (delta < 1 || delta > 256)
          return E_INVALIDARG;
        break;
      case NCoderPropID::kNumThreads: break;
      case NCoderPropID::kLevel: break;
      default: return E_INVALIDARG;
    }
  }
  _delta = delta;
  return S_OK;
}

STDMETHODIMP CEncoder::WriteCoderProperties(ISequentialOutStream *outStream)
{
  Byte prop = (Byte)(_delta - 1);
  return outStream->Write(&prop, 1, NULL);
}

#endif


class CDecoder:
  public ICompressFilter,
  public ICompressSetDecoderProperties2,
  CDelta,
  public CMyUnknownImp
{
public:
  MY_UNKNOWN_IMP2(ICompressFilter, ICompressSetDecoderProperties2)
  INTERFACE_ICompressFilter(;)
  STDMETHOD(SetDecoderProperties2)(const Byte *data, UInt32 size);
};

STDMETHODIMP CDecoder::Init()
{
  DeltaInit();
  return S_OK;
}

STDMETHODIMP_(UInt32) CDecoder::Filter(Byte *data, UInt32 size)
{
  Delta_Decode(_state, _delta, data, size);
  return size;
}

STDMETHODIMP CDecoder::SetDecoderProperties2(const Byte *props, UInt32 size)
{
  if (size != 1)
    return E_INVALIDARG;
  _delta = (unsigned)props[0] + 1;
  return S_OK;
}


REGISTER_FILTER_E(Delta,
    CDecoder(),
    CEncoder(),
    3, "Delta")

}}

// BitmEncoder.h -- the Most Significant Bit of byte is First

#ifndef ZIP7_INC_BITM_ENCODER_H
#define ZIP7_INC_BITM_ENCODER_H

#include "../IStream.h"

template<class TOutByte>
class CBitmEncoder
{
  unsigned _bitPos;
  Byte _curByte;
  TOutByte _stream;
public:
  bool Create(UInt32 bufferSize) { return _stream.Create(bufferSize); }
  void SetStream(ISequentialOutStream *outStream) { _stream.SetStream(outStream);}
  UInt64 GetProcessedSize() const { return _stream.GetProcessedSize() + ((8 - _bitPos + 7) >> 3); }
  void Init()
  {
    _stream.Init();
    _bitPos = 8;
    _curByte = 0;
  }
  HRESULT Flush()
  {
    if (_bitPos < 8)
      WriteBits(0, _bitPos);
    return _stream.Flush();
  }
  void WriteBits(UInt32 value, unsigned numBits)
  {
    while (numBits > 0)
    {
      if (numBits < _bitPos)
      {
        _curByte = (Byte)(_curByte | (value << (_bitPos -= numBits)));
        return;
      }
      numBits -= _bitPos;
      UInt32 newBits = (value >> numBits);
      value -= (newBits << numBits);
      _stream.WriteByte((Byte)(_curByte | newBits));
      _bitPos = 8;
      _curByte = 0;
    }
  }
};

#endif

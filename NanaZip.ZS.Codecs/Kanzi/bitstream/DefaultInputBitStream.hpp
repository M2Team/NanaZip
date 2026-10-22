/*
Copyright 2011-2026 Frederic Langlet
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
you may obtain a copy of the License at

                http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once
#ifndef knz_DefaultInputBitStream
#define knz_DefaultInputBitStream

#include "../BitStreamException.hpp"
#include "../InputBitStream.hpp"
#include "../InputStream.hpp"
#include "../Memory.hpp"
#include "../Seekable.hpp"
#include "../util/strings.hpp"


namespace kanzi {

#if defined(_MSC_VER) && _MSC_VER <= 1500
   class DefaultInputBitStream FINAL : public InputBitStream
#else
   class DefaultInputBitStream FINAL : public InputBitStream, public Seekable
#endif
   {
   private:
       InputStream& _is;
       byte* _buffer;
       int _position; // index of current byte (consumed if bitIndex == -1)
       uint _availBits; // bits not consumed in _current
       int64 _read;
       uint64 _current;
       bool _closed;
       int _maxPosition;
       uint _bufferSize;

       int readFromInputStream(uint count);

       // return number of available bits
       uint pullCurrent();

       void _close();


   public:
       // Returns 1 or 0
       int readBit();

       uint64 readBits(uint length);

       uint readBits(byte bits[], uint count);

       void close() { _close(); }

       // Number of bits read
       uint64 read() const
       {
           return uint64(_read + (int64(_position) << 3) - int64(_availBits));
       }

       // Return false when the bitstream is closed or the End-Of-Stream has been reached
       bool hasMoreToRead();

       bool isClosed() const { return _closed; }

#if !defined(_MSC_VER) || _MSC_VER > 1500
       int64 tell();

       bool seek(int64 pos);
#endif

       DefaultInputBitStream(InputStream& is, uint bufferSize = 65536);

       ~DefaultInputBitStream();
   };

   // Returns 1 or 0
   inline int DefaultInputBitStream::readBit()
   {
       if (_availBits == 0)
           _availBits = pullCurrent(); // Triggers an exception if stream is closed

       _availBits--;
       return int(_current >> _availBits) & 1;
   }

   inline uint64 DefaultInputBitStream::readBits(uint count)
   {
      if ((count == 0) || (count > 64))
          throw BitStreamException("Invalid bit count: " + TOSTR(count) + " (must be in [1..64])");

      if (count <= _availBits) {
          _availBits -= count;
          return (_current >> _availBits) & (uint64(-1) >> (64 - count));
      }

      // Not enough spots available in 'current'
      count -= _availBits;
      uint64 res = _current & ((uint64(1) << _availBits) - 1);
      _availBits = pullCurrent();

      if (_availBits < count)
          throw BitStreamException("No more data to read in the bitstream", BitStreamException::END_OF_STREAM);

      _availBits -= count;
      const uint64 tail = (_current >> _availBits) & (uint64(-1) >> (64 - count));
      return (count == 64) ? tail : ((res << count) | tail);
   }

   // Pull 64 bits of current value from buffer.
   inline uint DefaultInputBitStream::pullCurrent()
   {
       if (_position + 7 > _maxPosition) {
           if (_position > _maxPosition)
               readFromInputStream(_bufferSize);

           if (_position + 7 > _maxPosition) {
               // End of stream: overshoot max position => adjust bit index
               uint shift = uint(_maxPosition - _position) * 8;
               _availBits = shift + 8;
               uint64 val = 0;

               while (_position <= _maxPosition) {
                   val |= (uint64(_buffer[_position++]) << shift);
                   shift -= 8;
               }

               _current = val;
               return _availBits;
           }
       }

       // Regular processing, buffer length is multiple of 8
       _current = uint64(BigEndian::readLong64(&_buffer[_position]));
       _position += 8;
       return 64;
   }

#if !defined(_MSC_VER) || _MSC_VER > 1500
   inline int64 DefaultInputBitStream::tell()
   {
       if (isClosed())
           return -1;

       _is.clear();
       const int64 res = int64(_is.tellg());
       return (res < 0) ? -1 : 8 * (res - int64(_maxPosition + 1 - _position)) - int64(_availBits);
   }

   inline bool DefaultInputBitStream::seek(int64 pos)
   {
       if (isClosed())
           return false;

       if (pos < 0)
           return false;

       // Update internal states to force read at new stream position
       _read += (8 * int64(_position) - int64(_availBits));
       _availBits = 0;
       _position = 0;
       _maxPosition = -1;
       _is.clear();
       _is.seekg(std::streampos(pos >> 3));

       if (_is.fail())
           return false;

       if ((pos & 7) != 0)
           readBits(pos & 7);

       return true;
   }
#endif

}
#endif

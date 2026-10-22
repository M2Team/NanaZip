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
#ifndef knz_DebugInputBitStream
#define knz_DebugInputBitStream

#include "../InputBitStream.hpp"
#include "../OutputStream.hpp"


namespace kanzi {

   class DebugInputBitStream FINAL : public InputBitStream
   {
   private:
       InputBitStream& _delegate;
       OutputStream& _out;
       int _width;
       int _idx;
       bool _mark;
       bool _hexa;
       bool _show;
       byte _current;

       void printByte(byte val);

       void _close() { _delegate.close(); }

   public:
       DebugInputBitStream(InputBitStream& ibs);

       DebugInputBitStream(InputBitStream& ibs, OutputStream& os);

       DebugInputBitStream(InputBitStream& ibs, OutputStream& os, int width);

       ~DebugInputBitStream();

       // Returns 1 or 0
       int readBit();

       uint64 readBits(uint length);

       uint readBits(byte bits[], uint length);

       // Number of bits read
       uint64 read() const { return _delegate.read(); }

       // Return false when the bitstream is closed or the End-Of-Stream has been reached
       bool hasMoreToRead() { return _delegate.hasMoreToRead(); }

       void close() { _close(); }

       void showByte(bool show) { _show = show; }

       void setHexa(bool hexa) { _hexa = hexa; }

       bool hexa() const { return _hexa; }

       bool showByte() const { return _show; }

       void setMark(bool mark) { _mark = mark; }

       bool mark() const { return _mark; }
   };
}
#endif


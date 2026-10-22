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
#ifndef knz_DebugOutputBitStream
#define knz_DebugOutputBitStream

#include "../OutputBitStream.hpp"
#include "../OutputStream.hpp"

namespace kanzi
{

   class DebugOutputBitStream FINAL : public OutputBitStream
   {
   private:
       OutputBitStream& _delegate;
       OutputStream& _out;
       int _width;
       int _idx;
       bool _mark;
       bool _show;
       bool _hexa;
       byte _current;

       void printByte(byte val);

       void _close() { _delegate.close(); }

   public:
       DebugOutputBitStream(OutputBitStream& obs);

       DebugOutputBitStream(OutputBitStream& obs, OutputStream& os);

       DebugOutputBitStream(OutputBitStream& obs, OutputStream& os, int width);

       ~DebugOutputBitStream();

       void writeBit(int bit);

       uint writeBits(uint64 bits, uint length);

       uint writeBits(const byte bits[], uint length);

       // Return number of bits written so far
       uint64 written() const { return _delegate.written(); }

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


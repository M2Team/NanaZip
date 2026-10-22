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

#include <iomanip>
#include <iostream>
#include "../bitstream/DebugInputBitStream.hpp"

using namespace kanzi;
using namespace std;

DebugInputBitStream::DebugInputBitStream(InputBitStream& ibs) : _delegate(ibs), _out(cout), _width(80)
{
    _idx = 0;
    _mark = false;
    _show = false;
    _hexa = false;
    _current = kanzi::byte(0);
}

DebugInputBitStream::DebugInputBitStream(InputBitStream& ibs, ostream& os) : _delegate(ibs), _out(os), _width(80)
{
    _idx = 0;
    _mark = false;
    _show = false;
    _hexa = false;
    _current = kanzi::byte(0);
}

DebugInputBitStream::DebugInputBitStream(InputBitStream& ibs, ostream& os, int width) : _delegate(ibs), _out(os)
{
    if ((width != -1) && (width < 8))
        width = 8;

    if (width != -1)
        width &= 0xFFFFFFF8;

    _width = width;
    _idx = 0;
    _mark = false;
    _show = false;
    _hexa = false;
    _current = kanzi::byte(0);
}

DebugInputBitStream::~DebugInputBitStream()
{
    _close();
}

// Returns 1 or 0
int DebugInputBitStream::readBit()
{
    int res = _delegate.readBit();
    _current <<= 1;
    _current |= kanzi::byte(res);
    _out << ((res & 1) == 1 ? "1" : "0");
    _idx++;

    if (_mark == true)
        _out << "r";

    if ((_width != -1) && ((_idx - 1) % _width == _width - 1)) {
        if (showByte())
            printByte(_current);

        _out << endl;
        _idx = 0;
    }
    else if ((_idx & 7) == 0) {
        if (showByte())
            printByte(_current);
        else
            _out << " ";
    }

    return res;
}

uint64 DebugInputBitStream::readBits(uint count)
{
    uint64 res = _delegate.readBits(count);

    for (uint i = 1; i <= count; i++) {
        int bit = (res >> (count - i)) & 1;
        _idx++;
        _current <<= 1;
        _current |= kanzi::byte(bit);
        _out << ((bit == 1) ? "1" : "0");

        if ((_mark == true) && (i == count))
            _out << "r";

        if ((_width != -1) && (_idx % _width == 0)) {
            if (showByte())
                printByte(_current);

            _out << endl;
            _idx = 0;
        }
        else if ((_idx & 7) == 0) {
            if (showByte())
                printByte(_current);
            else
                _out << " ";
        }
    }

    return res;
}

uint DebugInputBitStream::readBits(kanzi::byte bits[], uint count)
{
    count = _delegate.readBits(bits, count);

    for (uint i = 0; i < count; i++) {
        const int bit = int(bits[i >> 3] >> (7 - (i & 7))) & 1;
        _idx++;
        _current <<= 1;
        _current |= kanzi::byte(bit);
        _out << ((bit == 1) ? "1" : "0");

        if ((_mark == true) && (i + 1 == count))
            _out << "r";

        if ((_width != -1) && (_idx % _width == 0)) {
            if (showByte())
                printByte(_current);

            _out << endl;
            _idx = 0;
        }
        else if ((_idx & 7) == 0) {
            if (showByte())
                printByte(_current);
            else
                _out << " ";
        }
    }

    return count;
}

void DebugInputBitStream::printByte(kanzi::byte b)
{
    int val = int(b);

    if (_hexa == true) {
        _out << hex << " [0x";
        _out << ((val < 16) ? "0" : "");
        _out << val << "] ";
        _out << dec;
        return;
    }

    _out << " [";

    if (val < 10)
        _out << "00";
    else if (val < 100)
        _out << "0";

    _out << val << "] ";
}


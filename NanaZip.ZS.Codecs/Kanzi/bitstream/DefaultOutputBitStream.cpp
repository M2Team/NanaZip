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

#include "DefaultOutputBitStream.hpp"

using namespace kanzi;
using namespace std;

DefaultOutputBitStream::DefaultOutputBitStream(OutputStream& os, uint bufferSize) : _os(os)
{
    if (bufferSize < 1024)
        throw invalid_argument("Invalid buffer size (must be at least 1024)");

    if (bufferSize > 1 << 29)
        throw invalid_argument("Invalid buffer size (must be at most 536870912)");

    if ((bufferSize & 7) != 0)
        throw invalid_argument("Invalid buffer size (must be a multiple of 8)");

    _availBits = 64;
    _bufferSize = bufferSize;
    _buffer = new kanzi::byte[_bufferSize];
    _position = 0;
    _current = 0;
    _written = 0;
    _closed = false;
    memset(&_buffer[0], 0, size_t(_bufferSize));
}

uint DefaultOutputBitStream::writeBits(const kanzi::byte bits[], uint count)
{
    if (isClosed() == true)
        throw BitStreamException("Stream closed", BitStreamException::STREAM_CLOSED);

    uint remaining = count;
    uint start = 0;

    // Byte aligned cursor ?
    if ((_availBits & 7) == 0) {
        // Fill up _current
        while ((_availBits != 64) && (remaining >= 8)) {
            writeBits(uint64(bits[start]), 8);
            start++;
            remaining -= 8;
        }

        const uint maxPos = _bufferSize - 8;

        // Copy bits array to internal buffer
        while ((remaining >> 3) >= maxPos - _position) {
            memcpy(&_buffer[_position], &bits[start], maxPos - _position);
            start += (maxPos - _position);
            remaining -= ((maxPos - _position) << 3);
            _position = maxPos;
            flush();
        }

        const uint r = (remaining >> 6) << 3;

        if (r > 0) {
            memcpy(&_buffer[_position], &bits[start], r);
            start += r;
            _position += r;
            remaining -= (r << 3);
        }
    }
    else if (remaining >= 64) {
        // Not kanzi::byte aligned
        const uint r = 64 - _availBits;
        const uint a = _availBits;

        while (remaining >= 256) {
            const uint64 v1 = uint64(BigEndian::readLong64(&bits[start]));
            const uint64 v2 = uint64(BigEndian::readLong64(&bits[start + 8]));
            const uint64 v3 = uint64(BigEndian::readLong64(&bits[start + 16]));
            const uint64 v4 = uint64(BigEndian::readLong64(&bits[start + 24]));
            _current |= (v1 >> r);

            if (_position >= _bufferSize - 32)
                flush();

            BigEndian::writeLong64(&_buffer[_position], _current);
            BigEndian::writeLong64(&_buffer[_position + 8],  (v1 << a) | (v2 >> r));
            BigEndian::writeLong64(&_buffer[_position + 16], (v2 << a) | (v3 >> r));
            BigEndian::writeLong64(&_buffer[_position + 24], (v3 << a) | (v4 >> r));
            _position += 32;
            _current = (v4 << a);
            start += 32;
            remaining -= 256;
            _availBits = 64;
        }

        while (remaining >= 64) {
           const uint64 v = uint64(BigEndian::readLong64(&bits[start]));
           _current |= (v >> r);
           pushCurrent();
           _current = v << a;
           start += 8;
           remaining -= 64;
        }

        _availBits = a;
    }

    // Last bytes
    while (remaining >= 8) {
        writeBits(uint64(bits[start]), 8);
        start++;
        remaining -= 8;
    }

    if (remaining > 0)
        writeBits(uint64(bits[start]) >> (8 - remaining), remaining);

    return count;
}

void DefaultOutputBitStream::_close()
{
    if (isClosed() == true)
        return;

    uint savedBitIndex = _availBits;
    uint savedPosition = _position;
    uint64 savedCurrent = _current;

    try {
        // Push last bytes (the very last kanzi::byte may be incomplete)
        uint shift = 56;

        while (_availBits < 64) {
            _buffer[_position++] = kanzi::byte(_current >> shift);
            shift -= 8;
            _availBits += 8;
        }

        _written -= int64(_availBits - 64); // can be negative
        _availBits = 64;
        flush();
    }
    catch (const BitStreamException&) {
        // Revert fields to allow subsequent attempts in case of transient failure
        _position = savedPosition;
        _availBits = savedBitIndex;
        _current = savedCurrent;
        throw; // re-throw
    }

    try {
        _os.flush();

        if (_os.fail())
            throw BitStreamException("Write to bitstream failed.", BitStreamException::INPUT_OUTPUT);
    }
    catch (const ios_base::failure& e) {
        throw BitStreamException(e.what(), BitStreamException::INPUT_OUTPUT);
    }

    _closed = true;
    _position = 0;
    _availBits = 0;
    _written -= 64; // adjust because _availBits = 0

    // Reset fields to force a flush() and trigger an exception
    // on writeBit() or writeBits()
    kanzi::byte* buffer = new kanzi::byte[8];
    memset(&buffer[0], 0, size_t(8));
    delete[] _buffer;
    _buffer = buffer;
    _bufferSize = 8;
}

// Write buffer to underlying stream
void DefaultOutputBitStream::flush()
{
    if (isClosed() == true)
        throw BitStreamException("Stream closed", BitStreamException::STREAM_CLOSED);

    try {
        if (_position > 0) {
            _os.write(reinterpret_cast<char*>(_buffer), _position);

            if (_os.fail())
                throw BitStreamException("Write to bitstream failed", BitStreamException::INPUT_OUTPUT);

            _written += (int64(_position) << 3);
            _position = 0;
        }
    }
    catch (const ios_base::failure& e) {
        throw BitStreamException(e.what(), BitStreamException::INPUT_OUTPUT);
    }
}

DefaultOutputBitStream::~DefaultOutputBitStream()
{
    try {
        _close();
    }
    catch (const exception&) {
        // Ignore and continue
    }

    delete[] _buffer;
}

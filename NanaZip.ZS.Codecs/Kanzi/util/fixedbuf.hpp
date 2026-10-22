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
#ifndef knz_fixedbuf
#define knz_fixedbuf

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <streambuf>
#include "../SliceArray.hpp"
#include "../types.hpp"


// Ahem ... Visual Studio
// This code is required because Microsoft cannot bother to implement streambuf::pubsetbuf().
// Also On libstdc++, pubsetbuf() silently ignores the supplied buffer and leaves internal pointers null.

class ifixedbuf : public std::streambuf {
public:
    ifixedbuf(char* data, std::size_t size) {
        // Always manually set the read pointers.
        // pubsetbuf() is unreliable on libstdc++, and MSVC doesn't implement it.
        this->setg(data, data, data + size);
    }
};

class ofixedbuf : public std::streambuf {
public:
    ofixedbuf(char* data, std::size_t size) {
        // Always set buffer manually - pubsetbuf is useless on libstdc++
        this->setp(data, data + size);
    }

    std::size_t written() const {
        return this->pptr() - this->pbase();
    }
};

class growable_ofixedbuf FINAL : public std::streambuf {
public:
    explicit growable_ofixedbuf(kanzi::SliceArray<kanzi::byte>* data)
        : _data(data)
    {
        this->setp(reinterpret_cast<char*>(_data->_array),
                   reinterpret_cast<char*>(_data->_array) + _data->_length);
    }

protected:
    virtual int_type overflow(int_type c) {
        if (traits_type::eq_int_type(c, traits_type::eof()))
            return traits_type::not_eof(c);

        _grow(std::size_t(this->pptr() - this->pbase()) + 1);
        *this->pptr() = traits_type::to_char_type(c);
        this->pbump(1);
        return c;
    }

    virtual std::streamsize xsputn(const char* s, std::streamsize n) {
        if (n <= 0)
            return 0;

        const std::size_t written = std::size_t(this->pptr() - this->pbase());
        const std::size_t required = written + std::size_t(n);

        if (required > std::size_t(this->epptr() - this->pbase()))
            _grow(required);

        std::memcpy(this->pptr(), s, std::size_t(n));
        this->pbump(int(n));
        return n;
    }

private:
    kanzi::SliceArray<kanzi::byte>* _data;

    void _grow(std::size_t required) {
        const std::size_t current = std::size_t(_data->_length);
        const std::size_t written = std::size_t(this->pptr() - this->pbase());
        const std::size_t grown = current + (std::max)(current >> 2, std::size_t(1) << 20);
        const std::size_t newSize = (std::max)(required, (std::max)(grown, std::size_t(1024)));
        kanzi::byte* buf = new kanzi::byte[newSize];

        if (written > 0)
            std::memcpy(buf, _data->_array, written);

        delete[] _data->_array;
        _data->_array = buf;
        _data->_length = int(newSize);
        this->setp(reinterpret_cast<char*>(_data->_array),
                   reinterpret_cast<char*>(_data->_array) + _data->_length);
        this->pbump(int(written));
    }
};

class iofixedbuf : public std::streambuf {
public:
    iofixedbuf(char* data, std::size_t size) {
        this->setg(data, data, data + size);
        this->setp(data, data + size);
    }
};

#endif

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
#include <ios>
#include <sstream>
#include "Event.hpp"
#include "util/strings.hpp"

using namespace kanzi;

Event::Event(Event::Type type, int id, const std::string& msg, WallTimer::TimeData evtTime)
    : _type(type)
    , _time(evtTime)
    , _msg(msg)
    , _id(id)
    , _size(0)
    , _offset(-1)
    , _hash(0)
    , _hashType(NO_HASH)
    , _skipFlags(0)
    , _info(nullptr)
{
}

Event::Event(Event::Type type, int id, const HeaderInfo& info, WallTimer::TimeData evtTime)
    : _type(type)
    , _time(evtTime)
    , _msg("")
    , _id(id)
    , _size(0)
    , _offset(-1)
    , _hash(0)
    , _hashType(NO_HASH)
    , _skipFlags(0)
{
    _info = new HeaderInfo();
    _info->inputName = info.inputName;
    _info->bsVersion = info.bsVersion;
    _info->checksumSize = info.checksumSize;
    _info->blockSize = info.blockSize;
    _info->entropyType = info.entropyType;
    _info->transformType = info.transformType;
    _info->originalSize = info.originalSize;
    _info->fileSize = info.fileSize;
}

Event::Event(Event::Type type, int id, int64 size, WallTimer::TimeData evtTime,
             uint64 hash, HashType hashType, int64 offset, uint8 skipFlags)
    : _type(type)
    , _time(evtTime)
    , _msg()
    , _id(id)
    , _size(size)
    , _offset(offset)
    , _hash(hash)
    , _hashType(hashType)
    , _skipFlags(skipFlags)
    , _info(nullptr)
{
}

Event::Event(const Event& other)
    : _type(other._type)
    , _time(other._time)
    , _msg(other._msg)
    , _id(other._id)
    , _size(other._size)
    , _offset(other._offset)
    , _hash(other._hash)
    , _hashType(other._hashType)
    , _skipFlags(other._skipFlags)
    , _info(nullptr)
{
    if (other._info != nullptr) {
        _info = new HeaderInfo();
        _info->inputName = other._info->inputName;
        _info->bsVersion = other._info->bsVersion;
        _info->checksumSize = other._info->checksumSize;
        _info->blockSize = other._info->blockSize;
        _info->entropyType = other._info->entropyType;
        _info->transformType = other._info->transformType;
        _info->originalSize = other._info->originalSize;
        _info->fileSize = other._info->fileSize;
    }
}

Event& Event::operator=(const Event& other)
{
    if (this != &other) {
        _type      = other._type;
        _time      = other._time;
        _msg       = other._msg;
        _id        = other._id;
        _size      = other._size;
        _offset    = other._offset;
        _hash      = other._hash;
        _hashType  = other._hashType;
        _skipFlags = other._skipFlags;

        if (_info != nullptr) {
           delete _info;
           _info = nullptr;
        }

        if (other._info != nullptr) {
            _info = new HeaderInfo();
            _info->inputName = other._info->inputName;
            _info->bsVersion = other._info->bsVersion;
            _info->checksumSize = other._info->checksumSize;
            _info->blockSize = other._info->blockSize;
            _info->entropyType = other._info->entropyType;
            _info->transformType = other._info->transformType;
            _info->originalSize = other._info->originalSize;
            _info->fileSize = other._info->fileSize;
        }
    }

    return *this;
}

#if defined(__cplusplus) && (__cplusplus >= 201103L)

Event::Event(Event&& other) noexcept
    : _type(other._type)
    , _time(other._time)
    , _msg(std::move(other._msg))
    , _id(other._id)
    , _size(other._size)
    , _offset(other._offset)
    , _hash(other._hash)
    , _hashType(other._hashType)
    , _skipFlags(other._skipFlags)
    , _info(other._info)
{
    other._info = nullptr;
}

Event& Event::operator=(Event&& other) noexcept
{
    if (this != &other) {
        _type       = other._type;
        _time       = other._time;
        _msg        = std::move(other._msg);
        _id         = other._id;
        _size       = other._size;
        _offset     = other._offset;
        _hash       = other._hash;
        _hashType   = other._hashType;
        _skipFlags  = other._skipFlags;

        if (_info != nullptr)
           delete _info;

        _info       = other._info;
        other._info = nullptr;
    }

    return *this;
}
#endif


std::string Event::toString() const
{
    if (_msg != "")
        return _msg;

    std::stringstream ss;
    ss << "{ \"type\":\"" << getTypeAsString() << "\"";

    if (_id >= 0)
        ss << ", \"id\":" << getId();

    if (_info != nullptr) {
       ss << ", \"inputName\":\"" << escapeJSONString(_info->inputName) << "\"";
       ss << ", \"bsVersion\":" << _info->bsVersion;
       ss << ", \"checksum\":" << _info->checksumSize;
       ss << ", \"blockSize\":" << _info->blockSize;
       ss << ", \"entropy\":\"" << _info->entropyType << "\"";
       ss << ", \"transform\":\"" << _info->transformType << "\"";

       if (_info->fileSize >= 0)
          ss << ", \"compressed\":" << _info->fileSize;

       if (_info->originalSize >= 0)
          ss << ", \"original\":" << _info->originalSize;
    }
    else {
       ss << ", \"size\":" << getSize();

       if (getType() != BLOCK_INFO)
           ss << ", \"time\":" << getTime().to_ms();

       if (_hashType != NO_HASH) {
           ss << ", \"hash\":\"";
           ss << std::uppercase << std::setfill('0');

           if (_hashType == SIZE_32)
              ss << std::setw(8) << std::hex << getHash() << "\"";
           else
              ss << std::setw(16) << std::hex << getHash() << "\"";

           ss << std::dec;
       }

       if (getType() == BLOCK_INFO) {
           ss << ", \"offset\":" << getOffset();
           ss << ", \"skipFlags\":";

           for (int i = 128; i >= 1; i >>= 1)
              ss << ((_skipFlags & i) == 0 ? "0" : "1");
       }
    }

    ss << " }";
    return ss.str();
}

std::string Event::getTypeAsString() const
{
    switch (_type) {
       case AFTER_HEADER_DECODING:
          return "AFTER_HEADER_DECODING";
       case COMPRESSION_END:
          return "COMPRESSION_END";
       case BEFORE_TRANSFORM:
          return "BEFORE_TRANSFORM";
       case AFTER_TRANSFORM:
          return "AFTER_TRANSFORM";
       case BEFORE_ENTROPY:
          return "BEFORE_ENTROPY";
       case AFTER_ENTROPY:
          return "AFTER_ENTROPY";
       case DECOMPRESSION_START:
          return "DECOMPRESSION_START";
       case DECOMPRESSION_END:
          return "DECOMPRESSION_END";
       case COMPRESSION_START:
          return "COMPRESSION_START";
       case BLOCK_INFO:
          return "BLOCK_INFO";
       default:
          return "Unknown Type";
    }
}

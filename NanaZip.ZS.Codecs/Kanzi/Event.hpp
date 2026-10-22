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
#ifndef knz_Event
#define knz_Event

#include <string>
#include <time.h>
#include "types.hpp"
#include "util/WallTimer.hpp"


namespace kanzi
{

   class Event {
      public:
          enum Type {
              COMPRESSION_START,
              COMPRESSION_END,
              BEFORE_TRANSFORM,
              AFTER_TRANSFORM,
              BEFORE_ENTROPY,
              AFTER_ENTROPY,
              DECOMPRESSION_START,
              DECOMPRESSION_END,
              AFTER_HEADER_DECODING,
              BLOCK_INFO
          };

          enum HashType {
              NO_HASH,
              SIZE_32,
              SIZE_64
          };

          typedef struct HeaderInfo {
              std::string inputName;
              int bsVersion;
              int checksumSize;
              int blockSize;
              std::string entropyType;
              std::string transformType;
              int64 originalSize;
              int64 fileSize;
          } HeaderInfo;

          Event(Type type, int id, const std::string& msg, WallTimer::TimeData evtTime);
          Event(Type type, int id, int64 size, WallTimer::TimeData evtTime, uint64 hash = 0,
                HashType hashType = NO_HASH, int64 offset = -1, uint8 skipFlags = 0);
          Event(Type type, int id, const HeaderInfo& info, WallTimer::TimeData evtTime);

          Event(const Event& other);
          Event& operator=(const Event& other);

#if defined(__cplusplus) && (__cplusplus >= 201103L)
          Event(Event&& other) noexcept;
          Event& operator=(Event&& other) noexcept;
#endif

          virtual ~Event() { if (_info != nullptr) delete _info; }

          int getId() const { return _id; }
          int64 getSize() const { return _size; }
          Event::Type getType() const { return _type; }
          WallTimer::TimeData getTime() const { return _time; }
          uint64 getHash() const { return _hashType != NO_HASH ? _hash : 0; }
          int64 getOffset() const { return _offset; }
          HashType getHashType() const { return _hashType; }
          HeaderInfo* getInfo() const { return _info; }
          std::string toString() const;
          std::string getTypeAsString() const;

      private:
          Event::Type _type;
          WallTimer::TimeData _time;
          std::string _msg;
          int _id;
          int64 _size;
          int64 _offset;
          uint64 _hash;
          HashType _hashType;
          uint8 _skipFlags;
          HeaderInfo* _info;
      };
}

#endif

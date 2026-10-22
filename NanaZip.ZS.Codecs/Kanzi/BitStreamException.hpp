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
#ifndef knz_BitStreamException
#define knz_BitStreamException

#include <string>
#include <stdexcept>
#include "types.hpp"


namespace kanzi
{

   class BitStreamException : public std::runtime_error
   {
   private:
       int _code;

   public:
       enum BitStreamStatus {
           UNDEFINED = 0,
           INPUT_OUTPUT = 1,
           END_OF_STREAM = 2,
           INVALID_STREAM = 3,
           STREAM_CLOSED = 4
       };

       BitStreamException(const std::string& msg) : std::runtime_error(msg)
       {
           _code = UNDEFINED;
       }

       BitStreamException(const std::string& msg, int code) : std::runtime_error(msg), _code(code)
       {
       }

#if __cplusplus >= 201103L
       BitStreamException(const BitStreamException&) = default;

       BitStreamException& operator=(const BitStreamException&) = default;
#endif

       int error() const { return _code; }

       ~BitStreamException() NOEXCEPT {}
   };

}
#endif


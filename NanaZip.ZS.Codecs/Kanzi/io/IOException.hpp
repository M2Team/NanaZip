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
#ifndef knz_IOException
#define knz_IOException

#include <string>
#include <stdexcept>
#include "../Error.hpp"
#include "../types.hpp"
#include "../util/strings.hpp"


namespace kanzi
{

   class IOException : public std::runtime_error
   {
   private:
       int _code;

   public:
       IOException(const std::string& msg) : std::runtime_error(msg + ". Error code: " + TOSTR(Error::ERR_UNKNOWN))
       {
           _code = Error::ERR_UNKNOWN;
       }

       IOException(const std::string& msg, int error) : std::runtime_error(msg + ". Error code: " + TOSTR(error))
       {
           _code = error;
       }

#if __cplusplus >= 201103L
       IOException(const IOException&) = default;

       IOException& operator=(const IOException&) = default;
#endif

       int error() const { return _code; }

       ~IOException() NOEXCEPT {}
   };

}
#endif


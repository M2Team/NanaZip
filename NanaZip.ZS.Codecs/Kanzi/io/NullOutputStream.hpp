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
#ifndef knz_NullOutputStream
#define knz_NullOutputStream

namespace kanzi
{
   template <class T, class traits = std::char_traits<T> >
   class basic_nullbuf : public std::basic_streambuf<T, traits>
   {
       typename traits::int_type overflow(typename traits::int_type c)
       {
           return traits::not_eof(c);
       }

       void close() {}
   };

   template <class T, class traits = std::char_traits<T> >
   class basic_onullstream : public std::basic_ostream<T, traits>
   {
   public:
       basic_onullstream() :
           std::basic_ios<T, traits>(&_sbuf),
           std::basic_ostream<T, traits>(&_sbuf)
       {
           this->init(&_sbuf);
       }

   private:
       basic_nullbuf<T, traits> _sbuf;
   };

   typedef basic_onullstream<char> NullOutputStream;
}

#endif


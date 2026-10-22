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
#ifndef knz_types
#define knz_types

    #if defined(_MSC_VER) && _MSC_VER < 1600
       // Visual Studio < 2010: no stdint.h
       typedef unsigned char      uint8_t;
       typedef signed char        int8_t;
       typedef unsigned short     uint16_t;
       typedef short              int16_t;
       typedef unsigned int       uint32_t;
       typedef int                int32_t;
       typedef unsigned __int64   uint64_t;
       typedef __int64            int64_t;
    #else
        #if (__cplusplus >= 201103L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201103L)
            // C++11 or later
            #include <cstdint>
            #include <cstddef>
        #else
            // C++98 / C++03
            #include <stdint.h>
            #include <stddef.h>
        #endif
    #endif

    #if defined(_MSC_VER)
        #if _MSC_VER < 1900
            // snprintf macro for MSVC < 2015
            #define snprintf _snprintf
        #endif

        #if defined(_M_X64) && !defined(__x86_64__)
            #define __x86_64__  _M_X64
        #endif
        #if defined(_M_IX86) && !defined(__i386__)
            #define __i386__  _M_IX86
        #endif
    #endif


    #if defined(_MSC_VER)
        #include <intrin.h>
        #define popcount __popcnt
    #else
        #if defined(__INTEL_COMPILER)
            #include <intrin.h>
            #define popcount _popcnt32
        #else
            #define popcount __builtin_popcount
        #endif
    #endif

   #ifdef __SSE__
      #include <xmmintrin.h>
   #endif

   #ifdef __SSE2__
      #include <emmintrin.h>
   #endif

   #ifdef __SSE3__
      #include <pmmintrin.h>
   #endif

   #ifdef __SSE4_1__
       #include <smmintrin.h>
   #endif

   #ifdef __AVX__
       #include <immintrin.h>
   #endif

   #ifdef __AVX2__
       #include <immintrin.h>
   #endif

   /*
   Visual Studio 2022 17.2                  MSVC++ 14.28 _MSC_VER == 1932
   Visual Studio 2022 17.1                  MSVC++ 14.28 _MSC_VER == 1931
   Visual Studio 2022 17.0                  MSVC++ 14.28 _MSC_VER == 1930
   Visual Studio 2019 version 16.10, 16.11  MSVC++ 14.28 _MSC_VER == 1929
   Visual Studio 2019 version 16.8, 16.9    MSVC++ 14.28 _MSC_VER == 1928
   Visual Studio 2019 version 16.7          MSVC++ 14.27 _MSC_VER == 1927
   Visual Studio 2019 version 16.6          MSVC++ 14.26 _MSC_VER == 1926
   Visual Studio 2019 version 16.5          MSVC++ 14.25 _MSC_VER == 1925
   Visual Studio 2019 Update 4              MSVC++ 14.24 _MSC_VER == 1924
   Visual Studio 2019 Update 3              MSVC++ 14.21 _MSC_VER == 1923
   Visual Studio 2019 Update 2              MSVC++ 14.21 _MSC_VER == 1922
   Visual Studio 2019 Update 1              MSVC++ 14.21 _MSC_VER == 1921
   Visual Studio 2019                       MSVC++ 14.20 _MSC_VER == 1920
   Visual Studio 2017 Update 9              MSVC++ 14.16 _MSC_VER == 1916
   Visual Studio 2017 Update 8              MSVC++ 14.15 _MSC_VER == 1915
   Visual Studio 2017 Update 7              MSVC++ 14.14 _MSC_VER == 1914
   Visual Studio 2017 Update 6              MSVC++ 14.13 _MSC_VER == 1913
   Visual Studio 2017 Update 5              MSVC++ 14.12 _MSC_VER == 1912
   Visual Studio 2017 Update 3&4            MSVC++ 14.11 _MSC_VER == 1911
   Visual Studio 2017                       MSVC++ 14.10 _MSC_VER == 1910
   Visual Studio 2015                       MSVC++ 14    _MSC_VER == 1900
   Visual Studio 2013                       MSVC++ 12    _MSC_VER == 1800
   Visual Studio 2012                       MSVC++ 11    _MSC_VER == 1700
   Visual Studio 2010                       MSVC++ 10    _MSC_VER == 1600
   Visual Studio 2008                       MSVC++ 9     _MSC_VER == 1500
   Visual Studio 2005                       MSVC++ 8     _MSC_VER == 1400
   Visual Studio 2003 Beta                  MSVC++ 7.1   _MSC_VER == 1310
   Visual Studio 2002                       MSVC++ 7     _MSC_VER == 1300
   Visual Studio                            MSVC++ 6.0   _MSC_VER == 1200
   Visual Studio                            MSVC++ 5     _MSC_VER == 1100
   Visual Studio                            MSVC++ 4.2   _MSC_VER == 1020
   Visual Studio                            MSVC++ 4.1   _MSC_VER == 1010
   Visual Studio                            MSVC++ 4     _MSC_VER == 1000
   Visual Studio                            MSVC++ 2     _MSC_VER == 900
   Visual Studio                            MSVC++ 1     _MSC_VER == 800
   */

   #ifdef _MSC_VER
      #if _MSC_VER >= 1930
         #define _MSC_VER_STR 2022
      #elif _MSC_VER >= 1920
         #define _MSC_VER_STR 2019
      #elif _MSC_VER >= 1910
         #define _MSC_VER_STR 2017
      #elif _MSC_VER == 1900
         #define _MSC_VER_STR 2015
      #elif _MSC_VER == 1800
         #define _MSC_VER_STR 2013
      #elif _MSC_VER == 1700
         #define _MSC_VER_STR 2012
      #elif _MSC_VER == 1600
         #define _MSC_VER_STR 2010
      #elif _MSC_VER == 1500
         #define _MSC_VER_STR 2008
      #elif _MSC_VER == 1400
         #define _MSC_VER_STR 2005
      #elif _MSC_VER == 1300
         #define _MSC_VER_STR 2003
      #elif _MSC_VER == 1200
         #define _MSC_VER_STR 2002
      #endif
   #endif

    // Notice: in Visual Studio (prior to VS2017 version 15.7)
    // __cplusplus always defaults to 199711L (aka C++98) !!! (unless
    // the extra option /Zc:__cplusplus is added to the command line).
    // Otherwise, using the _MSVC_LANG macro returns the proper C++ version.
    #if (__cplusplus >= 201103L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201103L)
       // C++ 11 or higher
       #define FINAL final
       #define NOEXCEPT noexcept
    #else
       #define FINAL
       #define NOEXCEPT throw()

       #if defined(_MSC_VER)
          #if _MSC_VER < 1300
             typedef signed char int8_t;
             typedef signed short int16_t;
             typedef signed int int32_t;
             typedef unsigned char uint8_t;
             typedef unsigned short uint16_t;
             typedef unsigned int uint32_t;
          #else
             typedef signed __int8 int8_t;
             typedef signed __int16 int16_t;
             typedef signed __int32 int32_t;
             typedef unsigned __int8 uint8_t;
             typedef unsigned __int16 uint16_t;
             typedef unsigned __int32 uint32_t;
             typedef signed __int64 int64_t;
             typedef unsigned __int64 uint64_t;
          #endif
       #else
             // If stdint.h did not provide fixed-width types, define them here.
             #ifndef INT64_MAX
                 typedef signed char        int8_t;
                 typedef signed short       int16_t;
                 typedef signed int         int32_t;
                 typedef unsigned char      uint8_t;
                 typedef unsigned short     uint16_t;
                 typedef unsigned int       uint32_t;
                 typedef signed long long   int64_t;
                 typedef unsigned long long uint64_t;
             #endif
       #endif

       #if !defined(nullptr)
          #define nullptr NULL
       #endif
    #endif


    namespace kanzi
    {
#if (__cplusplus >= 201703L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
        using byte = std::byte;
#else
        typedef uint8_t byte;
#endif
        typedef int8_t   int8;
        typedef uint8_t  uint8;
        typedef int16_t  int16;
        typedef uint16_t uint16;
        typedef int32_t  int32;
        typedef uint32_t uint32;
        typedef uint32_t uint;

        typedef int64_t  int64;
        typedef uint64_t uint64;
    }

   #if defined(__MINGW32__)
      #define PATH_SEPARATOR '/'
   #elif defined(WIN32) || defined(_WIN32) || defined(_WIN64)
      #define PATH_SEPARATOR '\\'
   #else
      #define PATH_SEPARATOR '/'
   #endif


   // Likely / unlikely macros
   #if defined(__GNUC__) || defined(__clang__)
       #ifndef KANZI_LIKELY
           #define KANZI_LIKELY(x)   __builtin_expect(!!(x), 1)
       #endif
       #ifndef KANZI_UNLIKELY
           #define KANZI_UNLIKELY(x) __builtin_expect(!!(x), 0)
       #endif
   #else
       #ifndef KANZI_LIKELY
           #define KANZI_LIKELY(x)   (x)
       #endif
       #ifndef KANZI_UNLIKELY
           #define KANZI_UNLIKELY(x) (x)
       #endif
   #endif

   // Force inline macro
   #if defined(__GNUC__) || defined(__clang__)
      #define KANZI_ALWAYS_INLINE inline __attribute__((always_inline))
   #elif defined(_MSC_VER)
      #define KANZI_ALWAYS_INLINE __forceinline
   #else
      #define KANZI_ALWAYS_INLINE inline
   #endif

   #if defined(_MSC_VER)
      #define KANZI_ALIGNED_(x) __declspec(align(x))
   #elif defined(__GNUC__)
      #define KANZI_ALIGNED_(x) __attribute__ ((aligned(x)))
   #endif

#endif

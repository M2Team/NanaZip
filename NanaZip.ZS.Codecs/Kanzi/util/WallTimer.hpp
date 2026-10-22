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
#ifndef knz_WallTimer
#define knz_WallTimer

#include "../types.hpp"

// Portable wall timer

// 1. Detect Standard and Platform
#if __cplusplus >= 201103L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201103L)
    #include <chrono>
    #define KNZ_USE_CHRONO
#elif defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
    #define KNZ_USE_WINDOWS_QPC
#else
    #include <sys/time.h>
    #define KNZ_USE_POSIX_GETTIMEOFDAY
#endif

class WallTimer {
public:
    struct TimeData {
#if defined(KNZ_USE_CHRONO)
        std::chrono::steady_clock::time_point value;
#elif defined(KNZ_USE_WINDOWS_QPC)
        LARGE_INTEGER value;
#elif defined(KNZ_USE_POSIX_GETTIMEOFDAY)
        struct timeval value;
#endif

        // Converts the internal timestamp into total milliseconds.
#if defined(KNZ_USE_CHRONO)
        kanzi::uint64 to_ms() const {
            return static_cast<kanzi::uint64>(std::chrono::duration<double, std::milli>(value.time_since_epoch()).count());
        }
#elif defined(KNZ_USE_WINDOWS_QPC)
        double to_ms(long long win_freq = 1) const {
            return static_cast<kanzi::uint64>(value.QuadPart) * 1000.0 / win_freq;
        }
#elif defined(KNZ_USE_POSIX_GETTIMEOFDAY)
        double to_ms() const {
            return (static_cast<kanzi::uint64>(value.tv_sec) * 1000.0) + (value.tv_usec / 1000.0);
        }
#endif
    };

    WallTimer() {
#if defined(KNZ_USE_WINDOWS_QPC)
        if (_initialized == false) {
            QueryPerformanceFrequency(&_frequency);
            _initialized = true;
        }
#endif
        _start = getCurrentTime();
    }

    TimeData getCurrentTime() const {
        TimeData now;
#if defined(KNZ_USE_CHRONO)
        now.value = std::chrono::steady_clock::now();
#elif defined(KNZ_USE_WINDOWS_QPC)
        QueryPerformanceCounter(&now.value);
#elif defined(KNZ_USE_POSIX_GETTIMEOFDAY)
        gettimeofday(&now.value, 0);
#endif
        return now;
    }

    static double calculateDifference(const TimeData& start, const TimeData& end) {
#if defined(KNZ_USE_CHRONO)
        return std::chrono::duration<double, std::milli>(end.value - start.value).count();
#elif defined(KNZ_USE_WINDOWS_QPC)
        if (_initialized == false) {
            QueryPerformanceFrequency(&_frequency);
            _initialized = true;
        }

        return static_cast<double>(end.value.QuadPart - start.value.QuadPart)
               * 1000.0
               / static_cast<double>(_frequency.QuadPart);
#elif defined(KNZ_USE_POSIX_GETTIMEOFDAY)
        const double sec = double(end.value.tv_sec - start.value.tv_sec);
        const double usec = double(end.value.tv_usec - start.value.tv_usec);
        return (sec * 1000.0) + (usec / 1000.0);
#endif
    }

    double elapsed_ms() const {
        return calculateDifference(_start, getCurrentTime());
    }

private:
    TimeData _start;

#if defined(KNZ_USE_WINDOWS_QPC)
    static LARGE_INTEGER _frequency;
    static bool _initialized;
#endif
};

#endif

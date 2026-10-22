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
#ifndef knz_Clock
#define knz_Clock

#include "WallTimer.hpp"

namespace kanzi
{
   class Clock {
   private:
           WallTimer _timer;
           WallTimer::TimeData _start;
           WallTimer::TimeData _stop;

   public:
           Clock()
           {
                   start();
                   _stop = _start;
           }

           void start()
           {
                   _start = _timer.getCurrentTime();
           }

           void stop()
           {
                   _stop = _timer.getCurrentTime();
           }

           double elapsed() const
           {
                   // In millisec
                   return WallTimer::calculateDifference(_start, _stop);
           }
   };
}

#endif

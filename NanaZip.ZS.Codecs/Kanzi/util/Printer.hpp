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
#ifndef knz_Printer
#define knz_Printer


#ifdef CONCURRENCY_ENABLED
#include <mutex>
#endif

namespace kanzi
{

   // Thread safe printer
   class Printer
   {
      public:
         Printer(std::ostream& os) { _os = &os; }

         ~Printer() {
            try  {
                _os->flush();
            }
            catch (const std::exception&) {
                // Ignore: best effort
            }
         }

         void print(const char* msg, bool print) {
            if ((print == true) && (msg != nullptr)) {
   #ifdef CONCURRENCY_ENABLED
               std::lock_guard<std::mutex> lock(getMutex());
   #endif
               (*_os) << msg;
            }
         }

         void println(const char* msg, bool print) {
            if ((print == true) && (msg != nullptr)) {
   #ifdef CONCURRENCY_ENABLED
               std::lock_guard<std::mutex> lock(getMutex());
   #endif
               (*_os) << msg << std::endl;
            }
         }

         void print(const std::string& msg, bool print) {
            if (print == true) {
   #ifdef CONCURRENCY_ENABLED
               std::lock_guard<std::mutex> lock(getMutex());
   #endif
               (*_os) << msg;
            }
         }

         void println(const std::string& msg, bool print) {
            if (print == true) {
   #ifdef CONCURRENCY_ENABLED
               std::lock_guard<std::mutex> lock(getMutex());
   #endif
               (*_os) << msg << std::endl;
            }
         }


   private:
   #ifdef CONCURRENCY_ENABLED
         static std::mutex& getMutex() {
            static std::mutex mtx;
            return mtx;
         }
   #endif
         std::ostream* _os;
   };

}
#endif

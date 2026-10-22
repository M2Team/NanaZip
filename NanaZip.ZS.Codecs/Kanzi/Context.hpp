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
#ifndef knz_Context
#define knz_Context

#include <map>
#include <string>

#if __cplusplus >= 201703L
    #include <variant>
#endif

#include "concurrent.hpp" // definition of CONCURRENCY_ENABLED

namespace kanzi {

   #if __cplusplus >= 201703L
    // C++17+ version using std::variant
    typedef std::variant<int64, std::string> ContextVal;

   #else
    // C++98 / C++03 / C++11 / C++14
    struct ContextVal {
        int64       lVal;
        std::string sVal;
        bool        isString;

        ContextVal() : lVal(0), isString(false) {}
        ContextVal(int64 v) : lVal(v), isString(false) {}
        ContextVal(const std::string& s) : lVal(0), sVal(s), isString(true) {}
    };
   #endif


   class Context {
   public:

   #ifdef CONCURRENCY_ENABLED
       Context(ThreadPool* p = nullptr) : _pool(p) {}
       Context(const Context& c) : _map(c._map), _pool(c._pool) {}
       Context(const Context& c, ThreadPool* p) : _map(c._map), _pool(p) {}
       Context& operator=(const Context& c) = default;
   #else
       Context() {}
       Context(const Context& c) : _map(c._map) {}
       Context& operator=(const Context& c) { _map = c._map; return *this; }
   #endif

       ~Context() {}

       bool has(const std::string& key) const;

       int getInt(const std::string& key, int defValue = 0) const;
       int64 getLong(const std::string& key, int64 defValue = 0) const;
       std::string getString(const std::string& key,
                             const std::string& defValue = "") const;

       void putInt(const std::string& key, int value);
       void putLong(const std::string& key, int64 value);
       void putString(const std::string& key, const std::string& value);

   #ifdef CONCURRENCY_ENABLED
       ThreadPool* getPool() const { return _pool; }
   #endif

   private:
       std::map<std::string, ContextVal> _map;

   #ifdef CONCURRENCY_ENABLED
       ThreadPool* _pool;
   #endif
   };


   inline bool Context::has(const std::string& key) const
   {
       return _map.find(key) != _map.end();
   }


   inline int Context::getInt(const std::string& key, int defValue) const
   {
       return int(getLong(key, defValue));
   }


   inline int64 Context::getLong(const std::string& key, int64 defValue) const
   {
       const std::map<std::string, ContextVal>::const_iterator it = _map.find(key);

       if (it == _map.end())
          return defValue;

   #if __cplusplus >= 201703L
       if (std::holds_alternative<int64>(it->second))
           return std::get<int64>(it->second);

       return defValue;
   #else
       return it->second.isString ? defValue : it->second.lVal;
   #endif
   }


   inline std::string Context::getString(const std::string& key,
                                         const std::string& defValue) const
   {
       const std::map<std::string, ContextVal>::const_iterator it = _map.find(key);

       if (it == _map.end())
           return defValue;

   #if __cplusplus >= 201703L
       if (std::holds_alternative<std::string>(it->second))
           return std::get<std::string>(it->second);

       return defValue;
   #else
       return it->second.isString ? it->second.sVal : defValue;
   #endif
   }


   inline void Context::putInt(const std::string& key, int value)
   {
   #if __cplusplus >= 201703L
       _map[key] = int64(value);
   #else
       _map[key] = ContextVal((int64)value);
   #endif
   }


   inline void Context::putLong(const std::string& key, int64 value)
   {
   #if __cplusplus >= 201703L
       _map[key] = value;
   #else
       _map[key] = ContextVal(value);
   #endif
   }

   inline void Context::putString(const std::string& key, const std::string& value)
   {
   #if __cplusplus >= 201703L
       _map[key] = value;
   #else
       _map[key] = ContextVal(value);
   #endif
   }

}
#endif



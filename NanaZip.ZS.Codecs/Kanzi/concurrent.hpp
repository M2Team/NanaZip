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
#ifndef knz_concurrent
#define knz_concurrent

#include "types.hpp"

#if __cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1700)
    // C++ 11 (or partial)
    #include <atomic>
    #define HAVE_STD_ATOMICS 1

    #ifndef CONCURRENCY_DISABLED
        #ifdef __clang__
            // Process clang first because it may define __GNUC__ with an old version
            #define CONCURRENCY_ENABLED
        #elif __GNUC__
            // Require g++ 5.0 minimum, 4.8.4 generates exceptions on futures (?)
            #if ((__GNUC__ << 16) + __GNUC_MINOR__ >= (5 << 16) + 0)
                #define CONCURRENCY_ENABLED
            #endif
        #else
            #define CONCURRENCY_ENABLED
        #endif
    #endif
#else
    #define HAVE_STD_ATOMICS 0
#endif


#ifdef CONCURRENCY_ENABLED
   #include <vector>
   #include <queue>
   #include <memory>
   #include <thread>
   #include <mutex>
   #include <condition_variable>
   #include <future>
   #include <functional>
   #if __cplusplus >= 201703L
   #include <tuple>
   #endif
   #include <stdexcept>

   #ifdef __x86_64__
      #ifdef __clang__
          #define CPU_PAUSE() __builtin_ia32_pause()
      #elif __GNUC__
          #define CPU_PAUSE() __builtin_ia32_pause()
      #elif _MSC_VER
          #include <immintrin.h>
          #define CPU_PAUSE() _mm_pause()
      #else
         #define CPU_PAUSE() std::this_thread::yield();
      #endif
   #else
      #define CPU_PAUSE() std::this_thread::yield();
   #endif
#else
   #define CPU_PAUSE()
#endif


template <class T>
class Task {
    public:
        Task() {}
        virtual ~Task() {}
        virtual T run() = 0;
};


#ifdef CONCURRENCY_ENABLED
   class ThreadPool FINAL {
   public:
       ThreadPool(int threads = 8);

       template<class F, class... Args>
#if __cplusplus >= 201703L // result_of deprecated from C++17
       std::future<typename std::invoke_result_t<F, Args...>> schedule(F&& f, Args&&... args);
#else
       std::future<typename std::result_of<F(Args...)>::type> schedule(F&& f, Args&&... args);
#endif

       ~ThreadPool() noexcept;

   private:
       std::vector<std::thread> _workers;
       std::queue<std::function<void()>> _tasks;
       std::mutex _mutex;
       std::condition_variable _condition;
       bool _stop;
   };


   inline ThreadPool::ThreadPool(int threads)
       :   _stop(false)
   {
       if ((threads <= 0) || (threads > 1024))
           throw std::invalid_argument("The number of threads must be in [1..1024]");

       // Start and run threads
       for (int i = 0; i < threads; i++)
           _workers.emplace_back(
               [this]
               {
                   for(;;)
                   {
                       std::function<void()> task;

                       {
                           std::unique_lock<std::mutex> lock(_mutex);
                           _condition.wait(lock,
                               [this] { return _stop || !_tasks.empty(); });

                           if (_stop && _tasks.empty())
                               return;

                           task = std::move(_tasks.front());
                           _tasks.pop();
                       }

                       task();
                   }
               }
           );
   }


   template<class F, class... Args>
#if __cplusplus >= 201703L // result_of deprecated from C++17
   std::future<typename std::invoke_result_t<F, Args...> > ThreadPool::schedule(F&& f, Args&&... args)
   {
       using return_type = typename std::invoke_result<F, Args...>::type;
#else
   std::future<typename std::result_of<F(Args...)>::type> ThreadPool::schedule(F&& f, Args&&... args)
   {
       using return_type = typename std::result_of<F(Args...)>::type;
#endif

       #if __cplusplus >= 201703L
       auto task = std::make_shared<std::packaged_task<return_type()>>(
           [fn = std::forward<F>(f), params = std::make_tuple(std::forward<Args>(args)...)]() mutable -> return_type {
               return std::apply(std::move(fn), std::move(params));
           }
       );
       #else
       auto task = std::make_shared<std::packaged_task<return_type()>>(
           std::bind(std::forward<F>(f), std::forward<Args>(args)...)
       );
       #endif

       std::future<return_type> res = task->get_future();

       {
           std::unique_lock<std::mutex> lock(_mutex);

           if (_stop == true)
               throw std::runtime_error("ThreadPool stopped");

           _tasks.emplace([task](){ (*task)(); });
       }

       _condition.notify_one();
       return res;
   }


   // the destructor joins all threads
   inline ThreadPool::~ThreadPool() noexcept
   {
       {
           std::unique_lock<std::mutex> lock(_mutex);
           _stop = true;
       }

       _condition.notify_all();

       for (std::thread& w : _workers)
           w.join();
   }



    template<class T>
    class BoundedConcurrentQueue {
    public:
        BoundedConcurrentQueue(int nbItems, T* data) : _index(0), _size(nbItems), _data(data) {}

        ~BoundedConcurrentQueue() { }

        T* get() { int idx = _index.fetch_add(1, std::memory_order_acq_rel); return (idx >= _size) ? nullptr : &_data[idx]; }

        void clear() { _index.store(_size); }

    private:
        std::atomic_int _index;
        int _size;
        T* _data;
    };

#endif


#if HAVE_STD_ATOMICS
    typedef std::atomic_int atomic_int_t;

    #define LOAD_ATOMIC(a)  ((a).load(std::memory_order_acquire))
    #define STORE_ATOMIC(a, v) ((a).store((v), std::memory_order_release))
    #define EXCHANGE_ATOMIC(a, v)  ((a).exchange((v), std::memory_order_acq_rel))
    #define FETCH_ADD_ATOMIC(a, v) ((a).fetch_add((v), std::memory_order_acq_rel))
    #define COMPARE_EXCHANGE_ATOMIC(obj, expected, desired) \
                                    ((obj).compare_exchange_strong((expected), (desired), \
                                    std::memory_order_release, std::memory_order_acquire))

#else

    typedef int atomic_int_t;
    #define LOAD_ATOMIC(a) (a)
    #define STORE_ATOMIC(a, v)  ((a) = (v))
    #define EXCHANGE_ATOMIC(a, v)  exchange_atomic_int((a), (v))
    #define FETCH_ADD_ATOMIC(a, v)  fetch_add_atomic_int((a), (v))
    #define COMPARE_EXCHANGE_ATOMIC(obj, expected, desired) \
                                    compare_exchange_fallback((obj), (expected), (desired))

    inline int exchange_atomic_int(int& a, int v)
    {
        int old = a;
        a = v;
        return old;
    }

    inline int fetch_add_atomic_int(int& a, int v)
    {
        int old = a;
        a += v;
        return old;
    }

    inline bool compare_exchange_fallback(int& obj, int& expected, int desired)
    {
        if (obj == expected) {
            obj = desired;
            return true;
        } else {
            expected = obj; // update expected on failure
            return false;
        }
    }

#endif


#endif

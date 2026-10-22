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
#ifndef knz_CompressedOutputStream
#define knz_CompressedOutputStream


#include <string>
#include <vector>
#include "../concurrent.hpp"
#include "../Context.hpp"
#include "../Event.hpp"
#include "../Listener.hpp"
#include "../OutputStream.hpp"
#include "../SliceArray.hpp"
#include "../bitstream/DefaultOutputBitStream.hpp"
#include "../util/XXHash.hpp"

#if __cplusplus >= 201103L
   #include <functional>
#endif

#ifdef CONCURRENCY_ENABLED
#include <future>
#endif

namespace kanzi {

   class EncodingTaskResult FINAL {
   public:
       int _blockId;
       int _error; // 0 = OK
       std::string _msg;

       EncodingTaskResult()
       {
           _blockId = -1;
           _error = 0;
       }

       EncodingTaskResult(int blockId, int error, const std::string& msg)
           : _blockId(blockId)
           , _error(error)
           , _msg(msg)
       {
       }

       EncodingTaskResult(const EncodingTaskResult& result)
           : _blockId(result._blockId)
           , _error(result._error)
           , _msg(result._msg)
       {
       }

       EncodingTaskResult& operator = (const EncodingTaskResult& result)
       {
           _msg = result._msg;
           _blockId = result._blockId;
           _error = result._error;
           return *this;
       }

#if __cplusplus >= 201103L
       EncodingTaskResult(EncodingTaskResult&& other) noexcept
           : _blockId(other._blockId)
           , _error(other._error)
           , _msg(std::move(other._msg)) // Transfer ownership of string buffer
       {
       }

       // Move Assignment Operator
       EncodingTaskResult& operator=(EncodingTaskResult&& other) noexcept
       {
           if (this != &other) {
               _blockId = other._blockId;
               _error = other._error;
               _msg = std::move(other._msg); // Transfer ownership of string buffer
           }

           return *this;
       }
#endif

       ~EncodingTaskResult() {}
   };

   // A task used to encode a block
   // Several tasks (transform+entropy) may run in parallel
   template <class T>
   class EncodingTask FINAL : public Task<T> {
   private:
       SliceArray<byte>* _data;
       SliceArray<byte>* _buffer;
       DefaultOutputBitStream* _obs;
       XXHash32* _hasher32;
       XXHash64* _hasher64;
#ifdef CONCURRENCY_ENABLED
       std::mutex* _blockMutex;
       std::condition_variable* _blockCondition;
#endif
       atomic_int_t* _processedBlockId;
       std::vector<Listener<Event>*> _listeners;
       Context _ctx;

       void storeProcessedBlockId(int value);

       void fetchAddProcessedBlockId();

   public:
       EncodingTask(SliceArray<byte>* iBuffer, SliceArray<byte>* oBuffer,
           DefaultOutputBitStream* obs, XXHash32* hasher32, XXHash64* hasher64,
#ifdef CONCURRENCY_ENABLED
           std::mutex* blockMutex, std::condition_variable* blockCondition,
#endif
           atomic_int_t* processedBlockId, std::vector<Listener<Event>*>& listeners,
           const Context& ctx);

       ~EncodingTask(){}

       T run();
   };

   class CompressedOutputStream : public OutputStream {
       friend class EncodingTask<EncodingTaskResult>;

   public:
       CompressedOutputStream(OutputStream& os,
                   int jobs = 1,
                   const std::string& entropy = "NONE",
                   const std::string& transform = "NONE",
                   int blockSize = 4*1024*1024,
                   int checksum = 0,
                   uint64 originalSize = 0,
#ifdef CONCURRENCY_ENABLED
                   ThreadPool* pool = nullptr,
#endif
                   bool headerless = false);

       CompressedOutputStream(OutputStream& os, Context& ctx, bool headerless = false);

       ~CompressedOutputStream();

       bool addListener(Listener<Event>& bl);

       bool removeListener(Listener<Event>& bl);

       std::ostream& write(const char* s, std::streamsize n);

       std::ostream& put(char c);

       std::ostream& flush();

       std::streampos tellp();

       std::ostream& seekp(std::streampos pos);

       void close();

       uint64 getWritten() const { return (_obs->written() + 7) >> 3; }


  protected:

       void writeHeader();


   private:
       static const int BITSTREAM_TYPE;
       static const int BITSTREAM_FORMAT_VERSION;
       static const int DEFAULT_BUFFER_SIZE;
       static const byte COPY_BLOCK_MASK;
       static const byte TRANSFORMS_MASK;
       static const int MIN_BITSTREAM_BLOCK_SIZE;
       static const int MAX_BITSTREAM_BLOCK_SIZE;
       static const int SMALL_BLOCK_SIZE;
       static const int CANCEL_TASKS_ID;
       static const int MAX_CONCURRENCY;

       static inline uint32 mix32(uint32 checksum, uint32 hash, uint32 value) {
           checksum ^= hash * uint32(~value);
           checksum = (checksum << 13) | (checksum >> 19);
           return checksum * 5 + 0x52DCE729;
       }

       int _blockSize;
       int _bufferId; // index of current write buffer
       int _jobs;
       int _bufferThreshold;
       int _nbInputBlocks;
       int64 _inputSize;
       XXHash32* _hasher32;
       XXHash64* _hasher64;
       SliceArray<byte>** _buffers; // input & output per block
       short _entropyType;
       uint64 _transformType;
       DefaultOutputBitStream* _obs;
       atomic_int_t _initialized;
       atomic_int_t _closed;
       atomic_int_t _blockId;
       atomic_int_t _inputBlockId; // Counter for input blocks
       std::vector<Listener<Event>*> _listeners;
       std::vector<int> _jobsPerTask;
       Context _ctx;
       bool _headless;

#ifdef CONCURRENCY_ENABLED
       ThreadPool* _pool;
       std::vector<std::future<EncodingTaskResult> > _futures; // Futures for async tasks
       std::mutex _blockMutex;
       std::condition_variable _blockCondition;

       // Drain all tasks after an asynchronous failure before releasing shared resources.
       void drainTasks() noexcept;
#endif

       void processBuffer();

       void submitBlock();

       static void notifyListeners(std::vector<Listener<Event>*>& listeners, const Event& evt);
   };


   inline std::streampos CompressedOutputStream::tellp()
   {
       throw std::ios_base::failure("Not supported");
   }

   inline std::ostream& CompressedOutputStream::seekp(std::streampos)
   {
       throw std::ios_base::failure("Not supported");
   }

   inline std::ostream& CompressedOutputStream::flush()
   {
       // NOOP: let the underlying output stream flush itself when needed
       return *this;
   }
}
#endif

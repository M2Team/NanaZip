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

#include <limits>
#include <sstream>
#include "CompressedInputStream.hpp"
#include "IOException.hpp"
#include "../Error.hpp"
#include "../entropy/EntropyDecoderFactory.hpp"
#include "../transform/TransformFactory.hpp"
#include "../util/fixedbuf.hpp"

using namespace kanzi;
using namespace std;

// Clear ownership before delete so a throwing destructor cannot leave a
// dangling non-null pointer for later cleanup.
#define DELETE_AND_NULL(TYPE, PTR) \
    do { \
        TYPE* knzTmpPtr = (PTR); \
        (PTR) = nullptr; \
        delete knzTmpPtr; \
    } while (0)

const int CompressedInputStream::BITSTREAM_TYPE = 0x4B414E5A; // "KANZ"
const int CompressedInputStream::BITSTREAM_FORMAT_VERSION = 7;
const int CompressedInputStream::DEFAULT_BUFFER_SIZE = 256 * 1024;
const int CompressedInputStream::EXTRA_BUFFER_SIZE = 512;
const kanzi::byte CompressedInputStream::COPY_BLOCK_MASK = kanzi::byte(0x80);
const kanzi::byte CompressedInputStream::TRANSFORMS_MASK = kanzi::byte(0x10);
const int CompressedInputStream::MIN_BITSTREAM_BLOCK_SIZE = 1024;
const int CompressedInputStream::MAX_BITSTREAM_BLOCK_SIZE = 1024 * 1024 * 1024;
const int CompressedInputStream::TRANSFORMED_COPY_VERSION = 7;
const int CompressedInputStream::CANCEL_TASKS_ID = -1;
const int CompressedInputStream::MAX_CONCURRENCY = 64;
const int CompressedInputStream::MAX_BLOCK_ID = int((uint(1) << 31) - 1);


CompressedInputStream::CompressedInputStream(InputStream& is,
                   int tasks,
                   const string& entropy,
                   const string& transform,
                   int blockSize,
                   int checksum,
                   uint64 originalSize,
#ifdef CONCURRENCY_ENABLED
                   ThreadPool* pool,
#endif
                   bool headerless,
                   int bsVersion)
    : InputStream(is.rdbuf())
    , _parentCtx(nullptr)
{
#ifdef CONCURRENCY_ENABLED
    if ((tasks <= 0) || (tasks > MAX_CONCURRENCY)) {
        stringstream ss;
        ss << "The number of jobs must be in [1.." << MAX_CONCURRENCY << "], got " << tasks;
        throw invalid_argument(ss.str());
    }

    _pool = pool; // may be null
#else
    if (tasks != 1)
        throw invalid_argument("The number of jobs is limited to 1 in this version");
#endif

    _hasher32 = nullptr;
    _hasher64 = nullptr;
    _blockId = 0;
    _bufferId = 0;
    _maxBufferId = 0;
    _submitBlockId = 0;
    _blockSize = blockSize;
    _bufferThreshold = 0;
    _available = 0;
    _entropyType = EntropyDecoderFactory::getType(entropy.c_str()); // throws on error
    _transformType = TransformFactory<kanzi::byte>::getType(transform.c_str()); // throws on error
    _initialized = 0;
    _closed = 0;
    _gcount = 0;
    _ibs = new DefaultInputBitStream(is, DEFAULT_BUFFER_SIZE);
    _jobs = tasks;
    _outputSize = originalSize;
    _nbInputBlocks = 0;
    _buffers = new SliceArray<kanzi::byte>*[2 * _jobs];
    _headless = headerless;
    _consumeBlockId = 0;

    if (_headless == true) {
       if ((_blockSize < MIN_BITSTREAM_BLOCK_SIZE) || (_blockSize > MAX_BITSTREAM_BLOCK_SIZE)) {
           stringstream ss;
           ss << "Invalid or missing block size: " << _blockSize;
           throw invalid_argument(ss.str());
       }

       _ctx.putInt("bsVersion", bsVersion);
       _ctx.putString("entropy", entropy);
       _ctx.putString("transform", transform);
       _ctx.putInt("blockSize", blockSize);

       if (checksum == 32) {
          _hasher32 = new XXHash32(BITSTREAM_TYPE);
          _hasher64 = nullptr;
       }
       else if (checksum == 64) {
          _hasher32 = nullptr;
          _hasher64 = new XXHash64(BITSTREAM_TYPE);
       }
       else if (checksum != 0) {
           throw invalid_argument("The block checksum size must be 0, 32 or 64");
       }
    }

    _jobsPerTask.resize(_jobs);
    std::fill(_jobsPerTask.begin(), _jobsPerTask.end(), 1);

#ifdef CONCURRENCY_ENABLED
    _futures.resize(_jobs);
#else
    _results.resize(_jobs);
#endif

    for (int i = 0; i < 2 * _jobs; i++)
        _buffers[i] = new SliceArray<kanzi::byte>(nullptr, 0, 0);
}

CompressedInputStream::CompressedInputStream(InputStream& is, Context& ctx, bool headerless)
    : InputStream(is.rdbuf())
    , _ctx(ctx)
    , _parentCtx(&ctx)
{
    int tasks = _ctx.getInt("jobs", 1);

#ifdef CONCURRENCY_ENABLED
    if ((tasks <= 0) || (tasks > MAX_CONCURRENCY)) {
        stringstream ss;
        ss << "The number of jobs must be in [1.." << MAX_CONCURRENCY << "], got " << tasks;
        throw invalid_argument(ss.str());
    }

    _pool = _ctx.getPool(); // may be null
#else
    if (tasks != 1)
        throw invalid_argument("The number of jobs is limited to 1 in this version");
#endif
    _blockId = 0;
    _bufferId = 0;
    _maxBufferId = 0;
    _submitBlockId = 0;
    _blockSize = 0;
    _bufferThreshold = 0;
    _available = 0;
    _entropyType = EntropyDecoderFactory::NONE_TYPE;
    _transformType = TransformFactory<kanzi::byte>::NONE_TYPE;
    _initialized = 0;
    _closed = 0;
    _gcount = 0;
    _ibs = new DefaultInputBitStream(is, DEFAULT_BUFFER_SIZE);
    _jobs = tasks;
    _hasher32 = nullptr;
    _hasher64 = nullptr;
    _outputSize = 0;
    _nbInputBlocks = 0;
    _headless = headerless;
    _consumeBlockId = 0;

    if (_headless == true) {
        // Validation of required values
        // Optional bsVersion
        const int bsVersion = _ctx.getInt("bsVersion", BITSTREAM_FORMAT_VERSION);

        if (bsVersion > BITSTREAM_FORMAT_VERSION) {
            stringstream ss;
            ss << "Invalid or missing bitstream version, cannot read this version of the stream: " << bsVersion;
            throw invalid_argument(ss.str());
        }

        _ctx.putInt("bsVersion", bsVersion);
        string entropy = _ctx.getString("entropy");
        _entropyType = EntropyDecoderFactory::getType(entropy.c_str()); // throws on error

        string transform = _ctx.getString("transform");
        _transformType = TransformFactory<kanzi::byte>::getType(transform.c_str()); // throws on error

        _blockSize = _ctx.getInt("blockSize", 0);

        if ((_blockSize < MIN_BITSTREAM_BLOCK_SIZE) || (_blockSize > MAX_BITSTREAM_BLOCK_SIZE)) {
            stringstream ss;
            ss << "Invalid or missing block size: " << _blockSize;
            throw invalid_argument(ss.str());
        }

        _bufferThreshold = _blockSize;

        // Optional outputSize
        if (_ctx.has("outputSize")) {
            _outputSize = _ctx.getLong("outputSize", 0);

            if ((_outputSize < 0) || (_outputSize >= (int64(1) << 48)))
                _outputSize = 0; // not provided
        }

        const int nbBlocks = int((_outputSize + int64(_blockSize - 1)) / int64(_blockSize));
        _nbInputBlocks = min(nbBlocks, MAX_CONCURRENCY - 1);

        // Optional checksum
        int checksum = ctx.getInt("checksum", 0);

        if (checksum == 0) {
            _hasher32 = nullptr;
            _hasher64 = nullptr;
        }
        else if (checksum == 32) {
            _hasher32 = new XXHash32(BITSTREAM_TYPE);
            _hasher64 = nullptr;
        }
        else if (checksum == 64) {
            _hasher32 = nullptr;
            _hasher64 = new XXHash64(BITSTREAM_TYPE);
        }
        else {
            throw invalid_argument("The block checksum size must be 0, 32 or 64");
        }
    }

    _jobsPerTask.resize(_jobs);
    std::fill(_jobsPerTask.begin(), _jobsPerTask.end(), 1);

#ifdef CONCURRENCY_ENABLED
    _futures.resize(_jobs);
#else
    _results.resize(_jobs);
#endif

    _buffers = new SliceArray<kanzi::byte>*[2 * _jobs];

    for (int i = 0; i < 2 * _jobs; i++)
        _buffers[i] = new SliceArray<kanzi::byte>(nullptr, 0, 0);
}

CompressedInputStream::~CompressedInputStream()
{
    try {
        close();
    }
    catch (const exception&) {
        // Ignore and continue
    }

    for (int i = 0; i < 2 * _jobs; i++) {
        if (_buffers[i]->_array != nullptr)
            delete[] _buffers[i]->_array;

        delete _buffers[i];
    }

    delete[] _buffers;
    delete _ibs;

    if (_hasher32 != nullptr) {
        delete _hasher32;
        _hasher32 = nullptr;
    }

    if (_hasher64 != nullptr) {
        delete _hasher64;
        _hasher64 = nullptr;
    }
}

void CompressedInputStream::submitBlock(int bufferId)
{
    const int blkSize = max(_blockSize + EXTRA_BUFFER_SIZE, _blockSize + (_blockSize >> 4));

    if (_buffers[bufferId]->_length < blkSize) {
        if (_buffers[bufferId]->_array != nullptr)
           delete[] _buffers[bufferId]->_array;

        _buffers[bufferId]->_array = new kanzi::byte[blkSize];
        _buffers[bufferId]->_length = blkSize;
    }

    Context copyCtx(_ctx);
    copyCtx.putLong("tType", _transformType);
    copyCtx.putInt("eType", _entropyType);
    copyCtx.putInt("blockId", _submitBlockId + 1);
    copyCtx.putInt("jobs", _jobsPerTask[bufferId]);
    copyCtx.putInt("tasks", _jobs);

    _buffers[bufferId]->_index = 0;
    _buffers[_jobs + bufferId]->_index = 0;

    DecodingTask<DecodingTaskResult>* task = new DecodingTask<DecodingTaskResult>(
        _buffers[bufferId],
        _buffers[_jobs + bufferId],
        blkSize,
        _ibs, _hasher32, _hasher64,
#ifdef CONCURRENCY_ENABLED
        &_blockMutex, &_blockCondition,
#endif
        &_blockId,
        _listeners, copyCtx);

#ifdef CONCURRENCY_ENABLED
    std::shared_ptr<DecodingTask<DecodingTaskResult>> safeTask(task);

    auto taskRunner = [safeTask]() {
        return safeTask->run();
    };

    if (_pool == nullptr) {
        // std::async returns std::future<DecodingTaskResult>
        _futures[bufferId] = std::async(std::launch::async, taskRunner);
    }
    else {
        // pool->schedule returns std::future<DecodingTaskResult>
        _futures[bufferId] = _pool->schedule(taskRunner);
    }
#else
    // Synchronous execution
    try {
        _results[bufferId] = task->run();
	delete task;
    } catch (...) {
	delete task;
        throw;
    }
#endif

    _submitBlockId++;
}


int CompressedInputStream::_get(int inc)
{
    try {
        if (LOAD_ATOMIC(_initialized) == 0) {
             readHeader();

             for (int i = 0; i < _jobs; i++)
                submitBlock(i);
        }

        if (_available == 0) {
            if (LOAD_ATOMIC(_closed) == 1)
                throw ios_base::failure("Stream closed");

            DecodingTaskResult res;

#ifdef CONCURRENCY_ENABLED
            if (_futures[_bufferId].valid()) {
                 res = _futures[_bufferId].get();
            } else {
                 setstate(ios::eofbit);
                 return EOF;
            }
#else
            res = _results[_bufferId];
#endif
            if (res._error != 0)
                throw IOException(res._msg, res._error);

            if (res._decoded > _blockSize) {
                stringstream ss;
                ss << "Block " << res._blockId << " incorrectly decompressed";
                throw IOException(ss.str(), Error::ERR_PROCESS_BLOCK);
            }

            // Fire events
            if (!_listeners.empty()) {
                Event::HashType hashType = Event::NO_HASH;

                if (_hasher32 != nullptr)
                    hashType = Event::SIZE_32;
                else if (_hasher64 != nullptr)
                    hashType = Event::SIZE_64;

                Event evt(Event::AFTER_TRANSFORM, res._blockId,
                    int64(res._decoded), res._completionTime, res._checksum, hashType);
                CompressedInputStream::notifyListeners(_listeners, evt);
            }

            _available = res._decoded;

            if (_available == 0) {
                if (res._skipped == false) {
                    setstate(ios::eofbit);
                    return EOF;
                }

                submitBlock(_bufferId);
                _bufferId = (_bufferId + 1) % _jobs;
                _consumeBlockId++;
            }

            _buffers[_bufferId]->_index = 0;
        }

        int res = int(_buffers[_bufferId]->_array[_buffers[_bufferId]->_index]);

        if (inc == 0)
            return res;

        _available -= inc;
        _buffers[_bufferId]->_index += inc;

        if (_available == 0) {
            submitBlock(_bufferId);
            _bufferId = (_bufferId + 1) % _jobs;
            _consumeBlockId++;
        }

        return res;
    }
    catch (const IOException&) {
        setstate(ios::badbit);
        throw;
    }
    catch (const exception&) {
        setstate(ios::badbit);
        throw;
    }
    catch (...) {
        setstate(ios::badbit);
        throw;
    }
}


istream& CompressedInputStream::read(char* data, streamsize length)
{
    if (length < 0)
        throw ios_base::failure("Invalid buffer size");

    try {
        streamsize remaining = length;

        _gcount = 0;

        while (remaining > 0) {
            // Reuse _get(0) logic implicitly
            if (LOAD_ATOMIC(_initialized) == 0) {
                 readHeader();

                 for (int i = 0; i < _jobs; i++)
                     submitBlock(i);
            }

            if (_available == 0) {
                DecodingTaskResult res;
#ifdef CONCURRENCY_ENABLED
                if (_futures[_bufferId].valid()) {
                     res = _futures[_bufferId].get();
                } else {
                     setstate(ios::eofbit);
                     break;
                }
#else
                res = _results[_bufferId];
#endif
                if (res._error != 0)
                    throw IOException(res._msg, res._error);

                if (res._decoded > _blockSize) {
                    stringstream ss;
                    ss << "Block " << res._blockId << " incorrectly decompressed";
                    throw IOException(ss.str(), Error::ERR_PROCESS_BLOCK);
                }

                if (!_listeners.empty()) {
                    Event::HashType hashType = Event::NO_HASH;

                    if (_hasher32 != nullptr)
                        hashType = Event::SIZE_32;
                    else if (_hasher64 != nullptr)
                        hashType = Event::SIZE_64;

                    Event evt(Event::AFTER_TRANSFORM, res._blockId,
                        int64(res._decoded), res._completionTime, res._checksum, hashType);
                    CompressedInputStream::notifyListeners(_listeners, evt);
                }

                _available = res._decoded;
                _buffers[_bufferId]->_index = 0;

                if ((_available == 0) && (res._skipped == false)) {
                   setstate(ios::eofbit);
                   break;
                }
            }

            const streamsize lenChunk = min(remaining, streamsize(_available));

            if (lenChunk > 0) {
                memcpy(&data[_gcount], &_buffers[_bufferId]->_array[_buffers[_bufferId]->_index], size_t(lenChunk));
                _buffers[_bufferId]->_index += int(lenChunk);
                _gcount += lenChunk;
                remaining -= lenChunk;
                _available -= lenChunk;
            }

            if (_available == 0) {
                submitBlock(_bufferId);
                _bufferId = (_bufferId + 1) % _jobs;
                _consumeBlockId++;
            }
        }

        return *this;
    }
    catch (const IOException&) {
        setstate(ios::badbit);
        throw;
    }
    catch (const exception&) {
        setstate(ios::badbit);
        throw;
    }
    catch (...) {
        setstate(ios::badbit);
        throw;
    }
}


void CompressedInputStream::readHeader()
{
    if (EXCHANGE_ATOMIC(_initialized, 1) == 1)
        return;

    if (_headless == true)
        return;

    // Read stream type
    const int type = int(_ibs->readBits(32));

    // Sanity check
    if (type != BITSTREAM_TYPE) {
        throw IOException("Invalid stream type", Error::ERR_INVALID_FILE);
    }

    // Read stream version
    const int bsVersion = int(_ibs->readBits(4));

    // Sanity check
    if (bsVersion > BITSTREAM_FORMAT_VERSION) {
        stringstream ss;
        ss << "Invalid bitstream, cannot read this version of the stream: " << bsVersion;
        throw IOException(ss.str(), Error::ERR_STREAM_VERSION);
    }

    _ctx.putInt("bsVersion", bsVersion);
    uint64 ckSize = 0;

    // Read block checksum
    if (bsVersion >= 6) {
        ckSize = _ibs->readBits(2);

        if (ckSize == 1) {
            _hasher32 = new XXHash32(BITSTREAM_TYPE);
        }
        else if (ckSize == 2) {
            _hasher64 = new XXHash64(BITSTREAM_TYPE);
        }
        else if (ckSize == 3) {
           throw IOException("Invalid bitstream, incorrect block checksum size",
               Error::ERR_INVALID_FILE);
        }
    }
    else {
       if (_ibs->readBit() == 1)
           _hasher32 = new XXHash32(BITSTREAM_TYPE);
    }

    try {
        // Read entropy codec
        _entropyType = short(_ibs->readBits(5));
        _ctx.putString("entropy", EntropyDecoderFactory::getName(_entropyType));
    }
    catch (const invalid_argument&) {
        stringstream err;
        err << "Invalid bitstream, unknown entropy type: " << _entropyType;
        throw IOException(err.str(), Error::ERR_INVALID_CODEC);
    }

    try {
        // Read transform: 8*6 bits
        _transformType = _ibs->readBits(48);
        _ctx.putString("transform", TransformFactory<kanzi::byte>::getName(_transformType));
    }
    catch (const invalid_argument&) {
        stringstream err;
        err << "Invalid bitstream, unknown transform type: " << _transformType;
        throw IOException(err.str(), Error::ERR_INVALID_CODEC);
    }

    // Read block size
    _blockSize = int(_ibs->readBits(28) << 4);
    _ctx.putInt("blockSize", _blockSize);
    _bufferThreshold = _blockSize;

    if ((_blockSize < MIN_BITSTREAM_BLOCK_SIZE) || (_blockSize > MAX_BITSTREAM_BLOCK_SIZE)) {
        stringstream ss;
        ss << "Invalid bitstream, incorrect block size: " << _blockSize;
        throw IOException(ss.str(), Error::ERR_BLOCK_SIZE);
    }

    // Read original size
    // 0 -> not provided, <2^16 -> 1, <2^32 -> 2, <2^48 -> 3
    const int szMask = int(_ibs->readBits(2));

    if (szMask != 0) {
        _outputSize = _ibs->readBits(16 * szMask);

        if (_parentCtx != nullptr)
            _parentCtx->putLong("outputSize", _outputSize);

        const int nbBlocks = int((_outputSize + int64(_blockSize - 1)) / int64(_blockSize));
        _nbInputBlocks = min(nbBlocks, MAX_CONCURRENCY - 1);
    }

    if (bsVersion >= 6) {
       // Padding
       _ibs->readBits(15);
    }

    // Assign optimal number of tasks and jobs per task (if the number of blocks is available)
    if (_jobs > 1) {
        // Limit the number of tasks if there are fewer blocks that _jobs
        int nbTasks = (_nbInputBlocks != 0) ? min(_nbInputBlocks, _jobs) : _jobs;
        Global::computeJobsPerTask(&_jobsPerTask[0], _jobs, nbTasks);
    }
    else {
        _jobsPerTask[0] = 1;
    }

    // Read & verify checksum
    const int crcSize = bsVersion <= 5 ? 16 : 24;
    const uint32 cksum1 = uint32(_ibs->readBits(crcSize));
    uint32 seed = (bsVersion >= 6 ? 0x01030507 : 1) * uint32(bsVersion);
    const uint32 HASH = 0x1E35A7BD;
    uint32 cksum2 = HASH * seed;

    typedef uint32 (*Mix32Function)(uint32, uint32, uint32);
    const Mix32Function mix32 = (bsVersion >= 7) ? CompressedInputStream::mix32_v7
        : CompressedInputStream::mix32_v6;

    if (bsVersion >= 6)
        cksum2 = mix32(cksum2, HASH, uint32(ckSize));

    cksum2 = mix32(cksum2, HASH, uint32(_entropyType));
    cksum2 = mix32(cksum2, HASH, uint32(_transformType >> 32));
    cksum2 = mix32(cksum2, HASH, uint32(_transformType));
    cksum2 = mix32(cksum2, HASH, uint32(_blockSize));

    if (szMask != 0) {
        cksum2 = mix32(cksum2, HASH, uint32(_outputSize >> 32));
        cksum2 = mix32(cksum2, HASH, uint32(_outputSize));
    }

    cksum2 = (cksum2 >> 23) ^ (cksum2 >> 3);

    if (cksum1 != (cksum2 & ((1 << crcSize) - 1)))
        throw IOException("Invalid bitstream, header checksum mismatch", Error::ERR_CRC_CHECK);

    if (_listeners.size() > 0) {
        Event::HeaderInfo info;
        info.inputName = _ctx.getString("inputName", "");
        info.bsVersion = bsVersion;
        info.checksumSize = int(32 * ckSize);
        info.blockSize = _blockSize;
        info.entropyType = EntropyDecoderFactory::getName(_entropyType);
        info.transformType = TransformFactory<kanzi::byte>::getName(_transformType);
        int64 fileSize = _ctx.getLong("fileSize", 0);
        info.fileSize = (fileSize >= 0) ? fileSize : -1;
        info.originalSize = (szMask != 0) ? _outputSize : -1;

        WallTimer timer;
        Event evt(Event::AFTER_HEADER_DECODING, 0, info, timer.getCurrentTime());
        notifyListeners(_listeners, evt);
    }
}


bool CompressedInputStream::addListener(Listener<Event>& bl)
{
    _listeners.push_back(&bl);
    return true;
}


bool CompressedInputStream::removeListener(Listener<Event>& bl)
{
    std::vector<Listener<Event>*>::iterator it = find(_listeners.begin(), _listeners.end(), &bl);

    if (it == _listeners.end())
        return false;

    _listeners.erase(it);
    return true;
}


void CompressedInputStream::close()
{
    if (EXCHANGE_ATOMIC(_closed, 1) == 1)
        return;

    // Signal to break the waits in DecodingTask::run immediately and
    // ensure no thread is writing to _buffers before we delete them.
#ifdef CONCURRENCY_ENABLED
    {
        std::lock_guard<std::mutex> lock(_blockMutex);
        STORE_ATOMIC(_blockId, CANCEL_TASKS_ID);
    }

    _blockCondition.notify_all();

    for (size_t i = 0; i < _futures.size(); i++) {
        if (_futures[i].valid()) {
            try {
                _futures[i].get();
            }
            catch (...) {
                // Ignore exceptions, we are closing anyway.
            }
        }
    }
#else
    STORE_ATOMIC(_blockId, CANCEL_TASKS_ID);
#endif

    try {
        _ibs->close();
    }
    catch (const BitStreamException& e) {
        throw IOException(e.what(), e.error());
    }

    _available = 0;

    // Force subsequent reads to trigger submitBlock immediately
    _bufferThreshold = 0;

    // Buffer cleanup: force error on any subsequent read attempt
    for (int i = 0; i < 2 * _jobs; i++) {
        if (_buffers[i]->_array != nullptr)
           delete[] _buffers[i]->_array;

        _buffers[i]->_array = nullptr;
        _buffers[i]->_length = 0;
        _buffers[i]->_index = 0;
    }
}


void CompressedInputStream::notifyListeners(vector<Listener<Event>*>& listeners, const Event& evt)
{
    for (vector<Listener<Event>*>::iterator it = listeners.begin(); it != listeners.end(); ++it)
        (*it)->processEvent(evt);
}

template <class T>
DecodingTask<T>::DecodingTask(SliceArray<kanzi::byte>* iBuffer, SliceArray<kanzi::byte>* oBuffer,
    int blockSize, DefaultInputBitStream* ibs, XXHash32* hasher32, XXHash64* hasher64,
#ifdef CONCURRENCY_ENABLED
    std::mutex* blockMutex, std::condition_variable* blockCondition,
#endif
    atomic_int_t* processedBlockId, vector<Listener<Event>*>& listeners,
    const Context& ctx)
    : _listeners(listeners)
    , _ctx(ctx)
{
    _blockLength = blockSize;
    _data = iBuffer;
    _buffer = oBuffer;
    _ibs = ibs;
    _hasher32 = hasher32;
    _hasher64 = hasher64;
#ifdef CONCURRENCY_ENABLED
    _blockMutex = blockMutex;
    _blockCondition = blockCondition;
#endif
    _processedBlockId = processedBlockId;
}

template <class T>
void DecodingTask<T>::storeProcessedBlockId(int value)
{
#ifdef CONCURRENCY_ENABLED
    {
        std::lock_guard<std::mutex> lock(*_blockMutex);
        STORE_ATOMIC(*_processedBlockId, value);
    }

    _blockCondition->notify_all();
#else
    STORE_ATOMIC(*_processedBlockId, value);
#endif
}

template <class T>
int DecodingTask<T>::readBlockHeader(InputBitStream* ibs, int bsVersion,
    uint64 encodedBlockLength, uint64 transformType, ParsedBlockHeader& header, string& msg)
{
    if (encodedBlockLength < 8) {
        msg = "Invalid block size";
        return Error::ERR_BLOCK_SIZE;
    }

    const kanzi::byte mode = kanzi::byte(ibs->readBits(8));
    kanzi::byte skipFlags = kanzi::byte(0);
    bool hasSkipFlags = false;
    const bool copyBlock = (mode & CompressedInputStream::COPY_BLOCK_MASK) != kanzi::byte(0);

    if (copyBlock == true) {
        if ((bsVersion >= CompressedInputStream::TRANSFORMED_COPY_VERSION) &&
            ((mode & CompressedInputStream::TRANSFORMS_MASK) != kanzi::byte(0))) {
            header._transformedCopy = true;

            if (TransformFactory<kanzi::byte>::getTransformCount(transformType) > 4)
                hasSkipFlags = true;
            else
                skipFlags = (mode << 4) | kanzi::byte(0x0F);
        }
    }
    else if ((mode & CompressedInputStream::TRANSFORMS_MASK) != kanzi::byte(0)) {
        hasSkipFlags = true;
    }
    else {
        skipFlags = (mode << 4) | kanzi::byte(0x0F);
    }

    const int dataSize = 1 + (int(mode >> 5) & 0x03);
    header._size = uint(1 + dataSize + (hasSkipFlags ? 1 : 0) + (bsVersion >= 7 ? 1 : 0));

    if (encodedBlockLength < uint64(header._size << 3)) {
        msg = "Invalid block size";
        return Error::ERR_BLOCK_SIZE;
    }

    header._bytes[0] = mode;
    uint idx = 1;

    if (hasSkipFlags == true) {
        skipFlags = kanzi::byte(ibs->readBits(8));
        header._bytes[idx++] = skipFlags;
    }

    uint32 preTransformLength = 0;

    for (int i = 0; i < dataSize; i++) {
        const kanzi::byte value = kanzi::byte(ibs->readBits(8));
        header._bytes[idx++] = value;
        preTransformLength = (preTransformLength << 8) | uint32(uint8(value));
    }

    header._skipFlags = skipFlags;
    header._preTransformLength = int(preTransformLength);
    header._rawCopy = copyBlock && !header._transformedCopy;

    if (bsVersion >= 7) {
        const uint32 headerChecksum = uint32(ibs->readBits(8));
        header._bytes[idx] = kanzi::byte(headerChecksum);
        const uint32 hash = 0x1E35A7BDu;
        uint32 cksum = hash * 0x01030507u;
        cksum = CompressedInputStream::mix32_v7(cksum, hash, uint32(uint8(mode)));
        cksum = CompressedInputStream::mix32_v7(cksum, hash, uint32(uint8(skipFlags)));
        cksum = CompressedInputStream::mix32_v7(cksum, hash, preTransformLength);
        cksum = CompressedInputStream::mix32_v7(cksum, hash,
            uint32(encodedBlockLength >> 32));
        cksum = CompressedInputStream::mix32_v7(cksum, hash,
            uint32(encodedBlockLength));
        cksum = (cksum >> 23) ^ (cksum >> 3);

        if (headerChecksum != (cksum & 0xFFu)) {
            msg = "Invalid bitstream, block header checksum mismatch";
            return Error::ERR_CRC_CHECK;
        }
    }

    return 0;
}

// Decode mode + transformed entropy coded data
// mode | 0b1yy0xxxx => raw copy block
//      | 0b1yy1xxxx => transformed copy block (version >= 7)
//      | 0b0yy00000 => size(size(block))-1
//  case 4 transforms or less
//      | 0b0001xxxx => transform sequence skip flags (1 means skip)
//  case more than 4 transforms
//      | 0b0yy00000 0bxxxxxxxx => transform sequence skip flags in next kanzi::byte (1 means skip)
template <class T>
T DecodingTask<T>::run()
{
    int blockId = _ctx.getInt("blockId");
    bool streamPerTask = _ctx.getInt("tasks") > 1;
    const int bsVersion = _ctx.getInt("bsVersion", CompressedInputStream::BITSTREAM_FORMAT_VERSION);
    uint64 tType = _ctx.getLong("tType");
    short eType = short(_ctx.getInt("eType"));

#ifdef CONCURRENCY_ENABLED
    {
        std::unique_lock<std::mutex> lock(*_blockMutex);
        _blockCondition->wait(lock, [this, blockId]() {
            const int taskId = LOAD_ATOMIC(*_processedBlockId);
            return (taskId == CompressedInputStream::CANCEL_TASKS_ID) || (taskId == blockId - 1);
        });
    }

    if (LOAD_ATOMIC(*_processedBlockId) == CompressedInputStream::CANCEL_TASKS_ID) {
        // Skip, an error occurred
        return T(*_data, blockId, 0, 0, 0, "Canceled");
    }
#endif

    uint64 checksum1 = 0;
    EntropyDecoder* ed = nullptr;
    InputBitStream* ibs = nullptr;
    TransformSequence<kanzi::byte>* transform = nullptr;

    try {
        // Read shared bitstream sequentially (each task is gated by _processedBlockId)
#if !defined(_MSC_VER) || _MSC_VER > 1500
        const uint64 blockOffset = _ibs->tell();
#endif
        const uint lr = 3 + uint(_ibs->readBits(5));
        uint64 read = _ibs->readBits(lr);

        if (read == 0) {
            storeProcessedBlockId(CompressedInputStream::CANCEL_TASKS_ID);
            return T(*_data, blockId, 0, 0, 0, "Success");
        }

        const uint64 encodedBlockBytes = (read + 7) >> 3;
        const uint64 encodedBlockLength = read;
        const int from = _ctx.getInt("from", 1);
        const int to = _ctx.getInt("to", CompressedInputStream::MAX_BLOCK_ID);
        const bool bufferBlock = (streamPerTask == true) || (blockId < from);
        const uint maxTransformedCopySize = uint(max(_blockLength + _blockLength / 2, 2048u));
        // Keep the normal bitstream cap for entropy-coded blocks, while
        // allowing transformed-copy blocks to carry bounded transform expansion.
        const uint maxEntropyTransformSize = uint(min(maxTransformedCopySize,
                                                       uint(CompressedInputStream::MAX_BITSTREAM_BLOCK_SIZE)));
        ParsedBlockHeader blockHeader;
        bool blockHeaderParsed = false;

        if ((bsVersion >= 7) && (bufferBlock == true)) {
            string msg;
            const int error = readBlockHeader(_ibs, bsVersion, encodedBlockLength,
                tType, blockHeader, msg);

            if (error != 0) {
                storeProcessedBlockId(CompressedInputStream::CANCEL_TASKS_ID);
                return T(*_data, blockId, 0, 0, error, msg);
            }

            blockHeaderParsed = true;

            const uint maxAllowedTransformSize = blockHeader._transformedCopy ?
                maxTransformedCopySize : maxEntropyTransformSize;

            if ((blockHeader._preTransformLength <= 0) ||
                (uint(blockHeader._preTransformLength) > maxAllowedTransformSize)) {
                storeProcessedBlockId(CompressedInputStream::CANCEL_TASKS_ID);
                stringstream ss;
                ss << "Invalid compressed block length: " << blockHeader._preTransformLength;
                return T(*_data, blockId, 0, 0, Error::ERR_READ_FILE, ss.str());
            }

            const uint64 checksumSize = (_hasher64 != nullptr) ? 8 :
                ((_hasher32 != nullptr) ? 4 : 0);
            const uint64 maxEncodedBlockBytes = uint64(blockHeader._preTransformLength) +
                uint64(blockHeader._size) + checksumSize;

            if (encodedBlockBytes > maxEncodedBlockBytes) {
                storeProcessedBlockId(CompressedInputStream::CANCEL_TASKS_ID);
                return T(*_data, blockId, 0, 0, Error::ERR_BLOCK_SIZE, "Invalid block size");
            }

            read -= (uint64(blockHeader._size) << 3);
        }
        else if (encodedBlockBytes > uint64((numeric_limits<int>::max)())) {
            storeProcessedBlockId(CompressedInputStream::CANCEL_TASKS_ID);
            return T(*_data, blockId, 0, 0, Error::ERR_BLOCK_SIZE, "Invalid block size");
        }

        const uint r = uint(encodedBlockBytes);

        // Read from the shared bitstream if
        // - there is one that one task (each with their own local bitstream)
        // - the block is going to be skipped (bits must be consumed)
        if (bufferBlock == true) {
            if (_data->_length < int(max(_blockLength, r))) {
                _data->_length = int(max(_blockLength, r));
                delete[] _data->_array;
                _data->_array = new kanzi::byte[_data->_length];
            }

            for (uint i = 0; i < blockHeader._size; i++)
                _data->_array[i] = blockHeader._bytes[i];

            for (int n = int(blockHeader._size); read > 0; ) {
                const uint chkSize = uint(min(read, uint64(1) << 30));
                _ibs->readBits(&_data->_array[n], chkSize);
                n += ((chkSize + 7) >> 3);
                read -= uint64(chkSize);
            }
        }

        // After completion of the bitstream reading, increment the block id.
        // It unblocks the task processing the next block (if any)
        storeProcessedBlockId(blockId);

        // Check if the block must be skipped
        if (blockId < from) {
            return T(*_data, blockId, 0, 0, 0, "Skipped", true);
        }
        else if (blockId >= to) {
            return T(*_data, blockId, 0, 0, 0, "Success");
        }

        ifixedbuf buf(reinterpret_cast<char*>(&_data->_array[0]), streamsize(r));
        istream ios(&buf);
        ibs = (streamPerTask == true) ? new DefaultInputBitStream(ios) : _ibs;

        if (blockHeaderParsed == false) {
            string msg;
            const int error = readBlockHeader(ibs, bsVersion, encodedBlockLength,
                tType, blockHeader, msg);

            if (error != 0) {
                storeProcessedBlockId(CompressedInputStream::CANCEL_TASKS_ID);

                if (streamPerTask == true) {
                    try {
                        DELETE_AND_NULL(InputBitStream, ibs);
                    }
                    catch (...) {
                    }
                }

                return T(*_data, blockId, 0, checksum1, error, msg);
            }
        }
        else {
            ibs->readBits(blockHeader._size << 3);
        }

        const kanzi::byte skipFlags = blockHeader._skipFlags;
        const bool transformedCopy = blockHeader._transformedCopy;
        const int preTransformLength = blockHeader._preTransformLength;

        if (blockHeader._rawCopy == true) {
            tType = TransformFactory<kanzi::byte>::NONE_TYPE;
            eType = EntropyDecoderFactory::NONE_TYPE;
        }
        else if (transformedCopy == true) {
            eType = EntropyDecoderFactory::NONE_TYPE;
        }

        const uint maxAllowedTransformSize = transformedCopy ?
            maxTransformedCopySize : maxEntropyTransformSize;

        if ((preTransformLength <= 0) ||
            (uint(preTransformLength) > maxAllowedTransformSize)) {
            // Error => cancel concurrent decoding tasks
            storeProcessedBlockId(CompressedInputStream::CANCEL_TASKS_ID);
            stringstream ss;
            ss << "Invalid compressed block length: " << preTransformLength;

            if (streamPerTask == true) {
                try {
                    DELETE_AND_NULL(InputBitStream, ibs);
                }
                catch (...) {
                }
            }

            return T(*_data, blockId, 0, checksum1, Error::ERR_READ_FILE, ss.str());
        }

        if ((bsVersion >= 7) && (blockHeaderParsed == false)) {
            const uint64 checksumSize = (_hasher64 != nullptr) ? 8 :
                ((_hasher32 != nullptr) ? 4 : 0);
            const uint64 maxEncodedBlockBytes = uint64(preTransformLength) +
                uint64(blockHeader._size) + checksumSize;

            if (encodedBlockBytes > maxEncodedBlockBytes) {
                storeProcessedBlockId(CompressedInputStream::CANCEL_TASKS_ID);

                if (streamPerTask == true) {
                    try {
                        DELETE_AND_NULL(InputBitStream, ibs);
                    }
                    catch (...) {
                    }
                }

                return T(*_data, blockId, 0, checksum1, Error::ERR_BLOCK_SIZE,
                    "Invalid block size");
            }
        }

        Event::HashType hashType = Event::NO_HASH;
        WallTimer timer;

        // Extract checksum from bitstream (if any)
        if (_hasher32 != nullptr) {
            checksum1 = ibs->readBits(32);
            hashType = Event::SIZE_32;
        }
        else if (_hasher64 != nullptr) {
            checksum1 = ibs->readBits(64);
            hashType = Event::SIZE_64;
        }

        if (_listeners.size() > 0) {
#if !defined(_MSC_VER) || _MSC_VER > 1500
            if (_ctx.getInt("verbosity", 0) > 4) {
                Event evt1(Event::BLOCK_INFO, blockId, int64(r), timer.getCurrentTime(), checksum1,
                           hashType, blockOffset, uint8(skipFlags));
                CompressedInputStream::notifyListeners(_listeners, evt1);
            }
#endif

            // Notify before entropy
            Event evt2(Event::BEFORE_ENTROPY, blockId, int64(r), timer.getCurrentTime(), checksum1, hashType);
            CompressedInputStream::notifyListeners(_listeners, evt2);
        }

        const int bufferSize = max(int(_blockLength), preTransformLength + CompressedInputStream::EXTRA_BUFFER_SIZE);

        if (_buffer->_length < bufferSize) {
            _buffer->_length = bufferSize;
            if (_buffer->_array != nullptr)
               delete[] _buffer->_array;

            _buffer->_array = new kanzi::byte[_buffer->_length];
        }

        const int savedIdx = _data->_index;
        _ctx.putInt("size", preTransformLength);

        if (transformedCopy == true) {
            // The encoder kept the transformed bytes verbatim because entropy
            // coding expanded the block. Read the payload directly into the
            // post-entropy buffer, then continue with the inverse transform.
            for (uint n = 0, remaining = uint(preTransformLength); remaining > 0; ) {
                const uint chunk = min(remaining, uint(1) << 27);

                if (ibs->readBits(&_buffer->_array[n], 8 * chunk) != 8 * chunk) {
                    storeProcessedBlockId(CompressedInputStream::CANCEL_TASKS_ID);

                    if (streamPerTask == true) {
                        try {
                            DELETE_AND_NULL(InputBitStream, ibs);
                        }
                        catch (...) {
                        }
                    }

                    return T(*_data, blockId, 0, checksum1, Error::ERR_PROCESS_BLOCK,
                        "Entropy decoding failed");
                }

                n += chunk;
                remaining -= chunk;
            }

            if (streamPerTask == true) {
                DELETE_AND_NULL(InputBitStream, ibs);
            }
        }
        else {
            // Each block is decoded separately
            // Rebuild the entropy decoder to reset block statistics
            ed = EntropyDecoderFactory::newDecoder(*ibs, _ctx, eType);

            // Block entropy decode
            if (ed->decode(_buffer->_array, 0, preTransformLength) != preTransformLength) {
                // Error => cancel concurrent decoding tasks
                storeProcessedBlockId(CompressedInputStream::CANCEL_TASKS_ID);
                try {
                    DELETE_AND_NULL(EntropyDecoder, ed);
                }
                catch (...) {
                }

                if (streamPerTask == true) {
                    try {
                        DELETE_AND_NULL(InputBitStream, ibs);
                    }
                    catch (...) {
                    }
                }

                return T(*_data, blockId, 0, checksum1, Error::ERR_PROCESS_BLOCK,
                    "Entropy decoding failed");
            }

            if (streamPerTask == true) {
                DELETE_AND_NULL(InputBitStream, ibs);
            }

            DELETE_AND_NULL(EntropyDecoder, ed);
        }

        if (_listeners.size() > 0) {
            // Notify after entropy
            Event evt1(Event::AFTER_ENTROPY, blockId,
                int64(preTransformLength), timer.getCurrentTime(), checksum1, hashType);
            CompressedInputStream::notifyListeners(_listeners, evt1);

            // Notify before transform (block size after entropy decoding)
            Event evt2(Event::BEFORE_TRANSFORM, blockId,
                int64(preTransformLength), timer.getCurrentTime(), checksum1, hashType);
            CompressedInputStream::notifyListeners(_listeners, evt2);
        }

        transform = TransformFactory<kanzi::byte>::newTransform(_ctx, tType);
        transform->setSkipFlags(skipFlags);
        _buffer->_index = 0;

        // Inverse transform
        bool res = transform->inverse(*_buffer, *_data, preTransformLength);
        DELETE_AND_NULL(TransformSequence<kanzi::byte>, transform);

        if (res == false) {
            storeProcessedBlockId(CompressedInputStream::CANCEL_TASKS_ID);
            return T(*_data, blockId, 0, checksum1, Error::ERR_PROCESS_BLOCK,
                "Transform inverse failed");
        }

        const int decoded = _data->_index - savedIdx;

        // Verify checksum
        if (_hasher32 != nullptr) {
            const uint32 checksum2 = _hasher32->hash(&_data->_array[savedIdx], decoded);

            if (checksum2 != uint32(checksum1)) {
                storeProcessedBlockId(CompressedInputStream::CANCEL_TASKS_ID);
                stringstream ss;
                ss << "Corrupted bitstream: expected checksum " << std::hex << checksum1 << ", found " << std::hex << checksum2;
                return T(*_data, blockId, decoded, checksum1, Error::ERR_CRC_CHECK, ss.str());
            }
        }
        else if (_hasher64 != nullptr) {
            const uint64 checksum2 = _hasher64->hash(&_data->_array[savedIdx], decoded);

            if (checksum2 != checksum1) {
                storeProcessedBlockId(CompressedInputStream::CANCEL_TASKS_ID);
                stringstream ss;
                ss << "Corrupted bitstream: expected checksum " << std::hex << checksum1 << ", found " << std::hex << checksum2;
                return T(*_data, blockId, decoded, checksum1, Error::ERR_CRC_CHECK, ss.str());
            }
        }

        return T(*_data, blockId, decoded, checksum1, 0, "Success");
    }
    catch (const exception& e) {
        // Cancel any in-flight task waiting on this block.
        storeProcessedBlockId(CompressedInputStream::CANCEL_TASKS_ID);

        if (transform != nullptr) {
            try {
                DELETE_AND_NULL(TransformSequence<kanzi::byte>, transform);
            }
            catch (...) {
            }
        }

        if (ed != nullptr) {
            try {
                DELETE_AND_NULL(EntropyDecoder, ed);
            }
            catch (...) {
            }
        }

        if ((streamPerTask == true) && (ibs != nullptr)) {
            try {
                DELETE_AND_NULL(InputBitStream, ibs);
            }
            catch (...) {
            }
        }

        return T(*_data, blockId, 0, checksum1, Error::ERR_PROCESS_BLOCK, e.what());
    }
    catch (...) {
        // Cancel any in-flight task waiting on this block.
        storeProcessedBlockId(CompressedInputStream::CANCEL_TASKS_ID);

        if (transform != nullptr) {
            try {
                DELETE_AND_NULL(TransformSequence<kanzi::byte>, transform);
            }
            catch (...) {
            }
        }

        if (ed != nullptr) {
            try {
                DELETE_AND_NULL(EntropyDecoder, ed);
            }
            catch (...) {
            }
        }

        if ((streamPerTask == true) && (ibs != nullptr)) {
            try {
                DELETE_AND_NULL(InputBitStream, ibs);
            }
            catch (...) {
            }
        }

        return T(*_data, blockId, 0, checksum1, Error::ERR_PROCESS_BLOCK,
            "Unknown block processing error");
    }
}

#undef DELETE_AND_NULL

/****************************************************************************
 Copyright (c) 2016 Chukong Technologies Inc.
 
 http://www.cocos2d-x.org
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 
 Inspired by https://github.com/vit-vit/CTPL
 
 ****************************************************************************/
#pragma once

#include <functional>
#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
#include <chrono>
#include <queue>

#if HAS_CXX11_THREAD_LOCAL
#define CC_ATTRIBUTE_THREAD_LOCAL thread_local
#elif defined (__GNUC__)
#define CC_ATTRIBUTE_THREAD_LOCAL __thread
#elif defined (_MSC_VER)
#define CC_ATTRIBUTE_THREAD_LOCAL __declspec(thread)
#else // !C++11 && !__GNUC__ && !_MSC_VER
#error "Define a thread local storage qualifier for your compiler/platform!"
#endif

namespace cocos2d {

/**
 * @addtogroup base
 * @{
 */
    
template<typename T, typename ...Args>
std::unique_ptr<T> make_unique( Args&& ...args )
{
    return std::unique_ptr<T>( new T( std::forward<Args>(args)... ) );
}

class ThreadPool;
class Worker {
public:
	explicit Worker(ThreadPool *owner);
	~Worker() = default;
	bool isAlive;
	std::chrono::steady_clock::time_point lastActive;
	std::mutex lastActiveMutex;
    std::unique_ptr<std::thread> _thread;
};

class ThreadPool
{
public:
	static constexpr size_t _defaultThreadMin = 4;
	static constexpr size_t _defaultThreadMax = 20;

	static constexpr float _defaultShrinkInterval = 100.0f; // in ms
	static constexpr uint64_t _defaultIdleTime = 3500; // in ms

    /*
     * Gets the default thread pool which is a cached thread pool with default parameters.
     */
    static ThreadPool *getDefaultThreadPool();

    /*
     * Destroys the default thread pool
     */
    static void destroyDefaultThreadPool();

	ThreadPool(size_t poolSize);
	ThreadPool(size_t minThreadNum = _defaultThreadMin, size_t maxThreadNum = _defaultThreadMax, float shrinkInterval = _defaultShrinkInterval, uint64_t idleTime = _defaultIdleTime);

    ~ThreadPool();

    /* Pushs a task to thread pool
     *  @param runnable The callback of the task executed in sub thread
     *  @param type The task type, it's TASK_TYPE_DEFAULT if this argument isn't assigned
     *  @note This function has to be invoked in cocos thread
     */
    void pushTask(std::function<void(std::thread::id /*threadId*/)>&& runnable);

    // Gets the minimum thread numbers
    inline size_t getMinThreadNum() const { return _minThreadNum; };

    // Gets the maximum thread numbers
    inline size_t getMaxThreadNum() const { return _maxThreadNum; };
	
private:
	friend Worker;
    
    ThreadPool(const ThreadPool&) = delete; // Remove copy constructor, copying a thread pool doesn't make sense.
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = default;
    

    size_t _minThreadNum;
	size_t _maxThreadNum;
	uint64_t _maxIdleTime;

    float _shrinkInterval;

	std::vector<std::unique_ptr<Worker>> _workers;
	std::queue<std::function<void(std::thread::id)>> _workQueue;
	std::mutex _workerMutex;
	std::condition_variable_any _workerConditional;

	void evaluateThreads(float dt);
};

// end of base group
/// @}

} // namespace cocos2d {

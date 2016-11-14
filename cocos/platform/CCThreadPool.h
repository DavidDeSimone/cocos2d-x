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

#include "platform/CCPlatformMacros.h"
#include "CCStdC.h"

#include <functional>
#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
#include <chrono>

namespace cocos2d {

/**
 * @addtogroup base
 * @{
 */

class ThreadPool;
class Worker {
public:
	explicit Worker(ThreadPool *owner);
	~Worker() = default;
	std::atomic_bool isAlive;
	std::chrono::steady_clock::time_point lastActive;
	std::mutex lastActiveMutex;
private:
	std::unique_ptr<std::thread> _thread;
};

class ThreadPool
{
public:


	static constexpr int _defaultThreadMin = 4;
	static constexpr int _defaultThreadMax = 20;

	static constexpr float _defaultShrinkInterval = 5.0f; // in ms
	static constexpr uint64_t _defaultIdleTime = 3500; // in ms

    enum class TaskType
    {
        DEFAULT = 0,
        NETWORK,
        IO,
        AUDIO,
        USER = 1000
    };

    /*
     * Gets the default thread pool which is a cached thread pool with default parameters.
     */
    static ThreadPool *getDefaultThreadPool();

    /*
     * Destroys the default thread pool
     */
    static void destroyDefaultThreadPool();

    /*
     * Creates a cached thread pool
     * @note The return value has to be delete while it doesn't needed
     */
    static ThreadPool *createdThreadPool(int minThreadNum = _defaultThreadMin, int maxThreadNum = _defaultThreadMax, int shrinkInterval = _defaultShrinkInterval, uint64_t idleTime = _defaultIdleTime);

    /*
     * Creates a thread pool with fixed thread count
     * @note The return value has to be delete while it doesn't needed
     */
    static ThreadPool *createFixedSizeThreadPool(int threadNum);

    // the destructor waits for all the functions in the queue to be finished
    ~ThreadPool();

    /* Pushs a task to thread pool
     *  @param runnable The callback of the task executed in sub thread
     *  @param type The task type, it's TASK_TYPE_DEFAULT if this argument isn't assigned
     *  @note This function has to be invoked in cocos thread
     */
    void pushTask(std::function<void(int /*threadId*/)>&& runnable, TaskType type = TaskType::DEFAULT);

    // Stops all tasks, it will remove all tasks in queue
    void stopAllTasks();

    // Stops some tasks by type
    void stopTasksByType(TaskType type);

	void setFixedSize(bool fixedSize) {
		_isFixedSize = fixedSize;
	}

    // Gets the minimum thread numbers
    inline int getMinThreadNum() const
    { return _minThreadNum; };

    // Gets the maximum thread numbers
    inline int getMaxThreadNum() const
    { return _maxThreadNum; };
	
private:
	friend Worker;
	ThreadPool(int minNum, int maxNum);
    ThreadPool(int minNum, int maxNum, int _shrinkStep, uint32_t maxIdleTime);

    ThreadPool(const ThreadPool&) = delete;

    ThreadPool(ThreadPool&&);

    int _minThreadNum;
    int _maxThreadNum;
	uint64_t _maxIdleTime;

    float _shrinkInterval;
    bool _isFixedSize;

	std::vector<std::unique_ptr<Worker>> _workers;
	std::vector<std::function<void(int)>> _workQueue;
	std::mutex _workerMutex;
	std::condition_variable_any _workerConditional;
	std::atomic_bool isAlive;

	void evaluateThreads();
};

// end of base group
/// @}

} // namespace cocos2d {

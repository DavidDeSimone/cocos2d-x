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

#include "cocos/platform/CCThreadPool.h"
#include "CCStdC.h"
#include "base/CCDirector.h"
#include "base/CCScheduler.h"

#include <sstream>
#include <string>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "ThreadPool"
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG,__VA_ARGS__)
#else
#define LOGD(...) printf(__VA_ARGS__)
#endif

namespace cocos2d {

static ThreadPool *__defaultThreadPool = nullptr;

ThreadPool *ThreadPool::getDefaultThreadPool()
{
    if (__defaultThreadPool == nullptr)
    {
        __defaultThreadPool = createdThreadPool(_defaultThreadMin,
                                                  _defaultThreadMax,
                                                  _defaultShrinkInterval);
    }

    return __defaultThreadPool;
}

void ThreadPool::destroyDefaultThreadPool()
{
    CC_SAFE_DELETE(__defaultThreadPool);
}

ThreadPool *ThreadPool::createdThreadPool(size_t minThreadNum, size_t maxThreadNum, float shrinkInterval, uint64_t maxIdleTime)
{
	return new(std::nothrow) ThreadPool(minThreadNum, maxThreadNum, shrinkInterval, maxIdleTime);
}

ThreadPool *ThreadPool::createFixedSizeThreadPool(size_t threadNum)
{
    return new (std::nothrow) ThreadPool(threadNum, threadNum);
}

ThreadPool::ThreadPool(size_t minNum, size_t maxNum, float shrinkInterval, uint64_t maxIdleTime)
        :  _minThreadNum(minNum)
        ,  _maxThreadNum(maxNum)
		, _shrinkInterval(shrinkInterval)
		, _maxIdleTime(maxIdleTime)
{
	if (_minThreadNum != _maxThreadNum)
	{
		std::ostringstream address;
		address << (void const *)this;
		_scheduleId = address.str();
		Director::getInstance()->getScheduler()->schedule(std::bind(&ThreadPool::evaluateThreads, this, std::placeholders::_1), this, _shrinkInterval, false, _scheduleId);
	}
	
}

// the destructor waits for all the functions in the queue to be finished
ThreadPool::~ThreadPool()
{
	if (_minThreadNum != _maxThreadNum)
	{
		Director::getInstance()->getScheduler()->unschedule(_scheduleId, this);
	}
	
	std::unique_lock<std::mutex>(_workerMutex);
	for (auto& worker : _workers)
	{
		worker->isAlive = false;
		worker->join();
	}

	_workers.clear();
}

Worker::Worker(ThreadPool *owner)
	: isAlive(true)
{
	lastActive = std::chrono::steady_clock::now();
	_thread = std::unique_ptr<std::thread>(new (std::nothrow) std::thread([this, owner] {
		while (true)
		{

			std::function<void(std::thread::id)> task;
			{
				std::unique_lock<std::mutex>(owner->_workerMutex);
				owner->_workerConditional.wait(owner->_workerMutex, [=]() -> bool {
					return !isAlive;
				});

				if (!isAlive)
				{
					break;
				}

				task = std::move(owner->_workQueue.front());
				owner->_workQueue.erase(owner->_workQueue.begin());
			}

			task(std::this_thread::get_id());

			{
				std::unique_lock<std::mutex>(lastActiveMutex);
				lastActive = std::chrono::steady_clock::now();
			}
		}
	}));
	_thread->detach();
}

void Worker::join()
{
	if (_thread)
	{
		_thread->join();
	}
}

void ThreadPool::pushTask(std::function<void(std::thread::id)>&& runnable)
{
	{
		std::unique_lock<std::mutex>(_workerMutex);
		if (_workers.size() < _maxThreadNum)
		{
			_workers.push_back(std::unique_ptr<Worker>(new (std::nothrow) Worker(this)));
		}

		_workQueue.push_back(std::forward<std::function<void(std::thread::id)>>(runnable));
	}

	_workerConditional.notify_one();
}

void ThreadPool::evaluateThreads(float /* dt */)
{
	auto now = std::chrono::steady_clock::now();
	std::unique_lock<std::mutex>(_workerMutex);
	for (auto it = _workers.begin(); it != _workers.end();)
	{
		auto&& worker = *it;
		std::unique_lock<std::mutex>(worker->lastActiveMutex);
		auto lifetime = std::chrono::duration_cast<std::chrono::milliseconds>((now - worker->lastActive)).count();
		if (lifetime > _maxIdleTime)
		{
			worker->isAlive = false;
			it = _workers.erase(it);
		}
		else
		{
			++it;
		}
	}
}

} // namespace cocos2d

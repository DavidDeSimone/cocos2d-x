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
 
 ****************************************************************************/

#include "cocos/platform/CCThreadPool.h"
#include "base/CCDirector.h"
#include "base/CCScheduler.h"

#include <sstream>
#include <string>

NS_CC_BEGIN

ThreadPool::ThreadPool(size_t poolSize)
{
	ThreadPool(poolSize, poolSize);
}

ThreadPool::ThreadPool(size_t minNum, size_t maxNum, float shrinkInterval, uint64_t maxIdleTime)
        :  _minThreadNum(minNum)
        ,  _maxThreadNum(maxNum)
        , _shrinkInterval(shrinkInterval)
        , _maxIdleTime(maxIdleTime)
{
	if (_minThreadNum != _maxThreadNum)
	{
		Director::getInstance()->getScheduler()->schedule(std::bind(&ThreadPool::evaluateThreads, this, std::placeholders::_1), this, _shrinkInterval, false, "ThreadPool");
	}
	
}

ThreadPool::~ThreadPool()
{
	if (_minThreadNum != _maxThreadNum)
	{
		Director::getInstance()->getScheduler()->unschedule("ThreadPool", this);
	}
    
    std::vector<std::unique_ptr<std::thread>> threads;
    
    {
        std::unique_lock<std::mutex> lock(_workerMutex);
        for (auto&& worker : _workers)
        {
            worker->isAlive = false;
            threads.push_back(std::move(worker->_thread));
        }
    }
    
    _workerConditional.notify_all();
    for (auto&& thread : threads)
    {
        thread->join();
    }
}
    
void ThreadPool::pushTask(std::function<void(std::thread::id)>&& runnable)
{
    {
        std::unique_lock<std::mutex> lock(_workerMutex);
        if (_workers.size() < _maxThreadNum)
        {
            _workers.push_back(cocos2d::make_unique<Worker>(this));
        }
            
        _workQueue.push(std::forward<std::function<void(std::thread::id)>>(runnable));
        _workerConditional.notify_one();
    }
}
    
void ThreadPool::evaluateThreads(float /* dt */)
{
    auto now = std::chrono::steady_clock::now();
    uint32_t killedWorkers = 0;
    std::unique_lock<std::mutex> lock(_workerMutex);
    // If work is in progress, return and free lock
    if (_workQueue.size() != 0 || _workers.size() == _minThreadNum)
    {
        return;
    }
    
    for (auto&& worker : _workers)
    {
        std::unique_lock<std::mutex>(worker->lastActiveMutex);
        auto lifetime = std::chrono::duration_cast<std::chrono::milliseconds>((now - worker->lastActive)).count();
        if (lifetime > _maxIdleTime)
        {
            worker->isAlive = false;
            ++killedWorkers;
        }
            
        if (_workers.size() - killedWorkers == _minThreadNum)
        {
            break;
        }
    }
    
    if (killedWorkers > 0)
    {
        _workerConditional.notify_all();
    }
}

Worker::Worker(ThreadPool *owner)
	: isAlive(true)
{
    lastActive = std::chrono::steady_clock::now();
    _thread = cocos2d::make_unique<std::thread>([this, owner] {
        std::function<void(std::thread::id)> task;
        while (true)
        {
            {
                std::unique_lock<std::mutex>(owner->_workerMutex);
                owner->_workerConditional.wait(owner->_workerMutex, [this, owner]() -> bool {
                    return !isAlive || owner->_workQueue.size() > 0;
                });

                if (!isAlive)
                {
                    // Deletion is done within the thread loop, to prevent use after free
                    // on the worker object.
                    auto it = std::find_if(owner->_workers.begin(), owner->_workers.end(),
                                           [this](const std::unique_ptr<Worker>& worker) -> bool {
                                               return this == worker.get();
                                           });
                    owner->_workers.erase(it);
                    break;
                }

                task = std::move(owner->_workQueue.front());
                owner->_workQueue.pop();
            }

            task(std::this_thread::get_id());

            {
                std::unique_lock<std::mutex> lock(lastActiveMutex);
                lastActive = std::chrono::steady_clock::now();
            }
        }
	});
}

NS_CC_END

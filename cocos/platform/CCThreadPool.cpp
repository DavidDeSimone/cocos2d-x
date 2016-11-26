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

ThreadPool* ThreadPool::_defaulThreadPool = nullptr;
ThreadPool* ThreadPool::getDefaultThreadPool()
{
    if (!_defaulThreadPool)
    {
        _defaulThreadPool = new (std::nothrow) ThreadPool();
    }
    
    return _defaulThreadPool;
}

void ThreadPool::destroyDefaultThreadPool()
{
    CC_SAFE_DELETE(_defaulThreadPool);
}

ThreadPool::ThreadPool(size_t poolSize)
{
	ThreadPool(poolSize, poolSize);
}

ThreadPool::ThreadPool(size_t minNum, size_t maxNum, float shrinkInterval)
        :  _minThreadNum(minNum)
        ,  _maxThreadNum(maxNum)
        , _shrinkInterval(shrinkInterval)
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
    std::vector<std::unique_ptr<std::thread>> killedThreads;
    {
        std::unique_lock<std::mutex> lock(_workerMutex);
        // If work is in progress, return and free lock
        if (_workQueue.size() != 0 || _workers.size() == _minThreadNum)
        {
            return;
        }
        
        for (auto&& worker : _workers)
        {
            if (!worker->runningTask)
            {
                worker->isAlive = false;
                killedThreads.push_back(std::move(worker->_thread));
            }
            
            if (_workers.size() - killedThreads.size() == _minThreadNum)
            {
                break;
            }
        }
    }

    if (killedThreads.size() > 0)
    {
        _workerConditional.notify_all();
        for (auto&& thread : killedThreads)
        {
            thread->join();
        }
    }
}

Worker::Worker(ThreadPool *owner)
	: isAlive(true)
    , runningTask(false)
{
    _thread = cocos2d::make_unique<std::thread>([this, owner] {
        std::function<void(std::thread::id)> task;
        while (true)
        {
            {
                std::unique_lock<std::mutex> lock(owner->_workerMutex);
                owner->_workerConditional.wait(lock, [this, owner]() -> bool {
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
                    return;
                }

                task = std::move(owner->_workQueue.front());
                owner->_workQueue.pop();
            }

            runningTask = true;
            task(std::this_thread::get_id());
            runningTask = false;
        }
	});
}

NS_CC_END

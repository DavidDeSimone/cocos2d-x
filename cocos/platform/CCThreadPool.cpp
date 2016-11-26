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
    _queue = std::make_shared<WorkQueue>();
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
    
    {
        std::unique_lock<std::mutex> lock(_queue->_workerMutex);
        for (auto&& worker : _queue->_workers)
        {
            worker->isAlive = false;
        }
    }
    
    _queue->_workerConditional.notify_all();
}
    
void ThreadPool::pushTask(std::function<void(std::thread::id)>&& runnable)
{
    {
        std::unique_lock<std::mutex> lock(_queue->_workerMutex);
        if (_queue->_workers.size() < _maxThreadNum)
        {
            _queue->_workers.push_back(cocos2d::make_unique<Worker>(this));
        }
            
        _queue->_workQueue.push(std::forward<std::function<void(std::thread::id)>>(runnable));
        _queue->_workerConditional.notify_one();
    }
}
    
void ThreadPool::evaluateThreads(float /* dt */)
{
    uint64_t killedThreads = 0;
    {
        std::unique_lock<std::mutex> lock(_queue->_workerMutex);
        // If work is in progress, return and free lock
        if (_queue->_workQueue.size() != 0 || _queue->_workers.size() == _minThreadNum)
        {
            return;
        }
        
        for (auto&& worker : _queue->_workers)
        {
            worker->isAlive = false;
            ++killedThreads;
            
            if (_queue->_workers.size() - killedThreads == _minThreadNum)
            {
                break;
            }
        }
    }

    if (killedThreads > 0)
    {
        _queue->_workerConditional.notify_all();
    }
}

Worker::Worker(ThreadPool *owner)
	: isAlive(true)
{
    auto _queue = owner->_queue;
    std::thread([this, owner, _queue] {
        std::function<void(std::thread::id)> task;
        while (true)
        {
            {
                std::unique_lock<std::mutex> lock(_queue->_workerMutex);
                _queue->_workerConditional.wait(lock, [this, owner, _queue]() -> bool {
                    return !isAlive || _queue->_workQueue.size() > 0;
                });

                if (!isAlive)
                {
                    // Deletion is done within the thread loop, to prevent use after free
                    // on the worker object.
                    auto it = std::find_if(_queue->_workers.begin(), _queue->_workers.end(),
                                           [this](const std::unique_ptr<Worker>& worker) -> bool {
                                               return this == worker.get();
                                           });
                    _queue->_workers.erase(it);
                    return;
                }

                task = std::move(_queue->_workQueue.front());
                _queue->_workQueue.pop();
            }

            task(std::this_thread::get_id());
        }
	}).detach();
}

NS_CC_END

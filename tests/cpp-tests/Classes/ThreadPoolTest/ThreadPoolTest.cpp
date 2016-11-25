/****************************************************************************
 Copyright (c) 2012 cocos2d-x.org
 Copyright (c) 2013-2016 Chukong Technologies Inc.
 
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

#include "ThreadPoolTest.h"

USING_NS_CC;

ThreadPoolTest::ThreadPoolTest()
{
    ADD_TEST_CASE(ThreadPoolTest1);
};

ThreadPoolTest1::ThreadPoolTest1()
{
    _testThreadPool = cocos2d::make_unique<ThreadPool>(1, 3);
    _testThreadPool->pushTask([](std::thread::id id) {
        CCASSERT(id != Director::getInstance()->getCocos2dThreadId(), "This operation should be performed offthread");
        
        int curr = 1;
        int prev = 1;
        int temp = 0;
        for (auto i = 0; i < 300000; ++i)
        {
            temp = curr;
            curr += prev;
            prev = temp;
        }
        
        Director::getInstance()->getScheduler()->performFunctionInCocosThread([curr]() {
            CCLOG("Calcuated offthread fib number as %d", curr);
        });
    });
    
    auto lambda = [] (std::thread::id id) {
        CCASSERT(id != Director::getInstance()->getCocos2dThreadId(), "This operation should be performed offthread");
        int curr = 1;
        int prev = 1;
        int temp = 0;
        for (auto i = 0; i < 300000; ++i)
        {
            temp = curr;
            curr += prev;
            prev = temp;
        }
        Director::getInstance()->getScheduler()->performFunctionInCocosThread([curr]() {
            CCLOG("Calcuated offthread l-value fib number as %d", curr);
        });
    };
    
    _testThreadPool->pushTask(lambda);
}

std::string ThreadPoolTest1::title() const
{
    return "Testing async CC Thread Pool";
}

std::string ThreadPoolTest1::subtitle() const
{
    return "Verify program doesn't crash";
}
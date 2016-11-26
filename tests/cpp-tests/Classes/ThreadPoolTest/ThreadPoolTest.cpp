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
#include <cmath>

USING_NS_CC;

#define ITERATION_COUNT 4000

ThreadPoolTest::ThreadPoolTest()
{
    ADD_TEST_CASE(ThreadPoolTest1);
};

ThreadPoolTest1::ThreadPoolTest1()
{
    TTFConfig ttfConfig("fonts/arial.ttf", 15);
    auto label = Label::createWithTTF(ttfConfig, "% of jobs complete: 0");
    label->setPositionNormalized(Vec2(0.5, 0.5));
    addChild(label);
    _testThreadPool = cocos2d::make_unique<ThreadPool>(1, 20);
    _testThreadPool->pushTask([](std::thread::id id) {
        CCASSERT(id != Director::getInstance()->getCocos2dThreadId(), "This operation should be performed offthread");
        
        int curr = 1;
        int prev = 1;
        int temp = 0;
        for (auto i = 0; i < 3000; ++i)
        {
            temp = curr;
            curr += prev;
            prev = temp;
        }
        
        Director::getInstance()->getScheduler()->performFunctionInCocosThread([curr]() {
            CCLOG("Calcuated offthread fib number as %d", curr);
        });
    });
    
    

    std::shared_ptr<std::atomic_int> completedCount = std::make_shared<std::atomic_int>(0);
    auto lambda = [completedCount, label] (std::thread::id id) {
        CCASSERT(id != Director::getInstance()->getCocos2dThreadId(), "This operation should be performed offthread");

        int seconds = std::round(10 * rand_0_1());
        std::this_thread::sleep_for(std::chrono::milliseconds(seconds));
        *completedCount += 1;
        int value = *completedCount;
        Director::getInstance()->getScheduler()->performFunctionInCocosThread([value, label]() {
            float percent = ((float)value / ITERATION_COUNT) * 100;
            std::stringstream ss;
            ss << "percent of operations complete: " << percent << "%";
            label->setString(ss.str());
        });
    };
    
    
    for (auto i = 0; i < ITERATION_COUNT; ++i)
    {
       _testThreadPool->pushTask(lambda);
    }
}

std::string ThreadPoolTest1::title() const
{
    return "Testing async CC Thread Pool";
}

std::string ThreadPoolTest1::subtitle() const
{
    return "Verify program doesn't crash";
}

ThreadPoolNoGrowShrink::ThreadPoolNoGrowShrink()
{
    
}
    
std::string ThreadPoolNoGrowShrink::title() const
{
    return "";
}

std::string ThreadPoolNoGrowShrink::subtitle() const
{
    return "";
}

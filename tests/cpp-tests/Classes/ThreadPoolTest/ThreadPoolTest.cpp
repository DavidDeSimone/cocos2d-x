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
#include "platform/CCThreadPool.h"

USING_NS_CC;

ThreadPoolTest::ThreadPoolTest()
{
    ADD_TEST_CASE(ThreadPoolTest1);
};

ThreadPoolTest1::ThreadPoolTest1()
{
    ThreadPool pool(1, 3);
    pool.pushTask([](std::thread::id) {
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
    
    
    
    auto write = FileUtils::getInstance()->getWritablePath();
    FileUtils::getInstance()->writeStringToFile("This is a test str\n",  write + "__tmp.txt", [write](bool success) {
        FileUtils::getInstance()->getStringFromFile(write + "__tmp.txt", [](std::string&& result) {
            CCLOG("Got test result %s", result.c_str());
        });
    });
}

std::string ThreadPoolTest1::title() const
{
    return "Testing async CC Thread Pool";
}

std::string ThreadPoolTest1::subtitle() const
{
    return "";
}
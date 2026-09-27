/*
 * Copyright (C) 2026 The Android Open Source Project
 * SPDX-License-Identifier: Apache-2.0
 */
#include <gtest/gtest.h>
#include <IImsMediaThread.h>
#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>

namespace {
using namespace std::chrono_literals;
class HeldWorker : public IImsMediaThread {
public:
    ~HeldWorker() override { release(); StopThread(); JoinThread(); }
    bool waitForEntry() {
        std::unique_lock<std::mutex> lock(mMutex);
        return mCondition.wait_for(lock, 2s, [this] { return mEntered; });
    }
    void release() {
        std::lock_guard<std::mutex> lock(mMutex);
        mReleased = true;
        mCondition.notify_all();
    }
    void* run() override {
        std::unique_lock<std::mutex> lock(mMutex);
        mEntered = true;
        mCondition.notify_all();
        mCondition.wait(lock, [this] { return mReleased; });
        ++exits;
        return nullptr;
    }
    std::atomic<int> exits{0};
private:
    std::mutex mMutex;
    std::condition_variable mCondition;
    bool mEntered = false;
    bool mReleased = false;
};

TEST(IImsMediaThreadTest, StopFlagDoesNotAllowReleaseBeforeWorkerReturns) {
    HeldWorker worker;
    ASSERT_TRUE(worker.StartThread("held-worker"));
    ASSERT_TRUE(worker.waitForEntry());
    worker.StopThread();
    auto joined = std::async(std::launch::async, [&worker] { worker.JoinThread(); });
    EXPECT_EQ(joined.wait_for(20ms), std::future_status::timeout);
    EXPECT_EQ(worker.exits.load(), 0);
    worker.release();
    joined.get();
    EXPECT_EQ(worker.exits.load(), 1);
    worker.JoinThread(); // repeated shutdown is harmless
}

TEST(IImsMediaThreadTest, DuplicateStartDoesNotCreateAnotherWorker) {
    HeldWorker worker;
    ASSERT_TRUE(worker.StartThread());
    ASSERT_TRUE(worker.waitForEntry());
    EXPECT_FALSE(worker.StartThread());
    worker.release();
    worker.JoinThread();
    EXPECT_EQ(worker.exits.load(), 1);
    EXPECT_TRUE(worker.IsThreadStopped());
}

TEST(IImsMediaThreadTest, RestartWaitsForPreviouslyStoppedWorker) {
    HeldWorker worker;
    ASSERT_TRUE(worker.StartThread());
    ASSERT_TRUE(worker.waitForEntry());
    worker.StopThread();
    auto restarted = std::async(std::launch::async, [&worker] { return worker.StartThread(); });
    EXPECT_EQ(restarted.wait_for(20ms), std::future_status::timeout);
    worker.release();
    EXPECT_TRUE(restarted.get());
    worker.JoinThread();
    EXPECT_EQ(worker.exits.load(), 2);
}
} // namespace

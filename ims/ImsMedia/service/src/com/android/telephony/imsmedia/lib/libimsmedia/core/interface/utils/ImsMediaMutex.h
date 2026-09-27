/**
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef IMS_MEDIA_MUTEX
#define IMS_MEDIA_MUTEX

#include <ImsMediaTrace.h>
#include <mutex>
#include <functional>
#include <signal.h>

using namespace std::chrono_literals;

class ImsMediaMutex
{
public:
    ImsMediaMutex()
    {
        mMutex = new std::timed_mutex();
        mCallback = nullptr;
        mUserData = nullptr;
        mTimeout = 1000ms;
    }
    ~ImsMediaMutex()
    {
        if (mMutex != nullptr)
        {
            delete mMutex;
        }

        mCallback = nullptr;
        mUserData = nullptr;
    };

    /**
     * @brief Set the callback when the mutex cannot acquire lock over the timeout, if the callback
     * is not set, the crash will be triggered as default operation.
     *
     * @param callback The callback function
     * @param userData The client who sets the callback
     */
    void setCallback(std::function<void(void* userData)> callback, void* userData)
    {
        mCallback = callback;
        mUserData = userData;
    }

    /**
     * @brief Set the timeout threshold. The default timeout is 1000ms.
     *
     * @param time The timeout threshold in milliseconds unit
     */
    void setTimeout(std::chrono::duration<int32_t, std::milli> time)
    {
        IMLOGD1("[lock] set timeout=%d", std::chrono::milliseconds(time).count());
        mTimeout = time;
    }

    /**
     * @brief lock the mutex for timeout, trigger the notification when the timeout is happened.
     */
    void lock()
    {
        if (!mMutex->try_lock_for(mTimeout))
        {
            IMLOGW0("[lock] timeout");
            if (mCallback != nullptr)
            {
                mCallback(mUserData);
            }
            else
            {
                /* Send SIGTERM (terminate) signal to the current process. This will restart the
                 * ImsMedia service and successive calls will not be impacted by deadlock.
                 */
                raise(SIGTERM);
            }

            // The RAII guard will always unlock on destruction. If a signal handler or timeout
            // callback returns, wait until ownership is actually acquired before returning.
            mMutex->lock();
        }
    }

    /**
     * @brief unlock the mutex
     */
    void unlock() { mMutex->unlock(); }

    class Autolock
    {
    public:
        inline Autolock(ImsMediaMutex& mutex) :
                mImsMutex(&mutex)
        {
            mutex.lock();
        }
        inline Autolock(ImsMediaMutex* mutex) :
                mImsMutex(mutex)
        {
            mutex->lock();
        }
        inline ~Autolock() { mImsMutex->unlock(); }

    private:
        ImsMediaMutex* mImsMutex;
    };

private:
    std::timed_mutex* mMutex;
    std::function<void(void* userData)> mCallback;
    std::chrono::duration<int32_t, std::milli> mTimeout;
    void* mUserData;
};

#endif

/**
 * Copyright (C) 2022 The Android Open Source Project
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

#include <StreamScheduler.h>
#include <ImsMediaTrace.h>
#include <stdint.h>
#include <chrono>
#include <thread>
#include <algorithm>

using namespace std::chrono;

#define RUN_WAIT_TIMEOUT_MS  2
#define STOP_WAIT_TIMEOUT_MS 1000
#define START_WAIT_TIMEOUT_MS 2000

StreamScheduler::StreamScheduler() :
        mIsRunning(false),
        mStartResult(RESULT_SUCCESS)
{
}

StreamScheduler::~StreamScheduler()
{
    Stop();
}

void StreamScheduler::RegisterNode(BaseNode* pNode)
{
    if (pNode != nullptr)
    {
        IMLOGD2("[RegisterNode] [%p], node[%s]", this, pNode->GetNodeName());
        ImsMediaMutex::Autolock lock(mMutex);
        mListRegisteredNode.push_back(pNode);
    }
}

void StreamScheduler::DeRegisterNode(BaseNode* pNode)
{
    if (pNode != nullptr)
    {
        IMLOGD2("[DeRegisterNode] [%p], node[%s]", this, pNode->GetNodeName());
        ImsMediaMutex::Autolock lock(mMutex);
        mListRegisteredNode.remove(pNode);
    }
}

ImsMediaResult StreamScheduler::Start()
{
    IMLOGD1("[Start] [%p] enter", this);
    if (!IsThreadStopped())
    {
        return RESULT_NOT_READY;
    }
    JoinThread();

    for (auto& node : mListRegisteredNode)
    {
        if (node != nullptr)
        {
            IMLOGD2("[Start] [%p] registered node [%s]", this, node->GetNodeName());
        }
    }

    if (mListRegisteredNode.empty())
    {
        mIsRunning = false;
        IMLOGD1("[Start] [%p] exit - no registered nodes", this);
        return RESULT_SUCCESS;
    }

    mConditionStarted.reset();
    mConditionExit.reset();
    mStartResult = RESULT_NOT_READY;
    mIsRunning = true;
    IMLOGD1("[Start] [%p] Start thread", this);

    if (!StartThread("StreamScheduler"))
    {
        mIsRunning = false;
        IMLOGE1("[Start] [%p] failed to create scheduler thread", this);
        return RESULT_NOT_READY;
    }

    if (mConditionStarted.wait_timeout(START_WAIT_TIMEOUT_MS))
    {
        IMLOGE1("[Start] [%p] timed out starting registered nodes", this);
        StopThread();
        Awake();
        mConditionExit.wait_timeout(STOP_WAIT_TIMEOUT_MS);
        JoinThread();
        return RESULT_NOT_READY;
    }

    ImsMediaResult result = mStartResult.load();

    if (result != RESULT_SUCCESS)
    {
        mConditionExit.wait_timeout(STOP_WAIT_TIMEOUT_MS);
        JoinThread();
    }

    IMLOGD2("[Start] [%p] exit result[%d]", this, result);
    return result;
}

void StreamScheduler::Stop()
{
    IMLOGD1("[Stop] [%p] enter", this);

    if (!IsThreadStopped())
    {
        StopThread();
        Awake();
        mConditionExit.wait_timeout(STOP_WAIT_TIMEOUT_MS);
    }

    // A timeout or exit notification is not proof that run() has returned.
    JoinThread();
    IMLOGD1("[Stop] [%p] exit", this);
}

void StreamScheduler::Awake()
{
    if (!mIsRunning)
    {
        mIsRunning = true;
        mConditionMain.signal();
    }
}

bool StreamScheduler::RunRegisteredNode()
{
    bool needToRun = false;
    // the list to contain non-source type node
    std::list<BaseNode*> listNodesToRun;

    for (auto& node : mListRegisteredNode)
    {
        if (node != nullptr && node->GetState() == kNodeStateRunning && !node->IsRunTime())
        {
            if (node->IsSourceNode())  // process the source node
            {
                node->ProcessData();
                needToRun = true;
            }
            else if (node->GetDataCount() > 0)
            {
                listNodesToRun.push_back(node);  // store node to run
            }
        }
    }

    for (auto& node : listNodesToRun)
    {
        if (IsThreadStopped())
        {
            break;
        }

        if (node != nullptr)
        {
            node->ProcessData();  // process the non runtime node

            if (node->GetDataCount() > 0)
            {
                needToRun = true;
            }
        }
    }

    listNodesToRun.clear();
    return needToRun;
}

void* StreamScheduler::run()
{
    IMLOGD1("[run] [%p] enter", this);

    // start nodes
    mMutex.lock();

    ImsMediaResult startResult = RESULT_SUCCESS;

    for (auto& node : mListRegisteredNode)
    {
        if (node != nullptr && !node->IsRunTimeStart())
        {
            if (node->GetState() == kNodeStateStopped)
            {
                ImsMediaResult result = node->ProcessStart();

                if (result != RESULT_SUCCESS)
                {
                    IMLOGE2("[run] start node[%s] failed[%d]", node->GetNodeName(), result);
                    startResult = result;
                    break;
                }
            }
        }
    }

    mStartResult = startResult;
    mMutex.unlock();
    mConditionStarted.signal();

    if (startResult != RESULT_SUCCESS)
    {
        StopThread();
    }

    while (!IsThreadStopped())
    {
        mMutex.lock();
        bool needToRun = RunRegisteredNode();
        mMutex.unlock();

        if (IsThreadStopped())
        {
            break;
        }

        if (needToRun)
        {
            mIsRunning = true;
            mConditionMain.wait_timeout(RUN_WAIT_TIMEOUT_MS);
        }
        else
        {
            mIsRunning = false;
            mConditionMain.wait();
        }
    }

    mIsRunning = false;
    mConditionExit.signal();
    IMLOGD1("[run] [%p] exit", this);
    return nullptr;
}

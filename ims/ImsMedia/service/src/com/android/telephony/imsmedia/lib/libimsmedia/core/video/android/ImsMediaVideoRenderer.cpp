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

#include <ImsMediaVideoRenderer.h>
#include <ImsMediaTrace.h>
#include <ImsMediaTimer.h>
#include <ImsMediaVideoUtil.h>

#include <cstring>
#include <limits>
#include <pthread.h>

#define CODEC_TIMEOUT_NANO 100000
#define INTERVAL_MILLIS    10
#define MAX_FRAME_QUEUE_SIZE 32

static void* RunRendererThread(void* context)
{
    if (context != nullptr)
    {
        static_cast<ImsMediaVideoRenderer*>(context)->processBuffers();
    }
    return nullptr;
}

ImsMediaVideoRenderer::ImsMediaVideoRenderer()
{
    mCallback = nullptr;
    mWindow = nullptr;
    mCodec = nullptr;
    mFormat = nullptr;
    mCodecType = -1;
    mWidth = 0;
    mHeight = 0;
    mFarOrientationDegree = 0;
    mNearOrientationDegree = 0;
    mStopped = true;
    mAcquiredWindow = nullptr;
    mRenderThreadStarted = false;
}

ImsMediaVideoRenderer::~ImsMediaVideoRenderer()
{
    Stop();

    std::lock_guard<std::mutex> lock(mMutex);
    while (!mFrameDatas.empty())
    {
        FrameData* frame = mFrameDatas.front();
        delete frame;
        mFrameDatas.pop_front();
    }
}

void ImsMediaVideoRenderer::SetSessionCallback(BaseSessionCallback* callback)
{
    mCallback = callback;
}

void ImsMediaVideoRenderer::SetCodec(int32_t codecType)
{
    IMLOGD1("[SetCodec] codec[%d]", codecType);
    mCodecType = codecType;
}

void ImsMediaVideoRenderer::SetResolution(uint32_t width, uint32_t height)
{
    IMLOGD2("[SetResolution] width[%d], height[%d]", width, height);
    mWidth = width;
    mHeight = height;
}

void ImsMediaVideoRenderer::SetDeviceOrientation(uint32_t orientation)
{
    IMLOGD1("[SetDeviceOrientation] orientation[%d]", orientation);
    mNearOrientationDegree = orientation;
}

void ImsMediaVideoRenderer::SetSurface(ANativeWindow* window)
{
    IMLOGD1("[SetSurface] surface[%p]", window);
    mWindow = window;
}

void ImsMediaVideoRenderer::SetCodecSprop(const std::string& sprop)
{
    IMLOGD1("[SetCodecSprop] sprop[%s]", sprop.c_str());
    mSpropValue = sprop;
}

bool ImsMediaVideoRenderer::Start()
{
    IMLOGD0("[Start]");
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mStopped || mRenderThreadStarted || mCodec != nullptr || mFormat != nullptr)
        {
            IMLOGE0("[Start] renderer is already running");
            return false;
        }
    }

    if (mCodecType != kVideoCodecAvc && mCodecType != kVideoCodecHevc)
    {
        IMLOGE1("[Start] invalid codec[%d]", mCodecType);
        return false;
    }

    const uint64_t maxInputSize = static_cast<uint64_t>(mWidth) * mHeight * 10;
    if (mWidth == 0 || mHeight == 0 ||
            maxInputSize > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))
    {
        IMLOGE2("[Start] invalid resolution[%ux%u]", mWidth, mHeight);
        return false;
    }

    mFormat = AMediaFormat_new();
    if (mFormat == nullptr)
    {
        IMLOGE0("[Start] Unable to create media format");
        return false;
    }

    AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_WIDTH, mWidth);
    AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_HEIGHT, mHeight);

    const char* kMimeType = mCodecType == kVideoCodecHevc ? "video/hevc" : "video/avc";

    AMediaFormat_setString(mFormat, AMEDIAFORMAT_KEY_MIME, kMimeType);
    AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_COLOR_FORMAT,
            21);  // #21 : COLOR_FormatYUV420SemiPlanar
    AMediaFormat_setInt32(
            mFormat, AMEDIAFORMAT_KEY_MAX_INPUT_SIZE, static_cast<int32_t>(maxInputSize));
    AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_ROTATION, mFarOrientationDegree);

    mCodec = AMediaCodec_createDecoderByType(kMimeType);

    if (mCodec == nullptr)
    {
        IMLOGE0("[Start] Unable to create decoder");
        AMediaFormat_delete(mFormat);
        mFormat = nullptr;
        return false;
    }

    if (mWindow != nullptr)
    {
        ANativeWindow_acquire(mWindow);
        mAcquiredWindow = mWindow;
    }

    media_status_t err = AMediaCodec_configure(mCodec, mFormat, mAcquiredWindow, nullptr, 0);

    if (err != AMEDIA_OK)
    {
        IMLOGE1("[Start] configure error[%d]", err);
        AMediaCodec_delete(mCodec);
        mCodec = nullptr;
        AMediaFormat_delete(mFormat);
        mFormat = nullptr;
        if (mAcquiredWindow != nullptr)
        {
            ANativeWindow_release(mAcquiredWindow);
            mAcquiredWindow = nullptr;
        }
        return false;
    }

    err = AMediaCodec_start(mCodec);

    if (err != AMEDIA_OK)
    {
        IMLOGE1("[Start] codec start[%d]", err);
        AMediaCodec_delete(mCodec);
        mCodec = nullptr;
        AMediaFormat_delete(mFormat);
        mFormat = nullptr;
        if (mAcquiredWindow != nullptr)
        {
            ANativeWindow_release(mAcquiredWindow);
            mAcquiredWindow = nullptr;
        }
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mStopped = false;
    }

    const int threadError = pthread_create(&mRenderThread, nullptr, RunRendererThread, this);
    if (threadError != 0)
    {
        IMLOGE1("[Start] unable to create renderer thread: %s", std::strerror(threadError));
        std::lock_guard<std::mutex> lock(mMutex);
        mStopped = true;
        AMediaCodec_stop(mCodec);
        AMediaCodec_delete(mCodec);
        mCodec = nullptr;
        AMediaFormat_delete(mFormat);
        mFormat = nullptr;
        if (mAcquiredWindow != nullptr)
        {
            ANativeWindow_release(mAcquiredWindow);
            mAcquiredWindow = nullptr;
        }
        return false;
    }
    mRenderThreadStarted = true;

    return true;
}

void ImsMediaVideoRenderer::Stop()
{
    IMLOGD0("[Stop]");

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mStopped = true;
    }

    if (mRenderThreadStarted)
    {
        const int threadError = pthread_join(mRenderThread, nullptr);
        if (threadError != 0)
        {
            IMLOGE1("[Stop] unable to join renderer thread: %s", std::strerror(threadError));
            return;
        }
        mRenderThreadStarted = false;
    }

    if (mCodec != nullptr)
    {
        AMediaCodec_stop(mCodec);
        AMediaCodec_delete(mCodec);
        mCodec = nullptr;
    }

    if (mAcquiredWindow != nullptr)
    {
        ANativeWindow_release(mAcquiredWindow);
        mAcquiredWindow = nullptr;
    }

    if (mFormat != nullptr)
    {
        AMediaFormat_delete(mFormat);
        mFormat = nullptr;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    while (!mFrameDatas.empty())
    {
        delete mFrameDatas.front();
        mFrameDatas.pop_front();
    }
}

void ImsMediaVideoRenderer::OnDataFrame(
        uint8_t* buffer, uint32_t size, uint32_t timestamp, const bool isConfigFrame)
{
    if (size == 0 || buffer == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    if (mStopped)
    {
        return;
    }

    if (mFrameDatas.size() >= MAX_FRAME_QUEUE_SIZE)
    {
        IMLOGW1("[OnDataFrame] renderer queue full[%zu], dropping frame", mFrameDatas.size());
        return;
    }

    IMLOGD_PACKET2(IM_PACKET_LOG_VIDEO, "[OnDataFrame] frame size[%u], list[%zu]", size,
            mFrameDatas.size());
    FrameData* frame = new (std::nothrow) FrameData(buffer, size, timestamp, isConfigFrame);
    if (frame == nullptr || frame->data == nullptr)
    {
        IMLOGE0("[OnDataFrame] unable to allocate frame buffer");
        delete frame;
        return;
    }
    mFrameDatas.push_back(frame);
}

void ImsMediaVideoRenderer::processBuffers()
{
    uint32_t nextTime = ImsMediaTimer::GetTimeInMilliSeconds();
    uint32_t timeDiff = 0;

    IMLOGD1("[processBuffers] enter time[%u]", nextTime);

    while (true)
    {
        FrameData* frame = nullptr;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            if (mStopped)
            {
                break;
            }
            if (!mFrameDatas.empty())
            {
                frame = mFrameDatas.front();
            }
        }

        if (frame == nullptr)
        {
            nextTime = ImsMediaTimer::GetTimeInMilliSeconds();
            ImsMediaTimer::Sleep(1);
            continue;
        }

        auto index = AMediaCodec_dequeueInputBuffer(mCodec, CODEC_TIMEOUT_NANO);

        if (index >= 0)
        {
            size_t bufferSize = 0;
            uint8_t* inputBuffer = AMediaCodec_getInputBuffer(mCodec, index, &bufferSize);

            if (inputBuffer != nullptr && frame->size <= bufferSize)
            {
                std::memcpy(inputBuffer, frame->data, frame->size);
                IMLOGD_PACKET4(IM_PACKET_LOG_VIDEO,
                        "[processBuffers] queue input buffer index[%d], size[%d], TS[%d], "
                        "config[%d]",
                        index, frame->size, frame->timestamp, frame->isConfig);

                media_status_t err = AMediaCodec_queueInputBuffer(mCodec, index, 0, frame->size,
                        static_cast<uint64_t>(frame->timestamp) * 1000, 0);

                if (err != AMEDIA_OK)
                {
                    IMLOGE1("[processBuffers] Unable to queue input buffers - err[%d]", err);
                }
            }
            else
            {
                IMLOGE2("[processBuffers] invalid input size[%u], capacity[%zu]", frame->size,
                        bufferSize);
                AMediaCodec_queueInputBuffer(mCodec, index, 0, 0, 0, 0);
            }

            std::lock_guard<std::mutex> lock(mMutex);
            if (!mFrameDatas.empty() && mFrameDatas.front() == frame)
            {
                delete frame;
                mFrameDatas.pop_front();
            }
        }

        AMediaCodecBufferInfo info{};
        index = AMediaCodec_dequeueOutputBuffer(mCodec, &info, CODEC_TIMEOUT_NANO);

        if (index >= 0)
        {
            IMLOGD_PACKET5(IM_PACKET_LOG_VIDEO,
                    "[processBuffers] index[%d], size[%d], offset[%d], time[%ld], flags[%d]", index,
                    info.size, info.offset, info.presentationTimeUs, info.flags);

            AMediaCodec_releaseOutputBuffer(mCodec, index, true);
        }
        else if (index == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)
        {
            IMLOGD0("[processBuffers] output buffer changed");
        }
        else if (index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
        {
            if (mFormat != nullptr)
            {
                AMediaFormat_delete(mFormat);
            }
            mFormat = AMediaCodec_getOutputFormat(mCodec);
            if (mFormat != nullptr)
            {
                IMLOGD1("[processBuffers] format changed, format[%s]",
                        AMediaFormat_toString(mFormat));
            }
            else
            {
                IMLOGE0("[processBuffers] unable to read output format");
            }
        }
        else if (index == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
        {
            IMLOGD0("[processBuffers] no output buffer");
        }
        else
        {
            IMLOGD1("[processBuffers] unexpected index[%d]", index);
        }

        nextTime += INTERVAL_MILLIS;
        uint32_t nCurrTime = ImsMediaTimer::GetTimeInMilliSeconds();

        if (nextTime > nCurrTime)
        {
            timeDiff = nextTime - nCurrTime;
            IMLOGD_PACKET1(IM_PACKET_LOG_VIDEO, "[processBuffers] timeDiff[%u]", timeDiff);
            ImsMediaTimer::Sleep(timeDiff);
        }
    }

    IMLOGD0("[processBuffers] exit");
}

void ImsMediaVideoRenderer::UpdateDeviceOrientation(uint32_t degree)
{
    IMLOGD1("[UpdateDeviceOrientation] orientation[%d]", degree);
    mNearOrientationDegree = degree;
}

void ImsMediaVideoRenderer::UpdatePeerOrientation(uint32_t degree)
{
    IMLOGD1("[UpdatePeerOrientation] orientation[%d]", degree);

    if (mFarOrientationDegree != degree)
    {
        Stop();
        mFarOrientationDegree = degree;
        Start();
    }
}

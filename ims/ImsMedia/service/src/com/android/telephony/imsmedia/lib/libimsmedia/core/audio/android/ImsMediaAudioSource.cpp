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

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ImsMediaDefine.h>
#include <ImsMediaTimer.h>
#include <ImsMediaTrace.h>
#include <ImsMediaAudioUtil.h>
#include <ImsMediaAudioSource.h>
#include <utils/Errors.h>

#include <chrono>
#define AAUDIO_STATE_TIMEOUT_NANO (100 * 1000000L)
#define AAUDIO_START_TIMEOUT_NANO (10 * AAUDIO_STATE_TIMEOUT_NANO)
#define AAUDIO_RESTART_RETRY_DELAY_MS (100)
#define AAUDIO_RESTART_START_WAIT_ATTEMPTS (3)
#define AAUDIO_RECOVERY_STOP_TIMEOUT_MS (4000)
#define NUM_FRAMES_PER_SEC        (50)
#define DEFAULT_SAMPLING_RATE     (8000)
#define CODEC_TIMEOUT_NANO        (100000)

using namespace android;

ImsMediaAudioSource::ImsMediaAudioSource()
{
    mAudioStream = nullptr;
    mCodec = nullptr;
    mFormat = nullptr;
    mCallback = nullptr;
    mCodecType = -1;
    mMode = 0;
    mPtime = 0;
    mSamplingRate = DEFAULT_SAMPLING_RATE;
    mBufferSize = 0;
    mEvsBandwidth = kEvsBandwidthNone;
    mEvsBitRate = 0;
    mEvsChAwOffset = 0;
    mIsEvsInitialized = false;
    mMediaDirection = 0;
    mIsDtxEnabled = false;
    mIsOctetAligned = false;
    mDisconnectedAudioStream.store(nullptr);
    mMutexUplink.setTimeout(std::chrono::milliseconds(AAUDIO_RECOVERY_STOP_TIMEOUT_MS));
}

ImsMediaAudioSource::~ImsMediaAudioSource()
{
    Stop();
}

void ImsMediaAudioSource::SetUplinkCallback(IFrameCallback* callback)
{
    ImsMediaMutex::Autolock lock(mMutexUplink);
    mCallback = callback;
}

void ImsMediaAudioSource::SetCodec(int32_t type)
{
    IMLOGD1("[SetCodec] type[%d]", type);
    mCodecType = type;
}

void ImsMediaAudioSource::SetCodecMode(uint32_t mode)
{
    IMLOGD1("[SetCodecMode] mode[%d]", mode);
    mMode = mode;
}

void ImsMediaAudioSource::SetEvsBitRate(uint32_t bitrate)
{
    IMLOGD1("[SetEvsBitRate] bitrate[%d]", bitrate);
    mEvsBitRate = bitrate;
}

void ImsMediaAudioSource::SetSamplingRate(int32_t samplingRate)
{
    mSamplingRate = samplingRate;
}

void ImsMediaAudioSource::SetEvsChAwOffset(int32_t offset)
{
    mEvsChAwOffset = offset;
}

void ImsMediaAudioSource::SetPtime(uint32_t time)
{
    IMLOGD1("[SetPtime] Ptime[%d]", time);
    mPtime = time;
}

void ImsMediaAudioSource::SetEvsBandwidth(int32_t evsBandwidth)
{
    mEvsBandwidth = (kEvsBandwidth)evsBandwidth;
}

void ImsMediaAudioSource::SetMediaDirection(int32_t direction)
{
    mMediaDirection = direction;
}

void ImsMediaAudioSource::SetDtxEnabled(bool isDtxEnabled)
{
    mIsDtxEnabled = isDtxEnabled;
}

void ImsMediaAudioSource::SetOctetAligned(bool isOctetAligned)
{
    mIsOctetAligned = isOctetAligned;
}

bool ImsMediaAudioSource::Start()
{
    if (!IsThreadStopped() || mAudioStream != nullptr || mCodec != nullptr || mFormat != nullptr)
    {
        IMLOGE0("[Start] audio source is already running");
        return false;
    }

    if (mPtime == 0 || mSamplingRate <= 0)
    {
        IMLOGE2("[Start] invalid packetization interval[%u] or sampling rate[%d]", mPtime,
                mSamplingRate);
        return false;
    }

    mDisconnectedAudioStream.store(nullptr);
    openAudioStream();

    if (mAudioStream == nullptr)
    {
        IMLOGE0("[Start] create audio stream failed");
        return false;
    }

    // configure and start codec
    if (!startCodec())
    {
        IMLOGE0("[Start] start codec failed");
        AAudioStream_close(mAudioStream);
        mAudioStream = nullptr;
        return false;
    }

    // start audio
    auto audioResult = AAudioStream_requestStart(mAudioStream);

    if (audioResult != AAUDIO_OK)
    {
        IMLOGE1("[Start] Error start stream[%s]", AAudio_convertResultToText(audioResult));

        stopCodec();
        AAudioStream_close(mAudioStream);
        mAudioStream = nullptr;
        return false;
    }

    aaudio_stream_state_t inputState = AAUDIO_STREAM_STATE_STARTING;
    aaudio_stream_state_t nextState = AAUDIO_STREAM_STATE_UNINITIALIZED;
    audioResult = AAudioStream_waitForStateChange(
            mAudioStream, inputState, &nextState, AAUDIO_START_TIMEOUT_NANO);

    if (audioResult != AAUDIO_OK || nextState != AAUDIO_STREAM_STATE_STARTED)
    {
        IMLOGE2("[Start] Error start stream[%s], state[%s]",
                AAudio_convertResultToText(audioResult),
                AAudio_convertStreamStateToText(nextState));

        AAudioStream_requestStop(mAudioStream);
        AAudioStream_close(mAudioStream);
        mAudioStream = nullptr;
        stopCodec();
        return false;
    }

    IMLOGI1("[Start] start stream state[%s]", AAudio_convertStreamStateToText(nextState));

    // start audio read thread
    mConditionExit.reset();
    if (!StartThread("ImsMediaAudioSource"))
    {
        IMLOGE0("[Start] audio read thread failed to start");
        Stop();
        return false;
    }
    return true;
}

void ImsMediaAudioSource::Stop()
{
    IMLOGD0("[Stop]");
    if (!IsThreadStopped())
    {
        StopThread();
        mConditionExit.wait_timeout(AAUDIO_RECOVERY_STOP_TIMEOUT_MS);
    }
    JoinThread();

    ImsMediaMutex::Autolock lock(mMutexUplink);

    if (mAudioStream != nullptr)
    {
        aaudio_stream_state_t inputState = AAUDIO_STREAM_STATE_STOPPING;
        aaudio_stream_state_t nextState = AAUDIO_STREAM_STATE_UNINITIALIZED;
        aaudio_result_t result = AAudioStream_requestStop(mAudioStream);

        if (result != AAUDIO_OK)
        {
            IMLOGE1("[Stop] Error stop stream[%s]", AAudio_convertResultToText(result));
        }

        // TODO: if it causes extra delay in stop, optimize later
        result = AAudioStream_waitForStateChange(
                mAudioStream, inputState, &nextState, AAUDIO_STATE_TIMEOUT_NANO);

        if (result != AAUDIO_OK)
        {
            IMLOGE1("[Stop] Error stop stream[%s]", AAudio_convertResultToText(result));
        }

        IMLOGI1("[Stop] stream state[%s]", AAudio_convertStreamStateToText(nextState));

        AAudioStream_close(mAudioStream);
        mAudioStream = nullptr;
    }

    stopCodec();
    mDisconnectedAudioStream.store(nullptr);
}

void ImsMediaAudioSource::ProcessCmr(const uint32_t cmr)
{
    IMLOGI1("[ProcessCmr] cmr[%d]", cmr);

    if (IsThreadStopped())
    {
        return;
    }

    ImsMediaMutex::Autolock lock(mMutexUplink);
    mMode = cmr;
    stopCodec();
    startCodec();
}

void ImsMediaAudioSource::audioErrorCallback(
        AAudioStream* stream, void* userData, aaudio_result_t error)
{
    if (stream == nullptr || userData == nullptr)
    {
        return;
    }

    aaudio_stream_state_t streamState = AAudioStream_getState(stream);
    IMLOGW2("[errorCallback] error[%s], state[%d]", AAudio_convertResultToText(error), streamState);

    if (error == AAUDIO_ERROR_DISCONNECTED)
    {
        reinterpret_cast<ImsMediaAudioSource*>(userData)
                ->mDisconnectedAudioStream.store(stream);
    }
}

void* ImsMediaAudioSource::run()
{
    IMLOGD0("[run] enter");
    uint32_t nNextTime = ImsMediaTimer::GetTimeInMilliSeconds();
    int16_t buffer[PCM_BUFFER_SIZE];
    uint8_t l16Buffer[PCM_BUFFER_SIZE * sizeof(buffer[0])];
    uint32_t evsFlags = 2;
    uint8_t outputBuf[PCM_BUFFER_SIZE];
    int size = 0;

    outputBuf[0] = 0;

    for (;;)
    {
        if (IsThreadStopped())
        {
            IMLOGD0("[run] terminated");
            break;
        }

        AAudioStream* disconnectedStream = mDisconnectedAudioStream.exchange(nullptr);
        if (disconnectedStream != nullptr)
        {
            restartAudioStream(disconnectedStream);
            if (IsThreadStopped())
            {
                break;
            }
            nNextTime = ImsMediaTimer::GetTimeInMilliSeconds();
        }

        mMutexUplink.lock();

        if (mAudioStream != nullptr &&
                AAudioStream_getState(mAudioStream) == AAUDIO_STREAM_STATE_STARTED)
        {
            aaudio_result_t readSize = AAudioStream_read(mAudioStream, buffer, mBufferSize, 0);

            if (readSize > 0)
            {
                IMLOGD_PACKET1(IM_PACKET_LOG_AUDIO, "[run] nReadSize[%d]", readSize);
                const int64_t ptsUsec = ImsMediaTimer::GetTimeInMicroSeconds();
                if (mCodecType == kAudioCodecAmr || mCodecType == kAudioCodecAmrWb)
                {
                    queueInputBuffer(buffer,
                            static_cast<uint32_t>(readSize) * sizeof(buffer[0]));
                    dequeueOutputBuffer();
                }
                else if (mCodecType == kAudioCodecEvs)
                {
                    // TODO: Integration with libEVS is required.
                    if (!mIsEvsInitialized)
                    {
                        mIsEvsInitialized = true;
                    }

                    if (mCallback != nullptr && outputBuf[0] != 0)
                    {
                        // TODO: integration with libEVS is require to encode outputBuf
                        mCallback->onDataFrame(outputBuf, size, ptsUsec, evsFlags);
                    }
                    size = 0;
                }
                else if (mCodecType == kAudioCodecL16)
                {
                    for (aaudio_result_t i = 0; i < readSize; ++i)
                    {
                        const uint16_t sample = static_cast<uint16_t>(buffer[i]);
                        l16Buffer[i * 2] = static_cast<uint8_t>(sample >> 8);
                        l16Buffer[i * 2 + 1] = static_cast<uint8_t>(sample);
                    }

                    if (mCallback != nullptr)
                    {
                        mCallback->onDataFrame(
                                l16Buffer, static_cast<uint32_t>(readSize) * sizeof(buffer[0]),
                                ptsUsec, evsFlags);
                    }
                    size = 0;
                }
            }
        }

        mMutexUplink.unlock();

        nNextTime += mPtime;
        uint32_t nCurrTime = ImsMediaTimer::GetTimeInMilliSeconds();

        if (nNextTime > nCurrTime)
        {
            ImsMediaTimer::Sleep(nNextTime - nCurrTime);
        }
    }

    mConditionExit.signal();
    return nullptr;
}

void ImsMediaAudioSource::openAudioStream()
{
    const aaudio_sharing_mode_t sharingModes[] = {
            AAUDIO_SHARING_MODE_EXCLUSIVE, AAUDIO_SHARING_MODE_SHARED};
    aaudio_result_t result = AAUDIO_ERROR_UNAVAILABLE;

    for (aaudio_sharing_mode_t sharingMode : sharingModes)
    {
        AAudioStreamBuilder* builder = nullptr;
        result = AAudio_createStreamBuilder(&builder);

        if (result != AAUDIO_OK)
        {
            IMLOGE1("[openAudioStream] Error creating stream builder[%s]",
                    AAudio_convertResultToText(result));
            return;
        }

        AAudioStreamBuilder_setInputPreset(builder, AAUDIO_INPUT_PRESET_VOICE_COMMUNICATION);
        AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_INPUT);
        AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
        AAudioStreamBuilder_setChannelCount(builder, 1);
        AAudioStreamBuilder_setSampleRate(builder, mSamplingRate);
        AAudioStreamBuilder_setSharingMode(builder, sharingMode);
        // Call capture needs the voice-communication preprocessing chain. Low latency without
        // a session requests RAW capture on the legacy backend and also allows MMAP capture.
        AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_NONE);
        AAudioStreamBuilder_setSessionId(builder, AAUDIO_SESSION_ID_ALLOCATE);
        AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_VOICE_COMMUNICATION);
        AAudioStreamBuilder_setErrorCallback(builder, audioErrorCallback, this);
        AAudioStreamBuilder_setPrivacySensitive(builder, true);

        result = AAudioStreamBuilder_openStream(builder, &mAudioStream);
        AAudioStreamBuilder_delete(builder);

        if (result == AAUDIO_OK && mAudioStream != nullptr)
        {
            IMLOGI1("[openAudioStream] sharingMode[%d]", sharingMode);
            break;
        }

        IMLOGW2("[openAudioStream] Failed sharingMode[%d], error[%s]",
                sharingMode, AAudio_convertResultToText(result));
        if (mAudioStream != nullptr)
        {
            AAudioStream_close(mAudioStream);
            mAudioStream = nullptr;
        }
    }

    if (mAudioStream == nullptr)
    {
        IMLOGE1("[openAudioStream] Failed to openStream. Error[%s]",
                AAudio_convertResultToText(result));
        return;
    }

    int32_t framesPerBurst = AAudioStream_getFramesPerBurst(mAudioStream);
    if (framesPerBurst <= 0 || framesPerBurst > PCM_BUFFER_SIZE)
    {
        IMLOGE2("[openAudioStream] invalid framesPerBurst[%d], capacity[%d]",
                framesPerBurst, PCM_BUFFER_SIZE);
        AAudioStream_close(mAudioStream);
        mAudioStream = nullptr;
        mBufferSize = 0;
        return;
    }
    mBufferSize = static_cast<uint32_t>(framesPerBurst);
    IMLOGD3("[openAudioStream] samplingRate[%d], framesPerBurst[%d], "
            "performanceMode[%d]",
            AAudioStream_getSampleRate(mAudioStream), mBufferSize,
            AAudioStream_getPerformanceMode(mAudioStream));
    // Set the buffer size to the burst size - this will give us the minimum
    // possible latency
    AAudioStream_setBufferSizeInFrames(mAudioStream, mBufferSize);
}

void ImsMediaAudioSource::restartAudioStream(AAudioStream* disconnectedStream)
{
    {
        ImsMediaMutex::Autolock lock(mMutexUplink);

        if (mAudioStream != disconnectedStream)
        {
            IMLOGI0("[restartAudioStream] Ignore stale disconnect");
            return;
        }

        AAudioStream_requestStop(mAudioStream);
        AAudioStream_close(mAudioStream);
        mAudioStream = nullptr;
    }

    uint32_t attempt = 0;
    while (!IsThreadStopped())
    {
        ++attempt;
        {
            ImsMediaMutex::Autolock lock(mMutexUplink);
            openAudioStream();

            if (mAudioStream != nullptr)
            {
                aaudio_stream_state_t inputState = AAUDIO_STREAM_STATE_STARTING;
                aaudio_stream_state_t nextState = AAUDIO_STREAM_STATE_UNINITIALIZED;
                aaudio_result_t result = AAudioStream_requestStart(mAudioStream);

                if (result == AAUDIO_OK)
                {
                    for (uint32_t waitAttempt = 0;
                            waitAttempt < AAUDIO_RESTART_START_WAIT_ATTEMPTS; ++waitAttempt)
                    {
                        result = AAudioStream_waitForStateChange(
                                mAudioStream, inputState, &nextState,
                                AAUDIO_START_TIMEOUT_NANO);
                        if (result != AAUDIO_ERROR_TIMEOUT ||
                                nextState != AAUDIO_STREAM_STATE_STARTING)
                        {
                            break;
                        }

                        IMLOGW2("[restartAudioStream] Attempt[%u] still in state[%s]",
                                attempt, AAudio_convertStreamStateToText(nextState));
                    }
                }

                if (result == AAUDIO_OK && nextState == AAUDIO_STREAM_STATE_STARTED)
                {
                    IMLOGI2("[restartAudioStream] Recovered on attempt[%u], state[%s]",
                            attempt, AAudio_convertStreamStateToText(nextState));
                    return;
                }

                IMLOGE3("[restartAudioStream] Attempt[%u] failed[%s], state[%s]",
                        attempt, AAudio_convertResultToText(result),
                        AAudio_convertStreamStateToText(nextState));
                AAudioStream* failedStream = mAudioStream;
                AAudioStream_requestStop(failedStream);
                AAudioStream_close(failedStream);
                mAudioStream = nullptr;

                AAudioStream* pendingStream = failedStream;
                mDisconnectedAudioStream.compare_exchange_strong(pendingStream, nullptr);
            }
            else
            {
                IMLOGE1("[restartAudioStream] Attempt[%u] failed to open", attempt);
            }
        }

        if (!IsThreadStopped())
        {
            ImsMediaTimer::Sleep(AAUDIO_RESTART_RETRY_DELAY_MS);
        }
    }

    IMLOGI0("[restartAudioStream] Recovery stopped");
}

bool ImsMediaAudioSource::startCodec()
{
    char kMimeType[128] = {'\0'};
    int amrBitrate = 0;
    // TODO: Integration with libEVS is required.
    ImsMediaAudioUtil::ConvertEvsBandwidthToStr(mEvsBandwidth, mEvsbandwidthStr, MAX_EVS_BW_STRLEN);

    switch (mCodecType)
    {
        case kAudioCodecAmr:
            sprintf(kMimeType, "audio/3gpp");
            amrBitrate = ImsMediaAudioUtil::ConvertAmrModeToBitrate(mMode);
            break;
        case kAudioCodecAmrWb:
            sprintf(kMimeType, "audio/amr-wb");
            amrBitrate = ImsMediaAudioUtil::ConvertAmrWbModeToBitrate(mMode);
            break;
        case kAudioCodecEvs:
            IMLOGE0("[startCodec] EVS encoding is not implemented");
            return false;
        case kAudioCodecL16:
            break;
        default:
            return false;
    }

    IMLOGD1("[startCodec] codec type[%s]", kMimeType);

    if (mCodecType == kAudioCodecAmr || mCodecType == kAudioCodecAmrWb)
    {
        mFormat = AMediaFormat_new();
        if (mFormat == nullptr)
        {
            IMLOGE0("[startCodec] unable to create media format");
            return false;
        }
        AMediaFormat_setString(mFormat, AMEDIAFORMAT_KEY_MIME, kMimeType);
        AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, mSamplingRate);
        AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, 1);
        AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_BIT_RATE, amrBitrate);

        mCodec = AMediaCodec_createEncoderByType(kMimeType);

        if (mCodec == nullptr)
        {
            IMLOGE1("[startCodec] unable to create %s codec instance", kMimeType);
            AMediaFormat_delete(mFormat);
            mFormat = nullptr;
            return false;
        }

        IMLOGD0("[startCodec] configure codec");
        media_status_t codecResult = AMediaCodec_configure(
                mCodec, mFormat, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);

        if (codecResult != AMEDIA_OK)
        {
            IMLOGE2("[startCodec] unable to configure[%s] codec - err[%d]", kMimeType, codecResult);
            AMediaCodec_delete(mCodec);
            mCodec = nullptr;
            AMediaFormat_delete(mFormat);
            mFormat = nullptr;
            return false;
        }

        codecResult = AMediaCodec_start(mCodec);

        if (codecResult != AMEDIA_OK)
        {
            IMLOGE1("[Start] unable to start codec - err[%d]", codecResult);
            AMediaCodec_delete(mCodec);
            mCodec = nullptr;
            AMediaFormat_delete(mFormat);
            mFormat = nullptr;
            return false;
        }
    }
    return true;
}

void ImsMediaAudioSource::stopCodec()
{
    if (mCodec != nullptr)
    {
        AMediaCodec_stop(mCodec);
        AMediaCodec_delete(mCodec);
        mCodec = nullptr;
    }

    if (mFormat != nullptr)
    {
        AMediaFormat_delete(mFormat);
        mFormat = nullptr;
    }
}

void ImsMediaAudioSource::queueInputBuffer(int16_t* buffer, uint32_t size)
{
    if (mCodec == nullptr)
    {
        return;
    }

    ssize_t index = AMediaCodec_dequeueInputBuffer(mCodec, 0);

    if (index >= 0)
    {
        size_t bufferSize = 0;
        uint8_t* inputBuffer = AMediaCodec_getInputBuffer(mCodec, index, &bufferSize);

        if (inputBuffer == nullptr || size > bufferSize)
        {
            IMLOGE2("[queueInputBuffer] invalid input size[%u], capacity[%zu]", size,
                    bufferSize);
            auto err = AMediaCodec_queueInputBuffer(
                    mCodec, index, 0, 0, ImsMediaTimer::GetTimeInMicroSeconds(), 0);
            if (err != AMEDIA_OK)
            {
                IMLOGE1("[queueInputBuffer] Unable to return input buffer - err[%d]", err);
            }
        }
        else
        {
            memcpy(inputBuffer, buffer, size);
            IMLOGD_PACKET2(IM_PACKET_LOG_AUDIO,
                    "[queueInputBuffer] queue input buffer index[%d], size[%d]", index, size);

            auto err = AMediaCodec_queueInputBuffer(
                    mCodec, index, 0, size, ImsMediaTimer::GetTimeInMicroSeconds(), 0);

            if (err != AMEDIA_OK)
            {
                IMLOGE1("[queueInputBuffer] Unable to queue input buffers - err[%d]", err);
            }
        }
    }
}

void ImsMediaAudioSource::dequeueOutputBuffer()
{
    AMediaCodecBufferInfo info;
    auto index = AMediaCodec_dequeueOutputBuffer(mCodec, &info, CODEC_TIMEOUT_NANO);

    if (index >= 0)
    {
        IMLOGD_PACKET5(IM_PACKET_LOG_AUDIO,
                "[dequeueOutputBuffer] index[%d], size[%d], offset[%d], time[%ld], flags[%d]",
                index, info.size, info.offset, info.presentationTimeUs, info.flags);

        if (info.size > 0)
        {
            size_t buffCapacity = 0;
            uint8_t* buf = AMediaCodec_getOutputBuffer(mCodec, index, &buffCapacity);

            if (buf != nullptr && info.offset >= 0 &&
                    static_cast<size_t>(info.offset) <= buffCapacity &&
                    static_cast<size_t>(info.size) <=
                            buffCapacity - static_cast<size_t>(info.offset))
            {
                if (mCallback != nullptr)
                {
                    mCallback->onDataFrame(
                            buf + info.offset, info.size, info.presentationTimeUs, info.flags);
                }
            }
            else
            {
                IMLOGE3("[dequeueOutputBuffer] invalid offset[%d], size[%d], capacity[%zu]",
                        info.offset, info.size, buffCapacity);
            }
        }

        AMediaCodec_releaseOutputBuffer(mCodec, index, false);
    }
    else if (index == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)
    {
        IMLOGD0("[dequeueOutputBuffer] Encoder output buffer changed");
    }
    else if (index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
    {
        if (mFormat != nullptr)
        {
            AMediaFormat_delete(mFormat);
        }

        mFormat = AMediaCodec_getOutputFormat(mCodec);
        IMLOGD1("[dequeueOutputBuffer] Encoder format changed, format[%s]",
                AMediaFormat_toString(mFormat));
    }
    else if (index == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
    {
        IMLOGD0("[dequeueOutputBuffer] no output buffer");
    }
    else
    {
        IMLOGD1("[dequeueOutputBuffer] unexpected index[%d]", index);
    }
}

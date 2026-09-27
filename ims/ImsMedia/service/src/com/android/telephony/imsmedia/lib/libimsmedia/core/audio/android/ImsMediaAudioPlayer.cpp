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
// #include <sys/timeb.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <chrono>

#include <cutils/properties.h>

#include <ImsMediaDefine.h>
#include <ImsMediaTrace.h>
#include <ImsMediaTimer.h>
#include <ImsMediaAudioUtil.h>
#include <ImsMediaAudioPlayer.h>
#include <utils/Errors.h>

#define AAUDIO_STATE_TIMEOUT_NANO (100 * 1000000L)
#define AAUDIO_START_TIMEOUT_NANO (10 * AAUDIO_STATE_TIMEOUT_NANO)
#define AUDIO_FRAME_DURATION_NANO (20 * 1000000L)
#define NUM_FRAMES_PER_SEC        (50)
#define DEFAULT_SAMPLING_RATE     (8000)
#define CODEC_TIMEOUT_NANO        (100000)

static constexpr char kLowLatencyPlaybackProperty[] =
        "persist.radio.imsmedia.low_latency_playback";
// RFC 3267 storage headers with Q set and no following speech data.
static constexpr uint8_t kAmrNoDataFrameHeader = 0x7C;
static constexpr uint8_t kAmrWbSpeechLostFrameHeader = 0x74;
static constexpr uint64_t kPlaybackRecoveryIntervalUs = 100000;

using namespace android;

ImsMediaAudioPlayer::ImsMediaAudioPlayer()
{
    mAudioStream = nullptr;
    mPlaybackActive = false;
    mNextRecoveryTimeUs = 0;
    mCodec = nullptr;
    mFormat = nullptr;
    mCodecType = 0;
    mCodecMode = 0;
    mSamplingRate = DEFAULT_SAMPLING_RATE;
    mEvsChAwOffset = 0;
    mEvsBandwidth = kEvsBandwidthNone;
    memset(mBuffer, 0, sizeof(mBuffer));
    mEvsBitRate = 0;
    mEvsCodecHeaderMode = kRtpPayloadHeaderModeEvsHeaderFull;
    mIsOctetAligned = false;
    mIsDtxEnabled = false;
    mDisconnectedAudioStream.store(nullptr);
    mMutex.setTimeout(std::chrono::milliseconds(3000));
}

ImsMediaAudioPlayer::~ImsMediaAudioPlayer()
{
    Stop();
}

void ImsMediaAudioPlayer::SetCodec(int32_t type)
{
    IMLOGD_PACKET1(IM_PACKET_LOG_AUDIO, "[SetCodec] type[%d]", type);
    mCodecType = type;
}

void ImsMediaAudioPlayer::SetEvsBitRate(int32_t bitRate)
{
    mEvsBitRate = bitRate;
    IMLOGD_PACKET1(IM_PACKET_LOG_AUDIO, "[SetEvsBitRate] mEvsBitRate[%d]", mEvsBitRate);
}

void ImsMediaAudioPlayer::SetEvsChAwOffset(int32_t offset)
{
    mEvsChAwOffset = offset;
}

void ImsMediaAudioPlayer::SetSamplingRate(int32_t samplingRate)
{
    mSamplingRate = samplingRate;
}

void ImsMediaAudioPlayer::SetEvsBandwidth(int32_t evsBandwidth)
{
    mEvsBandwidth = (kEvsBandwidth)evsBandwidth;
}

void ImsMediaAudioPlayer::SetEvsPayloadHeaderMode(int32_t EvsPayloadHeaderMode)
{
    mEvsCodecHeaderMode = (kRtpPayloadHeaderMode)EvsPayloadHeaderMode;
}

void ImsMediaAudioPlayer::SetCodecMode(uint32_t mode)
{
    IMLOGD1("[SetCodecMode] mode[%d]", mode);
    mCodecMode = mode;
}

void ImsMediaAudioPlayer::SetDtxEnabled(bool isDtxEnabled)
{
    mIsDtxEnabled = isDtxEnabled;
}

void ImsMediaAudioPlayer::SetOctetAligned(bool isOctetAligned)
{
    mIsOctetAligned = isOctetAligned;
}

void ImsMediaAudioPlayer::ProcessCmr(const uint32_t cmr)
{
    IMLOGD1("[ProcessCmr] cmr[%d]", cmr);

    mCodecMode = cmr;
    Stop();
    Start();
}

bool ImsMediaAudioPlayer::Start()
{
    ImsMediaMutex::Autolock lock(mMutex);
    if (mPlaybackActive || mAudioStream != nullptr || mCodec != nullptr ||
            mFormat != nullptr || mSamplingRate <= 0)
    {
        IMLOGE0("[Start] audio player is already running or has invalid configuration");
        return false;
    }
    mDisconnectedAudioStream.store(nullptr);
    mNextRecoveryTimeUs = 0;

    char kMimeType[128] = {'\0'};
    switch (mCodecType)
    {
        case kAudioCodecAmr:
            sprintf(kMimeType, "audio/3gpp");
            break;
        case kAudioCodecAmrWb:
            sprintf(kMimeType, "audio/amr-wb");
            break;
        case kAudioCodecEvs:
            IMLOGE0("[Start] EVS decoding is not implemented");
            return false;
        case kAudioCodecL16:
            sprintf(kMimeType, "audio/l16");
            break;
        default:
            return false;
    }

    openAudioStream();

    if (mAudioStream == nullptr)
    {
        IMLOGE0("[Start] create audio stream failed");
        return false;
    }

    IMLOGD1("[Start] Creating codec[%s]", kMimeType);

    if (mCodecType == kAudioCodecAmr || mCodecType == kAudioCodecAmrWb)
    {
        mFormat = AMediaFormat_new();
        if (mFormat == nullptr)
        {
            IMLOGE0("[Start] unable to create media format");
            AAudioStream_close(mAudioStream);
            mAudioStream = nullptr;
            return false;
        }
        AMediaFormat_setString(mFormat, AMEDIAFORMAT_KEY_MIME, kMimeType);
        AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, mSamplingRate);
        AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, 1);

        mCodec = AMediaCodec_createDecoderByType(kMimeType);

        if (mCodec == nullptr)
        {
            IMLOGE1("[Start] unable to create %s codec instance", kMimeType);
            AMediaFormat_delete(mFormat);
            mFormat = nullptr;
            AAudioStream_close(mAudioStream);
            mAudioStream = nullptr;
            return false;
        }

        IMLOGD0("[Start] configure codec");
        media_status_t codecResult = AMediaCodec_configure(mCodec, mFormat, nullptr, nullptr, 0);

        if (codecResult != AMEDIA_OK)
        {
            IMLOGE2("[Start] unable to configure[%s] codec - err[%d]", kMimeType, codecResult);
            AMediaCodec_delete(mCodec);
            mCodec = nullptr;
            AMediaFormat_delete(mFormat);
            mFormat = nullptr;
            AAudioStream_close(mAudioStream);
            mAudioStream = nullptr;
            return false;
        }
    }

    aaudio_stream_state_t inputState = AAUDIO_STREAM_STATE_STARTING;
    aaudio_stream_state_t nextState = AAUDIO_STREAM_STATE_UNINITIALIZED;
    auto result = AAudioStream_requestStart(mAudioStream);

    if (result != AAUDIO_OK)
    {
        IMLOGE1("[Start] Error start stream[%s]", AAudio_convertResultToText(result));
        if (mCodecType == kAudioCodecAmr || mCodecType == kAudioCodecAmrWb)
        {
            AMediaCodec_delete(mCodec);
            mCodec = nullptr;
            AMediaFormat_delete(mFormat);
            mFormat = nullptr;
        }
        AAudioStream_close(mAudioStream);
        mAudioStream = nullptr;
        return false;
    }

    result = AAudioStream_waitForStateChange(
            mAudioStream, inputState, &nextState, AAUDIO_START_TIMEOUT_NANO);

    if (result != AAUDIO_OK || nextState != AAUDIO_STREAM_STATE_STARTED)
    {
        IMLOGE2("[Start] Error start stream[%s], state[%s]",
                AAudio_convertResultToText(result),
                AAudio_convertStreamStateToText(nextState));
        if (mCodecType == kAudioCodecAmr || mCodecType == kAudioCodecAmrWb)
        {
            AMediaCodec_delete(mCodec);
            mCodec = nullptr;
            AMediaFormat_delete(mFormat);
            mFormat = nullptr;
        }
        AAudioStream_requestStop(mAudioStream);
        AAudioStream_close(mAudioStream);
        mAudioStream = nullptr;
        return false;
    }

    IMLOGI1("[Start] start stream state[%s]", AAudio_convertStreamStateToText(nextState));

    if (mCodecType == kAudioCodecAmr || mCodecType == kAudioCodecAmrWb)
    {
        media_status_t codecResult = AMediaCodec_start(mCodec);
        if (codecResult != AMEDIA_OK)
        {
            IMLOGE1("[Start] unable to start codec - err[%d]", codecResult);
            AMediaCodec_delete(mCodec);
            mCodec = nullptr;
            AMediaFormat_delete(mFormat);
            mFormat = nullptr;
            AAudioStream_requestStop(mAudioStream);
            AAudioStream_close(mAudioStream);
            mAudioStream = nullptr;
            return false;
        }
    }

    mPlaybackActive = true;
    IMLOGD0("[Start] exit");
    return true;
}

void ImsMediaAudioPlayer::Stop()
{
    IMLOGD0("[Stop] enter");
    ImsMediaMutex::Autolock lock(mMutex);
    mPlaybackActive = false;
    mNextRecoveryTimeUs = 0;
    if ((mCodecType == kAudioCodecAmr || mCodecType == kAudioCodecAmrWb) && (mCodec != nullptr))
    {
        AMediaCodec_stop(mCodec);
        AMediaCodec_delete(mCodec);
        mCodec = nullptr;
    }

    if ((mCodecType == kAudioCodecAmr || mCodecType == kAudioCodecAmrWb) && (mFormat != nullptr))
    {
        AMediaFormat_delete(mFormat);
        mFormat = nullptr;
    }

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

    mDisconnectedAudioStream.store(nullptr);

    IMLOGD0("[Stop] exit ");
}

bool ImsMediaAudioPlayer::onDataFrame(uint8_t* buffer, uint32_t size, FrameType frameType,
        bool /*hasNextFrame*/, uint8_t /*nextFrameByte*/)
{
    ImsMediaMutex::Autolock lock(mMutex);

    if (!mPlaybackActive)
    {
        return false;
    }

    AAudioStream* disconnectedStream = mDisconnectedAudioStream.exchange(nullptr);
    if (disconnectedStream != nullptr)
    {
        restartAudioStream(disconnectedStream);
    }
    else if (mAudioStream == nullptr &&
            ImsMediaTimer::GetTimeInMicroSeconds() >= mNextRecoveryTimeUs)
    {
        restartAudioStream(nullptr);
    }

    if (mAudioStream == nullptr ||
            AAudioStream_getState(mAudioStream) != AAUDIO_STREAM_STATE_STARTED)
    {
        return false;
    }

    if (frameType == LOST && (size == 0 || buffer == nullptr) &&
            (mCodecType == kAudioCodecAmr || mCodecType == kAudioCodecAmrWb))
    {
        if (mCodec == nullptr)
        {
            return false;
        }

        uint8_t lossFrameHeader = mCodecType == kAudioCodecAmr
                ? kAmrNoDataFrameHeader
                : kAmrWbSpeechLostFrameHeader;
        IMLOGD_PACKET2(IM_PACKET_LOG_AUDIO,
                "[onDataFrame] conceal lost frame codec[%d], header[%02X]", mCodecType,
                lossFrameHeader);
        bool audioProduced = decodeAmr(&lossFrameHeader, sizeof(lossFrameHeader));
        return audioProduced || writeSilenceFrame();
    }

    if (size == 0 || buffer == nullptr)
    {
        return frameType == SPEECH ? false : writeSilenceFrame();
    }

    if (mCodecType == kAudioCodecAmr || mCodecType == kAudioCodecAmrWb)
    {
        if (mCodec == nullptr)
        {
            return false;
        }
        else
        {
            bool audioProduced = decodeAmr(buffer, size);
            return audioProduced || (frameType != SPEECH && writeSilenceFrame());
        }
    }
    else if (mCodecType == kAudioCodecEvs)
    {
        // TODO:Integration with libEVS is required.
        return decodeEvs(buffer, size);
    }
    else if (mCodecType == kAudioCodecL16)
    {
        if (size > sizeof(mBuffer) || size % sizeof(mBuffer[0]) != 0)
        {
            IMLOGE2("[onDataFrame] invalid L16 size[%u], capacity[%zu]", size,
                    sizeof(mBuffer));
            return false;
        }
        const uint32_t numFrames = size / sizeof(mBuffer[0]);
        for (uint32_t i = 0; i < numFrames; ++i)
        {
            mBuffer[i] = static_cast<uint16_t>(buffer[i * 2]) << 8 | buffer[i * 2 + 1];
        }
        return writeAudioFrames(mBuffer, static_cast<int32_t>(numFrames));
    }
    return false;
}

bool ImsMediaAudioPlayer::writeAudioFrames(const uint16_t* buffer, int32_t numFrames)
{
    if (buffer == nullptr || numFrames <= 0 || numFrames > PCM_BUFFER_SIZE ||
            mAudioStream == nullptr)
    {
        return false;
    }

    int32_t totalFramesWritten = 0;
    uint64_t deadlineUs = ImsMediaTimer::GetTimeInMicroSeconds() +
            AUDIO_FRAME_DURATION_NANO / 1000;

    while (totalFramesWritten < numFrames)
    {
        uint64_t nowUs = ImsMediaTimer::GetTimeInMicroSeconds();
        if (nowUs >= deadlineUs)
        {
            IMLOGW2("[writeAudioFrames] timeout after frames[%d/%d]", totalFramesWritten,
                    numFrames);
            return false;
        }

        aaudio_result_t result = AAudioStream_write(mAudioStream, buffer + totalFramesWritten,
                numFrames - totalFramesWritten,
                static_cast<int64_t>((deadlineUs - nowUs) * 1000));
        if (result < 0)
        {
            IMLOGE1("[writeAudioFrames] write failed[%s]", AAudio_convertResultToText(result));
            return false;
        }
        if (result == 0)
        {
            IMLOGW2("[writeAudioFrames] short write frames[%d/%d]", totalFramesWritten,
                    numFrames);
            return false;
        }

        totalFramesWritten += result;
    }

    return true;
}

bool ImsMediaAudioPlayer::writeSilenceFrame()
{
    if (mSamplingRate <= 0)
    {
        return false;
    }

    int32_t numFrames = mSamplingRate / NUM_FRAMES_PER_SEC;
    if (numFrames <= 0 || numFrames > PCM_BUFFER_SIZE)
    {
        IMLOGE2("[writeSilenceFrame] invalid rate[%d], frames[%d]", mSamplingRate, numFrames);
        return false;
    }

    memset(mBuffer, 0, numFrames * sizeof(mBuffer[0]));
    return writeAudioFrames(mBuffer, numFrames);
}

bool ImsMediaAudioPlayer::decodeAmr(uint8_t* buffer, uint32_t size)
{
    bool audioProduced = false;
    auto index = AMediaCodec_dequeueInputBuffer(mCodec, CODEC_TIMEOUT_NANO);

    if (index >= 0)
    {
        size_t bufferSize = 0;
        uint8_t* inputBuffer = AMediaCodec_getInputBuffer(mCodec, index, &bufferSize);
        if (inputBuffer == nullptr || size > bufferSize)
        {
            IMLOGE2("[decodeAmr] invalid input size[%u], capacity[%zu]", size, bufferSize);
            auto err = AMediaCodec_queueInputBuffer(
                    mCodec, index, 0, 0, ImsMediaTimer::GetTimeInMicroSeconds(), 0);
            if (err != AMEDIA_OK)
            {
                IMLOGE1("[decodeAmr] Unable to return input buffer - err[%d]", err);
            }
        }
        else
        {
            memcpy(inputBuffer, buffer, size);
            IMLOGD_PACKET2(IM_PACKET_LOG_AUDIO,
                    "[decodeAmr] queue input buffer index[%d], size[%d]", index, size);

            auto err = AMediaCodec_queueInputBuffer(
                    mCodec, index, 0, size, ImsMediaTimer::GetTimeInMicroSeconds(), 0);
            if (err != AMEDIA_OK)
            {
                IMLOGE1("[decodeAmr] Unable to queue input buffers - err[%d]", err);
            }
        }
    }
    else
    {
        IMLOGE1("[decodeAmr] Unable to get input buffers - err[%d]", index);
    }

    AMediaCodecBufferInfo info;
    index = AMediaCodec_dequeueOutputBuffer(mCodec, &info, CODEC_TIMEOUT_NANO);

    if (index >= 0)
    {
        IMLOGD_PACKET5(IM_PACKET_LOG_AUDIO,
                "[decodeAmr] index[%d], size[%d], offset[%d], time[%ld], flags[%d]", index,
                info.size, info.offset, info.presentationTimeUs, info.flags);

        if (info.size > 0)
        {
            size_t buffCapacity = 0;
            uint8_t* buf = AMediaCodec_getOutputBuffer(mCodec, index, &buffCapacity);
            if (buf != nullptr && info.offset >= 0 &&
                    static_cast<size_t>(info.offset) <= buffCapacity &&
                    static_cast<size_t>(info.size) <=
                            buffCapacity - static_cast<size_t>(info.offset) &&
                    static_cast<size_t>(info.size) <= sizeof(mBuffer) &&
                    info.size % sizeof(mBuffer[0]) == 0)
            {
                memcpy(mBuffer, buf + info.offset, info.size);
                audioProduced = writeAudioFrames(
                        mBuffer, static_cast<int32_t>(info.size / sizeof(mBuffer[0])));
            }
            else
            {
                IMLOGE3("[decodeAmr] invalid output offset[%d], size[%d], capacity[%zu]",
                        info.offset, info.size, buffCapacity);
            }
        }

        AMediaCodec_releaseOutputBuffer(mCodec, index, false);
    }
    else if (index == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)
    {
        IMLOGD0("[decodeAmr] output buffer changed");
    }
    else if (index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
    {
        if (mFormat != nullptr)
        {
            AMediaFormat_delete(mFormat);
        }

        mFormat = AMediaCodec_getOutputFormat(mCodec);
        IMLOGD1("[decodeAmr] format changed, format[%s]", AMediaFormat_toString(mFormat));
    }
    else if (index == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
    {
        IMLOGD0("[decodeAmr] no output buffer");
    }
    else
    {
        IMLOGD1("[decodeAmr] unexpected index[%d]", index);
    }

    return audioProduced;
}

// TODO: Integration with libEVS is required.
bool ImsMediaAudioPlayer::decodeEvs(uint8_t* buffer, uint32_t size)
{
    (void)buffer;
    (void)size;
    IMLOGE0("[decodeEvs] EVS decoding is not implemented");
    return false;
}

void ImsMediaAudioPlayer::openAudioStream()
{
    const aaudio_sharing_mode_t sharingModes[] = {
            AAUDIO_SHARING_MODE_EXCLUSIVE, AAUDIO_SHARING_MODE_SHARED};
    const bool lowLatencyPlayback =
            property_get_bool(kLowLatencyPlaybackProperty, false);
    const aaudio_performance_mode_t performanceMode = lowLatencyPlayback
            ? AAUDIO_PERFORMANCE_MODE_LOW_LATENCY
            : AAUDIO_PERFORMANCE_MODE_NONE;
    aaudio_result_t result = AAUDIO_ERROR_UNAVAILABLE;

    IMLOGI2("[openAudioStream] lowLatencyPlayback[%d], requestedPerformanceMode[%d]",
            lowLatencyPlayback, performanceMode);

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

        AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
        AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
        AAudioStreamBuilder_setChannelCount(builder, 1);
        AAudioStreamBuilder_setSampleRate(builder, mSamplingRate);
        AAudioStreamBuilder_setSharingMode(builder, sharingMode);
        AAudioStreamBuilder_setPerformanceMode(builder, performanceMode);
        AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_VOICE_COMMUNICATION);
        AAudioStreamBuilder_setContentType(builder, AAUDIO_CONTENT_TYPE_SPEECH);
        AAudioStreamBuilder_setErrorCallback(builder, audioErrorCallback, this);

        result = AAudioStreamBuilder_openStream(builder, &mAudioStream);
        AAudioStreamBuilder_delete(builder);

        if (result == AAUDIO_OK && mAudioStream != nullptr)
        {
            IMLOGI3("[openAudioStream] sharingMode[%d], requestedPerformanceMode[%d], "
                    "actualPerformanceMode[%d]",
                    sharingMode, performanceMode,
                    AAudioStream_getPerformanceMode(mAudioStream));
            return;
        }

        IMLOGW2("[openAudioStream] Failed sharingMode[%d], error[%s]",
                sharingMode, AAudio_convertResultToText(result));
        if (mAudioStream != nullptr)
        {
            AAudioStream_close(mAudioStream);
            mAudioStream = nullptr;
        }
    }

    IMLOGE1("[openAudioStream] Failed to openStream. Error[%s]",
            AAudio_convertResultToText(result));
}

void ImsMediaAudioPlayer::restartAudioStream(AAudioStream* disconnectedStream)
{
    if (mAudioStream != disconnectedStream)
    {
        IMLOGI0("[restartAudioStream] Ignore stale disconnect");
        return;
    }

    // Retry a failed reopen on later frames while this playback session is active.
    mNextRecoveryTimeUs = ImsMediaTimer::GetTimeInMicroSeconds() +
            kPlaybackRecoveryIntervalUs;
    if (mAudioStream != nullptr)
    {
        AAudioStream_requestStop(mAudioStream);
        AAudioStream_close(mAudioStream);
        mAudioStream = nullptr;
    }
    openAudioStream();

    if (mAudioStream == nullptr)
    {
        return;
    }

    aaudio_stream_state_t inputState = AAUDIO_STREAM_STATE_STARTING;
    aaudio_stream_state_t nextState = AAUDIO_STREAM_STATE_UNINITIALIZED;
    aaudio_result_t result = AAudioStream_requestStart(mAudioStream);
    if (result != AAUDIO_OK)
    {
        IMLOGE1("[restartAudioStream] Error start stream[%s]", AAudio_convertResultToText(result));
        AAudioStream_close(mAudioStream);
        mAudioStream = nullptr;
        return;
    }

    result = AAudioStream_waitForStateChange(
            mAudioStream, inputState, &nextState, AAUDIO_START_TIMEOUT_NANO);

    if (result != AAUDIO_OK || nextState != AAUDIO_STREAM_STATE_STARTED)
    {
        IMLOGE2("[restartAudioStream] Error start stream[%s], state[%s]",
                AAudio_convertResultToText(result),
                AAudio_convertStreamStateToText(nextState));
        AAudioStream_requestStop(mAudioStream);
        AAudioStream_close(mAudioStream);
        mAudioStream = nullptr;
        return;
    }

    mNextRecoveryTimeUs = 0;
    IMLOGI1("[restartAudioStream] start stream state[%s]",
            AAudio_convertStreamStateToText(nextState));
}

void ImsMediaAudioPlayer::audioErrorCallback(
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
        reinterpret_cast<ImsMediaAudioPlayer*>(userData)
                ->mDisconnectedAudioStream.store(stream);
    }
}

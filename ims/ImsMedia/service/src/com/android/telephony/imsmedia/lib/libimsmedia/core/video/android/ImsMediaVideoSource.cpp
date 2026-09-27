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

#include <ImsMediaVideoSource.h>
#include <ImsMediaTrace.h>
#include <ImsMediaTimer.h>
#include <ImsMediaImageRotate.h>

#include <algorithm>
#include <cstring>
#include <list>
#include <limits>
#include <pthread.h>
#include <time.h>

#define CODEC_TIMEOUT_NANO 100000

void* ImsMediaVideoSource::RunPauseImageThread(void* context)
{
    if (context != nullptr)
    {
        static_cast<ImsMediaVideoSource*>(context)->EncodePauseImage();
    }
    return nullptr;
}

ImsMediaVideoSource::ImsMediaVideoSource()
{
    mCamera = nullptr;
    mWindow = nullptr;
    mCodec = nullptr;
    mFormat = nullptr;
    mImageReaderSurface = nullptr;
    mImageReader = nullptr;
    mListener = nullptr;
    mCodecType = -1;
    mVideoMode = -1;
    mCodecProfile = 0;
    mCodecLevel = 0;
    mCameraId = 0;
    mCameraZoom = 0;
    mWidth = 0;
    mHeight = 0;
    mCodecStride = 0;
    mFramerate = 0;
    mBitrate = 0;
    mIntraInterval = 1;
    mImagePath = "";
    mDeviceOrientation = -1;
    mTimestamp = 0;
    mPrevTimestamp = 0;
    mStopped = true;
    mCodecStarted = false;
    mPauseImageThreadStarted = false;
}

ImsMediaVideoSource::~ImsMediaVideoSource()
{
    Stop();
}

void ImsMediaVideoSource::SetListener(IVideoSourceCallback* listener)
{
    ImsMediaMutex::Autolock lock(mMutex);
    mListener = listener;
}

void ImsMediaVideoSource::SetVideoMode(const int32_t mode)
{
    IMLOGD1("[SetVideoMode] mode[%d]", mode);
    mVideoMode = mode;
}

void ImsMediaVideoSource::SetCameraConfig(const uint32_t cameraId, const uint32_t cameraZoom)
{
    IMLOGD2("[SetCameraConfig] id[%d], zoom[%d]", cameraId, cameraZoom);
    mCameraId = cameraId;
    mCameraZoom = cameraZoom;
}

void ImsMediaVideoSource::SetImagePath(const std::string& path)
{
    IMLOGD1("[SetImagePath] path[%s]", path.c_str());
    mImagePath = path;
}

void ImsMediaVideoSource::SetCodecConfig(int32_t codecType, const uint32_t profile,
        const uint32_t level, const uint32_t bitrate, const uint32_t framerate,
        const uint32_t interval)
{
    IMLOGD6("[SetCodecConfig] type[%d], profile[%d], level[%d], bitrate[%d], FPS[%d], interval[%d]",
            codecType, profile, level, bitrate, framerate, interval);
    mCodecType = codecType;
    mCodecProfile = profile;
    mCodecLevel = level;
    mBitrate = bitrate;
    mFramerate = framerate;
    mIntraInterval = interval;
}

void ImsMediaVideoSource::SetResolution(const uint32_t width, const uint32_t height)
{
    IMLOGD2("[SetResolution] width[%d], height[%d]", width, height);
    mWidth = width;
    mHeight = height;
}

void ImsMediaVideoSource::SetSurface(ANativeWindow* window)
{
    IMLOGD1("[SetSurface] surface[%p]", window);
    mWindow = window;
}

void ImsMediaVideoSource::SetDeviceOrientation(const uint32_t degree)
{
    IMLOGD1("[SetDeviceOrientation] degree[%d]", degree);

    if (static_cast<uint32_t>(mDeviceOrientation) != degree)
    {
        if (mVideoMode == kVideoModeRecording)
        {
            int32_t facing = kCameraFacingFront;
            int32_t sensorOrientation = 0;
            int32_t rotateDegree = 0;

            if (mCamera != nullptr)
            {
                mCamera->GetSensorOrientation(mCameraId, &facing, &sensorOrientation);
                IMLOGD2("[SetDeviceOrientation] camera facing[%d], sensorOrientation[%d]", facing,
                        sensorOrientation);
            }

            // assume device is always portrait
            if (mWidth > mHeight)
            {
                if (facing == kCameraFacingFront)
                {
                    sensorOrientation = (sensorOrientation + 180) % 360;
                }

                rotateDegree = sensorOrientation - degree;

                if (rotateDegree < 0)
                {
                    rotateDegree += 360;
                }
            }
            else
            {
                if (degree == 90 || degree == 270)
                {
                    rotateDegree = (degree + 180) % 360;
                }
                else
                {
                    rotateDegree = degree;
                }
            }

            if (mListener != nullptr)
            {
                mListener->OnEvent(kVideoSourceEventUpdateOrientation, facing, rotateDegree);
            }
        }

        mDeviceOrientation = degree;
    }
}

bool ImsMediaVideoSource::Start()
{
    IMLOGD1("[Start], VideoMode[%d]", mVideoMode);

    {
        ImsMediaMutex::Autolock lock(mMutex);
        if (!mStopped || mPauseImageThreadStarted || mCodec != nullptr || mFormat != nullptr ||
                mCamera != nullptr || mImageReader != nullptr)
        {
            IMLOGE0("[Start] video source is already running");
            return false;
        }

        if (mVideoMode != kVideoModePreview && mVideoMode != kVideoModeRecording &&
                mVideoMode != kVideoModePauseImage)
        {
            IMLOGE1("[Start] invalid video mode[%d]", mVideoMode);
            return false;
        }
        mStopped = false;
    }

    if (mVideoMode == kVideoModeRecording || mVideoMode == kVideoModePauseImage)
    {
        const uint64_t maxInputSize = static_cast<uint64_t>(mWidth) * mHeight * 10;
        if ((mCodecType != kVideoCodecAvc && mCodecType != kVideoCodecHevc) || mWidth == 0 ||
                mHeight == 0 || mFramerate == 0 || mBitrate == 0 ||
                maxInputSize > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()) ||
                mBitrate > static_cast<uint32_t>(std::numeric_limits<int32_t>::max() / 1000) ||
                mCodecProfile > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) ||
                mCodecLevel > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) ||
                mIntraInterval > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()))
        {
            IMLOGE4("[Start] invalid encoder config[%ux%u, %u fps, %u kbps]", mWidth, mHeight,
                    mFramerate, mBitrate);
            Stop();
            return false;
        }

        mFormat = AMediaFormat_new();
        if (mFormat == nullptr)
        {
            IMLOGE0("[Start] Unable to create media format");
            Stop();
            return false;
        }

        AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_WIDTH, mWidth);
        AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_HEIGHT, mHeight);

        const char* kMimeType = mCodecType == kVideoCodecHevc ? "video/hevc" : "video/avc";

        AMediaFormat_setString(mFormat, AMEDIAFORMAT_KEY_MIME, kMimeType);

        AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_COLOR_FORMAT,
                0x00000015);  // COLOR_FormatYUV420SemiPlanar
        AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_BIT_RATE, mBitrate * 1000);
        AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_PROFILE, mCodecProfile);
        AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_LEVEL, mCodecLevel);
        AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_BITRATE_MODE,
                2);  // #2 : BITRATE_MODE_CBR
        AMediaFormat_setFloat(mFormat, AMEDIAFORMAT_KEY_FRAME_RATE, mFramerate);
        AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, mIntraInterval);
        AMediaFormat_setInt32(
                mFormat, AMEDIAFORMAT_KEY_MAX_INPUT_SIZE, static_cast<int32_t>(maxInputSize));

        mCodec = AMediaCodec_createEncoderByType(kMimeType);

        if (mCodec == nullptr)
        {
            IMLOGE0("[Start] Unable to create encoder");
            Stop();
            return false;
        }

        media_status_t err = AMediaCodec_configure(
                mCodec, mFormat, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);

        if (err != AMEDIA_OK)
        {
            IMLOGE1("[Start] configure error[%d]", err);
            Stop();
            return false;
        }

        mImageReaderSurface = CreateImageReader(mWidth, mHeight);

        if (mImageReaderSurface == nullptr)
        {
            IMLOGE0("[Start] create image reader failed");
            Stop();
            return false;
        }

        mCodecStride = mWidth;
        AMediaFormat* encoderInputFormat = AMediaCodec_getInputFormat(mCodec);

        if (encoderInputFormat != nullptr)
        {
            // Check if encoder is initialized with the expected configurations.
            int32_t width = 0, height = 0;
            AMediaFormat_getInt32(encoderInputFormat, AMEDIAFORMAT_KEY_WIDTH, &width);
            AMediaFormat_getInt32(encoderInputFormat, AMEDIAFORMAT_KEY_HEIGHT, &height);
            AMediaFormat_getInt32(encoderInputFormat, AMEDIAFORMAT_KEY_STRIDE, &mCodecStride);
            AMediaFormat_delete(encoderInputFormat);

            // TODO: More configuration checks should be added
            if (mWidth != static_cast<uint32_t>(width) ||
                    mHeight != static_cast<uint32_t>(height) || width > mCodecStride ||
                    mCodecStride <= 0 ||
                    mCodecStride > std::numeric_limits<uint16_t>::max())
            {
                IMLOGE0("Encoder doesn't support requested configuration.");
                Stop();
                return false;
            }
        }

        err = AMediaCodec_start(mCodec);

        if (err != AMEDIA_OK)
        {
            IMLOGE1("[Start] codec start[%d]", err);
            Stop();
            return false;
        }
        mCodecStarted = true;
    }

    if (mCameraId != std::numeric_limits<uint32_t>::max() &&
            (mVideoMode == kVideoModePreview || mVideoMode == kVideoModeRecording))
    {
        mCamera = ImsMediaCamera::getInstance();
        if (mCamera == nullptr)
        {
            IMLOGE0("[Start] camera service is unavailable");
            Stop();
            return false;
        }
        mCamera->Initialize();
        mCamera->SetCameraConfig(mCameraId, mCameraZoom, mFramerate);

        if (!mCamera->OpenCamera())
        {
            IMLOGE0("[Start] error open camera");
            mCamera->DeInitialize();
            mCamera = nullptr;
            Stop();
            return false;
        }

        if (mCamera->CreateSession(mWindow, mImageReaderSurface) == false)
        {
            IMLOGE0("[Start] error create camera session");
            mCamera->DeleteSession();
            mCamera->DeInitialize();
            mCamera = nullptr;
            Stop();
            return false;
        }

        if (mCamera->StartSession(mVideoMode == kVideoModeRecording) == false)
        {
            IMLOGE0("[Start] error camera start");
            mCamera->StopSession();
            mCamera->DeleteSession();
            mCamera->DeInitialize();
            mCamera = nullptr;
            Stop();
            return false;
        }
    }
    else if (mVideoMode == kVideoModePauseImage)
    {
        if (!mPauseImageSource.Initialize(mWidth, mHeight, mCodecStride))
        {
            IMLOGE0("[Start] pause image initialization failed");
            Stop();
            return false;
        }

        // start encoder output thread
        if (mCodec != nullptr)
        {
            const int threadError =
                    pthread_create(&mPauseImageThread, nullptr, RunPauseImageThread, this);
            if (threadError != 0)
            {
                IMLOGE1("[Start] unable to create pause-image thread: %s",
                        std::strerror(threadError));
                Stop();
                return false;
            }
            mPauseImageThreadStarted = true;
        }
    }

    mDeviceOrientation = -1;
    IMLOGD0("[Start] exit");
    return true;
}

void ImsMediaVideoSource::Stop()
{
    IMLOGD0("[Stop]");

    {
        ImsMediaMutex::Autolock lock(mMutex);
        mStopped = true;
    }

    if (mPauseImageThreadStarted)
    {
        const int threadError = pthread_join(mPauseImageThread, nullptr);
        if (threadError != 0)
        {
            IMLOGE1("[Stop] unable to join pause-image thread: %s", std::strerror(threadError));
            return;
        }
        mPauseImageThreadStarted = false;
    }

    ImsMediaMutex::Autolock lock(mMutex);

    if (mCamera != nullptr)
    {
        mCamera->StopSession();
        mCamera->DeleteSession();
        mCamera->DeInitialize();
        mCamera = nullptr;
    }

    if (mImageReader != nullptr)
    {
        AImageReader_delete(mImageReader);
        mImageReader = nullptr;
        mImageReaderSurface = nullptr;
    }

    if (mCodec != nullptr)
    {
        if (mCodecStarted)
        {
            AMediaCodec_stop(mCodec);
        }
        AMediaCodec_delete(mCodec);
        mCodec = nullptr;
        mCodecStarted = false;
    }

    if (mFormat != nullptr)
    {
        AMediaFormat_delete(mFormat);
        mFormat = nullptr;
    }

    if (mVideoMode == kVideoModePauseImage)
    {
        mPauseImageSource.Uninitialize();
    }
}

bool ImsMediaVideoSource::IsStopped()
{
    ImsMediaMutex::Autolock lock(mMutex);
    return mStopped;
}

void ImsMediaVideoSource::onCameraFrame(AImage* pImage)
{
    ImsMediaMutex::Autolock lock(mMutex);

    if (mStopped || mImageReader == nullptr || mCodec == nullptr || pImage == nullptr)
    {
        return;
    }

    auto index = AMediaCodec_dequeueInputBuffer(mCodec, CODEC_TIMEOUT_NANO);

    if (index >= 0)
    {
        size_t buffCapacity = 0;
        uint8_t* encoderBuf = AMediaCodec_getInputBuffer(mCodec, index, &buffCapacity);
        if (!encoderBuf || !buffCapacity)
        {
            IMLOGE1("[onCameraFrame] returned null buffer pointer or buffCapacity[%zu]",
                    buffCapacity);
            AMediaCodec_queueInputBuffer(mCodec, index, 0, 0, 0, 0);
            return;
        }

        int32_t width = 0, height = 0, ylen = 0, uvlen = 0, result = 0;
        uint8_t *yPlane = nullptr, *uvPlane = nullptr;
        if (AImage_getWidth(pImage, &width) != AMEDIA_OK ||
                AImage_getHeight(pImage, &height) != AMEDIA_OK ||
                AImage_getPlaneData(pImage, 0, &yPlane, &ylen) != AMEDIA_OK ||
                AImage_getPlaneData(pImage, 1, &uvPlane, &uvlen) != AMEDIA_OK ||
                yPlane == nullptr || uvPlane == nullptr || ylen < 0 || uvlen < 0)
        {
            IMLOGE0("[onCameraFrame] invalid image planes");
            AMediaCodec_queueInputBuffer(mCodec, index, 0, 0, 0, 0);
            return;
        }

        const size_t imageSize = static_cast<size_t>(ylen) + static_cast<size_t>(uvlen);
        const uint64_t rawYSize = static_cast<uint64_t>(width) * height;
        if (width <= 0 || height <= 0 || rawYSize > static_cast<uint32_t>(ylen) ||
                rawYSize / 2 > static_cast<uint32_t>(uvlen) ||
                width > std::numeric_limits<uint16_t>::max() ||
                height > std::numeric_limits<uint16_t>::max())
        {
            IMLOGE4("[onCameraFrame] invalid image size[%dx%d, Y:%d, UV:%d]", width, height,
                    ylen, uvlen);
            AMediaCodec_queueInputBuffer(mCodec, index, 0, 0, 0, 0);
            return;
        }

        size_t queuedSize = 0;
        if (mWidth > mHeight)  // landscape mode, copy without rotate
        {
            if (imageSize > buffCapacity)
            {
                result = -1;
            }
            else
            {
                memcpy(encoderBuf, yPlane, static_cast<size_t>(ylen));
                memcpy(encoderBuf + ylen, uvPlane, static_cast<size_t>(uvlen));
                queuedSize = imageSize;
            }
        }
        else
        {
            int32_t facing, sensorOrientation;
            mCamera->GetSensorOrientation(mCameraId, &facing, &sensorOrientation);

            switch (facing)
            {
                case ACAMERA_LENS_FACING_FRONT:
                {
                    result = ImsMediaImageRotate::YUV420_SP_Rotate270(
                            encoderBuf, buffCapacity, mCodecStride, yPlane, uvPlane, width, height);
                    queuedSize = static_cast<size_t>(mCodecStride) * width * 3 / 2;
                }
                break;

                case ACAMERA_LENS_FACING_BACK:
                {
                    result = ImsMediaImageRotate::YUV420_SP_Rotate90(
                            encoderBuf, buffCapacity, mCodecStride, yPlane, uvPlane, width, height);
                    queuedSize = static_cast<size_t>(mCodecStride) * width * 3 / 2;
                }
                break;

                case ACAMERA_LENS_FACING_EXTERNAL:
                {
                    if (rawYSize + rawYSize / 2 > buffCapacity)
                    {
                        result = -1;
                    }
                    else
                    {
                        memcpy(encoderBuf, yPlane, static_cast<size_t>(rawYSize));
                        memcpy(encoderBuf + rawYSize, uvPlane,
                                static_cast<size_t>(rawYSize / 2));
                        queuedSize = static_cast<size_t>(rawYSize + rawYSize / 2);
                    }
                }
                break;

                default:
                    result = -1;
                    break;
            }
        }

        IMLOGD_PACKET1(IM_PACKET_LOG_VIDEO, "[onCameraFrame] queue buffer size[%zu]", imageSize);

        if (result == 0 && queuedSize > 0 && queuedSize <= buffCapacity)
        {
            AMediaCodec_queueInputBuffer(
                    mCodec, index, 0, queuedSize, ImsMediaTimer::GetTimeInMicroSeconds(), 0);
        }
        else
        {
            IMLOGE5("Camera image resolution[%dx%d]. Encoder resolution[%dx%d] buffer size[%zu]",
                    width, height, mWidth, mHeight, buffCapacity);
            AMediaCodec_queueInputBuffer(mCodec, index, 0, 0, 0, 0);
            return;
        }
    }
    else
    {
        IMLOGE1("[onCameraFrame] dequeueInputBuffer returned index[%d]", index);
    }

    processOutputBuffer();
}

bool ImsMediaVideoSource::changeBitrate(const uint32_t bitrate)
{
    IMLOGD1("[changeBitrate] bitrate[%d]", bitrate);
    ImsMediaMutex::Autolock lock(mMutex);

    if (mStopped || mCodec == nullptr || bitrate > std::numeric_limits<int32_t>::max())
    {
        return false;
    }

    AMediaFormat* params = AMediaFormat_new();
    if (params == nullptr)
    {
        return false;
    }
    AMediaFormat_setInt32(
            params, AMEDIACODEC_KEY_VIDEO_BITRATE, static_cast<int32_t>(bitrate));
    media_status_t status = AMediaCodec_setParameters(mCodec, params);
    AMediaFormat_delete(params);

    if (status != AMEDIA_OK)
    {
        IMLOGE1("[changeBitrate] error[%d]", status);
        return false;
    }

    return true;
}

void ImsMediaVideoSource::requestIdrFrame()
{
    IMLOGD0("[requestIdrFrame]");
    ImsMediaMutex::Autolock lock(mMutex);

    if (mStopped || mCodec == nullptr)
    {
        return;
    }

    AMediaFormat* params = AMediaFormat_new();
    if (params == nullptr)
    {
        return;
    }
    AMediaFormat_setInt32(params, AMEDIACODEC_KEY_REQUEST_SYNC_FRAME, 0);
    media_status_t status = AMediaCodec_setParameters(mCodec, params);
    AMediaFormat_delete(params);

    if (status != AMEDIA_OK)
    {
        IMLOGE1("[requestIdrFrame] error[%d]", status);
    }
}

void ImsMediaVideoSource::EncodePauseImage()
{
    IMLOGD0("[EncodePauseImage] start");

    uint32_t nextTime = ImsMediaTimer::GetTimeInMilliSeconds();
    uint32_t timeInterval = 66;

    if (mFramerate != 0)
    {
        timeInterval = std::max(1U, 1000 / mFramerate);
    }

    while (!IsStopped())
    {
        {
            ImsMediaMutex::Autolock lock(mMutex);
            if (mStopped || mCodec == nullptr)
            {
                break;
            }

            auto index = AMediaCodec_dequeueInputBuffer(mCodec, CODEC_TIMEOUT_NANO);

            if (index >= 0)
            {
                size_t buffCapacity = 0;
                uint8_t* encoderBuf = AMediaCodec_getInputBuffer(mCodec, index, &buffCapacity);
                if (!encoderBuf || !buffCapacity)
                {
                    IMLOGE1("[EncodePauseImage] returned null buffer pointer or buffCapacity[%zu]",
                            buffCapacity);
                    AMediaCodec_queueInputBuffer(mCodec, index, 0, 0, 0, 0);
                }
                else
                {
                    size_t len = mPauseImageSource.GetYuvImage(encoderBuf, buffCapacity);
                    AMediaCodec_queueInputBuffer(mCodec, index, 0, len,
                            ImsMediaTimer::GetTimeInMicroSeconds(), 0);
                }
            }
            else
            {
                IMLOGE1("[EncodePauseImage] dequeueInputBuffer returned index[%d]", index);
            }

            processOutputBuffer();
        }

        if (IsStopped())
        {
            break;
        }

        nextTime += timeInterval;
        uint32_t nCurrTime = ImsMediaTimer::GetTimeInMilliSeconds();

        if (nextTime > nCurrTime)
        {
            uint32_t timeDiff = nextTime - nCurrTime;
            IMLOGD_PACKET1(IM_PACKET_LOG_VIDEO, "[EncodePauseImage] timeDiff[%u]", timeDiff);
            ImsMediaTimer::Sleep(timeDiff);
        }
    }

    IMLOGD0("[EncodePauseImage] end");
}

void ImsMediaVideoSource::processOutputBuffer()
{
    AMediaCodecBufferInfo info{};
    auto index = AMediaCodec_dequeueOutputBuffer(mCodec, &info, CODEC_TIMEOUT_NANO);

    if (index >= 0)
    {
        IMLOGD_PACKET5(IM_PACKET_LOG_VIDEO,
                "[processOutputBuffer] index[%d], size[%d], offset[%d], time[%ld], flags[%d]",
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
                if (mListener != nullptr)
                {
                    mListener->OnUplinkEvent(
                            buf + info.offset, info.size, info.presentationTimeUs, info.flags);
                }
            }
            else
            {
                IMLOGE3("[processOutputBuffer] invalid offset[%d], size[%d], capacity[%zu]",
                        info.offset, info.size, buffCapacity);
            }
        }

        AMediaCodec_releaseOutputBuffer(mCodec, index, false);
    }
    else if (index == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)
    {
        IMLOGI0("[processOutputBuffer] Encoder output buffer changed");
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
            IMLOGI1("[processOutputBuffer] Encoder format changed, format[%s]",
                    AMediaFormat_toString(mFormat));
        }
        else
        {
            IMLOGE0("[processOutputBuffer] unable to read encoder output format");
        }
    }
    else if (index == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
    {
        IMLOGD_PACKET0(IM_PACKET_LOG_VIDEO, "[processOutputBuffer] no output buffer");
    }
    else
    {
        IMLOGI1("[processOutputBuffer] unexpected index[%d]", index);
    }
}

static void ImageCallback(void* context, AImageReader* reader)
{
    if (context == nullptr)
    {
        return;
    }

    ImsMediaVideoSource* pVideoSource = static_cast<ImsMediaVideoSource*>(context);

    AImage* image = nullptr;
    auto status = AImageReader_acquireNextImage(reader, &image);

    if (status != AMEDIA_OK)
    {
        return;
    }

    pVideoSource->onCameraFrame(image);
    AImage_delete(image);
}

ANativeWindow* ImsMediaVideoSource::CreateImageReader(int width, int height)
{
    media_status_t status =
            AImageReader_new(width, height, AIMAGE_FORMAT_YUV_420_888, 2, &mImageReader);

    if (status != AMEDIA_OK)
    {
        return nullptr;
    }

    AImageReader_ImageListener listener{
            .context = this,
            .onImageAvailable = ImageCallback,
    };

    status = AImageReader_setImageListener(mImageReader, &listener);
    if (status != AMEDIA_OK)
    {
        AImageReader_delete(mImageReader);
        mImageReader = nullptr;
        return nullptr;
    }

    ANativeWindow* nativeWindow = nullptr;
    status = AImageReader_getWindow(mImageReader, &nativeWindow);
    if (status != AMEDIA_OK || nativeWindow == nullptr)
    {
        AImageReader_delete(mImageReader);
        mImageReader = nullptr;
        return nullptr;
    }
    return nativeWindow;
}

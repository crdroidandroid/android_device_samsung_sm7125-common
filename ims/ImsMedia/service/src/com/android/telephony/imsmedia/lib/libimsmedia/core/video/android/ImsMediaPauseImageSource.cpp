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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <android-base/unique_fd.h>
#include <android/bitmap.h>
#include <android/imagedecoder.h>
#include <ImsMediaTrace.h>
#include "ImsMediaPauseImageSource.h"

// TODO: Pause images from Irvine source are used. Get new pause images from UX team and replace.
// TODO: Write unit test cases for this class
#define DEFAULT_FHD_PORTRAIT_PAUSE_IMG_PATH    "pause_images/pause_img_fhd_p.jpg"
#define DEFAULT_FHD_LANDSCAPE_PAUSE_IMG_PATH   "pause_images/pause_img_fhd_l.jpg"
#define DEFAULT_HD_PORTRAIT_PAUSE_IMG_PATH     "pause_images/pause_img_hd_p.jpg"
#define DEFAULT_HD_LANDSCAPE_PAUSE_IMG_PATH    "pause_images/pause_img_hd_l.jpg"
#define DEFAULT_VGA_PORTRAIT_PAUSE_IMG_PATH    "pause_images/pause_img_vga_p.jpg"
#define DEFAULT_VGA_LANDSCAPE_PAUSE_IMG_PATH   "pause_images/pause_img_vga_l.jpg"
#define DEFAULT_QVGA_PORTRAIT_PAUSE_IMG_PATH   "pause_images/pause_img_qvga_p.jpg"
#define DEFAULT_QVGA_LANDSCAPE_PAUSE_IMG_PATH  "pause_images/pause_img_qvga_l.jpg"
#define DEFAULT_CIF_PORTRAIT_PAUSE_IMG_PATH    "pause_images/pause_img_cif_p.jpg"
#define DEFAULT_CIF_LANDSCAPE_PAUSE_IMG_PATH   "pause_images/pause_img_cif_l.jpg"
#define DEFAULT_QCIF_PORTRAIT_PAUSE_IMG_PATH   "pause_images/pause_img_qcif_p.jpg"
#define DEFAULT_QCIF_LANDSCAPE_PAUSE_IMG_PATH  "pause_images/pause_img_qcif_l.jpg"
#define DEFAULT_SIF_PORTRAIT_PAUSE_IMG_PATH    "pause_images/pause_img_sif_p.jpg"
#define DEFAULT_SIF_LANDSCAPE_PAUSE_IMG_PATH   "pause_images/pause_img_sif_l.jpg"
#define DEFAULT_SQCIF_PORTRAIT_PAUSE_IMG_PATH  "pause_images/pause_img_sqcif_p.jpg"
#define DEFAULT_SQCIF_LANDSCAPE_PAUSE_IMG_PATH "pause_images/pause_img_sqcif_l.jpg"

extern AAssetManager* gpAssetManager;

ImsMediaPauseImageSource::ImsMediaPauseImageSource()
{
    mYuvImageBuffer = nullptr;
    mBufferSize = 0;
}

ImsMediaPauseImageSource::~ImsMediaPauseImageSource()
{
    Uninitialize();
}

void ImsMediaPauseImageSource::Uninitialize()
{
    if (mYuvImageBuffer != nullptr)
    {
        free(mYuvImageBuffer);
        mYuvImageBuffer = nullptr;
    }
    mBufferSize = 0;
}

bool ImsMediaPauseImageSource::Initialize(int width, int height, int stride)
{
    IMLOGD3("[ImsMediaPauseImageSource] Init(width:%d, height:%d, stride:%d)", width, height,
            stride);
    Uninitialize();
    if (width <= 0 || height <= 0 || stride < width || width % 2 != 0 || height % 2 != 0 ||
            stride % 2 != 0)
    {
        IMLOGE3("[ImsMediaPauseImageSource] Invalid dimensions[%dx%d], stride[%d]", width,
                height, stride);
        return false;
    }

    mWidth = width;
    mHeight = height;

    // Decode JPEG image and save in YUV buffer.
    AAsset* asset = getImageAsset();
    if (asset == nullptr)
    {
        IMLOGE0("[ImsMediaPauseImageSource] Failed to open pause image");
        return false;
    }

    AImageDecoder* decoder = nullptr;
    int result = AImageDecoder_createFromAAsset(asset, &decoder);
    if (result != ANDROID_IMAGE_DECODER_SUCCESS)
    {
        IMLOGE0("[ImsMediaPauseImageSource] Failed to decode pause image");
        AAsset_close(asset);
        return false;
    }

    result = AImageDecoder_setAndroidBitmapFormat(decoder, ANDROID_BITMAP_FORMAT_RGBA_8888);
    if (result != ANDROID_IMAGE_DECODER_SUCCESS)
    {
        IMLOGE0("[ImsMediaPauseImageSource] Failed to select RGBA output");
        AImageDecoder_delete(decoder);
        AAsset_close(asset);
        return false;
    }

    const AImageDecoderHeaderInfo* info = AImageDecoder_getHeaderInfo(decoder);
    int32_t JpegWidth = AImageDecoderHeaderInfo_getWidth(info);
    int32_t JpegHeight = AImageDecoderHeaderInfo_getHeight(info);
    if (JpegWidth != mWidth || JpegHeight != mHeight)
    {
        IMLOGE0("[ImsMediaPauseImageSource] Decoded image resolution doesn't match with JPEG image"
                "resolution");
        AImageDecoder_delete(decoder);
        AAsset_close(asset);
        return false;
    }

    size_t decStride = AImageDecoder_getMinimumStride(decoder);  // Image decoder does not
    // use padding by default
    if (static_cast<size_t>(width) > SIZE_MAX / 4 ||
            decStride < static_cast<size_t>(width) * 4 ||
            decStride > SIZE_MAX / static_cast<size_t>(height))
    {
        IMLOGE0("[ImsMediaPauseImageSource] Invalid decoder stride");
        AImageDecoder_delete(decoder);
        AAsset_close(asset);
        return false;
    }

    size_t size = static_cast<size_t>(height) * decStride;
    int8_t* pixels = reinterpret_cast<int8_t*>(malloc(size));
    if (pixels == nullptr)
    {
        IMLOGE0("[ImsMediaPauseImageSource] Failed to allocate RGBA buffer");
        AImageDecoder_delete(decoder);
        AAsset_close(asset);
        return false;
    }

    result = AImageDecoder_decodeImage(decoder, pixels, decStride, size);
    if (result != ANDROID_IMAGE_DECODER_SUCCESS)
    {
        IMLOGE0("[ImsMediaPauseImageSource] error occurred, and the file could not be decoded.");
        AImageDecoder_delete(decoder);
        free(pixels);
        AAsset_close(asset);
        return false;
    }

    mYuvImageBuffer = ConvertRgbaToYuv(pixels, width, height, stride, decStride);

    AImageDecoder_delete(decoder);
    free(pixels);
    AAsset_close(asset);
    return mYuvImageBuffer != nullptr;
}

size_t ImsMediaPauseImageSource::GetYuvImage(uint8_t* buffer, size_t len)
{
    if (buffer == nullptr || mYuvImageBuffer == nullptr || mBufferSize == 0)
    {
        IMLOGE0("[ImsMediaPauseImageSource] GetYuvImage. buffer == nullptr");
        return 0;
    }

    if (len >= mBufferSize)
    {
        memcpy(buffer, mYuvImageBuffer, mBufferSize);
        return mBufferSize;
    }

    IMLOGE2("[ImsMediaPauseImageSource] buffer size is smaller. Expected Bufsize[%zu], passed[%zu]",
            mBufferSize, len);
    return 0;
}

AAsset* ImsMediaPauseImageSource::getImageAsset()
{
    IMLOGD0("[ImsMediaPauseImageSource] getImageFileFd");
    if (gpAssetManager == nullptr)
    {
        IMLOGE0("[ImsMediaPauseImageSource] AssetManager is nullptr");
        return nullptr;
    }

    const char* filePath = getImageFilePath();
    if (filePath == nullptr)
    {
        return nullptr;
    }
    return AAssetManager_open(gpAssetManager, filePath, AASSET_MODE_RANDOM);
}

const char* ImsMediaPauseImageSource::getImageFilePath()
{
    if (mWidth == 1920 && mHeight == 1080)
        return DEFAULT_FHD_LANDSCAPE_PAUSE_IMG_PATH;
    else if (mWidth == 1080 && mHeight == 1920)
        return DEFAULT_FHD_PORTRAIT_PAUSE_IMG_PATH;
    else if (mWidth == 1280 && mHeight == 720)
        return DEFAULT_HD_LANDSCAPE_PAUSE_IMG_PATH;
    else if (mWidth == 720 && mHeight == 1280)
        return DEFAULT_HD_PORTRAIT_PAUSE_IMG_PATH;
    else if (mWidth == 640 && mHeight == 480)
        return DEFAULT_VGA_LANDSCAPE_PAUSE_IMG_PATH;
    else if (mWidth == 480 && mHeight == 640)
        return DEFAULT_VGA_PORTRAIT_PAUSE_IMG_PATH;
    else if (mWidth == 352 && mHeight == 288)
        return DEFAULT_CIF_LANDSCAPE_PAUSE_IMG_PATH;
    else if (mWidth == 288 && mHeight == 352)
        return DEFAULT_CIF_PORTRAIT_PAUSE_IMG_PATH;
    else if (mWidth == 352 && mHeight == 240)
        return DEFAULT_SIF_LANDSCAPE_PAUSE_IMG_PATH;
    else if (mWidth == 240 && mHeight == 352)
        return DEFAULT_SIF_PORTRAIT_PAUSE_IMG_PATH;
    else if (mWidth == 320 && mHeight == 240)
        return DEFAULT_QVGA_LANDSCAPE_PAUSE_IMG_PATH;
    else if (mWidth == 240 && mHeight == 320)
        return DEFAULT_QVGA_PORTRAIT_PAUSE_IMG_PATH;
    else if (mWidth == 176 && mHeight == 144)
        return DEFAULT_QCIF_LANDSCAPE_PAUSE_IMG_PATH;
    else if (mWidth == 144 && mHeight == 176)
        return DEFAULT_QCIF_PORTRAIT_PAUSE_IMG_PATH;
    else if (mWidth == 128 && mHeight == 96)
        return DEFAULT_SQCIF_LANDSCAPE_PAUSE_IMG_PATH;
    else if (mWidth == 96 && mHeight == 128)
        return DEFAULT_SQCIF_PORTRAIT_PAUSE_IMG_PATH;
    else
    {
        IMLOGE2("Resolution [%dx%d] pause image is not available", mWidth, mHeight);
    }

    return nullptr;
}

int8_t* ImsMediaPauseImageSource::ConvertRgbaToYuv(
        int8_t* pixels, int width, int height, int stride, size_t sourceStride)
{
    if (pixels == nullptr || width <= 0 || height <= 0 || stride < width)
    {
        return nullptr;
    }

    if (static_cast<size_t>(stride) > SIZE_MAX / static_cast<size_t>(height))
    {
        return nullptr;
    }
    const size_t yPlaneSize = static_cast<size_t>(stride) * height;
    if (yPlaneSize > SIZE_MAX - yPlaneSize / 2)
    {
        return nullptr;
    }
    mBufferSize = yPlaneSize + yPlaneSize / 2;
    int8_t* pDstArray = reinterpret_cast<int8_t*>(calloc(mBufferSize, 1));
    if (pDstArray == nullptr)
    {
        mBufferSize = 0;
        return nullptr;
    }

    size_t nYIndex = 0;
    size_t nUVIndex = yPlaneSize;
    int32_t r, g, b, padLen = stride - width;
    double y, u, v;

    for (int32_t j = 0; j < height; j++)
    {
        const uint8_t* sourceRow = reinterpret_cast<uint8_t*>(pixels) + j * sourceStride;
        for (int32_t i = 0; i < width; i++)
        {
            const uint8_t* pixel = sourceRow + i * 4;
            r = pixel[0];
            g = pixel[1];
            b = pixel[2];

            // rgb to yuv
            y = 0.257 * r + 0.504 * g + 0.098 * b + 16;
            u = 128 - 0.148 * r - 0.291 * g + 0.439 * b;
            v = 128 + 0.439 * r - 0.368 * g - 0.071 * b;

            // clip y
            pDstArray[nYIndex++] = (uint8_t)((y < 0) ? 0 : ((y > 255) ? 255 : y));

            if (j % 2 == 0 && i % 2 == 1)
            {
                pDstArray[nUVIndex++] = (uint8_t)((u < 0) ? 0 : ((u > 255) ? 255 : u));
                pDstArray[nUVIndex++] = (uint8_t)((v < 0) ? 0 : ((v > 255) ? 255 : v));
            }
        }

        // Add padding if stride > width
        if (padLen > 0)
        {
            nYIndex += padLen;

            if (j % 2 == 0)
            {
                nUVIndex += padLen;
            }
        }
    }

    return pDstArray;
}

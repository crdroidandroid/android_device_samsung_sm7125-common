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

#include "ImsMediaImageRotate.h"
#include <ImsMediaTrace.h>

void ImsMediaImageRotate::YUV420_Planar_Rotate90_Flip(
        uint8_t* pbDst, uint8_t* pbSrc, uint16_t nSrcWidth, uint16_t nSrcHeight)
{
    if (pbDst == nullptr || pbSrc == nullptr || nSrcWidth == 0 || nSrcHeight == 0
            || nSrcWidth % 2 != 0 || nSrcHeight % 2 != 0)
    {
        return;
    }
    uint16_t x, y;
    uint64_t srcIdx, dstIdx;
    const size_t size = static_cast<size_t>(nSrcWidth) * nSrcHeight;
    dstIdx = size - 1;

    // Rotate Y buffer
    for (y = 0; y < nSrcWidth; y++)
    {
        srcIdx = y;
        for (x = 0; x < nSrcHeight; x++)
        {
            pbDst[dstIdx] = pbSrc[srcIdx];  // Y

            srcIdx += nSrcWidth;
            dstIdx--;
        }
    }

    dstIdx = size + size / 2 - 1;
    const uint64_t usize = size / 4;
    nSrcWidth /= 2;
    nSrcHeight /= 2;

    // Rotate UV buffer
    for (y = 0; y < nSrcWidth; y++)
    {
        srcIdx = size + y;
        for (x = 0; x < nSrcHeight; x++)
        {
            pbDst[dstIdx - usize] = pbSrc[srcIdx];  // U
            pbDst[dstIdx] = pbSrc[usize + srcIdx];  // V

            srcIdx += nSrcWidth;
            dstIdx--;
        }
    }
}

int ImsMediaImageRotate::YUV420_SP_Rotate90(uint8_t* pOutBuffer, size_t nOutBufSize,
        uint16_t outputStride, uint8_t* pYPlane, uint8_t* pUVPlane, uint16_t nSrcWidth,
        uint16_t nSrcHeight)
{
    const size_t nDstWidth = nSrcHeight;
    const size_t nDstHt = nSrcWidth;
    if (pOutBuffer == nullptr || pYPlane == nullptr || pUVPlane == nullptr || nSrcWidth == 0 ||
            nSrcHeight == 0 || nSrcWidth % 2 != 0 || nSrcHeight % 2 != 0 ||
            outputStride % 2 != 0 ||
            nDstWidth > outputStride)
    {
        IMLOGE2("Invalid destination width[%zu] or stride[%u]", nDstWidth, outputStride);
        return -1;
    }

    const size_t yPlaneSize = static_cast<size_t>(outputStride) * nDstHt;
    const size_t dstSize = yPlaneSize + yPlaneSize / 2;
    if (nOutBufSize < dstSize)
    {
        IMLOGE2("Output buffer is too small. Required[%zu], passed[%zu]", dstSize, nOutBufSize);
        return -1;
    }

    uint16_t x, y;
    const size_t nPadWidth = outputStride - nDstWidth;
    uint64_t srcIdx, dstIdx = yPlaneSize - 1;

    // Rotate Y buffer
    for (y = 0; y < nSrcWidth; y++)
    {
        dstIdx -= nPadWidth;
        srcIdx = nSrcWidth - y - 1;
        for (x = 0; x < nSrcHeight; x++)
        {
            pOutBuffer[dstIdx] = pYPlane[srcIdx];  // Y
            srcIdx += nSrcWidth;
            dstIdx--;
        }
    }

    dstIdx = dstSize - 1;
    nSrcWidth /= 2;
    nSrcHeight /= 2;

    // Rotate UV buffer
    for (y = 0; y < nSrcWidth; y++)
    {
        dstIdx -= nPadWidth;
        srcIdx = (nSrcWidth - y - 1) * 2;
        for (x = 0; x < nSrcHeight; x++)
        {
            pOutBuffer[dstIdx--] = pUVPlane[srcIdx + 1];  // V
            pOutBuffer[dstIdx--] = pUVPlane[srcIdx];      // U
            srcIdx += nSrcWidth * 2;
        }
    }

    return 0;
}

void ImsMediaImageRotate::YUV420_SP_Rotate90_Flip(uint8_t* pbDst, uint8_t* pYPlane,
        uint8_t* pUVPlane, uint16_t nSrcWidth, uint16_t nSrcHeight)
{
    if (pbDst == nullptr || pYPlane == nullptr || pUVPlane == nullptr || nSrcWidth == 0
            || nSrcHeight == 0 || nSrcWidth % 2 != 0 || nSrcHeight % 2 != 0)
    {
        return;
    }
    uint16_t x, y;
    uint64_t srcIdx, dstIdx;
    const size_t size = static_cast<size_t>(nSrcWidth) * nSrcHeight;

    dstIdx = size - 1;

    // Rotate Y buffer
    for (y = 0; y < nSrcWidth; y++)
    {
        srcIdx = y;
        for (x = 0; x < nSrcHeight; x++)
        {
            pbDst[dstIdx] = pYPlane[srcIdx];  // Y
            srcIdx += nSrcWidth;
            dstIdx--;
        }
    }

    dstIdx = size + size / 2 - 1;
    nSrcWidth /= 2;
    nSrcHeight /= 2;

    // Rotate UV buffer
    for (y = 0; y < nSrcWidth; y++)
    {
        srcIdx = y * 2;
        for (x = 0; x < nSrcHeight; x++)
        {
            pbDst[dstIdx--] = pUVPlane[srcIdx + 1];  // V
            pbDst[dstIdx--] = pUVPlane[srcIdx];      // U
            srcIdx += nSrcWidth * 2;
        }
    }
}

int ImsMediaImageRotate::YUV420_SP_Rotate270(uint8_t* pOutBuffer, size_t nOutBufSize,
        uint16_t outputStride, uint8_t* pYPlane, uint8_t* pUVPlane, uint16_t nSrcWidth,
        uint16_t nSrcHeight)
{
    const size_t nDstWidth = nSrcHeight;
    const size_t nDstHt = nSrcWidth;
    if (pOutBuffer == nullptr || pYPlane == nullptr || pUVPlane == nullptr || nSrcWidth == 0 ||
            nSrcHeight == 0 || nSrcWidth % 2 != 0 || nSrcHeight % 2 != 0 ||
            outputStride % 2 != 0 ||
            nDstWidth > outputStride)
    {
        IMLOGE2("Invalid destination width[%zu] or stride[%u]", nDstWidth, outputStride);
        return -1;
    }

    const size_t yPlaneSize = static_cast<size_t>(outputStride) * nDstHt;
    const size_t dstSize = yPlaneSize + yPlaneSize / 2;
    if (nOutBufSize < dstSize)
    {
        IMLOGE2("Output buffer is too small. Required[%zu], passed[%zu]", dstSize, nOutBufSize);
        return -1;
    }

    uint16_t x, y;
    const size_t nPadWidth = outputStride - nDstWidth;
    uint64_t srcIdx, dstIdx = yPlaneSize - 1;
    const size_t size = static_cast<size_t>(nSrcWidth) * nSrcHeight;

    // Rotate Y buffer
    for (y = 0; y < nSrcWidth; y++)
    {
        dstIdx -= nPadWidth;
        srcIdx = size - nSrcWidth + y;
        for (x = 0; x < nSrcHeight; x++)
        {
            pOutBuffer[dstIdx] = pYPlane[srcIdx];  // Y
            srcIdx -= nSrcWidth;
            dstIdx--;
        }
    }

    dstIdx = dstSize - 1;
    nSrcWidth /= 2;
    nSrcHeight /= 2;

    // Rotate UV buffer
    for (y = 0; y < nSrcWidth; y++)
    {
        dstIdx -= nPadWidth;
        srcIdx = (size / 2) - (nSrcWidth - y) * 2;
        for (x = 0; x < nSrcHeight; x++)
        {
            pOutBuffer[dstIdx--] = pUVPlane[srcIdx + 1];  // V
            pOutBuffer[dstIdx--] = pUVPlane[srcIdx];      // U
            srcIdx -= nSrcWidth * 2;
        }
    }

    return 0;
}

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

#include <ImsMediaBitReader.h>
#include <ImsMediaTrace.h>
#include <string.h>

ImsMediaBitReader::ImsMediaBitReader()
{
    mBuffer = nullptr;
    mMaxBufferSize = 0;
    mBytePos = 0;
    mBitPos = 0;
    mBitBuffer = 0;
    mBufferEOF = false;
}

ImsMediaBitReader::~ImsMediaBitReader() {}

void ImsMediaBitReader::SetBuffer(uint8_t* pbBuffer, uint32_t nBufferSize)
{
    mBytePos = 0;
    mBitPos = 32;
    mBitBuffer = 0;
    mBufferEOF = false;
    mBuffer = pbBuffer;
    mMaxBufferSize = nBufferSize;
}

uint32_t ImsMediaBitReader::Read(uint32_t nSize)
{
    uint32_t value;
    if (nSize == 0)
        return 0;
    if (mBuffer == nullptr || nSize > 24 || mBufferEOF)
    {
        IMLOGE2("[Read] nSize[%d], bBufferEOF[%d]", nSize, mBufferEOF);
        // Invalid widths do not consume input or mark an otherwise valid buffer as ended.
        if (mBuffer == nullptr)
        {
            mBufferEOF = true;
        }
        return 0;
    }

    // read from byte buffer
    while ((32 - mBitPos) < nSize)
    {
        if (mBytePos >= mMaxBufferSize)
        {
            mBufferEOF = true;
            IMLOGE2("[Read] End of Buffer : nBytePos[%d], nMaxBufferSize[%d]", mBytePos,
                    mMaxBufferSize);
            return 0;
        }

        mBitPos -= 8;
        mBitBuffer <<= 8;
        mBitBuffer += mBuffer[mBytePos++];
    }

    // read from bit buffer
    value = mBitBuffer << mBitPos >> (32 - nSize);
    mBitPos += nSize;
    return value;
}

bool ImsMediaBitReader::ReadByteBuffer(uint8_t* pbDst, uint32_t nBitSize)
{
    if (nBitSize == 0)
    {
        return true;
    }

    if (pbDst == nullptr || mBuffer == nullptr || mBufferEOF || mBitPos > 32 ||
            mBytePos > mMaxBufferSize)
    {
        IMLOGE1("[ReadByteBuffer] Invalid nBitSize[%d]", nBitSize);
        mBufferEOF = true;
        return false;
    }

    const uint64_t bufferedBits = 32 - mBitPos;
    const uint64_t unreadBits = static_cast<uint64_t>(mMaxBufferSize - mBytePos) * 8;
    if (static_cast<uint64_t>(nBitSize) > bufferedBits + unreadBits)
    {
        IMLOGE3("[ReadByteBuffer] nBitSize[%d], BytePos[%d], BufferSize[%d]", nBitSize, mBytePos,
                mMaxBufferSize);
        mBufferEOF = true;
        return false;
    }

    uint32_t dst_pos = 0;
    uint32_t nByteSize;
    uint32_t nRemainBitSize;
    nByteSize = nBitSize >> 3;
    nRemainBitSize = nBitSize & 0x07;

    if (mBitPos == 32)
    {
        memcpy(pbDst, mBuffer + mBytePos, nByteSize);
        mBytePos += nByteSize;
        dst_pos += nByteSize;
    }
    else
    {
        for (dst_pos = 0; dst_pos < nByteSize; dst_pos++)
        {
            pbDst[dst_pos] = Read(8);
        }
    }

    if (nRemainBitSize > 0)
    {
        uint32_t v;
        v = Read(nRemainBitSize);
        v <<= (8 - nRemainBitSize);
        pbDst[dst_pos] = (unsigned char)v;
    }

    return true;
}

uint32_t ImsMediaBitReader::ReadByUEMode()
{
    uint32_t i = 0;

    while (!mBufferEOF)
    {
        const uint32_t bit = Read(1);
        if (mBufferEOF)
        {
            return 0;
        }

        if (bit != 0)
        {
            break;
        }

        if (i >= 24)
        {
            IMLOGE0("[ReadByUEMode] Exp-Golomb value is too large");
            mBufferEOF = true;
            return 0;
        }

        i++;
    }

    const uint32_t suffix = Read(i);
    if (mBufferEOF)
    {
        return 0;
    }

    return ((1U << i) - 1) + suffix;
}

bool ImsMediaBitReader::IsBufferEnd() const
{
    return mBufferEOF;
}

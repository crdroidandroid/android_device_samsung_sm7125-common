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

#include <ImsMediaBitWriter.h>
#include <ImsMediaTrace.h>
#include <string.h>

ImsMediaBitWriter::ImsMediaBitWriter()
{
    mBuffer = nullptr;
    mMaxBufferSize = 0;
    mBytePos = 0;
    mBitPos = 0;
    mBitBuffer = 0;
    mBufferFull = false;
}

ImsMediaBitWriter::~ImsMediaBitWriter() {}

bool ImsMediaBitWriter::HasCapacity(uint32_t nBitSize) const
{
    if (mBuffer == nullptr || mBytePos > mMaxBufferSize || mBitPos >= 8)
    {
        return false;
    }

    const uint64_t currentBitPosition = static_cast<uint64_t>(mBytePos) * 8 + mBitPos;
    const uint64_t maxBitPosition = static_cast<uint64_t>(mMaxBufferSize) * 8;
    return static_cast<uint64_t>(nBitSize) <= maxBitPosition - currentBitPosition;
}

void ImsMediaBitWriter::SetBuffer(uint8_t* pbBuffer, uint32_t nBufferSize)
{
    mBytePos = 0;
    mBitPos = 0;
    mBitBuffer = 0;
    mBufferFull = false;
    mBuffer = pbBuffer;
    mMaxBufferSize = nBufferSize;
}

bool ImsMediaBitWriter::Write(uint32_t nValue, uint32_t nSize)
{
    if (nSize == 0)
    {
        return false;
    }

    if (nSize > 24 || mBufferFull || !HasCapacity(nSize))
    {
        IMLOGE3("[Write] nSize[%d], BufferFull[%d], BufferSize[%d]", nSize, mBufferFull,
                mMaxBufferSize);
        return false;
    }

    // write to bit buffer
    mBitBuffer += (nValue << (32 - nSize) >> mBitPos);
    mBitPos += nSize;

    // write to byte buffer
    while (mBitPos >= 8)
    {
        mBuffer[mBytePos++] = (uint8_t)(mBitBuffer >> 24);
        mBitBuffer <<= 8;
        mBitPos -= 8;
    }

    if (mBytePos >= mMaxBufferSize)
    {
        mBufferFull = true;
    }

    return true;
}

bool ImsMediaBitWriter::WriteByteBuffer(uint8_t* pbSrc, uint32_t nBitSize)
{
    if (nBitSize == 0)
    {
        return true;
    }

    if (pbSrc == nullptr || mBufferFull || !HasCapacity(nBitSize))
    {
        IMLOGE2("[WriteByteBuffer] nBitSize[%d], BufferFull[%d]", nBitSize, mBufferFull);
        return false;
    }

    uint32_t nByteSize;
    uint32_t nRemainBitSize;
    nByteSize = nBitSize >> 3;
    nRemainBitSize = nBitSize & 0x07;

    if (mBitPos == 0)
    {
        memcpy(mBuffer + mBytePos, pbSrc, nByteSize);
        mBytePos += nByteSize;
    }
    else
    {
        uint32_t i;

        for (i = 0; i < nByteSize; i++)
        {
            if (!Write(pbSrc[i], 8))
            {
                return false;
            }
        }
    }

    if (nRemainBitSize > 0)
    {
        uint32_t v = pbSrc[nByteSize];
        v >>= (8 - nRemainBitSize);

        if (!Write(v, nRemainBitSize))
        {
            return false;
        }
    }

    if (mBytePos >= mMaxBufferSize && mBitPos == 0)
    {
        mBufferFull = true;
    }

    return true;
}

bool ImsMediaBitWriter::WriteByteBuffer(uint32_t value)
{
    if (mBufferFull || !HasCapacity(32))
    {
        IMLOGE1("[WriteByteBuffer] BufferFull[%d]", mBufferFull);
        return false;
    }

    uint32_t nRemainBitSize = 32;

    for (int32_t i = 0; i < 4; i++)
    {
        nRemainBitSize -= 8;
        uint8_t v = (value >> nRemainBitSize) & 0x00ff;

        if (!Write(v, 8))
        {
            return false;
        }
    }

    return true;
}

void ImsMediaBitWriter::Seek(uint32_t nSize)
{
    if (mBufferFull || !HasCapacity(nSize))
    {
        IMLOGE2("[Seek] nSize[%d], BufferSize[%d]", nSize, mMaxBufferSize);
        mBufferFull = true;
        return;
    }

    const uint64_t newBitPosition = static_cast<uint64_t>(mBitPos) + nSize;
    if (newBitPosition >= 8 && mBitPos > 0)
    {
        // Preserve bits already staged in the current byte before moving to a later byte.
        // OR is required because a header and payload writer may share the same output byte.
        mBuffer[mBytePos] |= static_cast<uint8_t>(mBitBuffer >> 24);
        mBitBuffer = 0;
    }

    mBytePos += static_cast<uint32_t>(newBitPosition / 8);
    mBitPos = static_cast<uint32_t>(newBitPosition % 8);

    if (mBytePos >= mMaxBufferSize && mBitPos == 0)
    {
        mBufferFull = true;
    }
}

void ImsMediaBitWriter::AddPadding()
{
    if (mBitPos > 0)
    {
        Write(0, 8 - mBitPos);
    }
}

uint32_t ImsMediaBitWriter::GetBufferSize()
{
    uint32_t nSize;
    nSize = (mBitPos + 7) >> 3;
    nSize += mBytePos;
    return nSize;
}

void ImsMediaBitWriter::Flush()
{
    if (mBitPos > 0)
    {
        if (mBuffer == nullptr || mBytePos >= mMaxBufferSize)
        {
            IMLOGE2("[Flush] BytePos[%d], BufferSize[%d]", mBytePos, mMaxBufferSize);
            mBufferFull = true;
            return;
        }

        mBuffer[mBytePos] |= (uint8_t)(mBitBuffer >> 24);
    }
}

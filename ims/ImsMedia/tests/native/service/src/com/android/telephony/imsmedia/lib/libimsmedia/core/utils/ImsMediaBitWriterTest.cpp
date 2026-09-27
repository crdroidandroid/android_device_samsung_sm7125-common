/**
 * Copyright (C) 2023 The Android Open Source Project
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

#include <gtest/gtest.h>
#include <ImsMediaBitWriter.h>
#include <string.h>

class ImsMediaBitWriterTest : public ::testing::Test
{
public:
protected:
    virtual void SetUp() override {}

    virtual void TearDown() override {}
};

TEST_F(ImsMediaBitWriterTest, SetBufferAndWriteBitTest)
{
    uint8_t testBuffer[] = {1, 2, 4, 8, 16, 32, 64, 128};
    uint8_t dstBuffer[8] = {0};

    ImsMediaBitWriter writer;

    EXPECT_EQ(writer.Write(0, 24), false);
    writer.SetBuffer(dstBuffer, sizeof(dstBuffer));
    EXPECT_EQ(writer.Write(0, 32), false);

    for (int32_t i = 0; i < sizeof(testBuffer); i++)
    {
        EXPECT_EQ(writer.Write(testBuffer[i], 8), true);
    }

    EXPECT_EQ(writer.Write(0, 8), false);
    EXPECT_EQ(memcmp(dstBuffer, testBuffer, sizeof(testBuffer)), 0);
}

TEST_F(ImsMediaBitWriterTest, SetBufferAndWriteByteTest)
{
    uint8_t testBuffer[] = {1, 2, 4, 8, 16, 32, 64, 128};
    uint8_t dstBuffer[8] = {0};

    ImsMediaBitWriter writer;
    writer.SetBuffer(dstBuffer, sizeof(dstBuffer));

    for (int32_t i = 0; i < sizeof(testBuffer); i++)
    {
        EXPECT_EQ(writer.WriteByteBuffer(testBuffer + i, 8), true);
    }

    EXPECT_EQ(memcmp(dstBuffer, testBuffer, sizeof(testBuffer)), 0);
}

TEST_F(ImsMediaBitWriterTest, SetBufferAndSeekToWriteTest)
{
    uint8_t testBuffer[] = {1, 2, 4, 8, 16, 32, 64, 128};
    uint8_t dstBuffer[8] = {1, 2, 4, 8};

    ImsMediaBitWriter writer;
    writer.SetBuffer(dstBuffer, sizeof(dstBuffer));
    writer.Seek(32);
    writer.WriteByteBuffer(testBuffer + 4, 32);

    EXPECT_EQ(memcmp(dstBuffer, testBuffer, sizeof(testBuffer)), 0);
}

TEST_F(ImsMediaBitWriterTest, RejectsWritePastBufferWithoutCorruptingGuardBytes)
{
    uint8_t guardedBuffer[] = {0xAA, 0, 0xBB};
    uint8_t src[] = {1, 2};
    ImsMediaBitWriter writer;

    writer.SetBuffer(guardedBuffer + 1, 1);
    EXPECT_FALSE(writer.Write(0xFFFF, 16));
    EXPECT_EQ(guardedBuffer[0], 0xAA);
    EXPECT_EQ(guardedBuffer[1], 0);
    EXPECT_EQ(guardedBuffer[2], 0xBB);

    writer.SetBuffer(guardedBuffer + 1, 1);
    EXPECT_FALSE(writer.WriteByteBuffer(src, 16));
    EXPECT_EQ(guardedBuffer[0], 0xAA);
    EXPECT_EQ(guardedBuffer[1], 0);
    EXPECT_EQ(guardedBuffer[2], 0xBB);
}

TEST_F(ImsMediaBitWriterTest, RejectsPartialWriteThatCannotBeFlushed)
{
    uint8_t guardedBuffer[] = {0xAA, 0, 0xBB};
    ImsMediaBitWriter writer;

    writer.SetBuffer(guardedBuffer + 1, 1);
    EXPECT_TRUE(writer.Write(0x0F, 4));
    EXPECT_FALSE(writer.Write(0xFF, 8));
    writer.AddPadding();
    writer.Flush();

    EXPECT_EQ(guardedBuffer[0], 0xAA);
    EXPECT_EQ(guardedBuffer[1], 0xF0);
    EXPECT_EQ(guardedBuffer[2], 0xBB);
}

TEST_F(ImsMediaBitWriterTest, RejectsSeekPastBuffer)
{
    uint8_t guardedBuffer[] = {0xAA, 0, 0xBB};
    ImsMediaBitWriter writer;

    writer.SetBuffer(guardedBuffer + 1, 1);
    writer.Seek(16);
    EXPECT_FALSE(writer.Write(0xFF, 8));

    EXPECT_EQ(guardedBuffer[0], 0xAA);
    EXPECT_EQ(guardedBuffer[1], 0);
    EXPECT_EQ(guardedBuffer[2], 0xBB);
}

TEST_F(ImsMediaBitWriterTest, SeekPreservesPendingBits)
{
    uint8_t buffer = 0;
    ImsMediaBitWriter writer;

    writer.SetBuffer(&buffer, sizeof(buffer));
    EXPECT_TRUE(writer.Write(0b101, 3));
    writer.Seek(2);
    EXPECT_TRUE(writer.Write(0b111, 3));
    writer.Flush();

    EXPECT_EQ(buffer, 0b10100111);
}

TEST_F(ImsMediaBitWriterTest, SeekPreservesPendingBitsAcrossByteBoundary)
{
    uint8_t buffer[] = {0, 0};
    ImsMediaBitWriter writer;

    writer.SetBuffer(buffer, sizeof(buffer));
    EXPECT_TRUE(writer.Write(0b101, 3));
    writer.Seek(7);
    EXPECT_TRUE(writer.Write(0b111111, 6));

    EXPECT_EQ(buffer[0], 0b10100000);
    EXPECT_EQ(buffer[1], 0b00111111);
}

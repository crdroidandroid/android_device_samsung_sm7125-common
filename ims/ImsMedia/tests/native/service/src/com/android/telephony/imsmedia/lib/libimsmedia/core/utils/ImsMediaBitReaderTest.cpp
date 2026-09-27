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
#include <ImsMediaBitReader.h>
#include <string.h>

class ImsMediaBitReaderTest : public ::testing::Test
{
public:
protected:
    virtual void SetUp() override {}

    virtual void TearDown() override {}
};

TEST_F(ImsMediaBitReaderTest, SetBufferAndReadBitTest)
{
    uint8_t testBuffer[] = {1, 2, 4, 8, 16, 32, 64, 128};

    ImsMediaBitReader reader;
    EXPECT_EQ(reader.Read(24), 0);
    reader.SetBuffer(testBuffer, sizeof(testBuffer));
    EXPECT_EQ(reader.Read(32), 0);

    for (int32_t i = 0; i < sizeof(testBuffer); i++)
    {
        EXPECT_EQ(reader.Read(8), testBuffer[i]);
    }

    EXPECT_EQ(reader.Read(8), 0);
}

TEST_F(ImsMediaBitReaderTest, SetBufferAndReadByteTest)
{
    uint8_t testBuffer[] = {1, 2, 4, 8, 16, 32, 64, 128};

    ImsMediaBitReader reader;
    reader.SetBuffer(testBuffer, sizeof(testBuffer));

    uint8_t dstBuffer[8] = {0};

    for (int32_t i = 0; i < sizeof(testBuffer); i++)
    {
        EXPECT_TRUE(reader.ReadByteBuffer(dstBuffer + i, 8));
    }

    EXPECT_EQ(memcmp(dstBuffer, testBuffer, sizeof(testBuffer)), 0);
}

TEST_F(ImsMediaBitReaderTest, SetBufferAndReadUEModeTest)
{
    uint8_t testBuffer[] = {0xDA};  // 11011010

    ImsMediaBitReader reader;
    reader.SetBuffer(testBuffer, sizeof(testBuffer));

    EXPECT_EQ(reader.ReadByUEMode(), 0);
    EXPECT_EQ(reader.ReadByUEMode(), 0);
    EXPECT_EQ(reader.ReadByUEMode(), 2);
    EXPECT_EQ(reader.ReadByUEMode(), 1);
}

TEST_F(ImsMediaBitReaderTest, RejectsReadPastBufferWithoutCorruptingGuardBytes)
{
    uint8_t srcBuffer[] = {0x5A};
    uint8_t guardedBuffer[] = {0xAA, 1, 2, 0xBB};
    ImsMediaBitReader reader;

    reader.SetBuffer(srcBuffer, sizeof(srcBuffer));
    EXPECT_FALSE(reader.ReadByteBuffer(guardedBuffer + 1, 16));
    EXPECT_TRUE(reader.IsBufferEnd());

    EXPECT_EQ(guardedBuffer[0], 0xAA);
    EXPECT_EQ(guardedBuffer[1], 1);
    EXPECT_EQ(guardedBuffer[2], 2);
    EXPECT_EQ(guardedBuffer[3], 0xBB);
}

TEST_F(ImsMediaBitReaderTest, RejectsReadPastPartiallyConsumedBuffer)
{
    uint8_t srcBuffer[] = {0x5A};
    uint8_t dstBuffer = 0xCC;
    ImsMediaBitReader reader;

    reader.SetBuffer(srcBuffer, sizeof(srcBuffer));
    EXPECT_EQ(reader.Read(4), 5);
    EXPECT_FALSE(reader.ReadByteBuffer(&dstBuffer, 8));
    EXPECT_EQ(dstBuffer, 0xCC);
}

TEST_F(ImsMediaBitReaderTest, RejectsTruncatedUEModeValue)
{
    uint8_t srcBuffer[] = {0};
    ImsMediaBitReader reader;

    reader.SetBuffer(srcBuffer, sizeof(srcBuffer));
    EXPECT_EQ(reader.ReadByUEMode(), 0);
    EXPECT_TRUE(reader.IsBufferEnd());
}

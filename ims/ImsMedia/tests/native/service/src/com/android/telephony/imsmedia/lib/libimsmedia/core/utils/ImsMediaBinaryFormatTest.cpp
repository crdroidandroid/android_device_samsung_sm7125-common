/**
 * Copyright (C) 2026 The Android Open Source Project
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

#include <ImsMediaBinaryFormat.h>
#include <gtest/gtest.h>

#include <cstring>

TEST(ImsMediaBinaryFormatTest, Base64RoundTrip)
{
    uint8_t source[] = {0, 1, 2, 0xFE, 0xFF};
    char encoded[16] = {};
    uint8_t decoded[sizeof(source)] = {};
    uint32_t decodedSize = 0;

    EXPECT_TRUE(ImsMediaBinaryFormat::BinaryToBase00(
            encoded, sizeof(encoded), source, sizeof(source), BINARY_FORMAT_BASE64));
    EXPECT_STREQ(encoded, "AAEC/v8=");
    EXPECT_TRUE(ImsMediaBinaryFormat::Base00ToBinary(
            decoded, &decodedSize, sizeof(decoded), encoded, BINARY_FORMAT_BASE64));
    EXPECT_EQ(decodedSize, sizeof(source));
    EXPECT_EQ(std::memcmp(decoded, source, sizeof(source)), 0);
}

TEST(ImsMediaBinaryFormatTest, RejectsInvalidBase64WithoutWriting)
{
    char incomplete[] = "A";
    char invalidCharacter[] = "A!==";
    char tooMuchPadding[] = "A===";
    char misplacedPadding[] = "AA=A";
    char* invalidInputs[] = {
            incomplete, invalidCharacter, tooMuchPadding, misplacedPadding};

    for (char* encoded : invalidInputs)
    {
        uint8_t decoded[] = {0xAA, 0xBB};
        uint32_t decodedSize = 7;
        EXPECT_FALSE(ImsMediaBinaryFormat::Base00ToBinary(
                decoded, &decodedSize, sizeof(decoded), encoded, BINARY_FORMAT_BASE64));
        EXPECT_EQ(decoded[0], 0xAA);
        EXPECT_EQ(decoded[1], 0xBB);
    }
}

TEST(ImsMediaBinaryFormatTest, Base64HonorsCapacityAndLineBreaks)
{
    uint8_t source[] = {1, 2, 3};
    char guarded[] = {static_cast<char>(0xAA), 1, 2, 3, 4, static_cast<char>(0xBB)};
    EXPECT_FALSE(ImsMediaBinaryFormat::BinaryToBase00(
            guarded + 1, 4, source, sizeof(source), BINARY_FORMAT_BASE64));
    EXPECT_EQ(static_cast<uint8_t>(guarded[0]), 0xAA);
    EXPECT_EQ(static_cast<uint8_t>(guarded[sizeof(guarded) - 1]), 0xBB);

    char encoded[] = "AQ\r\nID";
    uint8_t decoded[3] = {};
    uint32_t decodedSize = 0;
    EXPECT_TRUE(ImsMediaBinaryFormat::Base00ToBinary(
            decoded, &decodedSize, sizeof(decoded), encoded, BINARY_FORMAT_BASE64));
    EXPECT_EQ(decodedSize, sizeof(decoded));
    EXPECT_EQ(std::memcmp(decoded, source, sizeof(source)), 0);
}

TEST(ImsMediaBinaryFormatTest, Base16ValidatesInputAndCapacity)
{
    uint8_t source[] = {0x01, 0xAB, 0xFF};
    char encoded[7] = {};
    uint8_t decoded[sizeof(source)] = {};
    uint32_t decodedSize = 0;

    EXPECT_TRUE(ImsMediaBinaryFormat::BinaryToBase00(
            encoded, sizeof(encoded), source, sizeof(source), BINARY_FORMAT_BASE16));
    EXPECT_STREQ(encoded, "01ABFF");
    EXPECT_TRUE(ImsMediaBinaryFormat::Base00ToBinary(
            decoded, &decodedSize, sizeof(decoded), encoded, BINARY_FORMAT_BASE16));
    EXPECT_EQ(decodedSize, sizeof(source));
    EXPECT_EQ(std::memcmp(decoded, source, sizeof(source)), 0);

    char odd[] = "ABC";
    char invalid[] = "0Z";
    EXPECT_FALSE(ImsMediaBinaryFormat::Base00ToBinary(
            decoded, &decodedSize, sizeof(decoded), odd, BINARY_FORMAT_BASE16));
    EXPECT_FALSE(ImsMediaBinaryFormat::Base00ToBinary(
            decoded, &decodedSize, sizeof(decoded), invalid, BINARY_FORMAT_BASE16));
}
